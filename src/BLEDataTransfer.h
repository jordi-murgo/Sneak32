#ifndef BLE_DATA_TRANSFER_H
#define BLE_DATA_TRANSFER_H

#include <BLECharacteristic.h>
#include <Arduino.h>

// The MTU size determines the maximum packet size for BLE data transfer
// Supported values: 128, 192, 256, 512 bytes

class SendDataOverBLECallbacks : public BLECharacteristicCallbacks
{
public:
    void onWrite(BLECharacteristic *pCharacteristic) override;
};

void prepareJsonData(boolean only_relevant_stations = false);
void sendPacket(uint16_t packetNumber, const String& requestTyp);
void checkTransmissionTimeout();

#endif // BLE_DATA_TRANSFER_H
