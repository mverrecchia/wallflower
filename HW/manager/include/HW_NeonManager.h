// HW/include/HW_NeonManager.h
#pragma once

#include "NeonManager.h"
#include "HW_FFTAudioAnalyzer.h"
#include "HW_PlatformUtils.h"
#include "MicrophoneSensor.h"
#include "PlatformTypes.h"

#include <PubSubClient.h>
#include <esp_now.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Preferences.h>

// MQTT lock topics
extern const char* MQTT_LOCK_REQUEST_TOPIC;
extern const char* MQTT_LOCK_RESPONSE_TOPIC;

class HW_NeonManager : public NeonManager {
public:
    static const unsigned long LOCK_TIMEOUT_MS = 300000; // 5-minute timeout
    
    // Device will only accept commands from clients that have acquired a lock
    // This is a security feature to prevent unauthorized control

    HW_NeonManager(PubSubClient& mqttClient)
        : NeonManager(new HW_PlatformUtils(), nullptr) // Pass nullptr initially
        , m_mqttClient(mqttClient)
        , m_microphone()
        , m_clientLocked(false)
        , m_authorizedClientId("")
        , m_lockTimestamp(0)
    {
        // Now m_pUtils is initialized and we can use it to create the analyzer
        m_pFftAnalyzer = new HW_FFTAudioAnalyzer(m_pUtils);
    }
    bool initialize() override;
    void update(float deltaTime) override;

    // ESP-NOW message handling
    void handleEspNowMessage(const uint8_t* mac, const uint8_t* data, int len);
    void handleHeartbeatMessage(const uint8_t* mac, const ControllerHeartbeatMessageFull_S* msg);
    
    // MQTT related methods
    void handleMqtt();
    void handleMqttMessage(char* topic, byte* payload, unsigned int length);
    void reconnectMQTT(void);
    void publishManagerStatus(void);
    
    // Client locking methods
    bool requestClientLock(const char* clientId);
    bool releaseClientLock(const char* clientId);
    bool isClientAuthorized(const char* clientId);
    void checkLockTimeout();
    void publishLockResponse(const char* clientId, bool success);
    
    // Sensor getters
    MicrophoneSensor& getMicrophone() { return m_microphone; }
    FFTAudioAnalyzer* getFFTAnalyzer() { return m_pFftAnalyzer; }

public:
    // Overridden from NeonManager - this will queue the message for sending via the ESP-NOW task
    virtual void queueMessage(MessageType_E messageType, size_t idx, const void* msg) override;
    
    // This method does the actual direct ESP-NOW sending 
    void sendMessage(MessageType_E messageType, size_t idx, const void* msg, uint8_t messageSize, bool isBroadcast);
private:
    esp_now_peer_info_t m_broadcastPeer;
    BroadcastType_E m_broadcastType;
    Preferences m_nonVolatileStorage;

    PubSubClient& m_mqttClient;
    MicrophoneSensor m_microphone;

    char m_managerStatusMessage[2048];

    StaticJsonDocument<2048> m_managerStatusBuffer;
    StaticJsonDocument<1024> m_profileConfigBuffer;
    StaticJsonDocument<4096> m_bezierConfigBuffer;
    StaticJsonDocument<2048> m_audioConfigBuffer;
    StaticJsonDocument<2048> m_manualConfigBuffer;
    StaticJsonDocument<256> m_lockConfigBuffer;
    
    // Client lock state
    bool m_clientLocked;
    String m_authorizedClientId;
    unsigned long m_lockTimestamp;

    bool parseProfileConfig(JsonDocument& doc);
    bool parseAudioConfig(JsonDocument& doc);
    bool parseManualConfig(JsonDocument& doc, size_t idx);

    void appendMacAddress(uint8_t* mac, size_t idx) override;

    void configureBroadcastPeer(void);
    void configureUnicastPeers(void);

    void saveAudioConfigToEEPROM(AudioConfiguration_S audioConfig);
    void loadNonVolatileSettings(void);
};
