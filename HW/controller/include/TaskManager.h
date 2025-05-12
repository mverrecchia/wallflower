// TaskManager.h
#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "HW_NeonController.h"
#include "DistanceSensor.h"
#include "ArduinoOTA.h"

class TaskManager {
public:
    // Core assignments
    static constexpr BaseType_t NETWORK_CORE = 0;
    static constexpr BaseType_t PROCESSING_CORE = 1;

    // Task priorities
    static constexpr uint8_t ESP_NOW_PRIORITY = 2;
    static constexpr uint8_t HEARTBEAT_PRIORITY = 2;
    static constexpr uint8_t CONTROLLER_UPDATE_PRIORITY = 1;

    // Timing constants
    static constexpr uint32_t CONTROLLER_UPDATE_FREQ_HZ = 100;
    static constexpr uint32_t CONTROLLER_PERIOD_MS = 1000 / CONTROLLER_UPDATE_FREQ_HZ;

    static constexpr uint32_t ESP_NOW_FREQ_HZ = 100;
    static constexpr uint32_t ESP_NOW_PERIOD_MS = 1000 / ESP_NOW_FREQ_HZ;
    
    static constexpr uint32_t HEARTBEAT_FREQ_HZ = 1;
    static constexpr uint32_t HEARTBEAT_PERIOD_MS = 1000 / HEARTBEAT_FREQ_HZ;

    // Stack sizes
    static constexpr uint32_t ESP_NOW_STACK_SIZE = 3072;
    static constexpr uint32_t HEARTBEAT_STACK_SIZE = 2048;
    static constexpr uint32_t CONTROLLER_STACK_SIZE = 4096;
    
    struct EspNowMessage {
        uint8_t data[256];
        uint8_t length;
        MessageType_E messageType;
    };

    // Global queues, tasks and synchronization objects
    static QueueHandle_t espNowOutQueue; // New queue for ESP-NOW outgoing messages
    
    static SemaphoreHandle_t controllerStateMutex;
    
    static EventGroupHandle_t controllerEventGroup;
    
    static TaskHandle_t espNowTaskHandle;
    static TaskHandle_t heartbeatTaskHandle;
    static TaskHandle_t controllerUpdateTaskHandle;

    // System initialization
    static void initializeTaskSystem();
    
    // Task creation methods
    static void createEspNowTask(HW_NeonController* controller);
    static void createHeartbeatTask(HW_NeonController* controller);
    static void createControllerUpdateTask(HW_NeonController* controller);
    
private:
    // Task functions
    static void espNowTask(void* parameter);
    static void heartbeatTask(void* parameter);
    static void controllerUpdateTask(void* parameter);

};