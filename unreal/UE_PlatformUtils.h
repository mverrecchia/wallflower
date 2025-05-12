#pragma once
#include "PlatformUtils.h"
#include "CoreMinimal.h"

class UE_PlatformUtils : public PlatformUtils {
public:
    float mapRange(float value, float inMin, float inMax, float outMin, float outMax) override
    {
        return FMath::GetMappedRangeValueClamped(FVector2D(inMin, inMax),
                                                 FVector2D(outMin, outMax),
                                                 value);
    }

    float clamp(float value, float min, float max) override
    {
        return FMath::Clamp(value, min, max);
    }

    void log(const char* message) override
    {
        UE_LOG(LogTemp, Log, TEXT("%s"), UTF8_TO_TCHAR(message));
    }

    void logWarning(const char* message) override
    {
        UE_LOG(LogTemp, Warning, TEXT("%s"), UTF8_TO_TCHAR(message));
    }

    void logError(const char* message) override
    {
        UE_LOG(LogTemp, Error, TEXT("%s"), UTF8_TO_TCHAR(message));
    }

    void logFloat(float value) override
    {
        UE_LOG(LogTemp, Log, TEXT("%f"), value);
    }

    uint16_t mapSpeed16(float normalizedSpeed) override
    {
        return normalizedSpeed;
    }
    uint16_t mapSpeed32(float normalizedSpeed) override
    {
        return normalizedSpeed;
    }
};