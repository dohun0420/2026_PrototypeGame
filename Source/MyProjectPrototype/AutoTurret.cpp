#include "AutoTurret.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"

AAutoTurret::AAutoTurret()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	RootComponent = TurretMesh;
}

void AAutoTurret::BeginPlay()
{
	Super::BeginPlay();

	const float SafeFireRate = FMath::Max(FireRate, 0.05f);
	GetWorldTimerManager().SetTimer(
		FireTimerHandle,
		this,
		&AAutoTurret::FireMuzzleRay,
		SafeFireRate,
		true);
}

void AAutoTurret::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateTarget();
	if (!IsValid(CurrentTarget))
	{
		return;
	}

	const FVector MuzzlePosition = TurretMesh->DoesSocketExist(MuzzleSocketName)
		? TurretMesh->GetSocketLocation(MuzzleSocketName)
		: GetActorLocation();
	const FRotator TargetRotation = UKismetMathLibrary::FindLookAtRotation(
		MuzzlePosition,
		CurrentTarget->GetActorLocation());
	const FRotator NewRotation = FMath::RInterpTo(
		TurretMesh->GetComponentRotation(),
		TargetRotation,
		DeltaTime,
		12.0f);
	TurretMesh->SetWorldRotation(NewRotation);
}

void AAutoTurret::UpdateTarget()
{
	TArray<AActor*> FoundMonsters;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("Monster"), FoundMonsters);

	AActor* ClosestMonster = nullptr;
	float MinimumDistanceSquared = FMath::Square(AttackRange);
	const FVector TurretLocation = GetActorLocation();

	for (AActor* Monster : FoundMonsters)
	{
		if (!IsValid(Monster) || Monster->IsActorBeingDestroyed())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			TurretLocation,
			Monster->GetActorLocation());
		if (DistanceSquared < MinimumDistanceSquared)
		{
			MinimumDistanceSquared = DistanceSquared;
			ClosestMonster = Monster;
		}
	}

	CurrentTarget = ClosestMonster;
}

void AAutoTurret::FireMuzzleRay()
{
	if (!IsValid(CurrentTarget))
	{
		return;
	}

	const FVector StartLocation = TurretMesh->DoesSocketExist(MuzzleSocketName)
		? TurretMesh->GetSocketLocation(MuzzleSocketName)
		: GetActorLocation();
	const FVector LaunchDirection = (
		CurrentTarget->GetActorLocation() - StartLocation).GetSafeNormal();
	const FVector EndLocation = StartLocation + LaunchDirection * AttackRange;

	FHitResult HitResult;
	FCollisionQueryParams CollisionParameters(SCENE_QUERY_STAT(AutoTurretTrace), false, this);
	CollisionParameters.AddIgnoredActor(this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		ECC_Visibility,
		CollisionParameters);
	const FVector LaserEndPosition = bHit ? HitResult.ImpactPoint : EndLocation;

	DrawDebugLine(
		GetWorld(),
		StartLocation,
		LaserEndPosition,
		FColor::Red,
		false,
		LaserDuration,
		0,
		LaserThickness);

	if (!bHit)
	{
		return;
	}

	AActor* HitActor = HitResult.GetActor();
	if (IsValid(HitActor) && (HitActor == CurrentTarget || HitActor->ActorHasTag(TEXT("Monster"))))
	{
		UGameplayStatics::ApplyDamage(
			HitActor,
			DamageAmount,
			GetInstigatorController(),
			this,
			UDamageType::StaticClass());
	}
}
