// HW_NeonController.cpp
#include "HW_NeonController.h"
#include "TaskManager.h"

bool HW_NeonController::initialize()
{
    initializeI2C();
    initializeSupplies();
    initializeMotor();
    
    return true;
}

void HW_NeonController::initializeI2C(void)
{
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(I2C_SPEED);  // 400kHz I2C clock
    
    if (!m_dac.begin())
    {
        if (Serial) Serial.println("Failed to initialize DAC");
    }
}

void HW_NeonController::initializeSupplies(void)
{
    pinMode(PIN_SUPPLY_0_EN, OUTPUT);
    pinMode(PIN_SUPPLY_1_EN, OUTPUT);

    // hardcode for now until we have a generalized way to set the supply output type
    m_supplyOutputType[0] = SupplyOutputType_E::DAC;
    m_supplyOutputType[1] = SupplyOutputType_E::DAC;

    for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
    {
        setSupplyEnable(idx, false);
        applySupplyEnable(idx);
    }
}

void HW_NeonController::initializeMotor(void)
{
    pinMode(PIN_MOTOR_EN, OUTPUT);
    pinMode(PIN_MOTOR_DIR, OUTPUT);
    pinMode(PIN_MOTOR_STEP, OUTPUT);

    setMotorEnable(false);  // Start disabled
    setDirection(true);  // Start right

    applyMotorEnable();
    applyMotorDirection();
}

void HW_NeonController::applyDACOutput(MCP4728_channel_t dacChannel, float voltage)
{
    // Convert voltage (0-3.3V) to 12-bit DAC value (0-4095)
    uint16_t dacValue = static_cast<uint16_t>((voltage / HW::DAC_PWR_V) * HW::DAC_RESOLUTION);
    m_dac.setChannelValue(dacChannel, dacValue);
}

void HW_NeonController::applySupplyEnable(size_t idx)
{
    bool enable = getSupplyEnable(idx);
    if (idx == 0)
    {
        digitalWrite(PIN_SUPPLY_0_EN, enable);
    }
    else if (idx == 1)
    {
        digitalWrite(PIN_SUPPLY_1_EN, enable);
    }
}

void HW_NeonController::applySupplyBrightness(size_t idx)
{
    float brightness = getSupplyBrightness(idx);
    float voltage = brightness * HW::DAC_MAX_V;

    if (idx == 0)
    {
        applyDACOutput(MCP4728_CHANNEL_B, voltage);
    }
    else if (idx == 1)
    {
        applyDACOutput(MCP4728_CHANNEL_A, voltage);
    }
}

void HW_NeonController::applyNeonSettings(void)
{
    if (m_i2cWriteTimer.hasElapsed(I2C_WRITE_TIMEOUT))
    {
        for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
        {
            applySupplyEnable(idx);
            applySupplyBrightness(idx);
        }
        m_i2cWriteTimer.reset();
    }
}

void HW_NeonController::applyMotorSettings(void)
{
    bool motorEnable = getEnable();

    if (motorEnable)
    {
        applyMotorDirection();
        applyMotorMaxSpeed();
        applyMotorSpeed();
        applyMotorAcceleration();
        runMotorSpeed();
        
        // Signal that a profile is active if the motor is running
        if (getTargetSpeed() != 0.0f)
        {
            m_profileActive = true;
        }
    }
    else
    {
        stopMotor();
        m_profileActive = false;
    }
}

void HW_NeonController::applyMotorEnable(void)
{
    bool enable = getEnable();
    digitalWrite(PIN_MOTOR_EN, enable);
}

void HW_NeonController::applyMotorDirection(void)
{
    bool direction = getDirection();
    digitalWrite(PIN_MOTOR_DIR, direction);
}

void HW_NeonController::applyMotorSpeed(void)
{
    float speed = getTargetSpeed();
    uint16_t speedMapped = m_pUtils->mapSpeed32(speed);
    m_stepper.setSpeed(speedMapped);
}

void HW_NeonController::applyMotorMaxSpeed(void)
{
    // This is not a normalized value, this is the actual max speed
    m_stepper.setMaxSpeed(HW::MAX_RPM);
}

void HW_NeonController::applyMotorAcceleration(void)
{
    // This is not a normalized value, this is the actual acceleration
    m_stepper.setAcceleration(HW::MAX_ACCEL);
}

void HW_NeonController::runMotorSpeed(void)
{
    m_stepper.runSpeed();
}

void HW_NeonController::runMotor(void)
{
    m_stepper.run();
}

void HW_NeonController::stopMotor(void)
{
    m_stepper.stop();
}

void HW_NeonController::updateOperatingMode(void)
{
    bool operatingMode = digitalRead(PIN_MODE);
    if (operatingMode)
    {
        setOperatingMode(OperatingMode_E::MANAGER);
    }
    else
    {
        setOperatingMode(OperatingMode_E::IDLE);
    }
}

void HW_NeonController::updateDistanceValue(void)
{
    uint16_t sensorDistance = 0;
    m_distanceSensor.updateSensorState();

    bool distanceMeasurementValid = m_distanceSensor.getDistanceMeasurement(&sensorDistance);
    
    if (distanceMeasurementValid)
    {
        if (sensorDistance == HW::INVALID_DISTANCE)
        {
            setDistance(-1.0f);
        }
        else
        {
            // Normalize distance between 0-1
            float normalizedDistance = (float)(sensorDistance - HW::MIN_DISTANCE) / (HW::MAX_DISTANCE - HW::MIN_DISTANCE);
            normalizedDistance = m_pUtils->clamp(normalizedDistance, NORMALIZED_MIN, NORMALIZED_MAX);
            setDistance(normalizedDistance);
        }
    }
}

void HW_NeonController::update(float deltaTime)
{   
    NeonController::update(deltaTime);
}

void HW_NeonController::queueMessage(MessageType_E messageType, const void* msg)
{
    uint8_t messageSize = 0;

    switch (messageType)
    {
        default:
        case MessageType_E::HEARTBEAT_C: messageSize = sizeof(ControllerHeartbeatMessageFull_S); break;
    }
    
    // Queue for ESP-NOW task if available
    if (TaskManager::espNowOutQueue != nullptr)
    {
        TaskManager::EspNowMessage espNowMsg;
        
        if (messageSize > sizeof(espNowMsg.data))
        {
            m_pUtils->logError("ESP-NOW message too large for queue");
            return;
        }
        
        memcpy(espNowMsg.data, msg, messageSize);
        espNowMsg.length = messageSize;
        espNowMsg.messageType = messageType;
        
        // Send to queue
        if (xQueueSend(TaskManager::espNowOutQueue, &espNowMsg, pdMS_TO_TICKS(10)) != pdPASS)
        {
            m_pUtils->logError("Failed to queue ESP-NOW message");
        }
    } 
    else
    {
        // Fall back to direct sending if queue not initialized
        sendMessage(messageType, msg, messageSize);
        if (Serial) Serial.println("Warning: ESP-NOW queue not initialized, sending directly");
    }
}

void HW_NeonController::sendMessage(MessageType_E messageType, const void* msg, uint8_t messageSize)
{
    const uint8_t* targetMac = getManagerMac();
    
    // only send if we have a manager registered
    if (!getManagerRegistered())
    {
        return;
    }
    
    esp_err_t result = esp_now_send(targetMac, (const uint8_t*)msg, messageSize);

    if (result != ESP_OK)
    {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "CONTROLLER: Failed to send message : %d", result);
        m_pUtils->logError(buffer);
    }
}

void HW_NeonController::handleEspNowMessage(const uint8_t* data, int len)
{
    MessageType_E messageType = static_cast<MessageType_E>(data[0]); 
    switch (messageType)
    {
        case MessageType_E::HEARTBEAT_M:
            if (len >= sizeof(ManagerHeartbeatMessageFull_S))
            {
                ManagerHeartbeatMessageFull_S msg;
                memcpy(&msg, data, sizeof(ManagerHeartbeatMessageFull_S));
                handleHeartbeatMessage(&msg);
            }
            break;
        case MessageType_E::MANUAL:
            if (len >= sizeof(ManualMessageFull_S))
            {
                ManualMessageFull_S msg;
                memcpy(&msg, data, sizeof(ManualMessageFull_S));
                handleManualMessage(&msg);
            }
            break;
        case MessageType_E::PROFILE:
            if (len >= sizeof(ProfileMessageFull_S))
            {
                ProfileMessageFull_S msg;
                memcpy(&msg, data, sizeof(ProfileMessageFull_S));
                handleProfileMessage(&msg);
            }
            break;
        case MessageType_E ::AUDIO:
            if (len >= sizeof(AudioMessageFull_S))
            {
                AudioMessageFull_S msg;
                memcpy(&msg, data, sizeof(AudioMessageFull_S));
                handleAudioMessage(&msg);
            }
            break;
        default:
            m_pUtils->logError("CONTROLLER: Unknown message type");
            break;
    }
}
