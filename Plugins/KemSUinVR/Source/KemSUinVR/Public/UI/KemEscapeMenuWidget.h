#pragma once
#include "CoreMinimal.h"
#include "UI/KemWidgets.h"
#include "KemEscapeMenuWidget.generated.h"

class UHorizontalBox;
UCLASS()
class KEMSUINVR_API UKemEscapeMenuWidget : public UKemPanelWidgetBase
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) void SelectSection(const FString& Section);
    UFUNCTION(BlueprintCallable) void ResetNavigation();
    UFUNCTION(BlueprintPure) FString GetSelectedSection() const { return Tab; }
    UFUNCTION(BlueprintPure) AActor* GetSelectedSceneObject() const { return SelectedObject.Get(); }
    UFUNCTION(BlueprintCallable) void SelectCatalogObject(FName ObjectId);
    void SetVRLayout(bool Value) { bVRLayout = Value; }
    FVector2D GetDesignSize() const { return bVRLayout ? FVector2D(1000,1280) : FVector2D(1120,1000); }
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
protected:
    virtual void BuildUI() override;
private:
    void RefreshPage();
    class UKemMenuArtWidget* MakeIcon(const FString& Symbol, float Size = 24.f);
    UBorder* MakeSurface(FLinearColor Color, float Radius = 18.f);
    void BuildToolsPage();
    bool bVRLayout = false;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ThicknessLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> VolumeLabel;
    UPROPERTY(Transient) TObjectPtr<class UKemMenuArtWidget> StrokePreview;
    void BuildObjectPage();
    void AddObjectCard(class UPanelWidget* Parent, const FString& Name, const FString& Id, bool bEnabled);
    void AddPreview(class UPanelWidget* Parent, const FString& Id, float Height);
    void AddLabel(const FString& Text, int32 Size = 22);
    void AddAction(const FString& Text, const FString& Id, bool bEnabled = true);
    USlider* MakeMenuSlider(float Value);
    UEditableTextBox* MakeEntry(const FString& Hint);
    UKemActionButton* MakeButton(const FString& Text, const FString& Id);
    void AddKeyboard(UEditableTextBox* Target, const FString& Characters);
    UFUNCTION() void OnAction(const FString& Id);
    UFUNCTION() void OnVolume(float Value);
    UFUNCTION() void OnVoiceVolume(float Value);
    UFUNCTION() void OnThickness(float Value);
    UFUNCTION() void OnRoomResult(bool bSuccess, const FString& Message);
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(Transient) TArray<TObjectPtr<UKemActionButton>> SectionButtons;
    UPROPERTY(Transient) TObjectPtr<class UScrollBox> PageScroll;
    UPROPERTY(Transient) TObjectPtr<UKemActionButton> LobbyButton;
    TWeakObjectPtr<AActor> SelectedObject;
    FString SelectedObjectId;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> Page;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RoomLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RoleLabel;
    UPROPERTY(Transient) TObjectPtr<UKemActionButton> MicrophoneButton;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> TextEntry;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> KeyboardTarget;
    FString Tab = TEXT("room");
    FString Confirmation;
    FString LastRoomSignature;
    float RefreshTime = 0.f;
};
