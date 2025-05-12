#include "TaskManager.h"

QueueHandle_t TaskManager::espNowOutQueue = nullptr;

SemaphoreHandle_t TaskManager::controllerStateMutex = nullptr;

TaskHandle_t TaskManager::espNowTaskHandle = nullptr;
TaskHandle_t TaskManager::heartbeatTaskHandle = nullptr;
TaskHandle_t TaskManager::controllerUpdateTaskHandle = nullptr;

void TaskManager::initializeTaskSystem()
{
    espNowOutQueue = xQueueCreate(10, sizeof(EspNowMessage));
    controllerStateMutex = xSemaphoreCreateMutex();
}

void TaskManager::createEspNowTask(HW_NeonController* controller)
{
    xTaskCreatePinnedToCore(
        espNowTask,
        "ESP-NOW",
        ESP_NOW_STACK_SIZE,
        controller,
        ESP_NOW_PRIORITY,
        &espNowTaskHandle,
        NETWORK_CORE
    );
}

void TaskManager::createHeartbeatTask(HW_NeonController* controller)
{
    xTaskCreatePinnedToCore(
        heartbeatTask,
        "Heartbeat",
        HEARTBEAT_STACK_SIZE,
        controller,
        HEARTBEAT_PRIORITY,
        &heartbeatTaskHandle,
        NETWORK_CORE
    );
}

void TaskManager::createControllerUpdateTask(HW_NeonController* controller)
{
    xTaskCreatePinnedToCore(
        controllerUpdateTask,
        "ControllerUpdate",
        CONTROLLER_STACK_SIZE,
        controller,
        CONTROLLER_UPDATE_PRIORITY,
        &controllerUpdateTaskHandle,
        PROCESSING_CORE
    );
}

void TaskManager::espNowTask(void* parameter)
{
    HW_NeonController* controller = static_cast<HW_NeonController*>(parameter);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(ESP_NOW_PERIOD_MS);
    EspNowMessage espNowMsg;
    
    for(;;)
    {        
        if (espNowOutQueue != nullptr && xQueueReceive(espNowOutQueue, &espNowMsg, 0) == pdTRUE)
        {
            controller->sendMessage(espNowMsg.messageType, espNowMsg.data, espNowMsg.length);
        }
        
        vTaskDelayUntil(&xLastWakeTime, period);
    }
}

void TaskManager::heartbeatTask(void* parameter)
{
    HW_NeonController* controller = static_cast<HW_NeonController*>(parameter);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS);
    
    for(;;)
    {
        // Take mutex before accessing controller state
        if (xSemaphoreTake(controllerStateMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            controller->queueHeartbeatMessage();
            xSemaphoreGive(controllerStateMutex);
        }
        
        vTaskDelayUntil(&xLastWakeTime, period);
    }
}

// Core 1
void TaskManager::controllerUpdateTask(void* parameter)
{
    HW_NeonController* controller = static_cast<HW_NeonController*>(parameter);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(CONTROLLER_PERIOD_MS);
    float deltaTimeSeconds = CONTROLLER_PERIOD_MS / 1000.0f;
    // Update controller state with proper mutex protection
    for(;;)
    {
        if (xSemaphoreTake(controllerStateMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            controller->update(deltaTimeSeconds);
            xSemaphoreGive(controllerStateMutex);
        }
                    
        vTaskDelayUntil(&xLastWakeTime, period);
    }
}
