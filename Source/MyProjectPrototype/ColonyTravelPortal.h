#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ColonyTravelPortal.generated.h"

class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UColonyTravelPromptWidget;
class APlayerController;
class APawn;
class UWorld;

/** Runtime entrance/exit used by the Colony 2 travel prototype. */
UCLASS()
class MYPROJECTPROTOTYPE_API AColonyTravelPortal : public AActor
{
    GENERATED_BODY()

public:
    AColonyTravelPortal();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    bool CanInteract(const APawn* PlayerPawn) const;
    void Interact(APlayerController* PlayerController);

    UPROPERTY(EditAnywhere, Category="Colony Travel", meta=(ClampMin="1"))
    int32 ColonyId = 2;

    /** Soft reference keeps the destination map discoverable by the cooker. */
    UPROPERTY(EditAnywhere, Category="Colony Travel")
    TSoftObjectPtr<UWorld> DestinationLevel;

    UPROPERTY(EditAnywhere, Category="Colony Travel")
    bool bIsExitPortal = false;

    /** Exterior entrances require proximity; the temporary interior exit does not. */
    UPROPERTY(EditAnywhere, Category="Colony Travel")
    bool bRequiresProximity = true;

    UPROPERTY(EditAnywhere, Category="Colony Travel", meta=(ClampMin="100.0"))
    float InteractionRadius = 550.f;

    UPROPERTY(EditAnywhere, Category="Colony Travel")
    FText InteractionPrompt;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USphereComponent> InteractionSphere;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> PortalMesh;

    UPROPERTY(Transient)
    TObjectPtr<UColonyTravelPromptWidget> PromptWidget;

    bool bTravelInProgress = false;
    FTimerHandle TravelGuardTimer;

    void EnsurePromptWidget(APlayerController* PlayerController);
    void HandlePendingReturn(APlayerController* PlayerController, APawn* PlayerPawn);
    void ResetTravelGuard();
    FString GetPromptString() const;
};
