#include "NeonControllerActor.h"

ANeonControllerActor::ANeonControllerActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // create a default scene root component
    USceneComponent* RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(RootComp);

    // create both mesh components and attach to root
    RotatingMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RotatingMesh"));
    RotatingMeshComponent->SetupAttachment(RootComponent);

    StationaryMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationaryMesh"));
    StationaryMeshComponent->SetupAttachment(RootComponent);  // Attach to root instead of rotating mesh

    m_controller = new UE_NeonController();
}

void ANeonControllerActor::BeginPlay()
{
    Super::BeginPlay();
}

void ANeonControllerActor::InitializeController()
{
    if (m_controller)
    {
        m_controller->setComponents(RotatingMeshComponent, StationaryMeshComponent);
        m_controller->setMaterials(RotatingMaterial0, RotatingMaterial1, StationaryMaterial0, StationaryMaterial1);
    }
}

float ANeonControllerActor::CalculateDistanceToCamera() const
{
    float projectedDistance = 0.0f;
    if (APlayerCameraManager* cameraManager = GetWorld()->GetFirstPlayerController()->PlayerCameraManager)
    {
        FVector cameraLocation = cameraManager->GetCameraLocation();
        FVector componentLocation = GetActorLocation();
        FVector componentForward = -GetActorRightVector();  

        FVector toCamera = cameraLocation - componentLocation;
        float projectedDistance = FVector::DotProduct(toCamera, componentForward);
       
        if (projectedDistance > UE::INVALID_DISTANCE)
        {
            float angle = FMath::Acos(FVector::DotProduct(toCamera.GetSafeNormal(), componentForward));
            float angleInDegrees = FMath::RadiansToDegrees(angle);
           
            if (angleInDegrees <= UE::ANGLE_WINDOW)
            {
                projectedDistance /= 100.0f; // Convert to meters
                float mappedProjectedDistance = FMath::Clamp((projectedDistance - UE::MIN_DISTANCE) / (UE::MAX_DISTANCE - UE::MIN_DISTANCE), NORMALIZED_MIN, NORMALIZED_MAX);
                return mappedProjectedDistance;
            }
        }
    }
    return projectedDistance;
}

void ANeonControllerActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (m_controller)
    {
        // actor needs to calculate distance, so we set it here rather than in updateDistanceOverride()
        m_controller->setDistance(CalculateDistanceToCamera());
        m_controller->update(DeltaTime);
    }
}
