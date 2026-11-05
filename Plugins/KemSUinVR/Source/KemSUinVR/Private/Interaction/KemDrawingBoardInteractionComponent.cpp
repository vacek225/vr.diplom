#include "Interaction/KemDrawingBoardInteractionComponent.h"
#include "UI/KemEscapeMenuSubsystem.h"
#include "Interaction/KemVRWidgetInteractorComponent.h"
#include "Interaction/KemVRGrabInteractionComponent.h"

#include "Actors/KemDesktopCharacter.h"
#include "Actors/KemDrawingBoardActor.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "MotionControllerComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
const TCHAR* VRDrawPressedActionPath = TEXT("/Game/XRFramework/Input/Actions/IA_Menu_Interact_Right_Pressed.IA_Menu_Interact_Right_Pressed");
const TCHAR* VRMenuMappingContextPath = TEXT("/Game/XRFramework/Input/IMC_Menu.IMC_Menu");
const TCHAR* VRDefaultMappingContextPath = TEXT("/Game/XRFramework/Input/IMC_Default.IMC_Default");
const TCHAR* VRWeaponRightMappingContextPath = TEXT("/Game/XRFramework/Input/IMC_Weapon_Right.IMC_Weapon_Right");
const FString DesktopDrawerId(TEXT("Desktop"));
const FString VRDrawerId(TEXT("VRRight"));

FKey MakeInputKey(const TCHAR* KeyName)
{
    return FKey(FName(KeyName));
}

bool IsInputKeyActive(APlayerController* PlayerController, const FKey& Key)
{
    if (!PlayerController || !Key.IsValid())
    {
        return false;
    }

    return PlayerController->IsInputKeyDown(Key) || PlayerController->GetInputAnalogKeyState(Key) > 0.15f;
}

float GetInputKeyStrength(APlayerController* PlayerController, const FKey& Key)
{
    if (!PlayerController || !Key.IsValid())
    {
        return 0.0f;
    }

    return FMath::Max(
        PlayerController->IsInputKeyDown(Key) ? 1.0f : 0.0f,
        PlayerController->GetInputAnalogKeyState(Key));
}

float GetFallbackVRDrawInputStrength(APlayerController* PlayerController)
{
    static const FKey CandidateKeys[] = {
        MakeInputKey(TEXT("OculusTouch_Right_Trigger")),
        MakeInputKey(TEXT("OculusTouch_Right_Trigger_Axis")),
        MakeInputKey(TEXT("ValveIndex_Right_Trigger_Axis")),
        MakeInputKey(TEXT("Vive_Right_Trigger_Axis")),
        MakeInputKey(TEXT("MixedReality_Right_Trigger_Axis")),
        MakeInputKey(TEXT("Gamepad_RightTriggerAxis"))
    };

    float MaxStrength = 0.0f;
    for (const FKey& CandidateKey : CandidateKeys)
    {
        MaxStrength = FMath::Max(MaxStrength, GetInputKeyStrength(PlayerController, CandidateKey));
    }

    return MaxStrength;
}

float GetVRDrawActionStrength(APlayerController* PlayerController, const UInputAction* Action)
{
    if (!PlayerController || !Action)
    {
        return 0.0f;
    }

    const UEnhancedPlayerInput* EnhancedPlayerInput = Cast<UEnhancedPlayerInput>(PlayerController->PlayerInput);
    if (!EnhancedPlayerInput)
    {
        return 0.0f;
    }

    return EnhancedPlayerInput->GetActionValue(Action).GetMagnitude();
}
}

UKemDrawingBoardInteractionComponent::UKemDrawingBoardInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

    static ConstructorHelpers::FObjectFinder<UInputAction> DrawPressedActionAsset(VRDrawPressedActionPath);
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> MenuMappingContextAsset(VRMenuMappingContextPath);
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultMappingContextAsset(VRDefaultMappingContextPath);
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> WeaponRightMappingContextAsset(VRWeaponRightMappingContextPath);
    if (DrawPressedActionAsset.Succeeded())
    {
        VRDrawPressedAction = DrawPressedActionAsset.Object;
    }

    if (MenuMappingContextAsset.Succeeded())
    {
        VRMenuMappingContext = MenuMappingContextAsset.Object;
    }

    if (DefaultMappingContextAsset.Succeeded())
    {
        VRDefaultMappingContext = DefaultMappingContextAsset.Object;
    }

    if (WeaponRightMappingContextAsset.Succeeded())
    {
        VRWeaponRightMappingContext = WeaponRightMappingContextAsset.Object;
    }
}

void UKemDrawingBoardInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
    RefreshMotionControllers();
    ResolveVRDrawInputKey();
}

void UKemDrawingBoardInteractionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!IsLocalPawn())
    {
        SetActiveBoard(nullptr, DesktopDrawerId);
        SetActiveBoard(nullptr, VRDrawerId);
        return;
    }

    auto* Menu = UKemEscapeMenuSubsystem::Get(ResolvePlayerController());
    auto* UI = GetOwner()->FindComponentByClass<UKemVRWidgetInteractorComponent>();
    auto* Grab = GetOwner()->FindComponentByClass<UKemVRGrabInteractionComponent>();
    if (Menu && (!Menu->CanUseTool() || Menu->Tool == EKemTool::Pointer || (UI && UI->IsOverUI(true)) ||
        (Grab && Grab->IsHandOccupied(true))))
    {
        SetActiveBoard(nullptr, DesktopDrawerId); SetActiveBoard(nullptr, VRDrawerId);
        bDesktopDrawDownLastFrame = false; bVRDrawInputActive = false;
        return;
    }
    ResolveVRDrawInputKey();
    RefreshMotionControllers();
    DrawSendTime += DeltaTime;
    if (DrawSendTime >= 1.f/30.f)
    {
        DrawSendTime = 0;
        UpdateDesktopDrawing(); UpdateVRDrawing();
    }
}

void UKemDrawingBoardInteractionComponent::ResolveVRDrawInputKey()
{
    if (VRDrawInputKey.IsValid() || !VRDrawPressedAction)
    {
        return;
    }

    const TObjectPtr<UInputMappingContext> MappingContexts[] = {
        VRMenuMappingContext,
        VRDefaultMappingContext,
        VRWeaponRightMappingContext
    };

    for (const TObjectPtr<UInputMappingContext>& MappingContext : MappingContexts)
    {
        if (!MappingContext)
        {
            continue;
        }

        for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
        {
            if (Mapping.Action == VRDrawPressedAction && Mapping.Key.IsValid())
            {
                VRDrawInputKey = Mapping.Key;
                UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Drawing board VR input key resolved to %s."), *VRDrawInputKey.ToString());
                return;
            }
        }
    }
}

void UKemDrawingBoardInteractionComponent::RefreshMotionControllers()
{
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn)
    {
        RightGripMotionController = nullptr;
        RightAimMotionController = nullptr;
        return;
    }

    TArray<UMotionControllerComponent*> MotionControllers;
    OwnerPawn->GetComponents<UMotionControllerComponent>(MotionControllers);

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
        const bool bIsRightController = MotionSource.Contains(TEXT("Right")) || MotionSource.Contains(TEXT("right")) ||
            ComponentName.Contains(TEXT("Right")) || ComponentName.Contains(TEXT("right"));
        if (!bIsRightController)
        {
            continue;
        }

        const bool bIsAimController = MotionSource.Contains(TEXT("Aim")) || MotionSource.Contains(TEXT("aim")) ||
            ComponentName.Contains(TEXT("Aim")) || ComponentName.Contains(TEXT("aim"));
        if (bIsAimController)
        {
            RightAimMotionController = MotionController;
        }
        else if (!RightGripMotionController)
        {
            RightGripMotionController = MotionController;
        }
    }

    if (!RightAimMotionController)
    {
        RightAimMotionController = RightGripMotionController;
    }
}

void UKemDrawingBoardInteractionComponent::UpdateDesktopDrawing()
{
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn || !OwnerPawn->IsA<AKemDesktopCharacter>())
    {
        SetActiveBoard(nullptr, DesktopDrawerId);
        bDesktopDrawDownLastFrame = false;
        return;
    }

    APlayerController* PlayerController = ResolvePlayerController();
    if (!PlayerController)
    {
        SetActiveBoard(nullptr, DesktopDrawerId);
        bDesktopDrawDownLastFrame = false;
        return;
    }

    const bool bDrawPressedNow = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
    FHitResult Hit;
    FVector2D BoardUV = FVector2D::ZeroVector;
    AKemDrawingBoardActor* HitBoard = nullptr;
    const bool bHitBoard = TraceBoardFromCursorOrView(Hit) &&
        (HitBoard = Cast<AKemDrawingBoardActor>(Hit.GetActor())) &&
        HitBoard->ResolveHitToBoardUV(Hit, BoardUV);

    if (bDrawPressedNow && bHitBoard)
    {
        if (auto* Menu = UKemEscapeMenuSubsystem::Get(PlayerController))
        {
            HitBoard->SetBrushColor(Menu->Tool == EKemTool::Eraser ? HitBoard->GetClearColor() : Menu->BrushColor);
            HitBoard->SetBrushSize(Menu->BrushSize);
        }
        if (!bDesktopDrawDownLastFrame || ActiveDesktopBoard != HitBoard)
        {
            SetActiveBoard(HitBoard, DesktopDrawerId);
            HitBoard->RequestDraw(PlayerController, BoardUV, true);
        }
        else
        {
            HitBoard->RequestDraw(PlayerController, BoardUV, false);
        }
    }
    else if (!bDrawPressedNow && bDesktopDrawDownLastFrame)
    {
        SetActiveBoard(nullptr, DesktopDrawerId);
    }
    else if (bDrawPressedNow && !bHitBoard)
    {
        SetActiveBoard(nullptr, DesktopDrawerId);
    }

    bDesktopDrawDownLastFrame = bDrawPressedNow;
}

void UKemDrawingBoardInteractionComponent::UpdateVRDrawing()
{
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn || OwnerPawn->IsA<AKemDesktopCharacter>())
    {
        SetActiveBoard(nullptr, VRDrawerId);
        return;
    }

    APlayerController* PlayerController = ResolvePlayerController();
    const bool bDrawPressedNow = IsVRDrawInputActive(PlayerController);
    if (!bDrawPressedNow)
    {
        SetActiveBoard(nullptr, VRDrawerId);
        return;
    }

    const FVector TraceStart = GetVRTraceStart();
    const FVector TraceDirection = GetVRTraceDirection();

    FHitResult Hit;
    FVector2D BoardUV = FVector2D::ZeroVector;
    AKemDrawingBoardActor* HitBoard = nullptr;
    const bool bHitBoard = TraceBoardFromMotionController(Hit) &&
        (HitBoard = Cast<AKemDrawingBoardActor>(Hit.GetActor())) &&
        HitBoard->ResolveHitToBoardUV(Hit, BoardUV);


    if (HitBoard)
    {
        if (auto* Menu = UKemEscapeMenuSubsystem::Get(PlayerController))
        {
            HitBoard->SetBrushColor(Menu->Tool == EKemTool::Eraser ? HitBoard->GetClearColor() : Menu->BrushColor);
            HitBoard->SetBrushSize(Menu->BrushSize);
        }
        if (ActiveVRBoard != HitBoard)
        {
            SetActiveBoard(HitBoard, VRDrawerId);
            HitBoard->RequestDraw(PlayerController, BoardUV, true);
        }
        else
        {
            HitBoard->RequestDraw(PlayerController, BoardUV, false);
        }
    }
    else
    {
        // Do not stop the active draw session on a brief trace miss.
        // VR controller rays can flicker for a frame, and dropping the session
        // here turns a line into separate box stamps.
    }
}

bool UKemDrawingBoardInteractionComponent::IsVRDrawInputActive(APlayerController* PlayerController)
{
    if (!PlayerController)
    {
        bVRDrawInputActive = false;
        return false;
    }

    const float ActionInputStrength = GetVRDrawActionStrength(PlayerController, VRDrawPressedAction);
    const float ResolvedInputStrength = GetInputKeyStrength(PlayerController, VRDrawInputKey);
    const float FallbackInputStrength = FMath::Max(GetFallbackVRDrawInputStrength(PlayerController),
        UKemVRWidgetInteractorComponent::GetTriggerStrength(PlayerController, true));
    const float EffectiveInputStrength = FMath::Max3(ActionInputStrength, ResolvedInputStrength, FallbackInputStrength);
    const float Threshold = bVRDrawInputActive ? VRDrawReleaseThreshold : VRDrawPressThreshold;

    bVRDrawInputActive = EffectiveInputStrength >= Threshold;
    return bVRDrawInputActive;
}

bool UKemDrawingBoardInteractionComponent::TraceBoardFromCursorOrView(FHitResult& OutHit) const
{
    APlayerController* PlayerController = ResolvePlayerController();
    UWorld* World = GetWorld();
    if (!PlayerController || !World)
    {
        return false;
    }

    FVector TraceStart = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    PlayerController->GetPlayerViewPoint(TraceStart, ViewRotation);
    const FVector TraceDirection = ViewRotation.Vector();

    return TraceBoardRay(TraceStart,TraceDirection,DesktopTraceDistance,OutHit);
}

bool UKemDrawingBoardInteractionComponent::TraceBoardFromMotionController(FHitResult& OutHit) const
{
    UWorld* World = GetWorld();
    if (!World || (!RightAimMotionController && !RightGripMotionController))
    {
        return false;
    }

    const FVector TraceStart = GetVRTraceStart();
    const FVector TraceDirection = GetVRTraceDirection();
    return TraceBoardRay(TraceStart,TraceDirection,VRTraceDistance,OutHit);
}

bool UKemDrawingBoardInteractionComponent::TraceBoardRay(const FVector& Start, const FVector& Direction, float Range, FHitResult& OutHit) const
{
    auto* World=GetWorld();
    if (!World) return false;
    FHitResult Obstacle;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(KemDrawingSurface),true,GetOwner());
    const bool bBlocked=World->LineTraceSingleByChannel(Obstacle,Start,Start+Direction*Range,ECC_Visibility,Query);
    if (bBlocked && Cast<AKemDrawingBoardActor>(Obstacle.GetActor())) { OutHit=Obstacle; return true; }
    float Nearest=bBlocked ? Obstacle.Distance : Range;
    bool bFound=false;
    for (TActorIterator<AKemDrawingBoardActor> It(World); It; ++It)
    {
        FHitResult Surface;
        if (It->TraceDrawingSurface(Start,Direction,Nearest,Surface))
        { OutHit=Surface; Nearest=Surface.Distance; bFound=true; }
    }
    return bFound;
}

FVector UKemDrawingBoardInteractionComponent::GetVRTraceStart() const
{
    if (RightAimMotionController)
    {
        return RightAimMotionController->GetComponentLocation();
    }

    return RightGripMotionController ? RightGripMotionController->GetComponentLocation() : FVector::ZeroVector;
}

FVector UKemDrawingBoardInteractionComponent::GetVRTraceDirection() const
{
    if (RightAimMotionController)
    {
        return RightAimMotionController->GetForwardVector().GetSafeNormal();
    }

    return RightGripMotionController ? RightGripMotionController->GetForwardVector().GetSafeNormal() : FVector::ForwardVector;
}

void UKemDrawingBoardInteractionComponent::SetActiveBoard(AKemDrawingBoardActor* NewBoard, const FString& DrawerId)
{
    if (DrawerId == DesktopDrawerId)
    {
        AKemDrawingBoardActor* PreviousBoard = ActiveDesktopBoard;
        if (PreviousBoard && PreviousBoard != NewBoard)
        {
            PreviousBoard->StopDraw(DrawerId);
        }

        ActiveDesktopBoard = NewBoard;
        if (!NewBoard && PreviousBoard)
        {
            PreviousBoard->StopDraw(DrawerId);
        }
        return;
    }

    AKemDrawingBoardActor* PreviousBoard = ActiveVRBoard;
    if (PreviousBoard && PreviousBoard != NewBoard)
    {
        PreviousBoard->StopDraw(DrawerId);
    }

    ActiveVRBoard = NewBoard;
    if (!NewBoard && PreviousBoard)
    {
        PreviousBoard->StopDraw(DrawerId);
    }
}

bool UKemDrawingBoardInteractionComponent::IsLocalPawn() const
{
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    return OwnerPawn && OwnerPawn->IsLocallyControlled();
}

APlayerController* UKemDrawingBoardInteractionComponent::ResolvePlayerController() const
{
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    return OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
}
