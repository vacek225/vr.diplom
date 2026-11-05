#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/KemTypes.h"
#include "KemWorldActors.generated.h"

class UKemParticipantModeratorWidget;
class AKemDrawingBoardActor;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class AKemPlayerControlProxy;
class AKemLocalLectureScreenActor;
class APlayerController;
class APawn;
class UKemVRWidgetInteractorComponent;
class UKemVRGrabInteractionComponent;

UENUM(BlueprintType)
enum class EKemScreenHandleType : uint8
{
    None,
    Resize,
    Move,
    Close,
    Copy
};

UCLASS(Abstract)
class KEMSUINVR_API AKemWidgetPanelActorBase : public AActor
{
    GENERATED_BODY()

public:
    AKemWidgetPanelActorBase();
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR")
    TObjectPtr<UWidgetComponent> WidgetComponent;

protected:
    virtual void BeginPlay() override;
    void FaceFirstPlayer();

    UPROPERTY(EditAnywhere, Category = "KemSUinVR")
    bool bFaceLocalPlayer = false;
};

UCLASS()
class KEMSUINVR_API AKemHubActor : public AKemWidgetPanelActorBase
{
    GENERATED_BODY()

public:
    AKemHubActor();
    virtual void Tick(float DeltaSeconds) override;
};

UCLASS()
class KEMSUINVR_API AKemRoomCodeDisplayActor : public AKemWidgetPanelActorBase
{
    GENERATED_BODY()

public:
    AKemRoomCodeDisplayActor();
    virtual void Tick(float DeltaSeconds) override;
};

UCLASS()
class KEMSUINVR_API AKemReturnToLobbyActor : public AKemWidgetPanelActorBase
{
    GENERATED_BODY()

public:
    AKemReturnToLobbyActor();
};

UCLASS()
class KEMSUINVR_API AKemTeacherControlPanelActor : public AKemWidgetPanelActorBase
{
    GENERATED_BODY()

public:
    AKemTeacherControlPanelActor();
    virtual void Tick(float DeltaSeconds) override;
};

UCLASS()
class KEMSUINVR_API AKemDrawingBoardControlPanelActor : public AKemWidgetPanelActorBase
{
    GENERATED_BODY()

public:
    AKemDrawingBoardControlPanelActor();

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    void BindNearestBoard();

private:
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "KemSUinVR|Drawing Board", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<AKemDrawingBoardActor> TargetBoard;
};

UCLASS()
class KEMSUINVR_API AKemBrowserDeskActor : public AKemWidgetPanelActorBase
{
    GENERATED_BODY()

public:
    AKemBrowserDeskActor();
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Desk", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> DeskScreenGroupRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Desk", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> DeskScreenBackdropVisual;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Desk", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> DeskKeyboardGroupRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Desk", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> DeskKeyboardVisual;
};

UCLASS()
class KEMSUINVR_API AKemSharedLectureScreenActor : public AKemWidgetPanelActorBase
{
    GENERATED_BODY()

public:
    AKemSharedLectureScreenActor();
    bool IsCopyHandle(const UPrimitiveComponent* Component) const;
    AKemLocalLectureScreenActor* SpawnLocalCopyForController(APlayerController* PlayerController) const;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> ScreenGroupRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ScreenBackdropVisual;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> ControlsGroupRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> CopyHandleVisual;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> CopyHandleTrigger;
};

UCLASS()
class KEMSUINVR_API AKemLocalLectureScreenActor : public AKemWidgetPanelActorBase
{
    GENERATED_BODY()

public:
    AKemLocalLectureScreenActor();
    EKemScreenHandleType ResolveHandleType(const UPrimitiveComponent* Component) const;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> ScreenGroupRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ScreenBackdropVisual;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> ControlsGroupRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ResizeHandleVisual;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> MoveHandleVisual;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> CloseHandleVisual;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> ResizeHandleTrigger;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> MoveHandleTrigger;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Screen", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> CloseHandleTrigger;
};

UCLASS()
class KEMSUINVR_API AKemParticipantModeratorActor : public AKemWidgetPanelActorBase
{
    GENERATED_BODY()

public:
    AKemParticipantModeratorActor();
    virtual void Tick(float DeltaSeconds) override;

    void BindToParticipant(APawn* TargetPawn, const FKemParticipantInfo& ParticipantInfo);
    bool IsBoundToPawn(const APawn* Pawn) const;

private:
    TWeakObjectPtr<APawn> ObservedPawn;
    FKemParticipantInfo CachedParticipantInfo;
};

UCLASS()
class KEMSUINVR_API AKemHostModeratorManagerActor : public AActor
{
    GENERATED_BODY()

public:
    AKemHostModeratorManagerActor();
    virtual void Tick(float DeltaSeconds) override;

private:
    TMap<TWeakObjectPtr<APawn>, TObjectPtr<AKemParticipantModeratorActor>> ModeratorPanels;
    float RefreshAccumulator = 0.0f;
};

UCLASS()
class KEMSUINVR_API AKemClassroomBootstrapActor : public AActor
{
    GENERATED_BODY()

public:
    AKemClassroomBootstrapActor();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    void EnsureLocalLayoutInitialized();
    void TeleportLocalPawnToTaggedSpawnIfNeeded();
    void EnsureAuditoriumSpawnsAssigned();
    void EnsurePlayerControlProxies();
    void EnsureLocalPlayerControlMode();
    void EnsureLocalVRWidgetInteractor();
    void EnsureLocalDrawingBoardInteractor();
    void EnsureLocalVRGrabInteractor();
    bool ShouldUseDesktopFallback() const;
    int32 ResolveSpawnIndexForPawn(const APawn* Pawn) const;
    TArray<FTransform> CollectAuditoriumSpawnTransforms() const;
    bool TryBuildMapSpecificAuditoriumSpawnTransform(const APawn* Pawn, int32 SpawnIndex, FTransform& OutTransform) const;
    FTransform BuildAuditoriumSpawnTransform(const APawn* Pawn, int32 SpawnIndex, const TArray<FTransform>& SpawnTransforms) const;
    FTransform BuildLayoutTransform(const FVector& LayoutOrigin, const FRotator& LayoutRotation, const FVector& RelativeOffset, float AdditionalYaw = 0.0f) const;

    template <typename TActorClass>
    TActorClass* SpawnIfMissing(const FTransform& SpawnTransform);

private:
    UPROPERTY(EditAnywhere, Category = "KemSUinVR")
    FVector HubOffset = FVector(180.0f, 0.0f, 120.0f);

    UPROPERTY(EditAnywhere, Category = "KemSUinVR")
    FVector RoomCodeOffset = FVector(220.0f, -120.0f, 175.0f);

    UPROPERTY(EditAnywhere, Category = "KemSUinVR")
    FVector TeacherPanelOffset = FVector(210.0f, 130.0f, 110.0f);

    UPROPERTY(EditAnywhere, Category = "KemSUinVR")
    FVector BrowserDeskOffset = FVector(210.0f, -140.0f, 105.0f);

    UPROPERTY(EditAnywhere, Category = "KemSUinVR")
    FVector SharedScreenOffset = FVector(460.0f, 0.0f, 190.0f);

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Auditorium")
    FName AuditoriumSpawnTag = TEXT("Kem_Auditorium_Spawn");

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Auditorium")
    FVector VRAuditoriumSpawnOffset = FVector(0.0f, 0.0f, 140.0f);

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Auditorium")
    FName FixedVRSpawnMapName = TEXT("3_Block");

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Auditorium")
    FVector FixedVRSpawnLocation = FVector(1072.0f, 2062.0f, -40.0f);

    UPROPERTY(EditAnywhere, Category = "KemSUinVR|Auditorium")
    FRotator FixedVRSpawnRotation = FRotator::ZeroRotator;

    bool bLocalLayoutInitialized = false;
    bool bLocalAuditoriumSpawnApplied = false;
    float ProxyRefreshAccumulator = 0.0f;
    TWeakObjectPtr<APawn> LastLayoutPawn;
    TWeakObjectPtr<APawn> LastVRInteractorPawn;
    TWeakObjectPtr<APawn> LastDrawingBoardInteractorPawn;
    TWeakObjectPtr<APawn> LastVRGrabInteractorPawn;
    TSet<TWeakObjectPtr<APawn>> AppliedAuditoriumSpawns;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AKemPlayerControlProxy>> PlayerControlProxies;
};
