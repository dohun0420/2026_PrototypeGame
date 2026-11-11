#include "DefenseBarricade.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "MonsterCharacter.h"
#include "NavigationSystem.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
struct FFragmentFace
{
    TArray<FVector> Points;
    FVector Normal;
};

TArray<FFragmentFace> MakeBoxFaces(const FVector& Extent)
{
    TArray<FFragmentFace> Faces;
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const int32 UAxis = (Axis + 1) % 3;
        const int32 VAxis = (Axis + 2) % 3;
        FVector U = FVector::ZeroVector, V = FVector::ZeroVector;
        U[UAxis] = Extent[UAxis];
        V[VAxis] = Extent[VAxis];
        for (double Sign : {-1.0, 1.0})
        {
            FVector Normal = FVector::ZeroVector;
            Normal[Axis] = Sign;
            const FVector Center = Normal * Extent[Axis];
            Faces.Add({{Center - U - V, Center + U - V, Center + U + V, Center - U + V}, Normal});
        }
    }
    return Faces;
}

// Keep the half of the cell closer to its seed, and close the cut with a new face.
void ClipCell(TArray<FFragmentFace>& Faces, const FVector& Normal, const FVector& Midpoint)
{
    TArray<FVector> Cut;
    for (FFragmentFace& Face : Faces)
    {
        TArray<FVector> Clipped;
        for (int32 Index = 0; Index < Face.Points.Num(); ++Index)
        {
            const FVector& A = Face.Points[Index];
            const FVector& B = Face.Points[(Index + 1) % Face.Points.Num()];
            const double DA = FVector::DotProduct(A - Midpoint, Normal);
            const double DB = FVector::DotProduct(B - Midpoint, Normal);
            if (DA <= 0.0) Clipped.Add(A);
            if ((DA <= 0.0) == (DB <= 0.0)) continue;

            const FVector Point = FMath::Lerp(A, B, DA / (DA - DB));
            Clipped.Add(Point);
            if (!Cut.ContainsByPredicate([&Point](const FVector& Existing) { return Existing.Equals(Point, .01); }))
                Cut.Add(Point);
        }
        Face.Points = MoveTemp(Clipped);
    }
    Faces.RemoveAll([](const FFragmentFace& Face) { return Face.Points.Num() < 3; });
    if (Cut.Num() < 3) return;

    FVector Center = FVector::ZeroVector;
    for (const FVector& Point : Cut) Center += Point;
    Center /= Cut.Num();
    FVector U, V;
    Normal.FindBestAxisVectors(U, V);
    auto Angle = [&](const FVector& Point)
    {
        return FMath::Atan2(FVector::DotProduct(Point - Center, V), FVector::DotProduct(Point - Center, U));
    };
    Cut.Sort([&](const FVector& A, const FVector& B) { return Angle(A) < Angle(B); });
    Faces.Add({MoveTemp(Cut), Normal});
}
}

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
    for (UProceduralMeshComponent* Fragment : Fragments) Fragment->DestroyComponent();
    Fragments.Reset();
    FragmentCenters.Reset();
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
    GetWorldTimerManager().SetTimerForNextTick(this, &ADefenseBarricade::PrepareFragments);
}

void ADefenseBarricade::PrepareFragments()
{
    if (!Fragments.IsEmpty()) return;

    const FVector Extent = Blocker->GetUnscaledBoxExtent();
    const FVector CellSize = Extent * 2.0 / FVector(2, 4, 3);
    FRandomStream Random(GetTypeHash(GetActorLocation()));
    TArray<FVector> Seeds;
    for (int32 X = 0; X < 2; ++X)
        for (int32 Y = 0; Y < 4; ++Y)
            for (int32 Z = 0; Z < 3; ++Z)
            {
                const FVector Offset(Random.FRandRange(.18f, .82f), Random.FRandRange(.18f, .82f),
                    Random.FRandRange(.18f, .82f));
                Seeds.Add((FVector(X, Y, Z) + Offset) * CellSize - Extent);
            }

    for (int32 Index = 0; Index < Seeds.Num(); ++Index)
    {
        TArray<FFragmentFace> Faces = MakeBoxFaces(Extent);
        const FVector& Seed = Seeds[Index];
        for (int32 Other = 0; Other < Seeds.Num(); ++Other)
        {
            if (Other == Index) continue;
            const FVector Normal = (Seeds[Other] - Seed).GetSafeNormal();
            ClipCell(Faces, Normal, (Seeds[Other] + Seed) * .5);
        }

        TArray<FVector> Vertices, Normals, Hull;
        TArray<int32> Triangles;
        TArray<FVector2D> UVs;
        for (const FFragmentFace& Face : Faces)
        {
            const int32 Start = Vertices.Num();
            FVector U, V;
            Face.Normal.FindBestAxisVectors(U, V);
            for (const FVector& Point : Face.Points)
            {
                const FVector Local = (Point - Seed) * .975;
                Vertices.Add(Local);
                Normals.Add(Face.Normal);
                UVs.Emplace(FVector::DotProduct(Point, U) / 100, FVector::DotProduct(Point, V) / 100);
                if (!Hull.ContainsByPredicate([&Local](const FVector& Existing) { return Existing.Equals(Local, .01); }))
                    Hull.Add(Local);
            }
            for (int32 I = 1; I + 1 < Face.Points.Num(); ++I)
            {
                const FVector Cross = FVector::CrossProduct(Face.Points[I] - Face.Points[0],
                    Face.Points[I + 1] - Face.Points[0]);
                if (Cross.IsNearlyZero()) continue;
                const bool bReverse = FVector::DotProduct(Cross, Face.Normal) > 0.0;
                Triangles.Append({Start, Start + (bReverse ? I + 1 : I), Start + (bReverse ? I : I + 1)});
            }
        }
        if (Triangles.IsEmpty() || Hull.Num() < 4) continue;

        auto* Fragment = NewObject<UProceduralMeshComponent>(this);
        AddInstanceComponent(Fragment);
        Fragment->SetupAttachment(Blocker);
        Fragment->SetRelativeLocation(Seed);
        Fragment->SetCanEverAffectNavigation(false);
        Fragment->SetCollisionObjectType(ECC_PhysicsBody);
        Fragment->SetCollisionResponseToAllChannels(ECR_Ignore);
        Fragment->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        Fragment->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        Fragment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Fragment->SetGenerateOverlapEvents(false);
        Fragment->bUseComplexAsSimpleCollision = false;
        Fragment->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs,
            TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
        Fragment->AddCollisionConvexMesh(Hull);
        Fragment->SetMaterial(0, Wall->GetMaterial(0));
        Fragment->SetVisibility(false);
        Fragment->RegisterComponent();
        Fragments.Add(Fragment);
        FragmentCenters.Add(Seed);
    }
}

void ADefenseBarricade::ScatterFragments(const FVector& Impact)
{
    PrepareFragments();
    const FVector Inward = -GetActorForwardVector();
    FRandomStream Random(GetTypeHash(GetActorLocation()));
    int32 SimulatingCount = 0;
    for (UProceduralMeshComponent* Fragment : Fragments)
    {
        const FVector FromHit = Fragment->GetComponentLocation() - Impact;
        const float Strength = FMath::GetMappedRangeValueClamped(FVector2D(0, 220), FVector2D(340, 105), FromHit.Size());
        const FVector Direction = (Inward + FromHit.GetSafeNormal() * .65
            + FVector::UpVector * Random.FRandRange(.15f, .8f)).GetSafeNormal();
        Fragment->SetVisibility(true);
        Fragment->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Fragment->SetSimulatePhysics(true);
        Fragment->AddImpulse(Direction * Strength, NAME_None, true);
        Fragment->AddAngularImpulseInRadians(Random.VRand() * Random.FRandRange(3.f, 9.f), NAME_None, true);
        SimulatingCount += Fragment->IsSimulatingPhysics();
    }
    UE_LOG(LogTemp, Display, TEXT("BARRICADE_SHATTER %s fragments=%d simulating=%d"),
        *GetName(), Fragments.Num(), SimulatingCount);
    GetWorldTimerManager().SetTimer(DebrisTimer, this, &ADefenseBarricade::HideFragments, 5.f, false);
}

void ADefenseBarricade::HideFragments()
{
    GetWorldTimerManager().ClearTimer(DebrisTimer);
    for (int32 Index = 0; Index < Fragments.Num(); ++Index)
    {
        UProceduralMeshComponent* Fragment = Fragments[Index];
        Fragment->SetVisibility(false);
        Fragment->SetSimulatePhysics(false);
        Fragment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Fragment->AttachToComponent(Blocker, FAttachmentTransformRules::KeepWorldTransform);
        Fragment->SetRelativeLocationAndRotation(FragmentCenters[Index], FRotator::ZeroRotator);
    }
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
    if (Health <= 0.f)
    {
        SetStanding(false);
        ScatterFragments(GetStrikePoint(DamageCauser->GetActorLocation()));
    }
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
    SetActorTickEnabled(false);
    Wall->SetVisibility(bStanding);
    if (bStanding) HideFragments();
    UE_LOG(LogTemp, Display, TEXT("BARRICADE_%s %s"), bStanding ? TEXT("REBUILT") : TEXT("BROKEN"), *GetName());
}

void ADefenseBarricade::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bStanding && Health >= MaxHealth * .3f && !IsSpaceOccupied())
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
