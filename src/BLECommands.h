#ifndef BLE_COMMANDS_H
#define BLE_COMMANDS_H

#include <BLECharacteristic.h>

class CommandsCallbacks : public BLECharacteristicCallbacks
{
public:
    void onWrite(BLECharacteristic *pCharacteristic) override;
};

#endif // BLE_COMMANDS_H

