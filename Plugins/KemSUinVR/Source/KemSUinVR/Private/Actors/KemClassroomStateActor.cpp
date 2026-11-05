#include "Actors/KemClassroomStateActor.h"

#include "Actors/KemVoiceControlProxy.h"
#include "Core/KemRoomSubsystem.h"
#include "Core/KemSettings.h"
#include "GameFramework/PlayerState.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/KemEngineAssembly.h"

AKemClassroomStateActor::AKemClassroomStateActor()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = true;
    SetReplicateMovement(false);
}

void AKemClassroomStateActor::BeginPlay()
{
    Super::BeginPlay();

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UKemRoomSubsystem* RoomSubsystem = GameInstance->GetSubsystem<UKemRoomSubsystem>())
        {
            RoomSubsystem->NotifyClassroomStateReady(this);
        }
    }
}

void AKemClassroomStateActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority())
    {
        return;
    }

    ParticipantRefreshAccumulator += DeltaSeconds;
    if (ParticipantRefreshAccumulator >= 0.5f)
    {
        ParticipantRefreshAccumulator = 0.0f;
        RefreshParticipants();
        EnsureVoiceProxies();
        ApplyBanList();
    }
}

void AKemClassroomStateActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AKemClassroomStateActor, RoomCode);
    DOREPLIFETIME(AKemClassroomStateActor, LectureUrl);
    DOREPLIFETIME(AKemClassroomStateActor, Participants);
    DOREPLIFETIME(AKemClassroomStateActor, BannedParticipantIds);
    DOREPLIFETIME(AKemClassroomStateActor, bRoomLocked);
}

void AKemClassroomStateActor::InitializeHostedRoom(const FKemRoomInfo& InRoomInfo, const FString& InDefaultLectureUrl)
{
    if (!HasAuthority())
    {
        return;
    }

    RoomCode = InRoomInfo.RoomCode;
    LectureUrl = InDefaultLectureUrl;
    ForceNetUpdate();
}

bool AKemClassroomStateActor::SetLectureUrl(const FString& NewUrl)
{
    if (!HasAuthority())
    {
        return false;
    }

    LectureUrl = NewUrl;
    ForceNetUpdate();
    return true;
}

bool AKemClassroomStateActor::SetParticipantVolume(const FString& ParticipantId, float NewVolume)
{
    if (!HasAuthority())
    {
        return false;
    }

    return UpdateParticipantRecord(ParticipantId, [NewVolume](FKemParticipantInfo& Participant)
    {
        Participant.VolumeMultiplier = FMath::Clamp(NewVolume, 0.0f, 2.0f);
    });
}

bool AKemClassroomStateActor::SetParticipantMuted(const FString& ParticipantId, bool bMuted)
{
    if (!HasAuthority())
    {
        return false;
    }

    const bool bUpdated = UpdateParticipantRecord(ParticipantId, [bMuted](FKemParticipantInfo& Participant)
    {
        Participant.bForcedMuted = bMuted;
    });

    if (bUpdated)
    {
        if (APlayerController* PlayerController = FindPlayerControllerByParticipantId(ParticipantId))
        {
            for (AKemVoiceControlProxy* Proxy : VoiceProxies)
            {
                if (Proxy && Proxy->GetOwner() == PlayerController)
                {
                    Proxy->ClientSetVoiceTransmitEnabled(!bMuted);
                    break;
                }
            }
        }
    }

    return bUpdated;
}

bool AKemClassroomStateActor::KickParticipant(const FString& ParticipantId)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (APlayerController* PlayerController = FindPlayerControllerByParticipantId(ParticipantId))
    {
        PlayerController->ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("Removed from the auditorium by the host.")));
        return true;
    }

    return false;
}

bool AKemClassroomStateActor::BanParticipant(const FString& ParticipantId)
{
    if (!HasAuthority())
    {
        return false;
    }

    BannedParticipantIds.AddUnique(ParticipantId);
    const bool bMuted = SetParticipantMuted(ParticipantId, true);
    const bool bKicked = KickParticipant(ParticipantId);
    return bMuted || bKicked;
}

AActor* AKemClassroomStateActor::SpawnPresentationPrimitive(EKemPresentationPrimitive PrimitiveType, const FTransform& SpawnTransform)
{
    if (!HasAuthority())
    {
        return nullptr;
    }

    const TCHAR* AssetPath = TEXT("/Engine/BasicShapes/Cube.Cube");
    switch (PrimitiveType)
    {
    case EKemPresentationPrimitive::Cylinder:
        AssetPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
        break;
    case EKemPresentationPrimitive::Sphere:
        AssetPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
        break;
    default:
        break;
    }

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, AssetPath);
    if (!Mesh)
    {
        return nullptr;
    }

    AStaticMeshActor* MeshActor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnTransform);
    if (!MeshActor)
    {
        return nullptr;
    }

    MeshActor->SetReplicates(true);
    MeshActor->SetReplicateMovement(true);
    MeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
    MeshActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    MeshActor->GetStaticMeshComponent()->SetSimulatePhysics(false);
    SpawnedPresentationActors.Add(MeshActor);
    return MeshActor;
}

void AKemClassroomStateActor::ClearSpawnedPresentationActors()
{
    if (!HasAuthority())
    {
        return;
    }

    for (AActor* Actor : SpawnedPresentationActors)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }

    SpawnedPresentationActors.Reset();
}

AKemEngineAssembly* AKemClassroomStateActor::SpawnEngine2112(const FTransform& SpawnTransform)
{
    if (!HasAuthority()) return nullptr;
    // Keep one shared training stand per room.
    for (AActor* Existing : SpawnedPresentationActors)
        if (AKemEngineAssembly* Engine=Cast<AKemEngineAssembly>(Existing); IsValid(Engine)) return Engine;
    AKemEngineAssembly* Engine=GetWorld()->SpawnActor<AKemEngineAssembly>(AKemEngineAssembly::StaticClass(),SpawnTransform);
    if (Engine) SpawnedPresentationActors.Add(Engine);
    return Engine;
}

void AKemClassroomStateActor::OnRep_RoomCode()
{
}

void AKemClassroomStateActor::OnRep_LectureUrl()
{
}

void AKemClassroomStateActor::OnRep_Participants()
{
}

void AKemClassroomStateActor::RefreshParticipants()
{
    if (!HasAuthority())
    {
        return;
    }

    AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    if (!GameState)
    {
        return;
    }

    TArray<FKemParticipantInfo> NewParticipants;
    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        if (!PlayerState)
        {
            continue;
        }

        FKemParticipantInfo Participant;
        Participant.ParticipantId = ResolveParticipantId(PlayerState);
        Participant.DisplayName = PlayerState->GetPlayerName();

        if (const FKemParticipantInfo* Existing = Participants.FindByPredicate([&Participant](const FKemParticipantInfo& ExistingParticipant)
            {
                return ExistingParticipant.ParticipantId == Participant.ParticipantId;
            }))
        {
            Participant.VolumeMultiplier = Existing->VolumeMultiplier;
            Participant.bForcedMuted = Existing->bForcedMuted;
            Participant.bBanned = Existing->bBanned;
        }

        Participant.bBanned = BannedParticipantIds.Contains(Participant.ParticipantId);
        NewParticipants.Add(Participant);
    }

    Participants = MoveTemp(NewParticipants);
    ForceNetUpdate();
}

void AKemClassroomStateActor::EnsureVoiceProxies()
{
    if (!HasAuthority())
    {
        return;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PlayerController = It->Get();
        if (!PlayerController)
        {
            continue;
        }

        const bool bHasProxy = VoiceProxies.ContainsByPredicate([PlayerController](const AKemVoiceControlProxy* Proxy)
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

        AKemVoiceControlProxy* Proxy = GetWorld()->SpawnActor<AKemVoiceControlProxy>(SpawnParameters);
        if (Proxy)
        {
            VoiceProxies.Add(Proxy);
            Proxy->ClientBootstrapVoice();
        }
    }
}

bool AKemClassroomStateActor::UpdateParticipantRecord(const FString& ParticipantId, TFunctionRef<void(FKemParticipantInfo&)> Mutator)
{
    if (FKemParticipantInfo* Participant = FindParticipantMutable(ParticipantId))
    {
        Mutator(*Participant);
        ForceNetUpdate();
        return true;
    }

    return false;
}

FString AKemClassroomStateActor::ResolveParticipantId(APlayerState* PlayerState) const
{
    if (!PlayerState)
    {
        return TEXT("InvalidPlayer");
    }

    if (PlayerState->GetUniqueId().IsValid())
    {
        return PlayerState->GetUniqueId()->ToString();
    }

    return PlayerState->GetPlayerName();
}

APlayerController* AKemClassroomStateActor::FindPlayerControllerByParticipantId(const FString& ParticipantId) const
{
    if (!GetWorld())
    {
        return nullptr;
    }

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PlayerController = It->Get();
        if (!PlayerController || !PlayerController->PlayerState)
        {
            continue;
        }

        if (ResolveParticipantId(PlayerController->PlayerState) == ParticipantId)
        {
            return PlayerController;
        }
    }

    return nullptr;
}

FKemParticipantInfo* AKemClassroomStateActor::FindParticipantMutable(const FString& ParticipantId)
{
    return Participants.FindByPredicate([&ParticipantId](const FKemParticipantInfo& Participant)
    {
        return Participant.ParticipantId == ParticipantId;
    });
}

void AKemClassroomStateActor::ApplyBanList()
{
    if (!HasAuthority())
    {
        return;
    }

    for (const FString& ParticipantId : BannedParticipantIds)
    {
        if (APlayerController* PlayerController = FindPlayerControllerByParticipantId(ParticipantId))
        {
            PlayerController->ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("You are banned from this auditorium.")));
        }
    }
}

bool AKemClassroomStateActor::OwnsPresentationActor(const AActor* Object) const
{
    return IsValid(Object) && SpawnedPresentationActors.Contains(Object);
}
bool AKemClassroomStateActor::RemovePresentationActor(AActor* Object)
{
    if(!HasAuthority() || !OwnsPresentationActor(Object)) return false;
    if(auto* Engine=Cast<AKemEngineAssembly>(Object)) if(Engine->IsBusy()) return false;
    SpawnedPresentationActors.Remove(Object);
    return Object->Destroy();
}
