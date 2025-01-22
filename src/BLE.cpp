#include <ArduinoJson.h>
#include <Preferences.h>

#include <sstream>
#include <esp_gap_ble_api.h>
#include <BLEUtils.h>

#include "BLE.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "AppPreferences.h"
#include "MacAddress.h"
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
                log_i("Loaded authorized address in MySecurity: %s", savedAddress.c_str());
            }
            else
            {
                log_i("No authorized address found in preferences");
            }
            preferences.end();
        }
        else
        {
            log_e("Failed to open preferences namespace in MySecurity");
        }
    }

    uint32_t onPassKeyRequest() override
    {
        log_i("Passkey Request");
        // Generar una passkey aleatoria de 6 dígitos
        uint32_t passKey = random(100000, 999999);
        log_i("Generated Passkey: %u", passKey);
        return passKey;
    }

    void onPassKeyNotify(uint32_t pass_key) override
    {
        log_i("Passkey to be entered: %u", pass_key);
    }

    bool onConfirmPIN(uint32_t pass_key) override
    {
        log_i("Confirm Passkey: %u", pass_key);
        // Considera implementar una forma más robusta de confirmación
        return true;
    }

    bool onSecurityRequest() override
    {
        log_i("Security Request Received");
        return true;
    }

    void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) override
    {
        if (auth_cmpl.success)
        {
            log_i("Pairing Successful");
            BLEAddress clientAddress = BLEAddress(auth_cmpl.bd_addr);
            log_i("Paired Client MAC: %s", clientAddress.toString().c_str());

            strncpy(appPrefs.authorized_address, clientAddress.toString().c_str(), sizeof(appPrefs.authorized_address));

            // Guardar la dirección en las preferencias
            Preferences preferences;
            if (preferences.begin(PREF_NAMESPACE, false))
            { // Modo lectura y escritura
                if (preferences.putString(PREF_AUTH_KEY, clientAddress.toString().c_str()))
                {
                    log_i("Authorized address saved successfully");
                }
                else
                {
                    log_e("Failed to save authorized address");
                }
                preferences.end();
            }
            else
            {
                log_e("Failed to open preferences namespace in onAuthenticationComplete");
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
            log_e("Pairing Failed. Reason: %d", auth_cmpl.fail_reason);
        }
    }
};

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer) override
    {
        deviceConnected = true;
        log_i("Device connected");

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
                    log_i("Loaded authorized address: %s", savedAddress.c_str());
                }
                else
                {
                    log_w("No authorized address found in preferences");
                }
                preferences.end();
            }
            else
            {
                log_e("Failed to open preferences namespace in onConnect");
            }
        }

        try
        {
            auto peerDevices = pServer->getPeerDevices(true);
            log_d("Number of peer devices: %d", peerDevices.size());

            for (auto &peerDevice : peerDevices)
            {
                uint16_t id = peerDevice.first;
                conn_status_t status = peerDevice.second;
                log_d("Peer device ID: %d, Connected: %d", id, status.connected);

                if (status.peer_device != nullptr)
                {
                    BLEClient *client = (BLEClient *)status.peer_device;
                    BLEAddress clientAddress = client->getPeerAddress();
                    log_d("Connected client address: %s", clientAddress.toString().c_str());

                    // Verificar si el dispositivo está autorizado
                    if (authorizedClientAddress != nullptr && *authorizedClientAddress == clientAddress)
                    {
                        log_i("Authorized client reconnected - skipping security");
                        return;
                    }

                    log_i("New client connected. Pairing will be handled by the BLE stack.");
                    // La pila BLE manejará el emparejamiento automáticamente
                }
                else
                {
                    log_e("Failed to get valid client address");
                }
            }
        }
        catch (std::exception &e)
        {
            log_e("Exception in onConnect: %s", e.what());
        }
        catch (...)
        {
            log_e("Unknown exception in onConnect");
        }
    }

    void onDisconnect(BLEServer *pServer) override
    {
        deviceConnected = false;
        log_i("Device disconnected");

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
    log_i("Setting up BLE...");

    // Create the BLE Device
    BLEDevice::init(appPrefs.device_name);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, (esp_power_level_t)appPrefs.bleTxPower);

    log_i("Setting BLE MTU to %d", appPrefs.bleMTU);
    BLEDevice::setMTU(appPrefs.bleMTU);

    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    BLEDevice::setSecurityCallbacks(new MySecurity());
    BLEDevice::setPower((esp_power_level_t)appPrefs.bleTxPower, ESP_BLE_PWR_TYPE_DEFAULT);

    // Ajustar parámetros de conexión para mayor estabilidad
    esp_ble_conn_update_params_t conn_params = {
        .min_int = 0x10,    // x 1.25ms = 20ms
        .max_int = 0x20,    // x 1.25ms = 40ms
        .latency = 0,       // Sin latencia para mayor estabilidad
        .timeout = 1000     // 10 segundos de timeout
    };
    
    esp_err_t result;
    int retries = 3;
    do {
        result = esp_ble_gap_update_conn_params(&conn_params);
        if (result != ESP_OK) {
            log_w("Failed to update connection parameters: %s, retrying...", esp_err_to_name(result));
            delay(100);  // Pequeña espera antes de reintentar
        }
    } while (result != ESP_OK && --retries > 0);

    if (result != ESP_OK) {
        log_e("Failed to update connection parameters after all retries: %s", esp_err_to_name(result));
    } else {
        log_i("Connection parameters updated successfully");
    }

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

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setKeySize(16);
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    log_i("Creating Scanner BLE service and characteristics");
    BLEService *pScannerService = pServer->createService(SNEAK32_SERVICE_UUID);

    pStatusCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)STATUS_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pStatusCharacteristic->setCallbacks(new ListSizesCallbacks());
    pStatusCharacteristic->addDescriptor(new BLE2902()); // Añadir CCCD
    BLEStatusUpdater.update();

    pTxCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)DATA_TRANSFER_UUID),
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
    pCommandsCharacteristic->setCallbacks(new BLECommands());
    pCommandsCharacteristic->addDescriptor(new BLE2902()); // Añadir CCCD

    BLECharacteristic *pFirmwareInfoCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)FIRMWARE_INFO_UUID),
        BLECharacteristic::PROPERTY_READ);
    pFirmwareInfoCharacteristic->setValue(getFirmwareInfoString().c_str());

    log_i("Starting BLE service");
    pScannerService->start();

    log_i("Configuring BLE advertising");
    BLEAdvertisingManager::setup();

    log_i("Starting BLE advertising");
    BLEAdvertisingManager::start();

    log_i("BLE Initialized");
}
