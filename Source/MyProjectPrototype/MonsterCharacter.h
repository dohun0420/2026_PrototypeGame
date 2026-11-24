// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MonsterCharacter.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class UBlendSpace;

UCLASS()
class MYPROJECTPROTOTYPE_API AMonsterCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AMonsterCharacter();

    bool TryStartMeleeAttack(AActor* Target);
    void CancelMeleeAttack();
    bool IsAttackInProgress() const { return bAttackInProgress; }
    bool IsAlive() const { return !bIsDead && CurrentHealth > 0.f; }
    float GetHealth() const { return CurrentHealth; }
    FName GetAttackPhase() const { return AttackPhase; }

    /** MonsterAIController 에서 호출합니다. (이전 헤더에는 선언이 없어 컴파일 오류의 원인이었습니다) */
    bool CanReachMeleeTarget(const AActor* Target) const;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual float TakeDamage(
        float DamageAmount,
        const FDamageEvent& DamageEvent,
        AController* EventInstigator,
        AActor* DamageCauser) override;

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster",
        meta = (ClampMin = "1.0"))
    float MaxHealth = 100.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Monster")
    float CurrentHealth = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "Monster|Attack", meta = (ClampMin = "0"))
    float AttackDamage = 10.f;
    UPROPERTY(EditDefaultsOnly, Category = "Monster|Attack", meta = (ClampMin = "0"))
    float AttackReach = 150.f;
    UPROPERTY(EditDefaultsOnly, Category = "Monster|Attack", meta = (ClampMin = "0.01"))
    float WindupSeconds = .4f;
    UPROPERTY(EditDefaultsOnly, Category = "Monster|Attack", meta = (ClampMin = "0.01"))
    float HitWindowSeconds = .15f;
    UPROPERTY(EditDefaultsOnly, Category = "Monster|Attack", meta = (ClampMin = "0.01"))
    float RecoverySeconds = .45f;
    UPROPERTY(EditDefaultsOnly, Category = "Monster|Attack", meta = (ClampMin = "0.01"))
    float AttackCooldown = 1.5f;
    UPROPERTY(EditDefaultsOnly, Category = "Monster|Attack")
    bool bShowAttackDebug = true;


    /** Idle <-> Walk 1D 블렌드 스페이스. 가로축은 이동 속도(cm/s)여야 합니다. */
    UPROPERTY(EditDefaultsOnly, Category = "Monster|Animation")
    TObjectPtr<UBlendSpace> LocomotionBlendSpace;

    /** 공격 몽타주. 안에 들어있는 공격 시퀀스를 꺼내서 재생합니다. */
    UPROPERTY(EditDefaultsOnly, Category = "Monster|Animation")
    TObjectPtr<UAnimMontage> AttackMontage;

    /** 몽타주에서 꺼낸(또는 직접 지정한) 실제 공격 시퀀스. */
    UPROPERTY(Transient)
    TObjectPtr<UAnimSequenceBase> AttackSequence;

private:
    bool bIsDead = false;
    bool bAttackInProgress = false;
    bool bDamageApplied = false;
    bool bAttackAnimPlaying = false;
    double NextAttackTime = 0.;
    FName AttackPhase = TEXT("Idle");
    TWeakObjectPtr<AActor> AttackTarget;
    FVector AttackForward = FVector::ForwardVector;
    FTimerHandle PhaseTimer;
    FTimerHandle HitTimer;

    bool IsValidAttackTarget(AActor* Target, bool bCheckDirection) const;
    void BeginHitWindow();
    void CheckMeleeHit();
    void BeginRecovery();
    void FinishMeleeAttack();

    void ResolveAttackSequence();
    void StartLocomotionAnimation();
    void UpdateLocomotionAnimation();
    void PlayAttackAnimation(float TotalDuration);
};