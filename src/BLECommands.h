#ifndef BLE_COMMANDS_H
#define BLE_COMMANDS_H

#include <BLECharacteristic.h>
#include <SimpleCLI.h>

class BLECommands  : public BLECharacteristicCallbacks {
private:
    static BLECharacteristic* pCharacteristic;
    
public:
    static SimpleCLI* pCli;

    void onWrite(BLECharacteristic* characteristic);
    static void respond(String text);
    static void respond(uint8_t *value, size_t length);
    static String getFormattedHelp();
};

#endif // BLE_COMMANDS_H

