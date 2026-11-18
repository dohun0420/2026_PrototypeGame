#pragma once

#include "DefenseBarricade.h"
#include "EditableDefenseBarricade.generated.h"

UCLASS()
class MYPROJECTPROTOTYPE_API AEditableDefenseBarricade : public ADefenseBarricade
{
    GENERATED_BODY()

public:
    AEditableDefenseBarricade();
    void OnConstruction(const FTransform& Transform) override;
    bool IsStanding() const override;

private:
    UPROPERTY(EditInstanceOnly, Category = "Layout", meta = (MakeEditWidget))
    FTransform WallEnd = FTransform(FVector(270.0, 0.0, 0.0));

    UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "2.0", Units = "cm"))
    float WallHeight = 200.0f;

    UPROPERTY(EditAnywhere, Category = "Layout", meta = (ClampMin = "2.0", Units = "cm"))
    float WallThickness = 90.0f;
};
