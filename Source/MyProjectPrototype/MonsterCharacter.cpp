#include "MonsterCharacter.h"
#include "MonsterAIController.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "ColonyCore.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MyProjectPrototypeCharacter.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "DefenseBarricade.h"

AMonsterCharacter::AMonsterCharacter()
{
    // 이동 속도에 맞춰 블렌드 스페이스 입력을 갱신하기 위해 Tick 사용
    PrimaryActorTick.bCanEverTick = true;
    AIControllerClass = AMonsterAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // 1. 외형: Mutant 스켈레탈 메시
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> MutantMesh(
        TEXT("/Game/Monster_Asset_Proto/Mutant.Mutant"));
    if (MutantMesh.Succeeded())
    {
        GetMesh()->SetSkeletalMesh(MutantMesh.Object);
    }
    GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
    GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
    // 충돌 판정은 캡슐이 담당 (메시가 공격/가시선 트레이스를 막지 않도록)
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 2. 블루프린트(AnimBP) 없이 애니메이션 에셋을 직접 재생하는 단일 노드 모드
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    // 3. 이동 블렌드 스페이스 (Idle / Walk)
    static ConstructorHelpers::FObjectFinder<UBlendSpace> LocomotionAsset(
        TEXT("/Game/Monster_Asset_Proto/BS_Monster_Movement.BS_Monster_Movement"));
    if (LocomotionAsset.Succeeded())
    {
        LocomotionBlendSpace = LocomotionAsset.Object;
    }

    // 4. 공격 몽타주
    static ConstructorHelpers::FObjectFinder<UAnimMontage> AttackMontageAsset(
        TEXT("/Game/Monster_Asset_Proto/Monster_Montage.Monster_Montage"));
    if (AttackMontageAsset.Succeeded())
    {
        AttackMontage = AttackMontageAsset.Object;
    }

    GetCharacterMovement()->MaxWalkSpeed = 350.f;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    bUseControllerRotationYaw = false;
}

void AMonsterCharacter::BeginPlay()
{
    Super::BeginPlay();
    CurrentHealth = MaxHealth;

    ResolveAttackSequence();
    StartLocomotionAnimation();
}

void AMonsterCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateLocomotionAnimation();
}

void AMonsterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
}

// ---------------------------------------------------------------------------
// 애니메이션
// ---------------------------------------------------------------------------

void AMonsterCharacter::ResolveAttackSequence()
{
    if (AttackSequence) return;

    // 몽타주의 첫 번째 슬롯 트랙에서 실제 공격 시퀀스를 꺼냅니다.
    if (AttackMontage)
    {
        for (const FSlotAnimationTrack& Slot : AttackMontage->SlotAnimTracks)
        {
            for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
            {
                UAnimSequenceBase* Reference = Segment.GetAnimReference();
                if (Reference)
                {
                    AttackSequence = Reference;
                    return;
                }
            }
        }
    }

    // 몽타주에서 못 찾았을 때의 예비 경로 (FBX 임포트 기본 이름)
    AttackSequence = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/Monster_Asset_Proto/Mutant_Swiping.Mutant_Swiping"));
    if (!AttackSequence)
    {
        UE_LOG(LogTemp, Warning, TEXT("MONSTER_ANIM %s: 공격 시퀀스를 찾지 못했습니다. 공격 애니메이션 없이 동작합니다."), *GetName());
    }
}

void AMonsterCharacter::StartLocomotionAnimation()
{
    bAttackAnimPlaying = false;
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!MeshComp || !MeshComp->GetSkeletalMeshAsset() || !LocomotionBlendSpace) return;

    MeshComp->PlayAnimation(LocomotionBlendSpace, true);
    UpdateLocomotionAnimation();
}

void AMonsterCharacter::UpdateLocomotionAnimation()
{
    if (bAttackAnimPlaying || !GetMesh()) return;

    if (UAnimSingleNodeInstance* Single = GetMesh()->GetSingleNodeInstance())
    {
        // 1D 블렌드 스페이스: X 값이 가로축(이동 속도)에 대응합니다.
        // (엔진 버전에 따라 이름이 다르면 SetBlendSpaceInput 로 바꿔 주세요)
        Single->SetBlendSpacePosition(FVector(GetVelocity().Size2D(), 0.f, 0.f));
    }
}

void AMonsterCharacter::PlayAttackAnimation(float TotalDuration)
{
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!AttackSequence || !MeshComp || !MeshComp->GetSkeletalMeshAsset()) return;

    bAttackAnimPlaying = true;
    MeshComp->PlayAnimation(AttackSequence, false);

    // 공격 전체 시간(준비+타격+회복)에 애니메이션 길이를 맞춥니다.
    const float Length = AttackSequence->GetPlayLength();
    if (UAnimSingleNodeInstance* Single = MeshComp->GetSingleNodeInstance())
    {
        Single->SetPlayRate(Length > 0.f ? Length / FMath::Max(0.1f, TotalDuration) : 1.f);
    }
}

// ---------------------------------------------------------------------------
// 피해 / 공격 판정
// ---------------------------------------------------------------------------

float AMonsterCharacter::TakeDamage(
    float DamageAmount,
    const FDamageEvent& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
    if (bIsDead || CurrentHealth <= 0.f || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f)
    {
        return 0.f;
    }

    const float AppliedDamage = FMath::Min(DamageAmount, CurrentHealth);
    CurrentHealth -= AppliedDamage;

    UE_LOG(LogTemp, Warning, TEXT("%s Health: %.0f / %.0f"), *GetName(), CurrentHealth, MaxHealth);

    if (CurrentHealth <= 0.f)
    {
        bIsDead = true;
        CancelMeleeAttack();
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        if (AMonsterAIController* AI = Cast<AMonsterAIController>(GetController()))
        {
            AI->StopMovement();
            if (AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Monster dead"));
        }
        SetActorEnableCollision(false);
        Destroy();
    }

    return AppliedDamage;
}

bool AMonsterCharacter::CanReachMeleeTarget(const AActor* Target) const
{
    return IsValidAttackTarget(const_cast<AActor*>(Target), false);
}

bool AMonsterCharacter::IsValidAttackTarget(AActor* Target, bool bCheckDirection) const
{
    if (const ADefenseBarricade* Barricade = Cast<ADefenseBarricade>(Target))
    {
        if (!Barricade->IsStanding()) return false;

        // 중심점이 아니라 충돌 범위(바운즈) 사이의 간격으로 판정 (길쭉한 방벽, 몬스터 크기 변화에 안전)
        FVector MC, ME, BC, BE;
        GetActorBounds(true, MC, ME);
        Target->GetActorBounds(true, BC, BE);
        const float Dx = FMath::Max(0.f, FMath::Abs(MC.X - BC.X) - (ME.X + BE.X));
        const float Dy = FMath::Max(0.f, FMath::Abs(MC.Y - BC.Y) - (ME.Y + BE.Y));
        const float Dz = FMath::Max(0.f, FMath::Abs(MC.Z - BC.Z) - (ME.Z + BE.Z));
        return FMath::Sqrt(Dx * Dx + Dy * Dy) <= AttackReach && Dz <= 80.f;
    }

    if (!IsAlive() || !IsValid(Target)) return false;
    const AMyProjectPrototypeCharacter* Player = Cast<AMyProjectPrototypeCharacter>(Target);
    const AColonyCore* Core = Cast<AColonyCore>(Target);
    if ((!Player || !Player->IsAlive()) && (!Core || !Core->IsAlive())) return false;
    const float TargetRadius = Player ? Player->GetCapsuleComponent()->GetScaledCapsuleRadius() : Core->GetTargetRadius();
    const FVector Delta = Target->GetActorLocation() - GetActorLocation();
    const float Gap = FMath::Max(0.f, Delta.Size2D()
        - GetCapsuleComponent()->GetScaledCapsuleRadius()
        - TargetRadius);
    if (Gap > AttackReach || FMath::Abs(Delta.Z) > 300.f) return false;
    if (bCheckDirection && FVector::DotProduct(Delta.GetSafeNormal2D(), AttackForward) < .573576f) return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterAttackVisibility), false, this);
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Params);
    return !bBlocked || Hit.GetActor() == Target;
}

bool AMonsterCharacter::TryStartMeleeAttack(AActor* Target)
{
    if (bAttackInProgress || GetWorld()->GetTimeSeconds() < NextAttackTime || !IsValidAttackTarget(Target, false)) return false;
    AttackTarget = Target;
    AttackForward = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    if (AttackForward.IsNearlyZero()) AttackForward = GetActorForwardVector();
    bAttackInProgress = true;
    bDamageApplied = false;
    AttackPhase = TEXT("Windup");
    NextAttackTime = GetWorld()->GetTimeSeconds() + FMath::Max(.01f, AttackCooldown);
    GetCharacterMovement()->StopMovementImmediately();
    if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    GetCharacterMovement()->bOrientRotationToMovement = false;
    SetActorRotation(AttackForward.Rotation());

    PlayAttackAnimation(WindupSeconds + HitWindowSeconds + RecoverySeconds);

    if (bShowAttackDebug)
    {
        const FVector Origin = GetActorLocation() + FVector(0, 0, 110);
        DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + AttackForward * 220.f, 30.f, FColor::Yellow, false, WindupSeconds, 0, 5.f);
        DrawDebugString(GetWorld(), Origin + FVector(0, 0, 25), TEXT("ATTACK!"), nullptr, FColor::Yellow, WindupSeconds, true);
    }
    UE_LOG(LogTemp, Display, TEXT("MONSTER_ATTACK %s Windup"), *GetName());
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::BeginHitWindow, FMath::Max(.01f, WindupSeconds), false);
    return true;
}

void AMonsterCharacter::BeginHitWindow()
{
    if (!IsAlive() || !bAttackInProgress) return;
    AttackPhase = TEXT("Strike");
    CheckMeleeHit();
    GetWorldTimerManager().SetTimer(HitTimer, this, &ThisClass::CheckMeleeHit, .03f, true);
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::BeginRecovery, FMath::Max(.01f, HitWindowSeconds), false);
}

void AMonsterCharacter::CheckMeleeHit()
{
    if (!IsAlive() || !bAttackInProgress || AttackPhase != TEXT("Strike") || bDamageApplied) return;
    AActor* Target = AttackTarget.Get();
    if (!IsValidAttackTarget(Target, true)) return;

    if (Cast<ADefenseBarricade>(Target))
    {
        bDamageApplied = true;
        const float Applied = UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
        UE_LOG(LogTemp, Display, TEXT("MONSTER_ATTACK %s Hit Barricade Damage=%.0f"), *GetName(), Applied);
        return;
    }

    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    const FVector Start = GetActorLocation() + AttackForward * (Radius + 5.f);
    const FVector End = GetActorLocation() + AttackForward * (Radius + AttackReach);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MonsterMeleeSweep), false, this);
    const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
        ECC_Pawn, FCollisionShape::MakeSphere(35.f), Params);
    if (bShowAttackDebug) DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, .2f, 0, 5.f);
    if (bHit && Hit.GetActor() == Target)
    {
        bDamageApplied = true;
        const float Applied = UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
        UE_LOG(LogTemp, Display, TEXT("MONSTER_ATTACK %s Hit Damage=%.0f"), *GetName(), Applied);
    }
}

void AMonsterCharacter::BeginRecovery()
{
    GetWorldTimerManager().ClearTimer(HitTimer);
    if (!IsAlive() || !bAttackInProgress) return;
    AttackPhase = TEXT("Recovery");
    UE_LOG(LogTemp, Display, TEXT("MONSTER_ATTACK %s Recovery"), *GetName());
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &ThisClass::FinishMeleeAttack, FMath::Max(.01f, RecoverySeconds), false);
}

void AMonsterCharacter::FinishMeleeAttack()
{
    bAttackInProgress = false;
    AttackPhase = TEXT("Idle");
    AttackTarget.Reset();
    GetCharacterMovement()->bOrientRotationToMovement = true;

    // 공격이 끝나면 Idle/Walk 블렌드 스페이스로 복귀
    if (bAttackAnimPlaying && IsAlive() && !IsActorBeingDestroyed())
    {
        StartLocomotionAnimation();
    }
}

void AMonsterCharacter::CancelMeleeAttack()
{
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().ClearTimer(HitTimer);
    FinishMeleeAttack();
}

void AMonsterCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelMeleeAttack();
    Super::EndPlay(Reason);
}