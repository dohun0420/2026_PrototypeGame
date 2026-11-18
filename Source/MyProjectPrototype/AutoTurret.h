#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AutoTurret.generated.h"

class UStaticMeshComponent;

UCLASS()
class MYPROJECTPROTOTYPE_API AAutoTurret : public AActor
{
	GENERATED_BODY()

public:
	AAutoTurret();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TurretMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float DamageAmount = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.05"))
	float FireRate = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackRange = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Visual", meta = (ClampMin = "0.0"))
	float LaserThickness = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Visual", meta = (ClampMin = "0.0"))
	float LaserDuration = 0.15f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AActor> CurrentTarget;

private:
	void UpdateTarget();
	void FireMuzzleRay();

	FTimerHandle FireTimerHandle;
};
