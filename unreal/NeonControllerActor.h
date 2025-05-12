// unreal/Source/YourProject/NeonControllerActor.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UE_NeonController.h"
#include "UE_PlatformTypes.h"
#include "NeonControllerActor.generated.h"

UCLASS()
class ANeonControllerActor : public AActor
{
    GENERATED_BODY()

public:
    ANeonControllerActor();
    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

    void InitializeController();
    float CalculateDistanceToCamera() const;

    UE_NeonController* GetController() { return m_controller; }

    // Unreal-specific components
    UPROPERTY()
    UStaticMeshComponent* RotatingMeshComponent;

    UPROPERTY()
    UStaticMeshComponent* StationaryMeshComponent;

    UPROPERTY()
    UMaterialInstanceDynamic* RotatingMaterial0;
    
    UPROPERTY()
    UMaterialInstanceDynamic* RotatingMaterial1;
    
    UPROPERTY()
    UMaterialInstanceDynamic* StationaryMaterial0;
    
    UPROPERTY()
    UMaterialInstanceDynamic* StationaryMaterial1;

private:
    UE_NeonController* m_controller;
};