// hardware/manager/src/main.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "HW_NeonManager.h"
#include "DeviceAddresses.h"
#include "TaskManager.h"

#include "HW_config.h"

HW_NeonManager* pManager = nullptr;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void espNowCallback(const uint8_t* mac, const uint8_t* data, int len)
{
    pManager->handleEspNowMessage(mac, data, len);
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    pManager->handleMqttMessage(topic, payload, length);
}

void setup()
{   
    Serial.begin(SERIAL_BAUD);
    Serial.println("NeonRotator Manager starting up...");

    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);

    delay(1000);

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Failed to initialize ESP-NOW");
    }

    esp_now_register_recv_cb(espNowCallback);

    static HW_NeonManager manager(mqttClient);
    pManager = &manager;
    
    if (!pManager->initialize())
    {
        Serial.println("Failed to initialize HW Manager");
    }

    // Initialize task communication system
    TaskManager::initializeTaskSystem();
    
    // Core 0 - Network tasks
    TaskManager::createManagerHeartbeatTask(pManager);
    TaskManager::createWifiMqttTask(pManager);
    TaskManager::createEspNowTask(pManager);

    // Core 1 - Processing tasks
    TaskManager::createManagerUpdateTask(pManager);
    
    Serial.println("All tasks started, manager ready");
}

void loop()
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}