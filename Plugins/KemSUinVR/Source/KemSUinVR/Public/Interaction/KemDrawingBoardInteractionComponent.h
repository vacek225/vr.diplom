#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KemDrawingBoardInteractionComponent.generated.h"

class AKemDrawingBoardActor;
class UInputAction;
class UInputMappingContext;
class UMotionControllerComponent;

UCLASS(ClassGroup=(KemSUinVR), meta=(BlueprintSpawnableComponent))
class KEMSUINVR_API UKemDrawingBoardInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UKemDrawingBoardInteractionComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    friend class FKemRayGrabTest;
    bool TraceBoardRay(const FVector& Start, const FVector& Direction, float Range, FHitResult& OutHit) const;
    void ResolveVRDrawInputKey();
    void RefreshMotionControllers();
    void UpdateDesktopDrawing();
    void UpdateVRDrawing();
    bool IsVRDrawInputActive(APlayerController* PlayerController);
    bool TraceBoardFromCursorOrView(FHitResult& OutHit) const;
    bool TraceBoardFromMotionController(FHitResult& OutHit) const;
    FVector GetVRTraceStart() const;
    FVector GetVRTraceDirection() const;
    void SetActiveBoard(AKemDrawingBoardActor* NewBoard, const FString& DrawerId);
    bool IsLocalPawn() const;
    APlayerController* ResolvePlayerController() const;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> VRDrawPressedAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> VRMenuMappingContext;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> VRDefaultMappingContext;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> VRWeaponRightMappingContext;

    UPROPERTY(Transient)
    TObjectPtr<UMotionControllerComponent> RightGripMotionController;

    UPROPERTY(Transient)
    TObjectPtr<UMotionControllerComponent> RightAimMotionController;

    UPROPERTY(Transient)
    TObjectPtr<AKemDrawingBoardActor> ActiveDesktopBoard;

    UPROPERTY(Transient)
    TObjectPtr<AKemDrawingBoardActor> ActiveVRBoard;

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Drawing Board")
    float DesktopTraceDistance = 4000.0f;

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Drawing Board")
    float VRTraceDistance = 3000.0f;

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Drawing Board")
    float VRTraceStartOffset = 8.0f;

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Drawing Board")
    float VRDrawPressThreshold = 0.55f;

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Drawing Board")
    float VRDrawReleaseThreshold = 0.15f;

    float DrawSendTime = 0.f;
    bool bDesktopDrawDownLastFrame = false;
    bool bVRDrawInputActive = false;
    FKey VRDrawInputKey;
};
