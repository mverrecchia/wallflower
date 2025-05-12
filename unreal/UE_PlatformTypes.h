#pragma once
#include "CoreMinimal.h"
#include "PlatformTypes.h"
#include "UE_PlatformTypes.generated.h"

UENUM(BlueprintType)
enum class EControlMode : uint8
{
    NONE      UMETA(DisplayName = "None"),
    MANUAL    UMETA(DisplayName = "Direct"),
    AUDIO     UMETA(DisplayName = "Audio-Reactive"),
    NETWORK   UMETA(DisplayName = "Network-Controlled"),
    BEZIER    UMETA(DisplayName = "Bezier Curve")
};

UENUM(BlueprintType)
enum class ENeonChannel : uint8
{
    ROTATING   UMETA(DisplayName = "Rotating"),
    STATIONARY UMETA(DisplayName = "Stationary")
};

UENUM(BlueprintType)
enum class EProfileType : uint8
{
    COS         UMETA(DisplayName = "Cosine"),
    EXPONENTIAL UMETA(DisplayName = "Exponential"),
    BOUNCE      UMETA(DisplayName = "Bounce"),
    PULSE       UMETA(DisplayName = "Pulse"),
    TRIANGLE    UMETA(DisplayName = "Triangle"),
    ELASTIC     UMETA(DisplayName = "Elastic"),
    CASCADE     UMETA(DisplayName = "Cascade"),
    FLICKER     UMETA(DisplayName = "Flicker")
};

namespace UETypeConversion
{
    inline NeonChannel_E UEToSharedType(ENeonChannel ueChannel)
    {
        switch(ueChannel)
        {
            case ENeonChannel::ROTATING:   return NeonChannel_E::ROTATING;
            case ENeonChannel::STATIONARY: return NeonChannel_E::STATIONARY;
            default:                       return NeonChannel_E::ROTATING;
        }
    }

    inline ENeonChannel SharedToUEType(NeonChannel_E channel)
    {
        switch(channel) 
        {
            case NeonChannel_E::ROTATING:   return ENeonChannel::ROTATING;
            case NeonChannel_E::STATIONARY: return ENeonChannel::STATIONARY;
            default:                        return ENeonChannel::ROTATING;
        }
    }

    inline ProfileType_E UEToProfileType(EProfileType ueProfile)
    {
        switch(ueProfile)
        {
            case EProfileType::COS:         return ProfileType_E::COS;
            case EProfileType::EXPONENTIAL: return ProfileType_E::EXPONENTIAL;
            case EProfileType::BOUNCE:      return ProfileType_E::BOUNCE;
            case EProfileType::PULSE:       return ProfileType_E::PULSE;
            case EProfileType::TRIANGLE:    return ProfileType_E::TRIANGLE;
            case EProfileType::ELASTIC:     return ProfileType_E::ELASTIC;
            case EProfileType::CASCADE:     return ProfileType_E::CASCADE;
            case EProfileType::FLICKER:     return ProfileType_E::FLICKER;
            default:                        return ProfileType_E::COS;
        }
    }

    inline EProfileType ProfileToUEType(ProfileType_E profile)
    {
        switch(profile)
        {
            case ProfileType_E::COS:        return EProfileType::COS;
            case ProfileType_E::EXPONENTIAL: return EProfileType::EXPONENTIAL;
            case ProfileType_E::BOUNCE:      return EProfileType::BOUNCE;
            case ProfileType_E::PULSE:       return EProfileType::PULSE;
            case ProfileType_E::TRIANGLE:    return EProfileType::TRIANGLE;
            case ProfileType_E::ELASTIC:     return EProfileType::ELASTIC;
            case ProfileType_E::CASCADE:     return EProfileType::CASCADE;
            case ProfileType_E::FLICKER:     return EProfileType::FLICKER;
            default:                         return EProfileType::COS;
        }
    }
}

namespace UE {
    struct FControllerConfig
    {
        FString RotatingMeshPath;    
        FVector RotatingLocation;
        float RotationSpeed;
        FVector RotationAxis;
        FString RotatingMaterial0Path;
        FString RotatingMaterial1Path;
        float RotatingMaterial0Brightness;
        float RotatingMaterial1Brightness;
        
        FString StationaryMeshPath;  
        FVector StationaryLocation;
        FString StationaryMaterial0Path;
        FString StationaryMaterial1Path;
        float StationaryMaterial0Brightness;
        float StationaryMaterial1Brightness;
        
        FControllerConfig()
            : RotatingLocation(FVector::ZeroVector)
            , RotationSpeed(20.0f)
            , RotationAxis(FVector(1.0f, 0.0f, 0.0f))
            , RotatingMaterial0Brightness(0.0f)
            , RotatingMaterial1Brightness(0.0f)
            , StationaryLocation(FVector::ZeroVector)
            , StationaryMaterial0Brightness(0.0f)
            , StationaryMaterial1Brightness(0.0f)
        {}
    };
}