#include "UI/KemEscapeMenuSubsystem.h"
#include "UI/KemEscapeMenuWidget.h"
#include "Actors/KemDesktopCharacter.h"
#include "Actors/KemPlayerControlProxy.h"
#include "Actors/KemClassroomStateActor.h"
#include "Core/KemRoomSubsystem.h"
#include "Engine/KemEngineAssembly.h"
#include "Engine/KemEnginePart.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/KemVRGrabInteractionComponent.h"
#include "MotionControllerComponent.h"
#include "Interaction/KemVRWidgetInteractorComponent.h"
#include "Interaction/KemDrawingBoardInteractionComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "Camera/CameraComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Misc/ConfigCacheIni.h"
#include "AudioDevice.h"
#include "VoipListenerSynthComponent.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"

namespace
{
class FKemEscapeProcessor final : public IInputProcessor
{
public:
    explicit FKemEscapeProcessor(UKemEscapeMenuSubsystem* InOwner) : Owner(InOwner) {}
    virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
    virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
    {
        auto* Menu = Owner.Get();
        UWorld* World = Menu ? Menu->GetTickableGameObjectWorld() : nullptr;
        if (!World || !World->IsGameWorld() || Event.GetKey() != EKeys::Escape) return false;
        UGameViewportClient* Viewport = World->GetGameViewport();
        if (!Viewport || !Viewport->Viewport) return false;
        auto* Widget=Menu->GetMenuWidget();
        auto SlateWidget=Widget ? Widget->GetCachedWidget() : TSharedPtr<SWidget>();
        const bool MenuFocused=SlateWidget.IsValid() && (SlateWidget->HasKeyboardFocus() || SlateWidget->HasFocusedDescendants());
        if(!Viewport->Viewport->HasFocus() && !MenuFocused) return false;
        if (!Event.IsRepeat()) Menu->ToggleMenu();
        return true; // Consume before the editor's End PIE shortcut and focused text boxes.
    }
    TWeakObjectPtr<UKemEscapeMenuSubsystem> Owner;
};
constexpr const TCHAR* Preferences = TEXT("KemSUinVR.EscapeMenu");
}

UKemEscapeMenuSubsystem* UKemEscapeMenuSubsystem::Get(const APlayerController* PC)
{
    return PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UKemEscapeMenuSubsystem>() : nullptr;
}

UWorld* UKemEscapeMenuSubsystem::GetTickableGameObjectWorld() const
{
    return GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
}

void UKemEscapeMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    GConfig->GetFloat(Preferences, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
    GConfig->GetFloat(Preferences, TEXT("VoiceVolume"), VoiceVolume, GGameUserSettingsIni);
    VoiceVolume=FMath::Clamp(VoiceVolume,0.f,1.f);
    GConfig->GetBool(Preferences, TEXT("Microphone"), bMicrophoneEnabled, GGameUserSettingsIni);
    MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
    if (FSlateApplication::IsInitialized())
    {
        EscapeProcessor = MakeShared<FKemEscapeProcessor>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(EscapeProcessor, 0);
    }
}

void UKemEscapeMenuSubsystem::Deinitialize()
{
    SetMenuOpen(false);
    DestroyPresentation();
    if (FSlateApplication::IsInitialized() && EscapeProcessor)
        FSlateApplication::Get().UnregisterInputPreProcessor(EscapeProcessor);
    EscapeProcessor.Reset();
    if (auto* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
    {
        if (Mapping) Input->RemoveMappingContext(Mapping);
        if (GripMapping) Input->RemoveMappingContext(GripMapping);
    }
    Super::Deinitialize();
}

bool UKemEscapeMenuSubsystem::IsVR() const
{
    return GEngine && GEngine->XRSystem.IsValid() && GEngine->IsStereoscopic3D() &&
        GEngine->XRSystem->CountTrackedDevices(EXRTrackedDeviceType::HeadMountedDisplay)>0;
}

void UKemEscapeMenuSubsystem::EnsureInput()
{
    auto* PC = ActiveController.Get();
    if (!PC) return;
    if (!Mapping)
    {
        // OpenXR action sets are built before the first LocalPlayer tick. This
        // persisted context is also listed in DefaultMappingContexts at startup.
        Mapping = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/KUB/Input/IMC_KemVRUI.IMC_KemVRUI"));
        GripMapping = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/KUB/Input/IMC_KemVRGrip.IMC_KemVRGrip"));
        ToggleAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/KUB/Input/IA_KemMenu.IA_KemMenu"));
    }
    if (!Mapping || !ToggleAction) return;
    auto* InputSubsystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (InputSubsystem && !InputSubsystem->HasMappingContext(Mapping))
        InputSubsystem->AddMappingContext(Mapping, 1000);
    // OpenXR action-set priority also affects shared bindings. Keep Grip at the
    // same priority as XRFramework Hands, outside the high-priority menu set.
    if (InputSubsystem && GripMapping && !InputSubsystem->HasMappingContext(GripMapping))
        InputSubsystem->AddMappingContext(GripMapping, 0);
    // Read PlayerInput directly: XR pawns may put their input component on the
    // pawn rather than on the controller. One edge must produce one toggle.
    const auto* Input = Cast<UEnhancedPlayerInput>(PC->PlayerInput);
    const bool bDown = (Input && Input->GetActionValue(ToggleAction).GetMagnitude() > .5f) ||
        PC->IsInputKeyDown(EKeys::Vive_Left_Menu_Click) || PC->IsInputKeyDown(EKeys::Vive_Right_Menu_Click);
    if (bDown && !bMenuButtonDown) ToggleMenu();
    bMenuButtonDown = bDown;
}

void UKemEscapeMenuSubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetTickableGameObjectWorld();
    if (!World || !World->IsGameWorld()) return;
    APlayerController* PC = GetLocalPlayer()->GetPlayerController(World);
    if (ActiveController.Get() != PC || (PanelActor && PanelActor->GetWorld() != World) ||
        (PC && (ActivePawn.Get()!=PC->GetPawn() || (bHadPawn && !PC->GetPawn()))))
    {
        SetMenuOpen(false);
        DestroyPresentation();
        if(ActiveController.Get()!=PC) bForcedMuted = false;
        ActiveController = PC;
        ActivePawn = PC ? PC->GetPawn() : nullptr;
    }
    if (!PC) return;
    ActivePawn = PC->GetPawn(); bHadPawn=PC->GetPawn()!=nullptr;
    EnsureInput();
    LobbyObjects.RemoveAll([World](const auto& Object) { return !Object.IsValid() || Object->GetWorld()!=World; });
    // Lobby has no classroom bootstrap, but local training needs the same grab RPC path.
    if(World->GetNetMode()==NM_Standalone && PC->GetPawn())
    {
        if(!AKemPlayerControlProxy::FindForController(PC))
        {
            FActorSpawnParameters Params; Params.Owner=PC;
            if(auto* Proxy=World->SpawnActor<AKemPlayerControlProxy>(Params)) Proxy->ClientEvaluateLocalControlMode();
        }
        if(IsVR() && !PC->GetPawn()->FindComponentByClass<UKemVRGrabInteractionComponent>())
        {
            auto* Grab=NewObject<UKemVRGrabInteractionComponent>(PC->GetPawn());
            PC->GetPawn()->AddInstanceComponent(Grab); Grab->RegisterComponent();
        }
    }
    if (bWaitForRelease && !PC->IsInputKeyDown(EKeys::LeftMouseButton) &&
        !UKemVRWidgetInteractorComponent::AreTriggersHeld(PC))
        bWaitForRelease = false;
    // The hub also needs UI interaction before the classroom bootstrap exists.
    if (IsVR() && PC->GetPawn() && !PC->GetPawn()->FindComponentByClass<UKemVRWidgetInteractorComponent>())
    {
        auto* Component = NewObject<UKemVRWidgetInteractorComponent>(PC->GetPawn());
        PC->GetPawn()->AddInstanceComponent(Component);
        Component->RegisterComponent();
    }
    PointerRefresh += DeltaTime;
    if (PointerRefresh > .05f)
    {
        PointerRefresh = 0;
        FVector Start; FRotator Rotation; PC->GetPlayerViewPoint(Start,Rotation);
        bool OverUI = false;
        if (IsVR() && PC->GetPawn())
        {
            TArray<UMotionControllerComponent*> Controllers; PC->GetPawn()->GetComponents(Controllers);
            UMotionControllerComponent* Right = nullptr;
            for (auto* C : Controllers)
                if (C->MotionSource.ToString().Contains(TEXT("Right")))
                { Right=C; if(C->MotionSource.ToString().Contains(TEXT("Aim"))) break; }
            if (Right) { Start=Right->GetComponentLocation(); Rotation=Right->GetComponentRotation(); }
            if (auto* UI=PC->GetPawn()->FindComponentByClass<UKemVRWidgetInteractorComponent>()) OverUI=UI->IsOverUI(true);
        }
        bool Active = CanUseTool() && bPointerEnabled && !OverUI;
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(KemPointer),false,PC->GetPawn());
        FVector End=Start+Rotation.Vector()*4000.f;
        if (World->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Query)) End=Hit.ImpactPoint;
        if (Active || bPointerWasActive)
            if (auto* Proxy=AKemPlayerControlProxy::FindForController(PC)) Proxy->ServerUpdatePointer(Start,End,Active);
        bPointerWasActive = Active;
    }
    AudioRefresh += DeltaTime;
    if (AudioRefresh > 1.f)
    {
        AudioRefresh = 0;
        if (auto Device = World->GetAudioDevice()) Device->SetTransientPrimaryVolume(MasterVolume);
        for(TObjectIterator<UVoipListenerSynthComponent> It;It;++It)
            if(It->GetWorld()==World) It->SetVolumeMultiplier(VoiceVolume);
    }
}

void UKemEscapeMenuSubsystem::DestroyPresentation()
{
    bPanelDragging = false;
    if (auto* PC = ActiveController.Get())
        if (APawn* Pawn = PC->GetPawn())
            if (auto* UI = Pawn->FindComponentByClass<UKemVRWidgetInteractorComponent>()) UI->CancelInteractions();
    if (MenuWidget) MenuWidget->RemoveFromParent();
    if (IsValid(PanelActor)) PanelActor->Destroy();
    MenuWidget = nullptr; WorldWidget = nullptr; PanelActor = nullptr;
}

void UKemEscapeMenuSubsystem::ToggleMenu() { SetMenuOpen(!bMenuOpen); }

void UKemEscapeMenuSubsystem::SetMenuOpen(bool bOpen)
{
    if (bMenuOpen == bOpen) return;
    if (!bOpen && MenuWidget) LastSection = MenuWidget->GetSelectedSection();
    APlayerController* PC = ActiveController.Get();
    if(!bOpen && !PC)
    {
        bMenuOpen=false; bWaitForRelease=true; bAppliedInputLock=false;
        DestroyPresentation(); return;
    }
    if (!PC) PC = GetLocalPlayer()->GetPlayerController(GetWorld());
    if (!PC) return;
    ActiveController = PC;
    if (bOpen)
    {
        bWasCursorVisible = PC->bShowMouseCursor;
        bWasClickEnabled = PC->bEnableClickEvents;
        bWasHoverEnabled = PC->bEnableMouseOverEvents;
        if (auto* Desktop = Cast<AKemDesktopCharacter>(PC->GetPawn())) Desktop->SuspendForMenu();
        MenuWidget = CreateWidget<UKemEscapeMenuWidget>(PC, UKemEscapeMenuWidget::StaticClass());
        if (!MenuWidget) return;
        MenuWidget->SetVRLayout(IsVR());
        MenuWidget->SelectSection(LastSection);
        bMenuOpen = true;
        PC->SetIgnoreMoveInput(true); bAppliedInputLock = true;
        if (IsVR())
        {
            PanelActor = PC->GetWorld()->SpawnActor<AActor>();
            WorldWidget = NewObject<UWidgetComponent>(PanelActor);
            PanelActor->SetRootComponent(WorldWidget);
            WorldWidget->SetMobility(EComponentMobility::Movable);
            WorldWidget->SetPivot(FVector2D(.5f,.5f));
            WorldWidget->SetGenerateOverlapEvents(false);
            WorldWidget->SetWidgetSpace(EWidgetSpace::World);
            WorldWidget->SetDrawSize(MenuWidget->GetDesignSize());
            WorldWidget->SetTwoSided(true);
            WorldWidget->SetBlendMode(EWidgetBlendMode::Transparent);
            WorldWidget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            WorldWidget->SetCollisionResponseToAllChannels(ECR_Ignore);
            WorldWidget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
            WorldWidget->RegisterComponent();
            WorldWidget->SetWidget(MenuWidget);
            PanelActor->SetActorScale3D(FVector(.085f));
            RepositionPanel();
        }
        else
        {
            MenuWidget->AddToPlayerScreen(100);
            PC->bShowMouseCursor = true; PC->bEnableClickEvents = true; PC->bEnableMouseOverEvents = true;
            FInputModeGameAndUI Mode;
            Mode.SetWidgetToFocus(MenuWidget->TakeWidget());
            Mode.SetHideCursorDuringCapture(false);
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(Mode);
        }
    }
    else
    {
        bMenuOpen = false; bWaitForRelease = true;
        if (bAppliedInputLock) PC->SetIgnoreMoveInput(false);
        bAppliedInputLock = false;
        PC->bShowMouseCursor = bWasCursorVisible;
        PC->bEnableClickEvents = bWasClickEnabled;
        PC->bEnableMouseOverEvents = bWasHoverEnabled;
        PC->SetInputMode(FInputModeGameOnly());
        if(PC->GetPawn())
            if(auto* Interaction=PC->GetPawn()->FindComponentByClass<UWidgetInteractionComponent>())
                Interaction->bShowDebug=PC->GetPawn()->IsA<AKemDesktopCharacter>();
        DestroyPresentation();
    }
}

void UKemEscapeMenuSubsystem::RepositionPanel()
{
    auto* PC = ActiveController.Get();
    if (!PC || !PanelActor) return;
    FVector View; FRotator Rotation; PC->GetPlayerViewPoint(View, Rotation);
    if (APawn* Pawn = PC->GetPawn())
    {
        TInlineComponentArray<UCameraComponent*> Cameras(Pawn);
        for (auto* Camera : Cameras)
            if (Camera->IsActive() && Camera->bLockToHmd)
            {
                View = Camera->GetComponentLocation();
                Rotation = Camera->GetComponentRotation();
                break;
            }
    }
    Rotation.Pitch = 0; Rotation.Roll = 0;
    PanelActor->SetActorLocation(View + Rotation.Vector() * 90.f - FVector(0, 0, 5));
    PanelActor->SetActorRotation(FRotator(0, Rotation.Yaw + 180.f, 0));
    bPanelDragging = false;
}

bool UKemEscapeMenuSubsystem::IsPanelEdge(const FVector2D& Point, const FVector2D& Size)
{
    constexpr float Border = 24.f; // Matches the dedicated outer frame padding.
    return Size.X > Border*2 && Size.Y > Border*2 && Point.X >= 0 && Point.Y >= 0 &&
        Point.X <= Size.X && Point.Y <= Size.Y &&
        (Point.X <= Border || Point.Y <= Border || Point.X >= Size.X-Border || Point.Y >= Size.Y-Border);
}

bool UKemEscapeMenuSubsystem::BeginPanelDrag(bool bRightHand, const FTransform& HandPose, const FHitResult& Hit)
{
    if (!bMenuOpen || !PanelActor || !WorldWidget || Hit.GetComponent() != WorldWidget) return false;
    const FVector Local = WorldWidget->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
    const FVector2D Point = WorldWidget->GetDrawSize() * WorldWidget->GetPivot() - FVector2D(Local.Y, Local.Z);
    if (!IsPanelEdge(Point, WorldWidget->GetDrawSize())) return false;
    if (!bPanelDragging)
    {
        bPanelDragging = true;
        bDragRightHand = bRightHand;
        PanelRelativeToHand = PanelActor->GetActorTransform().GetRelativeTransform(HandPose);
    }
    return true; // A second hand on the frame must not click through it.
}

void UKemEscapeMenuSubsystem::UpdatePanelDrag(bool bRightHand, const FTransform& HandPose)
{
    if (bPanelDragging && bDragRightHand == bRightHand && bMenuOpen && IsValid(PanelActor))
        PanelActor->SetActorTransform(PanelRelativeToHand * HandPose);
}

void UKemEscapeMenuSubsystem::EndPanelDrag(bool bRightHand)
{
    if (bDragRightHand == bRightHand) bPanelDragging = false;
}

void UKemEscapeMenuSubsystem::SetTool(EKemTool NewTool) { Tool = NewTool; }
void UKemEscapeMenuSubsystem::SavePreferences()
{
    GConfig->SetFloat(Preferences, TEXT("VoiceVolume"), VoiceVolume, GGameUserSettingsIni);
    GConfig->SetFloat(Preferences, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
    GConfig->SetBool(Preferences, TEXT("Microphone"), bMicrophoneEnabled, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}
void UKemEscapeMenuSubsystem::SetMasterVolume(float Value)
{
    MasterVolume = FMath::Clamp(Value, 0.f, 1.f);
    if (GetWorld()) if (auto Device = GetWorld()->GetAudioDevice()) Device->SetTransientPrimaryVolume(MasterVolume);
    SavePreferences();
}
void UKemEscapeMenuSubsystem::SetMicrophoneEnabled(bool bEnabled)
{
    bMicrophoneEnabled = bEnabled;
    if (ActiveController.IsValid()) ActiveController->ConsoleCommand(
        bEnabled && !bForcedMuted ? TEXT("ToggleSpeaking 1") : TEXT("ToggleSpeaking 0"), false);
    SavePreferences();
}
void UKemEscapeMenuSubsystem::SetForcedMuted(bool bMuted)
{
    bForcedMuted = bMuted;
    if (ActiveController.IsValid()) ActiveController->ConsoleCommand(
        bMicrophoneEnabled && !bForcedMuted ? TEXT("ToggleSpeaking 1") : TEXT("ToggleSpeaking 0"), false);
}

void UKemEscapeMenuSubsystem::SetVoiceVolume(float Value)
{
    VoiceVolume=FMath::Clamp(Value,0.f,1.f);
    for(TObjectIterator<UVoipListenerSynthComponent> It;It;++It)
        if(It->GetWorld()==GetWorld()) It->SetVolumeMultiplier(VoiceVolume);
    SavePreferences();
}

void UKemEscapeMenuSubsystem::TogglePointer() { bPointerEnabled=!bPointerEnabled; }

bool UKemEscapeMenuSubsystem::CanManageSceneObjects() const
{
    UWorld* World=GetTickableGameObjectWorld();
    if(!World || !World->IsGameWorld() || World->GetNetMode()==NM_Client) return false;
    const auto* Room=World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UKemRoomSubsystem>() : nullptr;
    if(!Room || !Room->GetCurrentRoomInfo().bIsInRoom) return World->GetNetMode()==NM_Standalone;
    const auto* State=Room->GetCurrentClassroomState();
    return Room->IsHost() && State && State->HasAuthority();
}
FTransform UKemEscapeMenuSubsystem::GetObjectSpawnTransform() const
{
    UWorld* World=GetTickableGameObjectWorld();
    auto* PC=World ? GetLocalPlayer()->GetPlayerController(World) : nullptr;
    if(!PC) return FTransform::Identity;
    FVector View; FRotator Rotation; PC->GetPlayerViewPoint(View,Rotation);
    Rotation.Pitch=0; Rotation.Roll=0;
    FVector Position=View+Rotation.Vector()*220.f;
    FHitResult Floor; FCollisionQueryParams Query(SCENE_QUERY_STAT(KemMenuObjectSpawn),false,PC->GetPawn());
    for(const auto& Object : LobbyObjects) if(Object.IsValid())
    {
        Query.AddIgnoredActor(Object.Get());
        if(auto* Engine=Cast<AKemEngineAssembly>(Object.Get()))
            for(AKemEnginePart* Part : Engine->Parts) if(IsValid(Part)) Query.AddIgnoredActor(Part);
    }
    if(World->LineTraceSingleByChannel(Floor,Position,Position-FVector(0,0,500),ECC_Visibility,Query))
        Position.Z=Floor.ImpactPoint.Z+80.f;
    return FTransform(Rotation,Position);
}
AActor* UKemEscapeMenuSubsystem::SpawnSceneObject(FName ObjectId)
{
    if(!CanManageSceneObjects()) return nullptr;
    const bool Engine=ObjectId==TEXT("engine");
    if(!Engine && ObjectId!=TEXT("cube") && ObjectId!=TEXT("sphere")) return nullptr;
    UWorld* World=GetTickableGameObjectWorld();
    FTransform Transform=GetObjectSpawnTransform();
    if(Engine) Transform.SetScale3D(FVector(1.35f));
    const FName CatalogTag(*FString::Printf(TEXT("KemCatalog:%s"),*ObjectId.ToString()));
    for(const auto& Existing : LobbyObjects)
        if(Existing.IsValid() && Existing->Tags.Contains(CatalogTag) && CanManageSceneObject(Existing.Get())) return Existing.Get();
    auto* Room=World->GetGameInstance()->GetSubsystem<UKemRoomSubsystem>();
    if(Room && Room->GetCurrentRoomInfo().bIsInRoom)
    {
        auto* State=Room->GetCurrentClassroomState();
        AActor* Object=Engine ? State->SpawnEngine2112(Transform) : State->SpawnPresentationPrimitive(
            ObjectId==TEXT("cube") ? EKemPresentationPrimitive::Cube : EKemPresentationPrimitive::Sphere,Transform);
        if(Object) { Object->Tags.AddUnique(CatalogTag); LobbyObjects.AddUnique(Object); }
        return Object;
    }
    if(Engine)
        for(const auto& Object : LobbyObjects)
            if(Object.IsValid() && Object->IsA<AKemEngineAssembly>()) return Object.Get();
    AActor* Object=nullptr;
    if(Engine) Object=World->SpawnActor<AKemEngineAssembly>(AKemEngineAssembly::StaticClass(),Transform);
    else
    {
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,ObjectId==TEXT("cube") ? TEXT("/Engine/BasicShapes/Cube.Cube") : TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        if(!Mesh) return nullptr;
        auto* Primitive=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),Transform);
        if(Primitive)
        {
            Primitive->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Primitive->GetStaticMeshComponent()->SetStaticMesh(Mesh);
        }
        Object=Primitive;
    }
    if(Object) { Object->Tags.AddUnique(CatalogTag); LobbyObjects.AddUnique(Object); }
    return Object;
}
bool UKemEscapeMenuSubsystem::CanManageSceneObject(const AActor* Object) const
{
    if(!IsValid(Object) || Object->GetWorld()!=GetTickableGameObjectWorld() || !CanManageSceneObjects()) return false;
    auto* Room=GetWorld()->GetGameInstance()->GetSubsystem<UKemRoomSubsystem>();
    if(Room && Room->GetCurrentRoomInfo().bIsInRoom)
        return Room->GetCurrentClassroomState()->OwnsPresentationActor(Object);
    return LobbyObjects.ContainsByPredicate([Object](const auto& Entry) { return Entry.Get()==Object; });
}
bool UKemEscapeMenuSubsystem::RemoveSceneObject(AActor* Object)
{
    if(!CanManageSceneObject(Object)) return false;
    if(auto* Engine=Cast<AKemEngineAssembly>(Object)) if(Engine->IsBusy()) return false;
    auto* Room=GetWorld()->GetGameInstance()->GetSubsystem<UKemRoomSubsystem>();
    if(Room && Room->GetCurrentRoomInfo().bIsInRoom) return Room->GetCurrentClassroomState()->RemovePresentationActor(Object);
    LobbyObjects.RemoveAll([Object](const auto& Entry) { return Entry.Get()==Object; });
    return Object->Destroy();
}
bool UKemEscapeMenuSubsystem::MoveSceneObjectInFront(AActor* Object)
{
    if(!CanManageSceneObject(Object)) return false;
    // Engine sockets/trays are derived from the stand transform; held parts must be released first.
    if(auto* Engine=Cast<AKemEngineAssembly>(Object))
    {
        if(Engine->IsBusy()) return false;
        FTransform Transform=GetObjectSpawnTransform(); Transform.SetScale3D(Object->GetActorScale3D());
        Object->SetActorTransform(Transform);
        return Engine->ConfigureLesson(Engine->GetLesson(),true);
    }
    FTransform Transform=GetObjectSpawnTransform(); Transform.SetScale3D(Object->GetActorScale3D());
    Object->SetActorTransform(Transform); Object->ForceNetUpdate(); return true;
}
