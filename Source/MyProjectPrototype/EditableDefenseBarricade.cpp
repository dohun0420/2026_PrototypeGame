#include "EditableDefenseBarricade.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AEditableDefenseBarricade::AEditableDefenseBarricade()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WallStart")));
    GetBlocker()->SetupAttachment(GetRootComponent());

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Stone(
        TEXT("/Game/Prototype/Materials/MI_BlockoutStone.MI_BlockoutStone"));
    if (Stone.Succeeded())
    {
        GetWallMesh()->SetMaterial(0, Stone.Object);
    }
}

void AEditableDefenseBarricade::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    FVector End = WallEnd.GetLocation();
    End.Z = 0.0;
    const double Length = End.Size();
    const bool bHasLength = Length >= 2.0;
    const double HalfHeight = FMath::Max(1.0, WallHeight * 0.5);
    const double HalfThickness = FMath::Max(1.0, WallThickness * 0.5);

    FVector Center = End * 0.5;
    Center.Z = HalfHeight;
    // The original barricade stores thickness on X and length on Y.
    const FQuat Rotation = End.ToOrientationQuat() * FQuat(FVector::UpVector, -UE_HALF_PI);
    GetBlocker()->SetRelativeLocationAndRotation(Center, Rotation);
    GetBlocker()->SetCollisionEnabled(bHasLength ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    GetWallMesh()->SetVisibility(bHasLength);
    SetDimensions(FVector(HalfThickness, FMath::Max(1.0, Length * 0.5), HalfHeight));
}

bool AEditableDefenseBarricade::IsStanding() const
{
    return Super::IsStanding() && GetBlocker()->IsCollisionEnabled();
}
