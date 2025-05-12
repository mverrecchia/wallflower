#include "UE_NeonManager.h"
#include "Misc/FileHelper.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool UE_NeonManager::initialize(void)
{
    if (!m_world)
    {
        m_pUtils->logError("No valid world in initialize");
        return false;
    }
    else
    {
        UE_FFTAudioAnalyzer* ueFftAnalyzer = static_cast<UE_FFTAudioAnalyzer*>(m_pFftAnalyzer);
        ueFftAnalyzer->setWorld(m_world);
    }

    if (!m_pFftAnalyzer->begin())
    {
        m_pUtils->logError("Failed to initialize FFT analyzer");
        return false;
    }

    return true;
}

void UE_NeonManager::update(float deltaTime)
{
    NeonManager::update(deltaTime);

    for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    {
        if (m_controllers[idx])
        {
            m_controllers[idx]->update(deltaTime);
        }
    }
}

void UE_NeonManager::queueMessage(MessageType_E messageType, size_t idx, const void* msg)
{
    switch (messageType)
    {
        case MessageType_E::HEARTBEAT_M:
        {
            spoofHeartbeatMessages(idx, msg);
            break;
        }
        case MessageType_E::MANUAL:
        {
            spoofManualMessages(idx, msg);
            break;
        }
        case MessageType_E::PROFILE:
        {
            spoofProfileMessages(idx, msg);
            break;
        }
        case MessageType_E::BEZIER:
        {
            spoofBezierMessages(idx, msg);
            break;
        }
        case MessageType_E::AUDIO:
        {
            spoofAudioMessages(idx, msg);
            break;
        }
        default:
            break;
    }
}

void UE_NeonManager::spoofHeartbeatMessages(size_t idx, const void* msg)
{
    (void)idx;
    const ManagerHeartbeatMessageFull_S* heartbeatMsg = static_cast<const ManagerHeartbeatMessageFull_S*>(msg);
    for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    {
        if (m_controllers[idx])
        {
            m_controllers[idx]->handleHeartbeatMessage(heartbeatMsg);
        }
    }
}

void UE_NeonManager::spoofManualMessages(size_t idx, const void* msg)
{
    const ManualMessageFull_S* controlMsg = static_cast<const ManualMessageFull_S*>(msg);

    if (m_controllers[idx])
    {
        m_controllers[idx]->handleManualMessage(controlMsg);
    }
}

void UE_NeonManager::spoofProfileMessages(size_t idx, const void* msg)
{
    const ProfileMessageFull_S* profileMsg = static_cast<const ProfileMessageFull_S*>(msg);

    if (m_controllers[idx])
    {
        m_controllers[idx]->handleProfileMessage(profileMsg);
    }
}

void UE_NeonManager::spoofAudioMessages(size_t idx, const void* msg)
{
    (void)idx;
    const AudioMessageFull_S* audioMsg = static_cast<const AudioMessageFull_S*>(msg);
    for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    {
        if (m_controllers[idx])
        {
            m_controllers[idx]->handleAudioMessage(audioMsg);
        }
    }
}

void UE_NeonManager::spoofBezierMessages(size_t idx, const void* msg)
{
    const BezierMessageFull_S* bezierMsg = static_cast<const BezierMessageFull_S*>(msg);

    if (m_controllers[idx])
    {
        m_controllers[idx]->handleBezierMessage(bezierMsg);
    }
}

void UE_NeonManager::appendMacAddress(uint8_t* mac, size_t idx)
{
    memset(mac, 0, MAC_ADDRESS_SIZE);
}
