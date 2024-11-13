#ifndef BLE_COMMANDS_H
#define BLE_COMMANDS_H

#include <BLECharacteristic.h>
#include <SimpleCLI.h>

class BLECommands  : public BLECharacteristicCallbacks {
public:
    static BLECharacteristic* pCharacteristic;
    static SimpleCLI* pCli;

    void onWrite(BLECharacteristic* characteristic);
};

#endif // BLE_COMMANDS_H

