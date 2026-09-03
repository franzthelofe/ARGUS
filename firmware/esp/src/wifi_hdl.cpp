// wifi_hdl.cpp

#include "wifi_hdl.h"

#define LOG_BUF_LINES 40

#include <LittleFS.h>

static String logBuf[LOG_BUF_LINES];
static int logIndex = 0;
static String payload;

WebServer server(80);

void logPrintln(const String &msg)
{
    Serial.println(msg);   // still prints over USB when it's plugged in
    logBuf[logIndex] = msg;
    logIndex = (logIndex + 1) % LOG_BUF_LINES;
}

static String buildLogString()
{
    String out;
    for (int i = 0; i < LOG_BUF_LINES; i++)
    {
        int idx = (logIndex + i) % LOG_BUF_LINES;
        if (logBuf[idx].length())
            out += logBuf[idx] + "\n";
    }
    return out;
}

static void handleLog()
{
    server.send(200, "text/plain", buildLogString());
}

/*_____________________________________________________________________________________*/


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

/*_____________________________________________________________________________________*/

void handleDirection(void)
{
    bool handledSomething = false;

    if (server.hasArg("dirCMD"))
    {
        String dir = server.arg("dirCMD");
        dir.trim();

        if (dir.length())
        {
            switch (dir[0])
            {
            case 'F':   // forward
                LEDColor(B4);
                LEDShow();
                handledSomething = true;
                break;
            case 'R':   // reverse
                LEDColor(B4);
                LEDShow();
                handledSomething = true;
                break;
            case 'X':   // stop
                led[0] = CRGB(0, 0, 0);
                LEDShow();
                handledSomething = true;
                break;
            default:
                server.send(400, "text/plain", "invalid dirCMD");
                return;
            }
        }
    }

    if (server.hasArg("dirLR"))
    {
        String dirLR = server.arg("dirLR");

        if (dirLR == "true")        // right
        {
            LEDColor(G4);
            LEDShow();
            handledSomething = true;
        }
        else if (dirLR == "false")  // left
        {
            LEDColor(R4);
            LEDShow();
            handledSomething = true;
        }
    }

    if (!handledSomething)
    {
        server.send(400, "text/plain", "missing args");
        return;
    }

    server.send(200, "text/plain", "ok");
}

void handleSpeed(void)
{
    if (server.hasArg("speed"))
    {
        String speedStr = server.arg("speed");
        speedStr.trim();
        int speed = speedStr.toInt();

        if (speed < 0 || speed > 255)
        {
            server.send(400, "text/plain", "invalid args");
            return;
        }

        logPrintln("Speed set to: " + String(speed) + "%");
        led[0] = CHSV(0, 255, speed);
        server.send(200, "text/plain", "ok");
    }
    else
    {
        server.send(400, "text/plain", "missing args");
    }
}

/*_____________________________________________________________________________________*/


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

    server.on("/log", handleLog);

    server.on("/dir", handleDirection);
    server.on("/speed", handleSpeed);

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
    
    LEDColor(G4);
    LEDShow();
    delay(500);

    setupServerRoutes();

    //  LittleFS.open("/");
}