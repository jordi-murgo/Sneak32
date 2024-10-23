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
BLECharacteristic *pListSizesCharacteristic = nullptr;
BLEScan *pBLEScan = nullptr;
bool deviceConnected = false;



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
                    Serial.printf(">>> Device connected: %s\n", client->getPeerAddress().toString().c_str());
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
        if (appPrefs.operation_mode == OPERATION_MODE_DETECTION)
        {
            updateDetectedDevicesCharacteristic();
        }
        else
        {
            updateListSizesCharacteristic();
        }
    }
};

/**
 * @brief Formats a version number into a string.
 *
 * This function takes a version number and converts it into a string
 * in the format "major.minor.patch".
 *
 * @param version The version number to format.
 * @return The formatted version string.
 */
String formatVersion(long version)
{
    int major = version / 10000;
    int minor = (version / 100) % 100;
    int patch = version % 100;
    return String(major) + "." +
           String(minor) + "." +
           String(patch);
}

String getHardwareInfo()
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);

    String mArch = String(CONFIG_SDK_TOOLPREFIX);
    if (mArch.endsWith("-"))
    {
        mArch = mArch.substring(0, mArch.length() - 1);
    }

    String info = "Architecture: " + mArch + "\n";
    info += "Chip Model: " + getChipInfo() + "\n";
    info += "CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz\n";
    info += "Flash Size: " + String(ESP.getFlashChipSize() / 1024) + " KBytes\n";
    info += "Flash Speed: " + String(ESP.getFlashChipSpeed() / 1000000) + " MHz\n";
    info += "RAM Size: " + String(ESP.getHeapSize() / 1024) + " KBytes\n";
#ifdef CONFIG_SPIRAM
    info += "PSRAM Size: " + String(ESP.getPsramSize()) + " bytes\n";
#endif
    info += "MAC Address: " + String(MacAddress(mac).toString().c_str()) + "\n";

    return info;
}

// Implementa una función para obtener la información del firmware
String getFirmwareInfo()
{
    String info = "PlatformIO Version: " + String(formatVersion(PLATFORMIO)) + "\n";
    info += "Arduino Version: " + String(formatVersion(ARDUINO)) + "\n";
    info += "GNU Compiler: " + String(__VERSION__) + "\n";
    info += "C++ Version: " + String(__cplusplus) + "\n";
    info += "ESP-IDF Version: " + String(ESP.getSdkVersion()) + "\n";
    info += "Device Board: " + String(ARDUINO_BOARD) + "\n";

    info += getHardwareInfo();

    info += "Compilation date: " + String(__DATE__) + " " + String(__TIME__) + "\n";

    info += "Sketch Size: " + String(ESP.getSketchSize() / 1024) + " KBytes\n";
    info += "Sketch MD5: " + String(ESP.getSketchMD5()) + "\n";

    return info;
}

class MySecurity : public BLESecurityCallbacks {
public:
    uint32_t onPassKeyRequest(){
        Serial.println("Solicitud de código de acceso");
        return 0; // Si no usas códigos de acceso, retorna 0
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
            Serial.println("Emparejamiento exitoso");
            // Obtener la dirección MAC del cliente
            BLEAddress clientAddress = BLEAddress(auth_cmpl.bd_addr);
            Serial.print("Dirección MAC del cliente: ");
            Serial.println(clientAddress.toString().c_str());

            strncpy(appPrefs.authorized_address, clientAddress.toString().c_str(), sizeof(appPrefs.authorized_address) - 1);
            appPrefs.authorized_address[sizeof(appPrefs.authorized_address) - 1] = '\0';
            saveAppPreferences();

            BLEAdvertisingManager::setup();
        } else {
            Serial.println("Emparejamiento fallido");
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

    // Configura parámetros de seguridad
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_ONLY; // Solo conexiones seguras
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));

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

    pListSizesCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)LIST_SIZES_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pListSizesCharacteristic->setCallbacks(new ListSizesCallbacks());
    pListSizesCharacteristic->addDescriptor(new BLE2902());

    if (appPrefs.operation_mode == OPERATION_MODE_DETECTION)
    {
        updateDetectedDevicesCharacteristic();
    }
    else if (appPrefs.operation_mode == OPERATION_MODE_SCAN)
    {
        updateListSizesCharacteristic();
    }
    else
    {
        pListSizesCharacteristic->setValue("0:0:0:0");
    }

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
        BLECharacteristic::PROPERTY_WRITE);
    pCommandsCharacteristic->addDescriptor(new BLE2902());
    pCommandsCharacteristic->setCallbacks(new CommandsCallbacks());

    BLECharacteristic *pFirmwareInfoCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)FIRMWARE_INFO_UUID),
        BLECharacteristic::PROPERTY_READ);
    pFirmwareInfoCharacteristic->setValue(getHardwareInfo().c_str());

    Serial.println("Starting BLE service");
    pScannerService->start();

    Serial.println("Configuring BLE advertising");
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisingManager::setup();

    Serial.println("Starting BLE advertising");
    BLEAdvertisingManager::start();

    Serial.println("BLE Initialized");
}

void updateDetectedDevicesCharacteristic()
{
    static size_t last_ssids_size = 0;
    static size_t last_stations_size = 0;
    static size_t last_ble_devices_size = 0;
    static bool last_alarm_detected = false;

    size_t ssids_size = WifiDetector.getDetectedNetworksCount();
    size_t stations_size = WifiDetector.getDetectedDevicesCount();
    size_t ble_devices_size = BLEDetector.getDetectedDevicesCount();
    bool alarm = BLEDetector.isSomethingDetected() | WifiDetector.isSomethingDetected();

    // Crear la cadena de texto con los tamaños de las listas
    String sizeString = String(ssids_size) + ":" +
                        String(stations_size) + ":" +
                        String(ble_devices_size) + ":" +
                        String(alarm);

    pListSizesCharacteristic->setValue(sizeString.c_str());
    Serial.printf("List sizes updated -> %s\n", sizeString.c_str());

    if (deviceConnected)
    {
        if (alarm != last_alarm_detected || ssidList.size() != last_ssids_size ||
            stationsList.size() != last_stations_size || bleDeviceList.size() != last_ble_devices_size)
        {
            Serial.println("Notifying list sizes");
            pListSizesCharacteristic->notify();

            last_ssids_size = ssidList.size();
            last_stations_size = stationsList.size();
            last_ble_devices_size = bleDeviceList.size();
            last_alarm_detected = alarm;
        }
    }
}

/**
 * @brief Updates the list sizes characteristic with the current sizes of the lists.
 *
 * This function checks if the sizes of the lists have changed and updates the characteristic accordingly.
 * It also notifies the clients if the device is connected.
 */
void updateListSizesCharacteristic()
{
    static size_t last_ssids_size = 0;
    static size_t last_stations_size = 0;
    static size_t last_ble_devices_size = 0;

    boolean alarm_detected = BLEDetector.isSomethingDetected() | WifiDetector.isSomethingDetected();

    if (pListSizesCharacteristic != nullptr &&
        (ssidList.size() != last_ssids_size ||
         stationsList.size() != last_stations_size ||
         bleDeviceList.size() != last_ble_devices_size))
    {
        // Crear la cadena de texto con los tamaños de las listas
        String sizeString = String(ssidList.size()) + ":" +
                            String(stationsList.size()) + ":" +
                            String(bleDeviceList.size()) + ":0";

        Serial.printf("List sizes updated -> %s\n", sizeString.c_str());
        pListSizesCharacteristic->setValue(sizeString.c_str());
        if (deviceConnected)
        {
            Serial.println("Notifying list sizes");
            pListSizesCharacteristic->notify();
        }
        last_ssids_size = ssidList.size();
        last_stations_size = stationsList.size();
        last_ble_devices_size = bleDeviceList.size();
    }
}



