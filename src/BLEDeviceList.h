#pragma once

#include <Arduino.h>
#include <vector>
#include <mutex>
#include "MacAddress.h"
#include "DynamicPsramAllocator.h"

// Structure for found BLE devices
struct BLEFoundDevice
{
  MacAddress address;
  int8_t rssi;
  String name;
  bool isPublic;
  time_t last_seen;
  uint32_t times_seen;

  // Existing constructor
  BLEFoundDevice(const MacAddress &addr, int8_t r, const String &n, bool isPublic, time_t seen, uint32_t times_seen = 1)
      : address(addr), rssi(r), name(n), isPublic(isPublic), last_seen(seen), times_seen(times_seen) {}
};

class BLEDeviceList
{
public:
  explicit BLEDeviceList(size_t maxSize);
  ~BLEDeviceList();

  // Delete copy constructor and assignment operator
  BLEDeviceList(const BLEDeviceList &) = delete;
  BLEDeviceList &operator=(const BLEDeviceList &) = delete;

  // Public methods
  void updateOrAddDevice(const MacAddress &address, int8_t rssi, const String &name, bool isPublic);
  size_t size() const
  {
    std::lock_guard<std::mutex> lock(deviceMutex);
    return deviceList.size();
  }
  const std::vector<BLEFoundDevice, DynamicPsramAllocator<BLEFoundDevice>> &getList() const
  {
    std::lock_guard<std::mutex> lock(deviceMutex);
    return deviceList;
  }
  std::vector<BLEFoundDevice, DynamicPsramAllocator<BLEFoundDevice>> getClonedList() const
  {
    std::lock_guard<std::mutex> lock(deviceMutex);
    std::vector<BLEFoundDevice, DynamicPsramAllocator<BLEFoundDevice>> result;
    result.reserve(deviceList.size());
    for (const auto& device : deviceList) {
        result.push_back(device);
    }
    return result;
  }
  void addDevice(const BLEFoundDevice &device);
  void clear();
  void remove_irrelevant_devices();
  bool is_device_in_list(const MacAddress &address);

private:
  std::vector<BLEFoundDevice, DynamicPsramAllocator<BLEFoundDevice>> deviceList;
  size_t maxSize;

  // C++ Mutex
  mutable std::mutex deviceMutex;
};

extern BLEDeviceList bleDeviceList;
