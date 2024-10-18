#include "BLEDeviceList.h"
#include <algorithm>
#include "AppPreferences.h"
#include <mutex>

extern time_t base_time;

// Constructor
BLEDeviceList::BLEDeviceList(size_t maxSize) : maxSize(maxSize) {
  deviceList.reserve(maxSize);
}

// Destructor
BLEDeviceList::~BLEDeviceList() = default;

// Mètode per actualitzar o afegir un dispositiu
void BLEDeviceList::updateOrAddDevice(const MacAddress &address, int rssi, const String &name, bool isPublic) {
  std::lock_guard<std::mutex> lock(deviceMutex);
  
  auto it = std::find_if(deviceList.begin(), deviceList.end(),
                         [&address](const BLEFoundDevice &device) {
                           return device.address == address;
                         });

  time_t now = millis() / 1000 + base_time;

  if (it != deviceList.end()) {
    it->rssi = rssi;
    it->last_seen = now;
    it->times_seen++;
    if (name.length() > 0) {
      it->name = name;
    }
  } else {
    BLEFoundDevice newDevice(address, rssi, name, isPublic, now);

    if (deviceList.size() < maxSize) {
      deviceList.emplace_back(newDevice);
      Serial.printf("Added new BLE device: %s %s\n", newDevice.address.toString().c_str(), newDevice.name.c_str());
    } else {
      auto oldest = std::min_element(deviceList.begin(), deviceList.end(),
                                     [](const BLEFoundDevice &a, const BLEFoundDevice &b) {
                                       return a.last_seen < b.last_seen;
                                     });
      if (oldest != deviceList.end()) {
        Serial.printf("Replacing BLE device: %s %s (seen %u times) with new device: %s %s\n", 
                      oldest->address.toString().c_str(), oldest->name.c_str(), oldest->times_seen, 
                      newDevice.address.toString().c_str(), newDevice.name.c_str());
        *oldest = std::move(newDevice);
      }
    }
  }
}

// Mètode per obtenir la mida de la llista
size_t BLEDeviceList::size() const {
  return deviceList.size();
}

// Mètode per obtenir una còpia clonada de la llista
std::vector<BLEFoundDevice> BLEDeviceList::getClonedList() const {
  return deviceList;  // Return a copy of the list to avoid locking issues
}

void BLEDeviceList::addDevice(const BLEFoundDevice& device) {
  std::lock_guard<std::mutex> lock(deviceMutex);
  deviceList.push_back(device);
  Serial.printf("Added new BLE device: %s %s\n", device.address.toString().c_str(), device.name.c_str());
}

void BLEDeviceList::clear() {
  std::lock_guard<std::mutex> lock(deviceMutex);
  deviceList.clear();
  Serial.println("BLE device list cleared");
}

void BLEDeviceList::remove_irrelevant_devices() {
  std::lock_guard<std::mutex> lock(deviceMutex);
  
  if (deviceList.empty()) {
    return;
  }

  size_t initial_size = deviceList.size();
  Serial.printf("Removing irrelevant BLE devices. List size: %zu\n", initial_size);

  deviceList.erase(
    std::remove_if(deviceList.begin(), deviceList.end(),
      [](const BLEFoundDevice &device) {
        Serial.printf("Checking BLE device: %s, rssi: %d (minimal_rssi: %d)\n", 
                      device.address.toString().c_str(), device.rssi, appPrefs.minimal_rssi);
        return device.rssi < appPrefs.minimal_rssi;
      }),
    deviceList.end()
  );

  Serial.printf("Removed %zu irrelevant BLE devices. New list size: %zu\n", initial_size - deviceList.size(), deviceList.size());
  Serial.println("BLEDeviceList::remove_irrelevant_devices - Exiting");
}

bool BLEDeviceList::is_device_in_list(const MacAddress& address) {
  auto deviceListCopy = getClonedList();
  auto it = std::find_if(deviceListCopy.begin(), deviceListCopy.end(),
                         [&address](const BLEFoundDevice &device) {
                           return device.address == address;
                         });

  return it != deviceListCopy.end();
}
