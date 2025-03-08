/**
 * @file main.cpp
 * @author Jordi Murgo (jordi.murgo@gmail.com)
 * @brief Main file for the WiFi monitoring and analysis project
 * @version 0.1
 * @date 2024-07-28
 *
 * @copyright Copyright (c) 2024 Jordi Murgo
 *
 * @license MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <WiFi.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <nvs_flash.h>
#include <esp_wifi_types.h>
#include <esp_log.h>
#include <esp_core_dump.h>  // Para la funcionalidad de core dump
#include <esp_task_wdt.h>   // Para la configuración del watchdog
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <time.h>
#include <string>
#include <Preferences.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "BLEDeviceWrapper.h" // Added for BLEDeviceWrapper access

#include "LedManager.h"

#include "BLE.h"
#include "Preferences.h"
#include "WifiScan.h"

// These definitions are now in TaskManager.h
// #define MAX_STATIONS 255
// #define MAX_SSIDS 200
// #define MAX_BLE_DEVICES 100

#include "BLEDeviceList.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "WifiScan.h"
#include "WifiDetect.h"
#include "BLEDetect.h"
#include "BLEScan.h"
#include "AppPreferences.h"
#include "FlashStorage.h"
#include "BLEAdvertisingManager.h"
#include "FirmwareInfo.h"
#include "BLEStatusUpdater.h"
#include "TaskManager.h"

// Define the boot button pin (adjust if necessary)
#define BOOT_BUTTON_PIN 0

/**
 * @brief BLEDeviceList is a list of BLE devices.
 *
 * This class is used to store and manage a list of BLE devices.
 * It uses a mutex to ensure that the list is accessed in a thread-safe manner.
 */
BLEDeviceList bleDeviceList(MAX_BLE_DEVICES);

/**
 * @brief WifiDeviceList is a list of WiFi devices.
 *
 * This class is used to store and manage a list of WiFi devices.
 * It uses a mutex to ensure that the list is accessed in a thread-safe manner.
 */
WifiDeviceList stationsList(MAX_STATIONS);

/**
 * @brief WifiNetworkList is a list of WiFi networks.
 *
 * This class is used to store and manage a list of WiFi networks.
 * It uses a mutex to ensure that the list is accessed in a thread-safe manner.
 */
WifiNetworkList ssidList(MAX_SSIDS);

time_t my_universal_time = 0;

extern bool deviceConnected;

void printSSIDAndBLELists();
void printTaskStats();
void printMemoryStats();
void printSystemInfo();

#ifdef PIN_NEOPIXEL
LedManager ledManager(PIN_NEOPIXEL, 1);
#else
LedManager ledManager(LED_BUILTIN);
#endif

// Add these global variables at the beginning of the file
unsigned long lastPrintTime = 0;
unsigned long lastDeviceListPrintTime = 0;
const unsigned long printInterval = 30000; // 30 seconds in milliseconds

// Base time for the last_seen field in the lists
time_t base_time = 0;

// Function to update base_time from a list
template <typename T>
void updateBaseTime(const std::vector<T, DynamicPsramAllocator<T>> &list)
{
  for (const auto &item : list)
  {
    base_time = std::max(base_time, item.last_seen);
  }
}

void printTaskStats() {
    // Get the number of tasks
    UBaseType_t uxTaskCount = uxTaskGetNumberOfTasks();
    
    // Print task statistics header
    log_i("-------------------------------------");
    log_i("FreeRTOS Task List (similar to ps):");
    log_i("-------------------------------------");
    log_i("Task Name          State Prio   Stack    ");
    log_i("------------------ ----- ------ --------");

    // Get information about all tasks using vTaskList if available
    #if defined(CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID) || defined(CONFIG_FREERTOS_USE_TRACE_FACILITY)
        // These configurations might enable vTaskList in some ESP-IDF versions
        // But we still can't use it directly because it's still not exposed in Arduino-ESP32
        // This is just a placeholder for future ESP-IDF versions
        log_i("Cannot access full task list in current ESP-IDF/Arduino version");
    #endif

    // Since we can't get the full task list, we'll at least show the current task
    // and provide some system information
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    if (currentTask) {
        const char* taskName = pcTaskGetName(currentTask);
        UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        UBaseType_t taskPriority = uxTaskPriorityGet(currentTask);
        
        // Calculate stack usage (estimate)
        UBaseType_t estimatedStackSize = 4096; // Default
        if (strstr(taskName, "WiFiScan") != NULL) estimatedStackSize = STACK_SIZE_WIFI_SCAN;
        else if (strstr(taskName, "BLEScan") != NULL) estimatedStackSize = STACK_SIZE_BLE_SCAN;
        else if (strstr(taskName, "DeviceMgmt") != NULL) estimatedStackSize = STACK_SIZE_DEVICE_MGMT;
        else if (strstr(taskName, "DataProcess") != NULL) estimatedStackSize = STACK_SIZE_DATA_PROCESS;
        else if (strstr(taskName, "BLEService") != NULL) estimatedStackSize = STACK_SIZE_BLE_SERVICE;
        else if (strstr(taskName, "Status") != NULL) estimatedStackSize = STACK_SIZE_STATUS;
        
        int stackUsagePercent = 0;
        if (stackHighWaterMark > 0 && estimatedStackSize > 0) {
            stackUsagePercent = 100 - ((stackHighWaterMark * 100) / estimatedStackSize);
            if (stackUsagePercent < 0) stackUsagePercent = 0;
            if (stackUsagePercent > 100) stackUsagePercent = 100;
        }
        
        // Print current task info
        log_i("%-18s %c %2d     %4lu(%2d%%)", 
             taskName,
             'R', // Running
             taskPriority,
             stackHighWaterMark * sizeof(StackType_t),
             stackUsagePercent);
    }
    
    // Try to get task handles from TaskManager for key tasks
    TaskManager& taskManager = TaskManager::getInstance();
    
    // List known tasks from TaskManager if available
    // Note: We need to add accessors in TaskManager to expose these
    // This is a sketch of how it could work with the right accessors
    log_i("%-18s %s %2d     %4s", "WiFiScan", "-", TASK_PRIORITY_WIFI_SCAN, "-");
    log_i("%-18s %s %2d     %4s", "BLEScan", "-", TASK_PRIORITY_BLE_SCAN, "-");
    log_i("%-18s %s %2d     %4s", "DeviceMgmt", "-", TASK_PRIORITY_DEVICE_MGMT, "-");
    log_i("%-18s %s %2d     %4s", "BLEService", "-", TASK_PRIORITY_BLE_SERVICE, "-");
    log_i("%-18s %s %2d     %4s", "Status", "-", TASK_PRIORITY_STATUS, "-");
    
    // Print legend
    log_i("-------------------------------------");
    log_i("Status: R=running B=blocked S=suspended r=ready");
    log_i("Prio: task priority");
    log_i("Stack: free bytes (percent used)");
    log_i("-------------------------------------");
    
    // Print scheduler state
    log_i("Task Scheduler is %s", 
          xTaskGetSchedulerState() == taskSCHEDULER_RUNNING ? "RUNNING" : 
         (xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ? "SUSPENDED" : "NOT STARTED"));
    
    // Print general system info
    log_i("Total tasks: %u", uxTaskCount);
    log_i("Minimum free stack (this task): %u bytes", uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
    
    log_i("");
}

void printMemoryStats() {
    log_i("Memory Statistics:");
    log_i("Total PSRAM: %d bytes", ESP.getPsramSize());
    log_i("Free PSRAM: %d bytes", ESP.getFreePsram());
    log_i("Total heap: %d bytes", ESP.getHeapSize());
    log_i("Free heap: %d bytes", ESP.getFreeHeap());
    log_w("Minimum free heap: %d bytes", ESP.getMinFreeHeap());
    log_i("");
}

void printSystemInfo() {
    // Ensure this message is visible in the console
    Serial.println("\n\n======== SYSTEM INFORMATION (PS COMMAND) ========");
    log_i("======== SYSTEM INFORMATION (ps) ========");
    
    // CPU information
    log_i("CPU Information:");
    log_i("  Cores: %d (%s)", ESP.getChipCores(), ESP.getChipModel());
    log_i("  Frequency: %d MHz", ESP.getCpuFreqMHz());
    log_i("  Chip model: %s", ESP.getChipModel());
    log_i("  Chip revision: %d", ESP.getChipRevision());
    log_i("  SDK version: %s", ESP.getSdkVersion());
    log_i("");
    
    // Print memory information
    printMemoryStats();
    
    // Add heap info from ESP-IDF
    size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t minFreeInternal = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    size_t minFreePsram = psramFound() ? heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM) : 0;
    
    log_i("Heap Information (ESP-IDF):");
    log_i("  Internal heap: %d bytes free (%d min free)", freeInternal, minFreeInternal);
    if (psramFound()) {
        log_i("  PSRAM: %d bytes free (%d min free)", freePsram, minFreePsram);
    }
    log_i("");
    
    // Print task statistics (similar to "ps")
    printTaskStats();
    
    // Runtime information
    log_i("Runtime Information:");
    log_i("  Uptime: %lu seconds", millis() / 1000);
    log_i("  Free sketch space: %lu bytes", ESP.getFreeSketchSpace());
    log_i("  System tick rate: %d Hz", configTICK_RATE_HZ);
    
    // Get partition info
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (it) {
        log_i("Flash Partition Information:");
        do {
            const esp_partition_t* part = esp_partition_get(it);
            log_i("  %s: %s [0x%X - 0x%X] (%d KB)", 
                 part->label, part->type == ESP_PARTITION_TYPE_APP ? "App" : "Data", 
                 part->address, part->address + part->size, part->size / 1024);
        } while ((it = esp_partition_next(it)) != NULL);
        esp_partition_iterator_release(it);
    }
    log_i("");
    
    // Print network information
    log_i("Network Statistics:");
    log_i("  WiFi networks: %d", ssidList.size());
    log_i("  WiFi devices: %d", stationsList.size()); 
    log_i("  BLE devices: %d", bleDeviceList.size());
    log_i("======================================");
    log_i("");
}


/**
 * @brief Prints the list of detected SSIDs and BLE devices.
 *
 * This function formats and prints a table of detected WiFi networks and BLE devices
 * to the serial console, including details such as RSSI, channel, and last seen time.
 */
void printSSIDAndBLELists()
{
  String listString;

  // SSID List
  listString = "\n---------------------------------------------------------------------------------\n";
  listString += "SSID                             | RSSI | Channel | Type   | Times | Last seen\n";
  listString += "---------------------------------|------|---------|--------|-------|------------\n";
  for (const auto &network : ssidList.getClonedList())
  {
    char line[150];
    snprintf(line, sizeof(line), "%-32s | %4d | %7d | %-6s | %5d | %d\n",
             network.ssid.c_str(), network.rssi, network.channel, network.type.c_str(),
             network.times_seen, network.last_seen);
    listString += line;
  }
  listString += "---------------------------------|------|---------|--------|-------|------------\n";
  listString += "Total SSIDs: " + String(ssidList.size()) + "\n";
  listString += "--------------------------------------------------------------------------------\n";

  // BLE Device List
  listString += "\n----------------------------------------------------------------------\n";
  listString += "Name                             | Public | RSSI | Times | Last seen\n";
  listString += "---------------------------------|--------|------|-------|------------\n";

  auto devices = bleDeviceList.getClonedList();
  for (const auto &device : devices)
  {
    char isPublicStr[6];
    snprintf(isPublicStr, sizeof(isPublicStr), "%s", device.isPublic ? "true" : "false");
    char line[150];
    if (device.name.length())
    {
      snprintf(line, sizeof(line), "%-32s | %-6s | %4d | %5d | %d\n",
               device.name.substring(0, 32).c_str(), isPublicStr, device.rssi,
               device.times_seen, device.last_seen);
    }
    else
    {
      // Unnamed (F8:77:B8:1F:B9:7C)
      snprintf(line, sizeof(line), "Unnamed (%17s)      | %-6s | %4d | %5d | %d\n",
               device.address.toString().c_str(), isPublicStr, device.rssi,
               device.times_seen, device.last_seen);
    }
    listString += line;
  }

  listString += "---------------------------------|--------|------|-------|------------\n";
  listString += "Total BLE devices: " + String(bleDeviceList.size()) + "\n";
  listString += "----------------------------------------------------------------------\n";

  log_i("\n%s", listString.c_str());
  
  // Print system information with ps-like output 
  printSystemInfo();
  
  // Update last device list print time
  lastDeviceListPrintTime = millis();
}

void firmwareInfo()
{
  log_d("----------------------------------------------------------------------");
  log_d("%s", getFirmwareInfoString().c_str());
  log_d("----------------------------------------------------------------------\n");
}

void checkAndRestartAdvertising()
{
  static unsigned long lastAdvertisingRestart = 0;
  const unsigned long advertisingRestartInterval = 60 * 60 * 1000; // 1 hora en milisegundos

  if (millis() - lastAdvertisingRestart > advertisingRestartInterval)
  {
    log_i("Restarting BLE advertising");
    BLEDevice::stopAdvertising();
    delay(100);
    BLEDevice::startAdvertising();
    lastAdvertisingRestart = millis();
  }
}

/**
 * @brief Setup function that runs once when the device starts.
 *
 * This function initializes serial communication, loads preferences,
 * sets up WiFi in promiscuous mode, and initializes BLE.
 */
void setup()
{
  // Initialize Serial communication
  Serial.begin(115200);
  delay(2000); // Give time for serial to initialize
  
  // Inicializar Core Dump
  log_i("Initializing Core Dump...");
  esp_err_t err = ESP_OK;
  
#ifdef USE_CORE_DUMP
  // Configurar core dump si está habilitado
  #if !defined(CONFIG_ESP_COREDUMP_ENABLE)
    log_i("Habilitando core dump manualmente (no configurado en SDK)");
    // Intentamos configurar manualmente - esto podría no funcionar
    // si el SDK no se compiló con soporte para core dump
  #endif

  // Verificar si ya existe un core dump
  size_t core_addr = 0;
  size_t core_size = 0;
  err = esp_core_dump_image_get(&core_addr, &core_size);
  if (err == ESP_OK && core_size > 0) {
    log_e("¡Se encontró un core dump previo! Dirección: 0x%x, Tamaño: %d bytes", core_addr, core_size);
    log_e("Recuerda extraerlo con 'espcoredump.py info_corefile'");
    
    // Breve parpadeo de LED rojo para indicar core dump detectado
    for (int i=0; i<5; i++) {
      ledManager.setPixelColor(0, LedManager::COLOR_RED);
      ledManager.show();
      delay(200);
      ledManager.setPixelColor(0, 0);
      ledManager.show();
      delay(200);
    }
  } else if (err != ESP_OK) {
    log_w("Error al verificar core dump: %d", err);
  } else {
    log_i("No se encontró core dump previo");
  }
#else
  log_w("Core Dump no habilitado en la configuración");
#endif

  // Configuración del watchdog
#ifdef SNEAK_TASK_WDT_TIMEOUT_S
  // Aplicamos nuestra configuración personalizada del watchdog
  log_i("Configurando watchdog a %d segundos", SNEAK_TASK_WDT_TIMEOUT_S);
  
  // Primero desactivamos el watchdog actual
  esp_task_wdt_deinit();
  
  // Luego lo inicializamos con el nuevo timeout
  // Los parámetros son: tiempo de timeout en segundos, y si debe causar panic
  ESP_ERROR_CHECK(esp_task_wdt_init(SNEAK_TASK_WDT_TIMEOUT_S, true));
  
  // Añadimos el core actual al watchdog
  ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
#endif
  
  // [RESTORED] WiFi functionality
  log_i("WiFi functionality restored for scanning");

  log_i("Starting...");

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    log_w("Erasing NVS flash...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Check for PSRAM and inform user
  if (psramFound()) {
    log_i("PSRAM found: %d bytes, using for data storage", ESP.getPsramSize());
  } else {
    log_w("PSRAM not found! Using regular memory for storage (reduced capacity)");
    // Reducir los límites máximos de dispositivos para evitar problemas de memoria
    // Estos valores se ajustarán en el DynamicPsramAllocator
  }

  // Load preferences
  loadAppPreferences();

  // Initialize BLE controller and services
  if (appPrefs.mode_ble) {
    log_i("Setting up BLE (Bluetooth Low Energy)");
    
    // [ADDED] Información detallada sobre estado de BLE
    if (BLEDevice::getInitialized()) {
      log_i("BLE ya inicializado previamente");
    }
    
    log_i("Intentando inicializar BLE con nombre: %s", appPrefs.device_name);
    
    if (!setupBLE()) {
      log_e("BLE initialization failed, disabling BLE functionality");
      appPrefs.mode_ble = false;
      // Update preferences to match this runtime setting
      saveAppPreferences();
    } else {
      log_i("BLE initialized successfully");
      
      // [ADDED] Configuración de potencia de transmisión
      esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
      if (err == ESP_OK) {
        log_i("BLE TX power set to maximum");
      } else {
        log_e("Error setting BLE TX power: %d", err);
      }
      
      // [ADDED] Verificar si el controlador BLE está realmente activo
      if (!btStarted()) {
        log_e("BT controller no iniciado, intentando iniciar manualmente");
        if (btStart()) {
          log_i("BT controller iniciado manualmente con éxito");
        } else {
          log_e("Error al iniciar manualmente BT controller");
        }
      } else {
        log_i("BT controller ya está activo");
      }
    }
  }

  // [DEBUG] WiFi disabled for testing
  log_i("[DEBUG] WiFi functionality disabled for testing");

  // Start operation mode
  switch (appPrefs.operation_mode)
  {
  case OPERATION_MODE_SCAN:
    log_i("Starting in SCAN mode");
    // [RESTORED] WiFi scan
    WifiScanner.setup();
    BLEScanner.setup();
    break;
  case OPERATION_MODE_DETECTION:
    log_i("Starting in DETECTION mode");
    // [RESTORED] WiFi detection
    WifiDetector.setup();
    BLEDetector.setup();
    break;
  default:
    log_i("Starting in OFF mode");
    break;
  }

  log_i("Setup complete");

  // Mostrar información sobre la memoria disponible
  if (psramFound()) {
    size_t psram_size = ESP.getPsramSize();
    size_t free_psram = ESP.getFreePsram();
    log_i("PSRAM disponible: %d bytes de %d bytes", free_psram, psram_size);
  }
  
  // Mostrar información del heap principal
  size_t free_heap = ESP.getFreeHeap();
  size_t total_heap = ESP.getHeapSize();
  log_i("Heap disponible: %d bytes de %d bytes", free_heap, total_heap);

  // Encuentra la partición NVS
  const esp_partition_t *nvs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);

  if (nvs_partition != NULL)
  {
    log_d("NVS partition found. Size: %d bytes, Offset: %d", 
          nvs_partition->size, nvs_partition->address);
  } else {
    log_e("NVS partition not found!");
  }

  ledManager.begin();
  ledManager.setPixelColor(0, LedManager::COLOR_GREEN);
  ledManager.show();

  delay(1000); // Espera 1 segundo antes de comenzar

  firmwareInfo();

  // Set CPU frequency
  if (appPrefs.cpu_speed != getCpuFrequencyMhz())
  {
    log_i("Setting CPU frequency to %u MHz", appPrefs.cpu_speed);
    setCpuFrequencyMhz(appPrefs.cpu_speed);
  }

  try
  {
    FlashStorage::loadAll();
  }
  catch (const std::exception &e)
  {
    log_e("Error loading from flash storage: %s", e.what());

    // Handle the error (e.g., clear the lists, use default values, etc.)
    stationsList.clear();
    ssidList.clear();
    bleDeviceList.clear();
  }

  // Update base_time from all lists in one go
  updateBaseTime(ssidList.getClonedList());
  updateBaseTime(bleDeviceList.getClonedList());
  updateBaseTime(stationsList.getClonedList());
  base_time++;
  log_i("Base time set to: %ld", base_time);

  // Setup BLE Core (Advertising and Service Characteristics) if not already initialized
  if (appPrefs.mode_ble && !BLEDeviceWrapper::isInitialized()) {
    log_i("Setting up BLE Core after flash load");
    
    // Add a delay before BLE initialization
    delay(500);
    
    // Try to initialize BLE with timeout protection
    unsigned long startTime = millis();
    const unsigned long BLE_INIT_TIMEOUT = 10000; // 10 seconds timeout
    
    bool bleInitSuccess = false;
    
    // Create a separate task for BLE initialization to avoid blocking the main task
    TaskHandle_t bleInitTaskHandle = NULL;
    xTaskCreatePinnedToCore(
      [](void* parameter) {
        bool* success = (bool*)parameter;
        *success = setupBLE();
        vTaskDelete(NULL);
      },
      "BLEInit",
      8192,
      &bleInitSuccess,
      1,
      &bleInitTaskHandle,
      0 // Run on Core 0
    );
    
    // Wait for BLE initialization with timeout
    while (!bleInitSuccess && (millis() - startTime < BLE_INIT_TIMEOUT)) {
      delay(100);
      
      // Check if the task is still running
      if (bleInitTaskHandle == NULL || eTaskGetState(bleInitTaskHandle) == eDeleted) {
        break;
      }
    }
    
    // If timeout occurred, delete the task
    if (bleInitTaskHandle != NULL && eTaskGetState(bleInitTaskHandle) != eDeleted) {
      vTaskDelete(bleInitTaskHandle);
      log_e("BLE initialization timed out");
    }
    
    if (!bleInitSuccess) {
      log_e("BLE initialization failed after flash load");
      appPrefs.mode_ble = false;
      saveAppPreferences();
      
      // Visual indication of BLE failure
      ledManager.setPixelColor(0, LedManager::COLOR_RED);
      ledManager.show();
      delay(2000);
      ledManager.setPixelColor(0, LedManager::COLOR_OFF);
      ledManager.show();
    }
  }

  if (appPrefs.operation_mode == OPERATION_MODE_DETECTION)
  {
    // [RESTORED] WiFi detection
    WifiDetector.setup();
    BLEDetector.setup();
  }
  else if (appPrefs.operation_mode == OPERATION_MODE_SCAN)
  {
    // [RESTORED] WiFi scan
    WifiScanner.setup(); 
    BLEScanner.setup();
  }

  // Add a delay after setting up detectors
  delay(1000);

  ledManager.setPixelColor(0, LedManager::COLOR_OFF);
  ledManager.show();

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
}

/**
 * @brief Scan mode loop.
 *
 * This function handles the scan mode loop, which is used to scan for WiFi devices.
 * It sets the WiFi channel, performs a BLE scan, and checks for transmission timeout.
 */
void scan_mode_loop()
{
  static unsigned long lastSaved = 0;
  static int currentChannel = 0;
  static unsigned long lastAdvertisingAttempt = 0;
  static int advertisingFailCount = 0;
  static bool advertisingEnabled = true;
  static unsigned long advertisingDisableTime = 0;
  static bool firstScanCycle = true;

  currentChannel = (currentChannel % 14) + 1;
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

  // [RESTORED] Activar escaneo WiFi
  // Note: WiFi scanning is always active in scan mode, regardless of BLE settings
  WifiScanner.setChannel(currentChannel);

  // Mostrar heap lliure específic a SRAM (INTERNAL)
  size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  // Mostrar heap lliure a PSRAM (SPIRAM)
  size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  log_d(">> Time: %lu, WiFi Ch: %2d, SSIDs: %zu, Stations: %zu, BLE: %zu, Heap: %d, Heap PSRAM: %d",
        millis() / 1000, currentChannel, ssidList.size(), stationsList.size(), 
        bleDeviceList.size(), freeInternal, freePsram);

  delay(appPrefs.wifi_channel_dwell_time);

  checkTransmissionTimeout();

  // Skip BLE advertising on first scan cycle to allow system to stabilize
  if (firstScanCycle && currentChannel == 14) {
    firstScanCycle = false;
    log_i("First scan cycle completed, BLE advertising will be enabled on next cycle");
    return;
  }
  
  // Programación defensiva: Verificar si BLE está habilitado y realmente iniciarlo
  // Solo en el canal 1 para hacer el intento de forma regular pero no excesiva
  if (appPrefs.mode_ble && currentChannel == 1) {
    log_i("============= BLE SCAN DEBUG INFO (CANAL 1) =============");
    log_i("Estado de BLE en ciclo canal 1:");
    log_i("* BT controlador activo: %s", btStarted() ? "SÍ" : "NO");
    log_i("* BLEDevice inicializado: %s", BLEDevice::getInitialized() ? "SÍ" : "NO"); 
    log_i("* Dispositivos BLE detectados: %d", bleDeviceList.size());
    log_i("* Heap libre: %d bytes", freeInternal);
    
    // NUNCA detenga un controlador BT que ya funciona
    if (!btStarted()) {
      // El controlador BT no está activo, intentar iniciarlo
      log_i("⚠️ BT controller no iniciado, iniciando ahora...");
      if (btStart()) {
        log_i("✅ BT controller iniciado exitosamente");
      } else {
        log_e("❌ ERROR CRÍTICO: No se pudo iniciar el controlador BT");
        // Si fallamos aquí, no tiene sentido continuar
        log_i("=========================================================");
        return;
      }
    } else {
      log_i("✅ BT controller ya está activo - MANTENIENDO EL ESTADO ACTUAL");
    }
    
    // Si llegamos aquí, el controlador BT debería estar activo
    
    // Paso 1: Configuración agresiva
    log_i("1️⃣ Configurando parámetros BLE");
    appPrefs.minimal_rssi = -100;  // Máxima sensibilidad
    appPrefs.passive_scan = false; // Escaneo activo 
    appPrefs.ble_scan_duration = 30; // 30 segundos de escaneo
    
    // Paso 2: Configurar potencia de transmisión al máximo
    log_i("2️⃣ Configurando potencia BT al máximo");
    esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    if (err != ESP_OK) {
      log_e("⚠️ Error configurando potencia BT: %d", err);
    }
    
    // Paso 3: Verificar inicialización de BLE
    bool needBleInit = false;
    if (!BLEDevice::getInitialized()) {
      log_i("⚠️ BLEDevice no inicializado, inicializando...");
      needBleInit = true;
    } else {
      log_i("✅ BLEDevice ya inicializado - MANTENIENDO EL ESTADO ACTUAL");
    }
    
    // Solo inicializar si es necesario
    if (needBleInit) {
      if (BLEDeviceWrapper::init(appPrefs.device_name)) {
        log_i("✅ BLE inicializado satisfactoriamente");
      } else {
        log_e("❌ Error crítico en inicialización BLE");
        // Si fallamos aquí, no tiene sentido continuar
        log_i("=========================================================");
        return;
      }
    }
    
    // Paso 4: Configurar e iniciar escaneo
    log_i("3️⃣ Configurando e iniciando escáner BLE");
    BLEScanner.setup();
    
    if (BLEScanner.start()) {
      log_i("✅ Escaneo BLE iniciado correctamente");
    } else {
      log_e("❌ Error al iniciar escaneo BLE");
    }
    
    // Final del bloque de debug info
    log_i("=========================================================");
  }

  // Gestión robusta del advertising BLE - only attempt on channel 1 to reduce frequency
  if (!deviceConnected && advertisingEnabled && currentChannel == 1)
  {
    // Solo intentar iniciar el advertising cada 30 segundos para evitar reinicios continuos
    if (millis() - lastAdvertisingAttempt >= 30000)
    {
      lastAdvertisingAttempt = millis();
      
      // Check heap before attempting BLE operations
      if (freeInternal < 40000) {
        log_w("Low heap memory (%d bytes), skipping BLE advertising attempt", freeInternal);
        return;
      }
      
      // Perform a memory cleanup before BLE operations
      log_i("Performing memory cleanup before BLE advertising");
      ESP.getMinFreeHeap(); // Force memory defragmentation
      
      // Add a delay after memory cleanup
      delay(100);
      
      bool advertisingConfigured = false;
      
      // Usar un bloque try-catch para evitar que los errores de BLE afecten al resto del sistema
      try
      {
        if (appPrefs.stealth_mode)
        {
          if (digitalRead(BOOT_BUTTON_PIN) == LOW)
          {
            log_i(">>> Boot button pressed, disabling stealth mode");
            BLEAdvertisingManager::configureNormalMode();
            advertisingConfigured = true;
          }
          else
          {
            BLEAdvertisingManager::configureStealthMode();
            advertisingConfigured = true;
          }
        }
        else
        {
          BLEAdvertisingManager::configureNormalMode();
          advertisingConfigured = true;
        }
        
        // Solo intentamos iniciar el advertising si la configuración fue exitosa
        if (advertisingConfigured) {
          delay(200); // Pequeña pausa para asegurar que la configuración se complete
          
          // Usar un bloque try-catch específico para el inicio del advertising
          try {
            if (!BLEAdvertisingManager::startAdvertising()) {
              log_e("Failed to start advertising");
              advertisingFailCount++;
              
              // Si hay demasiados fallos consecutivos, desactivar el advertising temporalmente
              if (advertisingFailCount >= 3) {
                log_w("Too many advertising failures, disabling advertising for 120 seconds");
                advertisingEnabled = false;
                advertisingDisableTime = millis();
              }
            } else {
              // Reiniciar el contador de fallos si el advertising se inicia correctamente
              advertisingFailCount = 0;
              
              // Add a longer delay after successful advertising start
              delay(300);
            }
          } catch (...) {
            log_e("Exception during advertising start");
            advertisingFailCount++;
          }
        }
      }
      catch (...)
      {
        log_e("Exception during BLE advertising configuration");
        advertisingFailCount++;
      }
    }
  }
  else if (!advertisingEnabled && (millis() - advertisingDisableTime >= 120000)) {
    // Re-enable advertising after timeout (increased to 2 minutes)
    advertisingEnabled = true;
    advertisingFailCount = 0;
    log_i("Re-enabling advertising after timeout");
  }

  if (currentChannel == 14)
  {
    printSSIDAndBLELists();
  }

  // Save all data to flash storage every autosave_interval minutes
  if (millis() - lastSaved >= appPrefs.autosave_interval * 60 * 1000)
  {
    log_i("Saving all data to flash storage");
    try
    {
      FlashStorage::saveAll();
      lastSaved = millis();
      log_i("Data saved successfully");
    }
    catch (const std::exception &e)
    {
      log_e("Error saving to flash storage: %s", e.what());
    }
  }
}

/**
 * @brief Monitor mode loop.
 *
 * This function handles the monitor mode loop, which is used to monitor WiFi devices.
 * It sets the WiFi channel 1, performs a BLE scan, and checks for transmission timeout.
 */

void detection_mode_loop()
{
  static auto clonedList = ssidList.getClonedList();
  static size_t currentSSIDIndex = 0;

  if (appPrefs.passive_scan)
  {
    WifiDetector.setChannel(1);
    log_i(">> Passive WiFi scan");
  }
  else
  {
    // Add bounds checking for currentSSIDIndex
    if (currentSSIDIndex >= clonedList.size())
    {
      currentSSIDIndex = 0;
    }

    const auto &currentNetwork = clonedList[currentSSIDIndex];
    // Configure ESP32 to broadcast the selected SSID
    // WifiDetector.setupAP(currentNetwork.ssid.c_str(), nullptr, 1);
    if (&currentNetwork && currentNetwork.ssid && currentNetwork.ssid.length() > 0)
    {
      WifiDetector.setupAP(currentNetwork.ssid.c_str(), nullptr, 1);
      log_i(">> Detection Mode (%02d/%02d) >> Alarm: %d, Broadcasting SSID: \"%s\", Last detection: %d",
            currentSSIDIndex + 1, clonedList.size(), WifiDetector.isSomethingDetected(), 
            currentNetwork.ssid.c_str(),
            millis() / 1000 - WifiDetector.getLastDetectionTime());
    }

    currentSSIDIndex++;
  }

  checkTransmissionTimeout();
  checkAndRestartAdvertising();

  delay(appPrefs.wifi_channel_dwell_time);
}

/**
 * @brief Main loop function that runs repeatedly.
 *
 * This function handles WiFi channel hopping, periodic BLE scanning,
 * and manages the transmission of collected data over BLE.
 */
void loop()
{
    // Alimentar el watchdog al inicio del loop
    esp_task_wdt_reset();
    
    static unsigned long lastStatsPrint = 0;
    const unsigned long STATS_PRINT_INTERVAL = 15000; // 15 segundos (reducido para ver resultados más rápido)

    if (millis() - lastStatsPrint >= STATS_PRINT_INTERVAL) {
        // Print system stats with ps-like output
        log_d("===== Imprimiendo información del sistema (ps) =====");
        printSystemInfo();
        
        lastStatsPrint = millis();
    }

    if (appPrefs.operation_mode == OPERATION_MODE_SCAN)
    {
        scan_mode_loop();
    }
    else if (appPrefs.operation_mode == OPERATION_MODE_DETECTION)
    {
        detection_mode_loop();
    }
    else
    {
        // ON / OFF Red LED
        log_i("Operation mode == OFF");
        ledManager.setPixelColor(0, LedManager::COLOR_RED);
        ledManager.show();
        delay(appPrefs.wifi_channel_dwell_time);
        ledManager.setPixelColor(0, LedManager::COLOR_OFF);
        ledManager.show();
        delay(appPrefs.wifi_channel_dwell_time);
    }
    
    // Only update BLE status if BLE mode is enabled
    if (appPrefs.mode_ble) {
        // PROGRAMACIÓN DEFENSIVA: Restauramos la llamada con verificaciones de nulidad implementadas
        BLEStatusUpdater.update();
        log_i("BLE status updated with null pointer checks");
    }
}
