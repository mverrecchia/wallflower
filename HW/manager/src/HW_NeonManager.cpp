// hardware/manager/src/HW_NeonManager.cpp
#include "HW_NeonManager.h"
#include "HW_PlatformUtils.h"
#include "PlatformConstants.h"
#include "DeviceAddresses.h"
#include <WiFi.h>
#include "HW_config.h"
#include "TaskManager.h"

const char* MQTT_CLIENT_ID = "wallflower_manager";
const char* MQTT_MANAGER_TOPIC_PREFIX = "wallflower/manager/status";
const char* MQTT_PROFILE_TOPIC = "wallflower/profile";
const char* MQTT_AUDIO_CONFIG_TOPIC = "wallflower/audio_config";
const char* MQTT_MANUAL_PREFIX = "wallflower/manual/";

const char* MQTT_LOCK_REQUEST_TOPIC = "wallflower/lock/request";
const char* MQTT_LOCK_RESPONSE_TOPIC = "wallflower/lock/response";

bool HW_NeonManager::initialize(void)
{
    bool success = false;

    // TODO: Remove these once we have a way to save and load settings
    // EEPROM.begin(HW::EEPROM_SIZE);
    // m_nonVolatileStorage.begin(HW::NVS_NAMESPACE, false);
    // loadNonVolatileSettings();

    // make sure FFT analyzer isn't null before calling begin
    if (m_pFftAnalyzer == nullptr)
    {
        m_pFftAnalyzer = new HW_FFTAudioAnalyzer(m_pUtils);
    }
    
    success = m_pFftAnalyzer->begin();

    pinMode(PIN_MODE, INPUT_PULLUP);

    return success;
}

void HW_NeonManager::update(float deltaTime)
{    
    checkLockTimeout();
    NeonManager::update(deltaTime);
}

void HW_NeonManager::handleMqtt()
{
    if (!m_mqttClient.connected())
    {
        reconnectMQTT();
    }
    m_mqttClient.loop();
}

void HW_NeonManager::queueMessage(MessageType_E messageType, size_t idx, const void* msg)
{
    uint8_t messageSize = 0;
    bool isBroadcast = false;
    
    switch (messageType)
    {
        case MessageType_E::HEARTBEAT_M:
        {
            messageSize = sizeof(ManagerHeartbeatMessageFull_S);
            isBroadcast = true;
            break;
        }
        case MessageType_E::MANUAL:
        {
            messageSize = sizeof(ManualMessageFull_S);
            isBroadcast = false;
            break;
        }
        case MessageType_E::PROFILE:
        {
            messageSize = sizeof(ProfileMessageFull_S);
            isBroadcast = false;
            break;
        }
        case MessageType_E::AUDIO:
        {
            messageSize = sizeof(AudioMessageFull_S);
            isBroadcast = true;
            break;
        }
        default:
            break;
    }

    if (TaskManager::espNowOutQueue != nullptr)
    {
        TaskManager::queueEspNowMessage(msg, messageSize, messageType, idx, isBroadcast);
    }
}

void HW_NeonManager::sendMessage(MessageType_E messageType, size_t idx, 
                                     const void* msg, uint8_t messageSize, bool isBroadcast)
{
    const uint8_t* targetMac;
    
    if (isBroadcast)
    {
        m_broadcastType = BroadcastType_E::BROADCAST;
        targetMac = m_broadcastPeer.peer_addr;
        configureBroadcastPeer();
    }
    else
    {
        m_broadcastType = BroadcastType_E::UNICAST;
        targetMac = DeviceAddresses::CONTROLLER_MACS[idx];
        configureUnicastPeers();
    }
    esp_err_t result = esp_now_send(targetMac, (const uint8_t*)msg, messageSize);
    if (result != ESP_OK)
    {
        if (result == ESP_ERR_ESPNOW_NOT_FOUND && !m_controllerActive[idx])
        {
            m_pUtils->logWarning("Controller disconnected");
        }

        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Error with message send: %d", result);
        m_pUtils->logError(buffer);
    }
}

void HW_NeonManager::handleEspNowMessage(const uint8_t* mac, const uint8_t* data, int len)
{
    MessageType_E messageType = static_cast<MessageType_E>(data[0]);    

    if (len >= sizeof(ControllerHeartbeatMessageFull_S))
    {
        ControllerHeartbeatMessageFull_S msg;
        memcpy(&msg, data, sizeof(ControllerHeartbeatMessageFull_S));
        handleHeartbeatMessage(mac, &msg);
    }
}

void HW_NeonManager::handleHeartbeatMessage(const uint8_t* mac, const ControllerHeartbeatMessageFull_S* msg)
{
    // Find controller index based on MAC address
    for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    {
        if (memcmp(mac, DeviceAddresses::CONTROLLER_MACS[idx], MAC_ADDRESS_SIZE) == 0)
        {
            m_controllerActive[idx] = true;
            m_controllerTimeouts[idx].start();
            for (size_t supplyIdx = 0; supplyIdx < NUM_SUPPLIES_PER_CONTROLLER; supplyIdx++)
            {
                m_controllerStateParameters[idx].supplies[supplyIdx].id = supplyIdx;
                m_controllerStateParameters[idx].supplies[supplyIdx].brightness = msg->heartbeat.supplies[supplyIdx].brightness;
                m_controllerStateParameters[idx].supplies[supplyIdx].enable = msg->heartbeat.supplies[supplyIdx].enable;
                m_controllerStateParameters[idx].supplies[supplyIdx].current = msg->heartbeat.supplies[supplyIdx].current;
            }
            m_controllerStateParameters[idx].motor = msg->heartbeat.motor;
            m_controllerStateParameters[idx].profileActive = msg->heartbeat.profileActive;
            m_controllerStateParameters[idx].distance = msg->heartbeat.distance;
        }
    }
}

bool HW_NeonManager::parseManualConfig(JsonDocument& doc, size_t idx)
{
    // runManualMode will handle the sending after m_directControlManual goes true
    m_directControlManual = true;

    // no current present since that comes from controller
    for (size_t supplyIdx = 0; supplyIdx < NUM_SUPPLIES_PER_CONTROLLER; supplyIdx++)
    {
        m_controllerRequestParameters[idx].supplies[supplyIdx].id = supplyIdx;
        m_controllerRequestParameters[idx].supplies[supplyIdx].enable = doc["supplies"][supplyIdx]["enabled"] | false;
        m_controllerRequestParameters[idx].supplies[supplyIdx].brightness = doc["supplies"][supplyIdx]["brightness"] | 0.0f;
    }
    m_controllerRequestParameters[idx].motor.enable = doc["motorEnable"] | false;
    m_controllerRequestParameters[idx].motor.enable = doc["motorEnable"] | false;
    m_controllerRequestParameters[idx].motor.direction = doc["motorDirection"] | false;
    m_controllerRequestParameters[idx].motor.speed = doc["motorSpeed"] | 0.0f;

    return true;
}

bool HW_NeonManager::parseProfileConfig(JsonDocument& doc)
{
    m_directControlProfile = true;

    for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    {
        // int to string key
        char key[4]; 
        snprintf(key, sizeof(key), "%zu", idx);
        
        if (doc.containsKey(key))
        {
            JsonObject controller = doc[key];
            if (controller)
            {
                uint8_t index = controller["index"] | 0;
                m_profileRequestParameters[index].type = static_cast<ProfileType_E>(controller["profileType"] | 0);
                m_profileRequestParameters[index].magnitude = controller["magnitude"] | 0.0f;
                m_profileRequestParameters[index].frequency = controller["frequency"] | 1.0f;
                m_profileRequestParameters[index].phase = controller["phase"] | 0.0f;
                m_profileRequestParameters[index].enable = controller["enable"] | false;
                m_profileRequestParameters[index].stopProfile = controller["stopProfile"] | false;
            }
        }
    }

    return true;
}

bool HW_NeonManager::parseAudioConfig(JsonDocument& doc)
{
    // Check for client ID in the document
    const char* clientId = doc["clientId"] | "";
    
    if (!isClientAuthorized(clientId))
    {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Audio config rejected: Unauthorized client %s", clientId);
        m_pUtils->logWarning(buffer);
        return false;
    }

    AudioAssignmentMode_E mode = AudioAssignmentMode_E::FIXED;
    FrequencyBand_E frequencyFlags[NUM_CONTROLLERS][NUM_SUPPLIES_PER_CONTROLLER];
    float magnitudeThresholds[NUM_AUDIO_BUCKETS];
    float lowFrequencyWeights[NUM_LOW_BINS];
    float midFrequencyWeights[NUM_MID_BINS];
    float highFrequencyWeights[NUM_HIGH_BINS];

    // TODO: Remove this once we have a way to save and load settings 
    // const char* modeStr = doc["audioMode"];
    // bool allowMultipleActive = doc["audioAllowMultipleActive"] | false;

    // if (memcmp(modeStr, "fixed", 5) == 0)
    // {
    //     mode = AudioAssignmentMode_E::FIXED;
    // }
    // else if (memcmp(modeStr, "random", 6) == 0)
    // {
    //     mode = AudioAssignmentMode_E::RANDOM;
    // }
    // else if (memcmp(modeStr, "sequential", 10) == 0)
    // {
    //     mode = AudioAssignmentMode_E::SEQUENTIAL;
    // }
    // else
    // {
    //     m_pUtils->logError("Invalid audio mode");
    //     return false;
    // }

    JsonArray thresholds = doc["audioMagnitudeThresholds"];

    for (size_t idx = 0; idx < NUM_AUDIO_BUCKETS; idx++)
    {
        magnitudeThresholds[idx] = thresholds[idx];
    }

    JsonArray supplyFlags = doc["audioSupplyFlags"];

    for (size_t controllerIdx = 0; controllerIdx < NUM_CONTROLLERS; controllerIdx++)
    {
        for (size_t supplyIdx = 0; supplyIdx < NUM_SUPPLIES_PER_CONTROLLER; supplyIdx++)
        {
            frequencyFlags[controllerIdx][supplyIdx] = supplyFlags[controllerIdx][supplyIdx];
        }
    }

    JsonObject weights = doc["audioWeights"];
    JsonArray lowWeights = weights["low"];
    JsonArray midWeights = weights["mid"];
    JsonArray highWeights = weights["high"];

    if (lowWeights && lowWeights.size() == NUM_LOW_BINS)
    {
        for (size_t i = 0; i < NUM_LOW_BINS; i++)
        {
            lowFrequencyWeights[i] = lowWeights[i];
        }
    }

    if (midWeights && midWeights.size() == NUM_MID_BINS)
    {
        for (size_t i = 0; i < NUM_MID_BINS; i++)
        {
            midFrequencyWeights[i] = midWeights[i];
        }
    }

    if (highWeights && highWeights.size() == NUM_HIGH_BINS)
    {
        for (size_t i = 0; i < NUM_HIGH_BINS; i++)
        {
            highFrequencyWeights[i] = highWeights[i];
        }
    }

    AudioConfiguration_S audioConfig;
    audioConfig.mode = mode;
    // audioConfig.allowMultipleActive = allowMultipleActive;
    
    // Copy arrays properly
    memcpy(audioConfig.frequencyFlags, frequencyFlags, sizeof(frequencyFlags));
    memcpy(audioConfig.lowFrequencyWeights, lowFrequencyWeights, sizeof(lowFrequencyWeights));
    memcpy(audioConfig.midFrequencyWeights, midFrequencyWeights, sizeof(midFrequencyWeights));
    memcpy(audioConfig.highFrequencyWeights, highFrequencyWeights, sizeof(highFrequencyWeights));
    memcpy(audioConfig.magnitudeThresholds, magnitudeThresholds, sizeof(magnitudeThresholds));
    
    if (m_pAudioOrchestrator)
    {
       setAudioConfig(audioConfig);
    }

    float fastAlpha = doc["audioFastAlpha"];
    float slowAlpha = doc["audioSlowAlpha"];

    if (m_pFftAnalyzer)
    {
        m_pFftAnalyzer->setFastAlpha(fastAlpha);
        m_pFftAnalyzer->setSlowAlpha(slowAlpha);
    }
    return true;
}

void HW_NeonManager::appendMacAddress(uint8_t* mac, size_t idx)
{
    memcpy(mac, DeviceAddresses::CONTROLLER_MACS[idx], MAC_ADDRESS_SIZE);
}

void HW_NeonManager::configureBroadcastPeer(void)
{
    for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    {
        esp_now_del_peer(DeviceAddresses::CONTROLLER_MACS[idx]);
    }

    memset(&m_broadcastPeer, 0, sizeof(m_broadcastPeer));
    uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(m_broadcastPeer.peer_addr, broadcastMac, MAC_ADDRESS_SIZE);
    m_broadcastPeer.channel = 10;
    m_broadcastPeer.encrypt = false;
    esp_now_add_peer(&m_broadcastPeer);
}

void HW_NeonManager::configureUnicastPeers(void)
{
    esp_now_del_peer(m_broadcastPeer.peer_addr);
    
    for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, DeviceAddresses::CONTROLLER_MACS[idx], MAC_ADDRESS_SIZE);
        peerInfo.channel = 10;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }
}

void HW_NeonManager::publishManagerStatus(void)
{
    m_managerStatusBuffer.clear();
    m_managerStatusBuffer["type"] = "manager_status";
    
    m_managerStatusBuffer["status"] = "online";
    m_managerStatusBuffer["connectedControllers"] = getConnectedControllerCount();
    m_managerStatusBuffer["expectedControllers"] = getExpectedControllerCount();
    m_managerStatusBuffer["state"] = static_cast<int>(getManagerState());
    
    m_managerStatusBuffer["locked"] = m_clientLocked;
    if (m_clientLocked)
    {
        m_managerStatusBuffer["lockedBy"] = m_authorizedClientId;
        m_managerStatusBuffer["lockTimeRemaining"] = (LOCK_TIMEOUT_MS - (millis() - m_lockTimestamp)) / 1000;
    }
    
    // Add controller statuses
    JsonArray controllers = m_managerStatusBuffer.createNestedArray("controllers");
    for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    {
        JsonObject controller = controllers.createNestedObject();
        controller["connected"] = m_controllerActive[idx];
        
        if (m_controllerActive[idx])
        {
            char macStr[18];
            const uint8_t* mac = DeviceAddresses::CONTROLLER_MACS[idx];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            controller["mac"] = macStr;
            for (size_t supplyIdx = 0; supplyIdx < NUM_SUPPLIES_PER_CONTROLLER; supplyIdx++ )
            {
                JsonObject supply = controller.createNestedObject("supply");
                supply["id"] = m_controllerStateParameters[idx].supplies[supplyIdx].id;
                supply["enable"] = m_controllerStateParameters[idx].supplies[supplyIdx].enable;
                supply["brightness"] = m_controllerStateParameters[idx].supplies[supplyIdx].brightness;
                supply["current"] = m_controllerStateParameters[idx].supplies[supplyIdx].current;
            }
            controller["motorEnable"] = m_controllerStateParameters[idx].motor.enable;
            controller["motorDirection"] = m_controllerStateParameters[idx].motor.direction;
            controller["motorSpeed"] = m_controllerStateParameters[idx].motor.speed;
            controller["distance"] = m_controllerStateParameters[idx].distance;
            controller["profileActive"] = m_controllerStateParameters[idx].profileActive;
        }
        else
        {
            controller["mac"] = "00:00:00:00:00:00";
            for (size_t supplyIdx = 0; supplyIdx < NUM_SUPPLIES_PER_CONTROLLER; supplyIdx++ )
            {
                JsonObject supply = controller.createNestedObject("supply");
                supply["id"] = 0;
                supply["enable"] = false;
                supply["brightness"] = 0.0f;
                supply["current"] = 0.0f;
            }
            controller["motorEnable"] = false;
            controller["motorDirection"] = false;
            controller["motorSpeed"] = 0.0f;
            controller["distance"] = 0.0f;
            controller["profileActive"] = false;
        }
    }
    serializeJson(m_managerStatusBuffer, m_managerStatusMessage);

    if (!m_mqttClient.publish(MQTT_MANAGER_TOPIC_PREFIX, m_managerStatusMessage))
    {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Failed to publish manager status: %d", m_mqttClient.state());
        m_pUtils->logError(buffer);
    }
}


void HW_NeonManager::reconnectMQTT(void)
{
    while (!m_mqttClient.connected()) 
    {
        // Will message is set when connecting
        // Include will message for status tracking
        const char* willTopic = MQTT_MANAGER_TOPIC_PREFIX;
        const char* willMessage = "{\"status\":\"offline\"}";
        if (m_mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, 
                               willTopic, 1, true, willMessage))
        {
            m_pUtils->logWarning("Manager connected to MQTT broker");
            
            if (!m_mqttClient.subscribe(MQTT_PROFILE_TOPIC))
            {
                m_pUtils->logWarning("Failed to subscribe to profile topic");
            }
            if (!m_mqttClient.subscribe(MQTT_AUDIO_CONFIG_TOPIC))
            {
                m_pUtils->logWarning("Failed to subscribe to audio_config topic");
            }
            if (!m_mqttClient.subscribe(MQTT_LOCK_REQUEST_TOPIC))
            {
                m_pUtils->logWarning("Failed to subscribe to lock request topic");
            }
            
            char manualTopic[50];
            for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
            {
                snprintf(manualTopic, sizeof(manualTopic), "%s%d", MQTT_MANUAL_PREFIX, idx);
                if(!m_mqttClient.subscribe(manualTopic))
                {
                    m_pUtils->logWarning("Failed to subscribe to manual topic");
                }
            }
            
            publishManagerStatus();
        }
        else
        {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "failed, rc=%d", m_mqttClient.state());
            m_pUtils->logWarning(buffer);
            m_pUtils->logWarning(" try again in 5 seconds");
            delay(5000);
        }
    }
}
void HW_NeonManager::handleMqttMessage(char* topic, byte* payload, unsigned int length)
{
    char* message = new char[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
        
    if (strcmp(topic, MQTT_PROFILE_TOPIC) == 0)
    {
        m_profileConfigBuffer.clear();
        deserializeJson(m_profileConfigBuffer, message);

        const char* clientId = m_manualConfigBuffer["clientId"] | "";
        
        if (!isClientAuthorized(clientId))
        {
            char buffer[128];
            snprintf(buffer, sizeof(buffer), "Profile command rejected: Unauthorized client %s", clientId);
            m_pUtils->logWarning(buffer);
        }
        else if (!parseProfileConfig(m_profileConfigBuffer))
        {
            m_pUtils->logWarning("Failed to parse profile config");
        }
    }
    else if (strcmp(topic, MQTT_AUDIO_CONFIG_TOPIC) == 0)
    {
        m_audioConfigBuffer.clear();
        deserializeJson(m_audioConfigBuffer, message);
        
        if (!parseAudioConfig(m_audioConfigBuffer))
        {
            m_pUtils->logWarning("Failed to parse audio config");
        }
    }
    else if (strcmp(topic, MQTT_LOCK_REQUEST_TOPIC) == 0)
    {
        m_lockConfigBuffer.clear();
        deserializeJson(m_lockConfigBuffer, message);
        
        const char* action = m_lockConfigBuffer["action"];
        const char* clientId = m_lockConfigBuffer["clientId"];
        
        // always process lock requests regardless of current lock state
        if (strcmp(action, "lock") == 0)
        {
            bool success = requestClientLock(clientId);
            publishLockResponse(clientId, success);
        } 
        else if (strcmp(action, "unlock") == 0)
        {
            if (m_clientLocked && m_authorizedClientId == clientId)
            {
                bool success = releaseClientLock(clientId);
                publishLockResponse(clientId, success);
            }
            else
            {
                publishLockResponse(clientId, false);
            }
        }
        else if (strcmp(action, "heartbeat") == 0)
        {
            if (m_clientLocked && m_authorizedClientId == clientId)
            {
                m_lockTimestamp = millis();
                publishLockResponse(clientId, true);
            }
        }
    }
    else if (strstr(topic, MQTT_MANUAL_PREFIX) != NULL)
    {
        char* controllerIdStr = topic + strlen(MQTT_MANUAL_PREFIX);
        size_t controllerId = atoi(controllerIdStr);

        m_manualConfigBuffer.clear();
        deserializeJson(m_manualConfigBuffer, message);
        
        const char* clientId = m_manualConfigBuffer["clientId"] | "";
        
        if (!isClientAuthorized(clientId))
        {
            char buffer[128];
            snprintf(buffer, sizeof(buffer), "Manual command rejected: Unauthorized client %s", clientId);
            m_pUtils->logWarning(buffer);
        }
        else if (!parseManualConfig(m_manualConfigBuffer, controllerId))
        {
            m_pUtils->logWarning("Failed to parse manual config");
        }
    }
    
    delete[] message;
}

void HW_NeonManager::saveAudioConfigToEEPROM(AudioConfiguration_S audioConfig)
{
    // m_nonVolatileStorage.putUChar("audio_mode", static_cast<uint8_t>(mode));
    // m_nonVolatileStorage.putBool("multi_active", allowMultipleActive);
    // m_nonVolatileStorage.putFloat("fast_alpha", fastAlpha);
    // m_nonVolatileStorage.putFloat("slow_alpha", slowAlpha);
    
    // // Save controller configs
    // for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    // {
    //     char keyFreq[16];
    //     char keyThresh[16];
        
    //     snprintf(keyFreq, sizeof(keyFreq), "freq_flag_%d", idx);
    //     snprintf(keyThresh, sizeof(keyThresh), "mag_thresh_%d", idx);
        
    //     m_nonVolatileStorage.putUChar(keyFreq, static_cast<uint8_t>(frequencyFlags[idx]));
    // }
    // for (size_t idx = 0; idx < NUM_AUDIO_BUCKETS; idx++)
    // {
    //     char keyThresh[16];
    //     snprintf(keyThresh, sizeof(keyThresh), "mag_thresh_%d", idx);
    //     m_nonVolatileStorage.putFloat(keyThresh, magnitudeThresholds[idx]);
    // }
    
    // m_pUtils->logWarning("Audio configuration saved to EEPROM");
}

void HW_NeonManager::loadNonVolatileSettings(void)
{
    // Check if we have saved audio settings
    // if (m_nonVolatileStorage.isKey("audio_mode"))
    // {
    //     m_pUtils->logWarning("Loading audio configuration from EEPROM");
        
    //     AudioAssignmentMode_E mode = static_cast<AudioAssignmentMode_E>(
    //         m_nonVolatileStorage.getUChar("audio_mode", static_cast<uint8_t>(AudioAssignmentMode_E::FIXED)));
        
    //     bool allowMultipleActive = m_nonVolatileStorage.getBool("multi_active", false);
    //     float fastAlpha = m_nonVolatileStorage.getFloat("fast_alpha", 0.9f);
    //     float slowAlpha = m_nonVolatileStorage.getFloat("slow_alpha", 0.2f);
        
    //     // Load controller configs
    //     FrequencyBand_E frequencyFlags[NUM_CONTROLLERS][NUM_SUPPLIES_PER_CONTROLLER];
    //     for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    //     {
    //         char keyFreq[16];
    //         snprintf(keyFreq, sizeof(keyFreq), "freq_flag_%d", idx);
    //         frequencyFlags[idx] = static_cast<FrequencyBand_E>(
    //             m_nonVolatileStorage.getUChar(keyFreq, static_cast<uint8_t>(FrequencyBand_E::MID_FREQ)));
            
    //     }

    //     float magnitudeThresholds[NUM_AUDIO_BUCKETS];
    //     for (size_t idx = 0; idx < NUM_AUDIO_BUCKETS; idx++)
    //     {
    //         char keyThresh[16];
    //         snprintf(keyThresh, sizeof(keyThresh), "mag_thresh_%d", idx);
    //         magnitudeThresholds[idx] = m_nonVolatileStorage.getFloat(keyThresh, 0.25f);
    //     }

    //     AudioConfiguration_S audioConfig = {
    //         .mode = mode,
    //         .allowMultipleActive = allowMultipleActive,
    //         .frequencyFlags = {frequencyFlags},
    //         .lowFrequencyWeights = {0.2},
    //         .midFrequencyWeights = {0.2},
    //         .highFrequencyWeights = {0.2},
    //         .magnitudeThresholds = {magnitudeThresholds}
    //     };
        
    //     if (m_pFftAnalyzer)
    //     {
    //         m_pFftAnalyzer->setFastAlpha(fastAlpha);
    //         m_pFftAnalyzer->setSlowAlpha(slowAlpha);
    //     }
        
    //     if (m_pAudioOrchestrator)
    //     {
    //         setAudioConfig(audioConfig);
    //     }
    // }
    // else
    // {
    //     m_pUtils->logWarning("No saved audio configuration found");
    // }
}

bool HW_NeonManager::requestClientLock(const char* clientId)
{
    if (m_clientLocked && m_authorizedClientId == clientId)
    {
        m_lockTimestamp = millis();
        return true;
    }
    
    if (!m_clientLocked || (millis() - m_lockTimestamp > LOCK_TIMEOUT_MS))
    {
        m_clientLocked = true;
        m_authorizedClientId = clientId;
        m_lockTimestamp = millis();
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Device locked by client: %s", clientId);
        m_pUtils->logWarning(buffer);
        return true;
    }
    
    return false;
}

bool HW_NeonManager::releaseClientLock(const char* clientId)
{
    if (m_clientLocked && m_authorizedClientId == clientId)
    {
        m_clientLocked = false;
        m_authorizedClientId = "";
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Device unlocked by client: %s", clientId);
        m_pUtils->logWarning(buffer);
        return true;
    }
    return false;
}

bool HW_NeonManager::isClientAuthorized(const char* clientId)
{
    if (clientId == nullptr || strlen(clientId) == 0)
    {
        return false;
    }
    
    if (m_clientLocked)
    {
        return (m_authorizedClientId == clientId);
    }
    else
    {
        return false;
    }
}

void HW_NeonManager::checkLockTimeout(void)
{
    if (m_clientLocked && (millis() - m_lockTimestamp > LOCK_TIMEOUT_MS))
    {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Client lock timed out for: %s", m_authorizedClientId.c_str());
        m_pUtils->logWarning(buffer);
        m_clientLocked = false;
        m_authorizedClientId = "";
    }
}

void HW_NeonManager::publishLockResponse(const char* clientId, bool success)
{
    DynamicJsonDocument response(256);
    response["clientId"] = clientId;
    response["success"] = success;
    response["locked"] = m_clientLocked;
    
    if (m_clientLocked)
    {
        response["lockedBy"] = m_authorizedClientId;
        response["timeRemaining"] = (LOCK_TIMEOUT_MS - (millis() - m_lockTimestamp)) / 1000;
    }
    
    char responseBuffer[256];
    serializeJson(response, responseBuffer);
    
    m_mqttClient.publish(MQTT_LOCK_RESPONSE_TOPIC, responseBuffer);
}
