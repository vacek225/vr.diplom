#pragma once

#include "CoreMinimal.h"
#include "UI/KemWidgets.h"
#include "KemEngineTrainingWidget.generated.h"

class AKemEngineAssembly;

UCLASS()
class KEMSUINVR_API UKemEnginePartLabelWidget : public UKemPanelWidgetBase
{
    GENERATED_BODY()
public:
    void SetPartName(const FString& Name);
protected:
    virtual void BuildUI() override;
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
    FString PartName;
};

UCLASS()
class KEMSUINVR_API UKemEngineTrainingWidget : public UKemPanelWidgetBase
{
    GENERATED_BODY()
public:
    void SetEngine(AKemEngineAssembly* InEngine) { Engine = InEngine; }
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
protected:
    virtual void BuildUI() override;
private:
    UFUNCTION() void HandleAction(const FString& Action);
    UPROPERTY(Transient) TObjectPtr<AKemEngineAssembly> Engine;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
    UPROPERTY(Transient) TArray<TObjectPtr<UKemActionButton>> HostButtons;
    float RefreshTime = 0.0f;
};
