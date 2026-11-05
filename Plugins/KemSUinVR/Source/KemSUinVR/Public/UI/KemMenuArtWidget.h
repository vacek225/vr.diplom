#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "KemMenuArtWidget.generated.h"

// Resolution-independent line icons and brush sample for both desktop and world UMG.
UCLASS()
class KEMSUINVR_API UKemMenuArtWidget : public UWidget
{
    GENERATED_BODY()
public:
    FString Symbol;
    FLinearColor Color = FLinearColor::White;
    float Thickness = 3.f;
    void Refresh();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
    TSharedPtr<class SKemMenuArt> Art;
};
