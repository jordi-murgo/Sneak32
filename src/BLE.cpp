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
#include "FirmwareInfo.h"

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

// Variables globales para Preferences
#define PREF_NAMESPACE "ble"
#define PREF_AUTH_KEY "auth_addr"

// Variable global para la dirección autorizada
BLEAddress *authorizedClientAddress = nullptr;

// Callback implementations
class MySecurity : public BLESecurityCallbacks
{
public:
    MySecurity()
    {
        Preferences preferences;
        if (preferences.begin(PREF_NAMESPACE, false))
        { // Modo lectura y escritura
            String savedAddress = preferences.getString(PREF_AUTH_KEY, "");
            if (!savedAddress.isEmpty())
            {
                authorizedClientAddress = new BLEAddress(savedAddress.c_str());
                Serial.printf("Loaded authorized address in MySecurity: %s\n", savedAddress.c_str());
            }
            else
            {
                Serial.println("No authorized address found in preferences");
            }
            preferences.end();
        }
        else
        {
            Serial.println("Failed to open preferences namespace in MySecurity");
        }
    }

    uint32_t onPassKeyRequest() override
    {
        Serial.println("Passkey Request");
        // Generar una passkey aleatoria de 6 dígitos
        uint32_t passKey = random(100000, 999999);
        Serial.print("Generated Passkey: ");
        Serial.println(passKey);
        return passKey;
    }

    void onPassKeyNotify(uint32_t pass_key) override
    {
        Serial.print("Passkey to be entered: ");
        Serial.println(pass_key);
    }

    bool onConfirmPIN(uint32_t pass_key) override
    {
        Serial.print("Confirm Passkey: ");
        Serial.println(pass_key);
        // Considera implementar una forma más robusta de confirmación
        return true;
    }

    bool onSecurityRequest() override
    {
        Serial.println("Security Request Received");
        return true;
    }

    void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) override
    {
        if (auth_cmpl.success)
        {
            Serial.println("Pairing Successful");
            BLEAddress clientAddress = BLEAddress(auth_cmpl.bd_addr);
            Serial.printf("Paired Client MAC: %s\n", clientAddress.toString().c_str());

            // Guardar la dirección en las preferencias
            Preferences preferences;
            if (preferences.begin(PREF_NAMESPACE, false))
            { // Modo lectura y escritura
                if (preferences.putString(PREF_AUTH_KEY, clientAddress.toString().c_str()))
                {
                    Serial.println("Authorized address saved successfully");
                }
                else
                {
                    Serial.println("Failed to save authorized address");
                }
                preferences.end();
            }
            else
            {
                Serial.println("Failed to open preferences namespace in onAuthenticationComplete");
            }

            // Actualizar la dirección autorizada en memoria
            if (authorizedClientAddress != nullptr)
            {
                delete authorizedClientAddress;
            }
            authorizedClientAddress = new BLEAddress(clientAddress);

            BLEAdvertisingManager::setup();
        }
        else
        {
            Serial.println("Pairing Failed");
            Serial.printf("Failure Reason: %d\n", auth_cmpl.fail_reason);
        }
    }
};

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer) override
    {
        deviceConnected = true;
        Serial.println("Device connected");

        // Asegurarse de que authorizedClientAddress está cargado
        if (authorizedClientAddress == nullptr)
        {
            Preferences preferences;
            if (preferences.begin(PREF_NAMESPACE, false))
            { // Modo lectura y escritura
                String savedAddress = preferences.getString(PREF_AUTH_KEY, "");
                if (!savedAddress.isEmpty())
                {
                    authorizedClientAddress = new BLEAddress(savedAddress.c_str());
                    Serial.printf("Loaded authorized address: %s\n", savedAddress.c_str());
                }
                else
                {
                    Serial.println("No authorized address found in preferences");
                }
                preferences.end();
            }
            else
            {
                Serial.println("Failed to open preferences namespace in onConnect");
            }
        }

        try
        {
            auto peerDevices = pServer->getPeerDevices(true);
            Serial.printf("Number of peer devices: %d\n", peerDevices.size());

            for (auto &peerDevice : peerDevices)
            {
                uint16_t id = peerDevice.first;
                conn_status_t status = peerDevice.second;
                Serial.printf("Peer device ID: %d, Connected: %d\n", id, status.connected);

                if (status.peer_device != nullptr)
                {
                    BLEClient *client = (BLEClient *)status.peer_device;
                    BLEAddress clientAddress = client->getPeerAddress();
                    Serial.printf("Connected client address: %s\n", clientAddress.toString().c_str());

                    // Verificar si el dispositivo está autorizado
                    if (authorizedClientAddress != nullptr && *authorizedClientAddress == clientAddress)
                    {
                        Serial.println("Authorized client reconnected - skipping security");
                        // No iniciar emparejamiento
                        return;
                    }

                    Serial.println("New client connected. Pairing will be handled by the BLE stack.");
                    // La pila BLE manejará el emparejamiento automáticamente
                }
                else
                {
                    Serial.println("Failed to get valid client address");
                }
            }
        }

        catch (std::exception &e)
        {
            Serial.printf("Exception in onConnect: %s\n", e.what());
        }
        catch (...)
        {
            Serial.println("Unknown exception in onConnect");
        }
    }

    void onDisconnect(BLEServer *pServer) override
    {
        deviceConnected = false;
        Serial.println("Device disconnected");

        // Reiniciar el Advertising
        BLEAdvertisingManager::start();
    }
};

class ListSizesCallbacks : public BLECharacteristicCallbacks
{
    void onRead(BLECharacteristic *pCharacteristic) override
    {
        BLEStatusUpdater.update();
    }
};

// Implementación de funciones
void setupBLE()
{
    Serial.println("Initializing BLE");
    BLEDevice::init(appPrefs.device_name);

    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    BLEDevice::setSecurityCallbacks(new MySecurity());

    // Configurar parámetros de seguridad
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_BOND; // Conexión segura con bonding
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;              // Sin capacidades de entrada/salida
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    // Establecer la potencia de transmisión BLE a -12 dBm
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N12);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setKeySize(16);
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    Serial.println("Creating Scanner BLE service and characteristics");
    BLEService *pScannerService = pServer->createService(SCANNER_SERVICE_UUID);

    pStatusCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)STATUS_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pStatusCharacteristic->setCallbacks(new ListSizesCallbacks());
    pStatusCharacteristic->addDescriptor(new BLE2902()); // Añadir CCCD
    BLEStatusUpdater.update();

    pTxCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)SCANNER_DATATRANSFER_UUID),
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE);
    pTxCharacteristic->setCallbacks(new SendDataOverBLECallbacks());
    pTxCharacteristic->addDescriptor(new BLE2902()); // Añadir CCCD

    pSettingsCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)SETTINGS_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pSettingsCharacteristic->setCallbacks(new SettingsCallbacks());
    pSettingsCharacteristic->addDescriptor(new BLE2902()); // Añadir CCCD
    pSettingsCharacteristic->setValue((uint8_t *)&appPrefs, sizeof(AppPreferencesData));

    BLECharacteristic *pCommandsCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)COMMANDS_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pCommandsCharacteristic->setCallbacks(new CommandsCallbacks());
    pCommandsCharacteristic->addDescriptor(new BLE2902()); // Añadir CCCD

    BLECharacteristic *pFirmwareInfoCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)FIRMWARE_INFO_UUID),
        BLECharacteristic::PROPERTY_READ);
    pFirmwareInfoCharacteristic->setValue(getFirmwareInfoString().c_str());

    Serial.println("Starting BLE service");
    pScannerService->start();

    Serial.println("Configuring BLE advertising");
    BLEAdvertisingManager::setup();

    Serial.println("Starting BLE advertising");
    BLEAdvertisingManager::start();

    Serial.println("BLE Initialized");
}
