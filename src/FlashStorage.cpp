#include "FlashStorage.h"
#include <stdexcept>

extern WifiNetworkList ssidList;
extern WifiDeviceList stationsList;
extern BLEDeviceList bleDeviceList;

const char *FlashStorage::NAMESPACE = "device_lists";
const char *FlashStorage::WIFI_DEVICES_KEY = "wifi_devices";
const char *FlashStorage::BLE_DEVICES_KEY = "ble_devices";
const char *FlashStorage::WIFI_NETWORKS_KEY = "wifi_networks";

Preferences FlashStorage::preferences;

void FlashStorage::saveWifiDevices(const WifiDeviceList &list)
{
    preferences.begin(NAMESPACE, false);
    std::vector<WifiDevice> devices = list.getClonedList();
    std::vector<WifiDeviceStruct> deviceStructs(devices.size());

    for (size_t i = 0; i < devices.size(); ++i)
    {
        const auto &device = devices[i];
        auto &deviceStruct = deviceStructs[i];

        memcpy(deviceStruct.address, device.address.getBytes(), 6);
        deviceStruct.rssi = device.rssi;
        deviceStruct.channel = device.channel;
        deviceStruct.last_seen = device.last_seen;
        deviceStruct.times_seen = device.times_seen;
    }

    size_t serializedSize = deviceStructs.size() * sizeof(WifiDeviceStruct);
    preferences.putBytes(WIFI_DEVICES_KEY, deviceStructs.data(), serializedSize);
    preferences.end();
    Serial.printf("Saved %zu WiFi devices\n", deviceStructs.size());
}

void FlashStorage::saveBLEDevices(const BLEDeviceList &list)
{
    preferences.begin(NAMESPACE, false);
    std::vector<BLEFoundDevice> devices = list.getClonedList();
    std::vector<BLEDeviceStruct> deviceStructs(devices.size());

    for (size_t i = 0; i < devices.size(); ++i)
    {
        const auto &device = devices[i];
        auto &deviceStruct = deviceStructs[i];

        memcpy(deviceStruct.address, device.address.getBytes(), 6);
        deviceStruct.rssi = device.rssi;
        memset(deviceStruct.name, 0, sizeof(deviceStruct.name));
        strncpy(deviceStruct.name, device.name.c_str(), sizeof(deviceStruct.name) - 1);
        deviceStruct.name[sizeof(deviceStruct.name) - 1] = '\0';
        deviceStruct.isPublic = device.isPublic;
        deviceStruct.last_seen = device.last_seen;
        deviceStruct.times_seen = device.times_seen;
    }

    size_t serializedSize = deviceStructs.size() * sizeof(BLEDeviceStruct);
    preferences.putBytes(BLE_DEVICES_KEY, deviceStructs.data(), serializedSize);
    preferences.end();
    Serial.printf("Saved %zu BLE devices\n", deviceStructs.size());
}

void FlashStorage::saveWifiNetworks(const WifiNetworkList &list)
{
    preferences.begin(NAMESPACE, false);
    std::vector<WifiNetwork> networks = list.getClonedList();
    std::vector<WifiNetworkStruct> networkStructs(networks.size());

    for (size_t i = 0; i < networks.size(); ++i)
    {
        const auto &network = networks[i];
        auto &networkStruct = networkStructs[i];

        memset(networkStruct.ssid, 0, sizeof(networkStruct.ssid));
        strncpy(networkStruct.ssid, network.ssid.c_str(), sizeof(networkStruct.ssid) - 1);
        networkStruct.ssid[sizeof(networkStruct.ssid) - 1] = '\0';
        networkStruct.rssi = network.rssi;
        networkStruct.channel = network.channel;
        memset(networkStruct.type, 0, sizeof(networkStruct.type));
        strncpy(networkStruct.type, network.type.c_str(), sizeof(networkStruct.type) - 1);
        networkStruct.type[sizeof(networkStruct.type) - 1] = '\0';
        networkStruct.last_seen = network.last_seen;
        networkStruct.times_seen = network.times_seen;
    }

    size_t serializedSize = networkStructs.size() * sizeof(WifiNetworkStruct);
    preferences.putBytes(WIFI_NETWORKS_KEY, networkStructs.data(), serializedSize);
    preferences.end();
    Serial.printf("Saved %zu WiFi networks\n", networkStructs.size());
}

void FlashStorage::loadWifiDevices(WifiDeviceList &list)
{
    preferences.begin(NAMESPACE, true);
    size_t serializedSize = preferences.getBytesLength(WIFI_DEVICES_KEY);
    Serial.printf("Loading WiFi devices. Serialized size: %zu bytes\n", serializedSize);
    if (serializedSize > 0)
    {
        if (serializedSize % sizeof(WifiDeviceStruct) != 0)
        {
            Serial.printf("Error: Serialized size (%zu) is not a multiple of WifiDeviceStruct size (%zu)\n", serializedSize, sizeof(WifiDeviceStruct));
            preferences.end();
            return;
        }
        std::vector<WifiDeviceStruct> deviceStructs(serializedSize / sizeof(WifiDeviceStruct));
        preferences.getBytes(WIFI_DEVICES_KEY, deviceStructs.data(), serializedSize);
        Serial.printf("Loaded %zu devices from flash\n", deviceStructs.size());
        for (const auto &deviceStruct : deviceStructs)
        {
            Serial.printf("Loaded Device: %02X:%02X:%02X:%02X:%02X:%02X, RSSI: %d, Channel: %d, Last seen: %ld, Times seen: %u\n",
                          deviceStruct.address[0], deviceStruct.address[1], deviceStruct.address[2],
                          deviceStruct.address[3], deviceStruct.address[4], deviceStruct.address[5],
                          deviceStruct.rssi, deviceStruct.channel, deviceStruct.last_seen, deviceStruct.times_seen);
            WifiDevice device(MacAddress(deviceStruct.address), deviceStruct.rssi, deviceStruct.channel, deviceStruct.last_seen, deviceStruct.times_seen);
            list.addDevice(device);
        }
        Serial.printf("Successfully loaded %zu WiFi devices\n", list.size());
    }
    else
    {
        Serial.println("No WiFi devices to load");
    }
    preferences.end();
}

void FlashStorage::loadBLEDevices(BLEDeviceList &list)
{
    preferences.begin(NAMESPACE, true);
    size_t serializedSize = preferences.getBytesLength(BLE_DEVICES_KEY);
    Serial.printf("Loading BLE devices. Serialized size: %zu bytes\n", serializedSize);
    if (serializedSize > 0)
    {
        if (serializedSize % sizeof(BLEDeviceStruct) != 0)
        {
            Serial.printf("Error: Serialized size (%zu) is not a multiple of BLEDeviceStruct size (%zu)\n", serializedSize, sizeof(BLEDeviceStruct));
            preferences.end();
            return;
        }
        std::vector<BLEDeviceStruct> deviceStructs(serializedSize / sizeof(BLEDeviceStruct));
        preferences.getBytes(BLE_DEVICES_KEY, deviceStructs.data(), serializedSize);
        for (const auto &deviceStruct : deviceStructs)
        {
            BLEFoundDevice device(MacAddress(deviceStruct.address), deviceStruct.rssi, 
                String(deviceStruct.name), deviceStruct.isPublic, deviceStruct.last_seen, deviceStruct.times_seen);
            list.addDevice(device);
        }
        Serial.printf("Loaded %zu BLE devices\n", deviceStructs.size());
    }
    else
    {
        Serial.println("No BLE devices to load");
    }
    preferences.end();
}

void FlashStorage::loadWifiNetworks(WifiNetworkList &list)
{
    preferences.begin(NAMESPACE, true);
    size_t serializedSize = preferences.getBytesLength(WIFI_NETWORKS_KEY);
    Serial.printf("Loading WiFi networks. Serialized size: %zu bytes\n", serializedSize);
    if (serializedSize > 0)
    {
        if (serializedSize % sizeof(WifiNetworkStruct) != 0)
        {
            Serial.printf("Error: Serialized size (%zu) is not a multiple of WifiNetworkStruct size (%zu)\n", serializedSize, sizeof(WifiNetworkStruct));
            preferences.end();
            return;
        }
        std::vector<WifiNetworkStruct> networkStructs(serializedSize / sizeof(WifiNetworkStruct));
        preferences.getBytes(WIFI_NETWORKS_KEY, networkStructs.data(), serializedSize);
        for (const auto &networkStruct : networkStructs)
        {
            WifiNetwork network(String(networkStruct.ssid), networkStruct.rssi, networkStruct.channel, 
            String(networkStruct.type), networkStruct.last_seen, networkStruct.times_seen);
            list.addNetwork(network);
        }
        Serial.printf("Loaded %zu WiFi networks\n", networkStructs.size());
    }
    else
    {
        Serial.println("No WiFi networks to load");
    }
    preferences.end();
}

void FlashStorage::saveAll()
{
    FlashStorage::saveWifiDevices(stationsList);
    FlashStorage::saveBLEDevices(bleDeviceList);
    FlashStorage::saveWifiNetworks(ssidList);
}

void FlashStorage::loadAll()
{
    try
    {
        Serial.println("Starting to load all data from flash storage");
        stationsList.clear();
        Serial.println("Loading WiFi devices...");
        FlashStorage::loadWifiDevices(stationsList);
        Serial.printf("Loaded %zu WiFi devices\n", stationsList.size());

        bleDeviceList.clear();
        Serial.println("Loading BLE devices...");
        FlashStorage::loadBLEDevices(bleDeviceList);
        Serial.printf("Loaded %zu BLE devices\n", bleDeviceList.size());

        ssidList.clear();
        Serial.println("Loading WiFi networks...");
        FlashStorage::loadWifiNetworks(ssidList);
        Serial.printf("Loaded %zu WiFi networks\n", ssidList.size());

        Serial.println("All data loaded successfully from flash storage");
    }
    catch (const std::exception &e)
    {
        Serial.printf("Error loading from flash storage: %s\n", e.what());
        // Clear all lists in case of an error
        stationsList.clear();
        bleDeviceList.clear();
        ssidList.clear();
        Serial.println("All lists cleared due to error");
    }
}
