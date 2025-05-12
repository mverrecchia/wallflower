// shared/src/NeonManager.cpp
#include "NeonManager.h"
#include <cstddef>
#include <cstring>

void NeonManager::updateInputFlags(float deltaTime)
{
    ManagerState_E managerState = getManagerState();
    bool controllerPaired = getControllerPaired();

    manageDirectControlFlags();

    if (m_audioReactivityEnabled && m_pAudioOrchestrator->getPulseReady())
    {
        m_directControlAudio = true;
        m_audioTimer.reset();
    }
    else
    {
        if (m_audioTimer.hasElapsed(AUDIO_REACTIVITY_TIMEOUT))
        {
            m_directControlAudio = false;
        }
        else 
        {
            m_audioTimer.update(deltaTime);
        }
    }
}

void NeonManager::update(float deltaTime)
{
    updateControllerTimeouts(deltaTime);
    updateManagerState(deltaTime);
    updateInputFlags(deltaTime);

    if (m_audioReactivityEnabled)
    {
        updateAudioListening(deltaTime);
    }

    if (m_directControlManual)
    {
        runManualMode(deltaTime);
    }
    else if (m_directControlAudio)
    {
        runAudioMode(deltaTime);
    }
    else if (m_directControlProfile)
    {
        runProfileMode(deltaTime);
    }
    else
    {
        // TODO: Run default mode, for now nothing
    }
}

void NeonManager::queueHeartbeatMessage(void)
{
    ManagerHeartbeatMessageFull_S msg;

    msg.heartbeat.networkStatus = 0;
    msg.heartbeat.expectedControllerCount = getExpectedControllerCount();
    msg.heartbeat.totalControllerCount = getConnectedControllerCount();

    queueMessage(msg.header, 0x00, &msg);
}

void NeonManager::queueManualMessage(size_t idx)
{
    ManualMessageFull_S msg;
    memcpy(msg.manual.supplies, m_controllerRequestParameters[idx].supplies, 
           sizeof(SupplyParameters_S) * NUM_SUPPLIES_PER_CONTROLLER);
    msg.manual.motor = m_controllerRequestParameters[idx].motor;
    
    queueMessage(msg.header, idx, &msg);
}

void NeonManager::queueProfileMessage(size_t idx)
{
    ProfileMessageFull_S msg;
    msg.profileParams.type =      m_profileRequestParameters[idx].type;
    msg.profileParams.magnitude = m_profileRequestParameters[idx].magnitude;
    msg.profileParams.frequency = m_profileRequestParameters[idx].frequency;
    msg.profileParams.phase =     m_profileRequestParameters[idx].phase;
    msg.profileParams.enable =    m_profileRequestParameters[idx].enable;
    msg.profileParams.stopProfile = m_profileRequestParameters[idx].stopProfile;
    
    queueMessage(msg.header, idx, &msg);
}

void NeonManager::queueAudioMessage(void)
{
    AudioMessageFull_S msg;
    
    const AudioMessage_S* audioMessages = m_pAudioOrchestrator->getAudioMessages();
    for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    {
        msg.audio[idx] = audioMessages[idx];
        appendMacAddress(msg.audio[idx].controllerMac, idx);
    }

    // For audio messages, use 0x00 index as these are broadcast messages
    queueMessage(msg.header, 0x00, &msg);
}

void NeonManager::updateManagerState(float deltaTime)
{
    m_stateTimer.update(deltaTime);
    size_t expectedControllerCount = getExpectedControllerCount();
    size_t connectedControllerCount = getConnectedControllerCount();
    ManagerState_E managerState = getManagerState();

    switch (managerState)
    {
        case ManagerState_E::IDLE:
            if (connectedControllerCount == 0)
            {
                setManagerState(ManagerState_E::PAIRING);
                m_stateTimer.start();
            }
            break;

        case ManagerState_E::PAIRING:
            if (connectedControllerCount == expectedControllerCount)
            {
                setManagerState(ManagerState_E::ACTIVE_FULL);
            }
            else if (connectedControllerCount > 0 && m_stateTimer.hasElapsed(CONNECTION_TIMEOUT))
            {
                setManagerState(ManagerState_E::ACTIVE_PARTIAL);
            }
            break;

        case ManagerState_E::ACTIVE_PARTIAL:
            m_stateTimer.stop();
            if (connectedControllerCount == expectedControllerCount)
            {
                setManagerState(ManagerState_E::ACTIVE_FULL);
            }
            else if (connectedControllerCount == 0)
            {
                setManagerState(ManagerState_E::PAIRING);
                m_stateTimer.start();
            }
            break;

        case ManagerState_E::ACTIVE_FULL:
            m_stateTimer.stop();
            if (connectedControllerCount < expectedControllerCount)
            {
                setManagerState(ManagerState_E::ACTIVE_PARTIAL);
            }
            break;
    }
}

void NeonManager::updateControllerTimeouts(float deltaTime)
{
    size_t connectedCount = 0;
    
    for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    {
        if (m_controllerActive[idx])
        {
            m_controllerTimeouts[idx].update(deltaTime);
            if (m_controllerTimeouts[idx].hasElapsed(CONNECTION_TIMEOUT))
            {
                m_controllerActive[idx] = false;
                m_controllerTimeouts[idx].stop();
            }
            else
            {
                connectedCount++;
            }
        }
    }
    m_connectedControllerCount = connectedCount;
}

void NeonManager::runManualMode(float deltaTime)
{
    for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
    {
        queueManualMessage(idx);
    }
    m_directControlManual = false;  
}

void NeonManager::runDefaultMode(float deltaTime)
{
    // TODO: handle default behavior
}

// Only one message is sent per pulse regardless of mode
// In all mode, all controllers pulse at the same time.
void NeonManager::runAudioMode(float deltaTime)
{
    queueAudioMessage();
}

void NeonManager::runProfileMode(float deltaTime)
{    
    if (m_directControlProfile)
    {
        for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
        {
            queueProfileMessage(idx);
        }
        
        m_directControlProfile = false;
    }
}

void NeonManager::updateAudioListening(float deltaTime)
{
    if (m_pFftAnalyzer != nullptr)
    {
        m_pFftAnalyzer->update(deltaTime);

        if (m_pAudioOrchestrator != nullptr)
        {
            m_pAudioOrchestrator->generatePulse(m_pFftAnalyzer->getNormalizedMagnitudes());
        }
    }
}

void NeonManager::manageDirectControlFlags(void)
{
    if (m_directControlProfile)
    {
        static bool hasSeenProfileActivity = false;
        bool anyActive = false;
        for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
        {
            anyActive |= m_controllerStateParameters[idx].profileActive;
        }

        if (anyActive)
        {
            hasSeenProfileActivity = true;
        }
        else if (hasSeenProfileActivity)
        {
            m_directControlProfile = false;
            hasSeenProfileActivity = false;
        }
    }
}
