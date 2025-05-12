// TaskManager.h
#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "HW_NeonManager.h"
#include <Arduino.h>

// Forward declarations
class HW_FFTAudioAnalyzer;
class MicrophoneSensor;

namespace TaskManager {
    // Core assignments
    static constexpr BaseType_t NETWORK_CORE = 0;
    static constexpr BaseType_t PROCESSING_CORE = 1;

    // Timing constants
    static constexpr uint32_t MANAGER_UPDATE_FREQ_HZ = 100;
    static constexpr uint32_t MANAGER_PERIOD_MS = 1000 / MANAGER_UPDATE_FREQ_HZ;
    
    static constexpr uint32_t HEARTBEAT_FREQ_HZ = 1;
    static constexpr uint32_t HEARTBEAT_PERIOD_MS = 1000 / HEARTBEAT_FREQ_HZ;
    
    static constexpr uint32_t MQTT_UPDATE_FREQ_HZ = 10;
    static constexpr uint32_t MQTT_PERIOD_MS = 1000 / MQTT_UPDATE_FREQ_HZ;
    
    // Stack sizes
    static constexpr uint32_t MANAGER_STACK_SIZE = 8192;
    static constexpr uint32_t HEARTBEAT_STACK_SIZE = 2048;
    static constexpr uint32_t WIFI_MQTT_STACK_SIZE = 6144;
    static constexpr uint32_t ESP_NOW_STACK_SIZE = 4096;
    
    // Task priorities
    static constexpr uint32_t MANAGER_PRIORITY = 3;
    static constexpr uint32_t HEARTBEAT_PRIORITY = 7;
    static constexpr uint32_t WIFI_MQTT_PRIORITY = 2;
    static constexpr uint32_t ESP_NOW_PRIORITY = 4;
    
    struct MqttCommand {
        char topic[64];
        char payload[512];
        size_t length;
    };
    
    struct EspNowMessage {
        uint8_t data[256];
        size_t length;
        MessageType_E messageType;
        size_t targetIdx; // Controller index to send to
        bool isBroadcast; // If true, broadcast to all controllers
    };

    // Task handles - exposed for inter-task signaling
    extern TaskHandle_t managerUpdateTaskHandle;
    extern TaskHandle_t managerHeartbeatTaskHandle;
    extern TaskHandle_t wifiMqttTaskHandle;
    extern TaskHandle_t espNowTaskHandle;

    // Queue handles
    extern QueueHandle_t audioAnalysisQueue;
    extern QueueHandle_t mqttCommandQueue;
    extern QueueHandle_t espNowOutQueue;
    
    // Semaphores and mutexes
    extern SemaphoreHandle_t controllerStateMutex;
    extern SemaphoreHandle_t mqttMutex;
    extern SemaphoreHandle_t mqttPublishMutex;

    // Initialize all tasks and communication primitives
    void initializeTaskSystem(void);

    // Main tasks
    void managerUpdateTask(void* parameter);
    void managerHeartbeatTask(void* parameter);
    void wifiMqttTask(void* parameter);
    void espNowTask(void* parameter);
    
    // Task creation functions
    void createManagerUpdateTask(HW_NeonManager* pManager);
    void createManagerHeartbeatTask(HW_NeonManager* pManager);
    void createWifiMqttTask(HW_NeonManager* pManager);
    void createEspNowTask(HW_NeonManager* pManager);
    
    // Helper function to queue messages for ESP-NOW task
    bool queueEspNowMessage(const void* msgData, size_t msgSize, MessageType_E msgType, size_t idx, bool isBroadcast = false);
}