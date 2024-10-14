#include "WifiNetworkList.h"
#include <algorithm>

extern time_t base_time;

WifiNetworkList::WifiNetworkList(size_t maxSize) : maxSize(maxSize) {
  networkList.reserve(maxSize);
  mutex = xSemaphoreCreateMutex();
  if (mutex == NULL) {
    Serial.println("Error creating mutex for WifiNetworkList");
  }
}

WifiNetworkList::~WifiNetworkList() {
  if (mutex != NULL) {
    vSemaphoreDelete(mutex);
  }
}

void WifiNetworkList::updateOrAddNetwork(const String &ssid, int8_t rssi, uint8_t channel, const String &type) {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    auto it = std::find_if(networkList.begin(), networkList.end(),
                           [&ssid](const WifiNetwork &network) {
                             return network.ssid == ssid;
                           });

    time_t now = millis() / 1000 + base_time;

    if (it != networkList.end()) {
      it->rssi = rssi;
      it->channel = channel;
      it->type = type;
      it->last_seen = now;
      it->times_seen++;  // Increment times seen
    } else {
      WifiNetwork newNetwork(ssid, rssi, channel, type, now);

      if (networkList.size() < maxSize) {
        networkList.push_back(newNetwork);
        Serial.printf("Added new network: %s\n", newNetwork.ssid.c_str());
      } else {
        auto oldest = std::min_element(networkList.begin(), networkList.end(),
                                       [](const WifiNetwork &a, const WifiNetwork &b) {
                                         return a.last_seen < b.last_seen;
                                       });
        Serial.printf("Replacing network: %s (seen %u times) with new network: %s\n", 
                      oldest->ssid.c_str(), oldest->times_seen, newNetwork.ssid.c_str());
        *oldest = newNetwork;
      }
    }

    xSemaphoreGive(mutex);
  } else {
    Serial.println("Failed to acquire mutex in WifiNetworkList");
  }
}

size_t WifiNetworkList::size() const {
  size_t currentSize = 0;
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    currentSize = networkList.size();
    xSemaphoreGive(mutex);
  }
  return currentSize;
}

std::vector<WifiNetwork> WifiNetworkList::getClonedList() const {
  std::vector<WifiNetwork> clonedList;
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    clonedList = networkList;
    xSemaphoreGive(mutex);
  }
  return clonedList;
}

void WifiNetworkList::addNetwork(const WifiNetwork& network) {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    networkList.push_back(network);
    Serial.printf("Added new WiFi network: %s\n", network.ssid.c_str());
    xSemaphoreGive(mutex);
  } else {
    Serial.println("Failed to acquire mutex in addNetwork");
  }
}

void WifiNetworkList::clear() {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    networkList.clear();
    Serial.println("WiFi network list cleared");
    xSemaphoreGive(mutex);
  } else {
    Serial.println("Failed to acquire mutex in clear");
  }
}
