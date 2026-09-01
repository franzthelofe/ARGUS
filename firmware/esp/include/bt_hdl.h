
#include "wifi_hdl.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>

#ifndef BT_HDL_H
#define BT_HDL_H

#define BT_NAME "HMSoft"

#define SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define CHAR_UUID    "0000ffe1-0000-1000-8000-00805f9b34fb"

static BLEAddress* btAddress = nullptr;
static bool doConnect = false;
static bool connected = false;

static BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
static BLEClient* pClient = nullptr;

class btClientCallbacks : public BLEClientCallbacks
{
    void onConnect(BLEClient* pClient) override
    {
        logPrintln("Connected to HM-10");
    }

    void onDisconnect(BLEClient* pClient) override
    {
        connected = false;

        logPrintln("Disonnected from HM-10");

        BLEDevice::getScan() -> start(5, false);
    }
};

class btAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice AdvertisedDevice) override
    {
        String deviceName = AdvertisedDevice.getName().c_str();

        logPrintln("Found Device: " + String(AdvertisedDevice.toString().c_str()));

        if (strcmp(deviceName.c_str(), BT_NAME))
        {
            logPrintln("HM-10 Found");

            BLEDevice::getScan() -> stop();

            btAddress = new BLEAddress(AdvertisedDevice.getAddress());

            doConnect = true;
        }
    }
};


bool connectBT() {

    logPrintln("Connecting...");

    pClient = BLEDevice::createClient();

    pClient->setClientCallbacks(new btClientCallbacks());

    if (!pClient->connect(*btAddress)) {

        logPrintln("Connection failed!");

        return false;
    }

    logPrintln("Connected!");

    BLERemoteService *pRemoteService = pClient->getService(SERVICE_UUID);

    if (pRemoteService == nullptr)
    {

        logPrintln("HM-10 service FFE0 not found!");

        pClient->disconnect();

        return false;
    }

    logPrintln("Service found!");

    // Get HM-10 characteristic
    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHAR_UUID);

    if (pRemoteCharacteristic == nullptr)
    {

        logPrintln("HM-10 characteristic FFE1 not found!");

        pClient->disconnect();

        return false;
    }

    logPrintln("Characteristic found!");

    connected = true;

    return true;
}

void     sendData(const char *data)
{

    if (!connected || pRemoteCharacteristic == nullptr)
    {

        logPrintln("Not connected!");

        return;
    }

    logPrintln("Sending: " + String(data));

    pRemoteCharacteristic->writeValue((uint8_t *)data, strlen(data),false);
}

#endif // BT_HDL_H  