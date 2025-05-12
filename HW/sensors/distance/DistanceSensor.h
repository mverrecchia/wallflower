// DistanceSensor.h
#pragma once

#include <Arduino.h>
#include <Wire.h>

class DistanceSensor {
public:
    // Compile-time constants for sensor configuration
    static constexpr uint8_t DEFAULT_I2C_ADDRESS = 0x74;
    static constexpr size_t READ_BUFFER_SIZE = 2;
    static constexpr size_t WRITE_BUFFER_SIZE = 1;
    static constexpr uint8_t TRIGGER_COMMAND = 0xB0;
    static constexpr uint8_t TRIGGER_REGISTER = 0x10;
    static constexpr uint8_t DISTANCE_REGISTER = 0x02;
    static constexpr size_t REGISTER_SIZE = 1;
    static constexpr size_t COMMAND_SIZE = 1;
    static constexpr unsigned long MEASUREMENT_DELAY_MS = 50;
    static constexpr unsigned long READ_TIMEOUT_MS = 20;

    enum class SensorState {
        IDLE,
        TRIGGER_REQUESTED,
        WAITING_FOR_TRIGGER_DELAY,
        WAITING_FOR_READ_DELAY,
        MEASUREMENT_READY
    };
    // Default constructor (inline)
    DistanceSensor() = default;

    bool getDistanceMeasurement(uint16_t* pDistance);
    void updateSensorState();

private:
    SensorState m_sensorState = SensorState::IDLE;
    uint8_t m_sensorBuffer[READ_BUFFER_SIZE];
    unsigned long m_measurementStartTime = 0;
    unsigned long m_readStartTime = 0;

    // Protected methods
    bool triggerMeasurement();
    bool initiateRead();
    uint8_t completeRead(uint8_t* pBuf);

    uint16_t convertBytesToDistance(uint8_t* buffer);
};