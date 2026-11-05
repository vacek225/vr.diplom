#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"
#include "KemEscapeMenuSubsystem.generated.h"

class UKemEscapeMenuWidget;
class UWidgetComponent;
class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
class IInputProcessor;

UENUM(BlueprintType)
enum class EKemTool : uint8 { Brush, Eraser, Pointer };

/** Local presentation and input state. Shared room actions remain server-authoritative. */
UCLASS()
class KEMSUINVR_API UKemEscapeMenuSubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UKemEscapeMenuSubsystem, STATGROUP_Tickables); }
    virtual bool IsTickable() const override { return !IsTemplate(); }
    virtual UWorld* GetTickableGameObjectWorld() const override;
    UFUNCTION(BlueprintPure, Category="KemSUinVR|Menu")
    static UKemEscapeMenuSubsystem* Get(const APlayerController* PC);
    UFUNCTION(BlueprintCallable) void ToggleMenu();
    UFUNCTION(BlueprintCallable) void SetMenuOpen(bool bOpen);
    UFUNCTION(BlueprintPure) bool IsMenuOpen() const { return bMenuOpen; }
    UFUNCTION(BlueprintPure) bool IsVR() const;
    bool CanUseTool() const { return !bMenuOpen && !bWaitForRelease; }
    void SetTool(EKemTool NewTool);
    UFUNCTION(BlueprintCallable) void TogglePointer();
    UFUNCTION(BlueprintPure) bool IsPointerEnabled() const { return bPointerEnabled; }
    bool CanManageSceneObjects() const;
    bool CanManageSceneObject(const AActor* Object) const;
    AActor* SpawnSceneObject(FName ObjectId);
    bool RemoveSceneObject(AActor* Object);
    bool MoveSceneObjectInFront(AActor* Object);
    FTransform GetObjectSpawnTransform() const;
    void SetMasterVolume(float Value);
    void SetVoiceVolume(float Value);
    UFUNCTION(BlueprintPure) UKemEscapeMenuWidget* GetMenuWidget() const { return MenuWidget; }
    void SetMicrophoneEnabled(bool bEnabled);
    void SetForcedMuted(bool bMuted);
    void RepositionPanel();
    bool BeginPanelDrag(bool bRightHand, const FTransform& HandPose, const FHitResult& Hit);
    void UpdatePanelDrag(bool bRightHand, const FTransform& HandPose);
    void EndPanelDrag(bool bRightHand);
    bool IsPanelDragging() const { return bPanelDragging; }
    static bool IsPanelEdge(const FVector2D& Point, const FVector2D& Size);
    // Pointer enum value is retained as the idle drawing state for compatibility.
    EKemTool Tool = EKemTool::Pointer;
    FLinearColor BrushColor = FLinearColor(FColor::FromHex(TEXT("8BDCF5")));
    float BrushSize = 6.f;
    float MasterVolume = 1.f;
    float VoiceVolume = 1.f;
    bool bMicrophoneEnabled = true;
    bool bForcedMuted = false;
private:
    void SavePreferences();
    void EnsureInput();
    void DestroyPresentation();
    UPROPERTY(Transient) TObjectPtr<UKemEscapeMenuWidget> MenuWidget;
    UPROPERTY(Transient) TObjectPtr<UWidgetComponent> WorldWidget;
    UPROPERTY(Transient) TObjectPtr<AActor> PanelActor;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> GripMapping;
    UPROPERTY(Transient) TObjectPtr<UInputAction> ToggleAction;
    TWeakObjectPtr<APlayerController> ActiveController;
    TWeakObjectPtr<APawn> ActivePawn;
    TSharedPtr<IInputProcessor> EscapeProcessor;
    bool bMenuButtonDown = false;
    bool bPanelDragging = false;
    bool bDragRightHand = false;
    FTransform PanelRelativeToHand;
    friend class FKemVRMenuInteractionTest;
    FString LastSection = TEXT("room");
    bool bMenuOpen = false;
    bool bHadPawn = false;
    bool bWaitForRelease = false;
    bool bAppliedInputLock = false;
    bool bWasCursorVisible = false;
    bool bWasClickEnabled = false;
    bool bWasHoverEnabled = false;
    float AudioRefresh = 0.f;
    float PointerRefresh = 0.f;
    bool bPointerWasActive = false;
    bool bPointerEnabled = false;
    TArray<TWeakObjectPtr<AActor>> LobbyObjects;
};
