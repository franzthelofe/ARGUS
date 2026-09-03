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
<<<<<<< HEAD

<<<<<<<< HEAD:website/src/script.js
========
function toEmbedded(cmd) {
  const dirMap = { forward: 'W', left: 'A', reverse: 'S', right: 'D', stop: 'X' };
  const dir = dirMap[cmd] || cmd;

  fetch('/cmd', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'dir=' + encodeURIComponent(dir),
  });
}

>>>>>>>> head:website/backup/script.js
=======
// =====================================================
// DATA TRANSMITTER
// =====================================================

function sendDirection(dirCMD, dirLR) {
  const dirMap = { forward: 'F', reverse: 'R', stop: 'X' };

  let body = '';

  if (dirCMD !== null && dirCMD !== undefined) {
    const dir = dirMap[dirCMD] || dirCMD;
    body += 'dirCMD=' + encodeURIComponent(dir);
  }

  if (dirLR !== null && dirLR !== undefined) {
    if (body) body += '&';
    body += 'dirLR=' + encodeURIComponent(dirLR);
  }

  fetch('/dir', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body,
  });
}

function sendSpeed(speed) {
  const body = 'speed=' + encodeURIComponent(speed);
  fetch('/speed', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body,
  });
}

(() => {
  // =====================================================
  // CONNECTION VARIABLES
  // =====================================================

  const DEFAULT_ESP32 = localStorage.getItem("argusEsp32Url") || "";

  let baseUrl = DEFAULT_ESP32.replace(/\/+$/, "");
  let socket = null;
  let connected = false;
  let activeDrive = null;
=======
>>>>>>> head
  let speed = 50;
  let currentView = "camera";
  let micOn = false;
  let speakerOn = false;
  let lightOn = false;

  // =====================================================
  // DOM HELPERS
  // =====================================================

  const $ = (id) => document.getElementById(id);
  const consoleEl = $("systemConsole");

  // =====================================================
  // CONSOLE / LOGGING
  // =====================================================

  function now() {
    return new Date().toLocaleTimeString([], { hour12: false });
  }

  function escapeHtml(value) {
    return String(value)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;")
      .replace(/'/g, "&#39;");
  }

  function log(message, level = "normal") {
    if (!consoleEl) return;

    const line = document.createElement("div");
    line.className =
      level === "error"
        ? "mb-1 text-emergency-red"
        : level === "warn"
          ? "mb-1 text-primary-container"
          : "mb-1";

    line.innerHTML = `<span class="text-text-dim/50 mr-2">[${escapeHtml(now())}]</span> &gt; ${escapeHtml(message)}`;

    const oldCursor = consoleEl.querySelector(".argus-console-cursor");
    if (oldCursor) oldCursor.remove();

    consoleEl.appendChild(line);

    const cursor = document.createElement("div");
    cursor.className =
      "argus-console-cursor mt-2 text-primary-container animate-pulse";
    cursor.textContent = "_";
    consoleEl.appendChild(cursor);

    // Scroll only the console, never the page.
    requestAnimationFrame(() => {
      consoleEl.scrollTop = consoleEl.scrollHeight;
    });
  }

  // =====================================================
  // CONNECTION STATE
  // =====================================================

  function setConnectionState(state, reason = "") {
    connected = state;

    const connectionText = $("connectionText");
    if (connectionText) {
      connectionText.textContent = state ? "[ CONNECTED ]" : "[ DISCONNECTED ]";
    }

    const badge = $("connectionBadge");
    if (badge) {
      badge.classList.toggle("border-emergency-red/30", !state);
      badge.classList.toggle("border-primary-container/50", state);
    }

    const connectBtn = $("connectBtn");
    if (connectBtn) {
      connectBtn.textContent = state ? "DISCONNECT" : "CONNECT";
    }

    const bluetoothStatus = $("bluetoothStatus");
    if (bluetoothStatus) {
      bluetoothStatus.textContent = state ? "ESP32 LINK" : "DISCONNECTED";
      bluetoothStatus.className = state
        ? "font-data-mono text-[10px] text-primary-container font-bold"
        : "font-data-mono text-[10px] text-emergency-red font-bold";
    }

    if (reason) {
      log(reason, state ? "normal" : "error");
    }
  }

  // =====================================================
  // HTTP COMMANDS
  // =====================================================

  function normalizeBase(url) {
    if (!url) return "";
    url = url.trim();
    if (!/^https?:\/\//i.test(url)) {
      url = "http://" + url;
    }
    return url.replace(/\/+$/, "");
  }

<<<<<<< HEAD
  async function httpCommand(command, extra = {}) {
    if (!baseUrl) {
      log("No ESP32-S3 URL configured.", "warn");
      return false;
    }

    try {
      const response = await fetch(`${baseUrl}/api/command`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ command, ...extra }),
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }

      return true;
    } catch (e) {
      log(`Command ${command} failed: ${e.message}`, "error");
      setConnectionState(false);
      return false;
    }
  }

=======
>>>>>>> head
  // =====================================================
  // WEBSOCKET
  // =====================================================

  function connectSocket() {
    if (!baseUrl) return;

    const wsUrl = baseUrl.replace(/^http/i, "ws") + "/ws";

    try {
      socket = new WebSocket(wsUrl);

      socket.onopen = () => {
        setConnectionState(true, `WebSocket connected: ${wsUrl}`);
        socket.send(
          JSON.stringify({
            type: "hello",
            client: "ARGUS-WEB",
            version: "3.0",
          }),
        );
        updateSensorState();
      };

      socket.onmessage = (event) => {
        try {
          handleMessage(JSON.parse(event.data));
        } catch {
          log(`WS: ${event.data}`);
        }
      };

      socket.onerror = () => {
        log("WebSocket error.", "error");
      };

      socket.onclose = () => {
        if (connected) {
          log("ESP32 WebSocket closed.", "warn");
        }
        setConnectionState(false);
        socket = null;
      };
    } catch (e) {
      log(`WebSocket unavailable: ${e.message}`, "error");
    }
  }

  // =====================================================
  // CONNECTION / DISCONNECTION
  // =====================================================

  async function connect() {
    const modal = $("connectionModal");
    if (!modal) return;

    modal.classList.remove("hidden");
    modal.classList.add("flex");

    const input = $("esp32Url");
    if (input) {
      input.value = baseUrl;
    }
  }

  async function doConnect() {
    const input = $("esp32Url");
    baseUrl = normalizeBase(input ? input.value : "");

    if (!baseUrl) {
      log("Enter the ESP32-S3 IP address or hostname.", "warn");
      return;
    }

    localStorage.setItem("argusEsp32Url", baseUrl);

    const modal = $("connectionModal");
    if (modal) {
      modal.classList.add("hidden");
      modal.classList.remove("flex");
    }

    log(`Connecting to ESP32-S3 at ${baseUrl}...`);

    try {
      const r = await fetch(`${baseUrl}/api/status`, { cache: "no-store" });

      if (r.ok) {
        const data = await r.json();
        handleMessage({ type: "status", ...data });
      }

      setConnectionState(true, "ESP32-S3 HTTP link responding.");
    } catch (e) {
      log(
        `HTTP status check failed: ${e.message}. Trying WebSocket...`,
        "warn",
      );
    }

    connectSocket();
    updateSensorState();
  }

<<<<<<< HEAD
  function disconnect() {
    if (socket) {
      socket.close();
    }

    socket = null;
    setConnectionState(false, "Disconnected by operator.");
    stopDrive();
  }

=======
>>>>>>> head
  // =====================================================
  // CAMERA / THERMAL
  // =====================================================

  function updateSensorState() {
    const camera = $("cameraFeed");
    const thermal = $("thermalCanvas");
    const placeholder = $("sensorPlaceholder");
    const thermalReadout = $("thermalReadout");

    if (!camera || !thermal || !placeholder) return;

    if (currentView === "camera") {
      const viewModeLabel = $("viewModeLabel");
      if (viewModeLabel) viewModeLabel.textContent = "CAMERA FEED";

      camera.classList.remove("hidden");
      thermal.classList.add("hidden");
      if (thermalReadout) thermalReadout.classList.add("hidden");

      const icon = $("sensorPlaceholderIcon");
      if (icon) icon.textContent = connected ? "videocam" : "videocam_off";

      const text = $("sensorPlaceholderText");
      if (text)
        text.textContent = connected
          ? "WAITING FOR CAMERA STREAM"
          : "WAITING FOR ESP32-S3";

      if (connected && baseUrl) {
        const stream = `${baseUrl}/stream`;
        if (camera.src !== stream) camera.src = stream;

        camera.onload = () => placeholder.classList.add("hidden");
        camera.onerror = () => {
          placeholder.classList.remove("hidden");
          if (text) text.textContent = "CAMERA STREAM UNAVAILABLE";
        };
      } else {
        camera.removeAttribute("src");
        placeholder.classList.remove("hidden");
      }
    } else {
      const viewModeLabel = $("viewModeLabel");
      if (viewModeLabel) viewModeLabel.textContent = "THERMAL";

      camera.classList.add("hidden");
      thermal.classList.remove("hidden");
      if (thermalReadout) thermalReadout.classList.remove("hidden");

      placeholder.classList.toggle("hidden", connected);

      const icon = $("sensorPlaceholderIcon");
      if (icon) icon.textContent = "device_thermostat";

      const text = $("sensorPlaceholderText");
      if (text)
        text.textContent = connected
          ? "WAITING FOR MLX90640"
          : "WAITING FOR ESP32-S3";

      if (connected) pollThermal();
    }
  }

  async function pollThermal() {
    if (currentView !== "thermal" || !connected || !baseUrl) return;

    try {
      const r = await fetch(`${baseUrl}/api/thermal`, { cache: "no-store" });
      if (!r.ok) throw new Error(`HTTP ${r.status}`);

      const data = await r.json();
      handleMessage({ type: "thermal", ...data });
    } catch (_) {
      // WebSocket telemetry may be used instead.
    }

    if (currentView === "thermal") {
      setTimeout(pollThermal, 250);
    }
  }

<<<<<<< HEAD
  function setView(view) {
    currentView = view;

    const cameraBtn = $("cameraBtn");
    const thermalBtn = $("thermalBtn");

    if (cameraBtn) {
      cameraBtn.classList.toggle("text-primary-container", view === "camera");
      cameraBtn.classList.toggle(
        "border-primary-container/50",
        view === "camera",
      );
      cameraBtn.classList.toggle("bg-surface-panel", view === "camera");
    }

    if (thermalBtn) {
      thermalBtn.classList.toggle("text-primary-container", view === "thermal");
      thermalBtn.classList.toggle(
        "border-primary-container/50",
        view === "thermal",
      );
      thermalBtn.classList.toggle("bg-surface-panel", view === "thermal");
    }

    sendCommand(view === "camera" ? "view_camera" : "view_thermal");
    updateSensorState();
    log(`Sensor view: ${view.toUpperCase()}`);
  }
=======
>>>>>>> head

  // =====================================================
  // PROCESS ESP32 MESSAGES
  // =====================================================

  function handleMessage(msg) {
    if (!msg || typeof msg !== "object") return;

    if (msg.type === "status") {
      if (typeof msg.battery === "number") {
        const battery = $("batteryValue");
        if (battery) battery.textContent = `${Math.round(msg.battery)}%`;
      }

      if (msg.connected !== undefined) {
        setConnectionState(!!msg.connected);
      }

      if (msg.mic !== undefined) {
        micOn = !!msg.mic;
        const el = $("micStatus");
        if (el) el.textContent = micOn ? "ON" : "OFF";
      }

      if (msg.speaker !== undefined) {
        speakerOn = !!msg.speaker;
        const el = $("speakerStatus");
        if (el) el.textContent = speakerOn ? "ON" : "OFF";
      }

      if (msg.light !== undefined) {
        lightOn = !!msg.light;
        const el = $("lightStatus");
        if (el) el.textContent = lightOn ? "ON" : "OFF";
      }
    } else if (msg.type === "thermal") {
      drawThermal(msg.pixels || msg.data || [], msg.min, msg.max, msg.center);
    } else if (msg.type === "ack") {
      log(`ACK: ${msg.command || "command"}`);
    } else if (msg.type === "log") {
      log(msg.message || "", msg.level || "normal");
    } else if (msg.type === "battery") {
      const value = Number(msg.value);
      if (Number.isFinite(value)) {
        const el = $("batteryValue");
        if (el) el.textContent = `${Math.round(value)}%`;
      }
    }
  }

  // =====================================================
  // THERMAL RENDERING
  // =====================================================

  function thermalColor(t, min, max) {
    let x = (t - min) / Math.max(0.001, max - min);
    x = Math.max(0, Math.min(1, x));

    // Blue -> Cyan -> Green -> Yellow -> Red
    const stops = [
      [0, 20, 70, 180],
      [0.25, 0, 190, 255],
      [0.5, 40, 210, 100],
      [0.75, 255, 220, 0],
      [1, 230, 40, 30],
    ];

    for (let i = 0; i < stops.length - 1; i++) {
      if (x <= stops[i + 1][0]) {
        const a = stops[i];
        const b = stops[i + 1];
        const q = (x - a[0]) / (b[0] - a[0]);

        return `rgb(${Math.round(a[1] + (b[1] - a[1]) * q)}, ${Math.round(a[2] + (b[2] - a[2]) * q)}, ${Math.round(a[3] + (b[3] - a[3]) * q)})`;
      }
    }

    return "rgb(230, 40, 30)";
  }

  function drawThermal(pixels, minHint, maxHint, centerHint) {
    // MLX90640 = 32 x 24 = 768 pixels
    if (!Array.isArray(pixels) || pixels.length < 768) return;

    const canvas = $("thermalCanvas");
    if (!canvas) return;

    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;

    canvas.width = Math.max(1, Math.floor(rect.width * dpr));
    canvas.height = Math.max(1, Math.floor(rect.height * dpr));

    const ctx = canvas.getContext("2d");
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    const w = rect.width;
    const h = rect.height;

    const vals = pixels.slice(0, 768).map(Number).filter(Number.isFinite);
    if (!vals.length) return;

    const min = Number.isFinite(Number(minHint))
      ? Number(minHint)
      : Math.min(...vals);
    const max = Number.isFinite(Number(maxHint))
      ? Number(maxHint)
      : Math.max(...vals);

    const tmp = document.createElement("canvas");
    const tw = 32;
    const th = 24;
    tmp.width = tw;
    tmp.height = th;

    const tc = tmp.getContext("2d");
    const image = tc.createImageData(tw, th);

    for (let y = 0; y < 24; y++) {
      for (let x = 0; x < 32; x++) {
        const color = thermalColor(vals[y * 32 + x], min, max)
          .match(/\d+/g)
          .map(Number);
        const i = (y * 32 + x) * 4;

        image.data[i] = color[0];
        image.data[i + 1] = color[1];
        image.data[i + 2] = color[2];
        image.data[i + 3] = 255;
      }
    }

    tc.putImageData(image, 0, 0);
    ctx.imageSmoothingEnabled = true;
    ctx.drawImage(tmp, 0, 0, w, h);

    const thermalMax = $("thermalMax");
    const thermalMin = $("thermalMin");
    const thermalCenter = $("thermalCenter");

    if (thermalMax) thermalMax.textContent = Number(maxHint ?? max).toFixed(1);
    if (thermalMin) thermalMin.textContent = Number(minHint ?? min).toFixed(1);

    const center = Number.isFinite(Number(centerHint))
      ? Number(centerHint)
      : vals[12 * 32 + 16];
    if (thermalCenter) thermalCenter.textContent = center.toFixed(1);

    const placeholder = $("sensorPlaceholder");
    if (placeholder) placeholder.classList.add("hidden");
  }

  // =====================================================
<<<<<<< HEAD
  // GENERIC COMMAND SENDING
  // =====================================================

  function sendCommand(command, extra = {}) {
    const payload = { type: "command", command, speed, ...extra };

    // Prefer WebSocket.
    if (socket && socket.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify(payload));
      return true;
    }

    // HTTP fallback.
    httpCommand(command, { speed, ...extra });
    return false;
  }

  // =====================================================
=======
>>>>>>> head
  // MOVEMENT CONTROL
  // (single source of truth — used by pointer AND keyboard)
  // =====================================================

<<<<<<< HEAD
  function startDrive(command, triggerBtn = null) {
    if (activeDrive === command) return;

    activeDrive = command;

    document
      .querySelectorAll(".drive-btn")
      .forEach((b) => b.classList.remove("is-held"));

    const btn =
      triggerBtn || document.querySelector(`[data-command="${command}"]`);
    if (btn) btn.classList.add("is-held");

    sendCommand(command);
    log(`DRIVE ${command.toUpperCase()} @ ${speed}%`);
  }

  function releaseDrive(command) {
    // Only stop if this release corresponds to the direction
    // currently driving — an already-overridden key/button
    // releasing later must not cancel the newer command.
    if (activeDrive !== command) return;
    stopDrive();
  }

  function stopDrive() {
    activeDrive = null;
    document
      .querySelectorAll(".drive-btn")
      .forEach((b) => b.classList.remove("is-held"));
    sendCommand("stop");
  }

  function bindHoldButton(id, command) {
    const btn = $(id);
    if (!btn) return;

    // Prevent scrolling/dragging while operating this control.
    btn.style.touchAction = "none";

    let pointerId = null;

    const start = (e) => {
      e.preventDefault();
      if (pointerId !== null) return; // already held by another pointer

      pointerId = e.pointerId;
      try {
        btn.setPointerCapture(pointerId);
      } catch (_) {
        /* ignore */
      }

      startDrive(command, btn);
    };

    const end = (e) => {
      if (e) e.preventDefault();
      if (pointerId === null) return;

      try {
        btn.releasePointerCapture(pointerId);
      } catch (_) {
        /* ignore */
      }
      pointerId = null;

      releaseDrive(command);
    };

    // Pointer Events only. No touchstart/touchend, no click handlers here.
    // pointerleave is intentionally NOT bound: once pointer capture is set
    // on pointerdown, this element keeps receiving pointerup/pointercancel
    // even if the finger/cursor drifts outside its bounds, so relying on
    // "leave" would stop the robot while the user is still holding it.
    btn.addEventListener("pointerdown", start);
    btn.addEventListener("pointerup", end);
    btn.addEventListener("pointercancel", end);
    btn.addEventListener("lostpointercapture", end);
    btn.addEventListener("contextmenu", (e) => e.preventDefault());
  }
=======

>>>>>>> head

  // =====================================================
  // TOGGLE CONTROLS
  // =====================================================

  function bindToggle(id, getState, setState, command, statusId) {
    const btn = $(id);
    if (!btn) return;

    btn.addEventListener("click", () => {
      const next = !getState();
      setState(next);

      const status = $(statusId);
      if (status) {
        status.textContent = next ? "ON" : "OFF";
        status.className = next
          ? "font-data-mono text-[10px] text-primary-container font-bold"
          : "font-data-mono text-[10px] text-text-dim font-bold";
      }

      btn.classList.toggle("active", next);

<<<<<<< HEAD
      sendCommand(command, { on: next });
      log(`${command.toUpperCase()}: ${next ? "ON" : "OFF"}`);
=======
      // TODO: no backend route exists yet for mic/speaker/light.
      log(`${command.toUpperCase()}: ${next ? "ON" : "OFF"} (not sent — no backend route yet)`, "warn");
>>>>>>> head
    });
  }

  // =====================================================
  // KEYBOARD CONTROLS
  // (drives the same startDrive/releaseDrive/stopDrive as
  //  the on-screen buttons — no synthetic PointerEvents)
  // =====================================================

  const keys = new Map([
<<<<<<< HEAD
    ["ArrowUp", "forward"],
    ["w", "forward"],
    ["ArrowDown", "reverse"],
    ["s", "reverse"],
    ["ArrowLeft", "left"],
    ["a", "left"],
    ["ArrowRight", "right"],
    ["d", "right"],
  ]);

  window.addEventListener("keydown", (e) => {
    if (e.repeat) return;
    if (document.activeElement?.tagName === "INPUT") return;

    const cmd = keys.get(e.key);
    if (cmd) {
      e.preventDefault();
      const btn = document.querySelector(`[data-command="${cmd}"]`);
      startDrive(cmd, btn);
      return;
    }

    if (e.key === " ") {
      e.preventDefault();
      stopDrive();
    }
  });

  window.addEventListener("keyup", (e) => {
    const cmd = keys.get(e.key);
    if (cmd) releaseDrive(cmd);
  });
=======
  ["ArrowUp", "forward"],
  ["w", "forward"],
  ["ArrowDown", "reverse"],
  ["s", "reverse"],
  ["ArrowLeft", "left"],
  ["a", "left"],
  ["ArrowRight", "right"],
  ["d", "right"],
]);

function keyToDirection(cmd) {
  if (cmd === "forward") return ["forward", null];
  if (cmd === "reverse") return ["reverse", null];
  if (cmd === "left") return [null, false];
  if (cmd === "right") return [null, true];
  return [null, null];
}

window.addEventListener("keydown", (e) => {
  if (e.repeat) return;
  if (document.activeElement?.tagName === "INPUT") return;

  const cmd = keys.get(e.key);
  if (cmd) {
    e.preventDefault();
    const [dirCMD, dirLR] = keyToDirection(cmd);
    sendDirection(dirCMD, dirLR);
    return;
  }

  if (e.key === " ") {
    e.preventDefault();
    sendDirection("stop", null);
  }
});

window.addEventListener("keyup", (e) => {
  if (keys.get(e.key)) sendDirection("stop", null);
});
>>>>>>> head

  // =====================================================
  // SAFETY CONTROLS
  // =====================================================

  // Stop the robot if the page loses focus or is hidden —
  // a held key/pointer with no page focus can't reliably fire
  // its own release event.
<<<<<<< HEAD
  window.addEventListener("blur", stopDrive);

  document.addEventListener("visibilitychange", () => {
    if (document.hidden) stopDrive();
  });
=======
  window.addEventListener("blur", () => sendDirection("stop", null));

document.addEventListener("visibilitychange", () => {
  if (document.hidden) sendDirection("stop", null);
});
>>>>>>> head

  // =====================================================
  // INITIALIZATION
  // =====================================================

  // -- connection modal --
  const connectBtn = $("connectBtn");
  if (connectBtn) {
    connectBtn.addEventListener("click", () =>
      connected ? disconnect() : connect(),
    );
  }

  const closeConnection = $("closeConnection");
  if (closeConnection) {
    closeConnection.addEventListener("click", () => {
      const modal = $("connectionModal");
      if (modal) {
        modal.classList.add("hidden");
        modal.classList.remove("flex");
      }
    });
  }

  const cancelConnection = $("cancelConnection");
  if (cancelConnection) {
    cancelConnection.addEventListener("click", () => {
      const modal = $("connectionModal");
      if (modal) {
        modal.classList.add("hidden");
        modal.classList.remove("flex");
      }
    });
  }

  const saveConnection = $("saveConnection");
  if (saveConnection) {
    saveConnection.addEventListener("click", doConnect);
  }

  // -- speed --
<<<<<<< HEAD
=======

  function debounce(fn, delay) {
  let timer = null;
  return (...args) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), delay);
  };
}

const sendSpeedDebounced = debounce(sendSpeed, 150);


>>>>>>> head
  const speedSlider = $("speedSlider");
  if (speedSlider) {
    speedSlider.addEventListener("input", (e) => {
      speed = Number(e.target.value);
      const speedValue = $("speedValue");
      if (speedValue) speedValue.textContent = `${speed}%`;
<<<<<<< HEAD
    });
  }

  // -- emergency stop --
  const estopBtn = $("estopBtn");
  if (estopBtn) {
    estopBtn.addEventListener("click", () => {
      stopDrive();
      sendCommand("estop");
      log("EMERGENCY STOP COMMAND SENT.", "error");
    });
  }

  // -- movement buttons (the ONLY movement-control system) --
  // bindHoldButton("forwardBtn", "forward");
  // bindHoldButton("reverseBtn", "reverse");
  // bindHoldButton("leftBtn", "left");
  // bindHoldButton("rightBtn", "right");

  const stopBtn = $("stopBtn");
  if (stopBtn) {
    stopBtn.addEventListener("click", stopDrive);
=======
      sendSpeedDebounced(speed);
    });
  }

    // -- movement buttons (hold-to-drive, auto-stop on release) --
  function bindHold(id, onPress, onRelease) {
    const btn = $(id);
    if (!btn) return;

    btn.addEventListener("pointerdown", (e) => {
      e.preventDefault();
      onPress();
    });

    btn.addEventListener("pointerup", onRelease);
    btn.addEventListener("pointercancel", onRelease);
    btn.addEventListener("lostpointercapture", onRelease);
  }

  bindHold(
    "forwardBtn",
    () => sendDirection("forward", null),
    () => sendDirection("stop", null),
  );

  bindHold(
    "reverseBtn",
    () => sendDirection("reverse", null),
    () => sendDirection("stop", null),
  );

  bindHold(
    "leftBtn",
    () => sendDirection(null, false),
    () => sendDirection("stop", null),
  );

  bindHold(
    "rightBtn",
    () => sendDirection(null, true),
    () => sendDirection("stop", null),
  );

  const stopBtn = $("stopBtn");
  if (stopBtn) {
    stopBtn.addEventListener("click", () => sendDirection("stop", null));
>>>>>>> head
  }

  // -- camera / thermal --
  const cameraBtn = $("cameraBtn");
  if (cameraBtn) cameraBtn.addEventListener("click", () => setView("camera"));

  const thermalBtn = $("thermalBtn");
  if (thermalBtn)
    thermalBtn.addEventListener("click", () => setView("thermal"));

  // -- microphone / speaker / light --
  bindToggle(
    "micBtn",
    () => micOn,
    (v) => (micOn = v),
    "microphone",
    "micStatus",
  );
  bindToggle(
    "speakerBtn",
    () => speakerOn,
    (v) => (speakerOn = v),
    "speaker",
    "speakerStatus",
  );
  bindToggle(
    "lightBtn",
    () => lightOn,
    (v) => (lightOn = v),
    "light",
    "lightStatus",
  );

  // -- bluetooth (informational only — link is ESP32-S3 -> HM-10 -> STM32) --
  const bluetoothBtn = $("bluetoothBtn");
  if (bluetoothBtn) {
    bluetoothBtn.addEventListener("click", () => {
      log("Bluetooth link is handled by ESP32-S3 -> HM-10 -> STM32.", "warn");
    });
  }

  // -- clear console --
  const clearConsole = $("clearConsole");
  if (clearConsole) {
    clearConsole.addEventListener("click", () => {
      if (consoleEl) {
        consoleEl.innerHTML =
          '<div class="mt-2 text-primary-container animate-pulse">_</div>';
      }
    });
  }

  // -- initial state --
  setView("camera");

  if (speedSlider) speedSlider.value = speed;

  const speedValue = $("speedValue");
  if (speedValue) speedValue.textContent = `${speed}%`;

  updateSensorState();

  if (baseUrl) {
    log(`Saved ESP32-S3 URL: ${baseUrl}`);
  }
})();
