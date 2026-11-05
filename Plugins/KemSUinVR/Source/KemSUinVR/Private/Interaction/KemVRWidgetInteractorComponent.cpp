#include "Interaction/KemVRWidgetInteractorComponent.h"
#include "UI/KemEscapeMenuSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "MotionControllerComponent.h"

UKemVRWidgetInteractorComponent::UKemVRWidgetInteractorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

float UKemVRWidgetInteractorComponent::GetTriggerStrength(const APlayerController* PC, bool bRightHand)
{
    if (!PC) return 0;
    const TCHAR* Side = bRightHand ? TEXT("Right") : TEXT("Left");
    float Strength = 0;
    for (const TCHAR* Device : {TEXT("Vive"), TEXT("OculusTouch"), TEXT("ValveIndex"), TEXT("MixedReality")})
        for (const TCHAR* Suffix : {TEXT("Trigger_Axis"), TEXT("Trigger_Click"), TEXT("Trigger")})
        {
            const FKey Key(*FString::Printf(TEXT("%s_%s_%s"), Device, Side, Suffix));
            if (Key.IsValid()) Strength = FMath::Max(Strength, FMath::Max(PC->GetInputAnalogKeyState(Key), PC->IsInputKeyDown(Key) ? 1.f : 0.f));
        }
    const auto* Input = Cast<UEnhancedPlayerInput>(PC->PlayerInput);
    const auto* Action = FindObject<UInputAction>(nullptr, bRightHand ? TEXT("/Game/KUB/Input/IA_KemUIRight.IA_KemUIRight") : TEXT("/Game/KUB/Input/IA_KemUILeft.IA_KemUILeft"));
    if (Input && Action) Strength = FMath::Max(Strength, Input->GetActionValue(Action).GetMagnitude());
    return Strength;
}

bool UKemVRWidgetInteractorComponent::AreTriggersHeld(const APlayerController* PC)
{
    return GetTriggerStrength(PC, false) >= .15f || GetTriggerStrength(PC, true) >= .15f;
}

void UKemVRWidgetInteractorComponent::BeginPlay()
{
    Super::BeginPlay();
    EnsureHand(Left, false);
    EnsureHand(Right, true);
}

void UKemVRWidgetInteractorComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelInteractions();
    for (auto* Hand : {&Left, &Right})
    {
        if (IsValid(Hand->Interaction)) Hand->Interaction->DestroyComponent();
        if (IsValid(Hand->Beam)) Hand->Beam->DestroyComponent();
        if (IsValid(Hand->Cursor)) Hand->Cursor->DestroyComponent();
    }
    Super::EndPlay(Reason);
}

void UKemVRWidgetInteractorComponent::EnsureHand(FKemUIHand& Hand, bool bRightHand)
{
    auto* Pawn = Cast<APawn>(GetOwner());
    if (!Pawn || !Pawn->IsLocallyControlled()) return;
    if (!Hand.Aim || !Hand.Aim->IsRegistered())
    {
        Hand.Aim = nullptr;
        TInlineComponentArray<UMotionControllerComponent*> Controllers(Pawn);
        for (auto* Controller : Controllers)
        {
            const FString Source = Controller->MotionSource.ToString();
            if (!Source.Contains(bRightHand ? TEXT("Right") : TEXT("Left"), ESearchCase::IgnoreCase)) continue;
            Hand.Aim = Controller;
            if (Source.Contains(TEXT("Aim"), ESearchCase::IgnoreCase)) break;
        }
        if (Hand.Aim) AddTickPrerequisiteComponent(Hand.Aim);
    }
    if (!Hand.Aim) return;
    if (!Hand.Interaction)
    {
        Hand.Interaction = NewObject<UWidgetInteractionComponent>(Pawn);
        Hand.Interaction->InteractionSource = EWidgetInteractionSource::Custom;
        Hand.Interaction->InteractionDistance = LaserDistance;
        Hand.Interaction->VirtualUserIndex = 1; // Separate from XRFramework's legacy pointer.
        Hand.Interaction->PointerIndex = bRightHand ? 1 : 2;
        Hand.Interaction->RegisterComponent();
        Pawn->AddInstanceComponent(Hand.Interaction);
        Hand.Interaction->SetComponentTickEnabled(false); // Explicitly updated after our custom trace.
        Hand.Interaction->AttachToComponent(Hand.Aim, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    }
    auto MakeVisual = [Pawn](const TCHAR* Asset)
    {
        auto* Visual = NewObject<UStaticMeshComponent>(Pawn);
        Visual->SetMobility(EComponentMobility::Movable);
        Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Visual->SetGenerateOverlapEvents(false);
        Visual->SetCastShadow(false);
        Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Asset));
        Visual->RegisterComponent();
        Pawn->AddInstanceComponent(Visual);
        Visual->SetHiddenInGame(true);
        return Visual;
    };
    if (!Hand.Beam) Hand.Beam = MakeVisual(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Hand.Cursor) Hand.Cursor = MakeVisual(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
}

void UKemVRWidgetInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    EnsureHand(Left, false);
    EnsureHand(Right, true);
    UpdateHand(Left, false, DeltaTime);
    UpdateHand(Right, true, DeltaTime);
}

void UKemVRWidgetInteractorComponent::UpdateHand(FKemUIHand& Hand, bool bRightHand, float DeltaTime)
{
    auto* Pawn = Cast<APawn>(GetOwner());
    auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    auto* Menu = UKemEscapeMenuSubsystem::Get(PC);
    const bool bTracked = Hand.Aim && Hand.Aim->IsRegistered() && Hand.Aim->IsTracked();
    if (!Pawn || !Pawn->IsLocallyControlled() || !Hand.Interaction || !bTracked)
    {
        ReleaseHand(Hand, bRightHand, true);
        if (Hand.Beam) Hand.Beam->SetHiddenInGame(true);
        if (Hand.Cursor) Hand.Cursor->SetHiddenInGame(true);
        Hand.bTriggerDown = GetTriggerStrength(PC, bRightHand) >= .15f;
        return;
    }
    const FTransform Pose(Hand.Aim->GetComponentQuat(), Hand.Aim->GetComponentLocation());
    FHitResult Hit;
    const FVector Start = Pose.GetLocation();
    const FVector End = Start + Pose.GetRotation().GetForwardVector() * LaserDistance;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(KemVRWidgetTrace), true, Pawn);
    GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);
    Hand.Interaction->SetCustomHitResult(Hit);
    Hand.Interaction->TickComponent(DeltaTime, LEVELTICK_All, nullptr);
    const bool bOverUI = Hand.Interaction->IsOverHitTestVisibleWidget();
    const float Strength = GetTriggerStrength(PC, bRightHand);
    if (!Hand.bTriggerDown && Strength >= .55f)
    {
        Hand.bTriggerDown = true;
        Hand.bDragging = Menu && Menu->BeginPanelDrag(bRightHand, Pose, Hit);
        if (!Hand.bDragging && bOverUI && (!Menu || Menu->IsMenuOpen() || Menu->CanUseTool()))
        {
            Hand.bPointerPressed = true;
            Hand.Interaction->PressPointerKey(EKeys::LeftMouseButton);
        }
    }
    else if (Hand.bTriggerDown && Strength < .15f)
    {
        Hand.bTriggerDown = false;
        ReleaseHand(Hand, bRightHand);
    }
    if (Hand.bDragging && Menu) Menu->UpdatePanelDrag(bRightHand, Pose);
    const bool bLeftPointer = !bRightHand && Menu && Menu->CanUseTool() && Menu->IsPointerEnabled();
    const bool bShow = bOverUI || (Menu && Menu->IsMenuOpen()) || bLeftPointer;
    const FVector BeamEnd = Hit.bBlockingHit ? Hit.ImpactPoint : End;
    Hand.Beam->SetHiddenInGame(!bShow);
    Hand.Beam->SetWorldLocation((Start + BeamEnd) * .5f);
    Hand.Beam->SetWorldRotation((BeamEnd - Start).Rotation());
    Hand.Beam->SetWorldScale3D(FVector(FVector::Distance(Start, BeamEnd)/100.f, LaserThickness/100.f, LaserThickness/100.f));
    Hand.Cursor->SetHiddenInGame(!bShow || !(bOverUI || (bLeftPointer && Hit.bBlockingHit)));
    Hand.Cursor->SetWorldLocation(BeamEnd);
    Hand.Cursor->SetWorldScale3D(FVector(.015f));
}

void UKemVRWidgetInteractorComponent::ReleaseHand(FKemUIHand& Hand, bool bRightHand, bool bCancel)
{
    const bool bPressed = Hand.bPointerPressed;
    Hand.bPointerPressed = false;
    if (Hand.Interaction && bCancel)
    {
        Hand.Interaction->SetCustomHitResult(FHitResult());
        Hand.Interaction->TickComponent(0, LEVELTICK_All, nullptr);
    }
    if (bPressed && Hand.Interaction) Hand.Interaction->ReleasePointerKey(EKeys::LeftMouseButton);
    auto* Pawn = Cast<APawn>(GetOwner());
    if (auto* Menu = UKemEscapeMenuSubsystem::Get(Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr))
        Menu->EndPanelDrag(bRightHand);
    Hand.bDragging = false;
}

void UKemVRWidgetInteractorComponent::CancelInteractions()
{
    ReleaseHand(Left, false, true);
    ReleaseHand(Right, true, true);
    auto* Pawn = Cast<APawn>(GetOwner());
    auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    Left.bTriggerDown = GetTriggerStrength(PC, false) >= .15f;
    Right.bTriggerDown = GetTriggerStrength(PC, true) >= .15f;
}

bool UKemVRWidgetInteractorComponent::IsOverUI() const
{
    return Left.bDragging || Right.bDragging ||
        (Left.Interaction && Left.Interaction->IsOverHitTestVisibleWidget()) ||
        (Right.Interaction && Right.Interaction->IsOverHitTestVisibleWidget());
}

bool UKemVRWidgetInteractorComponent::IsOverUI(bool bRightHand) const
{
    const auto& Hand = bRightHand ? Right : Left;
    return Hand.bDragging || (Hand.Interaction && Hand.Interaction->IsOverHitTestVisibleWidget());
}
