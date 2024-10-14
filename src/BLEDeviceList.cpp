#include "BLEDeviceList.h"
#include <algorithm>

extern time_t base_time;

// Constructor
BLEDeviceList::BLEDeviceList(size_t maxSize) : maxSize(maxSize) {
  deviceList.reserve(maxSize);
  // Crea un mutex binari
  mutex = xSemaphoreCreateMutex();
  if (mutex == NULL) {
    // Maneig d'error en la creació del mutex
    Serial.println("Error creant el mutex");
  }
}

// Destructor
BLEDeviceList::~BLEDeviceList() {
  if (mutex != NULL) {
    vSemaphoreDelete(mutex);
  }
}

// Mètode per actualitzar o afegir un dispositiu
void BLEDeviceList::updateOrAddDevice(const MacAddress &address, int rssi, const String &name, bool isPublic) {

  // Adquireix el mutex abans d'accedir a la llista
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    auto it = std::find_if(deviceList.begin(), deviceList.end(),
                           [&address](const BLEFoundDevice &device) {
                             return device.address == address;
                           });

    time_t now = millis() / 1000 + base_time;

    if (it != deviceList.end()) {
      // Update an existing device
      it->rssi = rssi;
      it->last_seen = now;
      it->times_seen++;  // Increment times seen
      if (name.length() > 0) {
        it->name = name;
      }
    } else {
      BLEFoundDevice newDevice(address, rssi, name, isPublic, now);

      if (deviceList.size() < maxSize) {
        deviceList.emplace_back(newDevice);
        Serial.printf("Added new BLE device: %s %s\n", newDevice.address.toString().c_str(), newDevice.name.c_str());
      } else {
        // Replace the least recently seen device
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

    // Allibera el mutex
    xSemaphoreGive(mutex);
  } else {
    // Maneig d'error en adquirir el mutex
    Serial.println("No s'ha pogut adquirir el mutex");
  }
}

// Mètode per obtenir la mida de la llista
size_t BLEDeviceList::size() const {
  size_t currentSize = 0;

  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    currentSize = deviceList.size();
    xSemaphoreGive(mutex);
  } else {
    Serial.println("No s'ha pogut adquirir el mutex");
  }

  return currentSize;
}

// Mètode per obtenir una còpia clonada de la llista
std::vector<BLEFoundDevice> BLEDeviceList::getClonedList() const {
  std::vector<BLEFoundDevice> clonedList;

  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    clonedList = deviceList; // Crea una còpia de la llista
    xSemaphoreGive(mutex);
  } else {
    Serial.println("No s'ha pogut adquirir el mutex");
  }

  return clonedList;
}

void BLEDeviceList::addDevice(const BLEFoundDevice& device) {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      deviceList.push_back(device);
      Serial.printf("Added new BLE device: %s %s\n", device.address.toString().c_str(), device.name.c_str());
    xSemaphoreGive(mutex);
  } else {
    Serial.println("No se ha podido adquirir el mutex en addDevice");
  }
}

void BLEDeviceList::clear() {
  if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    deviceList.clear();
    Serial.println("BLE device list cleared");
    xSemaphoreGive(mutex);
  } else {
    Serial.println("No se ha podido adquirir el mutex en clear");
  }
}
