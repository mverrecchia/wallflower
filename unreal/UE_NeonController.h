#pragma once

#include "NeonController.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlatformTypes.h"
#include "PlatformConstants.h"
#include "UE_PlatformTypes.h"
#include "UE_PlatformUtils.h"

class UE_NeonController : public NeonController {
public:
    UE_NeonController() 
    : NeonController(new UE_PlatformUtils())
    {
        memset(m_macAddress, 0, MAC_ADDRESS_SIZE);
    }

    void runMotorSpeed(void) override;
    void runMotor(void) override;
    void stopMotor(void) override;

    void update(float deltaTime);
    void updateDistanceValue(void) override;

    void setComponents(UStaticMeshComponent* rotatingComp, UStaticMeshComponent* stationaryComp);
    void setMaterials(UMaterialInstanceDynamic* rotMat0,
                      UMaterialInstanceDynamic* rotMat1,
                      UMaterialInstanceDynamic* statMat0,
                      UMaterialInstanceDynamic* statMat1);

protected:
    virtual void applyMotorSpeed(void) override;
    virtual void applyMotorSettings(void) override;
    virtual void applyNeonSettings(void) override;
    virtual void applyRotatingBrightness(void) override;
    virtual void applyStationaryBrightness(void) override;

    virtual void queueMessage(MessageType_E messageType, const void* msg) override;

private:
    float m_deltaTime = 0.0f;

    UPROPERTY()
    UStaticMeshComponent* m_rotatingComponent = nullptr;

    UPROPERTY()
    UStaticMeshComponent* m_stationaryComponent = nullptr;

    UPROPERTY()
    UMaterialInstanceDynamic* m_rotatingMaterial0 = nullptr;

    UPROPERTY()
    UMaterialInstanceDynamic* m_rotatingMaterial1 = nullptr;

    UPROPERTY()
    UMaterialInstanceDynamic* m_stationaryMaterial0 = nullptr;

    UPROPERTY()
    UMaterialInstanceDynamic* m_stationaryMaterial1 = nullptr;
};