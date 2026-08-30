/*
 ARGUS Controller v3 - hardware integration layer

 Expected ESP32-S3 HTTP/WebSocket interface:
   GET  /stream                 -> MJPEG stream (camera)
   GET  /api/status             -> JSON status (optional)
   POST /api/command            -> {"command":"forward","speed":50}
   WS   /ws                     -> JSON telemetry and command channel
   GET  /api/thermal            -> optional JSON thermal frame
   POST /api/light              -> {"on":true}
   POST /api/mic                -> {"on":true}
   POST /api/speaker            -> {"on":true}

 WebSocket telemetry examples:
   {"type":"status","connected":true,"battery":84}
   {"type":"thermal","pixels":[...768 numbers...],"min":27.2,"max":38.4,"center":36.1}
   {"type":"ack","command":"forward"}
   {"type":"log","message":"HM-10 connected"}
*/

(() => {

  // =====================================================
  // CONNECTION VARIABLES
  // =====================================================

  const DEFAULT_ESP32 =
    localStorage.getItem("argusEsp32Url") || "";

  let baseUrl =
    DEFAULT_ESP32.replace(/\/+$/, "");

  let socket = null;

  let connected = false;

  let activeDrive = null;

  let speed = 50;

  let currentView = "camera";

  let micOn = false;

  let speakerOn = false;

  let lightOn = false;


  // =====================================================
  // SHORTCUT FOR GETTING HTML ELEMENTS
  // =====================================================

  const $ = id =>
    document.getElementById(id);


  const consoleEl =
    $("systemConsole");


  // =====================================================
  // CURRENT TIME
  // =====================================================

  function now() {

    return new Date().toLocaleTimeString(
      [],
      {
        hour12: false
      }
    );

  }


  // =====================================================
// SYSTEM CONSOLE
// =====================================================

function log(message, level = "normal") {
    if (!consoleEl) return;

    const line = document.createElement("div");

    // Set message color
    if (level === "error") {
        line.className = "mb-1 text-emergency-red";
    } else if (level === "warn") {
        line.className = "mb-1 text-primary-container";
    } else {
        line.className = "mb-1";
    }

    // Build message safely
    const time = now();

    line.innerHTML =
        '<span class="text-text-dim/50 mr-2">[' +
        escapeHtml(time) +
        ']</span> &gt; ' +
        escapeHtml(message);

    // Remove old cursor
    const oldCursor = consoleEl.querySelector(
        ".argus-console-cursor"
    );

    if (oldCursor) {
        oldCursor.remove();
    }

    // Add new message
    consoleEl.appendChild(line);

    // Add cursor
    const cursor = document.createElement("div");

    cursor.className =
        "argus-console-cursor mt-2 text-primary-container animate-pulse";

    cursor.textContent = "_";

    consoleEl.appendChild(cursor);

    // IMPORTANT:
    // Scroll only the console.
    // Do NOT use scrollIntoView().
    requestAnimationFrame(() => {
        consoleEl.scrollTop = consoleEl.scrollHeight;
    });
}


// =====================================================
// HTML ESCAPE
// =====================================================

function escapeHtml(value) {
    return String(value)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#39;");

}

  // CONNECTION STATE
  // =====================================================

  function setConnectionState(
    state,
    reason = ""
  ) {

    connected = state;


    const connectionText =
      $("connectionText");

    if (connectionText) {

      connectionText.textContent =
        state
          ? "[ CONNECTED ]"
          : "[ DISCONNECTED ]";

    }


    const badge =
      $("connectionBadge");


    if (badge) {

      badge.classList.toggle(
        "border-emergency-red/30",
        !state
      );

      badge.classList.toggle(
        "border-primary-container/50",
        state
      );

    }


    const connectBtn =
      $("connectBtn");


    if (connectBtn) {

      connectBtn.textContent =
        state
          ? "DISCONNECT"
          : "CONNECT";

    }


    const bluetoothStatus =
      $("bluetoothStatus");


    if (bluetoothStatus) {

      bluetoothStatus.textContent =
        state
          ? "ESP32 LINK"
          : "DISCONNECTED";


      bluetoothStatus.className =
        state
          ? "font-data-mono text-[10px] text-primary-container font-bold"
          : "font-data-mono text-[10px] text-emergency-red font-bold";

    }


    if (reason) {

      log(
        reason,
        state
          ? "normal"
          : "error"
      );

    }

  }


  // =====================================================
  // NORMALIZE ESP32 ADDRESS
  // =====================================================

  function normalizeBase(url) {

    if (!url) return "";


    url = url.trim();


    if (!/^https?:\/\//i.test(url)) {

      url =
        "http://" + url;

    }


    return url.replace(
      /\/+$/,
      ""
    );

  }


  // =====================================================
  // HTTP COMMAND
  // =====================================================

  async function httpCommand(
    command,
    extra = {}
  ) {

    if (!baseUrl) {

      log(
        "No ESP32-S3 URL configured.",
        "warn"
      );

      return false;

    }


    try {

      const response =
        await fetch(
          `${baseUrl}/api/command`,
          {
            method: "POST",

            headers: {
              "Content-Type":
                "application/json"
            },

            body:
              JSON.stringify({
                command,
                ...extra
              })
          }
        );


      if (!response.ok) {

        throw new Error(
          `HTTP ${response.status}`
        );

      }


      return true;

    }

    catch (e) {

      log(
        `Command ${command} failed: ${e.message}`,
        "error"
      );


      setConnectionState(false);


      return false;

    }

  }


  // =====================================================
  // SEND COMMAND
  // =====================================================

  function sendCommand(
    command,
    extra = {}
  ) {

    const payload = {

      type: "command",

      command,

      speed,

      ...extra

    };


    // Prefer WebSocket

    if (
      socket &&
      socket.readyState ===
        WebSocket.OPEN
    ) {

      socket.send(
        JSON.stringify(payload)
      );

      return true;

    }


    // HTTP fallback

    httpCommand(
      command,
      {
        speed,
        ...extra
      }
    );


    return false;

  }


  // =====================================================
  // CONNECT WEBSOCKET
  // =====================================================

  function connectSocket() {

    if (!baseUrl) return;


    const wsUrl =
      baseUrl.replace(
        /^http/i,
        "ws"
      ) + "/ws";


    try {

      socket =
        new WebSocket(wsUrl);


      socket.onopen = () => {

        setConnectionState(
          true,
          `WebSocket connected: ${wsUrl}`
        );


        socket.send(
          JSON.stringify({
            type: "hello",
            client: "ARGUS-WEB",
            version: "3.0"
          })
        );


        updateSensorState();

      };


      socket.onmessage =
        event => {

          try {

            handleMessage(
              JSON.parse(event.data)
            );

          }

          catch {

            log(
              `WS: ${event.data}`
            );

          }

        };


      socket.onerror =
        () => {

          log(
            "WebSocket error.",
            "error"
          );

        };


      socket.onclose =
        () => {

          if (connected) {

            log(
              "ESP32 WebSocket closed.",
              "warn"
            );

          }


          setConnectionState(false);


          socket = null;

        };

    }

    catch (e) {

      log(
        `WebSocket unavailable: ${e.message}`,
        "error"
      );

    }

  }


  // =====================================================
  // OPEN CONNECTION WINDOW
  // =====================================================

  async function connect() {

    const modal =
      $("connectionModal");


    if (!modal) return;


    modal.classList.remove(
      "hidden"
    );


    modal.classList.add(
      "flex"
    );


    const input =
      $("esp32Url");


    if (input) {

      input.value =
        baseUrl;

    }

  }


  // =====================================================
  // ACTUALLY CONNECT
  // =====================================================

  async function doConnect() {

    baseUrl =
      normalizeBase(
        $("esp32Url").value
      );


    if (!baseUrl) {

      log(
        "Enter the ESP32-S3 IP address or hostname.",
        "warn"
      );

      return;

    }


    localStorage.setItem(
      "argusEsp32Url",
      baseUrl
    );


    $("connectionModal")
      .classList.add("hidden");


    $("connectionModal")
      .classList.remove("flex");


    log(
      `Connecting to ESP32-S3 at ${baseUrl}...`
    );


    try {

      const r =
        await fetch(
          `${baseUrl}/api/status`,
          {
            cache: "no-store"
          }
        );


      if (r.ok) {

        const data =
          await r.json();


        handleMessage({
          type: "status",
          ...data
        });

      }


      setConnectionState(
        true,
        "ESP32-S3 HTTP link responding."
      );

    }

    catch (e) {

      log(
        `HTTP status check failed: ${e.message}. Trying WebSocket...`,
        "warn"
      );

    }


    connectSocket();


    updateSensorState();

  }


  // =====================================================
  // DISCONNECT
  // =====================================================

  function disconnect() {

    if (socket) {

      socket.close();

    }


    socket = null;


    setConnectionState(
      false,
      "Disconnected by operator."
    );


    stopDrive();

  }


  // =====================================================
  // SENSOR VIEW
  // =====================================================

  function updateSensorState() {

    const camera =
      $("cameraFeed");


    const thermal =
      $("thermalCanvas");


    const placeholder =
      $("sensorPlaceholder");


    const thermalReadout =
      $("thermalReadout");


    if (
      !camera ||
      !thermal ||
      !placeholder
    ) {

      return;

    }


    // CAMERA

    if (
      currentView ===
      "camera"
    ) {

      $("viewModeLabel")
        .textContent =
        "CAMERA FEED";


      camera.classList.remove(
        "hidden"
      );


      thermal.classList.add(
        "hidden"
      );


      if (thermalReadout) {

        thermalReadout.classList.add(
          "hidden"
        );

      }


      $("sensorPlaceholderIcon")
        .textContent =
        connected
          ? "videocam"
          : "videocam_off";


      $("sensorPlaceholderText")
        .textContent =
        connected
          ? "WAITING FOR CAMERA STREAM"
          : "WAITING FOR ESP32-S3";


      if (
        connected &&
        baseUrl
      ) {

        const stream =
          `${baseUrl}/stream`;


        if (
          camera.src !==
          stream
        ) {

          camera.src =
            stream;

        }


        camera.onload =
          () => {

            placeholder.classList.add(
              "hidden"
            );

          };


        camera.onerror =
          () => {

            placeholder.classList.remove(
              "hidden"
            );


            $("sensorPlaceholderText")
              .textContent =
              "CAMERA STREAM UNAVAILABLE";

          };

      }

      else {

        camera.removeAttribute(
          "src"
        );


        placeholder.classList.remove(
          "hidden"
        );

      }

    }


    // THERMAL

    else {

      $("viewModeLabel")
        .textContent =
        "THERMAL";


      camera.classList.add(
        "hidden"
      );


      thermal.classList.remove(
        "hidden"
      );


      if (thermalReadout) {

        thermalReadout.classList.remove(
          "hidden"
        );

      }


      placeholder.classList.toggle(
        "hidden",
        connected
      );


      $("sensorPlaceholderIcon")
        .textContent =
        "device_thermostat";


      $("sensorPlaceholderText")
        .textContent =
        connected
          ? "WAITING FOR MLX90640"
          : "WAITING FOR ESP32-S3";


      if (connected) {

        pollThermal();

      }

    }

  }


  // =====================================================
  // POLL THERMAL SENSOR
  // =====================================================

  async function pollThermal() {

    if (
      currentView !== "thermal" ||
      !connected ||
      !baseUrl
    ) {

      return;

    }


    try {

      const r =
        await fetch(
          `${baseUrl}/api/thermal`,
          {
            cache: "no-store"
          }
        );


      if (!r.ok) {

        throw new Error(
          `HTTP ${r.status}`
        );

      }


      const data =
        await r.json();


      handleMessage({
        type: "thermal",
        ...data
      });

    }

    catch (_) {

      // WebSocket telemetry may
      // be used instead.

    }


    if (
      currentView ===
      "thermal"
    ) {

      setTimeout(
        pollThermal,
        250
      );

    }

  }


  // =====================================================
  // SWITCH CAMERA / THERMAL
  // =====================================================

  function setView(view) {

    currentView =
      view;


    const cameraBtn =
      $("cameraBtn");


    const thermalBtn =
      $("thermalBtn");


    if (cameraBtn) {

      cameraBtn.classList.toggle(
        "text-primary-container",
        view === "camera"
      );


      cameraBtn.classList.toggle(
        "border-primary-container/50",
        view === "camera"
      );


      cameraBtn.classList.toggle(
        "bg-surface-panel",
        view === "camera"
      );

    }


    if (thermalBtn) {

      thermalBtn.classList.toggle(
        "text-primary-container",
        view === "thermal"
      );


      thermalBtn.classList.toggle(
        "border-primary-container/50",
        view === "thermal"
      );


      thermalBtn.classList.toggle(
        "bg-surface-panel",
        view === "thermal"
      );

    }


    sendCommand(
      view === "camera"
        ? "view_camera"
        : "view_thermal"
    );


    updateSensorState();


    log(
      `Sensor view: ${view.toUpperCase()}`
    );

  }


  // =====================================================
  // PRESS AND HOLD MOVEMENT
  // =====================================================

  function bindHoldButton(
    id,
    command
  ) {

    const btn =
      $(id);


    if (!btn) return;


    let held = false;


    const start =
      e => {

        e.preventDefault();


        if (held) return;


        held = true;


        activeDrive =
          command;


        btn.classList.add(
          "is-held"
        );


        sendCommand(
          command
        );


        log(
          `DRIVE ${command.toUpperCase()} @ ${speed}%`
        );

      };


    const end =
      e => {

        if (e) {

          e.preventDefault();

        }


        if (!held) return;


        held = false;


        btn.classList.remove(
          "is-held"
        );


        if (
          activeDrive ===
          command
        ) {

          stopDrive();

        }

      };


    btn.addEventListener(
      "pointerdown",
      start
    );


    btn.addEventListener(
      "pointerup",
      end
    );


    btn.addEventListener(
      "pointercancel",
      end
    );


    btn.addEventListener(
      "pointerleave",
      end
    );


    btn.addEventListener(
      "contextmenu",
      e =>
        e.preventDefault()
    );

  }


  // =====================================================
  // STOP ROBOT
  // =====================================================

  function stopDrive() {

    activeDrive =
      null;


    document
      .querySelectorAll(
        ".drive-btn"
      )
      .forEach(
        b =>
          b.classList.remove(
            "is-held"
          )
      );


    sendCommand(
      "stop"
    );

  }


  // =====================================================
  // TOGGLE FUNCTIONS
  // =====================================================

  function bindToggle(
    id,
    getState,
    setState,
    command,
    statusId
  ) {

    const btn =
      $(id);


    if (!btn) return;


    btn.addEventListener(
      "click",
      () => {

        const next =
          !getState();


        setState(
          next
        );


        const status =
          $(statusId);


        if (status) {

          status.textContent =
            next
              ? "ON"
              : "OFF";


          status.className =
            next
              ? "font-data-mono text-[10px] text-primary-container font-bold"
              : "font-data-mono text-[10px] text-text-dim font-bold";

        }


        btn.classList.toggle(
          "active",
          next
        );


        sendCommand(
          command,
          {
            on: next
          }
        );


        log(
          `${command.toUpperCase()}: ${
            next ? "ON" : "OFF"
          }`
        );

      }
    );

  }


  // =====================================================
  // PROCESS ESP32 MESSAGES
  // =====================================================

  function handleMessage(msg) {

    if (
      !msg ||
      typeof msg !==
        "object"
    ) {

      return;

    }


    // STATUS

    if (
      msg.type ===
      "status"
    ) {

      if (
        typeof msg.battery ===
        "number"
      ) {

        const battery =
          $("batteryValue");


        if (battery) {

          battery.textContent =
            `${Math.round(msg.battery)}%`;

        }

      }


      if (
        msg.connected !==
        undefined
      ) {

        setConnectionState(
          !!msg.connected
        );

      }


      if (
        msg.mic !==
        undefined
      ) {

        micOn =
          !!msg.mic;


        $("micStatus")
          .textContent =
          micOn
            ? "ON"
            : "OFF";

      }


      if (
        msg.speaker !==
        undefined
      ) {

        speakerOn =
          !!msg.speaker;


        $("speakerStatus")
          .textContent =
          speakerOn
            ? "ON"
            : "OFF";

      }


      if (
        msg.light !==
        undefined
      ) {

        lightOn =
          !!msg.light;


        $("lightStatus")
          .textContent =
          lightOn
            ? "ON"
            : "OFF";

      }

    }


    // THERMAL DATA

    else if (
      msg.type ===
      "thermal"
    ) {

      drawThermal(
        msg.pixels ||
        msg.data ||
        [],
        msg.min,
        msg.max,
        msg.center
      );

    }


    // COMMAND ACKNOWLEDGEMENT

    else if (
      msg.type ===
      "ack"
    ) {

      log(
        `ACK: ${
          msg.command ||
          "command"
        }`
      );

    }


    // SYSTEM LOG

    else if (
      msg.type ===
      "log"
    ) {

      log(
        msg.message ||
        "",
        msg.level ||
        "normal"
      );

    }


    // BATTERY UPDATE

    else if (
      msg.type ===
      "battery"
    ) {

      const value =
        Number(
          msg.value
        );


      if (
        Number.isFinite(
          value
        )
      ) {

        $("batteryValue")
          .textContent =
          `${Math.round(value)}%`;

      }

    }

  }


  // =====================================================
  // THERMAL COLOR MAP
  // =====================================================

  function thermalColor(
    t,
    min,
    max
  ) {

    let x =
      (t - min) /
      Math.max(
        0.001,
        max - min
      );


    x =
      Math.max(
        0,
        Math.min(
          1,
          x
        )
      );


    // Blue → Cyan → Green → Yellow → Red

    const stops = [

      [0, 20, 70, 180],

      [.25, 0, 190, 255],

      [.5, 40, 210, 100],

      [.75, 255, 220, 0],

      [1, 230, 40, 30]

    ];


    for (
      let i = 0;
      i < stops.length - 1;
      i++
    ) {

      if (
        x <=
        stops[i + 1][0]
      ) {

        const a =
          stops[i];


        const b =
          stops[i + 1];


        const q =
          (x - a[0]) /
          (b[0] - a[0]);


        return `rgb(
          ${Math.round(
            a[1] +
            (b[1] - a[1]) * q
          )},
          ${Math.round(
            a[2] +
            (b[2] - a[2]) * q
          )},
          ${Math.round(
            a[3] +
            (b[3] - a[3]) * q
          )}
        )`;

      }

    }


    return "rgb(230,40,30)";

  }


  // =====================================================
  // DRAW MLX90640 THERMAL DATA
  // =====================================================

  function drawThermal(
    pixels,
    minHint,
    maxHint,
    centerHint
  ) {

    // MLX90640 = 32 x 24 = 768 pixels

    if (
      !Array.isArray(pixels) ||
      pixels.length < 768
    ) {

      return;

    }


    const canvas =
      $("thermalCanvas");


    if (!canvas) return;


    const rect =
      canvas.getBoundingClientRect();


    const dpr =
      window.devicePixelRatio ||
      1;


    canvas.width =
      Math.max(
        1,
        Math.floor(
          rect.width * dpr
        )
      );


    canvas.height =
      Math.max(
        1,
        Math.floor(
          rect.height * dpr
        )
      );


    const ctx =
      canvas.getContext(
        "2d"
      );


    ctx.setTransform(
      dpr,
      0,
      0,
      dpr,
      0,
      0
    );


    const w =
      rect.width;


    const h =
      rect.height;


    const vals =
      pixels
        .slice(0, 768)
        .map(Number)
        .filter(
          Number.isFinite
        );


    if (!vals.length) {

      return;

    }


    const min =
      Number.isFinite(
        Number(minHint)
      )
        ? Number(minHint)
        : Math.min(...vals);


    const max =
      Number.isFinite(
        Number(maxHint)
      )
        ? Number(maxHint)
        : Math.max(...vals);


    const tmp =
      document.createElement(
        "canvas"
      );


    const tw = 32;

    const th = 24;


    tmp.width =
      tw;


    tmp.height =
      th;


    const tc =
      tmp.getContext(
        "2d"
      );


    const image =
      tc.createImageData(
        tw,
        th
      );


    for (
      let y = 0;
      y < 24;
      y++
    ) {

      for (
        let x = 0;
        x < 32;
        x++
      ) {

        const color =
          thermalColor(
            vals[
              y * 32 + x
            ],
            min,
            max
          )
          .match(
            /\d+/g
          )
          .map(Number);


        const i =
          (
            y * 32 + x
          ) * 4;


        image.data[i] =
          color[0];


        image.data[i + 1] =
          color[1];


        image.data[i + 2] =
          color[2];


        image.data[i + 3] =
          255;

      }

    }


    tc.putImageData(
      image,
      0,
      0
    );


    ctx.imageSmoothingEnabled =
      true;


    ctx.drawImage(
      tmp,
      0,
      0,
      w,
      h
    );


    const thermalMax =
      $("thermalMax");


    const thermalMin =
      $("thermalMin");


    const thermalCenter =
      $("thermalCenter");


    if (thermalMax) {

      thermalMax.textContent =
        Number(
          maxHint ?? max
        ).toFixed(1);

    }


    if (thermalMin) {

      thermalMin.textContent =
        Number(
          minHint ?? min
        ).toFixed(1);

    }


    const center =
      Number.isFinite(
        Number(centerHint)
      )
        ? Number(centerHint)
        : vals[
            12 * 32 + 16
          ];


    if (thermalCenter) {

      thermalCenter.textContent =
        center.toFixed(1);

    }


    const placeholder =
      $("sensorPlaceholder");


    if (placeholder) {

      placeholder.classList.add(
        "hidden"
      );

    }

  }


  // =====================================================
  // UI EVENTS
  // =====================================================

  const connectBtn =
    $("connectBtn");


  if (connectBtn) {

    connectBtn.addEventListener(
      "click",
      () => {

        connected
          ? disconnect()
          : connect();

      }
    );

  }


  const closeConnection =
    $("closeConnection");


  if (closeConnection) {

    closeConnection.addEventListener(
      "click",
      () => {

        $("connectionModal")
          .classList.add(
            "hidden"
          );


        $("connectionModal")
          .classList.remove(
            "flex"
          );

      }
    );

  }


  const cancelConnection =
    $("cancelConnection");


  if (cancelConnection) {

    cancelConnection.addEventListener(
      "click",
      () => {

        $("connectionModal")
          .classList.add(
            "hidden"
          );


        $("connectionModal")
          .classList.remove(
            "flex"
          );

      }
    );

  }


  const saveConnection =
    $("saveConnection");


  if (saveConnection) {

    saveConnection.addEventListener(
      "click",
      doConnect
    );

  }


  // =====================================================
  // SPEED
  // =====================================================

  const speedSlider =
    $("speedSlider");


  if (speedSlider) {

    speedSlider.addEventListener(
      "input",
      e => {

        speed =
          Number(
            e.target.value
          );


        const speedValue =
          $("speedValue");


        if (speedValue) {

          speedValue.textContent =
            `${speed}%`;

        }

      }
    );

  }


  // =====================================================
  // EMERGENCY STOP
  // =====================================================

  const estopBtn =
    $("estopBtn");


  if (estopBtn) {

    estopBtn.addEventListener(
      "click",
      () => {

        stopDrive();


        sendCommand(
          "estop"
        );


        log(
          "EMERGENCY STOP COMMAND SENT.",
          "error"
        );

      }
    );

  }


  // =====================================================
  // MOVEMENT BUTTONS
  // =====================================================

  bindHoldButton(
    "forwardBtn",
    "forward"
  );


  bindHoldButton(
    "reverseBtn",
    "reverse"
  );


  bindHoldButton(
    "leftBtn",
    "left"
  );


  bindHoldButton(
    "rightBtn",
    "right"
  );


  const stopBtn =
    $("stopBtn");


  if (stopBtn) {

    stopBtn.addEventListener(
      "click",
      stopDrive
    );

  }


  // =====================================================
  // CAMERA / THERMAL
  // =====================================================

  const cameraBtn =
    $("cameraBtn");


  if (cameraBtn) {

    cameraBtn.addEventListener(
      "click",
      () =>
        setView("camera")
    );

  }


  const thermalBtn =
    $("thermalBtn");


  if (thermalBtn) {

    thermalBtn.addEventListener(
      "click",
      () =>
        setView("thermal")
    );

  }


  // =====================================================
  // MICROPHONE
  // =====================================================

  bindToggle(
    "micBtn",

    () => micOn,

    v => micOn = v,

    "microphone",

    "micStatus"
  );


  // =====================================================
  // SPEAKER
  // =====================================================

  bindToggle(
    "speakerBtn",

    () => speakerOn,

    v => speakerOn = v,

    "speaker",

    "speakerStatus"
  );


  // =====================================================
  // LIGHT
  // =====================================================

  bindToggle(
    "lightBtn",

    () => lightOn,

    v => lightOn = v,

    "light",

    "lightStatus"
  );


  // =====================================================
  // BLUETOOTH
  // =====================================================

  const bluetoothBtn =
    $("bluetoothBtn");


  if (bluetoothBtn) {

    bluetoothBtn.addEventListener(
      "click",
      () => {

        log(
          "Bluetooth link is handled by ESP32-S3 → HM-10 → STM32.",
          "warn"
        );

      }
    );

  }


  // =====================================================
  // CLEAR SYSTEM CONSOLE
  // =====================================================

  const clearConsole =
    $("clearConsole");


  if (clearConsole) {

    clearConsole.addEventListener(
      "click",
      () => {

        consoleEl.innerHTML =
          '<div class="mt-2 text-primary-container animate-pulse">_</div>';

      }
    );

  }


  // =====================================================
  // KEYBOARD CONTROL
  // =====================================================

  const keys =
    new Map([

      [
        "ArrowUp",
        "forward"
      ],

      [
        "w",
        "forward"
      ],

      [
        "ArrowDown",
        "reverse"
      ],

      [
        "s",
        "reverse"
      ],

      [
        "ArrowLeft",
        "left"
      ],

      [
        "a",
        "left"
      ],

      [
        "ArrowRight",
        "right"
      ],

      [
        "d",
        "right"
      ]

    ]);


  window.addEventListener(
    "keydown",
    e => {

      if (e.repeat) return;


      const cmd =
        keys.get(e.key);


      if (
        cmd &&
        document.activeElement?.tagName !==
          "INPUT"
      ) {

        e.preventDefault();


        const btn =
          document.querySelector(
            `[data-command="${cmd}"]`
          );


        if (btn) {

          btn.dispatchEvent(
            new PointerEvent(
              "pointerdown",
              {
                bubbles: true
              }
            )
          );

        }

      }


      // SPACE = STOP

      if (
        e.key === " " &&
        document.activeElement?.tagName !==
          "INPUT"
      ) {

        e.preventDefault();


        stopDrive();

      }

    }
  );


  window.addEventListener(
    "keyup",
    e => {

      const cmd =
        keys.get(e.key);


      if (cmd) {

        const btn =
          document.querySelector(
            `[data-command="${cmd}"]`
          );


        if (btn) {

          btn.dispatchEvent(
            new PointerEvent(
              "pointerup",
              {
                bubbles: true
              }
            )
          );

        }

      }

    }
  );


  // =====================================================
  // SAFETY
  // STOP ROBOT IF PAGE LOSES FOCUS
  // =====================================================

  window.addEventListener(
    "blur",
    stopDrive
  );


  document.addEventListener(
    "visibilitychange",
    () => {

      if (
        document.hidden
      ) {

        stopDrive();

      }

    }
  );


  // =====================================================
  // INITIAL STATE
  // =====================================================

  setView(
    "camera"
  );


  if ($("speedSlider")) {

    $("speedSlider").value =
      speed;

  }


  if ($("speedValue")) {

    $("speedValue").textContent =
      `${speed}%`;

  }


  updateSensorState();


  if (baseUrl) {

  log(
    `Saved ESP32-S3 URL: ${baseUrl}`
  );

}

})();
// ======================================================
// ARGUS BUTTON CONTROLS - SAFE BUTTON SNIPPET
// ======================================================

document.addEventListener("DOMContentLoaded", () => {

    // -------------------------
    // Helper: send command
    // -------------------------
    function sendCommand(command) {
        console.log("[COMMAND] " + command);

        // Prevent the page from moving when a control is pressed
        window.scrollTo({
            top: window.scrollY,
            behavior: "instant"
        });

        // Add command to system console
        const consoleEl = document.getElementById("systemConsole");

        if (consoleEl) {
            const line = document.createElement("div");

            line.className = "mb-1";

            const time = new Date().toLocaleTimeString();

            line.innerHTML =
                `<span class="text-text-dim/50 mr-2">[${time}]</span>` +
                `&gt; COMMAND: ${command.toUpperCase()}`;

            consoleEl.appendChild(line);

            // Keep ONLY the console at the bottom
            consoleEl.scrollTop = consoleEl.scrollHeight;
        }
    }


    // ==================================================
    // MOVEMENT BUTTONS
    // ==================================================

    const movementButtons = {
        forwardBtn: "forward",
        reverseBtn: "reverse",
        leftBtn: "left",
        rightBtn: "right",
        stopBtn: "stop"
    };


    Object.entries(movementButtons).forEach(([id, command]) => {

        const button = document.getElementById(id);

        if (!button) {
            console.warn("Button not found:", id);
            return;
        }

        // Mouse / touchscreen press
        button.addEventListener("pointerdown", (event) => {
            event.preventDefault();

            button.setPointerCapture?.(event.pointerId);

            sendCommand(command);
        });

        // Release button
        button.addEventListener("pointerup", (event) => {
            event.preventDefault();

            // Stop automatically after releasing
            if (command !== "stop") {
                sendCommand("stop");
            }
        });

        // If finger/mouse leaves the button
        button.addEventListener("pointercancel", () => {
            if (command !== "stop") {
                sendCommand("stop");
            }
        });

        button.addEventListener("contextmenu", (event) => {
            event.preventDefault();
        });
    });


    // ==================================================
    // EMERGENCY STOP
    // ==================================================

    const estopBtn = document.getElementById("estopBtn");

    if (estopBtn) {
        estopBtn.addEventListener("click", (event) => {
            event.preventDefault();

            sendCommand("stop");

            console.log("[EMERGENCY STOP]");
        });
    }


    // ==================================================
    // CAMERA / THERMAL SWITCH
    // ==================================================

    const cameraBtn = document.getElementById("cameraBtn");
    const thermalBtn = document.getElementById("thermalBtn");

    const cameraFeed = document.getElementById("cameraFeed");
    const thermalCanvas = document.getElementById("thermalCanvas");

    const viewModeLabel = document.getElementById("viewModeLabel");

    if (cameraBtn) {
        cameraBtn.addEventListener("click", (event) => {

            event.preventDefault();

            if (cameraFeed) {
                cameraFeed.classList.remove("hidden");
            }

            if (thermalCanvas) {
                thermalCanvas.classList.add("hidden");
            }

            if (viewModeLabel) {
                viewModeLabel.textContent = "CAMERA FEED";
            }

            console.log("[VIEW] CAMERA");
        });
    }


    if (thermalBtn) {
        thermalBtn.addEventListener("click", (event) => {

            event.preventDefault();

            if (cameraFeed) {
                cameraFeed.classList.add("hidden");
            }

            if (thermalCanvas) {
                thermalCanvas.classList.remove("hidden");
            }

            if (viewModeLabel) {
                viewModeLabel.textContent = "THERMAL FEED";
            }

            console.log("[VIEW] THERMAL");
        });
    }


    // ==================================================
    // SPEED SLIDER
    // ==================================================

    const speedSlider = document.getElementById("speedSlider");
    const speedValue = document.getElementById("speedValue");

    if (speedSlider && speedValue) {

        speedSlider.addEventListener("input", () => {

            const speed = speedSlider.value;

            speedValue.textContent = speed + "%";

            console.log("[SPEED] " + speed + "%");
        });
    }


    // ==================================================
    // PREVENT PAGE SCROLLING WHILE USING CONTROLS
    // ==================================================

    document.querySelectorAll(".drive-btn").forEach(button => {

        button.addEventListener("touchstart", (event) => {
            event.preventDefault();
        }, { passive: false });

        button.addEventListener("touchmove", (event) => {
            event.preventDefault();
        }, { passive: false });

        button.addEventListener("touchend", (event) => {
            event.preventDefault();
        }, { passive: false });

    });


    console.log("[ARGUS] Button controls initialized.");
});