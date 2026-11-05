#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/KemEngineAssembly.h"
#include "KemPlayerControlProxy.generated.h"

UCLASS()
class KEMSUINVR_API AKemPlayerControlProxy : public AActor
{
    GENERATED_BODY()

public:
    AKemPlayerControlProxy();

    void ActivateDesktopModeForOwner();

    UFUNCTION(Client, Reliable)
    void ClientEvaluateLocalControlMode();

    UFUNCTION(Server, Reliable)
    void ServerRequestDesktopMode();

    static AKemPlayerControlProxy* FindForController(const APlayerController* Controller);

    UFUNCTION(Server, Reliable)
    void ServerGrabEnginePart(class AKemEnginePart* Part, bool bRightHand, FTransform Hand);
    UFUNCTION(Server, Reliable)
    void ServerRayGrabEnginePart(class AKemEnginePart* Part, bool bRightHand, FTransform Ray);
    UFUNCTION(Server, Unreliable)
    void ServerMoveEnginePart(class AKemEnginePart* Part, bool bRightHand, FTransform Pose);
    UFUNCTION(Server, Reliable)
    void ServerReleaseEnginePart(class AKemEnginePart* Part, bool bRightHand, FTransform FinalPose);
    UFUNCTION(Server, Reliable)
    void ServerConfigureEngine(AKemEngineAssembly* Engine, EKemEngineLesson Mode, bool bReset);
    UFUNCTION(Client, Reliable)
    void ClientEngineFeedback(const FString& Message);

    UFUNCTION(Server, Unreliable)
    void ServerUpdatePointer(FVector_NetQuantize Start, FVector_NetQuantize End, bool bEnabled);
    UFUNCTION(Server, Reliable)
    void ServerDrawBoard(class AKemDrawingBoardActor* Board, FVector2D UV, bool bStart, FLinearColor Color, float Size);
    FString GetEngineFeedback() const;
private:
    UPROPERTY(Transient) TObjectPtr<class AKemPointerActor> PointerActor;
    float LastPointerUpdate = -100.f;
    float LastDrawUpdate = -100.f;
    FString EngineFeedback;
    float FeedbackUntil = 0;
};
