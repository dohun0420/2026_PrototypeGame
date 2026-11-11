#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DefenseBarricade.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class MYPROJECTPROTOTYPE_API ADefenseBarricade : public AActor
{
    GENERATED_BODY()

public:
    ADefenseBarricade();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

    void SetDimensions(const FVector& HalfExtent);
    bool IsStanding() const { return bStanding; }
    float GetHealthFraction() const { return Health / MaxHealth; }
    FVector GetStrikePoint(const FVector& From) const;
    FVector GetApproachPoint(float Side) const;
    bool CanRepair() const { return !bStanding || Health < MaxHealth; }
    void Repair(float Amount);

private:
    UPROPERTY(VisibleAnywhere, Category="Barricade") TObjectPtr<UBoxComponent> Blocker;
    UPROPERTY(VisibleAnywhere, Category="Barricade") TObjectPtr<UStaticMeshComponent> Wall;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Cracks;
    UPROPERTY(EditAnywhere, Category="Barricade", meta=(ClampMin="1")) float MaxHealth = 100.f;
    float Health = 100.f;
    float CollapseTime = 0.f;
    bool bStanding = true;

    bool IsSpaceOccupied() const;
    void SetStanding(bool bNewStanding);
    void UpdateCracks();
};
