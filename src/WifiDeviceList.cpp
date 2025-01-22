#include "WifiDeviceList.h"
#include <algorithm>
#include "AppPreferences.h"

extern time_t base_time;

WifiDeviceList::WifiDeviceList(size_t maxSize) : maxSize(maxSize)
{
  deviceList.reserve(maxSize);
}

WifiDeviceList::~WifiDeviceList() = default;

void WifiDeviceList::updateOrAddDevice(const MacAddress &address, const MacAddress &bssid, int8_t rssi, uint8_t channel)
{
  std::lock_guard<std::mutex> lock(deviceMutex);

  if (appPrefs.ignore_local_wifi_addresses && address.isLocallyAdministered())
  {
    log_d("Ignoring locally administered MAC: %s", address.toString().c_str());
    return;
  }

  auto it = std::find_if(deviceList.begin(), deviceList.end(),
                         [&address](const WifiDevice &device)
                         {
                           return device.address == address;
                         });

  time_t now = millis() / 1000 + base_time;

  if (it != deviceList.end())
  {
    it->rssi = std::max(it->rssi, rssi);
    it->channel = channel;
    it->bssid = bssid;
    it->last_seen = now;
    it->times_seen++;
    log_d("Device updated: %s", address.toString().c_str());
  }
  else
  {
    WifiDevice newDevice(address, bssid, rssi, channel, now, 1);

    if (deviceList.size() < maxSize)
    {
      deviceList.push_back(newDevice);
      log_i("New device found: %s", address.toString().c_str());
    }
    else
    {
      auto oldest = std::min_element(deviceList.begin(), deviceList.end(),
                                     [](const WifiDevice &a, const WifiDevice &b)
                                     {
                                       return a.last_seen < b.last_seen;
                                     });
      log_d("Replacing device: %s (seen %u times) with new device: %s",
             oldest->address.toString().c_str(), oldest->times_seen,
             newDevice.address.toString().c_str());
      *oldest = newDevice;
    }
  }
}

size_t WifiDeviceList::size() const
{
  return deviceList.size();
}

std::vector<WifiDevice, DynamicPsramAllocator<WifiDevice>> WifiDeviceList::getClonedList() const
{
    std::vector<WifiDevice, DynamicPsramAllocator<WifiDevice>> result;
    result.reserve(deviceList.size());
    for (const auto& device : deviceList) {
        result.push_back(device);
    }
    return result;
}

void WifiDeviceList::addDevice(const WifiDevice &device)
{
  std::lock_guard<std::mutex> lock(deviceMutex);
  deviceList.push_back(device);
  log_i("Added new WiFi device: %s", device.address.toString().c_str());
}

void WifiDeviceList::clear()
{
  std::lock_guard<std::mutex> lock(deviceMutex);
  log_i("WiFi device list cleared");
  deviceList.clear();
}

bool WifiDeviceList::is_device_in_list(const MacAddress &address)
{
  auto deviceListCopy = getClonedList();
  auto it = std::find_if(deviceListCopy.begin(), deviceListCopy.end(),
                         [&address](const WifiDevice &device)
                         {
                           return device.address == address;
                         });
  return it != deviceListCopy.end();
}
