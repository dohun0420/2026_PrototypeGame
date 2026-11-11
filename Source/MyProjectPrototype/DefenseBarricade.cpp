#include "DefenseBarricade.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "MonsterCharacter.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

ADefenseBarricade::ADefenseBarricade()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
    SetRootComponent(Blocker);
    Blocker->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Blocker->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Wall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wall"));
    Wall->SetupAttachment(Blocker);
    Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Wall->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Wall->SetStaticMesh(Cube.Object);

    for (int32 Index = 0; Index < 12; ++Index)
    {
        const FName Name(*FString::Printf(TEXT("Crack_%d"), Index));
        UStaticMeshComponent* Crack = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Crack->SetupAttachment(Wall);
        Crack->SetStaticMesh(Cube.Object);
        Crack->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Crack->SetCanEverAffectNavigation(false);
        Crack->SetVisibility(false);
        Cracks.Add(Crack);
    }
    SetDimensions(FVector(45, 135, 100));
}

void ADefenseBarricade::SetDimensions(const FVector& HalfExtent)
{
    Blocker->SetBoxExtent(HalfExtent);
    if (Blocker->IsRegistered()) FNavigationSystem::UpdateComponentData(*Blocker);
    Wall->SetRelativeScale3D(HalfExtent / 50.f);

    const FVector Points[] = {
        FVector(0, -30, 45), FVector(0, -15, 25), FVector(0, -22, 10),
        FVector(0, 2, -8), FVector(0, -5, -29), FVector(0, 15, -45),
        FVector(0, 34, -33)
    };
    for (int32 Index = 0; Index < Cracks.Num(); ++Index)
    {
        const int32 Segment = Index % 6;
        const FVector Delta = Points[Segment + 1] - Points[Segment];
        FVector Mid = (Points[Segment] + Points[Segment + 1]) * .5f;
        Mid.X = Index < 6 ? -50.5f : 50.5f;
        Cracks[Index]->SetRelativeLocation(Mid);
        Cracks[Index]->SetRelativeRotation(FRotationMatrix::MakeFromX(Delta).Rotator());
        Cracks[Index]->SetRelativeScale3D(FVector(Delta.Size() / 100.f, .025f, .025f));
    }
}

void ADefenseBarricade::BeginPlay()
{
    Super::BeginPlay();
    MaxHealth = FMath::Max(1.f, MaxHealth);
    Health = MaxHealth;
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Prototype/Materials/MI_BlockoutStone.MI_BlockoutStone"));
    if (Material) Wall->SetMaterial(0, Material);
    UMaterialInterface* CrackMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Prototype/Materials/MI_BlockoutWood.MI_BlockoutWood"));
    if (CrackMaterial)
        for (UStaticMeshComponent* Crack : Cracks) Crack->SetMaterial(0, CrackMaterial);
}

FVector ADefenseBarricade::GetStrikePoint(const FVector& From) const
{
    const FTransform Transform = GetActorTransform();
    const FVector Local = Transform.InverseTransformPosition(From);
    const FVector Extent = Blocker->GetUnscaledBoxExtent();
    const FVector Point(
        FMath::Clamp(Local.X, -Extent.X + 1.f, Extent.X - 1.f),
        FMath::Clamp(Local.Y, -Extent.Y + 1.f, Extent.Y - 1.f),
        FMath::Clamp(Local.Z, -Extent.Z + 1.f, Extent.Z - 1.f));
    return Transform.TransformPosition(Point);
}

FVector ADefenseBarricade::GetApproachPoint(float Side) const
{
    return GetActorTransform().TransformPosition(FVector(
        Side * (Blocker->GetUnscaledBoxExtent().X + 90.f), 0, -Blocker->GetUnscaledBoxExtent().Z));
}

float ADefenseBarricade::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (!bStanding || !Cast<AMonsterCharacter>(DamageCauser)
        || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f) return 0.f;
    const float Applied = FMath::Min(Health, DamageAmount);
    Health -= Applied;
    if (Health <= 0.f) SetStanding(false);
    UpdateCracks();
    UE_LOG(LogTemp, Display, TEXT("BARRICADE_HIT %s HP=%.0f"), *GetName(), Health);
    return Applied;
}

bool ADefenseBarricade::IsSpaceOccupied() const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BarricadeRebuild), false, this);
    return GetWorld()->OverlapAnyTestByObjectType(GetActorLocation(), GetActorQuat(),
        FCollisionObjectQueryParams(ECC_Pawn),
        FCollisionShape::MakeBox(Blocker->GetScaledBoxExtent()), Query);
}

void ADefenseBarricade::Repair(float Amount)
{
    if (Amount <= 0.f || !CanRepair()) return;
    if (bStanding)
    {
        Health = FMath::Min(MaxHealth, Health + Amount);
    }
    else
    {
        const float RebuildHealth = MaxHealth * .3f;
        Health = FMath::Min(RebuildHealth, Health + Amount);
        if (Health >= RebuildHealth) SetActorTickEnabled(true);
    }
    UpdateCracks();
}

void ADefenseBarricade::SetStanding(bool bNewStanding)
{
    bStanding = bNewStanding;
    Blocker->SetCollisionEnabled(bStanding ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    FNavigationSystem::UpdateComponentData(*Blocker);
    CollapseTime = 0.f;
    SetActorTickEnabled(!bStanding);
    if (bStanding)
    {
        Wall->SetRelativeLocation(FVector::ZeroVector);
        Wall->SetRelativeRotation(FRotator::ZeroRotator);
    }
    UE_LOG(LogTemp, Display, TEXT("BARRICADE_%s %s"), bStanding ? TEXT("REBUILT") : TEXT("BROKEN"), *GetName());
}

void ADefenseBarricade::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    CollapseTime += DeltaSeconds;
    const float Alpha = FMath::Clamp(CollapseTime / .45f, 0.f, 1.f);
    const FVector Extent = Blocker->GetUnscaledBoxExtent();
    Wall->SetRelativeRotation(FRotator(88.f * Alpha, 0, 0));
    Wall->SetRelativeLocation(FVector(-Extent.Z * Alpha, 0, (Extent.X - Extent.Z) * Alpha));
    if (Alpha < 1.f) return;

    if (Health < MaxHealth * .3f)
    {
        SetActorTickEnabled(false);
        return;
    }
    if (!IsSpaceOccupied())
    {
        SetStanding(true);
        UpdateCracks();
    }
}

void ADefenseBarricade::UpdateCracks()
{
    const float Fraction = GetHealthFraction();
    const int32 VisibleSegments = Fraction <= .3f ? 6 : Fraction <= .7f ? 3 : 0;
    for (int32 Index = 0; Index < Cracks.Num(); ++Index)
        Cracks[Index]->SetVisibility(bStanding && Index % 6 < VisibleSegments);
}
