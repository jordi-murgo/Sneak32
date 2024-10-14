#include "WifiDeviceList.h"
#include <algorithm>

extern time_t base_time;

WifiDeviceList::WifiDeviceList(size_t maxSize) : maxSize(maxSize) {
  deviceList.reserve(maxSize);
  mutex = xSemaphoreCreateMutex();
  if (mutex == NULL) {
    Serial.println("Error creating mutex for WifiDeviceList");
  }
}

WifiDeviceList::~WifiDeviceList() {
  if (mutex != NULL) {
    vSemaphoreDelete(mutex);
  }
}

void WifiDeviceList::updateOrAddDevice(const MacAddress &address, int8_t rssi, uint8_t channel) {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
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

    xSemaphoreGive(mutex);
  } else {
    Serial.println("Failed to acquire mutex in WifiDeviceList");
  }
}

size_t WifiDeviceList::size() const {
  size_t currentSize = 0;
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    currentSize = deviceList.size();
    xSemaphoreGive(mutex);
  }
  return currentSize;
}

std::vector<WifiDevice> WifiDeviceList::getClonedList() const {
  std::vector<WifiDevice> clonedList;
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    clonedList = deviceList;
    xSemaphoreGive(mutex);
  }
  return clonedList;
}

void WifiDeviceList::addDevice(const WifiDevice& device) {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    deviceList.push_back(device);
    Serial.printf("Added new WiFi device: %s\n", device.address.toString().c_str());
    xSemaphoreGive(mutex);
  } else {
    Serial.println("Failed to acquire mutex in addDevice");
  }
}

void WifiDeviceList::clear() {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    deviceList.clear();
    Serial.println("WiFi device list cleared");
    xSemaphoreGive(mutex);
  } else {
    Serial.println("Failed to acquire mutex in clear");
  }
}

void WifiDeviceList::remove_irrelevant_stations() {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    if (deviceList.empty()) {
      xSemaphoreGive(mutex);
      return;
    }

    Serial.printf("Removing irrelevant stations. List size: %zu\n", deviceList.size());

    uint32_t total_seens = 0;
    for (const auto &device : deviceList) {
      total_seens += device.times_seen;
    }

    uint32_t min_seens = static_cast<uint32_t>(round(total_seens / static_cast<double>(deviceList.size()) / 3.0));

    deviceList.erase(
      std::remove_if(deviceList.begin(), deviceList.end(),
        [min_seens](const WifiDevice &device) {
          Serial.printf("Irrellevant device: %s, seen: %u (min_seens: %u)\n", 
                        device.address.toString().c_str(), device.times_seen, min_seens);
          return device.times_seen < min_seens;
        }),
      deviceList.end()
    );

    Serial.printf("Removed irrelevant stations. New list size: %zu\n", deviceList.size());
    xSemaphoreGive(mutex);
  } else {
    Serial.println("Failed to acquire mutex in remove_irrelevant_stations");
  }
}
