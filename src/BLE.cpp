#include "BLE.h"
#include "BLEAdvertisingManager.h"
#include "BLEDeviceWrapper.h"
#include "BLEScan.h"
#include "AppPreferences.h"
#include "Logging.h"
#include "version.h"
#include "FirmwareInfo.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include <string>

extern AppPreferencesData appPrefs;

// BLE Server objects
BLEServer* pServer = nullptr;
BLEService* pScannerService = nullptr;
BLECharacteristic* pCommandCharacteristic = nullptr;
BLECharacteristic* pResponseCharacteristic = nullptr;
BLECharacteristic* pStatusCharacteristic = nullptr;
BLECharacteristic* pFirmwareInfoCharacteristic = nullptr;
BLECharacteristic* pDataTransferCharacteristic = nullptr;
BLECharacteristic* pSettingsCharacteristic = nullptr;
BLECharacteristic* pListSizesCharacteristic = nullptr;

// BLE Security
BLESecurity* pSecurity = nullptr;

// Connection status
bool deviceConnected = false;
bool oldDeviceConnected = false;

/**
 * @brief Server callbacks for BLE
 */
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        log_i("BLE client connected");
        
        // Mostrar información sobre cliente conectado
        try {
            auto peerDevices = pServer->getPeerDevices(true);
            if (peerDevices.size() > 0) {
                log_i("Number of connected peer devices: %d", peerDevices.size());
                
                for (auto &peerDevice : peerDevices) {
                    uint16_t id = peerDevice.first;
                    conn_status_t status = peerDevice.second;
                    log_i("Peer device ID: %d, Connected: %d", id, status.connected);
                    
                    if (status.peer_device != nullptr) {
                        BLEClient *client = (BLEClient *)status.peer_device;
                        log_i("Client address: %s", client->getPeerAddress().toString().c_str());
                    }
                }
            } else {
                log_i("No peer devices information available");
            }
        } catch (std::exception &e) {
            log_e("Exception in onConnect peer info: %s", e.what());
        } catch (...) {
            log_e("Unknown exception in onConnect peer info");
        }
        
        // No hacemos nada con el advertising aquí, dejamos que siga funcionando
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        log_i("BLE client disconnected");
        
        // Reiniciar advertising para que el dispositivo vuelva a ser visible inmediatamente
        delay(100); // Pequeña pausa para estabilizar
        
        try {
            log_i("Restarting advertising after disconnect");
            BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
            if (pAdvertising != nullptr) {
                pAdvertising->start();
                log_i("Advertising restarted successfully");
            } else {
                log_e("Failed to get advertising object for restart");
            }
        } catch (std::exception &e) {
            log_e("Exception restarting advertising: %s", e.what());
        } catch (...) {
            log_e("Unknown exception restarting advertising");
        }
    }
};

/**
 * @brief Command characteristic callbacks
 */
class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            log_i("Received command: %s", value.c_str());
            
            // Process command and prepare response
            String response = processCommand(value.c_str());
            
            // Send response
            if (pResponseCharacteristic != nullptr) {
                pResponseCharacteristic->setValue(response.c_str());
                pResponseCharacteristic->notify();
                log_i("Sent response: %s", response.c_str());
            } else {
                log_e("Response characteristic is null");
            }
        }
    }
    
    /**
     * @brief Process a command and return a response
     * 
     * @param command The command to process
     * @return String response
     */
    String processCommand(const char* command) {
        // Simple command processing
        String cmd = String(command);
        
        if (cmd == "VERSION") {
            return getFirmwareInfoString();
        } else if (cmd == "SCAN_START") {
            BLEScanner.start();
            return "Scan started";
        } else if (cmd == "SCAN_STOP") {
            BLEScanner.stop();
            return "Scan stopped";
        } else if (cmd.startsWith("SET_NAME:")) {
            String newName = cmd.substring(9);
            if (newName.length() > 0) {
                // Update device name
                strcpy(appPrefs.device_name, newName.c_str());
                // Save preferences
                saveAppPreferences();
                return "Name set to: " + newName;
            }
            return "Invalid name";
        } else {
            return "Unknown command";
        }
    }
};

/**
 * @brief Initialize the BLE subsystem
 * 
 * This method initializes the BLE device and advertising.
 * It performs the following steps:
 * 1. Initialize BLEDevice
 * 2. Setup the advertising manager
 * 3. Create and configure BLE service and characteristics
 * 
 * @return bool Returns true if initialization was successful
 */
bool setupBLE() {
    log_i("Initializing BLE module with memory optimization");
    
    // No podemos manipular el watchdog directamente
    log_i("Note: Watchdog remains active during BLE initialization");
    
    // Try to ensure heap is in good state before initialization
    size_t freeHeap = ESP.getFreeHeap();
    size_t minHeap = ESP.getMinFreeHeap();
    size_t heapSize = ESP.getHeapSize();
    
    log_i("Heap stats before BLE init: Free=%d, Min=%d, Size=%d", 
           freeHeap, minHeap, heapSize);
    
    // Check if we have enough memory to proceed
    if (freeHeap < 40000) { // Require at least 40KB of free heap
        log_e("Not enough free heap memory to initialize BLE safely: %d bytes", freeHeap);
        return false;
    }
    
    // Memory cleanup with multiple small delays and yields to prevent watchdog reset
    log_i("Performing memory cleanup before BLE init");
    for (int i = 0; i < 3; i++) {
        delay(10);  // Shorter delays to prevent watchdog issues
        yield();
    }
    
    // Try initialization with retries
    const int MAX_RETRIES = 3;  // Increased from 2 to 3
    bool success = false;
    
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        log_i("BLE initialization attempt %d/%d", attempt, MAX_RETRIES);
        
        // Pre-init delay with yield to prevent watchdog timer reset
        for (int i = 0; i < 6; i++) {
            delay(50);  // Split into smaller delays with yields
            yield();
        }
        
        try {
            if (BLEDeviceWrapper::init(appPrefs.device_name)) {
                log_i("BLE initialization successful");
                success = true;
                break;
            }
        } catch (const std::exception &e) {
            log_e("Exception during BLE initialization: %s", e.what());
        } catch (...) {
            log_e("Unknown exception during BLE initialization");
        }
        
        log_w("BLE initialization attempt %d failed, %s", 
              attempt, 
              attempt < MAX_RETRIES ? "retrying..." : "giving up");
        
        delay(1000); // Increased delay between retries
    }
    
    if (success) {
        // Setup advertising after successful init, but with a delay
        delay(500);
        try {
            // Establecer el rol de GAP a "General Discoverable" y "Undirected Connectable"
            esp_err_t errRet = esp_ble_gap_set_device_name(appPrefs.device_name);
            if (errRet) {
                log_e("Error setting GAP device name: %d", errRet);
            }
            
            // Configurar el poder de transmisión BLE para mejor alcance
            esp_err_t result = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
            log_i("Setting BLE TX power result: %d", result);
            
            // Obtener el nivel actual
            esp_power_level_t level = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
            log_i("Current BLE TX power level: %d", level);
            
            // Configurar el servidor BLE para hacer restart del advertising automáticamente después de desconexión
            log_i("Setting up BLE server and characteristics");
            
            // Crear el servidor BLE
            pServer = BLEDevice::createServer();
            if (pServer == nullptr) {
                log_e("Failed to create BLE server");
                return false;
            }
            
            // Configurar callbacks del servidor para gestionar conexiones
            log_i("Setting up server callbacks with auto-restart advertising");
            pServer->setCallbacks(new ServerCallbacks());
            
            // Crear el servicio BLE usando el UUID exacto definido en el cliente JavaScript
            BLEUUID serviceUUID(SNEAK32_SERVICE_UUID_STR);
            
            // Create a service with a reasonable attribute count to avoid excessive memory usage
            // while still accommodating all our characteristics
            pScannerService = pServer->createService(serviceUUID, 20, 0);  // Balanced attribute count
            
            // Yield to prevent watchdog timer from triggering
            delay(10);
            yield();
            
            if (pScannerService == nullptr) {
                log_e("Failed to create BLE service");
                return false;
            }
            
            log_i("Created BLE service with UUID: %s", SNEAK32_SERVICE_UUID_STR);
            
            // Crear características
            log_i("Creating BLE characteristics");
            
            // 1. Característica de estado (STATUS_UUID = 0xFFE1)
            pStatusCharacteristic = pScannerService->createCharacteristic(
                BLEUUID((uint16_t)STATUS_UUID), // Use 16-bit UUID with explicit cast
                BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
            );
            if (pStatusCharacteristic != nullptr) {
                pStatusCharacteristic->addDescriptor(new BLE2902());
                log_i("Status characteristic created successfully with UUID: %s and 16-bit value: 0x%04X", 
                     STATUS_UUID_STR, STATUS_UUID);
            } else {
                log_e("Failed to create status characteristic");
            }
            
            // 2. Característica de comandos (COMMANDS_UUID = 0xFFE4)
            pCommandCharacteristic = pScannerService->createCharacteristic(
                BLEUUID((uint16_t)COMMANDS_UUID), // Use 16-bit UUID with explicit cast
                BLECharacteristic::PROPERTY_WRITE
            );
            if (pCommandCharacteristic != nullptr) {
                pCommandCharacteristic->setCallbacks(new CommandCallbacks());
                log_i("Command characteristic created successfully with UUID: %s and 16-bit value: 0x%04X", 
                     COMMANDS_UUID_STR, COMMANDS_UUID);
            } else {
                log_e("Failed to create command characteristic");
            }
            
            // 3. Característica de respuesta (RESPONSE_UUID)
            pResponseCharacteristic = pScannerService->createCharacteristic(
                BLEUUID((uint16_t)RESPONSE_UUID), // Use 16-bit UUID with explicit cast
                BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
            );
            if (pResponseCharacteristic != nullptr) {
                pResponseCharacteristic->addDescriptor(new BLE2902());
                log_i("Response characteristic created successfully with UUID: %s and 16-bit value: 0x%04X", 
                     RESPONSE_UUID_STR, RESPONSE_UUID);
            } else {
                log_e("Failed to create response characteristic");
            }
            
            // 4. Característica de transferencia de datos (DATA_TRANSFER_UUID = 0xFFE0)
            pDataTransferCharacteristic = pScannerService->createCharacteristic(
                BLEUUID((uint16_t)DATA_TRANSFER_UUID), // Use 16-bit UUID with explicit cast
                BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE
            );
            if (pDataTransferCharacteristic != nullptr) {
                pDataTransferCharacteristic->addDescriptor(new BLE2902());
                log_i("Data transfer characteristic created with UUID: %s and 16-bit value: 0x%04X", 
                     DATA_TRANSFER_UUID_STR, DATA_TRANSFER_UUID);
            } else {
                log_e("Failed to create data transfer characteristic");
            }
            
            // 5. Característica de configuración (SETTINGS_UUID = 0xFFE2)
            pSettingsCharacteristic = pScannerService->createCharacteristic(
                BLEUUID((uint16_t)SETTINGS_UUID), // Use 16-bit UUID with explicit cast
                BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
            );
            if (pSettingsCharacteristic != nullptr) {
                pSettingsCharacteristic->addDescriptor(new BLE2902());
                log_i("Settings characteristic created with UUID: %s and 16-bit value: 0x%04X", 
                     SETTINGS_UUID_STR, SETTINGS_UUID);
            } else {
                log_e("Failed to create settings characteristic");
            }
            
            // 6. Característica de información de firmware (FIRMWARE_INFO_UUID = 0xFFE3)
            // Create using 16-bit UUID directly for better compatibility with web client
            log_i("Creating firmware info characteristic with UUID: 0x%04X and string UUID: %s", 
                   FIRMWARE_INFO_UUID, FIRMWARE_INFO_UUID_STR);
            
            // Yield to prevent watchdog timer from triggering
            delay(5);
            yield();
            
            // Create the firmware info characteristic using the 16-bit UUID approach
            // This is more memory efficient and still works with most clients
            log_i("Creating firmware info characteristic with 16-bit UUID: 0x%04X", FIRMWARE_INFO_UUID);
            
            // Use 16-bit UUID with explicit cast - this is more memory efficient
            pFirmwareInfoCharacteristic = pScannerService->createCharacteristic(
                BLEUUID((uint16_t)FIRMWARE_INFO_UUID),
                BLECharacteristic::PROPERTY_READ
            );
            
            // Check for memory allocation failure
            if (pFirmwareInfoCharacteristic == nullptr) {
                log_e("Failed to create firmware info characteristic - memory allocation error");
                // Try to recover some memory
                delay(10);
                yield();
            }
            
            if (pFirmwareInfoCharacteristic != nullptr) {
                // Set a descriptive value that includes the firmware version
                String firmwareInfo = getFirmwareInfoString();
                pFirmwareInfoCharacteristic->setValue(firmwareInfo.c_str());
                
                // Only add essential descriptors to save memory
                // Add a user description descriptor to help with discovery
                BLEDescriptor* pDesc = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
                if (pDesc != nullptr) {
                    pDesc->setValue("Firmware Info"); // Shorter value to save memory
                    pFirmwareInfoCharacteristic->addDescriptor(pDesc);
                    log_i("Added user description descriptor");
                } else {
                    log_e("Failed to create descriptor - memory allocation error");
                }
                
                // Add a 2902 descriptor (Client Characteristic Configuration)
                // This is essential for discovery by web clients
                BLEDescriptor* pCCCD = new BLE2902();
                if (pCCCD != nullptr) {
                    pFirmwareInfoCharacteristic->addDescriptor(pCCCD);
                    log_i("Added CCCD descriptor");
                } else {
                    log_e("Failed to create CCCD descriptor - memory allocation error");
                }
                
                // Set the characteristic value once
                pFirmwareInfoCharacteristic->setValue(firmwareInfo.c_str());
                log_i("Set firmware info value: %s", firmwareInfo.c_str());
                
                // Yield to prevent watchdog timer from triggering
                yield();
                
                log_i("Firmware info characteristic created successfully");
                log_i("Firmware info value: %s", firmwareInfo.c_str());
                
                // Log the raw UUID for debugging
                BLEUUID uuid = pFirmwareInfoCharacteristic->getUUID();
                log_i("Firmware info characteristic UUID: %s, 16-bit value: 0x%04X", 
                       uuid.toString().c_str(), FIRMWARE_INFO_UUID);
            } else {
                log_e("Failed to create firmware info characteristic");
            }
            
            // Yield to prevent watchdog timer from triggering
            delay(5);
            yield();
            
            // Verificar que todas las características estén correctamente inicializadas
            log_i("Verifying all characteristics before starting service");
            if (pFirmwareInfoCharacteristic == nullptr) {
                log_e("pFirmwareInfoCharacteristic is null after creation");
            } else {
                // Asegurar que el valor está establecido correctamente
                String firmwareInfo = getFirmwareInfoString();
                pFirmwareInfoCharacteristic->setValue(firmwareInfo.c_str());
                log_i("pFirmwareInfoCharacteristic initialized with value: %s", firmwareInfo.c_str());
                
                // Imprimir todos los UUIDs para verificación
                log_i("Service UUID: %s", SNEAK32_SERVICE_UUID_STR);
                log_i("Firmware Info UUID: 0x%04X", FIRMWARE_INFO_UUID);
                
                // Verificar que el UUID de la característica coincide con lo esperado
                BLEUUID uuid = pFirmwareInfoCharacteristic->getUUID();
                log_i("Firmware info characteristic UUID from object: %s", uuid.toString().c_str());
                
                // Verificar que el UUID coincide con el valor esperado (0xFFE3)
                if (uuid.equals(BLEUUID(FIRMWARE_INFO_UUID_STR))) {
                    log_i("Firmware info characteristic UUID verification PASSED");
                } else {
                    log_e("Firmware info characteristic UUID verification FAILED");
                    log_e("Expected: %s, Got: %s", FIRMWARE_INFO_UUID_STR, uuid.toString().c_str());
                    
                    // Try to fix the characteristic if verification failed
                    log_i("Attempting to recreate firmware info characteristic with string UUID");
                    pFirmwareInfoCharacteristic = pScannerService->createCharacteristic(
                        BLEUUID(FIRMWARE_INFO_UUID_STR),
                        BLECharacteristic::PROPERTY_READ
                    );
                    
                    if (pFirmwareInfoCharacteristic != nullptr) {
                        pFirmwareInfoCharacteristic->setValue(firmwareInfo.c_str());
                        log_i("Firmware info characteristic recreated successfully");
                    }
                }
            }
            
            // Yield before starting the service to prevent watchdog timer from triggering
            delay(10);
            yield();
            
            // Single yield before starting the service
            yield();
            
            // Iniciar el servicio
            log_i("Starting BLE service with UUID: %s", SNEAK32_SERVICE_UUID_STR);
            pScannerService->start();
            
            // Single yield after starting the service
            yield();
            
            log_i("BLE server and characteristics setup complete");
            
            // Verificar que pStatusCharacteristic esté correctamente inicializado
            if (pStatusCharacteristic == nullptr) {
                log_e("pStatusCharacteristic is null after creation");
            } else {
                log_i("pStatusCharacteristic initialized with Ready");
                // Inicializar con un valor predeterminado
                pStatusCharacteristic->setValue("Ready");
                
                // Intentar notificar para activar la característica
                try {
                    pStatusCharacteristic->notify();
                } catch(...) {
                    log_e("Error notifying pStatusCharacteristic");
                }
            }
            
            // Yield before configuring advertising
            delay(5);
            yield();
            
            // Configurar advertising con configuración mínima para reducir la carga
            log_i("Setting up minimal advertising configuration");
            
            // Yield before getting advertising object
            yield();
            
            BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
            if (pAdvertising != nullptr) {
                // Limpiar datos antiguos
                pAdvertising->stop();
                
                // Multiple small yields to prevent watchdog timer from triggering
                yield();
                delay(1);
                yield();
                
                // Añadir UUID de servicio
                pAdvertising->addServiceUUID(serviceUUID);
                
                // Configurar aparecer en escaneos
                pAdvertising->setScanResponse(true);
                
                // Configure minimal advertising data to reduce memory usage
                BLEAdvertisementData advData;
                
                // Set standard flags for BLE advertising
                advData.setFlags(0x06); // General Discoverable Mode + BR/EDR Not Supported
                
                // Yield to prevent watchdog timer from triggering
                yield();
                
                // Advertise only the essential service UUID using 16-bit UUID when possible
                // This reduces memory usage during advertising
                advData.setCompleteServices(BLEUUID((uint16_t)FIRMWARE_INFO_UUID));
                
                // Set the advertisement data
                pAdvertising->setAdvertisementData(advData);
                
                // Yield to prevent watchdog timer from triggering
                yield();
                
                // Configure minimal scan response data
                BLEAdvertisementData scanResponse;
                scanResponse.setName("Sneak32");
                // Don't duplicate service UUIDs in scan response to save memory
                pAdvertising->setScanResponseData(scanResponse);
                
                // Yield to prevent watchdog timer from triggering
                yield();
                
                // Use very conservative advertising parameters to reduce BLE stack memory usage
                // These longer intervals significantly reduce memory pressure
                pAdvertising->setMinInterval(0x100);  // 0x100 * 0.625ms = 160ms
                pAdvertising->setMaxInterval(0x200);  // 0x200 * 0.625ms = 320ms
                
                // Yield to prevent watchdog timer from triggering
                yield();
                
                // Configurar como conectable con valores mínimos
                pAdvertising->setMinPreferred(0x0C);  // Increased to reduce frequency
                pAdvertising->setMaxPreferred(0x18);  // Increased to reduce frequency
                
                // Yield before starting advertising
                yield();
                
                // Check heap before starting advertising
                size_t freeHeapBeforeAdv = ESP.getFreeHeap();
                log_i("Free heap before advertising: %d bytes", freeHeapBeforeAdv);
                
                if (freeHeapBeforeAdv < 20000) { // At least 20KB free heap required
                    log_e("Low memory before advertising (%d bytes). Proceeding with caution.", freeHeapBeforeAdv);
                }
                
                // Iniciar advertising con configuración mínima
                log_i("Starting BLE advertising with minimal configuration");
                pAdvertising->start();
                
                // Yield after starting advertising
                yield();
                
                log_i("BLE advertising started successfully");
                log_i("BLE device should now be visible and connectable");
            } else {
                log_e("Failed to get advertising object");
            }
            
        } catch (const std::exception &e) {
            log_e("Exception during BLE server setup: %s", e.what());
            return false;
        } catch (...) {
            log_e("Unknown exception during BLE server setup");
            return false;
        }
        
        return true;
    }
    
    log_e("BLE initialization failed after %d attempts", MAX_RETRIES);
    return false;
}
