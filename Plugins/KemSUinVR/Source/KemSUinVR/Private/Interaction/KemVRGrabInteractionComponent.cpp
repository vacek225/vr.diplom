#include "Interaction/KemVRGrabInteractionComponent.h"
#include "Actors/KemPlayerControlProxy.h"
#include "Engine/KemEnginePart.h"
#include "UI/KemEscapeMenuSubsystem.h"
#include "Interaction/KemVRWidgetInteractorComponent.h"
#include "EnhancedPlayerInput.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "MotionControllerComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"


UKemVRGrabInteractionComponent::UKemVRGrabInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UKemVRGrabInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
    RefreshMotionControllers();
    // Imported lobby props can have no query collision; enable only the known
    // small props, never the lobby walls or other static scene geometry.
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
        if (auto* Mesh=It->GetStaticMeshComponent(); IsLobbyProp(Mesh))
        {
            Mesh->SetMobility(EComponentMobility::Movable);
            Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        }
}

void UKemVRGrabInteractionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseHand(EKemGrabHand::Left);
    ReleaseHand(EKemGrabHand::Right);
    if (LeftFocus.IsValid()) LeftFocus->SetLocallyFocused(false);
    if (RightFocus.IsValid()) RightFocus->SetLocallyFocused(false);
    Super::EndPlay(Reason);
}

void UKemVRGrabInteractionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!IsLocalVRPawn())
    {
        ReleaseHand(EKemGrabHand::Left);
        ReleaseHand(EKemGrabHand::Right);
        return;
    }

    RefreshMotionControllers();
    if (LeftFocus.IsValid()) LeftFocus->SetLocallyFocused(false);
    if (RightFocus.IsValid()) RightFocus->SetLocallyFocused(false);
    LeftFocus.Reset(); RightFocus.Reset();
    for (EKemGrabHand Hand : {EKemGrabHand::Left,EKemGrabHand::Right})
    {
        PollRayGrab(Hand,DeltaTime);
        UpdateEngineHand(Hand,DeltaTime);
        UpdatePulledObject(Hand,DeltaTime);
        if (USceneComponent* Controller=GetHandComponent(Hand))
            if (UPrimitiveComponent* Candidate=ResolveBestCandidateNearHand(Controller->GetComponentLocation()))
                if (AKemEnginePart* Part=Cast<AKemEnginePart>(Candidate->GetOwner()))
                {
                    Part->SetLocallyFocused(true);
                    (Hand==EKemGrabHand::Left ? LeftFocus : RightFocus)=Part;
                }
    }
}

void UKemVRGrabInteractionComponent::UpdateEngineHand(EKemGrabHand Hand, float DeltaSeconds)
{
    FGrabState& Grab=GetGrabState(Hand);
    AKemEnginePart* Part=Grab.EnginePart.Get();
    if (!Part) return;
    APawn* Pawn=Cast<APawn>(GetOwner());
    const bool bRight=Hand==EKemGrabHand::Right;
    if (Part->IsHeldBy(Pawn,bRight)) Grab.bConfirmed=true;
    Grab.PendingTime+=DeltaSeconds;
    if ((Grab.bConfirmed && !Part->IsHeldBy(Pawn,bRight)) || (!Grab.bConfirmed && Grab.PendingTime>0.8f))
    {
        ReleaseHand(Hand);
        return;
    }
    USceneComponent* HandComponent=GetHandComponent(Hand);
    if (!HandComponent) return;
    FTransform Pose=GetHeldPose(Grab,HandComponent->GetComponentTransform(),DeltaSeconds);
    Pose.SetScale3D(Part->InstalledTransform().GetScale3D());
    // Predict only the granted local grab; server snapshots still own the state.
    if (Grab.bConfirmed && !Part->HasAuthority()) Part->SetActorTransform(Pose);
    if (Grab.bConfirmed && Part->HasAuthority())
    {
        if (AKemPlayerControlProxy* Proxy=AKemPlayerControlProxy::FindForController(Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr))
            Part->MoveHeld(Proxy,bRight,Pose);
        return;
    }
    Grab.PoseTimer+=DeltaSeconds;
    if (Grab.PoseTimer<0.04f) return;
    Grab.PoseTimer=0;
    if (AKemPlayerControlProxy* Proxy=AKemPlayerControlProxy::FindForController(Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr))
        Proxy->ServerMoveEnginePart(Part,bRight,Pose);
}

void UKemVRGrabInteractionComponent::RefreshMotionControllers()
{
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn)
    {
        LeftGripMotionController = nullptr;
        LeftAimMotionController = nullptr;
        RightGripMotionController = nullptr;
        RightAimMotionController = nullptr;
        return;
    }

    TArray<UMotionControllerComponent*> MotionControllers;
    OwnerPawn->GetComponents<UMotionControllerComponent>(MotionControllers);

    LeftGripMotionController = nullptr;
    LeftAimMotionController = nullptr;
    RightGripMotionController = nullptr;
    RightAimMotionController = nullptr;

    for (UMotionControllerComponent* MotionController : MotionControllers)
    {
        if (!MotionController)
        {
            continue;
        }

        const FString MotionSource = MotionController->MotionSource.ToString();
        const FString ComponentName = MotionController->GetName();
        const bool bIsLeftController = MotionSource.Contains(TEXT("Left")) || MotionSource.Contains(TEXT("left")) ||
            ComponentName.Contains(TEXT("Left")) || ComponentName.Contains(TEXT("left"));
        const bool bIsRightController = MotionSource.Contains(TEXT("Right")) || MotionSource.Contains(TEXT("right")) ||
            ComponentName.Contains(TEXT("Right")) || ComponentName.Contains(TEXT("right"));
        const bool bIsAimController = MotionSource.Contains(TEXT("Aim")) || MotionSource.Contains(TEXT("aim")) ||
            ComponentName.Contains(TEXT("Aim")) || ComponentName.Contains(TEXT("aim"));

        if (bIsLeftController)
        {
            if (bIsAimController)
            {
                LeftAimMotionController = MotionController;
            }
            else if (!LeftGripMotionController)
            {
                LeftGripMotionController = MotionController;
            }
        }
        else if (bIsRightController)
        {
            if (bIsAimController)
            {
                RightAimMotionController = MotionController;
            }
            else if (!RightGripMotionController)
            {
                RightGripMotionController = MotionController;
            }
        }
    }

    if (!LeftAimMotionController)
    {
        LeftAimMotionController = LeftGripMotionController;
    }

    if (!RightAimMotionController)
    {
        RightAimMotionController = RightGripMotionController;
    }
}

bool UKemVRGrabInteractionComponent::IsLocalVRPawn() const
{
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    return OwnerPawn && OwnerPawn->IsLocallyControlled();
}

USceneComponent* UKemVRGrabInteractionComponent::GetHandComponent(EKemGrabHand Hand) const
{
    if (Hand == EKemGrabHand::Left)
    {
        return LeftGripMotionController ? Cast<USceneComponent>(LeftGripMotionController.Get()) : Cast<USceneComponent>(LeftAimMotionController.Get());
    }

    return RightGripMotionController ? Cast<USceneComponent>(RightGripMotionController.Get()) : Cast<USceneComponent>(RightAimMotionController.Get());
}

UPrimitiveComponent* UKemVRGrabInteractionComponent::ResolveBestCandidateNearHand(const FVector& HandLocation) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    TArray<FOverlapResult> OverlapResults;
    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(KemVRGrabOverlap), false, GetOwner());
    const bool bHasOverlap = World->OverlapMultiByObjectType(
        OverlapResults,
        HandLocation,
        FQuat::Identity,
        ObjectQueryParams,
        FCollisionShape::MakeSphere(GrabRadius),
        QueryParams);


    UPrimitiveComponent* BestCandidate = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();

    for (const FOverlapResult& OverlapResult : OverlapResults)
    {
        UPrimitiveComponent* PrimitiveComponent = OverlapResult.GetComponent();
        if (!PrimitiveComponent || !PrimitiveComponent->IsRegistered())
        {
            continue;
        }

        if (!IsComponentGrabbable(PrimitiveComponent))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(PrimitiveComponent->GetComponentLocation(), HandLocation);
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestCandidate = PrimitiveComponent;
        }
    }

    if (BestCandidate)
    {
        return BestCandidate;
    }

    // Fallback: some meshes in the level can have disabled collision after import.
    // For the target Apple mesh, allow distance-based pickup even without overlap.
    const float MaxDistanceSquared = FMath::Square(GrabRadius);
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        AStaticMeshActor* StaticMeshActor = *It;
        UStaticMeshComponent* StaticMeshComponent = StaticMeshActor ? StaticMeshActor->GetStaticMeshComponent() : nullptr;
        if (!StaticMeshComponent || !IsComponentGrabbable(StaticMeshComponent))
        {
            continue;
        }

        const float DistanceSquared = StaticMeshComponent->Bounds.GetBox().ComputeSquaredDistanceToPoint(HandLocation);
        if (DistanceSquared < BestDistanceSquared && DistanceSquared <= MaxDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestCandidate = StaticMeshComponent;
        }
    }

    return BestCandidate;
}

bool UKemVRGrabInteractionComponent::IsComponentGrabbable(const UPrimitiveComponent* PrimitiveComponent) const
{
    if (!PrimitiveComponent)
    {
        return false;
    }

    const AActor* OwnerActor = PrimitiveComponent->GetOwner();
    if (Cast<AKemEnginePart>(OwnerActor)) return true;
    if (OwnerActor && OwnerActor->ActorHasTag(GrabbableTag))
    {
        return true;
    }

    if (PrimitiveComponent->ComponentHasTag(GrabbableTag))
    {
        return true;
    }

    const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PrimitiveComponent);
    const UStaticMesh* StaticMesh = StaticMeshComponent ? StaticMeshComponent->GetStaticMesh() : nullptr;
    if (!StaticMesh)
    {
        return false;
    }

    const FString MeshPath = StaticMesh->GetPathName();
    return IsLobbyProp(PrimitiveComponent) || MeshPath.Equals(TargetStaticMeshPath, ESearchCase::IgnoreCase) ||
        MeshPath.Contains(TEXT("/Game/KUB/lobby/AppleLowPoly"), ESearchCase::IgnoreCase);
}

bool UKemVRGrabInteractionComponent::TryGrab(EKemGrabHand Hand)
{
    FGrabState& GrabState = GetGrabState(Hand);
    if (GrabState.HeldComponent.IsValid() || GrabState.EnginePart.IsValid())
    {
        return true;
    }

    USceneComponent* HandComponent = GetHandComponent(Hand);
    if (!HandComponent)
    {
        return false;
    }

    UPrimitiveComponent* CandidateComponent = ResolveBestCandidateNearHand(HandComponent->GetComponentLocation());
    if (!CandidateComponent)
    {
        return false;
    }

    if (AKemEnginePart* Part=Cast<AKemEnginePart>(CandidateComponent->GetOwner()))
    {
        APawn* Pawn=Cast<APawn>(GetOwner());
        AKemPlayerControlProxy* Proxy=AKemPlayerControlProxy::FindForController(Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr);
        if (!Proxy) return false;
        GrabState.EnginePart=Part;
        GrabState.PartRelativeToHand=Part->GetActorTransform().GetRelativeTransform(HandComponent->GetComponentTransform());
        Proxy->ServerGrabEnginePart(Part,Hand==EKemGrabHand::Right,HandComponent->GetComponentTransform());
        return true;
    }

    if (CandidateComponent == GetGrabState(Hand == EKemGrabHand::Left ? EKemGrabHand::Right : EKemGrabHand::Left).HeldComponent.Get())
    {
        ReleaseHand(Hand == EKemGrabHand::Left ? EKemGrabHand::Right : EKemGrabHand::Left);
    }

    GrabState.HeldComponent = CandidateComponent;
    GrabState.bWasSimulatingPhysics = CandidateComponent->IsSimulatingPhysics();
    GrabState.CollisionBeforeGrab = CandidateComponent->GetCollisionEnabled();

    CandidateComponent->SetMobility(EComponentMobility::Movable);
    if (GrabState.bWasSimulatingPhysics)
    {
        CandidateComponent->SetSimulatePhysics(false);
    }

    CandidateComponent->AttachToComponent(HandComponent, FAttachmentTransformRules::KeepWorldTransform);
    return true;
}

void UKemVRGrabInteractionComponent::ReleaseHand(EKemGrabHand Hand)
{
    FGrabState& GrabState = GetGrabState(Hand);
    if (AKemEnginePart* Part=GrabState.EnginePart.Get())
    {
        APawn* Pawn=Cast<APawn>(GetOwner());
        if (AKemPlayerControlProxy* Proxy=AKemPlayerControlProxy::FindForController(Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr))
        {
            USceneComponent* Controller=GetHandComponent(Hand);
            Proxy->ServerReleaseEnginePart(Part,Hand==EKemGrabHand::Right,
                GrabState.bPulling ? GrabState.PullPose : Controller ? GrabState.PartRelativeToHand*Controller->GetComponentTransform() : Part->GetActorTransform());
        }
        GrabState=FGrabState();
        return;
    }
    UPrimitiveComponent* HeldComponent = GrabState.HeldComponent.Get();
    if (!HeldComponent)
    {
        GrabState = FGrabState();
        return;
    }

    HeldComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    HeldComponent->SetCollisionEnabled(GrabState.CollisionBeforeGrab);

    if (GrabState.bWasSimulatingPhysics)
    {
        HeldComponent->SetSimulatePhysics(true);
    }

    GrabState = FGrabState();
}

UKemVRGrabInteractionComponent::FGrabState& UKemVRGrabInteractionComponent::GetGrabState(EKemGrabHand Hand)
{
    return Hand == EKemGrabHand::Left ? LeftGrabState : RightGrabState;
}

const UKemVRGrabInteractionComponent::FGrabState& UKemVRGrabInteractionComponent::GetGrabState(EKemGrabHand Hand) const
{
    return Hand == EKemGrabHand::Left ? LeftGrabState : RightGrabState;
}

void UKemVRGrabInteractionComponent::HandleLeftGrabPressed()
{
    if (LeftGrabState.bRayLatched) { LeftGrabState.bRayLatched=false; return; }
    TryGrab(EKemGrabHand::Left);
}

void UKemVRGrabInteractionComponent::HandleLeftGrabReleased()
{
    if (!LeftGrabState.bRayLatched) ReleaseHand(EKemGrabHand::Left);
}

void UKemVRGrabInteractionComponent::HandleRightGrabPressed()
{
    if (RightGrabState.bRayLatched) { RightGrabState.bRayLatched=false; return; }
    TryGrab(EKemGrabHand::Right);
}

void UKemVRGrabInteractionComponent::HandleRightGrabReleased()
{
    if (!RightGrabState.bRayLatched) ReleaseHand(EKemGrabHand::Right);
}

bool UKemVRGrabInteractionComponent::IsLobbyProp(const UPrimitiveComponent* Component)
{
    const auto* Mesh=Cast<UStaticMeshComponent>(Component);
    if (!Mesh || !Mesh->GetStaticMesh()) return false;
    const FString Path=Mesh->GetStaticMesh()->GetPathName();
    return Path==TEXT("/Game/KUB/lobby/AppleLowPoly.AppleLowPoly") ||
        Path==TEXT("/Game/KUB/lobby/objects/Green_Apple_FBX.Green_Apple_FBX") ||
        Path==TEXT("/Game/Weapons/Pistol/Meshes/SM_Pistol.SM_Pistol") ||
        Path==TEXT("/Game/KUB/lobby/objects/skull.skull");
}

bool UKemVRGrabInteractionComponent::IsHandOccupied(bool bRightHand) const
{
    const auto& Grab=GetGrabState(bRightHand ? EKemGrabHand::Right : EKemGrabHand::Left);
    return Grab.EnginePart.IsValid() || Grab.HeldComponent.IsValid();
}

FTransform UKemVRGrabInteractionComponent::GetHeldPose(FGrabState& Grab, const FTransform& Hand, float DeltaSeconds) const
{
    const FTransform Target=Grab.PartRelativeToHand*Hand;
    if (!Grab.bPulling) return Target;
    // Constant speed keeps the pull smooth and independent of frame rate.
    Grab.PullPose.SetLocation(FMath::VInterpConstantTo(Grab.PullPose.GetLocation(),Target.GetLocation(),DeltaSeconds,500.f));
    Grab.PullPose.SetRotation(FQuat::Slerp(Grab.PullPose.GetRotation(),Target.GetRotation(),FMath::Clamp(DeltaSeconds*10.f,0.f,1.f)).GetNormalized());
    Grab.PullPose.SetScale3D(Target.GetScale3D());
    if (Grab.PullPose.GetLocation().Equals(Target.GetLocation(),1.f))
    { Grab.bPulling=false; return Target; }
    return Grab.PullPose;
}

void UKemVRGrabInteractionComponent::UpdatePulledObject(EKemGrabHand Hand, float DeltaSeconds)
{
    auto& Grab=GetGrabState(Hand);
    auto* Controller=GetHandComponent(Hand);
    if (Grab.bManualHold && Grab.HeldComponent.IsValid() && Controller)
        Grab.HeldComponent->SetWorldTransform(GetHeldPose(Grab,Controller->GetComponentTransform(),DeltaSeconds));
}

void UKemVRGrabInteractionComponent::PollGrip(EKemGrabHand Hand)
{
    auto* Pawn=Cast<APawn>(GetOwner());
    auto* PC=Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC) return;
    const bool bRight=Hand==EKemGrabHand::Right;
    bool& bGripWasDown=bRight ? bRightGripDown : bLeftGripDown;
    const auto* Input=Cast<UEnhancedPlayerInput>(PC->PlayerInput);
    const auto* GripAction=FindObject<UInputAction>(nullptr,bRight ? TEXT("/Game/KUB/Input/IA_KemGripRight.IA_KemGripRight") : TEXT("/Game/KUB/Input/IA_KemGripLeft.IA_KemGripLeft"));
    const FKey GripKey=bRight ? EKeys::Vive_Right_Grip_Click : EKeys::Vive_Left_Grip_Click;
    const float GripStrength=FMath::Max(PC->IsInputKeyDown(GripKey) ? 1.f : 0.f,
        Input && GripAction ? Input->GetActionValue(GripAction).GetMagnitude() : 0.f);
    const bool bGripDown=GripStrength>=(bGripWasDown ? .15f : .55f);
    if (bGripDown && !bGripWasDown)
    {
        if (bRight) HandleRightGrabPressed(); else HandleLeftGrabPressed();
    }
    else if (!bGripDown && bGripWasDown)
    {
        if (bRight) HandleRightGrabReleased(); else HandleLeftGrabReleased();
    }
    bGripWasDown=bGripDown;
}

void UKemVRGrabInteractionComponent::PollRayGrab(EKemGrabHand Hand, float DeltaSeconds)
{
    auto* Pawn=Cast<APawn>(GetOwner());
    auto* PC=Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC) return;
    const bool bRight=Hand==EKemGrabHand::Right;
    PollGrip(Hand);
    const bool bGripDown=bRight ? bRightGripDown : bLeftGripDown;
    bool& bTriggerWasDown=bRight ? bRightRayTriggerDown : bLeftRayTriggerDown;
    auto& Grab=GetGrabState(Hand);
    const float Trigger=UKemVRWidgetInteractorComponent::GetTriggerStrength(PC,bRight);
    const bool bTriggerDown=Trigger>=(bTriggerWasDown ? .15f : .55f);
    auto* Menu=UKemEscapeMenuSubsystem::Get(PC);
    auto* UI=Pawn->FindComponentByClass<UKemVRWidgetInteractorComponent>();
    if (bTriggerDown && !bTriggerWasDown && !bGripDown && Menu && Menu->CanUseTool() &&
        Menu->IsPointerEnabled() && (!UI || !UI->IsOverUI(bRight)) && !IsHandOccupied(bRight))
        TryRayGrab(Hand);
    bTriggerWasDown=bTriggerDown;
    auto* Controller=Cast<UMotionControllerComponent>(GetHandComponent(Hand));
    if ((Grab.bRayLatched || Grab.bManualHold) && Controller && !Controller->IsTracked()) ReleaseHand(Hand);
}

bool UKemVRGrabInteractionComponent::TryRayGrab(EKemGrabHand Hand)
{
    const bool bRight=Hand==EKemGrabHand::Right;
    if (IsHandOccupied(bRight)) return false;
    auto* Aim=bRight ? RightAimMotionController.Get() : LeftAimMotionController.Get();
    auto* Grip=GetHandComponent(Hand);
    auto* Pawn=Cast<APawn>(GetOwner());
    auto* PC=Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!Aim || !Grip || !PC) return false;
    const FTransform Ray(Aim->GetComponentQuat(),Aim->GetComponentLocation());
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(KemRayGrab),true,Pawn);
    if (!GetWorld()->LineTraceSingleByChannel(Hit,Ray.GetLocation(),Ray.GetLocation()+Aim->GetForwardVector()*400.f,ECC_Visibility,Query)) return false;
    auto* Part=Cast<AKemEnginePart>(Hit.GetActor());
    auto* Component=Hit.GetComponent();
    if (!Part && (!IsLobbyProp(Component) || GetWorld()->GetNetMode()==NM_Client)) return false;
    auto& Grab=GetGrabState(Hand);
    FTransform Initial=Part ? Part->GetActorTransform() : Component->GetComponentTransform();
    if (Part)
    {
        auto* Proxy=AKemPlayerControlProxy::FindForController(PC);
        if (!Proxy) return false;
        Grab.EnginePart=Part;
        Proxy->ServerRayGrabEnginePart(Part,bRight,Ray);
        if (Part->HasAuthority() && !Part->IsHeldBy(Pawn,bRight)) { Grab=FGrabState(); return false; }
    }
    else
    {
        const auto Other=bRight ? EKemGrabHand::Left : EKemGrabHand::Right;
        if (GetGrabState(Other).HeldComponent.Get()==Component) return false;
        Grab.HeldComponent=Component;
        Grab.bWasSimulatingPhysics=Component->IsSimulatingPhysics();
        Grab.CollisionBeforeGrab=Component->GetCollisionEnabled();
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetSimulatePhysics(false);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    }
    Grab.bRayLatched=true;
    Grab.bManualHold=true;
    Grab.bPulling=true;
    Grab.PullPose=Initial;
    Grab.PartRelativeToHand=Initial.GetRelativeTransform(Grip->GetComponentTransform());
    FVector CenterOffset=FVector::ZeroVector;
    if (!Part) CenterOffset=Grip->GetComponentQuat().UnrotateVector(Component->Bounds.Origin-Component->GetComponentLocation());
    Grab.PartRelativeToHand.SetTranslation(FVector(15,0,0)-CenterOffset);
    return true;
}
