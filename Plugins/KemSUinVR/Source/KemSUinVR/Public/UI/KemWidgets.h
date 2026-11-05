#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Core/KemTypes.h"
#include "KemWidgets.generated.h"

class UBorder;
class UEditableTextBox;
class USizeBox;
class USlider;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
class UWebBrowser;
class AKemDrawingBoardActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKemActionClickedSignature, const FString&, ActionId);

UCLASS()
class KEMSUINVR_API UKemActionButton : public UButton
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "KemSUinVR")
    FKemActionClickedSignature OnKemActionClicked;

    UPROPERTY()
    FString ActionId;

    void EnsureBinding();

private:
    UFUNCTION()
    void HandleInternalClicked();

    bool bBound = false;
};

UCLASS(Abstract)
class KEMSUINVR_API UKemPanelWidgetBase : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void BuildUI();

    UBorder* CreatePanelBorder(const FLinearColor& Tint) const;
    UVerticalBox* CreateVerticalBox() const;
    UTextBlock* CreateTextBlock(const FString& Text, int32 Size, const FLinearColor& Color = FLinearColor::White) const;
    UKemActionButton* CreateActionButton(const FString& Label, const FString& ActionId) const;
    UEditableTextBox* CreateTextBox(const FString& Hint) const;
    USlider* CreateSlider(float InitialValue) const;
};

UCLASS()
class KEMSUINVR_API UKemHubMenuWidget : public UKemPanelWidgetBase
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

protected:
    virtual void BuildUI() override;

private:
    void RebuildMenuState();
    class UKemRoomSubsystem* GetRoomSubsystem() const;

    UFUNCTION()
    void HandleRootAction(const FString& ActionId);

    UFUNCTION()
    void HandleAuditoriumAction(const FString& ActionId);

    UFUNCTION()
    void HandleKeyboardAction(const FString& ActionId);

    UFUNCTION()
    void HandleHostResult(bool bSuccess, const FString& Message);

    UFUNCTION()
    void HandleJoinResult(bool bSuccess, const FString& Message);

    UFUNCTION()
    void HandleRoomInfoChanged(const FKemRoomInfo& RoomInfo);

private:
    EKemHubMenuState MenuState = EKemHubMenuState::Root;
    FString PendingJoinCode;

    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> ContentBox;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> JoinCodePreviewText;
};

UCLASS()
class KEMSUINVR_API UKemRoomCodeWidget : public UKemPanelWidgetBase
{
    GENERATED_BODY()

public:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
    virtual void BuildUI() override;

private:
    class UKemRoomSubsystem* GetRoomSubsystem() const;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> HeaderText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CodeText;
};

UCLASS()
class KEMSUINVR_API UKemReturnToLobbyWidget : public UKemPanelWidgetBase
{
    GENERATED_BODY()

protected:
    virtual void BuildUI() override;

private:
    UFUNCTION()
    void HandleAction(const FString& ActionId);
};

UCLASS()
class KEMSUINVR_API UKemDrawingBoardControlWidget : public UKemPanelWidgetBase
{
    GENERATED_BODY()

public:
    void SetTargetBoard(AKemDrawingBoardActor* InTargetBoard);

protected:
    virtual void BuildUI() override;

private:
    void RefreshStatusText();

    UFUNCTION()
    void HandleAction(const FString& ActionId);

private:
    UPROPERTY(Transient)
    TObjectPtr<AKemDrawingBoardActor> TargetBoard;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;
};

UCLASS()
class KEMSUINVR_API UKemTeacherControlWidget : public UKemPanelWidgetBase
{
    GENERATED_BODY()

protected:
    virtual void BuildUI() override;

private:
    class AKemClassroomStateActor* GetRoomState() const;

    UFUNCTION()
    void HandleAction(const FString& ActionId);
};

UCLASS()
class KEMSUINVR_API UKemBrowserDeskWidget : public UKemPanelWidgetBase
{
    GENERATED_BODY()

public:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
    virtual void BuildUI() override;

private:
    class AKemClassroomStateActor* GetRoomState() const;
    void LoadUrl(const FString& Url, bool bReplicateToRoomState);

    UFUNCTION()
    void HandleAction(const FString& ActionId);

private:
    FString CachedUrl;

    UPROPERTY(Transient)
    TObjectPtr<UEditableTextBox> UrlTextBox;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CurrentUrlText;

    UPROPERTY(Transient)
    TObjectPtr<UWebBrowser> BrowserWidget;
};

UCLASS()
class KEMSUINVR_API UKemLectureScreenWidget : public UKemPanelWidgetBase
{
    GENERATED_BODY()

public:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    void SetAllowLocalCloneButton(bool bAllowClone);

protected:
    virtual void BuildUI() override;

private:
    class AKemClassroomStateActor* GetRoomState() const;
    void LoadLectureUrl(const FString& Url);

    UFUNCTION()
    void HandleAction(const FString& ActionId);

private:
    bool bAllowCloneButton = false;
    FString CachedLectureUrl;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> UrlText;

    UPROPERTY(Transient)
    TObjectPtr<UWebBrowser> BrowserWidget;

    UPROPERTY(Transient)
    TObjectPtr<UKemActionButton> CloneButton;
};

UCLASS()
class KEMSUINVR_API UKemParticipantModeratorWidget : public UKemPanelWidgetBase
{
    GENERATED_BODY()

public:
    void SetParticipantInfo(const FKemParticipantInfo& InParticipantInfo);

protected:
    virtual void BuildUI() override;

private:
    class AKemClassroomStateActor* GetRoomState() const;

    UFUNCTION()
    void HandleAction(const FString& ActionId);

    UFUNCTION()
    void HandleVolumeChanged(float NewValue);

private:
    FKemParticipantInfo ParticipantInfo;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> NameText;

    UPROPERTY(Transient)
    TObjectPtr<USlider> VolumeSlider;
};
