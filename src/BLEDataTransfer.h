#ifndef BLE_DATA_TRANSFER_H
#define BLE_DATA_TRANSFER_H

#include <BLECharacteristic.h>
#include <Arduino.h>

class SendDataOverBLECallbacks : public BLECharacteristicCallbacks
{
public:
    void onWrite(BLECharacteristic *pCharacteristic) override;
};

void prepareJsonData(boolean only_relevant_stations = false);
void sendPacket(uint16_t packetNumber, boolean only_relevant_stations = false);
void checkTransmissionTimeout();

#endif // BLE_DATA_TRANSFER_H
