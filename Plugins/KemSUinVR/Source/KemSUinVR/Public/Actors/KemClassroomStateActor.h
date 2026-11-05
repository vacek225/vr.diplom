#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/KemTypes.h"
#include "KemClassroomStateActor.generated.h"

class AKemVoiceControlProxy;

UCLASS()
class KEMSUINVR_API AKemClassroomStateActor : public AActor
{
    GENERATED_BODY()

public:
    AKemClassroomStateActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void InitializeHostedRoom(const FKemRoomInfo& InRoomInfo, const FString& InDefaultLectureUrl);

    UFUNCTION(BlueprintPure, Category = "KemSUinVR")
    FString GetRoomCode() const { return RoomCode; }

    UFUNCTION(BlueprintPure, Category = "KemSUinVR")
    FString GetLectureUrl() const { return LectureUrl; }

    UFUNCTION(BlueprintPure, Category = "KemSUinVR")
    const TArray<FKemParticipantInfo>& GetParticipants() const { return Participants; }

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR")
    bool SetLectureUrl(const FString& NewUrl);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR")
    bool SetParticipantVolume(const FString& ParticipantId, float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR")
    bool SetParticipantMuted(const FString& ParticipantId, bool bMuted);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR")
    bool KickParticipant(const FString& ParticipantId);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR")
    bool BanParticipant(const FString& ParticipantId);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR")
    AActor* SpawnPresentationPrimitive(EKemPresentationPrimitive PrimitiveType, const FTransform& SpawnTransform);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR|Engine")
    class AKemEngineAssembly* SpawnEngine2112(const FTransform& SpawnTransform);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR")
    void ClearSpawnedPresentationActors();
    bool OwnsPresentationActor(const AActor* Object) const;
    bool RemovePresentationActor(AActor* Object);

protected:
    UFUNCTION()
    void OnRep_RoomCode();

    UFUNCTION()
    void OnRep_LectureUrl();

    UFUNCTION()
    void OnRep_Participants();

private:
    void RefreshParticipants();
    void EnsureVoiceProxies();
    bool UpdateParticipantRecord(const FString& ParticipantId, TFunctionRef<void(FKemParticipantInfo&)> Mutator);
    FString ResolveParticipantId(APlayerState* PlayerState) const;
    APlayerController* FindPlayerControllerByParticipantId(const FString& ParticipantId) const;
    FKemParticipantInfo* FindParticipantMutable(const FString& ParticipantId);
    void ApplyBanList();

private:
    UPROPERTY(ReplicatedUsing=OnRep_RoomCode)
    FString RoomCode;

    UPROPERTY(ReplicatedUsing=OnRep_LectureUrl)
    FString LectureUrl;

    UPROPERTY(ReplicatedUsing=OnRep_Participants)
    TArray<FKemParticipantInfo> Participants;

    UPROPERTY(Replicated)
    TArray<FString> BannedParticipantIds;

    UPROPERTY(Replicated)
    bool bRoomLocked = false;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> SpawnedPresentationActors;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AKemVoiceControlProxy>> VoiceProxies;

    float ParticipantRefreshAccumulator = 0.0f;
};
