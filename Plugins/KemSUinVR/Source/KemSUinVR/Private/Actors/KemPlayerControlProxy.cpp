#include "Actors/KemPlayerControlProxy.h"
#include "Actors/KemPointerActor.h"
#include "Actors/KemDrawingBoardActor.h"

#include "Actors/KemDesktopCharacter.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "IXRTrackingSystem.h"
#include "Engine/KemEnginePart.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AKemPlayerControlProxy::AKemPlayerControlProxy()
{
    bReplicates = true;
    bOnlyRelevantToOwner = true;
    SetReplicateMovement(false);
}

void AKemPlayerControlProxy::ActivateDesktopModeForOwner()
{
    APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
    if (!PlayerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("KemSUinVR: Desktop mode activation skipped because owner is not a player controller."));
        return;
    }

    if (PlayerController->GetPawn() && PlayerController->GetPawn()->IsA<AKemDesktopCharacter>())
    {
        UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Desktop character already active for %s."), *PlayerController->GetName());
        return;
    }

    APawn* ExistingPawn = PlayerController->GetPawn();
    const FVector SpawnLocation = ExistingPawn ? ExistingPawn->GetActorLocation() : GetActorLocation();
    const FRotator SpawnRotation = PlayerController->GetControlRotation();

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = PlayerController;
    SpawnParameters.Instigator = ExistingPawn;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AKemDesktopCharacter* DesktopCharacter = GetWorld()->SpawnActor<AKemDesktopCharacter>(
        AKemDesktopCharacter::StaticClass(),
        SpawnLocation,
        FRotator(0.0f, SpawnRotation.Yaw, 0.0f),
        SpawnParameters);
    if (!DesktopCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("KemSUinVR: Failed to spawn desktop fallback character."));
        return;
    }

    PlayerController->Possess(DesktopCharacter);
    UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Desktop fallback character possessed by %s at %s."),
        *PlayerController->GetName(),
        *DesktopCharacter->GetActorLocation().ToCompactString());

    if (ExistingPawn)
    {
        ExistingPawn->Destroy();
    }
}

void AKemPlayerControlProxy::ClientEvaluateLocalControlMode_Implementation()
{
    bool bDesktopFallbackRequested = true;
    if (GEngine && GEngine->XRSystem.IsValid())
    {
        const bool bStereoRenderingActive = GEngine->IsStereoscopic3D();
        const bool bTrackedHMDPresent = GEngine->XRSystem->CountTrackedDevices(EXRTrackedDeviceType::HeadMountedDisplay) > 0;
        bDesktopFallbackRequested = !(bStereoRenderingActive && bTrackedHMDPresent);
    }

    if (bDesktopFallbackRequested && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            4.0f,
            FColor(24, 205, 255),
            TEXT("KemSUinVR: Desktop fallback enabled"));
    }

    UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Local control mode evaluated. Desktop fallback requested = %s."),
        bDesktopFallbackRequested ? TEXT("true") : TEXT("false"));

    if (bDesktopFallbackRequested)
    {
        ServerRequestDesktopMode();
    }
}

void AKemPlayerControlProxy::ServerRequestDesktopMode_Implementation()
{
    ActivateDesktopModeForOwner();
}

AKemPlayerControlProxy* AKemPlayerControlProxy::FindForController(const APlayerController* Controller)
{
    if (Controller && Controller->GetWorld())
        for (TActorIterator<AKemPlayerControlProxy> It(Controller->GetWorld()); It; ++It)
            if (It->GetOwner()==Controller) return *It;
    return nullptr;
}

void AKemPlayerControlProxy::ServerGrabEnginePart_Implementation(AKemEnginePart* Part, bool bRightHand, FTransform Hand)
{
    if (!IsValid(Part)) return;
    for (TActorIterator<AKemEnginePart> It(GetWorld()); It; ++It)
        if (It->IsOwnedByProxy(this) && It->IsHeldBy(Cast<APlayerController>(GetOwner())->GetPawn(),bRightHand))
        {
            ClientEngineFeedback(TEXT("Эта рука уже держит деталь"));
            return;
        }
    FString Reason;
    if (!Part->TryAcquire(this,bRightHand,Hand,Reason)) ClientEngineFeedback(Reason);
}

void AKemPlayerControlProxy::ServerMoveEnginePart_Implementation(AKemEnginePart* Part, bool bRightHand, FTransform Pose)
{
    if (IsValid(Part)) Part->MoveHeld(this,bRightHand,Pose);
}

void AKemPlayerControlProxy::ServerRayGrabEnginePart_Implementation(AKemEnginePart* Part, bool bRightHand, FTransform Ray)
{
    auto* PC=Cast<APlayerController>(GetOwner());
    if (!PC || !PC->GetPawn() || !IsValid(Part) || Part->GetWorld()!=GetWorld()) return;
    for (TActorIterator<AKemEnginePart> It(GetWorld()); It; ++It)
        if (It->IsOwnedByProxy(this) && It->IsHeldBy(PC->GetPawn(),bRightHand)) return;
    FString Reason;
    if (!Part->TryAcquireFromRay(this,bRightHand,Ray,Reason)) ClientEngineFeedback(Reason);
}

void AKemPlayerControlProxy::ServerReleaseEnginePart_Implementation(AKemEnginePart* Part, bool bRightHand, FTransform FinalPose)
{
    if (!IsValid(Part)) return;
    Part->MoveHeld(this,bRightHand,FinalPose);
    FString Reason;
    if (Part->Release(this,bRightHand,Reason)) ClientEngineFeedback(Reason);
}

void AKemPlayerControlProxy::ServerConfigureEngine_Implementation(AKemEngineAssembly* Engine, EKemEngineLesson Mode, bool bReset)
{
    const APlayerController* PC=Cast<APlayerController>(GetOwner());
    // The LAN host is the local controller on the authoritative machine.
    if (!IsValid(Engine) || !PC || !PC->IsLocalController()) return;
    if (!Engine->ConfigureLesson(Mode,bReset)) ClientEngineFeedback(TEXT("Сначала отпустите все детали"));
}

void AKemPlayerControlProxy::ClientEngineFeedback_Implementation(const FString& Message)
{
    EngineFeedback=Message;
    FeedbackUntil=GetWorld()->GetTimeSeconds()+5.0f;
}

FString AKemPlayerControlProxy::GetEngineFeedback() const
{
    return GetWorld() && GetWorld()->GetTimeSeconds()<FeedbackUntil ? EngineFeedback : FString();
}

void AKemPlayerControlProxy::ServerUpdatePointer_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bEnabled)
{
    APlayerController* PC=Cast<APlayerController>(GetOwner());
    if (!PC || !PC->GetPawn() || FVector(Start).ContainsNaN() || FVector(End).ContainsNaN()) return;
    if (FVector::DistSquared(PC->GetPawn()->GetActorLocation(),Start)>FMath::Square(400.f) ||
        FVector::DistSquared(Start,End)>FMath::Square(5000.f)) return;
    const float Now=GetWorld()->GetTimeSeconds();
    if (Now-LastPointerUpdate<.035f) return;
    LastPointerUpdate=Now;
    if (!IsValid(PointerActor) && bEnabled)
    {
        FActorSpawnParameters Params; Params.Owner=this;
        PointerActor=GetWorld()->SpawnActor<AKemPointerActor>(AKemPointerActor::StaticClass(),Params);
    }
    if (IsValid(PointerActor)) PointerActor->UpdatePointer(Start,End,bEnabled);
}

void AKemPlayerControlProxy::ServerDrawBoard_Implementation(AKemDrawingBoardActor* Board, FVector2D UV, bool bStart, FLinearColor Color, float Size)
{
    auto* PC=Cast<APlayerController>(GetOwner());
    if(!PC || !PC->GetPawn() || !IsValid(Board) || Board->GetWorld()!=GetWorld() ||
        FVector::DistSquared(PC->GetPawn()->GetActorLocation(),Board->GetActorLocation())>FMath::Square(4500.f)) return;
    const float Now=GetWorld()->GetTimeSeconds();
    if(Now-LastDrawUpdate<.02f) return;
    LastDrawUpdate=Now;
    if(!Board->AcceptDraw(this,UV,bStart,Color,Size) && Board->GetSegmentCount()>=2048)
        ClientEngineFeedback(TEXT("Доска заполнена. Преподаватель может очистить её."));
}
