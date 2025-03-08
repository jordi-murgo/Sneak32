/**
 * @file BLEDeviceWrapper.cpp
 * @brief Implementation of BLEDeviceWrapper class
 */

#include "BLEDeviceWrapper.h"

// Initialize static members
bool BLEDeviceWrapper::initialized = false;
std::string BLEDeviceWrapper::initializedName = "";
