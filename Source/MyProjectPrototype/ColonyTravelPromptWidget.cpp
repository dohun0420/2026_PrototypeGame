#include "ColonyTravelPromptWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

void UColonyTravelPromptWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    PromptTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(),
        TEXT("ColonyTravelPromptText"));
    WidgetTree->RootWidget = PromptTextBlock;

    PromptTextBlock->SetText(PendingText);
    PromptTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.82f, 0.22f, 1.f)));
    PromptTextBlock->SetFontSize(24.f);
    PromptTextBlock->SetShadowOffset(FVector2D(1.5f, 1.5f));
    PromptTextBlock->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
}

void UColonyTravelPromptWidget::SetPromptText(const FText& InText)
{
    PendingText = InText;
    if (PromptTextBlock)
    {
        PromptTextBlock->SetText(PendingText);
    }
}

void UColonyTravelPromptWidget::SetPromptVisible(bool bVisible)
{
    SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
