#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KemVRWidgetInteractorComponent.generated.h"
class UMotionControllerComponent;
class UStaticMeshComponent;
class UWidgetInteractionComponent;
class APlayerController;

USTRUCT()
struct FKemUIHand
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<UMotionControllerComponent> Aim;
    UPROPERTY(Transient) TObjectPtr<UWidgetInteractionComponent> Interaction;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Beam;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Cursor;
    bool bTriggerDown = false;
    bool bPointerPressed = false;
    bool bDragging = false;
};

/** Independent virtual pointers for both controllers. Grip remains a gameplay input. */
UCLASS(ClassGroup=(KemSUinVR), meta=(BlueprintSpawnableComponent))
class KEMSUINVR_API UKemVRWidgetInteractorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UKemVRWidgetInteractorComponent();
    bool IsOverUI() const;
    bool IsOverUI(bool bRightHand) const;
    void CancelInteractions();
    static bool AreTriggersHeld(const APlayerController* PC);
    static float GetTriggerStrength(const APlayerController* PC, bool bRightHand);
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
    void EnsureHand(FKemUIHand& Hand, bool bRightHand);
    void UpdateHand(FKemUIHand& Hand, bool bRightHand, float DeltaTime);
    void ReleaseHand(FKemUIHand& Hand, bool bRightHand, bool bCancel = false);
    UPROPERTY(Transient) FKemUIHand Left;
    UPROPERTY(Transient) FKemUIHand Right;
    UPROPERTY(EditAnywhere, Category="KemSUinVR|VR UI") float LaserDistance = 2500.f;
    UPROPERTY(EditAnywhere, Category="KemSUinVR|VR UI") float LaserThickness = .25f;
    friend class FKemVRMenuInteractionTest;
};
