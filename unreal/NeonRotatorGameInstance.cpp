// NeonRotatorGameInstance.cpp
#include "NeonRotatorGameInstance.h"
#include "WebSocketsModule.h"
#include "NeonManagerActor.h"
#include "Kismet/GameplayStatics.h"

void UNeonRotatorGameInstance::Init()
{
    Super::Init();

    // TODO: WebSockets support - disabling for now
    // if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    // {
    //     FModuleManager::Get().LoadModule("WebSockets");
    // }

    // // Create WebSocket server
    // WebSocket = FWebSocketsModule::Get().CreateWebSocket("ws://localhost:8081");

    // WebSocket->OnConnected().AddLambda([this]() {
    //     UE_LOG(LogTemp, Warning, TEXT("WebSocket connected"));
    //     FString IdentificationMessage = TEXT("{\"type\":\"client_id\", \"id\":\"UE\"}");
    //     WebSocket->Send(IdentificationMessage);
    // });

    // WebSocket->OnMessage().AddLambda([this](const FString& Message) {
    //     OnWebSocketMessage(Message);
    // });

    // WebSocket->Connect();

    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("NeonManager"), FoundActors);

    if (FoundActors.Num() > 0)
    {
        Manager = Cast<ANeonManagerActor>(FoundActors[0]);
    }
}

void UNeonRotatorGameInstance::Shutdown()
{
    Super::Shutdown();
}

void UNeonRotatorGameInstance::OnWebSocketConnected(FString ClientEndpoint)
{
    UE_LOG(LogTemp, Warning, TEXT("Client connected: %s"), *ClientEndpoint);
}

void UNeonRotatorGameInstance::OnWebSocketMessage(const FString& Message)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);

    if (!Manager)
    {
        UE_LOG(LogTemp, Warning, TEXT("NeonManagerActor not found"));
        return;
    }

    if (FJsonSerializer::Deserialize(Reader, JsonObject))
    {
        FString Type;
        if (!JsonObject->TryGetStringField(TEXT("type"), Type))
        {
            UE_LOG(LogTemp, Warning, TEXT("Message missing type field"));
            return;
        }

        // Handle profile messages
        if (Type == TEXT("profile"))
        {
            ProfileMessageFull_S ProfileMsg;
            ProfileMsg.header = MessageType_E::PROFILE;

            if (!JsonObject->GetObjectField(TEXT("profile")))
            {
                UE_LOG(LogTemp, Warning, TEXT("Message missing profile field"));
                return;
            }

            ProfileMsg.profileParams.type = static_cast<ProfileType_E>(JsonObject->GetIntegerField(TEXT("profileType")));
            ProfileMsg.profileParams.magnitude = JsonObject->GetNumberField(TEXT("magnitude"));
            ProfileMsg.profileParams.frequency = JsonObject->GetNumberField(TEXT("frequency"));
            ProfileMsg.profileParams.duration = JsonObject->GetNumberField(TEXT("duration"));
            ProfileMsg.profileParams.phase = JsonObject->GetNumberField(TEXT("phase"));
            ProfileMsg.profileParams.loopCount = JsonObject->GetIntegerField(TEXT("loopCount"));
            ProfileMsg.profileParams.blockNewProfiles = JsonObject->GetBoolField(TEXT("blockNewProfiles"));

            Manager->m_neonManager->requestProfileSend(0.0f);
        }
        else if (Type == TEXT("audio_config"))
        {
            AudioConfiguration_S AudioConfig;
            AudioConfig.mode = static_cast<AudioAssignmentMode_E>(JsonObject->GetIntegerField(TEXT("mode")));
            AudioConfig.allowMultipleActive = JsonObject->GetBoolField(TEXT("allowMultipleActive"));
            for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
            {
                const TSharedPtr<FJsonObject>& ControllerConfig = JsonObject->GetObjectField(FString::Printf(TEXT("controller%d"), idx));
                AudioConfig.controllerConfigs[idx].frequencyFlags = static_cast<FrequencyBand_E>(ControllerConfig->GetIntegerField(TEXT("frequencyFlags")));
                AudioConfig.controllerConfigs[idx].magnitudeThreshold = ControllerConfig->GetNumberField(TEXT("magnitudeThreshold"));
            }

            Manager->m_neonManager->setAudioConfig(AudioConfig.mode, AudioConfig.allowMultipleActive, AudioConfig.controllerConfigs);
        }
    }
}


void UNeonRotatorGameInstance::OnWebSocketClosed(const FString& EndPoint)
{
    UE_LOG(LogTemp, Warning, TEXT("Client disconnected: %s"), *EndPoint);
}