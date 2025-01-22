#include "WifiNetworkList.h"
#include <algorithm>
#include "AppPreferences.h"

extern time_t base_time;

WifiNetworkList::WifiNetworkList(size_t maxSize) : maxSize(maxSize)
{
  networkList.reserve(maxSize);
}

WifiNetworkList::~WifiNetworkList() = default;

void WifiNetworkList::updateOrAddNetwork(const String &ssid, const MacAddress &address, int8_t rssi, uint8_t channel, const String &type)
{
  std::lock_guard<std::mutex> lock(networkMutex);

  if (appPrefs.ignore_local_wifi_addresses && address.isLocallyAdministered())
  {
    log_d("Ignoring locally administered BSSID: %s", address.toString().c_str());
    return;
  }

  auto it = std::find_if(networkList.begin(), networkList.end(),
                         [&ssid, &address, &type](const WifiNetwork &network)
                         {
                           if (type == "probe")
                           {
                             // si es tracta d'un probe request busquem si existeix una xarxa previa amb el mateix ssid
                             return network.ssid == ssid;
                           }
                           else if (type == "beacon" || type == "assoc")
                           {
                             // si es tracta d'un beacon, busquem si existeix una xarxa previa amb el mateix ssid i address, o un probe amb el mateix ssid
                             return network.ssid == ssid && (network.address == address || network.type != "beacon");
                           }
                           else 
                           {
                             // data, deauth, auth, action, timing, other
                             // busquem si existeix una xarxa previa amb el mateix address
                             return network.address == address;
                           }

                           return false;
                         });

  time_t now = millis() / 1000 + base_time;

  if (it != networkList.end())
  {
    it->rssi = std::max(it->rssi, rssi); // El mejor de los rssi
    if (type == "beacon")
    {
      // Los beacons lo cambiamos todo
      it->channel = channel;
      it->type = type;
      it->address = address;
    }
    
    if (type == "assoc")
    {
      // No cambiamos el tipo, solo el canal y el address
      it->channel = channel;
      it->address = address;
    }
    // Para el resto solo actualizamos el último visto y el número de veces visto
    it->last_seen = now;
    it->times_seen++;
  }
  else
  {
    WifiNetwork newNetwork(ssid, address, rssi, channel, type, now, 1);

    if (networkList.size() < maxSize)
    {
      networkList.push_back(newNetwork);
      log_i("New network found: %s (%s)", ssid.c_str(), address.toString().c_str());
    }
    else
    {
      auto oldest = std::min_element(networkList.begin(), networkList.end(),
                                     [](const WifiNetwork &a, const WifiNetwork &b)
                                     {
                                       return a.last_seen < b.last_seen;
                                     });
      log_d("Replacing network: %s '%s' (seen %u times) with new network: %s '%s' (type: %s) \n",
                    oldest->address.toString().c_str(), oldest->ssid.c_str(), oldest->times_seen, 
                    newNetwork.address.toString().c_str(), newNetwork.ssid.c_str(), newNetwork.type.c_str());
      *oldest = newNetwork;
    }
  }
}

size_t WifiNetworkList::size() const
{
  return networkList.size();
}

std::vector<WifiNetwork, DynamicPsramAllocator<WifiNetwork>> WifiNetworkList::getClonedList() const
{
    std::vector<WifiNetwork, DynamicPsramAllocator<WifiNetwork>> result;
    result.reserve(networkList.size());
    for (const auto& network : networkList) {
        result.push_back(network);
    }
    return result;
}

void WifiNetworkList::addNetwork(const WifiNetwork &network)
{
  std::lock_guard<std::mutex> lock(networkMutex);
  networkList.push_back(network);
  log_i("Added new WiFi network: %s", network.ssid.c_str());
}

void WifiNetworkList::clear()
{
  std::lock_guard<std::mutex> lock(networkMutex);
  log_i("Clearing WiFi networks list");
  networkList.clear();
}

void WifiNetworkList::remove_irrelevant_networks()
{
  std::lock_guard<std::mutex> lock(networkMutex);

  if (networkList.empty())
  {
    return;
  }

  size_t initial_size = networkList.size();
  log_i("Removing irrelevant networks. List size: %zu", initial_size);

  uint32_t total_seens = 0;
  uint32_t total_beaconed_networks = 0;
  for (const auto &network : networkList)
  {
    if (network.type == "beacon")
    {
      total_seens += network.times_seen;
      total_beaconed_networks++;
    }
  }

  uint32_t min_seens = static_cast<uint32_t>(round(total_seens / static_cast<double>(total_beaconed_networks) * 3.0));

  networkList.erase(
      std::remove_if(networkList.begin(), networkList.end(),
                     [min_seens](const WifiNetwork &network)
                     {
                       log_d("Irrelevant network: %s, seen: %u (min_seens: %u), rssi: %d (minimal_rssi: %d)",
                             network.ssid.c_str(), network.times_seen, min_seens, network.rssi, appPrefs.minimal_rssi);
                       return (network.times_seen < min_seens && network.type == "beacon") || network.rssi < appPrefs.minimal_rssi;
                     }),
      networkList.end());

  log_i("Removed %zu irrelevant networks. New list size: %zu", initial_size - networkList.size(), networkList.size());
}

bool WifiNetworkList::is_ssid_in_list(const String &ssid)
{
  auto networkListCopy = getClonedList();

  auto it = std::find_if(networkListCopy.begin(), networkListCopy.end(),
                         [&ssid](const WifiNetwork &network)
                         {
                           return network.ssid == ssid;
                         });
  return it != networkListCopy.end();
}

