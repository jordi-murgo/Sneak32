#ifndef BLE_SETTINGS_H
#define BLE_SETTINGS_H

#include <BLECharacteristic.h>
#include "AppPreferences.h"

class SettingsCallbacks : public BLECharacteristicCallbacks
{
public:
    void onWrite(BLECharacteristic *pCharacteristic) override;
    void onRead(BLECharacteristic *pCharacteristic) override;

private:
    void updateBLEDeviceName(const char *newName);
};

#endif // BLE_SETTINGS_H
