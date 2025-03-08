#include "TaskManager.h"
#include "AppPreferences.h"
#include "BLEScan.h"
#include "WifiScan.h"
#include "BLEDeviceList.h"
#include "WifiDeviceList.h"
#include "WifiNetworkList.h"
#include "FlashStorage.h"
#include "BLEStatusUpdater.h"

// Referencia a las listas globales
extern BLEDeviceList bleDeviceList;
extern WifiDeviceList stationsList;
extern WifiNetworkList ssidList;
extern AppPreferencesData appPrefs;

// Implementación de BLEScanCallbacks
BLEScanCallbacks::BLEScanCallbacks(QueueHandle_t queue, int8_t minRssi) 
    : deviceQueue(queue), minimalRssi(minRssi) {
}

void BLEScanCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
    // Ignorar si no cumple con RSSI mínimo
    if (advertisedDevice.getRSSI() < minimalRssi) {
        return;
    }
    
    // Preparar mensaje para la cola
    BLEDeviceMessage msg;
    
    // Copiar dirección MAC - convertir de puntero a array a puntero simple
    const uint8_t (*rawAddrArray)[6] = advertisedDevice.getAddress().getNative();
    const uint8_t* rawAddr = reinterpret_cast<const uint8_t*>(rawAddrArray);
    memcpy(msg.address, rawAddr, 6);
    
    // Copiar nombre si está disponible
    if (advertisedDevice.haveName()) {
        strncpy(msg.name, advertisedDevice.getName().c_str(), sizeof(msg.name) - 1);
        msg.name[sizeof(msg.name) - 1] = '\0'; // Asegurar null-termination
    } else {
        msg.name[0] = '\0';
    }
    
    // Configurar otros campos
    msg.rssi = advertisedDevice.getRSSI();
    
    // Determinar si es dirección pública o privada
    switch (advertisedDevice.getAddressType()) {
        case BLE_ADDR_TYPE_PUBLIC:
        case BLE_ADDR_TYPE_RPA_PUBLIC:
            msg.isPublic = true;
            break;
        default:
            msg.isPublic = false;
            break;
    }
    
    // Copiar payload si está disponible
    size_t payloadLen = advertisedDevice.getPayloadLength();
    if (payloadLen > 0) {
        if (payloadLen <= sizeof(msg.payload)) {
            memcpy(msg.payload, advertisedDevice.getPayload(), payloadLen);
            msg.payload_len = payloadLen;
        } else {
            memcpy(msg.payload, advertisedDevice.getPayload(), sizeof(msg.payload));
            msg.payload_len = sizeof(msg.payload);
        }
    } else {
        msg.payload_len = 0;
    }
    
    // Enviar a la cola
    if (deviceQueue) {
        xQueueSend(deviceQueue, &msg, 0);
    }
}

// Constructor
TaskManager::TaskManager() {
    log_i("Inicializando TaskManager");
}

// Destructor
TaskManager::~TaskManager() {
    stopAll();
}

// Inicializa el sistema de tareas
bool TaskManager::init() {
    log_i("Iniciando sistema de tareas");

    // Crear colas primero
    wifiPacketQueue = xQueueCreate(QUEUE_SIZE_WIFI_PACKETS, sizeof(WifiPacketMessage));
    bleDeviceQueue = xQueueCreate(QUEUE_SIZE_BLE_DEVICES, sizeof(BLEDeviceMessage));
    commandQueue = xQueueCreate(QUEUE_SIZE_COMMAND, sizeof(CommandMessage));

    if (!wifiPacketQueue || !bleDeviceQueue || !commandQueue) {
        log_e("Error al crear colas");
        return false;
    }

    // Crear semáforos
    wifiListMutex = xSemaphoreCreateMutex();
    bleListMutex = xSemaphoreCreateMutex();
    flashMutex = xSemaphoreCreateMutex();

    if (!wifiListMutex || !bleListMutex || !flashMutex) {
        log_e("Error al crear semáforos");
        return false;
    }

    // Crear tareas según el modo de operación
    bool tasksCreated = false;

    // La tarea de gestión de dispositivos siempre se inicia
    if (!createDeviceManagementTask()) {
        log_e("Error al crear tarea de gestión de dispositivos");
        return false;
    }

    // La tarea de servicio BLE siempre se inicia
    if (!createBLEServiceTask()) {
        log_e("Error al crear tarea de servicio BLE");
        return false;
    }

    // Iniciar tareas según el modo de operación
    switch (appPrefs.operation_mode) {
        case OPERATION_MODE_SCAN:
            log_i("Iniciando tareas en modo SCAN");
            log_i("[DEBUG] WiFi task disabled for testing");
            tasksCreated = createBLEScanTask();
            break;
        
        case OPERATION_MODE_DETECTION:
            log_i("Iniciando tareas en modo DETECTION");
            log_i("[DEBUG] WiFi task disabled for testing");
            tasksCreated = createBLEScanTask();
            break;
        
        case OPERATION_MODE_OFF:
        default:
            // En modo apagado, solo iniciamos la tarea de estado
            log_i("Iniciando tareas en modo OFF");
            tasksCreated = true;
            break;
    }

    // Siempre iniciamos la tarea de estado
    if (!createStatusTask()) {
        log_e("Error al crear tarea de estado");
        return false;
    }

    log_i("Sistema de tareas iniciado correctamente");
    return tasksCreated;
}

// Detiene todas las tareas
void TaskManager::stopAll() {
    log_i("Deteniendo todas las tareas");

    // Detener tareas si existen
    if (wifiScanTaskHandle) {
        vTaskDelete(wifiScanTaskHandle);
        wifiScanTaskHandle = nullptr;
    }
    
    if (bleScanTaskHandle) {
        vTaskDelete(bleScanTaskHandle);
        bleScanTaskHandle = nullptr;
    }
    
    if (deviceMgmtTaskHandle) {
        vTaskDelete(deviceMgmtTaskHandle);
        deviceMgmtTaskHandle = nullptr;
    }
    
    if (dataProcessTaskHandle) {
        vTaskDelete(dataProcessTaskHandle);
        dataProcessTaskHandle = nullptr;
    }
    
    if (bleServiceTaskHandle) {
        vTaskDelete(bleServiceTaskHandle);
        bleServiceTaskHandle = nullptr;
    }
    
    if (statusTaskHandle) {
        vTaskDelete(statusTaskHandle);
        statusTaskHandle = nullptr;
    }

    // Liberar colas
    if (wifiPacketQueue) {
        vQueueDelete(wifiPacketQueue);
        wifiPacketQueue = nullptr;
    }
    
    if (bleDeviceQueue) {
        vQueueDelete(bleDeviceQueue);
        bleDeviceQueue = nullptr;
    }
    
    if (commandQueue) {
        vQueueDelete(commandQueue);
        commandQueue = nullptr;
    }

    // Liberar semáforos
    if (wifiListMutex) {
        vSemaphoreDelete(wifiListMutex);
        wifiListMutex = nullptr;
    }
    
    if (bleListMutex) {
        vSemaphoreDelete(bleListMutex);
        bleListMutex = nullptr;
    }
    
    if (flashMutex) {
        vSemaphoreDelete(flashMutex);
        flashMutex = nullptr;
    }

    log_i("Todas las tareas detenidas");
}

// Métodos para crear tareas individuales
bool TaskManager::createWifiScanTask() {
    log_i("Creando tarea de escaneo WiFi");
    BaseType_t result = xTaskCreatePinnedToCore(
        wifiScanTask,
        "WiFiScan",
        STACK_SIZE_WIFI_SCAN,
        this,
        TASK_PRIORITY_WIFI_SCAN,
        &wifiScanTaskHandle,
        PRO_CPU_CORE  // Tarea crítica en el núcleo PRO
    );
    
    if (result != pdPASS) {
        log_e("Error al crear tarea WiFiScan: %d", result);
        return false;
    }
    
    return true;
}

bool TaskManager::createBLEScanTask() {
    log_i("Creando tarea de escaneo BLE");
    BaseType_t result = xTaskCreatePinnedToCore(
        bleScanTask,
        "BLEScan",
        STACK_SIZE_BLE_SCAN,
        this,
        TASK_PRIORITY_BLE_SCAN,
        &bleScanTaskHandle,
        PRO_CPU_CORE  // Tarea crítica en el núcleo PRO
    );
    
    if (result != pdPASS) {
        log_e("Error al crear tarea BLEScan: %d", result);
        return false;
    }
    
    return true;
}

bool TaskManager::createDeviceManagementTask() {
    log_i("Creando tarea de gestión de dispositivos");
    BaseType_t result = xTaskCreatePinnedToCore(
        deviceManagementTask,
        "DeviceMgmt",
        STACK_SIZE_DEVICE_MGMT,
        this,
        TASK_PRIORITY_DEVICE_MGMT,
        &deviceMgmtTaskHandle,
        APP_CPU_CORE  // Tarea con mucha memoria en el núcleo APP
    );
    
    if (result != pdPASS) {
        log_e("Error al crear tarea DeviceMgmt: %d", result);
        return false;
    }
    
    return true;
}

bool TaskManager::createDataProcessTask() {
    log_i("Creando tarea de procesamiento de datos");
    BaseType_t result = xTaskCreatePinnedToCore(
        dataProcessTask,
        "DataProcess",
        STACK_SIZE_DATA_PROCESS,
        this,
        TASK_PRIORITY_DATA_PROCESS,
        &dataProcessTaskHandle,
        APP_CPU_CORE  // Núcleo APP para procesamiento
    );
    
    if (result != pdPASS) {
        log_e("Error al crear tarea DataProcess: %d", result);
        return false;
    }
    
    return true;
}

bool TaskManager::createBLEServiceTask() {
    log_i("Creando tarea de servicio BLE");
    BaseType_t result = xTaskCreatePinnedToCore(
        bleServiceTask,
        "BLEService",
        STACK_SIZE_BLE_SERVICE,
        this,
        TASK_PRIORITY_BLE_SERVICE,
        &bleServiceTaskHandle,
        APP_CPU_CORE  // Núcleo APP para comunicación
    );
    
    if (result != pdPASS) {
        log_e("Error al crear tarea BLEService: %d", result);
        return false;
    }
    
    return true;
}

bool TaskManager::createStatusTask() {
    log_i("Creando tarea de estado");
    BaseType_t result = xTaskCreatePinnedToCore(
        statusTask,
        "Status",
        STACK_SIZE_STATUS,
        this,
        TASK_PRIORITY_STATUS,
        &statusTaskHandle,
        APP_CPU_CORE  // Núcleo APP para tareas no críticas
    );
    
    if (result != pdPASS) {
        log_e("Error al crear tarea Status: %d", result);
        return false;
    }
    
    return true;
}

// Funciones de las tareas

void TaskManager::wifiScanTask(void* parameter) {
    TaskManager* manager = static_cast<TaskManager*>(parameter);
    QueueHandle_t wifiQueue = manager->getWifiPacketQueue();
    WifiPacketMessage packet;
    
    log_i("Tarea de escaneo WiFi iniciada");
    
    // Configuración del WiFi en modo promiscuo
    WiFi.mode(WIFI_MODE_STA);
    esp_wifi_set_promiscuous(true);
    
    // Callback personalizado que envía los paquetes a la cola en lugar de procesarlos directamente
    esp_wifi_set_promiscuous_rx_cb([](void *buf, wifi_promiscuous_pkt_type_t type) {
        if (type != WIFI_PKT_MGMT && type != WIFI_PKT_CTRL && type != WIFI_PKT_DATA)
            return;
        
        wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
        
        // Crear mensaje
        WifiPacketMessage msg;
        if (pkt->rx_ctrl.sig_len > sizeof(msg.payload)) {
            return; // Paquete demasiado grande
        }
        
        // Copiar datos
        memcpy(msg.payload, pkt->payload, pkt->rx_ctrl.sig_len);
        msg.length = pkt->rx_ctrl.sig_len;
        msg.rssi = pkt->rx_ctrl.rssi;
        msg.channel = pkt->rx_ctrl.channel;
        
        // Extraer tipo y subtipo del frame
        uint16_t frame_control = pkt->payload[0] | (pkt->payload[1] << 8);
        msg.frame_type = (frame_control & 0x000C) >> 2;
        msg.frame_subtype = (frame_control & 0x00F0) >> 4;
        
        // Enviar a la cola
        QueueHandle_t queue = TaskManager::getInstance().getWifiPacketQueue();
        if (queue) {
            xQueueSend(queue, &msg, 0);
        }
    });
    
    // Configurar filtro para capturar todos los paquetes o solo los de gestión
    wifi_promiscuous_filter_t filter;
    if (appPrefs.only_management_frames) {
        filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    } else {
        filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL};
    }
    esp_wifi_set_promiscuous_filter(&filter);
    
    // Bucle principal, haciendo channel hopping
    uint8_t channel = 1;
    while (true) {
        // Cambiar canal
        channel = (channel % 14) + 1;
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        
        // Esperar el tiempo configurado en cada canal
        vTaskDelay(appPrefs.wifi_channel_dwell_time / portTICK_PERIOD_MS);
    }
}

void TaskManager::bleScanTask(void* parameter) {
    TaskManager* manager = static_cast<TaskManager*>(parameter);
    QueueHandle_t bleQueue = manager->getBLEDeviceQueue();
    
    log_i("Tarea de escaneo BLE iniciada");
    
    // Configurar escáner BLE
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(!appPrefs.passive_scan);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(90);
    
    // Crear e instalar callbacks
    BLEScanCallbacks* callbacks = new BLEScanCallbacks(bleQueue, appPrefs.minimal_rssi);
    pBLEScan->setAdvertisedDeviceCallbacks(callbacks);
    
    // Bucle principal
    while (true) {
        // Iniciar escaneo
        log_d("Iniciando escaneo BLE");
        pBLEScan->start(appPrefs.ble_scan_duration, false);
        pBLEScan->clearResults();
        
        // Esperar antes del próximo escaneo
        vTaskDelay(appPrefs.ble_scan_delay * 1000 / portTICK_PERIOD_MS);
    }
}

void TaskManager::deviceManagementTask(void* parameter) {
    TaskManager* manager = static_cast<TaskManager*>(parameter);
    QueueHandle_t wifiQueue = manager->getWifiPacketQueue();
    QueueHandle_t bleQueue = manager->getBLEDeviceQueue();
    SemaphoreHandle_t wifiMutex = manager->getWifiListMutex();
    SemaphoreHandle_t bleMutex = manager->getBLEListMutex();
    SemaphoreHandle_t flashMutex = manager->getFlashMutex();
    
    WifiPacketMessage wifiMsg;
    BLEDeviceMessage bleMsg;
    unsigned long lastSavedTime = 0;
    unsigned long lastMemoryCheckTime = 0;
    unsigned long lastCleanupTime = 0;
    const unsigned long MEMORY_CHECK_INTERVAL = 60000; // Verificar memoria cada minuto
    const unsigned long CLEANUP_INTERVAL = 300000;     // Limpiar dispositivos cada 5 minutos
    
    log_i("Tarea de gestión de dispositivos iniciada");
    
    while (true) {
        // Procesar mensajes WiFi (no bloqueante)
        while (xQueueReceive(wifiQueue, &wifiMsg, 0) == pdTRUE) {
            // Extraer información relevante del paquete WiFi
            const uint8_t *payload = wifiMsg.payload;
            uint8_t frame_type = wifiMsg.frame_type;
            uint8_t frame_subtype = wifiMsg.frame_subtype;
            int8_t rssi = wifiMsg.rssi;
            uint8_t channel = wifiMsg.channel;
            
            // Procesamiento según tipo de frame
            if (frame_type == 0) { // Management
                // Extraer direcciones MAC
                const uint8_t *dst_addr = &payload[4];
                const uint8_t *src_addr = &payload[10];
                const uint8_t *bssid = &payload[16];
                
                // Obtener SSID para ciertos tipos de frames
                char ssid[33] = {0};
                if (frame_subtype == 0 || frame_subtype == 2 || 
                    frame_subtype == 4 || frame_subtype == 5 || frame_subtype == 8) {
                    // Parsing de SSID (simplificado)
                    // TODO: Implementar parsing correcto de SSID
                }
                
                // Actualizar listas con mutex
                if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    // Actualizar lista de redes
                    String frameType = "unknown";
                    switch (frame_subtype) {
                        case 0: frameType = "assoc"; break;
                        case 2: frameType = "reassoc"; break;
                        case 4: frameType = "probe"; break;
                        case 5: frameType = "probe-resp"; break;
                        case 8: frameType = "beacon"; break;
                        default: frameType = "other"; break;
                    }
                    
                    // Si tenemos SSID o BSSID, actualizamos la lista de redes
                    if (ssid[0] || memcmp(bssid, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) != 0) {
                        ssidList.updateOrAddNetwork(String(ssid), MacAddress(bssid), rssi, channel, frameType);
                    }
                    
                    // Actualizar lista de dispositivos
                    stationsList.updateOrAddDevice(MacAddress(src_addr), MacAddress(bssid), rssi, channel);
                    
                    // Si el destino no es broadcast, también lo añadimos
                    if (memcmp(dst_addr, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) != 0) {
                        stationsList.updateOrAddDevice(MacAddress(dst_addr), MacAddress(bssid), rssi, channel);
                    }
                    
                    xSemaphoreGive(wifiMutex);
                }
            }
            // Procesamiento para otros tipos (control, datos)...
        }
        
        // Procesar mensajes BLE (no bloqueante)
        while (xQueueReceive(bleQueue, &bleMsg, 0) == pdTRUE) {
            // Ignorar dispositivos aleatorios si está configurado
            if (appPrefs.ignore_random_ble_addresses && !bleMsg.isPublic) {
                continue;
            }
            
            // Actualizar lista con mutex
            if (xSemaphoreTake(bleMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Convertir el array de bytes a MacAddress
                MacAddress mac(bleMsg.address);
                
                // Usar la versión de string para el nombre
                String name(bleMsg.name);
                
                bleDeviceList.updateOrAddDevice(mac, bleMsg.rssi, name, bleMsg.isPublic);
                xSemaphoreGive(bleMutex);
            }
        }
        
        // Verificación periódica de memoria
        unsigned long currentTime = millis();
        if (currentTime - lastMemoryCheckTime >= MEMORY_CHECK_INTERVAL) {
            // Verificar el estado de la memoria
            size_t freeHeap = ESP.getFreeHeap();
            size_t minFreeHeap = ESP.getMinFreeHeap();
            
            // Si el heap está bajo, limpiar dispositivos antiguos
            if (freeHeap < (ESP.getHeapSize() * 0.2)) { // menos del 20% libre
                log_w("Memoria baja (Free: %d bytes, Min: %d bytes) - Limpiando dispositivos antiguos",
                      freeHeap, minFreeHeap);
                
                // Limpiar redes WiFi antiguas
                if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    size_t before = ssidList.size();
                    // Eliminar redes que no se han visto en las últimas 6 horas
                    time_t threshold = time(nullptr) - 6 * 60 * 60;
                    // TODO: Implementar método de limpieza en las clases
                    // ssidList.removeOlderThan(threshold);
                    size_t after = ssidList.size();
                    log_i("Limpiados %d SSIDs", before - after);
                    xSemaphoreGive(wifiMutex);
                }
                
                // Limpiar dispositivos BLE antiguos
                if (xSemaphoreTake(bleMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    size_t before = bleDeviceList.size();
                    // TODO: Implementar método de limpieza
                    // bleDeviceList.removeOlderThan(threshold);
                    size_t after = bleDeviceList.size();
                    log_i("Limpiados %d dispositivos BLE", before - after);
                    xSemaphoreGive(bleMutex);
                }
            }
            
            lastMemoryCheckTime = currentTime;
        }
        
        // Guardar periódicamente en flash
        if (currentTime - lastSavedTime >= appPrefs.autosave_interval * 60 * 1000) {
            if (xSemaphoreTake(flashMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                log_i("Guardando datos en flash");
                try {
                    FlashStorage::saveAll();
                    lastSavedTime = currentTime;
                } catch (const std::exception &e) {
                    log_e("Error al guardar en flash: %s", e.what());
                }
                xSemaphoreGive(flashMutex);
            }
        }
        
        // Limpieza periódica para mantener tamaños óptimos
        if (currentTime - lastCleanupTime >= CLEANUP_INTERVAL) {
            // Solo limpiar si no estamos usando PSRAM (o hay poca memoria)
            if (!psramFound() || ESP.getFreePsram() < 10000) {
                // Purgar los dispositivos menos relevantes para mantener tamaños manejables
                log_i("Realizando limpieza periódica para mantener óptimo uso de memoria");
                
                // TODO: Implementar métodos para purgar dispositivos menos relevantes
                // (por ejemplo, con señal débil o vistos pocas veces)
                
                lastCleanupTime = currentTime;
            }
        }
        
        // Delay para no consumir CPU innecesariamente
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void TaskManager::dataProcessTask(void* parameter) {
    TaskManager* manager = static_cast<TaskManager*>(parameter);
    SemaphoreHandle_t wifiMutex = manager->getWifiListMutex();
    SemaphoreHandle_t bleMutex = manager->getBLEListMutex();
    
    log_i("Tarea de procesamiento de datos iniciada");
    
    while (true) {
        // Esta tarea es para procesamiento adicional, como análisis de patrones, 
        // correlación entre dispositivos BLE y WiFi, etc.
        
        // Ejemplo: imprimir estadísticas cada cierto tiempo
        static unsigned long lastPrintTime = 0;
        unsigned long currentTime = millis();
        
        if (currentTime - lastPrintTime >= 30000) { // cada 30 segundos
            // Obtener datos con mutexes
            if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                log_i("Estadísticas WiFi: %d redes, %d dispositivos",
                      ssidList.size(), stationsList.size());
                xSemaphoreGive(wifiMutex);
            }
            
            if (xSemaphoreTake(bleMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                log_i("Estadísticas BLE: %d dispositivos", bleDeviceList.size());
                xSemaphoreGive(bleMutex);
            }
            
            lastPrintTime = currentTime;
        }
        
        // Procesar datos...
        
        // Delay para no consumir CPU
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void TaskManager::bleServiceTask(void* parameter) {
    TaskManager* manager = static_cast<TaskManager*>(parameter);
    SemaphoreHandle_t wifiMutex = manager->getWifiListMutex();
    SemaphoreHandle_t bleMutex = manager->getBLEListMutex();
    
    log_i("Tarea de servicio BLE iniciada");
    
    // Esta tarea gestiona la comunicación BLE con los clientes
    // Implementa los servicios BLE, notificaciones y respuestas a comandos
    
    while (true) {
        // Verificar conexiones BLE y atender solicitudes
        
        // Actualizar datos cuando se solicite
        BLEStatusUpdater.update();
        
        // Delay corto para atender rápidamente las solicitudes BLE
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void TaskManager::statusTask(void* parameter) {
    TaskManager* manager = static_cast<TaskManager*>(parameter);
    
    log_i("Tarea de estado iniciada");
    
    // Esta tarea gestiona el estado global del sistema, como LEDs, 
    // cambios de modo, reinicio de advertising, etc.
    
    while (true) {
        // Comprobar si necesitamos reiniciar el advertising BLE
        static unsigned long lastAdvertisingRestart = 0;
        const unsigned long advertisingRestartInterval = 60 * 60 * 1000; // 1 hora
        
        if (millis() - lastAdvertisingRestart > advertisingRestartInterval) {
            log_i("Reiniciando BLE advertising");
            BLEDevice::stopAdvertising();
            delay(100);
            BLEDevice::startAdvertising();
            lastAdvertisingRestart = millis();
        }
        
        // Actualizar estado LED según modo
        // ...
        
        // Monitorizar memoria y reportar estadísticas
        static unsigned long lastMemoryReport = 0;
        if (millis() - lastMemoryReport > 30000) { // Cada 30 segundos
            log_d("Estadísticas de memoria: PSRAM libre: %d bytes, Heap libre: %d bytes", 
                  ESP.getFreePsram(), ESP.getFreeHeap());
            lastMemoryReport = millis();
        }
        
        // Delay más largo para esta tarea no crítica
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}