// hardware/shared/include/HW_PlatformUtils.h
#pragma once
#include "PlatformUtils.h"
#include <Arduino.h>

static constexpr size_t SPEEDS_32_SIZE = 49;
static constexpr size_t SPEEDS_16_SIZE = 58;

static constexpr uint16_t SPEEDS_32[SPEEDS_32_SIZE] =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 
    21, 23, 25, 26, 28, 30, 32, 35, 37, 40, 44, 49, 51, 54, 57, 61, 65, 
    70, 76, 82, 90, 99, 111, 125, 145, 171, 209, 268, 376, 500
};

static constexpr uint16_t SPEEDS_16[SPEEDS_16_SIZE] =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 21,
    23, 25, 27, 29, 31, 34, 36, 38, 41, 44, 47, 51, 53, 56, 60, 64, 69, 74, 80, 
    88, 97, 102, 108, 114, 121, 130, 139, 151, 164, 179, 198, 221, 250, 289, 341, 417, 500
};

class HW_PlatformUtils : public PlatformUtils {
public:
    float mapRange(float value, float inMin, float inMax, float outMin, float outMax) override
    {
        return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
    }

    float clamp(float value, float min, float max) override
    {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    void log(const char* message) override
    {
        if(Serial)
        {
            Serial.println(message);
        }
    }

    void logFloat(float value) override
    {
        if(Serial)
        {
            Serial.println(value);
        }
    }

    void logWarning(const char* message) override
    {
        if (Serial)
        {
            Serial.print("WARNING: ");
            Serial.println(message);
        }
    }

    void logError(const char* message) override
    {
        if (Serial)
        {
            Serial.print("ERROR: ");
            Serial.println(message);
        }
    }

    uint16_t mapSpeed16(float normalizedSpeed)
    {
        size_t idx = static_cast<size_t>(normalizedSpeed * (SPEEDS_16_SIZE - 1));
        uint16_t speed = SPEEDS_16[idx];
        return speed;
    }

    uint16_t mapSpeed32(float normalizedSpeed)
    {
        size_t idx = static_cast<size_t>(normalizedSpeed * (SPEEDS_32_SIZE - 1));
        uint16_t speed = SPEEDS_32[idx];
        return speed;
    }
};