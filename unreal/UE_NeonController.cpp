// unreal/Source/YourProject/UE_NeonController.cpp
#include "UE_NeonController.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

void UE_NeonController::setMaterials(UMaterialInstanceDynamic* rotMat0, UMaterialInstanceDynamic* rotMat1,
                                     UMaterialInstanceDynamic* statMat0, UMaterialInstanceDynamic* statMat1)
{
    m_rotatingMaterial0 = rotMat0;
    m_rotatingMaterial1 = rotMat1;
    m_stationaryMaterial0 = statMat0;
    m_stationaryMaterial1 = statMat1;
}

void UE_NeonController::setComponents(UStaticMeshComponent* rotatingComp, UStaticMeshComponent* stationaryComp)
{
    m_rotatingComponent = rotatingComp;
    m_stationaryComponent = stationaryComp;
}

void UE_NeonController::runMotorSpeed(void)
{
    // in UE, just ensure we're at target speed
    m_currentSpeed = m_targetSpeed;
}

void UE_NeonController::runMotor(void)
{
    // use passed-in deltaTime instead of GetWorld()
    if (m_currentSpeed != m_targetSpeed && m_acceleration > 0)
    {
        float speedDiff = m_targetSpeed - m_currentSpeed;
        float maxSpeedChange = m_acceleration * m_deltaTime;

        if (FMath::Abs(speedDiff) <= maxSpeedChange)
        {
            m_currentSpeed = m_targetSpeed;
        }
        else
        {
            m_currentSpeed += (speedDiff > 0 ? maxSpeedChange : -maxSpeedChange);
        }
    }
}

void UE_NeonController::stopMotor(void)
{
    m_currentSpeed = 0.0f;
}

void UE_NeonController::updateDistanceValue(void)
{
    // Controller Actor handles setting the m_distance value in ANeonControllerActor::Tick()
}

void UE_NeonController::update(float deltaTime)
{
    m_deltaTime = deltaTime;  // Store the deltaTime
    NeonController::update(deltaTime);
}

void UE_NeonController::applyMotorSettings(void)
{    
    applyMotorSpeed();
}

void UE_NeonController::applyNeonSettings(void)
{
    applyRotatingBrightness();
    applyStationaryBrightness();
}

void UE_NeonController::applyMotorSpeed(void)
{
    // target speed is normalized between 0.00-1.00
    float speed = m_targetSpeed;
    // scale it to an appropriate value for UE
    float speedMapped = speed * UE::MAX_SPEED;

    if (m_rotatingComponent && m_motorEnable)
    {
        FQuat CurrentRotation = m_rotatingComponent->GetComponentQuat();
        float RotationAmount = speedMapped * m_deltaTime;
        FQuat DeltaRotation = FQuat(FRotator(RotationAmount, 0.0f, 0.0f));
        FQuat NewRotation = CurrentRotation * DeltaRotation;
        m_rotatingComponent->SetWorldRotation(NewRotation);
    }
}

void UE_NeonController::applyRotatingBrightness(void)
{
    float brightness = getRotatingBrightness();
    float brightnessMapped = brightness * UE::MAX_BRIGHTNESS;

    if (m_rotatingMaterial0)
    {
        m_rotatingMaterial0->SetScalarParameterValue(FName("Brightness"), brightnessMapped);
    }
    if (m_rotatingMaterial1)
    {
        m_rotatingMaterial1->SetScalarParameterValue(FName("Brightness"), brightnessMapped);
    }
}

void UE_NeonController::applyStationaryBrightness(void)
{
    float brightness = getStationaryBrightness();
    float brightnessMapped = brightness * UE::MAX_BRIGHTNESS;

    if (m_stationaryMaterial0)
    {
        m_stationaryMaterial0->SetScalarParameterValue(FName("Brightness"), brightnessMapped);
    }
    if (m_stationaryMaterial1)
    {
        m_stationaryMaterial1->SetScalarParameterValue(FName("Brightness"), brightnessMapped);
    }
}

void UE_NeonController::queueMessage(MessageType_E messageType, const void* msg)
{

}