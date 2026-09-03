
#ifndef WIFI_HDL_H
#define WIFI_HDL_H

#include "fLed.h"

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

#include <inttypes.h>

#define IPADDR "192.168.4.1"

extern WebServer server;

void logPrintln(const String &msg);

void setupWIFI(const char *ssid, const char *pass);

void setupServerRoutes(void);

#endif //WIFI_HDL_H