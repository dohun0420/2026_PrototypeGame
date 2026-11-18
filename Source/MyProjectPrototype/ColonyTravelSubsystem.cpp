#include "ColonyTravelSubsystem.h"

#include "ColonyTravelPortal.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UColonyTravelSubsystem::TryInteract(APlayerController* PlayerController)
{
    if (!PlayerController || !PlayerController->IsLocalController() || !PlayerController->GetWorld())
    {
        return;
    }

    AColonyTravelPortal* BestPortal = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();
    for (TActorIterator<AColonyTravelPortal> It(PlayerController->GetWorld()); It; ++It)
    {
        AColonyTravelPortal* Portal = *It;
        if (!Portal || !Portal->CanInteract(PlayerController->GetPawn()))
        {
            continue;
        }

        const float DistanceSquared = PlayerController->GetPawn()
            ? FVector::DistSquared(PlayerController->GetPawn()->GetActorLocation(), Portal->GetActorLocation())
            : 0.f;
        if (!BestPortal || DistanceSquared < BestDistanceSquared)
        {
            BestPortal = Portal;
            BestDistanceSquared = DistanceSquared;
        }
    }

    if (BestPortal)
    {
        BestPortal->Interact(PlayerController);
    }
}

void UColonyTravelSubsystem::BeginColonyTravel(int32 InColonyId, const FTransform& InReturnTransform)
{
    ActiveColonyId = InColonyId;
    ReturnTransform = InReturnTransform;
    bHasReturnTransform = true;
    bReturnPending = false;
}

void UColonyTravelSubsystem::RequestReturnToField(int32 InColonyId)
{
    // Setting the id here also makes a directly launched interior map testable.
    ActiveColonyId = InColonyId;
    bReturnPending = true;
}

bool UColonyTravelSubsystem::ConsumePendingReturn(
    int32 InColonyId,
    FTransform& OutReturnTransform,
    bool& bOutHasReturnTransform)
{
    if (!bReturnPending || ActiveColonyId != InColonyId)
    {
        return false;
    }

    OutReturnTransform = ReturnTransform;
    bOutHasReturnTransform = bHasReturnTransform;
    bReturnPending = false;
    return true;
}
