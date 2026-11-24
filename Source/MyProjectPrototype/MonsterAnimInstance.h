#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "MonsterAnimInstance.generated.h"

USTRUCT()
struct FMonsterAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FMonsterAnimInstanceProxy() : FAnimInstanceProxy() {}
	FMonsterAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	UPROPERTY(Transient)
	TObjectPtr<class UBlendSpace1D> LocomotionBlendSpace = nullptr;

	float Speed = 0.0f;
	float CurrentPlayTime = 0.0f;
	int32 CachedTriangulationIndex = -1;
	TArray<FBlendSampleData> BlendSampleDataCache;
};

UCLASS()
class MYPROJECTPROTOTYPE_API UMonsterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UMonsterAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return &Proxy; }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override {}

private:
	FMonsterAnimInstanceProxy Proxy;

	UPROPERTY()
	TObjectPtr<class ACharacter> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<class UBlendSpace1D> LocomotionBlendSpace;
};