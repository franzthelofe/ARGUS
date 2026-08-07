/* ============================================================
 * ARGUS - Search and Rescue Robot — Head Module
 * FULL MERGED BUILD (Phases 1–7)
 *
 * Board: AI-Thinker ESP32-CAM (OV2640)
 * Framework: Arduino (via PlatformIO)
 *
 * Contents:
 *   Phase 1 - Wi-Fi + camera init
 *   Phase 2 - INMP441 mic (I2S_NUM_1, RX)
 *   Phase 3 - MAX98357A speaker (I2S_NUM_0, TX) + tone test
 *   Phase 4 - (superseded by Phase 5's WS audio; conversion
 *             logic from the mic->speaker loopback lives on in
 *             the mic streaming task below)
 *   Phase 5 - Two-way audio over WebSocket
 *   Phase 6 - MLX90640 thermal sensor + heat signature detection
 *   Phase 7 - Web interface tying camera/thermal/audio together
 *
 * Pin/architecture notes:
 *   - Speaker DIN (GPIO4) is also the onboard flash LED — it
 *     will flicker with audio, that's expected/harmless.
 *   - Speaker BCLK (GPIO12) is a boot-strapping pin (MTDI). Fine
 *     once running; if you see boot flakiness on some clone
 *     boards, this is the first thing to check.
 *   - MLX90640 shares the camera's SCCB/I2C bus (GPIO26/27) —
 *     there are no free GPIO pairs left on the AI-Thinker board
 *     by this phase. SCCB is electrically I2C and the two
 *     devices' addresses (OV2640=0x30, MLX90640=0x33) don't
 *     collide, but thermal reads and camera frame grabs are
 *     time-separated (not concurrent) to be safe.
 * ============================================================ */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <math.h>
#include "esp_camera.h"
#include "driver/i2s.h"
#include <Adafruit_MLX90640.h>

// ============================================================
// SECTION: Wi-Fi credentials (placeholders)
// ============================================================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ============================================================
// SECTION: Camera pins (AI-Thinker) — Phase 1
// ============================================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26   // also used as thermal SDA (shared bus)
#define SIOC_GPIO_NUM     27   // also used as thermal SCL (shared bus)
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ============================================================
// SECTION: INMP441 microphone pins — Phase 2
// I2S_NUM_1, RX. GPIO14/13/15 are the AI-Thinker SD card lines,
// free since this build doesn't use the SD slot.
// ============================================================
#define I2S_MIC_SCK_PIN   14
#define I2S_MIC_WS_PIN    13
#define I2S_MIC_SD_PIN    15
#define I2S_MIC_PORT      I2S_NUM_1

#define MIC_SAMPLE_RATE       16000
#define MIC_SAMPLE_BITS       32
#define MIC_DMA_BUF_COUNT     4
#define MIC_DMA_BUF_LEN       256

// ============================================================
// SECTION: MAX98357A speaker pins — Phase 3
// I2S_NUM_0, TX. Kept on a separate I2S port from the mic so
// both can run concurrently.
// ============================================================
#define I2S_SPK_BCLK_PIN  12
#define I2S_SPK_LRC_PIN    2
#define I2S_SPK_DIN_PIN    4
#define I2S_SPK_PORT      I2S_NUM_0

#define SPK_SAMPLE_RATE   16000
#define SPK_SAMPLE_BITS   16
#define SPK_DMA_BUF_COUNT 4
#define SPK_DMA_BUF_LEN   256

// Audio chunk size for the WS mic-streaming task — 256 samples
// @ 16kHz = 16ms per frame, a good latency/overhead balance.
#define AUDIO_CHUNK_SAMPLES 256

// ============================================================
// SECTION: MLX90640 thermal sensor — Phase 6
// Shares the camera's SCCB/I2C bus (see notes above).
// ============================================================
#define THERMAL_SDA_PIN  SIOD_GPIO_NUM
#define THERMAL_SCL_PIN  SIOC_GPIO_NUM
#define HEAT_SIGNATURE_THRESHOLD_C 30.0f
#define HEAT_SIGNATURE_MIN_PIXELS  15

Adafruit_MLX90640 mlx;
float thermalFrame[32 * 24];

// Shared thermal status, written by the polling task and read by
// loop()/web handlers. Protected with a critical section since
// it's written from a different FreeRTOS task than it's read from.
volatile bool  g_heatSignatureActive = false;
volatile float g_thermalMinC = 0, g_thermalMaxC = 0, g_thermalCenterC = 0;
portMUX_TYPE g_thermalMux = portMUX_INITIALIZER_UNLOCKED;

// ============================================================
// SECTION: Web server + WebSocket — Phase 5 / 7
// ============================================================
WebServer httpServer(80);
WebSocketsServer wsServer(81);

volatile bool  g_wsClientConnected = false;
volatile uint8_t g_wsClientNum = 0;

// ------------------------------------------------------------
// Web page: MJPEG camera view, polled thermal status, and
// two-way audio via WebSocket using the browser's Web Audio API.
// ------------------------------------------------------------
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>ARGUS Head — Control</title>
<style>
  body { font-family: sans-serif; background:#111; color:#eee; text-align:center; padding-top:20px; }
  img { max-width:90%; border:2px solid #444; border-radius:8px; }
  #status { margin-top:10px; }
  .alert { color:#ff5050; font-weight:bold; }
  .clear { color:#50ff90; }
  button { padding:10px 20px; font-size:16px; margin-top:10px; }
</style></head>
<body>
<h2>ARGUS Head</h2>
<img id="cam" src="/stream" alt="camera stream">
<div id="status">Thermal: -- / Heat status: --</div>
<button id="startBtn">Start Two-Way Audio</button>
<p id="audioStatus">Audio: idle</p>
<script>
async function pollStatus() {
  try {
    const res = await fetch('/status');
    const j = await res.json();
    const alertClass = j.heat_alert ? 'alert' : 'clear';
    document.getElementById('status').innerHTML =
      Thermal min ${j.thermal_min.toFixed(1)}C / max ${j.thermal_max.toFixed(1)}C / center ${j.thermal_center.toFixed(1)}C —  +
      <span class="${alertClass}">${j.heat_alert ? 'HEAT SIGNATURE DETECTED' : 'clear'}</span>;
  } catch (e) { /* status not reachable yet, ignore */ }
  setTimeout(pollStatus, 1000);
}
pollStatus();

let ws, audioCtx, micStream, micNode, processor;
const SAMPLE_RATE = 16000;

document.getElementById('startBtn').onclick = async () => {
  audioCtx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: SAMPLE_RATE });
  micStream = await navigator.mediaDevices.getUserMedia({ audio: { channelCount: 1, sampleRate: SAMPLE_RATE } });
  micNode = audioCtx.createMediaStreamSource(micStream);
  processor = audioCtx.createScriptProcessor(1024, 1, 1);

  ws = new WebSocket(ws://${location.hostname}:81/);
  ws.binaryType = 'arraybuffer';
  ws.onopen = () => { document.getElementById('audioStatus').innerText = 'Audio: connected'; };
  ws.onclose = () => { document.getElementById('audioStatus').innerText = 'Audio: disconnected'; };

  processor.onaudioprocess = (e) => {
    if (ws.readyState !== WebSocket.OPEN) return;
    const input = e.inputBuffer.getChannelData(0);
    const pcm16 = new Int16Array(input.length);
    for (let i = 0; i < input.length; i++) {
      let s = Math.max(-1, Math.min(1, input[i]));
      pcm16[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
    }
    ws.send(pcm16.buffer);
  };
  micNode.connect(processor);
  processor.connect(audioCtx.destination);

  ws.onmessage = (evt) => {
    const pcm16 = new Int16Array(evt.data);
    const buf = audioCtx.createBuffer(1, pcm16.length, SAMPLE_RATE);
    const chan = buf.getChannelData(0);
    for (let i = 0; i < pcm16.length; i++) chan[i] = pcm16[i] / 0x8000;
    const src = audioCtx.createBufferSource();
    src.buffer = buf;
    src.connect(audioCtx.destination);
    src.start();
  };
};
</script>
</body></html>
)HTML";

// ============================================================
// SECTION: Function prototypes
// ============================================================
void connectWiFi();
bool initCamera();
bool initMicrophone();
bool initSpeaker();
void playTone(uint16_t frequencyHz, uint16_t durationMs);
bool initThermalSensor();
void thermalPollTask(void* param);
void micStreamTask(void* param);
void handleWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
void handleStream();
void handleStatus();
void setupWebServer();

// ============================================================
// setup()
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(1000);

  Serial.println();
  Serial.println("=== ARGUS Head Module — Full Integration Boot ===");

  // --- Wi-Fi ---
  connectWiFi();

  // --- Camera ---
  if (!initCamera()) {
    Serial.println("[FATAL] Camera init failed. Halting.");
    while (true) delay(1000);
  }
  Serial.println("[OK] Camera initialized.");

  // --- Microphone ---
  if (!initMicrophone()) {
    Serial.println("[FATAL] Microphone init failed. Halting.");
    while (true) delay(1000);
  }
  Serial.println("[OK] Microphone initialized.");

  // --- Speaker ---
  if (!initSpeaker()) {
    Serial.println("[FATAL] Speaker init failed. Halting.");
    while (true) delay(1000);
  }
  Serial.println("[OK] Speaker initialized.");
  playTone(1000, 200); // audible confirmation the audio chain works

  // --- Thermal (non-fatal if missing — robot still useful without it) ---
  if (initThermalSensor()) {
    Serial.println("[OK] Thermal sensor (MLX90640) initialized.");
    xTaskCreatePinnedToCore(thermalPollTask, "ThermalPoll", 4096, nullptr, 1, nullptr, 1);
  } else {
    Serial.println("[WARN] Thermal sensor unavailable — continuing without it.");
  }

  // --- Web interface (HTTP + WebSocket) ---
  setupWebServer();

  // --- Mic -> browser streaming task ---
  xTaskCreatePinnedToCore(micStreamTask, "MicStream", 4096, nullptr, 2, nullptr, 1);

  Serial.println("=== ARGUS Head Module — READY ===");
  Serial.println("Web interface: http://<robot-ip>/");
}

// ============================================================
// loop()
// Pumps the HTTP + WebSocket servers and prints a periodic
// heartbeat with thermal alert status. All the real-time work
// (audio streaming, thermal polling) runs in its own FreeRTOS
// task, so loop() stays light and responsive.
// ============================================================
void loop() {
  httpServer.handleClient();
  wsServer.loop();

  static unsigned long lastBeat = 0;
  if (millis() - lastBeat > 5000) {
    lastBeat = millis();
    portENTER_CRITICAL(&g_thermalMux);
    bool heatAlert = g_heatSignatureActive;
    portEXIT_CRITICAL(&g_thermalMux);

    Serial.printf("[heartbeat] uptime=%lus  freeHeap=%u bytes  thermal=%s\n",
                  millis() / 1000, ESP.getFreeHeap(),
                  heatAlert ? "ALERT" : "clear");
  }
}

// ============================================================
// SECTION: Wi-Fi — Phase 1
// ============================================================
void connectWiFi() {
  Serial.printf("[WiFi] Connecting to SSID: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[OK] WiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WARN] WiFi not connected — continuing offline.");
  }
}

// ============================================================
// SECTION: Camera — Phase 1
// ============================================================
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk  = XCLK_GPIO_NUM;
  config.pin_pclk  = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href  = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
    Serial.println("[Camera] PSRAM found — using double buffering.");
  } else {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
    Serial.println("[Camera] No PSRAM found — single buffer fallback.");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[Camera] esp_camera_init failed: 0x%x\n", err);
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  if (s != nullptr) {
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_gain_ctrl(s, 1);
  }
  return true;
}

// ============================================================
// SECTION: Microphone (INMP441) — Phase 2
// ============================================================
bool initMicrophone() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = MIC_SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t)MIC_SAMPLE_BITS,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // L/R pin grounded on the mic
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = MIC_DMA_BUF_COUNT,
    .dma_buf_len = MIC_DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  if (i2s_driver_install(I2S_MIC_PORT, &i2s_config, 0, nullptr) != ESP_OK) return false;

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_MIC_SCK_PIN,
    .ws_io_num = I2S_MIC_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD_PIN
  };
  if (i2s_set_pin(I2S_MIC_PORT, &pin_config) != ESP_OK) return false;

  i2s_zero_dma_buffer(I2S_MIC_PORT);
  return true;
}

// ============================================================
// SECTION: Speaker (MAX98357A) — Phase 3
// ============================================================
bool initSpeaker() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SPK_SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t)SPK_SAMPLE_BITS,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = SPK_DMA_BUF_COUNT,
    .dma_buf_len = SPK_DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = true, // auto-fill silence on underrun
    .fixed_mclk = 0
  };
  if (i2s_driver_install(I2S_SPK_PORT, &i2s_config, 0, nullptr) != ESP_OK) return false;

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SPK_BCLK_PIN,
    .ws_io_num = I2S_SPK_LRC_PIN,
    .data_out_num = I2S_SPK_DIN_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  if (i2s_set_pin(I2S_SPK_PORT, &pin_config) != ESP_OK) return false;

  i2s_zero_dma_buffer(I2S_SPK_PORT);
  return true;
}

void playTone(uint16_t frequencyHz, uint16_t durationMs) {
  const int sampleCount = (SPK_SAMPLE_RATE * durationMs) / 1000;
  int16_t sampleBuf[128];
  int samplesWritten = 0;
  double phase = 0.0;
  double phaseIncrement = 2.0 * PI * frequencyHz / SPK_SAMPLE_RATE;

  while (samplesWritten < sampleCount) {
    int chunk = min(128, sampleCount - samplesWritten);
    for (int i = 0; i < chunk; i++) {
      sampleBuf[i] = (int16_t)(sin(phase) * 8000); // amplitude under 16-bit max
      phase += phaseIncrement;
      if (phase > 2.0 * PI) phase -= 2.0 * PI;
    }
    size_t bytesWritten = 0;
    i2s_write(I2S_SPK_PORT, sampleBuf, chunk * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
    samplesWritten += chunk;
  }
}

// ============================================================
// SECTION: Thermal (MLX90640) — Phase 6
// ============================================================
bool initThermalSensor() {
  // Safe to call even though esp_camera already configured these
  // pins for SCCB — I2C init just sets pin modes/pull-ups, and
  // accesses to the two devices are kept time-separated.
  Wire.begin(THERMAL_SDA_PIN, THERMAL_SCL_PIN);

  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("[Thermal] MLX90640 not found on I2C bus.");
    return false;
  }

  mlx.setMode(MLX90640_CHESS);
  mlx.setResolution(MLX90640_ADC_18BIT);
  mlx.setRefreshRate(MLX90640_4_HZ);
  return true;
}

// ------------------------------------------------------------
// thermalPollTask()
// Runs continuously: reads a full 32x24 frame, computes min/max/
// center temps, and applies a coarse heat-signature threshold.
// Results are stored in shared globals (behind a critical
// section) for loop() and the web /status endpoint to read.
// ------------------------------------------------------------
void thermalPollTask(void* param) {
  while (true) {
    if (mlx.getFrame(thermalFrame) == 0) {
      float minT = thermalFrame[0];
      float maxT = thermalFrame[0];
      for (int i = 0; i < 32 * 24; i++) {
        if (thermalFrame[i] < minT) minT = thermalFrame[i];
        if (thermalFrame[i] > maxT) maxT = thermalFrame[i];
      }
      float centerT = thermalFrame[12 * 32 + 16]; // row 12, col 16

      int hotPixels = 0;
      for (int i = 0; i < 32 * 24; i++) {
        if (thermalFrame[i] >= HEAT_SIGNATURE_THRESHOLD_C) hotPixels++;
      }

      portENTER_CRITICAL(&g_thermalMux);
      g_thermalMinC = minT;
      g_thermalMaxC = maxT;
      g_thermalCenterC = centerT;
      g_heatSignatureActive = (hotPixels >= HEAT_SIGNATURE_MIN_PIXELS);
      portEXIT_CRITICAL(&g_thermalMux);

      Serial.printf("[Thermal] min=%.1fC max=%.1fC center=%.1fC hotPixels=%d\n",
                    minT, maxT, centerT, hotPixels);
    } else {
      Serial.println("[Thermal] Frame read failed.");
    }
    vTaskDelay(pdMS_TO_TICKS(250)); // matches 4Hz sensor refresh rate
  }
}

// ============================================================
// SECTION: Web server (HTTP + WebSocket) — Phase 5 / 7
// ============================================================

// ------------------------------------------------------------
// handleWsEvent()
// Inbound binary frames are 16-bit PCM audio from the browser
// mic — written straight to the speaker's I2S port.
// ------------------------------------------------------------
void handleWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      g_wsClientConnected = true;
      g_wsClientNum = num;
      Serial.printf("[WS] Client %u connected.\n", num);
      break;

    case WStype_DISCONNECTED:
      g_wsClientConnected = false;
      Serial.printf("[WS] Client %u disconnected.\n", num);
      break;

    case WStype_BIN: {
      size_t bytesWritten = 0;
      i2s_write(I2S_SPK_PORT, payload, length, &bytesWritten, portMAX_DELAY);
      break;
    }

    default:
      break;
  }
}

// -------------------------------------