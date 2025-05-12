#include "TaskManager.h"
#include "HW_FFTAudioAnalyzer.h"
#include "MicrophoneSensor.h"
#include "PlatformConstants.h"

namespace TaskManager {
    // Task handles
    TaskHandle_t managerUpdateTaskHandle = nullptr;
    TaskHandle_t managerHeartbeatTaskHandle = nullptr;
    TaskHandle_t wifiMqttTaskHandle = nullptr;
    TaskHandle_t espNowTaskHandle = nullptr;

    // Queue handles
    QueueHandle_t mqttCommandQueue = nullptr;
    QueueHandle_t espNowOutQueue = nullptr;
    
    // Semaphores and mutexes
    SemaphoreHandle_t controllerStateMutex = nullptr;
    SemaphoreHandle_t mqttMutex = nullptr;
    SemaphoreHandle_t mqttPublishMutex = nullptr;
    

    void initializeTaskSystem(void)
    {
        mqttCommandQueue = xQueueCreate(10, sizeof(MqttCommand));
        espNowOutQueue = xQueueCreate(10, sizeof(EspNowMessage));
        
        
        controllerStateMutex = xSemaphoreCreateMutex();
        mqttMutex = xSemaphoreCreateMutex();
        mqttPublishMutex = xSemaphoreCreateMutex();
    }

    // Core 0
    void createWifiMqttTask(HW_NeonManager* pManager)
    {
        xTaskCreatePinnedToCore(
            wifiMqttTask,
            "WifiMqttTask",
            WIFI_MQTT_STACK_SIZE,
            pManager,
            WIFI_MQTT_PRIORITY,
            &wifiMqttTaskHandle,
            NETWORK_CORE
        );
    }
    
    void createEspNowTask(HW_NeonManager* pManager)
    {
        xTaskCreatePinnedToCore(
            espNowTask,
            "EspNowTask",
            ESP_NOW_STACK_SIZE,
            pManager,
            ESP_NOW_PRIORITY,
            &espNowTaskHandle,
            NETWORK_CORE
        );
    }
    
    // Core 1
    void createManagerUpdateTask(HW_NeonManager* pManager)
    {
        xTaskCreatePinnedToCore(
            managerUpdateTask,
            "ManagerTask",
            MANAGER_STACK_SIZE,
            pManager,
            MANAGER_PRIORITY,
            &managerUpdateTaskHandle,
            PROCESSING_CORE
        );
    }

    void createManagerHeartbeatTask(HW_NeonManager* pManager)
    {
        xTaskCreatePinnedToCore(
            managerHeartbeatTask,
            "HeartbeatTask",
            HEARTBEAT_STACK_SIZE,
            pManager,
            HEARTBEAT_PRIORITY,
            &managerHeartbeatTaskHandle,
            PROCESSING_CORE
        );
    }
    // Task implementations
    void managerUpdateTask(void* parameter)
    {
        HW_NeonManager* manager = static_cast<HW_NeonManager*>(parameter);
        TickType_t xLastWakeTime = xTaskGetTickCount();
        const TickType_t period = pdMS_TO_TICKS(MANAGER_PERIOD_MS);
        float deltaTimeSeconds = MANAGER_PERIOD_MS / 1000.0f;

        for(;;)
        {   
            // Update manager state with proper mutex protection
            if (xSemaphoreTake(controllerStateMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                manager->update(deltaTimeSeconds);
                xSemaphoreGive(controllerStateMutex);
            }
            
            vTaskDelayUntil(&xLastWakeTime, period);
        }
    }
    
    void managerHeartbeatTask(void* parameter)
    {
        HW_NeonManager* manager = static_cast<HW_NeonManager*>(parameter);
        TickType_t xLastWakeTime = xTaskGetTickCount();
        const TickType_t period = pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS);
        
        for(;;)
        {
            if (xSemaphoreTake(controllerStateMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                manager->queueHeartbeatMessage();
                xSemaphoreGive(controllerStateMutex);
            }
            
            vTaskDelayUntil(&xLastWakeTime, period);
        }
    }

    void wifiMqttTask(void* parameter)
    {
        HW_NeonManager* manager = static_cast<HW_NeonManager*>(parameter);
        TickType_t xLastWakeTime = xTaskGetTickCount();
        const TickType_t period = pdMS_TO_TICKS(MQTT_PERIOD_MS);
        MqttCommand mqttCmd;
        
        TickType_t lastStatusTime = 0;
        const TickType_t STATUS_PUBLISH_PERIOD = pdMS_TO_TICKS(1000);
        
        for(;;)
        {
            if (WiFi.status() != WL_CONNECTED)
            {
                WiFi.reconnect();
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            
            if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                manager->handleMqtt();
                xSemaphoreGive(mqttMutex);
            }

            TickType_t currentTime = xTaskGetTickCount();
            if ((currentTime - lastStatusTime) >= STATUS_PUBLISH_PERIOD)
            {
                lastStatusTime = currentTime;
                
                if (xSemaphoreTake(mqttPublishMutex, pdMS_TO_TICKS(10)) == pdTRUE)
                {
                    manager->publishManagerStatus();
                    xSemaphoreGive(mqttPublishMutex);
                }
            }
            
            vTaskDelayUntil(&xLastWakeTime, period);
        }
    }
    
    void espNowTask(void* parameter)
    {
        HW_NeonManager* manager = static_cast<HW_NeonManager*>(parameter);
        EspNowMessage outgoingMsg;
        
        for(;;)
        {
            if (xQueueReceive(espNowOutQueue, &outgoingMsg, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                uint8_t messageSize = outgoingMsg.length;
                manager->sendMessage(
                    outgoingMsg.messageType,
                    outgoingMsg.targetIdx,
                    outgoingMsg.data,
                    messageSize,
                    outgoingMsg.isBroadcast
                );
            }
            // Small delay to prevent CPU hogging
            // vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    bool queueEspNowMessage(const void* msgData, size_t msgSize, MessageType_E msgType, size_t idx, bool isBroadcast)
    {
        if (!espNowOutQueue)
        {
            return false;
        }
        
        EspNowMessage espNowMsg;
        
        if (msgSize > sizeof(espNowMsg.data))
        {
            Serial.println("ESP-NOW message too large for queue");
            return false;
        }
        
        memcpy(espNowMsg.data, msgData, msgSize);
        espNowMsg.length = msgSize;
        espNowMsg.messageType = msgType;
        espNowMsg.targetIdx = idx;
        espNowMsg.isBroadcast = isBroadcast;
        
        return xQueueSend(espNowOutQueue, &espNowMsg, pdMS_TO_TICKS(10)) == pdPASS;
    }
}