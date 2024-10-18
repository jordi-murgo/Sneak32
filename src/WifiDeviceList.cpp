#include "WifiDeviceList.h"
#include <algorithm>
#include "AppPreferences.h"

extern time_t base_time;

WifiDeviceList::WifiDeviceList(size_t maxSize) : maxSize(maxSize) {
  deviceList.reserve(maxSize);
}

WifiDeviceList::~WifiDeviceList() = default;

void WifiDeviceList::updateOrAddDevice(const MacAddress &address, int8_t rssi, uint8_t channel) {
  std::lock_guard<std::mutex> lock(deviceMutex);
  
  auto it = std::find_if(deviceList.begin(), deviceList.end(),
                         [&address](const WifiDevice &device) {
                           return device.address == address;
                         });

  time_t now = millis() / 1000 + base_time;

  if (it != deviceList.end()) {
    it->rssi = rssi;
    it->channel = channel;
    it->last_seen = now;
    it->times_seen++;  
  } else {
    WifiDevice newDevice(address, rssi, channel, now);

    if (deviceList.size() < maxSize) {
      deviceList.push_back(newDevice);
      Serial.printf("Added new WiFi device: %s\n", newDevice.address.toString().c_str());
    } else {
      auto oldest = std::min_element(deviceList.begin(), deviceList.end(),
                                     [](const WifiDevice &a, const WifiDevice &b) {
                                       return a.last_seen < b.last_seen;
                                     });
      Serial.printf("Replacing WiFi device: %s (seen %u times) with new device: %s\n", 
                    oldest->address.toString().c_str(), oldest->times_seen, newDevice.address.toString().c_str());
      *oldest = newDevice;
    }
  }
}

size_t WifiDeviceList::size() const {
  return deviceList.size();
}

std::vector<WifiDevice> WifiDeviceList::getClonedList() const {
  return deviceList;  // Return a copy of the list to avoid locking issues
}

void WifiDeviceList::addDevice(const WifiDevice& device) {
  std::lock_guard<std::mutex> lock(deviceMutex);
  deviceList.push_back(device);
  Serial.printf("Added new WiFi device: %s\n", device.address.toString().c_str());
}

void WifiDeviceList::clear() {
  std::lock_guard<std::mutex> lock(deviceMutex);
  deviceList.clear();
  Serial.println("WiFi device list cleared");
}

void WifiDeviceList::remove_irrelevant_stations() {
  std::lock_guard<std::mutex> lock(deviceMutex);
  
  if (deviceList.empty()) {
    return;
  }

  size_t initial_size = deviceList.size();
  Serial.printf("Removing irrelevant stations. List size: %zu\n", initial_size);

  uint32_t total_seens = 0;
  for (const auto &device : deviceList) {
    total_seens += device.times_seen;
  }

  uint32_t min_seens = static_cast<uint32_t>(round(total_seens / static_cast<double>(deviceList.size()) / 3.0));

  deviceList.erase(
    std::remove_if(deviceList.begin(), deviceList.end(),
      [min_seens](const WifiDevice &device) {
        Serial.printf("Irrelevant device: %s, seen: %u (min_seens: %u), rssi: %d (minimal_rssi: %d)\n", 
                      device.address.toString().c_str(), device.times_seen, min_seens, device.rssi, appPrefs.minimal_rssi);
        return device.times_seen < min_seens || device.rssi < appPrefs.minimal_rssi;
      }),
    deviceList.end()
  );

  Serial.printf("Removed %zu irrelevant stations. New list size: %zu\n", initial_size - deviceList.size(), deviceList.size());
  Serial.println("WifiDeviceList::remove_irrelevant_stations - Exiting");
}

bool WifiDeviceList::is_device_in_list(const MacAddress& address) {
  auto deviceListCopy = getClonedList();
  auto it = std::find_if(deviceListCopy.begin(), deviceListCopy.end(),
                         [&address](const WifiDevice &device) {
                           return device.address == address;
                         });
  return it != deviceListCopy.end();
}
