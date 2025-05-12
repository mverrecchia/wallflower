// shared/src/NeonController.cpp
#include "NeonController.h"
#include <cstring>

void NeonController::update(float deltaTime)
{
    m_i2cWriteTimer.update(deltaTime);

    updateOperatingMode();
    updateConnectionState(deltaTime);
    updateAudioActivity(deltaTime);
    updateModes(deltaTime);

    if (m_idleModeActive)
    {
        runIdleMode(deltaTime);
    }
    else if (m_manualModeActive)
    {
        runManualMode(deltaTime);
    }
    else if (m_audioActive)
    {
        runAudioMode(deltaTime);
    }
    else if (m_profileExecutor.getProfileActive())
    {
        runProfileMode(deltaTime);
    }
    else
    {
        runPassiveMode(deltaTime);
    }

    runDistanceOverride(deltaTime);

    // slow down i2c writes to 100Hz
    applyNeonSettings();
    applyMotorSettings();
}

void NeonController::runPassiveMode(float deltaTime)
{
    // timeout the brightness after 60 seonds
    m_passiveBrightnessTimeoutTimer.update(deltaTime);
    if (m_passiveBrightnessTimeoutTimer.hasElapsed(PASSIVE_BRIGHTNESS_TIMEOUT))
    {
        for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
        {
            setSupplyEnable(idx, false);
        }
    }
}

void NeonController::runProfileMode(float deltaTime)
{
    updateProfileExecutor(deltaTime, m_brightnessChangeEnable, m_motorChangeEnable);
}

void NeonController::runAudioMode(float deltaTime)
{
    for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
    {
        // skip supply 1 for now
        if (idx == 0)
        {
            setSupplyEnable(idx, m_audioRequestSupplies[idx].enable);
            setSupplyBrightness(idx, m_pUtils->clamp(m_audioRequestSupplies[idx].brightness, MIN_BRIGHTNESS, NORMALIZED_MAX));
        }
    }
}

void NeonController::runManualMode(float deltaTime)
{
    if (m_profileExecutor.getProfileActive())
    {
        m_profileExecutor.stopProfile();
    }

    for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
    {
        // TODO: reenable supply1
        if (idx == 0)
        {
            setSupplyEnable(idx, m_manualRequestSupplies[idx].enable);
            setSupplyBrightness(idx, m_manualRequestSupplies[idx].brightness);
        }
    }
    setMotorEnable(m_manualRequestMotor.enable);
    setSpeed(m_manualRequestMotor.speed);
    setDirection(m_manualRequestMotor.direction);

    m_manualModeActive = false;
}

void NeonController::runIdleMode(float deltaTime)
{
    // shouldn't have to do anything here but keeping it for now
}

// UE counterpart is empty since ControllerActor sets the distance, and HW distance is read from a sensor
void NeonController::runDistanceOverride(float deltaTime)
{
    m_distanceOverrideTimer.update(deltaTime);
    updateDistanceValue();
    static float targetSupply0, targetSupply1, targetMotor;
    static bool targetDirection;

    float distance = getDistance();
    // 0.0f is out of range
    bool withinDistanceThreshold = (distance < DISTANCE_OVERRIDE_THRESHOLD) && (distance > 0.0f);

    if (withinDistanceThreshold)
    {
        // Enter override mode if not already in it
        if (!m_isInDistanceOverride)
        {
            for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
            {
                m_distanceOverrideSupplies[idx].enable = getSupplyEnable(idx);
                m_distanceOverrideSupplies[idx].brightness = getSupplyBrightness(idx);
                if (idx == 0)
                {
                    // TODO: reenable supply1
                    setSupplyEnable(idx, true);
                }
            }
            setMotorEnable(true);

            m_distanceOverrideMotor.enable = getEnable();
            m_distanceOverrideMotor.direction = getDirection();
            m_distanceOverrideMotor.speed = getTargetSpeed();
            m_transitionTimer = 0.0f;
            m_isInDistanceOverride = true;
        }

        updateDistanceOverrideValues(distance, targetSupply0, targetSupply1, targetMotor, targetDirection);
        m_distanceOverrideTimer.start();
    }

    // Apply distance override if within threshold or during timeout
    if (withinDistanceThreshold
        || (!m_distanceOverrideTimer.hasElapsed(DISTANCE_OVERRIDE_TIMEOUT)
        &&   m_distanceOverrideTimer.isRunning()))
    {
        // Transition into distance-based values
        m_transitionTimer = std::min(m_transitionTimer + deltaTime, TRANSITION_DURATION);
        float progress = m_transitionTimer / TRANSITION_DURATION;

        for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
        {
            // this is using targetsupply0 for both supplies
            setSupplyBrightness(idx, m_pUtils->lerp(m_distanceOverrideSupplies[idx].brightness, targetSupply0, progress));
        }
        setSpeed(m_pUtils->lerp(m_distanceOverrideMotor.speed, targetMotor, progress));
        setDirection(targetDirection);
    }
    else if (m_isInDistanceOverride)
    {
        if (m_transitionTimer == TRANSITION_DURATION)
        {
            m_transitionTimer = 0.0f;
        }

        m_transitionTimer = std::min(m_transitionTimer + deltaTime, TRANSITION_DURATION);
        float progress = m_transitionTimer / TRANSITION_DURATION;

        for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
        {
            // this is using targetsupply0 for both supplies
            setSupplyBrightness(idx, m_pUtils->lerp(targetSupply0, m_distanceOverrideSupplies[idx].brightness, progress));
        }
        setSpeed(m_pUtils->lerp(targetMotor, m_distanceOverrideMotor.speed, progress));
        setDirection(targetDirection);

        if (m_transitionTimer >= TRANSITION_DURATION)
        {
            m_isInDistanceOverride = false;
            for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
            {
                // TODO: reenable supply1
                if (idx == 0)
                {
                    setSupplyEnable(idx, m_distanceOverrideSupplies[idx].enable);
                }
            }
            setMotorEnable(m_distanceOverrideMotor.enable);
            setDirection(m_distanceOverrideMotor.direction);
        }
    }
}

void NeonController::updateDistanceOverrideValues(float distance, float& rotatingValue, float& stationaryValue, float& motorValue, bool& direction)
{
    float normalizedDistance = m_pUtils->mapRange(distance, DISTANCE_OVERRIDE_THRESHOLD, 0.0f, NORMALIZED_MIN, NORMALIZED_MAX);
    normalizedDistance = m_pUtils->clamp(normalizedDistance, NORMALIZED_MIN, NORMALIZED_MAX);

    rotatingValue = normalizedDistance;
    stationaryValue = normalizedDistance;
    motorValue = normalizedDistance;
    direction = !m_distanceOverrideMotor.direction;
}

void NeonController::updateConnectionState(float deltaTime)
{
    m_connectionTimer.update(deltaTime);

    bool managerMessageValid = !m_connectionTimer.hasElapsed(CONNECTION_TIMEOUT);

    ControllerState_E controllerState = getControllerState();
    switch (controllerState)
    {
        case ControllerState_E::UNPAIRED:
            if (m_managerPresent && managerMessageValid) 
            {
                setControllerState(ControllerState_E::PAIRED);
            }
            break;

        case ControllerState_E::PAIRED:
            if (!m_managerPresent)
            {
                setControllerState(ControllerState_E::IDLE);
            }
            break;

        case ControllerState_E::IDLE:
        default:
            if (m_managerPresent && managerMessageValid)
            {
                setControllerState(ControllerState_E::PAIRED);
            }
            else
            {
                setControllerState(ControllerState_E::UNPAIRED);
            }
            break;
    }
}

void NeonController::queueHeartbeatMessage(void)
{
    ControllerHeartbeatMessageFull_S msg;
    for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
    {
        msg.heartbeat.supplies[idx].id = idx;
        msg.heartbeat.supplies[idx].enable = getSupplyEnable(idx);
        msg.heartbeat.supplies[idx].brightness = getSupplyBrightness(idx);
        msg.heartbeat.supplies[idx].current = getSupplyCurrent(idx);
    }
    msg.heartbeat.motor.enable = getEnable();
    msg.heartbeat.motor.speed = getTargetSpeed();
    msg.heartbeat.motor.direction = getDirection();
    msg.heartbeat.profileActive = m_profileExecutor.getProfileActive();
    msg.heartbeat.distance = getDistance();

    queueMessage(msg.header, &msg);
}

void NeonController::handleHeartbeatMessage(const ManagerHeartbeatMessageFull_S* msg)
{  
    setManagerPresent(true);
    m_connectionTimer.start();
}

void NeonController::handleProfileMessage(const ProfileMessageFull_S* msg)
{
    if (msg)
    {
        if (msg->profileParams.stopProfile)
        {
            stopProfileExecutor();
        }
        else
        {
            startProfileExecutor(msg->profileParams);
        }
    }
}

void NeonController::handleAudioMessage(const AudioMessageFull_S* msg)
{
    if (msg)
    {
        m_audioActive = true;
        m_audioActivityTimer.start();

        for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
        {
            if (memcmp(msg->audio[idx].controllerMac, m_macAddress, MAC_ADDRESS_SIZE) == 0)
            {
                for (size_t supplyIdx = 0; supplyIdx < NUM_SUPPLIES_PER_CONTROLLER; supplyIdx++)
                {
                    if (msg->audio[idx].frequencyFlags[supplyIdx] == 1)
                    {
                        m_audioRequestSupplies[supplyIdx].brightness = msg->audio[idx].prevailingWeightedLowMagnitude;
                        m_audioRequestSupplies[supplyIdx].enable = true;
                    }
                    else if (msg->audio[idx].frequencyFlags[supplyIdx] == 2)
                    {
                        m_audioRequestSupplies[supplyIdx].brightness = msg->audio[idx].prevailingWeightedMidMagnitude;
                        m_audioRequestSupplies[supplyIdx].enable = true;
                    }
                    else if (msg->audio[idx].frequencyFlags[supplyIdx] == 4)
                    {
                        m_audioRequestSupplies[supplyIdx].brightness = msg->audio[idx].prevailingWeightedHighMagnitude;
                        m_audioRequestSupplies[supplyIdx].enable = true;
                    }
                    else
                    {
                        m_audioRequestSupplies[supplyIdx].brightness = 0.1f;
                        m_audioRequestSupplies[supplyIdx].enable = true;
                    }
                }
                
                m_audioRequestMotor.enable = msg->audio[idx].motorEnable;
                break;
            }
        }
    }
}

void NeonController::handleManualMessage(const ManualMessageFull_S* msg)
{
    m_manualModeActive = true;

    if (m_profileExecutor.getProfileActive())
    {
        m_profileExecutor.stopProfile();
    }

    for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
    {
        m_manualRequestSupplies[idx].enable = msg->manual.supplies[idx].enable;
        m_manualRequestSupplies[idx].brightness = msg->manual.supplies[idx].brightness;
    }
    m_manualRequestMotor.enable = msg->manual.motor.enable;
    m_manualRequestMotor.speed = msg->manual.motor.speed;
    m_manualRequestMotor.direction = msg->manual.motor.direction;
}

void NeonController::updateModes(float deltaTime)
{
    OperatingMode_E operatingMode = getOperatingMode();
    // read the operating mode pin
    if (m_operatingMode == OperatingMode_E::MANAGER)
    {
        // Determine current mode
        bool inProfileMode = m_profileExecutor.getProfileActive();
        bool inAudioMode = m_audioActive;

        // Check for mode entry/exit
        if (inProfileMode && !m_wasInProfileMode)
        {
            enterProfileMode();
        }
        else if (inAudioMode && !m_wasInAudioMode)
        {
            enterAudioMode();
        }
        else
        {
            enterPassiveMode();
        }

        // Update mode tracking
        m_wasInProfileMode = inProfileMode;
        m_wasInAudioMode = inAudioMode;
    }
    else
    {
        enterIdleMode();
    }
}

void NeonController::enterProfileMode(void)
{
    m_brightnessChangeEnable = true;
    m_motorChangeEnable = true;

    for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
    {
        // TODO: reenable supply1
        if (idx == 0)
        {
            setSupplyEnable(idx, true);
        }
    }
    setMotorEnable(true);
}

void NeonController::enterAudioMode(void)
{
    for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
    {
        // TODO: reenable supply1
        if (idx == 0)
        {
            setSupplyEnable(idx, true);
        }
    }
}

// passive mode is when the controller is not actively being controlled by the manager
void NeonController::enterPassiveMode(void)
{
    // turn off motor, and set brightness to 0.25
    setMotorEnable(false);
    for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
    {
        if (idx == 0)
        {
            setSupplyEnable(idx, true);
            setSupplyBrightness(idx, 0.25f);
        }
        else
        {
             // TODO: reenable supply1
            setSupplyEnable(idx, false);
            setSupplyBrightness(idx, 0.0f);
        }
    }
    // start timeout timer
    m_passiveBrightnessTimeoutTimer.reset();
}

// idle mode is when the controller is not being controlled by the manager
void NeonController::enterIdleMode(void)
{
    for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
    {
        // TODO: reenable supply1
        if (idx == 0)
        {
            setSupplyEnable(idx, true);
            setSupplyBrightness(idx, 0.25f);
        }
        else
        {
            setSupplyEnable(idx, false);
            setSupplyBrightness(idx, 0.0f);
        }
    }
    setMotorEnable(true);
    setSpeed(0.75f);
    setDirection(true);
}

void NeonController::updateProfileExecutor(float deltaTime, bool brightnessEnable, bool motorEnable)
{
    float supply0Value, supply1Value, motorValue;

    if (m_profileExecutor.getProfileActive())
    {
        // Save initial state when profile first becomes active
        if (!m_profileActive)
        {
            m_profileActive = true;
            
            // Save current state before profile execution
            for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
            {
                m_profileSupplies[idx].brightness = getSupplyBrightness(idx);
                m_profileSupplies[idx].enable = getSupplyEnable(idx);
            }
            
            m_profileMotor.speed = getTargetSpeed();
            m_profileMotor.enable = getEnable();
            m_profileMotor.direction = getDirection();
        }
        
        m_profileExecutor.updateProfileValues(deltaTime, supply0Value, supply1Value, motorValue);

        if (brightnessEnable)
        {
            for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
            {
                // skip supply 1 for now
                if (idx == 0)
                {
                    setSupplyBrightness(idx, supply0Value);
                }
            }
        }

        if (motorEnable)
        {
            setSpeed(motorValue);
        }

        // Check if profile just completed
        if (!m_profileExecutor.getProfileActive())
        {
            m_profileActive = false;

            if (brightnessEnable)
            {
                for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
                {
                    if (idx == 0)
                    {
                        setSupplyBrightness(idx, m_profileSupplies[idx].brightness);
                        setSupplyEnable(idx, m_profileSupplies[idx].enable);
                    }
                }
            }

            if (motorEnable)
            {
                setSpeed(m_profileMotor.speed);
                setMotorEnable(m_profileMotor.enable);
                setDirection(m_profileMotor.direction);
            }
        }
    }
}

void NeonController::updateAudioActivity(float deltaTime)
{
    if (m_audioActive)
    {
        m_audioActivityTimer.update(deltaTime);
        if (m_audioActivityTimer.hasElapsed(AUDIO_REACTIVITY_TIMEOUT))
        {
            m_audioActive = false;
        }
    }
    else
    {
        m_audioActivityTimer.stop();
    }
}