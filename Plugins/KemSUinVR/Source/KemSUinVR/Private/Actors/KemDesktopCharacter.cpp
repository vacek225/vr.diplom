#include "Actors/KemDesktopCharacter.h"
#include "UI/KemEscapeMenuSubsystem.h"

#include "Actors/KemWorldActors.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Core/KemRoomSubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Actors/KemPlayerControlProxy.h"
#include "Engine/KemEnginePart.h"

AKemDesktopCharacter::AKemDesktopCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    bUseControllerRotationYaw = true;

    GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->MaxFlySpeed = DesktopMoveSpeed;
    GetCharacterMovement()->BrakingDecelerationFlying = 4096.0f;
    GetCharacterMovement()->AirControl = 1.0f;
    GetCharacterMovement()->GravityScale = 0.0f;

    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("DesktopCamera"));
    CameraComponent->SetupAttachment(GetCapsuleComponent());
    CameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
    CameraComponent->bUsePawnControlRotation = true;

    WidgetInteraction = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteraction"));
    WidgetInteraction->SetupAttachment(CameraComponent);
    WidgetInteraction->InteractionSource = EWidgetInteractionSource::CenterScreen;
    WidgetInteraction->InteractionDistance = 2500.0f;
    WidgetInteraction->TraceChannel = ECC_Visibility;
    WidgetInteraction->bShowDebug = true;
    WidgetInteraction->DebugColor = FLinearColor(0.08f, 0.78f, 1.0f, 1.0f);

    ActiveDragHandleType = EKemScreenHandleType::None;
}

void AKemDesktopCharacter::BeginPlay()
{
    Super::BeginPlay();

    GetCharacterMovement()->SetMovementMode(MOVE_Flying);

    if (IsLocallyControlled())
    {
        UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Desktop character BeginPlay at %s."), *GetActorLocation().ToCompactString());
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor(24, 205, 255), TEXT("KemSUinVR: Desktop character active"));
        }
        EnsureDesktopOverlayVisibility();
    }
}

void AKemDesktopCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!IsLocallyControlled())
    {
        return;
    }

    if (auto* Menu = UKemEscapeMenuSubsystem::Get(Cast<APlayerController>(GetController())))
        if (!Menu->CanUseTool()) return;
    EnsureDesktopOverlayVisibility();
    UpdateDesktopMovement(DeltaSeconds);
    UpdateEngineInteraction(DeltaSeconds);
    UpdateDesktopInteraction();
}

void AKemDesktopCharacter::SuspendForMenu()
{
    ReleaseEnginePart();
    ClearActiveScreenDrag();
    GetCharacterMovement()->StopMovementImmediately();
    if (WidgetInteraction)
    {
        WidgetInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
        WidgetInteraction->bShowDebug = false;
    }
    bWidgetPointerPressed = false;
    bWasPrimaryMouseButtonDown = false;
}

void AKemDesktopCharacter::UpdateDesktopMovement(float DeltaSeconds)
{
    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (!PlayerController)
    {
        return;
    }

    const float ForwardAxis = (PlayerController->IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f) - (PlayerController->IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f);
    const float RightAxis = (PlayerController->IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f) - (PlayerController->IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f);
    const float UpAxis = (PlayerController->IsInputKeyDown(EKeys::E) ? 1.0f : 0.0f) + (PlayerController->IsInputKeyDown(EKeys::SpaceBar) ? 1.0f : 0.0f)
        - (PlayerController->IsInputKeyDown(EKeys::Q) ? 1.0f : 0.0f) - (PlayerController->IsInputKeyDown(EKeys::LeftControl) ? 1.0f : 0.0f);

    if (!FMath::IsNearlyZero(ForwardAxis))
    {
        AddMovementInput(GetActorForwardVector(), ForwardAxis);
    }

    if (!FMath::IsNearlyZero(RightAxis))
    {
        AddMovementInput(GetActorRightVector(), RightAxis);
    }

    if (!FMath::IsNearlyZero(UpAxis))
    {
        AddMovementInput(FVector::UpVector, UpAxis);
    }

    float MouseDeltaX = 0.0f;
    float MouseDeltaY = 0.0f;
    PlayerController->GetInputMouseDelta(MouseDeltaX, MouseDeltaY);

    const bool bRotateEngine=HeldEnginePart.IsValid() && PlayerController->IsInputKeyDown(EKeys::R);
    if (!bRotateEngine && !FMath::IsNearlyZero(MouseDeltaX))
    {
        AddControllerYawInput(MouseDeltaX * DesktopLookSensitivity);
    }

    if (!bRotateEngine && !FMath::IsNearlyZero(MouseDeltaY))
    {
        AddControllerPitchInput(MouseDeltaY * -DesktopLookSensitivity);
    }
}

void AKemDesktopCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseEnginePart();
    if (FocusedEnginePart.IsValid()) FocusedEnginePart->SetLocallyFocused(false);
    Super::EndPlay(Reason);
}

void AKemDesktopCharacter::ReleaseEnginePart()
{
    if (AKemEnginePart* Part=HeldEnginePart.Get())
        if (AKemPlayerControlProxy* Proxy=AKemPlayerControlProxy::FindForController(Cast<APlayerController>(GetController())))
            Proxy->ServerReleaseEnginePart(Part,true,EngineHandOffset*FTransform(CameraComponent->GetComponentQuat(),
                CameraComponent->GetComponentLocation()+CameraComponent->GetForwardVector()*EngineGrabDistance));
    HeldEnginePart.Reset(); bEngineGrabConfirmed=false; EnginePendingTime=0;
}

void AKemDesktopCharacter::UpdateEngineInteraction(float DeltaSeconds)
{
    APlayerController* PC=Cast<APlayerController>(GetController());
    AKemPlayerControlProxy* Proxy=AKemPlayerControlProxy::FindForController(PC);
    if (!PC || !Proxy) return;
    if (FocusedEnginePart.IsValid()) FocusedEnginePart->SetLocallyFocused(false);
    FocusedEnginePart.Reset();
    FHitResult Hit;
    if (TraceDesktopInteraction(Hit))
        if (AKemEnginePart* Part=Cast<AKemEnginePart>(Hit.GetActor()))
        {
            FocusedEnginePart=Part; Part->SetLocallyFocused(true);
            if (!HeldEnginePart.IsValid() && PC->WasInputKeyJustPressed(EKeys::F))
            {
                EngineGrabDistance=FVector::Distance(CameraComponent->GetComponentLocation(),Hit.ImpactPoint);
                EngineHandOffset=Part->GetActorTransform().GetRelativeTransform(FTransform(CameraComponent->GetComponentQuat(),Hit.ImpactPoint));
                HeldEnginePart=Part;
                Proxy->ServerGrabEnginePart(Part,true,FTransform(CameraComponent->GetComponentQuat(),Hit.ImpactPoint));
            }
        }
    AKemEnginePart* Part=HeldEnginePart.Get();
    if (!Part) return;
    if (Part->IsHeldBy(this,true)) bEngineGrabConfirmed=true;
    EnginePendingTime+=DeltaSeconds;
    if (!PC->IsInputKeyDown(EKeys::F) || (bEngineGrabConfirmed && !Part->IsHeldBy(this,true)) ||
        (!bEngineGrabConfirmed && EnginePendingTime>0.8f))
    { ReleaseEnginePart(); return; }
    if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) EngineGrabDistance+=8;
    if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) EngineGrabDistance-=8;
    EngineGrabDistance=FMath::Clamp(EngineGrabDistance,35.0f,280.0f);
    if (PC->IsInputKeyDown(EKeys::R))
    {
        float X=0,Y=0; PC->GetInputMouseDelta(X,Y);
        EngineHandOffset.SetRotation((FRotator(-Y*0.5f,X*0.5f,0).Quaternion()*EngineHandOffset.GetRotation()).GetNormalized());
    }
    EnginePoseTimer+=DeltaSeconds;
    const FTransform Pose=EngineHandOffset*FTransform(CameraComponent->GetComponentQuat(),
        CameraComponent->GetComponentLocation()+CameraComponent->GetForwardVector()*EngineGrabDistance);
    if(bEngineGrabConfirmed && !HasAuthority()) Part->SetActorTransform(Pose);
    if(bEngineGrabConfirmed && HasAuthority()) { Part->MoveHeld(Proxy,true,Pose); return; }
    if (EnginePoseTimer>=0.04f)
    {
        EnginePoseTimer=0;
        Proxy->ServerMoveEnginePart(Part,true,Pose);
    }
}

void AKemDesktopCharacter::UpdateDesktopInteraction()
{
    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (!PlayerController || !WidgetInteraction)
    {
        return;
    }

    const bool bPrimaryMouseButtonDown = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
    const bool bJustPressed = bPrimaryMouseButtonDown && !bWasPrimaryMouseButtonDown;
    const bool bJustReleased = !bPrimaryMouseButtonDown && bWasPrimaryMouseButtonDown;

    FHitResult InteractionHit;
    const bool bHitSomething = TraceDesktopInteraction(InteractionHit);

    if (bJustPressed && bHitSomething)
    {
        if (AKemSharedLectureScreenActor* SharedScreen = Cast<AKemSharedLectureScreenActor>(InteractionHit.GetActor()))
        {
            if (SharedScreen->IsCopyHandle(InteractionHit.GetComponent()))
            {
                if (bWidgetPointerPressed)
                {
                    WidgetInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
                    bWidgetPointerPressed = false;
                }

                SharedScreen->SpawnLocalCopyForController(PlayerController);
                bWasPrimaryMouseButtonDown = bPrimaryMouseButtonDown;
                return;
            }
        }

        if (AKemLocalLectureScreenActor* LocalScreen = Cast<AKemLocalLectureScreenActor>(InteractionHit.GetActor()))
        {
            const EKemScreenHandleType HandleType = LocalScreen->ResolveHandleType(InteractionHit.GetComponent());
            if (HandleType == EKemScreenHandleType::Close)
            {
                if (bWidgetPointerPressed)
                {
                    WidgetInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
                    bWidgetPointerPressed = false;
                }

                LocalScreen->Destroy();
                bWasPrimaryMouseButtonDown = bPrimaryMouseButtonDown;
                return;
            }

            if (HandleType == EKemScreenHandleType::Move || HandleType == EKemScreenHandleType::Resize)
            {
                if (bWidgetPointerPressed)
                {
                    WidgetInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
                    bWidgetPointerPressed = false;
                }

                ActiveDraggedScreen = LocalScreen;
                ActiveDragHandleType = HandleType;
                ActiveDragDistance = (InteractionHit.ImpactPoint - CameraComponent->GetComponentLocation()).Length();
                bWasPrimaryMouseButtonDown = bPrimaryMouseButtonDown;
                return;
            }
        }
    }

    if (bPrimaryMouseButtonDown && ActiveDraggedScreen.IsValid())
    {
        AKemLocalLectureScreenActor* LocalScreen = ActiveDraggedScreen.Get();
        if (LocalScreen)
        {
            if (ActiveDragHandleType == EKemScreenHandleType::Move)
            {
                const FVector CameraLocation = CameraComponent->GetComponentLocation();
                const FVector CameraForward = CameraComponent->GetForwardVector();
                const float ClampedDistance = FMath::Clamp(ActiveDragDistance, 120.0f, 1200.0f);
                const FVector NewLocation = CameraLocation + CameraForward * ClampedDistance;
                const FRotator NewRotation(0.0f, GetControlRotation().Yaw, 0.0f);
                LocalScreen->SetActorLocationAndRotation(NewLocation, NewRotation);
            }
            else if (ActiveDragHandleType == EKemScreenHandleType::Resize)
            {
                float MouseDeltaX = 0.0f;
                float MouseDeltaY = 0.0f;
                PlayerController->GetInputMouseDelta(MouseDeltaX, MouseDeltaY);

                const float NewScale = FMath::Clamp(LocalScreen->GetActorScale3D().X + MouseDeltaY * -0.0018f, 0.25f, 1.8f);
                LocalScreen->SetActorScale3D(FVector(NewScale));
            }
        }

        bWasPrimaryMouseButtonDown = bPrimaryMouseButtonDown;
        return;
    }

    if (bJustReleased)
    {
        ClearActiveScreenDrag();
    }

    if (bPrimaryMouseButtonDown && !bWasPrimaryMouseButtonDown)
    {
        WidgetInteraction->PressPointerKey(EKeys::LeftMouseButton);
        bWidgetPointerPressed = true;
    }
    else if (!bPrimaryMouseButtonDown && bWasPrimaryMouseButtonDown)
    {
        if (bWidgetPointerPressed)
        {
            WidgetInteraction->ReleasePointerKey(EKeys::LeftMouseButton);
            bWidgetPointerPressed = false;
        }
    }

    bWasPrimaryMouseButtonDown = bPrimaryMouseButtonDown;
}

bool AKemDesktopCharacter::TraceDesktopInteraction(FHitResult& OutHit) const
{
    if (!GetWorld() || !CameraComponent)
    {
        return false;
    }

    const FVector Start = CameraComponent->GetComponentLocation();
    const FVector End = Start + CameraComponent->GetForwardVector() * WidgetInteraction->InteractionDistance;

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(KemDesktopInteractionTrace), true, this);
    return GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);
}

void AKemDesktopCharacter::ClearActiveScreenDrag()
{
    ActiveDraggedScreen = nullptr;
    ActiveDragHandleType = EKemScreenHandleType::None;
    ActiveDragDistance = 0.0f;
}

void AKemDesktopCharacter::EnsureDesktopOverlayVisibility()
{
    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    UKemRoomSubsystem* RoomSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKemRoomSubsystem>() : nullptr;
    if (!PlayerController || !RoomSubsystem)
    {
        return;
    }

    const bool bShouldShowWorldHub = !RoomSubsystem->GetCurrentRoomInfo().bIsInRoom;
    if (bShouldShowWorldHub != bDesktopMenuVisible)
    {
        bWasPrimaryMouseButtonDown = false;
        bDesktopMenuVisible = bShouldShowWorldHub;
        UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Desktop hub world panel visibility changed. Visible = %s."),
            bDesktopMenuVisible ? TEXT("true") : TEXT("false"));
        ApplyDesktopGameplayInputMode();
    }
}

void AKemDesktopCharacter::ApplyDesktopGameplayInputMode() const
{
    if (auto* Menu = UKemEscapeMenuSubsystem::Get(Cast<APlayerController>(GetController())))
        if (Menu->IsMenuOpen()) return;
    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        if (WidgetInteraction)
        {
            WidgetInteraction->SetComponentTickEnabled(true);
            WidgetInteraction->bShowDebug = true;
        }

        PlayerController->SetShowMouseCursor(false);
        PlayerController->bEnableClickEvents = false;
        PlayerController->bEnableMouseOverEvents = false;
        PlayerController->SetIgnoreLookInput(false);
        PlayerController->SetIgnoreMoveInput(false);
        FInputModeGameOnly InputMode;
        PlayerController->SetInputMode(InputMode);
    }
}
