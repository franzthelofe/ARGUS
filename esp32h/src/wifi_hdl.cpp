// wifi_hdl.cpp

#include "wifi_hdl.h"
#include <LittleFS.h>

WebServer server(80);

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

    server.serveStatic("/", LittleFS, "/index.html");
    server.serveStatic("/style.css", LittleFS, "/style.css");
    server.serveStatic("/script.js", LittleFS, "/script.js");

    server.on("/color", handleColor);
    server.on("/rgb", handleRGB);

    server.begin();
}

void setupWIFI(const char *ssid, const char *pass)
{
    WiFi.begin(ssid, pass);

    int i = 0;

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);

        switch (i)
        {
            case 0: LEDColor(B4); LEDShow(); i++; break;
            case 1: LEDColor(B3); LEDShow(); i++; break;
            case 2: LEDColor(B2); LEDShow(); i++; break;
            case 3: LEDColor(B1_); LEDShow(); i -= 3; break;
        }
    }

    Serial.println(WiFi.localIP());
    LEDColor(G1);
    LEDShow();
    delay(500);

    setupServerRoutes();
}