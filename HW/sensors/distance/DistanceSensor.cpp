#include "DistanceSensor.h"

void DistanceSensor::updateSensorState(void)
{
    unsigned long currentTime = millis();
    uint8_t bytesRead = 0;

    switch (m_sensorState)
    {
        case SensorState::IDLE:
        {
            // Nothing to do
            break;
        }
        case SensorState::TRIGGER_REQUESTED:
        {
            bool triggerResult = triggerMeasurement();
            if (triggerResult)
            {
                m_measurementStartTime = currentTime;
                m_sensorState = SensorState::WAITING_FOR_TRIGGER_DELAY;
            }
            else
            {
                m_sensorState = SensorState::IDLE;
            }
            break;
        }
        case SensorState::WAITING_FOR_TRIGGER_DELAY:
        {
            if (currentTime - m_measurementStartTime >= MEASUREMENT_DELAY_MS)
            {
                bool readResult = initiateRead();
                // Initiate read of the distance register
                if (readResult)
                {
                    m_readStartTime = currentTime;
                    m_sensorState = SensorState::WAITING_FOR_READ_DELAY;
                }
                else
                {
                    m_sensorState =SensorState:: IDLE;
                }
            }
            break;
        }

        case SensorState::WAITING_FOR_READ_DELAY:
        {
            if (currentTime - m_readStartTime >= READ_TIMEOUT_MS)
            {
                uint8_t bytesRead = completeRead(m_sensorBuffer);
                if (bytesRead > 0)
                {
                    m_sensorState = SensorState::MEASUREMENT_READY;
                }
                else
                {
                    m_sensorState = SensorState::IDLE;
                }
            }
            break;
        }
        case SensorState::MEASUREMENT_READY:
        {
            // Measurement is ready to be read in getDistance()
            break;
        }
    }
}

bool DistanceSensor::initiateRead(void)
{
    bool writeResult = false;

    Wire.beginTransmission(DEFAULT_I2C_ADDRESS);

    Wire.write(DISTANCE_REGISTER);

    if (Wire.endTransmission() == 0)
    {
        writeResult = true;
    }

    return writeResult;
}

uint8_t DistanceSensor::completeRead(uint8_t* pBuf)
{
   
    Wire.requestFrom(DEFAULT_I2C_ADDRESS, READ_BUFFER_SIZE);
    
    size_t bytesRead = 0;
    for (size_t idx = 0; idx < READ_BUFFER_SIZE; idx++)
    {
        if (Wire.available())
        {
            pBuf[idx] = Wire.read();
            bytesRead++;
        }
        else
        {
            break;
        }
    }
    
    return bytesRead;
}

bool DistanceSensor::triggerMeasurement(void)
{
    bool writeResult = false;
    Wire.beginTransmission(DEFAULT_I2C_ADDRESS);
    
    Wire.write(TRIGGER_REGISTER);
    Wire.write(TRIGGER_COMMAND);

    if (Wire.endTransmission() == 0)
    {
        writeResult = true;
    }

    return writeResult;
}

bool DistanceSensor::getDistanceMeasurement(uint16_t* pDistance)
{
    bool dataValid = false;
    // Trigger a new measurement if not already in progress
    if (m_sensorState == SensorState::IDLE)
    {
        m_sensorState = SensorState::TRIGGER_REQUESTED;
    }

    // If measurement is ready, read and return
    if (m_sensorState == SensorState::MEASUREMENT_READY)
    {
        *pDistance = convertBytesToDistance(m_sensorBuffer);
        dataValid = true;
        m_sensorState = SensorState::IDLE;
    }

    return dataValid;
}

uint16_t DistanceSensor::convertBytesToDistance(uint8_t* m_sensorBuffer)
{
    return ((m_sensorBuffer[0] << 8) | m_sensorBuffer[1]) + 10;
}
