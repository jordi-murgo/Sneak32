#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include "MacAddress.h"
#include <BLEDevice.h>
#include <BLEAdvertisedDevice.h>

// Tamaños máximos para listas de dispositivos 
// Ajustamos automáticamente basado en si hay PSRAM disponible o no
#define MAX_STATIONS (psramFound() ? 255 : 100)
#define MAX_SSIDS (psramFound() ? 200 : 50)
#define MAX_BLE_DEVICES (psramFound() ? 100 : 50)

// Definición de prioridades de tareas (mayor número = mayor prioridad)
#define TASK_PRIORITY_WIFI_SCAN    5  // Alta prioridad para captura de paquetes WiFi
#define TASK_PRIORITY_BLE_SCAN     4  // Alta prioridad para escaneo BLE
#define TASK_PRIORITY_DEVICE_MGMT  3  // Prioridad media para gestión de dispositivos
#define TASK_PRIORITY_DATA_PROCESS 2  // Prioridad media-baja para procesamiento
#define TASK_PRIORITY_BLE_SERVICE  2  // Prioridad media-baja para servicios BLE
#define TASK_PRIORITY_STATUS       1  // Baja prioridad para actualización de estado

// Definición de tamaños de stack para tareas
#define STACK_SIZE_WIFI_SCAN     4096
#define STACK_SIZE_BLE_SCAN      4096
#define STACK_SIZE_DEVICE_MGMT   8192  // Mayor stack para gestión de listas en PSRAM
#define STACK_SIZE_DATA_PROCESS  4096
#define STACK_SIZE_BLE_SERVICE   4096
#define STACK_SIZE_STATUS        2048

// Tamaños de colas
#define QUEUE_SIZE_WIFI_PACKETS  50
#define QUEUE_SIZE_BLE_DEVICES   50
#define QUEUE_SIZE_COMMAND       10

// Definición de los núcleos del ESP32 (0-1)
// Estos ya están definidos en el SDK de ESP32, usamos nombres diferentes
#define APP_CPU_CORE 1
#define PRO_CPU_CORE 0

// Tipos de mensajes
enum class MessageType {
    WIFI_PACKET,
    BLE_DEVICE,
    COMMAND,
    STATUS_UPDATE
};

// Estructura para mensajes WiFi
struct WifiPacketMessage {
    uint8_t payload[256];  // Buffer para el paquete WiFi
    size_t length;         // Longitud del paquete
    int8_t rssi;           // RSSI
    uint8_t channel;       // Canal
    uint8_t frame_type;    // Tipo de frame (management, control, data)
    uint8_t frame_subtype; // Subtipo específico del frame
};

// Estructura para mensajes BLE
struct BLEDeviceMessage {
    uint8_t address[6];   // Dirección MAC en formato raw
    char name[32];        // Nombre del dispositivo (si está disponible)
    int8_t rssi;          // RSSI
    bool isPublic;        // ¿Es dirección pública?
    uint8_t payload[31];  // Datos de advertising
    size_t payload_len;   // Longitud de los datos
};

// Estructura para mensajes de comando
struct CommandMessage {
    uint8_t command;      // Código de comando
    uint8_t params[32];   // Parámetros del comando
    size_t params_len;    // Longitud de parámetros
};

// Implementación concreta de BLEAdvertisedDeviceCallbacks
class BLEScanCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    BLEScanCallbacks(QueueHandle_t queue, int8_t minRssi);
    virtual void onResult(BLEAdvertisedDevice advertisedDevice) override;
    
private:
    QueueHandle_t deviceQueue;
    int8_t minimalRssi;
};

// Clase para gestionar las tareas y colas del sistema
class TaskManager {
public:
    static TaskManager& getInstance() {
        static TaskManager instance;
        return instance;
    }

    // Inicializa el sistema de tareas
    bool init();
    
    // Detiene todas las tareas
    void stopAll();

    // Obtiene los handles de las colas para envío de mensajes
    QueueHandle_t getWifiPacketQueue() const { return wifiPacketQueue; }
    QueueHandle_t getBLEDeviceQueue() const { return bleDeviceQueue; }
    QueueHandle_t getCommandQueue() const { return commandQueue; }

    // Obtiene los semáforos para acceso a recursos compartidos
    SemaphoreHandle_t getWifiListMutex() const { return wifiListMutex; }
    SemaphoreHandle_t getBLEListMutex() const { return bleListMutex; }
    SemaphoreHandle_t getFlashMutex() const { return flashMutex; }

private:
    TaskManager(); // Constructor privado (singleton)
    ~TaskManager();

    // Handles de tareas
    TaskHandle_t wifiScanTaskHandle = nullptr;
    TaskHandle_t bleScanTaskHandle = nullptr;
    TaskHandle_t deviceMgmtTaskHandle = nullptr;
    TaskHandle_t dataProcessTaskHandle = nullptr;
    TaskHandle_t bleServiceTaskHandle = nullptr;
    TaskHandle_t statusTaskHandle = nullptr;

    // Colas para comunicación entre tareas
    QueueHandle_t wifiPacketQueue = nullptr;
    QueueHandle_t bleDeviceQueue = nullptr;
    QueueHandle_t commandQueue = nullptr;

    // Semáforos para protección de recursos compartidos
    SemaphoreHandle_t wifiListMutex = nullptr;
    SemaphoreHandle_t bleListMutex = nullptr;
    SemaphoreHandle_t flashMutex = nullptr;

    // Métodos para crear tareas
    bool createWifiScanTask();
    bool createBLEScanTask();
    bool createDeviceManagementTask();
    bool createDataProcessTask();
    bool createBLEServiceTask();
    bool createStatusTask();

    // Funciones estáticas para las tareas
    static void wifiScanTask(void* parameter);
    static void bleScanTask(void* parameter);
    static void deviceManagementTask(void* parameter);
    static void dataProcessTask(void* parameter);
    static void bleServiceTask(void* parameter);
    static void statusTask(void* parameter);

    // Evitar copias o asignaciones
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
};