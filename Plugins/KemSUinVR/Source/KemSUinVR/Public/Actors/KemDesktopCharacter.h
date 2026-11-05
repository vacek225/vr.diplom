#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "KemDesktopCharacter.generated.h"

class UCameraComponent;
class UWidgetInteractionComponent;
class AKemLocalLectureScreenActor;
class AKemSharedLectureScreenActor;
class AKemEnginePart;
enum class EKemScreenHandleType : uint8;

UCLASS()
class KEMSUINVR_API AKemDesktopCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AKemDesktopCharacter();
    void SuspendForMenu();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

protected:
    void UpdateDesktopMovement(float DeltaSeconds);
    void UpdateDesktopInteraction();
    bool TraceDesktopInteraction(FHitResult& OutHit) const;
    void ClearActiveScreenDrag();
    void EnsureDesktopOverlayVisibility();
    void ApplyDesktopGameplayInputMode() const;
    void UpdateEngineInteraction(float DeltaSeconds);
    void ReleaseEnginePart();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR")
    TObjectPtr<UCameraComponent> CameraComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR")
    TObjectPtr<UWidgetInteractionComponent> WidgetInteraction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Desktop")
    float DesktopMoveSpeed = 700.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Desktop")
    float DesktopLookSensitivity = 0.11f;

private:
    bool bWasPrimaryMouseButtonDown = false;
    bool bWidgetPointerPressed = false;
    bool bDesktopMenuVisible = false;

    TWeakObjectPtr<AKemLocalLectureScreenActor> ActiveDraggedScreen;
    EKemScreenHandleType ActiveDragHandleType;
    float ActiveDragDistance = 0.0f;
    TWeakObjectPtr<AKemEnginePart> HeldEnginePart;
    TWeakObjectPtr<AKemEnginePart> FocusedEnginePart;
    FTransform EngineHandOffset;
    float EngineGrabDistance = 100;
    float EnginePoseTimer = 0;
    float EnginePendingTime = 0;
    bool bEngineGrabConfirmed = false;
};
