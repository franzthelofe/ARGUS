
#ifndef WIFI_HDL_H
#define WIFI_HDL_H

#include "fLed.h"

#include <WiFi.h>
#include <WebServer.h>

#include <inttypes.h>

extern WebServer server;

void setupWIFI(const char *ssid, const char *pass);

void setupServerRoutes(void);

#endif //WIFI_HDL_H