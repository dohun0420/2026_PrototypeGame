#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ColonyTravelPromptWidget.generated.h"

class UTextBlock;

/** Minimal native UI used so the interaction prompt is not mixed with debug messages. */
UCLASS()
class MYPROJECTPROTOTYPE_API UColonyTravelPromptWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetPromptText(const FText& InText);
    void SetPromptVisible(bool bVisible);

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PromptTextBlock;

    FText PendingText;
};
