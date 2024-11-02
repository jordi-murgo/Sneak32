#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_system.h>

String formatVersion(long version);
String getChipInfo();
String getChipFeatures();

JsonDocument getHardwareInfoJson();
JsonDocument getSoftwareInfoJson();
JsonDocument getFirmwareInfoJson();

extern String getFirmwareInfoString();