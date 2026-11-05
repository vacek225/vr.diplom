#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KemVRGrabInteractionComponent.generated.h"

class UInputAction;
class UMotionControllerComponent;
class UPrimitiveComponent;
class USceneComponent;
class AKemEnginePart;

UCLASS(ClassGroup=(KemSUinVR), meta=(BlueprintSpawnableComponent))
class KEMSUINVR_API UKemVRGrabInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UKemVRGrabInteractionComponent();
    bool IsHandOccupied(bool bRightHand) const;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    enum class EKemGrabHand : uint8
    {
        Left,
        Right
    };

    struct FGrabState
    {
        TWeakObjectPtr<UPrimitiveComponent> HeldComponent;
        bool bWasSimulatingPhysics = false;
        ECollisionEnabled::Type CollisionBeforeGrab = ECollisionEnabled::NoCollision;
        TWeakObjectPtr<AKemEnginePart> EnginePart;
        FTransform PartRelativeToHand;
        float PoseTimer = 0;
        float PendingTime = 0;
        bool bConfirmed = false;
        bool bRayLatched = false;
        bool bManualHold = false;
        bool bPulling = false;
        FTransform PullPose;
    };

    void PollGrip(EKemGrabHand Hand);
    void RefreshMotionControllers();
    bool IsLocalVRPawn() const;
    USceneComponent* GetHandComponent(EKemGrabHand Hand) const;
    UPrimitiveComponent* ResolveBestCandidateNearHand(const FVector& HandLocation) const;
    bool IsComponentGrabbable(const UPrimitiveComponent* PrimitiveComponent) const;
    bool TryGrab(EKemGrabHand Hand);
    void ReleaseHand(EKemGrabHand Hand);
    void UpdateEngineHand(EKemGrabHand Hand, float DeltaSeconds);
    bool TryRayGrab(EKemGrabHand Hand);
    void PollRayGrab(EKemGrabHand Hand, float DeltaSeconds);
    void UpdatePulledObject(EKemGrabHand Hand, float DeltaSeconds);
    FTransform GetHeldPose(FGrabState& Grab, const FTransform& Hand, float DeltaSeconds) const;
    static bool IsLobbyProp(const UPrimitiveComponent* Component);
    friend class FKemRayGrabTest;
    FGrabState& GetGrabState(EKemGrabHand Hand);
    const FGrabState& GetGrabState(EKemGrabHand Hand) const;

    UFUNCTION()
    void HandleLeftGrabPressed();

    UFUNCTION()
    void HandleLeftGrabReleased();

    UFUNCTION()
    void HandleRightGrabPressed();

    UFUNCTION()
    void HandleRightGrabReleased();

private:
    UPROPERTY(Transient)
    TObjectPtr<UMotionControllerComponent> LeftGripMotionController;

    UPROPERTY(Transient)
    TObjectPtr<UMotionControllerComponent> LeftAimMotionController;

    UPROPERTY(Transient)
    TObjectPtr<UMotionControllerComponent> RightGripMotionController;

    UPROPERTY(Transient)
    TObjectPtr<UMotionControllerComponent> RightAimMotionController;

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Grab")
    float GrabRadius = 22.0f;

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Grab")
    FName GrabbableTag = TEXT("Kem.Grabbable");

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Grab")
    FString TargetStaticMeshPath = TEXT("/Game/KUB/lobby/AppleLowPoly.AppleLowPoly");

    bool bLeftRayTriggerDown = false;
    bool bRightRayTriggerDown = false;
    bool bLeftGripDown = false;
    bool bRightGripDown = false;
    FGrabState LeftGrabState;
    FGrabState RightGrabState;
    TWeakObjectPtr<AKemEnginePart> LeftFocus;
    TWeakObjectPtr<AKemEnginePart> RightFocus;
};
