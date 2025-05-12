#include "NeonRotatorGameMode.h"

void ANeonRotatorGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    if (GetWorld() && GetWorld()->GetFirstPlayerController())
    {
        GetWorld()->GetFirstPlayerController()->SetInputMode(FInputModeGameAndUI());
        GetWorld()->GetFirstPlayerController()->bShowMouseCursor = true;
    }
}