#include "ArduinoJson.h"
#include "FirmwareInfo.h"
#include "MacAddress.h"

String formatVersion(long version)
{
    int major = version / 10000;
    int minor = (version / 100) % 100;
    int patch = version % 100;
    return String(major) + "." +
           String(minor) + "." +
           String(patch);
}

String getChipInfo()
{
    return String(ESP.getChipModel()) + 
           " (Cores: " + String(ESP.getChipCores()) + 
           ", Rev: " + String(ESP.getChipRevision()) + ")";
}

String getChipFeatures()
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    String features;
    if (chip_info.features & CHIP_FEATURE_EMB_FLASH) features += " EMB_FLASH";
    if (chip_info.features & CHIP_FEATURE_WIFI_BGN) features += " 2.4GHz_WIFI";
    if (chip_info.features & CHIP_FEATURE_BLE) features += " BLE";
    if (chip_info.features & CHIP_FEATURE_BT) features += " BT";
    if (chip_info.features & CHIP_FEATURE_IEEE802154) features += " IEEE802154";
    if (chip_info.features & CHIP_FEATURE_EMB_PSRAM) features += " EMB_PSRAM";

    return features.substring(1); // Remove initial space
}

JsonDocument getFirmwareInfoJson()
{
    JsonDocument doc;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);

    String mArch = String(CONFIG_SDK_TOOLPREFIX);
    if (mArch.endsWith("-"))
    {
        mArch = mArch.substring(0, mArch.length() - 1);
    }
    
    doc["arch"] = mArch;
    doc["chip"] = getChipInfo();
    doc["features"] = getChipFeatures();
    doc["cpu_mhz"] = ESP.getCpuFreqMHz();
    doc["flash_kb"] = ESP.getFlashChipSize() / 1024;
    doc["flash_mhz"] = ESP.getFlashChipSpeed() / 1000000;
    doc["ram_kb"] = ESP.getHeapSize() / 1024;
    doc["free_ram"] = ESP.getFreeHeap() / 1024;
    doc["min_ram"] = ESP.getMinFreeHeap() / 1024;
#ifdef CONFIG_SPIRAM
    doc["psram"] = ESP.getPsramSize() / 1024;
    doc["free_psram"] = ESP.getFreePsram() / 1024;
#endif
    doc["mac"] = MacAddress(mac).toString();

    doc["version"] = AUTO_VERSION;
    doc["built"] = AUTO_BUILD_TIME;
    doc["pio_ver"] = formatVersion(PLATFORMIO);
    doc["ard_ver"] = formatVersion(ARDUINO);
    doc["gcc_ver"] = String(__VERSION__);
    doc["cpp_ver"] = String(__cplusplus);
    doc["idf_ver"] = ESP.getSdkVersion();
    doc["board"] = ARDUINO_BOARD;
    doc["size_kb"] = ESP.getSketchSize() / 1024;
    doc["md5"] = ESP.getSketchMD5();
  
  return doc;
}

String getFirmwareInfoString()
{
    JsonDocument doc = getFirmwareInfoJson();
    return doc.as<String>();
}
