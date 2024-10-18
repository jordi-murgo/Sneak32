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
AppPreferencesData settings;

// Upload Variables
String preparedJsonData;
uint16_t totalPackets = 0;
unsigned long lastPacketRequestTime = 0;

// Callback implementations
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer) override
    {
        deviceConnected = true;
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
        if (appPrefs.operation_mode == OPERATION_MODE_DETECTION) {
            updateDetectedDevicesCharacteristic();
        } else {
            updateListSizesCharacteristic();
        }
    }
};

class SendDataOverBLECallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic) override
    {
        bool only_relevant_stations = false;
        std::string value = pCharacteristic->getValue();

        if (value.length() >= 4)
        {
            uint16_t requestedPacket = strtoul(value.substr(0, 4).c_str(), NULL, 16);
            Serial.printf("Received request for packet %d\n", requestedPacket);
            if (value.length() >= 5)
            {
                only_relevant_stations = true;
            }
            sendPacket(requestedPacket, only_relevant_stations);
            lastPacketRequestTime = millis();
        }
    }
};

class SettingsCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic) override
    {
        std::string value = pCharacteristic->getValue();
        std::istringstream ss(value);
        std::string token;

        uint8_t operation_mode = appPrefs.operation_mode;

        Serial.printf("SettingsCallbacks::onWrite -> %s\n", value.c_str());

        // Leer los valores
        if (std::getline(ss, token, ':'))
            appPrefs.only_management_frames = std::stoi(token) ? true : false;
        if (std::getline(ss, token, ':'))
            appPrefs.minimal_rssi = std::stoi(token);
        if (std::getline(ss, token, ':'))
            appPrefs.loop_delay = std::stoul(token);
        if (std::getline(ss, token, ':'))
            appPrefs.ble_scan_period = std::stoul(token);
        if (std::getline(ss, token, ':'))
            appPrefs.ignore_random_ble_addresses = std::stoi(token) ? true : false;
        if (std::getline(ss, token, ':'))
            appPrefs.ble_scan_duration = std::stoul(token);
        if (std::getline(ss, token, ':'))
            appPrefs.operation_mode = std::stoi(token);
        if (std::getline(ss, token, ':'))
            appPrefs.passive_scan = std::stoi(token) ? true : false;
        if (std::getline(ss, token, ':'))
            appPrefs.stealth_mode = std::stoi(token) ? true : false;
        if (std::getline(ss, token, ':'))
            appPrefs.autosave_interval = std::stoul(token);
        
        // El deviceName ahora es el último elemento
        if (std::getline(ss, token))
        {
            char olddevice_name[32];
            strncpy(olddevice_name, appPrefs.device_name, sizeof(olddevice_name) - 1);
            olddevice_name[sizeof(olddevice_name) - 1] = '\0';

            strncpy(appPrefs.device_name, token.c_str(), sizeof(appPrefs.device_name) - 1);
            appPrefs.device_name[sizeof(appPrefs.device_name) - 1] = '\0';

            if (strcmp(olddevice_name, appPrefs.device_name) != 0)
            {
                updateBLEDeviceName(appPrefs.device_name);
            }
        }

        if (operation_mode != appPrefs.operation_mode) {
            switch (appPrefs.operation_mode) {
                case OPERATION_MODE_SCAN:
                    // Deinicializar WifiDetect y BLEDetect
                    WifiDetector.stop();
                    BLEDetector.stop();

                    // Inicializar WifiScan y BLE Scan
                    WifiScanner.setup();
                    BLEScanner.setup();
                    break;

                case OPERATION_MODE_DETECTION:
                    // Deinicializar WifiScan y BLE Scan
                    WifiScanner.stop();
                    BLEScanner.stop();
                    // Inicializar WifiDetection y BLE Detect
                    WifiDetector.setup();
                    BLEDetector.setup();
                    break;

                default: // OPERATION_MODE_OFF
                    // Deinicializar WifiScan
                    WifiScanner.stop();
                    BLEScanner.stop();
                    // Deinicializar WifiDetection
                    WifiDetector.stop();
                    BLEDetector.stop();
                    break;
            }
        }

        saveAppPreferences();
    }

    void onRead(BLECharacteristic *pCharacteristic) override
    {
        // Crear la cadena de configuración
        String settingsString = String(appPrefs.only_management_frames) + ":" +
                                String(appPrefs.minimal_rssi) + ":" +
                                String(appPrefs.loop_delay) + ":" +
                                String(appPrefs.ble_scan_period) + ":" +
                                String(appPrefs.ignore_random_ble_addresses) + ":" +
                                String(appPrefs.ble_scan_duration) + ":" +
                                String(appPrefs.operation_mode) + ":" +
                                String(appPrefs.passive_scan) + ":" +
                                String(appPrefs.stealth_mode) + ":" +
                                String(appPrefs.autosave_interval) + ":" +
                                String(appPrefs.device_name);
        pCharacteristic->setValue(settingsString.c_str());
    }
};

/**
 * @brief Callback class for handling advertised devices (BLE SCAN)
 */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice) override
    {
        try
        {
            int rssi = advertisedDevice.getRSSI();

            if (rssi >= appPrefs.minimal_rssi)
            {
                std::string addressStr = advertisedDevice.getAddress().toString();
                std::string nameStr = advertisedDevice.haveName() ? advertisedDevice.getName() : "";
                std::string appearanceStr = std::to_string(advertisedDevice.getAppearance());
                std::string uuidStr = advertisedDevice.getServiceUUID().toString();
                boolean isPublic = (advertisedDevice.getAddressType() == BLE_ADDR_TYPE_PUBLIC);

                // Fix: Correctly initialize esp_bd_addr_t array
                esp_bd_addr_t bleaddr;
                memcpy(bleaddr, advertisedDevice.getAddress().getNative(), sizeof(esp_bd_addr_t));

                Serial.printf("Address: %s, isPublic: %d, Name: '%s', Appearance: %s, Service UUID: %s\n",
                              addressStr.c_str(), isPublic, nameStr.c_str(), appearanceStr.c_str(), uuidStr.c_str());

                if (!appPrefs.ignore_random_ble_addresses || isPublic)
                {
                    // Update or add device to the list
                    bleDeviceList.updateOrAddDevice(MacAddress(bleaddr), rssi, String(nameStr.c_str()), isPublic);
                }
            }
        }
        catch (const std::exception &e)
        {
            Serial.printf("Exception in onResult: %s\n", e.what());
        }
        catch (...)
        {
            Serial.println("Unknown exception in onResult");
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

// Function implementations
void setupBLE()
{
    Serial.println("Initializing BLE");
    BLEDevice::init(appPrefs.device_name);

    // Set the BLE TX power to -12 dBm
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N12);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    Serial.println("Creating Scanner BLE service and characteristics");
    BLEService *pScannerService = pServer->createService(SCANNER_SERVICE_UUID);

    pListSizesCharacteristic = pScannerService->createCharacteristic(
        BLEUUID((uint16_t)LIST_SIZES_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pListSizesCharacteristic->setCallbacks(new ListSizesCallbacks());
    pListSizesCharacteristic->addDescriptor(new BLE2902());

    if (appPrefs.operation_mode == OPERATION_MODE_DETECTION) {
        updateDetectedDevicesCharacteristic();
    } else if (appPrefs.operation_mode == OPERATION_MODE_SCAN) {
        updateListSizesCharacteristic();
    } else {
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

    pAdvertising->addServiceUUID(SCANNER_SERVICE_UUID);
    pAdvertising->setAppearance(ESP_BLE_APPEARANCE_SPORTS_WATCH);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);

    // if(appPrefs.stealth_mode) {
    //     pAdvertising->setAdvertisementType(ADV_TYPE_DIRECT_IND_LOW);
    // } else {
        pAdvertising->setAdvertisementType(ADV_TYPE_IND);
    // }

    Serial.println("Starting BLE advertising");
    BLEDevice::startAdvertising();

    Serial.println("BLE Initialized");
}


void updateDetectedDevicesCharacteristic()
{
    Serial.println("Updating detected devices characteristic");
    static size_t last_ssids_size = 0;
    static size_t last_stations_size = 0;
    static size_t last_ble_devices_size = 0;
    static bool alarm_detected = false;
    
    bool alarm = BLEDetector.isSomethingDetected() | WifiDetector.isSomethingDetected();
    if (pListSizesCharacteristic != nullptr)
    {
        // Crear la cadena de texto con los tamaños de las listas
        String sizeString = String(WifiDetector.getDetectedNetworksCount()) + ":" +
                            String(WifiDetector.getDetectedDevicesCount()) + ":" +
                            String(BLEDetector.getDetectedDevicesCount()) + ":" +
                            String(alarm);

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
    Serial.println("End of updateDetectedDevicesCharacteristic");
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

// To large to be a local variable
String jsonString = "";

/**
 * @brief Generates a JSON string containing WiFi and BLE device information.
 *
 * This function creates a JSON object with the following information:
 * - Timestamp
 * - Counts of stations and SSIDs
 * - Arrays of detected WiFi stations, SSIDs, and BLE devices
 *
 * @return String A JSON-formatted string containing the collected data.
 */
String generateJsonString(boolean only_relevant_stations = false)
{
    jsonString.clear();

    auto stationsClonedList = stationsList.getClonedList();
    auto ssidsClonedList = ssidList.getClonedList();
    auto bleDevicesClonedList = bleDeviceList.getClonedList();

    jsonString = "{";

    // Add stations array
    jsonString += "\"stations\":[";
    // Calculamos cual el valor relevante para el times_seen de los dispositivos wifi
    uint32_t total_seens = 0;
    for (const auto &device : stationsClonedList)
    {
        total_seens += device.times_seen;
    }
    uint32_t min_seens = static_cast<uint32_t>(round(total_seens / static_cast<double>(stationsClonedList.size()) / 3.0));

    Serial.printf("total_seens: %u, total_stations: %u, min_seens: %u, only_relevant_stations: %d\n", total_seens, stationsClonedList.size(), min_seens, only_relevant_stations);

    int stationItems = 0;
    for (size_t i = 0; i < stationsClonedList.size(); ++i)
    {
        if (only_relevant_stations && stationsClonedList[i].times_seen < min_seens)
        {
            Serial.printf("Skipping station %s with times_seen %u (min_seens: %u)\n",
                          stationsClonedList[i].address.toString().c_str(),
                          stationsClonedList[i].times_seen, min_seens);
            continue;
        }

        if (only_relevant_stations && stationsClonedList[i].rssi < appPrefs.minimal_rssi)
        {
            Serial.printf("Skipping station %s with RSSI %d (minimal_rssi: %d)\n",
                          stationsClonedList[i].address.toString().c_str(),
                          stationsClonedList[i].rssi, appPrefs.minimal_rssi);
            continue;
        }

        if (stationItems++ > 0)
            jsonString += ",";

        char station_json[256];

        snprintf(station_json, sizeof(station_json),
                 "{\"mac\":\"%s\",\"rssi\":%d,\"channel\":%d,\"last_seen\":%d,\"times_seen\":%u}",
                 stationsClonedList[i].address.toString().c_str(),
                 stationsClonedList[i].rssi,
                 stationsClonedList[i].channel,
                 stationsClonedList[i].last_seen,
                 stationsClonedList[i].times_seen);

        jsonString += String(station_json);
    }
    jsonString += "],";

    // Add SSIDs array
    jsonString += "\"ssids\":[";
    for (size_t i = 0; i < ssidsClonedList.size(); ++i)
    {
        if (only_relevant_stations && ssidsClonedList[i].rssi < appPrefs.minimal_rssi)
        {
            Serial.printf("Skipping SSID %s with RSSI %d (minimal_rssi: %d)\n",
                          ssidsClonedList[i].ssid.c_str(),
                          ssidsClonedList[i].rssi, appPrefs.minimal_rssi);
            continue;
        }


        if (i > 0)
            jsonString += ",";
        char ssid_json[256];

        snprintf(ssid_json, sizeof(ssid_json),
                 "{\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%d,\"type\":\"%s\",\"last_seen\":%d,\"times_seen\":%u}",
                 ssidsClonedList[i].ssid.c_str(),
                 ssidsClonedList[i].rssi,
                 ssidsClonedList[i].channel,
                 ssidsClonedList[i].type.c_str(),
                 ssidsClonedList[i].last_seen,
                 ssidsClonedList[i].times_seen);

        jsonString += ssid_json;
    }
    jsonString += "],";

    jsonString += "\"ble_devices\":[";
    for (size_t i = 0; i < bleDevicesClonedList.size(); ++i)
    {
        if (only_relevant_stations && bleDevicesClonedList[i].rssi < appPrefs.minimal_rssi)
        {
            Serial.printf("Skipping BLE device %s with RSSI %d (minimal_rssi: %d)\n",
                          bleDevicesClonedList[i].address.toString().c_str(),
                          bleDevicesClonedList[i].rssi, appPrefs.minimal_rssi);
            continue;
        }

        if (i > 0)
            jsonString += ",";
        char ble_json[256];
        snprintf(ble_json, sizeof(ble_json),
                 "{\"address\":\"%s\",\"name\":\"%s\",\"rssi\":%d,\"last_seen\":%d,\"is_public\":\"%s\",\"times_seen\":%u}",
                 bleDevicesClonedList[i].address.toString().c_str(),
                 bleDevicesClonedList[i].name.c_str(),
                 bleDevicesClonedList[i].rssi,
                 bleDevicesClonedList[i].last_seen,
                 bleDevicesClonedList[i].isPublic ? "true" : "false",
                 bleDevicesClonedList[i].times_seen);
        jsonString += ble_json;
    }
    jsonString += "],";

    // Add timestamp, counts, and free heap
    jsonString += "\"timestamp\":" + String((millis() / 1000) + base_time) + ",";
    jsonString += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    jsonString += "\"min_seens\":" + String(min_seens) + ",";
    jsonString += "\"minimal_rssi\":" + String(appPrefs.minimal_rssi) + ",";
    jsonString += "\"stations_count\":" + String(stationItems) + ",";
    jsonString += "\"ssids_count\":" + String(ssidsClonedList.size()) + ",";
    jsonString += "\"ble_devices_count\":" + String(bleDevicesClonedList.size());

    jsonString += "}";

    return jsonString;
}

// Implement other functions like sendPacket, prepareJsonData, etc. here
void prepareJsonData(boolean only_relevant_stations = false)
{
    Serial.printf("Preparing JSON data with only_relevant_stations: %d\n", only_relevant_stations);
    preparedJsonData = generateJsonString(only_relevant_stations);
}

// Constants for packet markers
#define PACKET_START_MARKER "START:"
#define PACKET_END_MARKER "END"

#define BLE_MTU_SIZE 500                                    // Adjust this based on your BLE MTU size
#define PACKET_DELAY 100                                    // Delay between packets in milliseconds
#define PACKET_HEADER_SIZE 4                                // 4 bytes para el número de paquete
#define MAX_PACKET_SIZE (BLE_MTU_SIZE - PACKET_HEADER_SIZE) // 496 bytes para los datos del paquete
#define PACKET_TIMEOUT 5000                                 // 5 segundos de timeout
#define TRANSMISSION_TIMEOUT 10000
/**
 * @brief Sends a packet of data over BLE. (XModem like)
 *
 * @param packetNumber The number of the packet to send.
 */
void sendPacket(uint16_t packetNumber, boolean only_relevant_stations = false)
{
    if (pTxCharacteristic != nullptr && deviceConnected)
    {
        if (packetNumber == 0)
        {
            prepareJsonData(only_relevant_stations);
            totalPackets = 1 + preparedJsonData.length() / MAX_PACKET_SIZE;

            // Enviar marcador de inicio
            char packetsHeader[5];
            snprintf(packetsHeader, sizeof(packetsHeader), "%04X", totalPackets);

            String startMarker = PACKET_START_MARKER + String(packetsHeader);
            pTxCharacteristic->setValue(startMarker.c_str());
            pTxCharacteristic->notify();
            Serial.println("Sent " + startMarker);
        }
        else if (packetNumber <= totalPackets)
        {
            // Enviar paquete de datos
            size_t startIndex = (packetNumber - 1) * MAX_PACKET_SIZE;
            size_t endIndex = min(startIndex + MAX_PACKET_SIZE, preparedJsonData.length());
            String chunk = preparedJsonData.substring(startIndex, endIndex);

            char packetHeader[PACKET_HEADER_SIZE + 1];
            snprintf(packetHeader, sizeof(packetHeader), "%04X", packetNumber);
            String packetData = String(packetHeader) + chunk;

            pTxCharacteristic->setValue(packetData.c_str());
            pTxCharacteristic->notify();
            Serial.println("Sent packet " + String(packetNumber));

            if (packetNumber == totalPackets)
            {
                // Limpiar el buffer de datos preparados
                preparedJsonData.clear();

                // Enviar marcador de fin después del último paquete
                delay(PACKET_DELAY);
                pTxCharacteristic->setValue(PACKET_END_MARKER);
                pTxCharacteristic->notify();
                Serial.println("Sent " + String(PACKET_END_MARKER));
            }
        }
    }
}

void checkTransmissionTimeout()
{
    if (millis() - lastPacketRequestTime > TRANSMISSION_TIMEOUT || !deviceConnected)
    {
        preparedJsonData.clear();
    }
}

// Función para actualizar el nombre del dispositivo BLE
void updateBLEDeviceName(const char *newName)
{
    Serial.printf("Updating BLE device name to: %s\n", newName);

    // Detener la publicidad
    BLEDevice::stopAdvertising();

    // Cambiar el nombre del dispositivo
    esp_ble_gap_set_device_name(newName);

    // Reiniciar la publicidad
    BLEDevice::startAdvertising();

    Serial.println("BLE device name updated and advertising restarted");
}