#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ColonyTravelSubsystem.generated.h"

class APlayerController;

/** Persistent state and interaction routing for travel between the field and colony interiors. */
UCLASS()
class MYPROJECTPROTOTYPE_API UColonyTravelSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Called by the player controller when the interact key is pressed. */
    void TryInteract(APlayerController* PlayerController);

    void BeginColonyTravel(int32 InColonyId, const FTransform& InReturnTransform);
    void RequestReturnToField(int32 InColonyId);

    /** Consumes a pending return once the matching field entrance has a player pawn. */
    bool ConsumePendingReturn(
        int32 InColonyId,
        FTransform& OutReturnTransform,
        bool& bOutHasReturnTransform);

    int32 GetActiveColonyId() const { return ActiveColonyId; }

private:
    UPROPERTY(Transient)
    int32 ActiveColonyId = INDEX_NONE;

    UPROPERTY(Transient)
    FTransform ReturnTransform = FTransform::Identity;

    UPROPERTY(Transient)
    bool bHasReturnTransform = false;

    UPROPERTY(Transient)
    bool bReturnPending = false;
};
