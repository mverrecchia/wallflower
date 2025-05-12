#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NeonRotatorGameMode.generated.h"

UCLASS()
class NEON_ROTATOR_API ANeonRotatorGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
};