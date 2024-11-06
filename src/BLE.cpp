#include <ArduinoJson.h>
#include <Preferences.h>

#include <sstream>
#include <esp_gap_ble_api.h>

#include "BLE.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "AppPreferences.h"
#include "MACAddress.h"
#include "WifiScan.h"
#include "WifiDetect.h"
#include "BLEScan.h"
#include "BLEDetect.h"
#include "sdkconfig.h" // For CONFIG_SPIRAM
#include "FlashStorage.h"

#include "BLECommands.h"
#include "BLESettings.h"
#include "BLEDataTransfer.h"

#include "BLEAdvertisingManager.h"

// Add this include
#include "BLEStatusUpdater.h"


// External variables
extern BLEDeviceList bleDeviceList;
extern WifiDeviceList stationsList;
extern WifiNetworkList ssidList;

extern String getChipInfo();
extern time_t base_time;

// Global variables
BLEServer *pServer = nullptr;
BLECharacteristic *pTxCharacteristic = nullptr;
BLECharacteristic *pSettingsCharacteristic = nullptr;
BLECharacteristic *pStatusCharacteristic = nullptr;


BLEScan *pBLEScan = nullptr;
bool deviceConnected = false;

// Add these global variables
Preferences securityPreferences;
BLEAddress *authorizedClientAddress = nullptr;

// Callback implementations
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer) override
    {
        deviceConnected = true;
        Serial.println("Device connected");
        
        try {
            auto peerDevices = pServer->getPeerDevices(true);
            Serial.printf("Number of peer devices: %d\n", peerDevices.size());
            
            for (auto &peerDevice : peerDevices)
            {
                uint16_t id = peerDevice.first;
                conn_status_t status = peerDevice.second;
                Serial.printf("Peer device ID: %d, Connected: %d\n", id, status.connected);
                
                if (status.peer_device != nullptr) {
                    BLEClient *client = (BLEClient *)status.peer_device;
                    BLEAddress clientAddress = client->getPeerAddress();
                    Serial.printf(">>> Device connected: %s\n", clientAddress.toString().c_str());

                    // Check if the connected device is the authorized client
                    if (authorizedClientAddress != nullptr && clientAddress.equals(*authorizedClientAddress)) {
                        Serial.println("Authorized client reconnected. Skipping pairing.");
                        // You might want to set a flag or perform any specific actions for an authorized client
                    } else {
                        Serial.println("New client connected. Initiating pairing.");
                        // Initiate pairing process
                        uint8_t address[6];
                        memcpy(address, clientAddress.getNative(), 6);
                        esp_ble_set_encryption(address, ESP_BLE_SEC_ENCRYPT_NO_MITM);
                    }
                } else {
                    Serial.println(">>> Device connected but peer_device is null");
                }
            }
        } catch (std::exception &e) {
            Serial.printf("Exception in onConnect: %s\n", e.what());
        } catch (...) {
            Serial.println("Unknown exception in onConnect");
        }
    }

    void onDisconnect(BLEServer *pServer) override
    {
        deviceConnected = false;
        pServer->getAdvertising()->start();
    }
};

class ListSizesCallbacks : public BLECharacteristicCallbacks
{
    void onRead(BLECharacteristic *pCharacteristic) override
    {
        BLEStatusUpdater.update();
    }
};

// Add this near the top of the file, after other includes
#include "FirmwareInfo.h"

class MySecurity : public BLESecurityCallbacks {
public:
    MySecurity() {
        securityPreferences.begin("ble_auth", false);
        String savedAddress = securityPreferences.getString("client_addr", "");
        if (!savedAddress.isEmpty()) {
            authorizedClientAddress = new BLEAddress(savedAddress.c_str());
        }
        securityPreferences.end();
    }

    uint32_t onPassKeyRequest(){
        Serial.println("Solicitud de código de acceso");
        return 123456; // Si no usas códigos de acceso, retorna 0
    }

    void onPassKeyNotify(uint32_t pass_key){
        Serial.print("El código de acceso es: ");
        Serial.println(pass_key);
    }

    bool onConfirmPIN(uint32_t pass_key){
        Serial.print("Confirmar código de acceso: ");
        Serial.println(pass_key);
        return true; // Retorna true para confirmar
    }

    bool onSecurityRequest(){
        Serial.println("Solicitud de seguridad recibida");
        return true; // Acepta la solicitud de seguridad
    }

    void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl){
        if(auth_cmpl.success){
            Serial.println("Pairing successful");
            BLEAddress clientAddress = BLEAddress(auth_cmpl.bd_addr);
            Serial.printf("Client MAC address: %s\n", clientAddress.toString().c_str());

            // Save the authorized client address
            securityPreferences.begin("ble_auth", false);
            securityPreferences.putString("client_addr", clientAddress.toString().c_str());
            securityPreferences.end();

            if (authorizedClientAddress != nullptr) {
                delete authorizedClientAddress;
            }
            authorizedClientAddress = new BLEAddress(clientAddress);

            BLEAdvertisingManager::setup();
        } else {
            Serial.println("Pairing failed");
        }
    }
};



// Function implementations
void setupBLE()
{
    Serial.println("Initializing BLE");
    BLEDevice::init(appPrefs.device_name);

    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    BLEDevice::setSecurityCallbacks(new MySecurity());

    // Configure security parameters
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    // Set the BLE TX power to -12 dBm
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N12);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLESecurity *pSecurity = new BLESecurity();
	pSecurity->setKeySize(16);
	pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_ONLY);
	pSecurity->setCapability(ESP_IO_CAP_IO);
	pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    Serial.println("Creating Scanner BLE service and characteristics");
    BLEService *pScannerService = pServer->createService(SCANNER_SERVICE_UUID);

    pStatusCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)STATUS_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pStatusCharacteristic->setCallbacks(new ListSizesCallbacks());
    pStatusCharacteristic->addDescriptor(new BLE2902());
    BLEStatusUpdater.update();

    pTxCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)SCANNER_DATATRANSFER_UUID),
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE);
    pTxCharacteristic->setCallbacks(new SendDataOverBLECallbacks());
    pTxCharacteristic->addDescriptor(new BLE2902());

    pSettingsCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)SETTINGS_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pSettingsCharacteristic->setCallbacks(new SettingsCallbacks());
    pSettingsCharacteristic->addDescriptor(new BLE2902());
    pSettingsCharacteristic->setValue((uint8_t *)&appPrefs, sizeof(AppPreferencesData));

    BLECharacteristic *pCommandsCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)COMMANDS_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pCommandsCharacteristic->addDescriptor(new BLE2902());
    pCommandsCharacteristic->setCallbacks(new CommandsCallbacks());

    BLECharacteristic *pFirmwareInfoCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)FIRMWARE_INFO_UUID),
        BLECharacteristic::PROPERTY_READ);
    pFirmwareInfoCharacteristic->setValue(getFirmwareInfoString().c_str());

    Serial.println("Starting BLE service");
    pScannerService->start();

    Serial.println("Configuring BLE advertising");
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisingManager::setup();

    Serial.println("Starting BLE advertising");
    BLEAdvertisingManager::start();

    Serial.println("BLE Initialized");
}





