#include "Actors/KemWorldActors.h"

#include "Actors/KemClassroomStateActor.h"
#include "Actors/KemDesktopCharacter.h"
#include "Actors/KemDrawingBoardActor.h"
#include "Actors/KemPlayerControlProxy.h"
#include "Algo/Sort.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/KemRoomSubsystem.h"
#include "Interaction/KemDrawingBoardInteractionComponent.h"
#include "Interaction/KemVRGrabInteractionComponent.h"
#include "Interaction/KemVRWidgetInteractorComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "IXRTrackingSystem.h"
#include "Misc/PackageName.h"
#include "UI/KemWidgets.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FName KemScreenResizeTag(TEXT("Kem.Screen.Resize"));
const FName KemScreenMoveTag(TEXT("Kem.Screen.Move"));
const FName KemScreenCloseTag(TEXT("Kem.Screen.Close"));
const FName KemScreenCopyTag(TEXT("Kem.Screen.Copy"));

bool IsCurrentMapNamed(const UWorld* World, const FString& TargetMapName)
{
    if (!World)
    {
        return false;
    }

    FString MapName = World->GetMapName();
    const FString PIEPrefix = TEXT("UEDPIE_");
    if (MapName.StartsWith(PIEPrefix))
    {
        int32 PrefixEndIndex = INDEX_NONE;
        if (MapName.FindChar(TEXT('_'), PrefixEndIndex))
        {
            const int32 SecondUnderscoreIndex = MapName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, PrefixEndIndex + 1);
            if (SecondUnderscoreIndex != INDEX_NONE && SecondUnderscoreIndex + 1 < MapName.Len())
            {
                MapName.RightChopInline(SecondUnderscoreIndex + 1, EAllowShrinking::No);
            }
        }
    }

    return FPackageName::GetShortName(MapName).Equals(TargetMapName, ESearchCase::IgnoreCase);
}

void ConfigureInvisibleHandleTrigger(UBoxComponent* TriggerComponent)
{
    if (!TriggerComponent)
    {
        return;
    }

    TriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    TriggerComponent->SetGenerateOverlapEvents(false);
    TriggerComponent->SetHiddenInGame(true);
}
}

AKemWidgetPanelActorBase::AKemWidgetPanelActorBase()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SceneRoot->SetMobility(EComponentMobility::Movable);
    RootComponent = SceneRoot;

    WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComponent"));
    WidgetComponent->SetupAttachment(SceneRoot);
    WidgetComponent->SetWidgetSpace(EWidgetSpace::World);
    WidgetComponent->SetDrawAtDesiredSize(false);
    WidgetComponent->SetTwoSided(true);
    WidgetComponent->SetCollisionProfileName(TEXT("UI"));
    WidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
}

void AKemWidgetPanelActorBase::BeginPlay()
{
    Super::BeginPlay();
    // Existing placed panels can still contain the old serialized Static mobility.
    SceneRoot->SetMobility(EComponentMobility::Movable);
    WidgetComponent->InitWidget();
}

void AKemWidgetPanelActorBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (WidgetComponent)
    {
        WidgetComponent->InitWidget();
    }
}

void AKemWidgetPanelActorBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bFaceLocalPlayer && !IsHidden())
    {
        FaceFirstPlayer();
    }
}

void AKemWidgetPanelActorBase::FaceFirstPlayer()
{
    if (APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        FVector ViewLocation = FVector::ZeroVector;
        FRotator ViewRotation = FRotator::ZeroRotator;
        PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

        const FVector ToViewer = ViewLocation - GetActorLocation();
        const FRotator DesiredRotation = ToViewer.Rotation();
        if (!ToViewer.IsNearlyZero() && !GetActorRotation().Equals(DesiredRotation, 0.1f))
        {
            SetActorRotation(DesiredRotation);
        }
    }
}

AKemHubActor::AKemHubActor()
{
    bFaceLocalPlayer = false;
    WidgetComponent->SetWidgetClass(UKemHubMenuWidget::StaticClass());
    WidgetComponent->SetDrawSize(FVector2D(1100.0f, 1200.0f));
    SetActorScale3D(FVector(0.08f));
}

void AKemHubActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UKemRoomSubsystem* RoomSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKemRoomSubsystem>() : nullptr;
    SetActorHiddenInGame(RoomSubsystem ? RoomSubsystem->GetCurrentRoomInfo().bIsInRoom : false);
}

AKemRoomCodeDisplayActor::AKemRoomCodeDisplayActor()
{
    bFaceLocalPlayer = false;
    WidgetComponent->SetWidgetClass(UKemRoomCodeWidget::StaticClass());
    WidgetComponent->SetDrawSize(FVector2D(700.0f, 280.0f));
    SetActorScale3D(FVector(0.08f));
}

void AKemRoomCodeDisplayActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (UKemRoomSubsystem* RoomSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKemRoomSubsystem>() : nullptr)
    {
        SetActorHiddenInGame(!RoomSubsystem->IsHost());
    }
}

AKemReturnToLobbyActor::AKemReturnToLobbyActor()
{
    bFaceLocalPlayer = true;
    WidgetComponent->SetWidgetClass(UKemReturnToLobbyWidget::StaticClass());
    WidgetComponent->SetDrawSize(FVector2D(640.0f, 260.0f));
    SetActorScale3D(FVector(0.08f));
}

AKemDrawingBoardControlPanelActor::AKemDrawingBoardControlPanelActor()
{
    bFaceLocalPlayer = true;
    WidgetComponent->SetWidgetClass(UKemDrawingBoardControlWidget::StaticClass());
    WidgetComponent->SetDrawSize(FVector2D(560.0f, 980.0f));
    SetActorScale3D(FVector(0.08f));
}

void AKemDrawingBoardControlPanelActor::BeginPlay()
{
    Super::BeginPlay();
    BindNearestBoard();
}

void AKemDrawingBoardControlPanelActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BindNearestBoard();
}

void AKemDrawingBoardControlPanelActor::BindNearestBoard()
{
    if (!GetWorld())
    {
        return;
    }

    AKemDrawingBoardActor* ResolvedBoard = TargetBoard;
    if (!ResolvedBoard)
    {
        float BestDistanceSquared = TNumericLimits<float>::Max();
        for (TActorIterator<AKemDrawingBoardActor> It(GetWorld()); It; ++It)
        {
            AKemDrawingBoardActor* Candidate = *It;
            if (!Candidate)
            {
                continue;
            }

            const float DistanceSquared = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
            if (DistanceSquared < BestDistanceSquared)
            {
                BestDistanceSquared = DistanceSquared;
                ResolvedBoard = Candidate;
            }
        }

        TargetBoard = ResolvedBoard;
    }

    if (!WidgetComponent)
    {
        return;
    }

    WidgetComponent->InitWidget();
    if (UKemDrawingBoardControlWidget* ControlWidget = Cast<UKemDrawingBoardControlWidget>(WidgetComponent->GetWidget()))
    {
        ControlWidget->SetTargetBoard(ResolvedBoard);
    }
}

AKemTeacherControlPanelActor::AKemTeacherControlPanelActor()
{
    bFaceLocalPlayer = false;
    WidgetComponent->SetWidgetClass(UKemTeacherControlWidget::StaticClass());
    WidgetComponent->SetDrawSize(FVector2D(900.0f, 1200.0f));
    SetActorScale3D(FVector(0.07f));
}

void AKemTeacherControlPanelActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (UKemRoomSubsystem* RoomSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKemRoomSubsystem>() : nullptr)
    {
        SetActorHiddenInGame(!RoomSubsystem->IsHost());
    }
}

AKemBrowserDeskActor::AKemBrowserDeskActor()
{
    bFaceLocalPlayer = false;
    DeskScreenGroupRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Group_DeskScreen"));
    DeskScreenGroupRoot->SetupAttachment(SceneRoot);

    WidgetComponent->SetupAttachment(DeskScreenGroupRoot);
    WidgetComponent->SetWidgetClass(UKemBrowserDeskWidget::StaticClass());
    WidgetComponent->SetDrawSize(FVector2D(1920.0f, 1080.0f));
    WidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

    DeskScreenBackdropVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual_DeskScreenBackdrop"));
    DeskScreenBackdropVisual->SetupAttachment(DeskScreenGroupRoot);
    DeskScreenBackdropVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DeskScreenBackdropVisual->SetRelativeLocation(FVector(0.0f, 0.0f, -5.0f));
    DeskScreenBackdropVisual->SetRelativeScale3D(FVector(16.5f, 9.3f, 0.08f));

    DeskKeyboardGroupRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Group_DeskKeyboard"));
    DeskKeyboardGroupRoot->SetupAttachment(SceneRoot);
    DeskKeyboardGroupRoot->SetRelativeLocation(FVector(0.0f, 115.0f, -24.0f));

    DeskKeyboardVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual_DeskKeyboard"));
    DeskKeyboardVisual->SetupAttachment(DeskKeyboardGroupRoot);
    DeskKeyboardVisual->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    DeskKeyboardVisual->SetRelativeScale3D(FVector(7.5f, 2.7f, 0.2f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        DeskScreenBackdropVisual->SetStaticMesh(CubeMesh.Object);
        DeskKeyboardVisual->SetStaticMesh(CubeMesh.Object);
    }

    SetActorScale3D(FVector(0.05f));
}

void AKemBrowserDeskActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (UKemRoomSubsystem* RoomSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKemRoomSubsystem>() : nullptr)
    {
        SetActorHiddenInGame(!RoomSubsystem->IsHost());
    }
}

AKemSharedLectureScreenActor::AKemSharedLectureScreenActor()
{
    bFaceLocalPlayer = false;
    ScreenGroupRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Group_Screen"));
    ScreenGroupRoot->SetupAttachment(SceneRoot);

    WidgetComponent->SetupAttachment(ScreenGroupRoot);
    WidgetComponent->SetWidgetClass(UKemLectureScreenWidget::StaticClass());
    WidgetComponent->SetDrawSize(FVector2D(1600.0f, 1100.0f));
    WidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    WidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ScreenBackdropVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual_ScreenBackdrop"));
    ScreenBackdropVisual->SetupAttachment(ScreenGroupRoot);
    ScreenBackdropVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ScreenBackdropVisual->SetRelativeLocation(FVector(0.0f, 0.0f, -4.0f));
    ScreenBackdropVisual->SetRelativeScale3D(FVector(13.5f, 9.0f, 0.08f));

    ControlsGroupRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Group_Controls"));
    ControlsGroupRoot->SetupAttachment(SceneRoot);
    ControlsGroupRoot->SetRelativeLocation(FVector(0.0f, 0.0f, 56.0f));

    CopyHandleVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual_CopyHandle"));
    CopyHandleVisual->SetupAttachment(ControlsGroupRoot);
    CopyHandleVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CopyHandleVisual->SetRelativeLocation(FVector(48.0f, 0.0f, 0.0f));
    CopyHandleVisual->SetRelativeScale3D(FVector(1.3f, 1.3f, 0.18f));

    CopyHandleTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger_CopyHandle"));
    CopyHandleTrigger->SetupAttachment(CopyHandleVisual);
    CopyHandleTrigger->SetBoxExtent(FVector(22.0f, 22.0f, 8.0f));
    CopyHandleTrigger->ComponentTags.Add(KemScreenCopyTag);
    ConfigureInvisibleHandleTrigger(CopyHandleTrigger);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        ScreenBackdropVisual->SetStaticMesh(CubeMesh.Object);
        CopyHandleVisual->SetStaticMesh(CubeMesh.Object);
    }

    SetActorScale3D(FVector(0.09f));
}

void AKemSharedLectureScreenActor::BeginPlay()
{
    Super::BeginPlay();

    if (UKemLectureScreenWidget* ScreenWidget = Cast<UKemLectureScreenWidget>(WidgetComponent->GetUserWidgetObject()))
    {
        ScreenWidget->SetAllowLocalCloneButton(false);
    }
}

bool AKemSharedLectureScreenActor::IsCopyHandle(const UPrimitiveComponent* Component) const
{
    return Component == CopyHandleTrigger;
}

AKemLocalLectureScreenActor* AKemSharedLectureScreenActor::SpawnLocalCopyForController(APlayerController* PlayerController) const
{
    if (!PlayerController || !GetWorld())
    {
        return nullptr;
    }

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AKemLocalLectureScreenActor* NewScreen = GetWorld()->SpawnActor<AKemLocalLectureScreenActor>(
        ViewLocation + ViewRotation.Vector() * 220.0f,
        ViewRotation,
        SpawnParameters);

    if (NewScreen)
    {
        NewScreen->SetActorScale3D(FVector(0.75f));
    }

    return NewScreen;
}

AKemLocalLectureScreenActor::AKemLocalLectureScreenActor()
{
    bReplicates = false;
    bFaceLocalPlayer = false;
    ScreenGroupRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Group_Screen"));
    ScreenGroupRoot->SetupAttachment(SceneRoot);

    WidgetComponent->SetupAttachment(ScreenGroupRoot);
    WidgetComponent->SetWidgetClass(UKemLectureScreenWidget::StaticClass());
    WidgetComponent->SetDrawSize(FVector2D(1200.0f, 850.0f));
    WidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -6.0f));

    ScreenBackdropVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual_ScreenBackdrop"));
    ScreenBackdropVisual->SetupAttachment(ScreenGroupRoot);
    ScreenBackdropVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ScreenBackdropVisual->SetRelativeLocation(FVector(0.0f, 0.0f, -10.0f));
    ScreenBackdropVisual->SetRelativeScale3D(FVector(10.0f, 7.0f, 0.08f));

    ControlsGroupRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Group_Controls"));
    ControlsGroupRoot->SetupAttachment(SceneRoot);
    ControlsGroupRoot->SetRelativeLocation(FVector(0.0f, 0.0f, 58.0f));

    ResizeHandleVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual_ResizeHandle"));
    ResizeHandleVisual->SetupAttachment(ControlsGroupRoot);
    ResizeHandleVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ResizeHandleVisual->SetRelativeLocation(FVector(-58.0f, 0.0f, 0.0f));
    ResizeHandleVisual->SetRelativeScale3D(FVector(1.3f, 1.3f, 0.18f));

    MoveHandleVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual_MoveHandle"));
    MoveHandleVisual->SetupAttachment(ControlsGroupRoot);
    MoveHandleVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MoveHandleVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    MoveHandleVisual->SetRelativeScale3D(FVector(4.8f, 1.3f, 0.18f));

    CloseHandleVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual_CloseHandle"));
    CloseHandleVisual->SetupAttachment(ControlsGroupRoot);
    CloseHandleVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CloseHandleVisual->SetRelativeLocation(FVector(58.0f, 0.0f, 0.0f));
    CloseHandleVisual->SetRelativeScale3D(FVector(1.3f, 1.3f, 0.18f));

    ResizeHandleTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger_ResizeHandle"));
    ResizeHandleTrigger->SetupAttachment(ResizeHandleVisual);
    ResizeHandleTrigger->SetBoxExtent(FVector(20.0f, 20.0f, 8.0f));
    ResizeHandleTrigger->ComponentTags.Add(KemScreenResizeTag);
    ConfigureInvisibleHandleTrigger(ResizeHandleTrigger);

    MoveHandleTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger_MoveHandle"));
    MoveHandleTrigger->SetupAttachment(MoveHandleVisual);
    MoveHandleTrigger->SetBoxExtent(FVector(72.0f, 20.0f, 8.0f));
    MoveHandleTrigger->ComponentTags.Add(KemScreenMoveTag);
    ConfigureInvisibleHandleTrigger(MoveHandleTrigger);

    CloseHandleTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger_CloseHandle"));
    CloseHandleTrigger->SetupAttachment(CloseHandleVisual);
    CloseHandleTrigger->SetBoxExtent(FVector(20.0f, 20.0f, 8.0f));
    CloseHandleTrigger->ComponentTags.Add(KemScreenCloseTag);
    ConfigureInvisibleHandleTrigger(CloseHandleTrigger);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        ScreenBackdropVisual->SetStaticMesh(CubeMesh.Object);
        ResizeHandleVisual->SetStaticMesh(CubeMesh.Object);
        MoveHandleVisual->SetStaticMesh(CubeMesh.Object);
        CloseHandleVisual->SetStaticMesh(CubeMesh.Object);
    }

    SetActorScale3D(FVector(0.07f));
}

void AKemLocalLectureScreenActor::BeginPlay()
{
    Super::BeginPlay();

    if (UKemLectureScreenWidget* ScreenWidget = Cast<UKemLectureScreenWidget>(WidgetComponent->GetUserWidgetObject()))
    {
        ScreenWidget->SetAllowLocalCloneButton(false);
    }
}

EKemScreenHandleType AKemLocalLectureScreenActor::ResolveHandleType(const UPrimitiveComponent* Component) const
{
    if (Component == ResizeHandleTrigger)
    {
        return EKemScreenHandleType::Resize;
    }

    if (Component == MoveHandleTrigger)
    {
        return EKemScreenHandleType::Move;
    }

    if (Component == CloseHandleTrigger)
    {
        return EKemScreenHandleType::Close;
    }

    return EKemScreenHandleType::None;
}

AKemParticipantModeratorActor::AKemParticipantModeratorActor()
{
    bReplicates = false;
    bFaceLocalPlayer = false;
    WidgetComponent->SetWidgetClass(UKemParticipantModeratorWidget::StaticClass());
    WidgetComponent->SetDrawSize(FVector2D(780.0f, 700.0f));
    SetActorScale3D(FVector(0.065f));
}

void AKemParticipantModeratorActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!ObservedPawn.IsValid())
    {
        Destroy();
        return;
    }

    SetActorLocation(ObservedPawn->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f));
}

void AKemParticipantModeratorActor::BindToParticipant(APawn* TargetPawn, const FKemParticipantInfo& ParticipantInfo)
{
    ObservedPawn = TargetPawn;
    CachedParticipantInfo = ParticipantInfo;

    if (UKemParticipantModeratorWidget* ModeratorWidget = Cast<UKemParticipantModeratorWidget>(WidgetComponent->GetUserWidgetObject()))
    {
        ModeratorWidget->SetParticipantInfo(CachedParticipantInfo);
    }
}

bool AKemParticipantModeratorActor::IsBoundToPawn(const APawn* Pawn) const
{
    return ObservedPawn.Get() == Pawn;
}

AKemHostModeratorManagerActor::AKemHostModeratorManagerActor()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AKemHostModeratorManagerActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    RefreshAccumulator += DeltaSeconds;
    if (RefreshAccumulator < 0.5f)
    {
        return;
    }
    RefreshAccumulator = 0.0f;

    UKemRoomSubsystem* RoomSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKemRoomSubsystem>() : nullptr;
    AKemClassroomStateActor* RoomState = RoomSubsystem ? RoomSubsystem->GetCurrentClassroomState() : nullptr;
    if (!RoomSubsystem || !RoomSubsystem->IsHost() || !RoomState)
    {
        for (TPair<TWeakObjectPtr<APawn>, TObjectPtr<AKemParticipantModeratorActor>>& Entry : ModeratorPanels)
        {
            if (Entry.Value)
            {
                Entry.Value->Destroy();
            }
        }
        ModeratorPanels.Reset();
        return;
    }

    APawn* LocalPawn = GetWorld() && GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    TSet<TWeakObjectPtr<APawn>> SeenPawns;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PlayerController = It->Get();
        APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
        if (!Pawn || Pawn == LocalPawn || !PlayerController->PlayerState)
        {
            continue;
        }

        const FString ParticipantId = PlayerController->PlayerState->GetUniqueId().IsValid()
            ? PlayerController->PlayerState->GetUniqueId()->ToString()
            : PlayerController->PlayerState->GetPlayerName();

        const FKemParticipantInfo* Participant = RoomState->GetParticipants().FindByPredicate([&ParticipantId](const FKemParticipantInfo& Entry)
        {
            return Entry.ParticipantId == ParticipantId;
        });

        if (!Participant)
        {
            continue;
        }

        SeenPawns.Add(Pawn);

        TObjectPtr<AKemParticipantModeratorActor>* ExistingPanel = ModeratorPanels.Find(Pawn);
        if (!ExistingPanel || !*ExistingPanel)
        {
            FActorSpawnParameters SpawnParameters;
            SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AKemParticipantModeratorActor* NewPanel = GetWorld()->SpawnActor<AKemParticipantModeratorActor>(Pawn->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f), FRotator::ZeroRotator, SpawnParameters);
            if (NewPanel)
            {
                NewPanel->BindToParticipant(Pawn, *Participant);
                ModeratorPanels.Add(Pawn, NewPanel);
            }
        }
        else
        {
            (*ExistingPanel)->BindToParticipant(Pawn, *Participant);
        }
    }

    for (auto It = ModeratorPanels.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || !SeenPawns.Contains(It.Key()))
        {
            if (It.Value())
            {
                It.Value()->Destroy();
            }
            It.RemoveCurrent();
        }
    }
}

AKemClassroomBootstrapActor::AKemClassroomBootstrapActor()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AKemClassroomBootstrapActor::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        EnsurePlayerControlProxies();
    }
}

void AKemClassroomBootstrapActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority())
    {
        ProxyRefreshAccumulator += DeltaSeconds;
        if (ProxyRefreshAccumulator >= 0.5f)
        {
            ProxyRefreshAccumulator = 0.0f;
            EnsurePlayerControlProxies();
        }

        EnsureAuditoriumSpawnsAssigned();
    }

    EnsureLocalPlayerControlMode();
    EnsureLocalVRWidgetInteractor();
    EnsureLocalDrawingBoardInteractor();
    EnsureLocalVRGrabInteractor();
    TeleportLocalPawnToTaggedSpawnIfNeeded();
    EnsureLocalLayoutInitialized();
}

void AKemClassroomBootstrapActor::EnsureAuditoriumSpawnsAssigned()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    UKemRoomSubsystem* RoomSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKemRoomSubsystem>() : nullptr;
    if (!RoomSubsystem || !RoomSubsystem->GetCurrentRoomInfo().bIsInRoom)
    {
        AppliedAuditoriumSpawns.Reset();
        return;
    }

    for (auto It = AppliedAuditoriumSpawns.CreateIterator(); It; ++It)
    {
        if (!(*It).IsValid())
        {
            It.RemoveCurrent();
        }
    }

    const TArray<FTransform> SpawnTransforms = CollectAuditoriumSpawnTransforms();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PlayerController = It->Get();
        APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
        if (!PlayerPawn || AppliedAuditoriumSpawns.Contains(PlayerPawn))
        {
            continue;
        }

        const int32 SpawnIndex = ResolveSpawnIndexForPawn(PlayerPawn);
        const FTransform SpawnTransform = BuildAuditoriumSpawnTransform(PlayerPawn, SpawnIndex, SpawnTransforms);
        PlayerPawn->SetActorLocationAndRotation(SpawnTransform.GetLocation(), SpawnTransform.Rotator());
        AppliedAuditoriumSpawns.Add(PlayerPawn);

        UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Authority assigned auditorium spawn %d to pawn %s at %s."),
            SpawnIndex,
            *PlayerPawn->GetName(),
            *SpawnTransform.GetLocation().ToCompactString());
    }
}

void AKemClassroomBootstrapActor::TeleportLocalPawnToTaggedSpawnIfNeeded()
{
    if (bLocalAuditoriumSpawnApplied || !GetWorld())
    {
        return;
    }

    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    UKemRoomSubsystem* RoomSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKemRoomSubsystem>() : nullptr;
    if (!PlayerController || !PlayerPawn || !RoomSubsystem || !RoomSubsystem->GetCurrentRoomInfo().bIsInRoom)
    {
        return;
    }

    const int32 SpawnIndex = ResolveSpawnIndexForPawn(PlayerPawn);
    const FTransform SpawnTransform = BuildAuditoriumSpawnTransform(PlayerPawn, SpawnIndex, CollectAuditoriumSpawnTransforms());
    PlayerPawn->SetActorLocationAndRotation(SpawnTransform.GetLocation(), SpawnTransform.Rotator());
    bLocalAuditoriumSpawnApplied = true;

    UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Local pawn aligned to auditorium spawn %d at %s."),
        SpawnIndex,
        *SpawnTransform.GetLocation().ToCompactString());
}

void AKemClassroomBootstrapActor::EnsureLocalLayoutInitialized()
{
    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    UKemRoomSubsystem* RoomSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UKemRoomSubsystem>() : nullptr;
    if (!PlayerController || !PlayerPawn || !RoomSubsystem || !RoomSubsystem->GetCurrentRoomInfo().bIsInRoom)
    {
        return;
    }

    const bool bShouldRefreshLayout = !bLocalLayoutInitialized || LastLayoutPawn.Get() != PlayerPawn;
    if (!bShouldRefreshLayout)
    {
        return;
    }

    const FVector LayoutOrigin = PlayerPawn->GetActorLocation();
    const FRotator LayoutRotation(0.0f, PlayerController->GetControlRotation().Yaw, 0.0f);

    SpawnIfMissing<AKemRoomCodeDisplayActor>(BuildLayoutTransform(LayoutOrigin, LayoutRotation, RoomCodeOffset));
    SpawnIfMissing<AKemTeacherControlPanelActor>(BuildLayoutTransform(LayoutOrigin, LayoutRotation, TeacherPanelOffset));
    SpawnIfMissing<AKemBrowserDeskActor>(BuildLayoutTransform(LayoutOrigin, LayoutRotation, BrowserDeskOffset));
    SpawnIfMissing<AKemSharedLectureScreenActor>(BuildLayoutTransform(LayoutOrigin, LayoutRotation, SharedScreenOffset, 180.0f));
    SpawnIfMissing<AKemHostModeratorManagerActor>(GetActorTransform());

    bLocalLayoutInitialized = true;
    LastLayoutPawn = PlayerPawn;
    UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Local layout refreshed around pawn %s at %s."),
        *PlayerPawn->GetName(),
        *LayoutOrigin.ToCompactString());
}

void AKemClassroomBootstrapActor::EnsurePlayerControlProxies()
{
    if (!HasAuthority() || !GetWorld())
    {
        return;
    }

    for (auto It = PlayerControlProxies.CreateIterator(); It; ++It)
    {
        if (!IsValid(It->Get()))
        {
            It.RemoveCurrent();
        }
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PlayerController = It->Get();
        if (!PlayerController)
        {
            continue;
        }

        const bool bHasProxy = PlayerControlProxies.ContainsByPredicate([PlayerController](const AKemPlayerControlProxy* Proxy)
        {
            return Proxy && Proxy->GetOwner() == PlayerController;
        });

        if (bHasProxy)
        {
            continue;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = PlayerController;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        if (AKemPlayerControlProxy* Proxy = GetWorld()->SpawnActor<AKemPlayerControlProxy>(SpawnParameters))
        {
            PlayerControlProxies.Add(Proxy);
            Proxy->ClientEvaluateLocalControlMode();
        }
    }
}

void AKemClassroomBootstrapActor::EnsureLocalPlayerControlMode()
{
    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerController || !PlayerPawn || PlayerPawn->IsA<AKemDesktopCharacter>() || !ShouldUseDesktopFallback())
    {
        return;
    }

    AKemPlayerControlProxy* LocalProxy = nullptr;
    for (const TObjectPtr<AKemPlayerControlProxy>& Proxy : PlayerControlProxies)
    {
        if (Proxy && Proxy->GetOwner() == PlayerController)
        {
            LocalProxy = Proxy;
            break;
        }
    }

    if (LocalProxy)
    {
        if (HasAuthority())
        {
            LocalProxy->ActivateDesktopModeForOwner();
        }
        else
        {
            LocalProxy->ServerRequestDesktopMode();
        }
    }
}

void AKemClassroomBootstrapActor::EnsureLocalVRWidgetInteractor()
{
    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerPawn || !PlayerPawn->IsLocallyControlled() || PlayerPawn->IsA<AKemDesktopCharacter>())
    {
        LastVRInteractorPawn = nullptr;
        return;
    }

    if (LastVRInteractorPawn.Get() == PlayerPawn && PlayerPawn->FindComponentByClass<UKemVRWidgetInteractorComponent>())
    {
        return;
    }

    if (!PlayerPawn->FindComponentByClass<UKemVRWidgetInteractorComponent>())
    {
        UKemVRWidgetInteractorComponent* VRWidgetInteractor = NewObject<UKemVRWidgetInteractorComponent>(PlayerPawn, TEXT("KemVRWidgetInteractor"));
        if (VRWidgetInteractor)
        {
            PlayerPawn->AddInstanceComponent(VRWidgetInteractor);
            VRWidgetInteractor->RegisterComponent();
            UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Attached VR widget interactor to pawn %s."), *PlayerPawn->GetName());
        }
    }

    LastVRInteractorPawn = PlayerPawn;
}

void AKemClassroomBootstrapActor::EnsureLocalDrawingBoardInteractor()
{
    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerPawn || !PlayerPawn->IsLocallyControlled())
    {
        LastDrawingBoardInteractorPawn = nullptr;
        return;
    }

    if (LastDrawingBoardInteractorPawn.Get() == PlayerPawn && PlayerPawn->FindComponentByClass<UKemDrawingBoardInteractionComponent>())
    {
        return;
    }

    if (!PlayerPawn->FindComponentByClass<UKemDrawingBoardInteractionComponent>())
    {
        UKemDrawingBoardInteractionComponent* DrawingBoardInteractor = NewObject<UKemDrawingBoardInteractionComponent>(PlayerPawn, TEXT("KemDrawingBoardInteractor"));
        if (DrawingBoardInteractor)
        {
            PlayerPawn->AddInstanceComponent(DrawingBoardInteractor);
            DrawingBoardInteractor->RegisterComponent();
            UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Attached drawing board interactor to pawn %s."), *PlayerPawn->GetName());
        }
    }

    LastDrawingBoardInteractorPawn = PlayerPawn;
}

void AKemClassroomBootstrapActor::EnsureLocalVRGrabInteractor()
{
    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerPawn || !PlayerPawn->IsLocallyControlled() || PlayerPawn->IsA<AKemDesktopCharacter>())
    {
        LastVRGrabInteractorPawn = nullptr;
        return;
    }

    if (LastVRGrabInteractorPawn.Get() == PlayerPawn && PlayerPawn->FindComponentByClass<UKemVRGrabInteractionComponent>())
    {
        return;
    }

    if (!PlayerPawn->FindComponentByClass<UKemVRGrabInteractionComponent>())
    {
        UKemVRGrabInteractionComponent* VRGrabInteractor = NewObject<UKemVRGrabInteractionComponent>(PlayerPawn, TEXT("KemVRGrabInteractor"));
        if (VRGrabInteractor)
        {
            PlayerPawn->AddInstanceComponent(VRGrabInteractor);
            VRGrabInteractor->RegisterComponent();
            UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Attached VR grab interactor to pawn %s."), *PlayerPawn->GetName());
        }
    }

    LastVRGrabInteractorPawn = PlayerPawn;
}

bool AKemClassroomBootstrapActor::ShouldUseDesktopFallback() const
{
    if (!GEngine || !GEngine->XRSystem.IsValid())
    {
        return true;
    }

    const bool bStereoRenderingActive = GEngine->IsStereoscopic3D();
    const bool bTrackedHMDPresent = GEngine->XRSystem->CountTrackedDevices(EXRTrackedDeviceType::HeadMountedDisplay) > 0;
    return !(bStereoRenderingActive && bTrackedHMDPresent);
}

int32 AKemClassroomBootstrapActor::ResolveSpawnIndexForPawn(const APawn* Pawn) const
{
    if (!Pawn || !GetWorld())
    {
        return 0;
    }

    const AGameStateBase* GameState = GetWorld()->GetGameState();
    const APlayerState* TargetPlayerState = Pawn->GetPlayerState();
    if (!GameState || !TargetPlayerState)
    {
        return 0;
    }

    TArray<APlayerState*> OrderedPlayerStates = GameState->PlayerArray;
    Algo::SortBy(OrderedPlayerStates, [](const APlayerState* PlayerState)
    {
        return PlayerState ? PlayerState->GetPlayerId() : MAX_int32;
    });

    int32 SpawnIndex = 0;
    for (const APlayerState* PlayerState : OrderedPlayerStates)
    {
        if (!PlayerState || !PlayerState->GetPawn())
        {
            continue;
        }

        if (PlayerState == TargetPlayerState)
        {
            return SpawnIndex;
        }

        ++SpawnIndex;
    }

    return 0;
}

TArray<FTransform> AKemClassroomBootstrapActor::CollectAuditoriumSpawnTransforms() const
{
    TArray<FTransform> SpawnTransforms;
    if (!GetWorld())
    {
        return SpawnTransforms;
    }

    TArray<AActor*> TaggedSpawnActors;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Candidate = *It;
        if (Candidate && Candidate->ActorHasTag(AuditoriumSpawnTag))
        {
            TaggedSpawnActors.Add(Candidate);
        }
    }

    Algo::SortBy(TaggedSpawnActors, [](const AActor* SpawnActor)
    {
        return SpawnActor ? SpawnActor->GetName() : FString();
    });

    for (const AActor* SpawnActor : TaggedSpawnActors)
    {
        SpawnTransforms.Add(SpawnActor->GetActorTransform());
    }

    if (SpawnTransforms.Num() > 0)
    {
        return SpawnTransforms;
    }

    TArray<APlayerStart*> PlayerStarts;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        if (APlayerStart* PlayerStart = *It)
        {
            PlayerStarts.Add(PlayerStart);
        }
    }

    Algo::SortBy(PlayerStarts, [](const APlayerStart* PlayerStart)
    {
        return PlayerStart ? PlayerStart->GetName() : FString();
    });

    for (const APlayerStart* PlayerStart : PlayerStarts)
    {
        SpawnTransforms.Add(PlayerStart->GetActorTransform());
    }

    if (SpawnTransforms.Num() == 0)
    {
        SpawnTransforms.Add(GetActorTransform());
    }

    return SpawnTransforms;
}

FTransform AKemClassroomBootstrapActor::BuildAuditoriumSpawnTransform(const APawn* Pawn, int32 SpawnIndex, const TArray<FTransform>& SpawnTransforms) const
{
    FTransform MapSpecificTransform;
    if (TryBuildMapSpecificAuditoriumSpawnTransform(Pawn, SpawnIndex, MapSpecificTransform))
    {
        return MapSpecificTransform;
    }

    if (SpawnTransforms.Num() == 0)
    {
        return GetActorTransform();
    }

    const int32 SafeSpawnIndex = FMath::Max(0, SpawnIndex);
    const FTransform& BaseTransform = SpawnTransforms[FMath::Min(SafeSpawnIndex, SpawnTransforms.Num() - 1)];

    if (SafeSpawnIndex < SpawnTransforms.Num())
    {
        FTransform AdjustedTransform = BaseTransform;
        if (Pawn && !Pawn->IsA<AKemDesktopCharacter>())
        {
            AdjustedTransform.AddToTranslation(VRAuditoriumSpawnOffset);
        }
        return AdjustedTransform;
    }

    const int32 OverflowIndex = SafeSpawnIndex - SpawnTransforms.Num() + 1;
    const int32 PairIndex = (OverflowIndex + 1) / 2;
    const float Direction = (OverflowIndex % 2 == 1) ? 1.0f : -1.0f;
    const float LateralOffset = 140.0f * PairIndex * Direction;
    const FVector Offset = BaseTransform.GetRotation().GetRightVector() * LateralOffset;

    FTransform AdjustedTransform = BaseTransform;
    AdjustedTransform.AddToTranslation(Offset);
    if (Pawn && !Pawn->IsA<AKemDesktopCharacter>())
    {
        AdjustedTransform.AddToTranslation(VRAuditoriumSpawnOffset);
    }
    return AdjustedTransform;
}

bool AKemClassroomBootstrapActor::TryBuildMapSpecificAuditoriumSpawnTransform(const APawn* Pawn, int32 SpawnIndex, FTransform& OutTransform) const
{
    if (!Pawn || Pawn->IsA<AKemDesktopCharacter>() || !GetWorld())
    {
        return false;
    }

    const FString TargetMapName = FixedVRSpawnMapName.ToString();
    if (!IsCurrentMapNamed(GetWorld(), TargetMapName))
    {
        return false;
    }

    const FString ShortMapName = TargetMapName;
    FTransform BaseTransform(FixedVRSpawnRotation, FixedVRSpawnLocation, FVector::OneVector);
    if (SpawnIndex <= 0)
    {
        OutTransform = BaseTransform;
        UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Using fixed VR spawn on map %s for pawn %s at %s."),
            *ShortMapName,
            *Pawn->GetName(),
            *OutTransform.GetLocation().ToCompactString());
        return true;
    }

    const int32 PairIndex = (SpawnIndex + 1) / 2;
    const float Direction = (SpawnIndex % 2 == 1) ? 1.0f : -1.0f;
    const float LateralOffset = 140.0f * PairIndex * Direction;
    BaseTransform.AddToTranslation(BaseTransform.GetRotation().GetRightVector() * LateralOffset);
    OutTransform = BaseTransform;
    UE_LOG(LogTemp, Log, TEXT("KemSUinVR: Using fixed VR spawn with offset on map %s for pawn %s at %s."),
        *ShortMapName,
        *Pawn->GetName(),
        *OutTransform.GetLocation().ToCompactString());
    return true;
}

FTransform AKemClassroomBootstrapActor::BuildLayoutTransform(const FVector& LayoutOrigin, const FRotator& LayoutRotation, const FVector& RelativeOffset, float AdditionalYaw) const
{
    const FTransform LayoutTransform(LayoutRotation, LayoutOrigin, FVector::OneVector);
    const FVector WorldLocation = LayoutTransform.TransformPosition(RelativeOffset);
    FRotator WorldRotation = LayoutRotation;
    WorldRotation.Yaw += AdditionalYaw;
    return FTransform(WorldRotation, WorldLocation, FVector::OneVector);
}

template <typename TActorClass>
TActorClass* AKemClassroomBootstrapActor::SpawnIfMissing(const FTransform& SpawnTransform)
{
    for (TActorIterator<TActorClass> It(GetWorld()); It; ++It)
    {
        return *It;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    return GetWorld()->SpawnActor<TActorClass>(TActorClass::StaticClass(), SpawnTransform, SpawnParameters);
}
