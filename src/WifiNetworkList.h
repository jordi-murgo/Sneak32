#pragma once

#include <Arduino.h>
#include <vector>
#include <mutex>
#include "MACAddress.h"

struct WifiNetwork {
  String ssid;
  MacAddress address;
  int8_t rssi;
  uint8_t channel;
  String type;
  time_t last_seen;
  uint32_t times_seen;

  WifiNetwork(const String& s, const MacAddress& addr, int8_t r, uint8_t ch, const String& t, time_t seen, uint32_t times_seen = 1)
    : ssid(s), address(addr), rssi(r), channel(ch), type(t), last_seen(seen), times_seen(times_seen) {}
};

class WifiNetworkList {
public:
  explicit WifiNetworkList(size_t maxSize);
  ~WifiNetworkList();

  WifiNetworkList(const WifiNetworkList&) = delete;
  WifiNetworkList& operator=(const WifiNetworkList&) = delete;

  void updateOrAddNetwork(const String &ssid, const MacAddress &address, int8_t rssi, uint8_t channel, const String &type);
  size_t size() const;
  std::vector<WifiNetwork> getClonedList() const;
  void addNetwork(const WifiNetwork& network);
  void clear();
  void remove_irrelevant_networks();
  bool is_ssid_in_list(const String& ssid);

private:
  std::vector<WifiNetwork> networkList;
  size_t maxSize;
  mutable std::mutex networkMutex;
};
