// wifi_hdl.cpp

#include "wifi_hdl.h"

#define LOG_BUF_LINES 40

#include <LittleFS.h>

static String logBuf[LOG_BUF_LINES];
static int logIndex = 0;

WebServer server(80);

void logPrintln(const String &msg)
{
    Serial.println(msg);   // still prints over USB when it's plugged in
    logBuf[logIndex] = msg;
    logIndex = (logIndex + 1) % LOG_BUF_LINES;
}

static void handleLog()
{
    String out;
    for (int i = 0; i < LOG_BUF_LINES; i++)
    {
        int idx = (logIndex + i) % LOG_BUF_LINES;
        if (logBuf[idx].length())
            out += logBuf[idx] + "\n";
    }
    server.send(200, "text/plain", out);
}

static void handleCSS()
{
    File f = LittleFS.open("/style.css", "r");
    if (!f)
    {
        server.send(404, "text/plain", "not found");
        return;
    }
    server.streamFile(f, "text/css");   // force the correct MIME type
    f.close();
}

static void handleColor()
{
    if (!server.hasArg("c"))
    {
        server.send(400, "text/plain", "missing c");

        return;
    }

    String c = server.arg("c");

    static const struct { const char *name; Color value; } table[] =
    {
        {"R1", R1}, {"R2", R2}, {"R3", R3}, {"R4", R4},
        {"G1", G1}, {"G2", G2}, {"G3", G3}, {"G4", G4},
        {"B1", B1_}, {"B2", B2}, {"B3", B3}, {"B4", B4},
    };

    for (auto &entry : table)
    {
        if (c == entry.name)
        {
            LEDColor(entry.value);
            LEDShow();

            server.send(200, "text/plain", "ok");

            return;
        }
    }

    server.send(400, "text/plain", "unknown color");
}

static void handleRGB()
{
    if (!server.hasArg("r") || !server.hasArg("g") || !server.hasArg("b"))
    {
        server.send(400, "text/plain", "missing r/g/b");

        return;
    }

    uint8_t r = server.arg("r").toInt();
    uint8_t g = server.arg("g").toInt();
    uint8_t b = server.arg("b").toInt();

    led[0] = CRGB(r, g, b);
    LEDShow();

    server.send(200, "text/plain", "ok");
}

void setupServerRoutes(void)
{
    if (!LittleFS.begin(true))   // true = format if mount fails
    {
        Serial.println("LittleFS mount failed!");
        return;
    }

    File root = LittleFS.open("/");
    File f = root.openNextFile();
    while (f)
    {
        Serial.printf("FOUND: %s (%d bytes)\n", f.name(), f.size());
        f = root.openNextFile();
    }

    server.on("/style.css", handleCSS);

    server.serveStatic("/", LittleFS, "/index.html");
    server.serveStatic("/script.js", LittleFS, "/script.js");

    server.on("/color", handleColor);
    server.on("/rgb", handleRGB);

    server.on("/log", handleLog);

    server.begin();
}

void setupWIFI(const char *ssid, const char *pass)
{
    LEDColor(B4);
    LEDShow();

    bool started = WiFi.softAP(ssid, pass);

    if (!started)
    {
        Serial.println("Failed to start AP!");
        LEDColor(R1);
        LEDShow();
        return;
    }

    IPAddress ip = WiFi.softAPIP();

    logPrintln("AP started: " + String(ssid));
    logPrintln("Browse to: http://" + ip.toString());
    
    LEDColor(G1);
    LEDShow();
    delay(500);

    setupServerRoutes();

    //  LittleFS.open("/");
}