// NeonManagerActor.cpp
#include "NeonManagerActor.h"

ANeonManagerActor::ANeonManagerActor()
{
    PrimaryActorTick.bCanEverTick = true;

    AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
    AudioComponent->bAutoActivate = false;
    AudioComponent->bAllowSpatialization = false;
    AudioComponent->bOverrideAttenuation = true;
    AudioComponent->PrimaryComponentTick.bCanEverTick = true;
}

void ANeonManagerActor::BeginPlay()
{
    Super::BeginPlay();

    Tags.Add(FName("NeonManager"));

    m_neonManager = std::make_unique<UE_NeonManager>();
    m_neonManager->setWorld(GetWorld());

    if (!LoadConfigAndSpawnControllers())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to spawn controllers"));
        return;
    }
    
    // Add null checks when setting up controllers
    for (size_t idx = 0; idx < NeonControllers.Num(); idx++)
    {
        if (m_neonManager && NeonControllers[idx])
        {
            UE_NeonController* controller = NeonControllers[idx]->GetController();
            if (controller)
            {
                m_neonManager->m_controllers[idx] = controller;
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Null controller at index %d"), idx);
            }
        }
    }

    if (!m_neonManager->initialize())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to initialize manager"));
        return;
    }

    if (SoundToAnalyze)
    {
        AudioComponent->SetSound(SoundToAnalyze);
    }

    PlayAnalyzerSound();
}

void ANeonManagerActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (!m_neonManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("No valid neon manager in Tick"));
        return;
    }

    m_neonManager->update(DeltaTime);
    ShowOnscreenMessages();
}


void ANeonManagerActor::PlayAnalyzerSound()
{
    if (AudioComponent && SoundToAnalyze)
    {
        AudioComponent->Play();
    }
}

FVector ANeonManagerActor::ParseJSONVector(const TSharedPtr<FJsonObject>& JsonObject)
{
    FVector Result = FVector::ZeroVector;
    if (JsonObject)
    {
        Result.X = JsonObject->GetNumberField(TEXT("x"));
        Result.Y = JsonObject->GetNumberField(TEXT("y"));
        Result.Z = JsonObject->GetNumberField(TEXT("z"));
    }
    return Result;
}

bool ANeonManagerActor::LoadConfigAndSpawnControllers(void)
{
    FString JsonPath = FPaths::ProjectDir();
    JsonPath.Append(TEXT("Config/controller_config.json"));
    
    UE_LOG(LogTemp, Warning, TEXT("Attempting to load JSON from: %s"), *JsonPath);
    FString JsonString;
    
    if (!FFileHelper::LoadFileToString(JsonString, *JsonPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load controller config JSON from: %s"), *JsonPath);
        return false;
    }
    
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON content"));
        return false;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* ControllersArray;
    if (!JsonObject->TryGetArrayField(TEXT("controllers"), ControllersArray))
    {
        UE_LOG(LogTemp, Error, TEXT("Could not find 'controllers' array in JSON"));
        return false;
    }

    if (m_neonManager)
    {
        m_neonManager->setExpectedControllerCount(ControllersArray->Num());
        UE_LOG(LogTemp, Warning, TEXT("Set expected controller count to: %d"), ControllersArray->Num());
    }
    
    bool bSuccess = true;
    for (const auto& ControllerValue : *ControllersArray)
    {
        TSharedPtr<FJsonObject> ControllerObject = ControllerValue->AsObject();
        UE::FControllerConfig Config;
        
        // Parse rotating mesh info
        const TSharedPtr<FJsonObject>* RotatingMesh;
        if (ControllerObject->TryGetObjectField(TEXT("rotatingMesh"), RotatingMesh))
        {
            Config.RotatingMeshPath = (*RotatingMesh)->GetStringField(TEXT("path"));
            Config.RotatingLocation = ParseJSONVector((*RotatingMesh)->GetObjectField(TEXT("location")));
            Config.RotationSpeed = (*RotatingMesh)->GetNumberField(TEXT("speed"));
            Config.RotationAxis = ParseJSONVector((*RotatingMesh)->GetObjectField(TEXT("rotationAxis")));
            
            const TSharedPtr<FJsonObject>* Material0 = nullptr;
            if ((*RotatingMesh)->TryGetObjectField(TEXT("material0"), Material0))
            {
                Config.RotatingMaterial0Path = (*Material0)->GetStringField(TEXT("path"));
                Config.RotatingMaterial0Brightness = (*Material0)->GetNumberField(TEXT("brightness"));
            }
            
            const TSharedPtr<FJsonObject>* Material1 = nullptr;
            if ((*RotatingMesh)->TryGetObjectField(TEXT("material1"), Material1))
            {
                Config.RotatingMaterial1Path = (*Material1)->GetStringField(TEXT("path"));
                Config.RotatingMaterial1Brightness = (*Material1)->GetNumberField(TEXT("brightness"));
            }
        }
        
        // Parse stationary mesh info
        const TSharedPtr<FJsonObject>* StationaryMesh;
        if (ControllerObject->TryGetObjectField(TEXT("stationaryMesh"), StationaryMesh))
        {
            Config.StationaryMeshPath = (*StationaryMesh)->GetStringField(TEXT("path"));
            Config.StationaryLocation = ParseJSONVector((*StationaryMesh)->GetObjectField(TEXT("location")));
            
            const TSharedPtr<FJsonObject>* Material0 = nullptr;
            if ((*StationaryMesh)->TryGetObjectField(TEXT("material0"), Material0))
            {
                Config.StationaryMaterial0Path = (*Material0)->GetStringField(TEXT("path"));
                Config.StationaryMaterial0Brightness = (*Material0)->GetNumberField(TEXT("brightness"));
            }
            
            const TSharedPtr<FJsonObject>* Material1 = nullptr;
            if ((*StationaryMesh)->TryGetObjectField(TEXT("material1"), Material1))
            {
                Config.StationaryMaterial1Path = (*Material1)->GetStringField(TEXT("path"));
                Config.StationaryMaterial1Brightness = (*Material1)->GetNumberField(TEXT("brightness"));
            }
        }
        
        if (!SpawnNeonController(Config))
        {
            bSuccess = false;
        }
    }
    
    return bSuccess;
}

UStaticMesh* ANeonManagerActor::LoadMeshAsset(const FString& Path)
{
    if (Path.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Empty mesh path provided"));
        return nullptr;
    }

    UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Path));
    if (!Mesh)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load mesh at path: %s"), *Path);
        return nullptr;
    }
    
    return Mesh;
}

UMaterialInterface* ANeonManagerActor::LoadMaterialAsset(const FString& Path)
{
    if (Path.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Empty material path provided"));
        return nullptr;
    }

    UMaterialInterface* Material = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *Path));
    if (!Material)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load material at path: %s"), *Path);
        return nullptr;
    }
    
    return Material;
}

bool ANeonManagerActor::SpawnNeonController(const UE::FControllerConfig& Config)
{
    bool bHasRotatingMesh = false;
    bool bHasStationaryMesh = false;
    
    // Load meshes - now optional
    UStaticMesh* RotatingMesh = nullptr;
    UStaticMesh* StationaryMesh = nullptr;
    
    if (!Config.RotatingMeshPath.IsEmpty())
    {
        RotatingMesh = LoadMeshAsset(Config.RotatingMeshPath);
        bHasRotatingMesh = (RotatingMesh != nullptr);
    }
    
    if (!Config.StationaryMeshPath.IsEmpty())
    {
        StationaryMesh = LoadMeshAsset(Config.StationaryMeshPath);
        bHasStationaryMesh = (StationaryMesh != nullptr);
    }
    
    // Require at least one mesh
    if (!bHasRotatingMesh && !bHasStationaryMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("No valid meshes provided in config"));
        return false;
    }
    
    // Spawn the controller at origin - we'll position meshes individually
    FActorSpawnParameters SpawnParams;
    auto NewController = GetWorld()->SpawnActor<ANeonControllerActor>(
        ANeonControllerActor::StaticClass(),
        Config.RotatingLocation,
        FRotator::ZeroRotator,
        SpawnParams
    );
    
    NewController->SetActorLocation(Config.RotatingLocation);

    if (!NewController)
    {
        return false;
    }
    
    // Set up rotating mesh if we have one
    if (bHasRotatingMesh && NewController->RotatingMeshComponent)
    {
        NewController->RotatingMeshComponent->SetStaticMesh(RotatingMesh);
        
        if (!Config.RotatingMaterial0Path.IsEmpty())
        {
            if (UMaterialInterface* RotMat0 = LoadMaterialAsset(Config.RotatingMaterial0Path))
            {
                NewController->RotatingMeshComponent->SetMaterial(1, RotMat0);
                NewController->RotatingMaterial0 = NewController->RotatingMeshComponent->CreateAndSetMaterialInstanceDynamic(1);
                if (NewController->RotatingMaterial0)
                {
                    NewController->RotatingMaterial0->SetScalarParameterValue(FName("Brightness"), Config.RotatingMaterial0Brightness);
                    UE_LOG(LogTemp, Warning, TEXT("Created RotatingMaterial0 dynamic instance"));
                }
            }
        }
        
        if (!Config.RotatingMaterial1Path.IsEmpty())
        {
            if (UMaterialInterface* RotMat1 = LoadMaterialAsset(Config.RotatingMaterial1Path))
            {
                NewController->RotatingMeshComponent->SetMaterial(2, RotMat1);
                NewController->RotatingMaterial1 = NewController->RotatingMeshComponent->CreateAndSetMaterialInstanceDynamic(2);
                if (NewController->RotatingMaterial1)
                {
                    NewController->RotatingMaterial1->SetScalarParameterValue(FName("Brightness"), Config.RotatingMaterial1Brightness);
                    UE_LOG(LogTemp, Warning, TEXT("Created RotatingMaterial1 dynamic instance"));
                }
            }
        }
    }
    
    // Set up stationary mesh if we have one
    if (bHasStationaryMesh && NewController->StationaryMeshComponent)
    {
        NewController->StationaryMeshComponent->SetStaticMesh(StationaryMesh);
        
        if (!Config.StationaryMaterial0Path.IsEmpty())
        {
            if (UMaterialInterface* StatMat0 = LoadMaterialAsset(Config.StationaryMaterial0Path))
            {
                NewController->StationaryMeshComponent->SetMaterial(1, StatMat0);
                NewController->StationaryMaterial0 = NewController->StationaryMeshComponent->CreateAndSetMaterialInstanceDynamic(1);
                if (NewController->StationaryMaterial0)
                {
                    NewController->StationaryMaterial0->SetScalarParameterValue(FName("Brightness"), Config.StationaryMaterial0Brightness);
                    UE_LOG(LogTemp, Warning, TEXT("Created StationaryMaterial0 dynamic instance"));
                }
            }
        }
        
        if (!Config.StationaryMaterial1Path.IsEmpty())
        {
            if (UMaterialInterface* StatMat1 = LoadMaterialAsset(Config.StationaryMaterial1Path))
            {
                NewController->StationaryMeshComponent->SetMaterial(2, StatMat1);
                NewController->StationaryMaterial1 = NewController->StationaryMeshComponent->CreateAndSetMaterialInstanceDynamic(2);
                if (NewController->StationaryMaterial1)
                {
                    NewController->StationaryMaterial1->SetScalarParameterValue(FName("Brightness"), Config.StationaryMaterial1Brightness);
                    UE_LOG(LogTemp, Warning, TEXT("Created StationaryMaterial1 dynamic instance"));
                }
            }
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("Setting rotation speed to: %f"), Config.RotationSpeed);

    NewController->InitializeController();
    NeonControllers.Add(NewController);
    
    return true;
}

void ANeonManagerActor::ShowOnscreenMessages()
{
    FString profileStr;

    switch(m_neonManager->getNetworkProfileType())
    {
        case ProfileType_E::COS:         profileStr = TEXT("COS");         break;
        case ProfileType_E::EXPONENTIAL: profileStr = TEXT("EXPONENTIAL"); break;
        case ProfileType_E::BOUNCE:      profileStr = TEXT("BOUNCE");      break;
        case ProfileType_E::PULSE:       profileStr = TEXT("PULSE");       break;
        case ProfileType_E::TRIANGLE:    profileStr = TEXT("TRIANGLE");    break;
        case ProfileType_E::ELASTIC:     profileStr = TEXT("ELASTIC");     break;
        case ProfileType_E::CASCADE:     profileStr = TEXT("CASCADE");     break;
        case ProfileType_E::FLICKER:     profileStr = TEXT("FLICKER");     break;
    }

    // Get network status
    size_t connectedCount = m_neonManager->getConnectedControllerCount();
    bool networkActive = connectedCount > 0;

    // Display persistent status
    GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::White, 
        FString::Printf(TEXT("Network: %s"), networkActive ? TEXT("ACTIVE") : TEXT("INACTIVE")));
    GEngine->AddOnScreenDebugMessage(3, 0.0f, FColor::White, 
        FString::Printf(TEXT("Next Profile: %s"), *profileStr));
}