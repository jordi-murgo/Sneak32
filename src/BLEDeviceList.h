#pragma once

#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "MACAddress.h"

// Estructura para dispositivos BLE encontrados
struct BLEFoundDevice {
  MacAddress address;
  int8_t rssi;
  String name;
  bool isPublic;
  time_t last_seen;
  uint32_t times_seen;

  // Constructor existente
  BLEFoundDevice(const MacAddress& addr, int8_t r, const String& n, bool isPublic, time_t seen, uint32_t times_seen = 1)
    : address(addr), rssi(r), name(n), isPublic(isPublic), last_seen(seen), times_seen(times_seen) {}
};

class BLEDeviceList {
public:
  explicit BLEDeviceList(size_t maxSize);
  ~BLEDeviceList();

  // Elimina el constructor de copia y el operador de asignación
  BLEDeviceList(const BLEDeviceList&) = delete;
  BLEDeviceList& operator=(const BLEDeviceList&) = delete;

  // Métodos públicos
  void updateOrAddDevice(const MacAddress &address, int rssi, const String &name, bool isPublic);
  size_t size() const;
  std::vector<BLEFoundDevice> getClonedList() const;
  void addDevice(const BLEFoundDevice& device);
  void clear();

private:
  std::vector<BLEFoundDevice> deviceList;
  size_t maxSize;

  // Handle del mutex de FreeRTOS
  SemaphoreHandle_t mutex;
};
