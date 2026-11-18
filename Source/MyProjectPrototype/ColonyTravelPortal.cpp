#include "ColonyTravelPortal.h"

#include "ColonyTravelPromptWidget.h"
#include "ColonyTravelSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AColonyTravelPortal::AColonyTravelPortal()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(SceneRoot);
    InteractionSphere->InitSphereRadius(InteractionRadius);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMarker"));
    PortalMesh->SetupAttachment(SceneRoot);
    // Portal actors sit at the player-capsule height used by the layout markers.
    // Offset only the visual disc back down to the floor.
    PortalMesh->SetRelativeLocation(FVector(0.f, 0.f, -91.f));
    PortalMesh->SetRelativeScale3D(FVector(2.8f, 2.8f, 0.18f));
    PortalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PortalMesh->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded())
    {
        PortalMesh->SetStaticMesh(CylinderMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> PortalMaterial(
        TEXT("/Game/Prototype/Materials/MI_BlockoutColony.MI_BlockoutColony"));
    if (PortalMaterial.Succeeded())
    {
        PortalMesh->SetMaterial(0, PortalMaterial.Object);
    }

}

void AColonyTravelPortal::BeginPlay()
{
    Super::BeginPlay();
    InteractionSphere->SetSphereRadius(InteractionRadius, true);
    PortalMesh->SetVisibility(!bIsExitPortal, true);
}

void AColonyTravelPortal::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerController || !PlayerController->IsLocalController() || !PlayerPawn)
    {
        return;
    }

    EnsurePromptWidget(PlayerController);

    if (!bIsExitPortal)
    {
        HandlePendingReturn(PlayerController, PlayerPawn);
    }

    const bool bShowPrompt = !bTravelInProgress && CanInteract(PlayerPawn);
    if (PromptWidget)
    {
        PromptWidget->SetPromptVisible(bShowPrompt);
    }

    // The interior test should contain only the exit prompt, not the character debug HP line.
    if (bIsExitPortal && GEngine)
    {
        GEngine->RemoveOnScreenDebugMessage(static_cast<uint64>(PlayerPawn->GetUniqueID()));
    }
}

void AColonyTravelPortal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (PromptWidget)
    {
        PromptWidget->RemoveFromParent();
        PromptWidget = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

void AColonyTravelPortal::EnsurePromptWidget(APlayerController* PlayerController)
{
    if (PromptWidget || !PlayerController)
    {
        return;
    }

    PromptWidget = CreateWidget<UColonyTravelPromptWidget>(
        PlayerController,
        UColonyTravelPromptWidget::StaticClass());
    if (!PromptWidget)
    {
        return;
    }

    PromptWidget->SetPromptText(FText::FromString(GetPromptString()));
    PromptWidget->AddToViewport(50);
    PromptWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
    PromptWidget->SetPositionInViewport(FVector2D(40.f, 120.f), false);
    PromptWidget->SetDesiredSizeInViewport(FVector2D(620.f, 48.f));
    PromptWidget->SetPromptVisible(false);
}

bool AColonyTravelPortal::CanInteract(const APawn* PlayerPawn) const
{
    if (!PlayerPawn || bTravelInProgress || DestinationLevel.IsNull())
    {
        return false;
    }
    if (!bRequiresProximity)
    {
        return true;
    }

    return InteractionSphere->IsOverlappingActor(PlayerPawn)
        || FVector::DistSquared(PlayerPawn->GetActorLocation(), GetActorLocation())
            <= FMath::Square(InteractionRadius);
}

void AColonyTravelPortal::Interact(APlayerController* PlayerController)
{
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!CanInteract(PlayerPawn))
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UColonyTravelSubsystem* TravelSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UColonyTravelSubsystem>()
        : nullptr;
    if (!TravelSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("COLONY_TRAVEL missing GameInstance subsystem"));
        return;
    }

    bTravelInProgress = true;
    if (bIsExitPortal)
    {
        TravelSubsystem->RequestReturnToField(ColonyId);
        UE_LOG(LogTemp, Display, TEXT("COLONY_TRAVEL EXIT colony=%d destination=%s"),
            ColonyId, *DestinationLevel.ToString());
    }
    else
    {
        TravelSubsystem->BeginColonyTravel(ColonyId, PlayerPawn->GetActorTransform());
        UE_LOG(LogTemp, Display, TEXT("COLONY_TRAVEL ENTER colony=%d destination=%s"),
            ColonyId, *DestinationLevel.ToString());
    }

    // If travel cannot start for any unexpected reason, allow another E press.
    GetWorldTimerManager().SetTimer(
        TravelGuardTimer,
        this,
        &ThisClass::ResetTravelGuard,
        5.f,
        false);
    UGameplayStatics::OpenLevelBySoftObjectPtr(this, DestinationLevel, true);
}

void AColonyTravelPortal::HandlePendingReturn(
    APlayerController* PlayerController,
    APawn* PlayerPawn)
{
    UGameInstance* GameInstance = GetGameInstance();
    UColonyTravelSubsystem* TravelSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UColonyTravelSubsystem>()
        : nullptr;
    if (!TravelSubsystem)
    {
        return;
    }

    FTransform ReturnTransform;
    bool bHasReturnTransform = false;
    if (!TravelSubsystem->ConsumePendingReturn(
        ColonyId,
        ReturnTransform,
        bHasReturnTransform))
    {
        return;
    }

    if (!bHasReturnTransform)
    {
        ReturnTransform = GetActorTransform();
        ReturnTransform.SetLocation(GetActorLocation() + FVector(0.f, 0.f, 20.f));
    }

    const bool bTeleported = PlayerPawn->TeleportTo(
        ReturnTransform.GetLocation(),
        ReturnTransform.Rotator(),
        false,
        true);
    if (!bTeleported)
    {
        TravelSubsystem->RequestReturnToField(ColonyId);
        UE_LOG(LogTemp, Warning,
            TEXT("COLONY_TRAVEL return blocked; retrying colony=%d location=%s"),
            ColonyId,
            *ReturnTransform.GetLocation().ToCompactString());
        return;
    }

    PlayerController->SetControlRotation(ReturnTransform.Rotator());
    UE_LOG(LogTemp, Display, TEXT("COLONY_TRAVEL RETURNED colony=%d location=%s"),
        ColonyId, *ReturnTransform.GetLocation().ToCompactString());
}

void AColonyTravelPortal::ResetTravelGuard()
{
    if (bTravelInProgress)
    {
        bTravelInProgress = false;
        UE_LOG(LogTemp, Warning,
            TEXT("COLONY_TRAVEL travel timeout; interaction restored colony=%d"),
            ColonyId);
    }
}

FString AColonyTravelPortal::GetPromptString() const
{
    if (!InteractionPrompt.IsEmpty())
    {
        return InteractionPrompt.ToString();
    }
    return bIsExitPortal
        ? TEXT("PRESS E TO EXIT")
        : FString::Printf(TEXT("COLONY %d - PRESS E TO ENTER"), ColonyId);
}
