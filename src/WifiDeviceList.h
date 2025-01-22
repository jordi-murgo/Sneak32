#pragma once

#include <Arduino.h>
#include <vector>
#include <mutex>
#include "MacAddress.h"
#include "DynamicPsramAllocator.h"

struct WifiDevice
{
  MacAddress address;
  MacAddress bssid;
  int8_t rssi;
  uint8_t channel;
  time_t last_seen;
  uint32_t times_seen; // New field

  WifiDevice(const MacAddress &addr, const MacAddress &bssid, int8_t r, uint8_t ch, time_t seen, uint32_t times_seen = 1)
      : address(addr), bssid(bssid), rssi(r), channel(ch), last_seen(seen), times_seen(times_seen) {}
};

class WifiDeviceList
{
public:
  explicit WifiDeviceList(size_t maxSize);
  ~WifiDeviceList();

  WifiDeviceList(const WifiDeviceList &) = delete;
  WifiDeviceList &operator=(const WifiDeviceList &) = delete;

  void updateOrAddDevice(const MacAddress &address, const MacAddress &bssid, int8_t rssi, uint8_t channel);
  size_t size() const;
  std::vector<WifiDevice, DynamicPsramAllocator<WifiDevice>> getClonedList() const;
  void addDevice(const WifiDevice &device);
  void clear();
  void remove_irrelevant_stations();
  bool is_device_in_list(const MacAddress &address);

private:
  std::vector<WifiDevice, DynamicPsramAllocator<WifiDevice>> deviceList;
  size_t maxSize;
  mutable std::mutex deviceMutex;
};
