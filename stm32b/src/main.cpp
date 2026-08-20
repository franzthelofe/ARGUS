/* ============================================================
 * ARGUS - Search and Rescue Robot — Body Module
 *
 * Board: STM32F411CE "Blackpill"
 * Framework: Arduino (via PlatformIO, STM32duino core)
 *
 * Responsibilities:
 *   - Receive drive commands over Bluetooth (HM-10 BLE, UART)
 *   - Auto-configure the HM-10 (name, role, optional PIN) via
 *     AT commands once at boot
 *   - Drive two DC motors through an L298N H-bridge for
 *     differential (tank-style) steering
 *   - Auto-stop if the Bluetooth link goes quiet OR the HM-10's
 *     STATE pin reports a dropped connection, so the robot
 *     never keeps crawling through unstable rubble on a lost
 *     link
 *
 * Command protocol (single ASCII bytes over HM-10, 9600 baud):
 *   'F'      forward
 *   'B'      backward
 *   'L'      pivot left  (left motor reverse, right motor forward)
 *   'R'      pivot right (left motor forward, right motor reverse)
 *   'S'      stop
 *   '0'-'9'  set speed to (digit/9)*100% of full PWM — takes
 *            effect immediately, and on every movement command
 *            after that
 *   anything else is ignored (harmless — covers stray bytes,
 *   newlines, HM-10 status chatter, etc.)
 *
 * Pin/architecture notes:
 *   - HM-10 is wired to USART1 (PA9 -> HM-10 RX, PA10 <- HM-10
 *     TX) at the module's default 9600 baud. If you've re-paired
 *     or reconfigured your HM-10 at a different baud, update
 *     BT_BAUD below.
 *   - HM-10 STATE pin -> PA8 (new). This is what lets the board
 *     detect a dropped BLE link instantly instead of only via
 *     the serial command timeout. If you don't wire it, the pin
 *     is pulled low internally and just reads as "disconnected"
 *     forever — harmless, the timeout-based safety stop still
 *     works on its own.
 *   - A second UART (USART2, PA2/PA3) is broken out purely for
 *     debug logging over a USB-TTL adapter. It's optional — the
 *     robot runs fine with nothing connected there.
 *   - ENA/ENB (motor speed) are on PA0/PA1, both PWM-capable
 *     timer pins on the Blackpill. IN1-IN4 are plain digital
 *     outputs — any free GPIO works, so double-check these
 *     against your actual wiring before flashing.
 *   - Onboard LED (PC13, active-low) blinks as a heartbeat —
 *     fast while driving, slow while idle — so you can tell the
 *     board is alive without a debug console open.
 * ============================================================ */

#include <Arduino.h>

// ============================================================
// SECTION: Bluetooth (HM-10) — USART1
// ============================================================
#define BT_TX_PIN     PA9
#define BT_RX_PIN     PA10
#define BT_BAUD       9600
#define BT_STATE_PIN  PA8   // wire to the HM-10's STATE pin
HardwareSerial btSerial(BT_RX_PIN, BT_TX_PIN);

// ============================================================
// SECTION: Debug UART — USART2 (optional, safe to leave
// unconnected)
// ============================================================
#define DEBUG_TX_PIN  PA2
#define DEBUG_RX_PIN  PA3
#define DEBUG_BAUD    115200
HardwareSerial dbgSerial(DEBUG_RX_PIN, DEBUG_TX_PIN);

// ============================================================
// SECTION: L298N motor driver pins
//   Left motor  -> OUT1/OUT2, direction = IN1/IN2, speed = ENA
//   Right motor -> OUT3/OUT4, direction = IN3/IN4, speed = ENB
// ============================================================
#define MOTOR_L_IN1  PB0
#define MOTOR_L_IN2  PB1
#define MOTOR_L_EN   PA0   // ENA, PWM

#define MOTOR_R_IN3  PB10
#define MOTOR_R_IN4  PB12
#define MOTOR_R_EN   PA1   // ENB, PWM

// ============================================================
// SECTION: Onboard LED heartbeat
// ============================================================
#define LED_PIN        PC13   // active LOW on the Blackpill
#define HEARTBEAT_MS   500

// ============================================================
// SECTION: Motion tuning
// ============================================================
// Kept conservative on purpose — ARGUS is meant to creep through
// unstable rubble, not race through it. 40% is the startup
// default; the operator can dial it up/down with a '0'-'9'
// command at any time.
#define DEFAULT_SPEED_PERCENT 40

// If no command arrives within this window while the robot is
// moving, assume the Bluetooth link dropped and stop rather than
// keep driving blind. This is the fallback safety net; the
// STATE-pin check below usually catches a drop faster than this.
#define COMMAND_TIMEOUT_MS 750

// ============================================================
// SECTION: HM-10 AT-command configuration
//
// The HM-10 only answers AT commands while it has no active BLE
// connection, so this has to run once, early, before anything
// pairs with it. Reply format varies by firmware/clone (genuine
// HM-10 vs. CC41-A-based boards etc.), so the setup routine logs
// whatever comes back rather than hard-failing on an unexpected
// or missing reply.
// ============================================================
#define BT_AT_CONFIG_ENABLED        1    // set to 0 to skip AT setup entirely (pure passthrough).
                                          // HM-10 settings persist in the module across power
                                          // cycles, so you typically only need this ON for the
                                          // first flash/boot -- then flip it off to save ~1-2s
                                          // of boot time on every boot after that.
#define BT_AT_INIT_DELAY_MS         1000 // let the module boot before talking AT to it
#define BT_AT_TIMEOUT_MS            300  // max wait for a reply if the module stays silent
#define BT_AT_INTERCOMMAND_DELAY_MS 50   // settle time between commands -- some clones miss
                                          // a command sent right after the previous one
#define BT_DEVICE_NAME      "ARGUS"

// Optional pairing PIN. Off by default -- turning it on means
// whatever phone/app connects will need to enter BT_PIN_CODE.
#define BT_REQUIRE_PIN  0
#define BT_PIN_CODE     "123456"   // must be exactly 6 digits

// NOTE ON CHANGING BAUD: the HM-10 also supports AT+BAUDx to
// change its UART speed, but that's deliberately not automated
// here. Send it and the module switches baud immediately -- every
// AT command and normal comm after that needs BT_BAUD updated and
// the board reflashed to match, or you just lose the link. If you
// want a non-default baud, set it once by hand with a USB-TTL
// adapter and AT+BAUDx, then update BT_BAUD above to match.

// ============================================================
// SECTION: State
// ============================================================
uint8_t       g_speedPwm      = (uint8_t)((DEFAULT_SPEED_PERCENT * 255) / 100);
unsigned long g_lastCommandMs = 0;
bool          g_isMoving      = false;
bool          g_btConnected   = false;

// ============================================================
// SECTION: Function prototypes
// ============================================================
void setupMotors();
void setSpeedPercent(uint8_t percent);
void driveForward();
void driveBackward();
void pivotLeft();
void pivotRight();
void stopMotors();
void handleCommand(char c);
void updateHeartbeatLed();
bool sendATCommand(const char* cmd, char* responseBuf, size_t bufLen,
                    unsigned long timeoutMs = BT_AT_TIMEOUT_MS);
void setupBluetoothModule();
void setupBluetoothStatePin();
void updateConnectionState();

// ============================================================
// setup()
// ============================================================
void setup() {
  dbgSerial.begin(DEBUG_BAUD);
  btSerial.begin(BT_BAUD);
  analogWriteResolution(8); // make the 0-255 PWM math below exact
  delay(200);

  dbgSerial.println();
  dbgSerial.println("=== ARGUS Body Module -- Boot ===");

  setupMotors();
  stopMotors();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // off (active-low)

  setupBluetoothStatePin();
#if BT_AT_CONFIG_ENABLED
  setupBluetoothModule(); // AT-config pass -- blocks for roughly a second or two
#else
  dbgSerial.println("[BT] AT-config skipped (BT_AT_CONFIG_ENABLED=0).");
#endif

  dbgSerial.print("[OK] Motors ready. Default speed: ");
  dbgSerial.print(DEFAULT_SPEED_PERCENT);
  dbgSerial.println("%");
  dbgSerial.println("[OK] Waiting for Bluetooth commands (F/B/L/R/S, 0-9)...");

  g_lastCommandMs = millis();
}

// ============================================================
// loop()
// ============================================================
void loop() {
  while (btSerial.available()) {
    handleCommand((char)btSerial.read());
  }

  updateConnectionState();

  // Safety: stop if the link's gone quiet while we were moving.
  if (g_isMoving && (millis() - g_lastCommandMs > COMMAND_TIMEOUT_MS)) {
    dbgSerial.println("[SAFETY] No command received in time -- stopping.");=
    stopMotors();
  }

  updateHeartbeatLed();
}

// ============================================================
// SECTION: Command handling
// ============================================================
void handleCommand(char c) {
  g_lastCommandMs = millis();

  if (c >= '0' && c <= '9') {
    uint8_t percent = (uint8_t)(((c - '0') * 100) / 9);
    setSpeedPercent(percent);
    dbgSerial.print("[CMD] Speed -> ");
    dbgSerial.print(percent);
    dbgSerial.println("%");
    return;
  }

  switch (c) {
    case 'F': case 'f':
      driveForward();
      dbgSerial.println("[CMD] Forward");
      break;
    case 'B': case 'b':
      driveBackward();
      dbgSerial.println("[CMD] Backward");
      break;
    case 'L': case 'l':
      pivotLeft();
      dbgSerial.println("[CMD] Pivot left");
      break;
    case 'R': case 'r':
      pivotRight();
      dbgSerial.println("[CMD] Pivot right");
      break;
    case 'S': case 's':
      stopMotors();
      dbgSerial.println("[CMD] Stop");
      break;
    default:
      // Ignore newlines, stray bytes, HM-10 status chatter, etc.
      break;
  }
}

// ============================================================
// SECTION: HM-10 AT-command configuration
// ============================================================

// Sends one AT command and captures whatever comes back. Returns
// early once the module has replied and then gone quiet for
// REPLY_IDLE_MS, rather than always blocking the full timeoutMs
// -- across several commands at boot that saves real time.
// timeoutMs is the fallback if the module never replies at all.
// Returns true if any bytes were received.
bool sendATCommand(const char* cmd, char* responseBuf, size_t bufLen,
                    unsigned long timeoutMs) {
  while (btSerial.available()) btSerial.read(); // drop stale bytes first

  btSerial.print(cmd);

  const unsigned long REPLY_IDLE_MS = 40;
  size_t idx = 0;
  unsigned long start = millis();
  unsigned long lastByteMs = start;
  while (millis() - start < timeoutMs && idx < bufLen - 1) {
    if (btSerial.available()) {
      responseBuf[idx++] = (char)btSerial.read();
      lastByteMs = millis();
    } else if (idx > 0 && (millis() - lastByteMs) >= REPLY_IDLE_MS) {
      break; // got a reply and it's gone quiet -- done, no need to wait out the timeout
    }
  }
  responseBuf[idx] = '\0';
  return idx > 0;
}

void setupBluetoothModule() {
  dbgSerial.println("[BT] Configuring HM-10 (AT mode)...");
  delay(BT_AT_INIT_DELAY_MS); // give the module time to boot before we talk AT to it

  char resp[64];
  char cmd[32];

  if (sendATCommand("AT", resp, sizeof(resp))) {
    dbgSerial.print("[BT] AT       -> "); dbgSerial.println(resp);
  } else {
    dbgSerial.println("[BT] No reply to AT -- module may already be paired, "
                       "or this clone doesn't echo bare AT. Continuing.");
  }
  delay(BT_AT_INTERCOMMAND_DELAY_MS);

  snprintf(cmd, sizeof(cmd), "AT+NAME%s", BT_DEVICE_NAME);
  if (sendATCommand(cmd, resp, sizeof(resp))) {
    dbgSerial.print("[BT] AT+NAME  -> "); dbgSerial.println(resp);
  }
  delay(BT_AT_INTERCOMMAND_DELAY_MS);

  if (sendATCommand("AT+ROLE0", resp, sizeof(resp))) { // 0 = peripheral/slave
    dbgSerial.print("[BT] AT+ROLE0 -> "); dbgSerial.println(resp);
  }
  delay(BT_AT_INTERCOMMAND_DELAY_MS);

#if BT_REQUIRE_PIN
  snprintf(cmd, sizeof(cmd), "AT+PASS%s", BT_PIN_CODE);
  if (sendATCommand(cmd, resp, sizeof(resp))) {
    dbgSerial.print("[BT] AT+PASS  -> "); dbgSerial.println(resp);
  }
  delay(BT_AT_INTERCOMMAND_DELAY_MS);

  if (sendATCommand("AT+TYPE2", resp, sizeof(resp))) { // 2 = require PIN auth
    dbgSerial.print("[BT] AT+TYPE2 -> "); dbgSerial.println(resp);
  }
  delay(BT_AT_INTERCOMMAND_DELAY_MS);
#endif

  while (btSerial.available()) btSerial.read(); // clear anything left before the main loop
  dbgSerial.println("[BT] HM-10 config pass done.");
}

// ============================================================
// SECTION: HM-10 connection-state monitoring (STATE pin)
//
// STATE goes high once a central connects and low once it
// disconnects -- the common behavior for boards sold as "HM-10",
// though it can vary by clone. Watch the debug log the first
// time you connect/disconnect to confirm yours matches. This
// gives an immediate electrical "link's gone" signal, on top of
// (not instead of) the serial command timeout above, which still
// covers modules where STATE isn't wired or doesn't behave this
// way.
// ============================================================
void setupBluetoothStatePin() {
  pinMode(BT_STATE_PIN, INPUT_PULLDOWN); // reads "disconnected" if left unwired
}

void updateConnectionState() {
  static bool lastRawState     = false;
  static unsigned long lastChangeMs = 0;
  const unsigned long DEBOUNCE_MS = 100;

  bool rawState = (digitalRead(BT_STATE_PIN) == HIGH);
  if (rawState != lastRawState) {
    lastRawState = rawState;
    lastChangeMs = millis();
  }

  if ((millis() - lastChangeMs) >= DEBOUNCE_MS && rawState != g_btConnected) {
    g_btConnected = rawState;
    if (g_btConnected) {
      dbgSerial.println("[BT] Link established (STATE high).");
    } else {
      dbgSerial.println("[BT] Link lost (STATE low) -- stopping now.");
      stopMotors(); // don't wait for COMMAND_TIMEOUT_MS -- we have a hard signal
    }
  }
}

// ============================================================
// SECTION: Motor control
// ============================================================
void setupMotors() {
  pinMode(MOTOR_L_IN1, OUTPUT);
  pinMode(MOTOR_L_IN2, OUTPUT);
  pinMode(MOTOR_L_EN,  OUTPUT);

  pinMode(MOTOR_R_IN3, OUTPUT);
  pinMode(MOTOR_R_IN4, OUTPUT);
  pinMode(MOTOR_R_EN,  OUTPUT);
}

void setSpeedPercent(uint8_t percent) {
  if (percent > 100) percent = 100;
  g_speedPwm = (uint8_t)((percent * 255) / 100);
  if (g_isMoving) {
    // Apply immediately if we're already driving, instead of
    // waiting for the next direction command.
    analogWrite(MOTOR_L_EN, g_speedPwm);
    analogWrite(MOTOR_R_EN, g_speedPwm);
  }
}

void driveForward() {
  digitalWrite(MOTOR_L_IN1, HIGH); digitalWrite(MOTOR_L_IN2, LOW);
  digitalWrite(MOTOR_R_IN3, HIGH); digitalWrite(MOTOR_R_IN4, LOW);
  analogWrite(MOTOR_L_EN, g_speedPwm);
  analogWrite(MOTOR_R_EN, g_speedPwm);
  g_isMoving = true;
}

void driveBackward() {
  digitalWrite(MOTOR_L_IN1, LOW); digitalWrite(MOTOR_L_IN2, HIGH);
  digitalWrite(MOTOR_R_IN3, LOW); digitalWrite(MOTOR_R_IN4, HIGH);
  analogWrite(MOTOR_L_EN, g_speedPwm);
  analogWrite(MOTOR_R_EN, g_speedPwm);
  g_isMoving = true;
}

void pivotLeft() {
  digitalWrite(MOTOR_L_IN1, LOW);  digitalWrite(MOTOR_L_IN2, HIGH); // left reverse
  digitalWrite(MOTOR_R_IN3, HIGH); digitalWrite(MOTOR_R_IN4, LOW);  // right forward
  analogWrite(MOTOR_L_EN, g_speedPwm);
  analogWrite(MOTOR_R_EN, g_speedPwm);
  g_isMoving = true;
}

void pivotRight() {
  digitalWrite(MOTOR_L_IN1, HIGH); digitalWrite(MOTOR_L_IN2, LOW);  // left forward
  digitalWrite(MOTOR_R_IN3, LOW);  digitalWrite(MOTOR_R_IN4, HIGH); // right reverse
  analogWrite(MOTOR_L_EN, g_speedPwm);
  analogWrite(MOTOR_R_EN, g_speedPwm);
  g_isMoving = true;
}

void stopMotors() {
  digitalWrite(MOTOR_L_IN1, LOW); digitalWrite(MOTOR_L_IN2, LOW);
  digitalWrite(MOTOR_R_IN3, LOW); digitalWrite(MOTOR_R_IN4, LOW);
  analogWrite(MOTOR_L_EN, 0);
  analogWrite(MOTOR_R_EN, 0);
  g_isMoving = false;
}

// ============================================================
// SECTION: Heartbeat LED
// Fast blink while driving, slow blink while idle -- a quick
// visual "is it alive / is it moving" check with no console.
// ============================================================
void updateHeartbeatLed() {
  static unsigned long lastToggle = 0;
  static bool ledOn = false;

  unsigned long interval = g_isMoving ? (HEARTBEAT_MS / 4) : HEARTBEAT_MS;

  if (millis() - lastToggle >= interval) {
    lastToggle = millis();
    ledOn = !ledOn;
    digitalWrite(LED_PIN, ledOn ? LOW : HIGH); // LOW = on (active-low)
  }
}