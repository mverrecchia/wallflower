// hardware/controller/src/main.cpp
#include <Arduino.h>
#include <WiFi.h>
#include "HW_NeonController.h"
#include "DeviceAddresses.h"
#include "TaskManager.h"

HW_NeonController* pController = nullptr;

void espNowCallback(const uint8_t* mac, const uint8_t* data, int len)
{
    if (pController && !pController->getManagerRegistered())
    {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, mac, 6);
        if (esp_now_add_peer(&peerInfo) == ESP_OK)
        {
            if (Serial)
            {
                Serial.println("CONTROLLER: Manager registered");
            }
            pController->setManagerMac(mac);
        }
    }
    pController->handleEspNowMessage(data, len);
}

void setup()
{
    Serial.begin(SERIAL_BAUD);
    Serial.println("NeonRotator Controller starting up...");

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(10, WIFI_SECOND_CHAN_NONE);
    uint8_t macAddress[MAC_ADDRESS_SIZE];
    esp_wifi_get_mac(WIFI_IF_STA, macAddress);

    delay(1000);
    static HW_NeonController controller;
    pController = &controller;

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Failed to initialize ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(espNowCallback);

    if (!pController->initialize())
    {
        Serial.println("Failed to initialize HW Controller");
        return;
    }

    pController->setMacAddress(macAddress);

    // Initialize the task system
    TaskManager::initializeTaskSystem();
    
    // Create and start tasks on Core 0 (Network)
    TaskManager::createEspNowTask(pController);
    TaskManager::createHeartbeatTask(pController);

    // create the controller update task on core 1 (processing)
    TaskManager::createControllerUpdateTask(pController);
    
    Serial.println("All tasks started, controller ready");
}

void loop()
{
    // All work is now done in tasks
    // This loop can be used for watchdog or very low priority background tasks
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}