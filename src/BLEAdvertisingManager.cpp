#include "BLEAdvertisingManager.h"
#include "AppPreferences.h"
#include "BLE.h"
#include <string>

BLEAdvertising* BLEAdvertisingManager::pAdvertising = BLEDevice::getAdvertising();
uint8_t BLEAdvertisingManager::advertisingMode = 0xFF;

void BLEAdvertisingManager::setup() {
    log_i(">> BLEAdvertisingManager::setup");
    pAdvertising = BLEDevice::getAdvertising();

    pAdvertising->addServiceUUID(SNEAK32_SERVICE_UUID);
    pAdvertising->setAppearance(ESP_BLE_APPEARANCE_GENERIC_WATCH);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);

    if (strlen(appPrefs.authorized_address) == 17) {
        BLEAddress authorizedAddress(appPrefs.authorized_address);
        // Clear the whitelist, only one device is allowed
        esp_ble_gap_clear_whitelist();
        BLEDevice::whiteListAdd(authorizedAddress);
        log_i("Added authorized address to whitelist: %s", appPrefs.authorized_address);
    }

    if (appPrefs.stealth_mode && strlen(appPrefs.authorized_address) == 17) {
        configureStealthMode();
    } else {
        configureNormalMode();
    }
}

void BLEAdvertisingManager::start() {
    log_i(">> BLEAdvertisingManager::start");
    pAdvertising->start();
}

void BLEAdvertisingManager::stop() {
    log_i(">> BLEAdvertisingManager::stop");
    pAdvertising->stop();
}

void BLEAdvertisingManager::updateAdvertisingData() {
    log_i(">> BLEAdvertisingManager::updateAdvertisingData");
    pAdvertising->stop();
    setup();
    pAdvertising->start();
}

void BLEAdvertisingManager::configureStealthMode() {
    if (advertisingMode != ADV_TYPE_DIRECT_IND_LOW) {
        log_i(">> BLEAdvertisingManager::configureStealthMode");
    }
    advertisingMode = ADV_TYPE_DIRECT_IND_LOW;

    pAdvertising->setAdvertisementType(static_cast<esp_ble_adv_type_t>(advertisingMode));
    pAdvertising->setScanFilter(true, true);

    BLEAdvertisementData data = getAdvertisementData();
    data.setFlags(ESP_BLE_ADV_FLAG_LIMIT_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
    pAdvertising->setAdvertisementData(data);
}

void BLEAdvertisingManager::configureNormalMode() {
    if (advertisingMode != ADV_TYPE_IND) {
        log_i(">> BLEAdvertisingManager::configureNormalMode");
    }
    advertisingMode = ADV_TYPE_IND;

    pAdvertising->setAdvertisementType(static_cast<esp_ble_adv_type_t>(advertisingMode));
    pAdvertising->setScanFilter(false, false);

    BLEAdvertisementData data = getAdvertisementData();
    data.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
    pAdvertising->setAdvertisementData(data);
}

BLEAdvertisementData BLEAdvertisingManager::getAdvertisementData() {
    BLEAdvertisementData data;
    
    /*
    uint8_t xiaomiCode[] = {0x8F, 0x03, 'X', 'i', 'a', 'o', 'm', 'i'};
    data.setManufacturerData(std::string((char*)xiaomiCode, 8));    
    data.setAppearance(ESP_BLE_APPEARANCE_SPORTS_WATCH);
    */

    data.setCompleteServices(BLEUUID(SNEAK32_SERVICE_UUID));
    data.setName(appPrefs.device_name);

    return data;
}
