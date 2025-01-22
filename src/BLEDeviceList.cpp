#include "BLEDeviceList.h"
#include <algorithm>
#include "AppPreferences.h"
#include <mutex>

extern time_t base_time;

// Constructor
BLEDeviceList::BLEDeviceList(size_t maxSize) : maxSize(maxSize)
{
  deviceList.reserve(maxSize);
}

// Destructor
BLEDeviceList::~BLEDeviceList() = default;

// Method to update or add a device
void BLEDeviceList::updateOrAddDevice(const MacAddress &address, int8_t rssi, const String &name, bool isPublic)
{
  std::lock_guard<std::mutex> lock(deviceMutex);

  auto it = std::find_if(deviceList.begin(), deviceList.end(),
                         [&address](const BLEFoundDevice &device)
                         {
                           return device.address == address;
                         });

  time_t now = millis() / 1000 + base_time;

  if (appPrefs.ignore_random_ble_addresses && !isPublic)
  {
    log_d("Ignoring non-public BLE address: %s", address.toString().c_str());
    return;
  }

  if (it != deviceList.end())
  {
    // Update existing device
    it->rssi = std::max(it->rssi, rssi);
    it->last_seen = now;
    it->times_seen++;
    if (name.length() > 0)
    {
      it->name = name;
    }
    it->isPublic = isPublic; // Update isPublic flag
    log_d("BLE device updated: %s", address.toString().c_str());
  }
  else
  {
    // Add new device
    if (deviceList.size() >= maxSize)
    {
      // Remove the oldest device
      auto oldest = std::min_element(deviceList.begin(), deviceList.end(),
                                     [](const BLEFoundDevice &a, const BLEFoundDevice &b)
                                     {
                                       return a.last_seen < b.last_seen;
                                     });
      *oldest = BLEFoundDevice(address, rssi, name, isPublic, now);
    }
    else
    {
      deviceList.emplace_back(address, rssi, name, isPublic, now);
    }
    log_i("New BLE device found: %s", address.toString().c_str());
  }

  // Remove devices with invalid MAC addresses
  deviceList.erase(
      std::remove_if(deviceList.begin(), deviceList.end(),
                     [](const BLEFoundDevice &device)
                     {
                       uint8_t invalid_mac[6] = {0, 0, 0, 0, 0, 0};
                       return memcmp(device.address.getBytes(), invalid_mac, 6) == 0;
                     }),
      deviceList.end());
}

void BLEDeviceList::addDevice(const BLEFoundDevice &device)
{
  std::lock_guard<std::mutex> lock(deviceMutex);
  deviceList.push_back(device);
  log_i("Added new BLE device: %s %s", device.address.toString().c_str(), device.name.c_str());
}

void BLEDeviceList::clear()
{
  std::lock_guard<std::mutex> lock(deviceMutex);
  deviceList.clear();
  log_i("BLE device list cleared");
}

void BLEDeviceList::remove_irrelevant_devices()
{
  std::lock_guard<std::mutex> lock(deviceMutex);

  if (deviceList.empty())
  {
    return;
  }

  size_t initial_size = deviceList.size();
  log_i("Removing irrelevant BLE devices. List size: %zu", initial_size);

  deviceList.erase(
      std::remove_if(deviceList.begin(), deviceList.end(),
                     [](const BLEFoundDevice &device)
                     {
                       log_d("Checking BLE device: %s, rssi: %d (minimal_rssi: %d)",
                             device.address.toString().c_str(), device.rssi, appPrefs.minimal_rssi);
                       return device.rssi < appPrefs.minimal_rssi;
                     }),
      deviceList.end());

  log_i("Removed %zu irrelevant BLE devices. New list size: %zu", initial_size - deviceList.size(), deviceList.size());
}

bool BLEDeviceList::is_device_in_list(const MacAddress &address)
{
  const auto &list = getList();
  auto it = std::find_if(list.begin(), list.end(),
                         [&address](const BLEFoundDevice &device)
                         {
                           return device.address == address;
                         });

  return it != list.end();
}
