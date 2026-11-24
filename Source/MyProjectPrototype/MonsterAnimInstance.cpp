#include "MonsterAnimInstance.h"
#include "GameFramework/Character.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimTypes.h"
#include "UObject/ConstructorHelpers.h"

void FMonsterAnimInstanceProxy::Update(float DeltaSeconds)
{
	FAnimInstanceProxy::Update(DeltaSeconds);

	if (!LocomotionBlendSpace) return;

	// UE 5.8: FBlendFilter를 포함한 4인자 시그니처 또는 샘플 직접 계산
	FBlendFilter BlendFilter;
	LocomotionBlendSpace->InitializeFilter(&BlendFilter);
	LocomotionBlendSpace->GetSamplesFromBlendInput(
		FVector(Speed, 0.0f, 0.0f),
		BlendSampleDataCache,
		CachedTriangulationIndex,
		true
	);

	float PlayLength = 1.0f;
	for (const FBlendSampleData& Sample : BlendSampleDataCache)
	{
		if (Sample.Animation && Sample.TotalWeight > 0.5f)
		{
			PlayLength = Sample.Animation->GetPlayLength();
			break;
		}
	}

	CurrentPlayTime += DeltaSeconds;
	if (PlayLength > 0.0f && CurrentPlayTime > PlayLength)
	{
		CurrentPlayTime = FMath::Fmod(CurrentPlayTime, PlayLength);
	}
}

bool FMonsterAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	// 1. BlendSpace(Idle <-> Walk) 포즈 계산
	if (LocomotionBlendSpace && BlendSampleDataCache.Num() > 0)
	{
		FAnimationPoseData OutPoseData(Output);
		LocomotionBlendSpace->GetAnimationPose(
			BlendSampleDataCache,
			FAnimExtractContext(static_cast<double>(CurrentPlayTime), true),
			OutPoseData
		);
	}
	else
	{
		Output.ResetToRefPose();
	}

	// 2. 공격 몽타주가 재생 중이면 해당 시점의 몽타주 포즈를 직접 추출해 덮어씌움
	if (UAnimInstance* AnimInst = Cast<UAnimInstance>(GetAnimInstanceObject()))
	{
		if (UAnimMontage* ActiveMontage = AnimInst->GetCurrentActiveMontage())
		{
			const float MontagePos = AnimInst->Montage_GetPosition(ActiveMontage);
			FAnimationPoseData MontagePoseData(Output);
			ActiveMontage->GetAnimationPose(
				MontagePoseData,
				FAnimExtractContext(static_cast<double>(MontagePos), true)
			);
		}
	}

	return true;
}

UMonsterAnimInstance::UMonsterAnimInstance()
{
	static ConstructorHelpers::FObjectFinder<UBlendSpace1D> BSAsset(
		TEXT("/Game/Monster_Asset_Proto/BS_Monster_Movement.BS_Monster_Movement")
	);
	if (BSAsset.Succeeded())
	{
		LocomotionBlendSpace = BSAsset.Object;
	}
}

void UMonsterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());
	Proxy.LocomotionBlendSpace = LocomotionBlendSpace;
}

void UMonsterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());
		if (!OwnerCharacter) return;
	}

	FVector Velocity = OwnerCharacter->GetVelocity();
	Velocity.Z = 0.0f;
	Proxy.Speed = Velocity.Size();
}