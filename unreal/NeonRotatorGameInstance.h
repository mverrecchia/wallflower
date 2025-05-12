#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "NeonManagerActor.h"
#include "NeonRotatorGameInstance.generated.h"

UCLASS()
class UNeonRotatorGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;
    virtual void Shutdown() override;
    
private:
    TSharedPtr<IWebSocket> WebSocket;
    TArray<AActor*> FoundActors;
    ANeonManagerActor* Manager = nullptr;

    void OnWebSocketConnected(FString ClientEndpoint);
    void OnWebSocketMessage(const FString& Message);
    void OnWebSocketClosed(const FString& EndPoint);
};