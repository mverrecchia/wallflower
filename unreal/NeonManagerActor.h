#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UE_NeonManager.h"
#include "UE_PlatformTypes.h"
#include "MessageTypes.h"
#include "NeonManagerActor.generated.h"

static constexpr float CONNECTION_CHECK_INTERVAL = 1.0f;

UCLASS()
class ANeonManagerActor : public AActor
{
    GENERATED_BODY()

public:
    ANeonManagerActor();

    // Audio Properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundWave* SoundToAnalyze;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
    UAudioComponent* AudioComponent;

    std::unique_ptr<UE_NeonManager> m_neonManager;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "Audio")
    void PlayAnalyzerSound();

private:
    bool LoadConfigAndSpawnControllers(void);
    bool SpawnNeonController(const UE::FControllerConfig& Config);
    void ShowOnscreenMessages();

    UStaticMesh* LoadMeshAsset(const FString& Path);
    UMaterialInterface* LoadMaterialAsset(const FString& Path);
    FVector ParseJSONVector(const TSharedPtr<FJsonObject>& JsonObject);

    TArray<ANeonControllerActor*> NeonControllers; 
    float ConnectionCheckTimer = 0.0f;

    EProfileType GetNextProfileType(EProfileType current)
    {
        switch(current)
        {
            case EProfileType::COS:         return EProfileType::BOUNCE;
            case EProfileType::BOUNCE:       return EProfileType::EXPONENTIAL;
            case EProfileType::EXPONENTIAL:  return EProfileType::PULSE;
            case EProfileType::PULSE:        return EProfileType::TRIANGLE;
            case EProfileType::TRIANGLE:     return EProfileType::ELASTIC;
            case EProfileType::ELASTIC:      return EProfileType::CASCADE;
            case EProfileType::CASCADE:      return EProfileType::FLICKER;
            case EProfileType::FLICKER:
            default:                         return EProfileType::COS;
        }
    }
};
