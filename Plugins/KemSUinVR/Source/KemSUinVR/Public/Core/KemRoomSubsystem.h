#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/KemTypes.h"
#include "KemRoomSubsystem.generated.h"

class AKemClassroomStateActor;
class UNetDriver;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKemRoomInfoChangedSignature, const FKemRoomInfo&, RoomInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKemRoomOperationResultSignature, bool, bSuccess, const FString&, Message);

UCLASS()
class KEMSUINVR_API UKemRoomSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UPROPERTY(BlueprintAssignable, Category = "KemSUinVR")
    FKemRoomInfoChangedSignature OnRoomInfoChanged;

    UPROPERTY(BlueprintAssignable, Category = "KemSUinVR")
    FKemRoomOperationResultSignature OnJoinOperationFinished;

    UPROPERTY(BlueprintAssignable, Category = "KemSUinVR")
    FKemRoomOperationResultSignature OnHostOperationFinished;

    UFUNCTION(BlueprintPure, Category = "KemSUinVR")
    TArray<FKemAuditoriumDefinition> GetAuditoriums() const;

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR")
    void HostAuditorium(FName AuditoriumId);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR")
    void JoinRoomByCode(const FString& RoomCode);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR")
    void LeaveCurrentRoom();

    UFUNCTION(BlueprintPure, Category = "KemSUinVR")
    FKemRoomInfo GetCurrentRoomInfo() const { return CurrentRoomInfo; }

    UFUNCTION(BlueprintPure, Category = "KemSUinVR")
    bool IsHost() const { return CurrentRoomInfo.bIsHost && CurrentRoomInfo.bIsInRoom; }

    UFUNCTION(BlueprintPure, Category = "KemSUinVR")
    FString GetCurrentRoomCode() const { return CurrentRoomInfo.RoomCode; }

    UFUNCTION(BlueprintPure, Category = "KemSUinVR")
    AKemClassroomStateActor* GetCurrentClassroomState() const;

    void NotifyClassroomStateReady(AKemClassroomStateActor* InStateActor);

private:
    static const FName SessionName;
    static const FName RoomCodeKey;
    static const FName AuditoriumKey;
    static const FName HostEndpointKey;

    void StartHostingAuditorium(const FKemAuditoriumDefinition& Auditorium);
    FString GenerateRoomCode(int32 CodeLength) const;
    FString SanitizeRoomCode(const FString& RoomCode) const;
    FString BuildTravelUrl(const FString& ResolvedConnectString, const FString& AdvertisedHostEndpoint) const;
    FString NormalizeEndpoint(const FString& Endpoint) const;
    FString ResolveAdvertisedHostEndpoint() const;
    int32 ScoreHostAddress(const FString& Address) const;
    void BroadcastRoomInfo();
    void BootstrapRoomAfterTravel(UWorld* World);
    void EnsureVoiceEnabledForLocalPlayer(UWorld* World) const;
    void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
    void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
    void ResetPendingJoinAttempt();

    void HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful);
    void HandleFindSessionsComplete(bool bWasSuccessful);
    void HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result);
    void HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful);

    void ClearSessionDelegates();

private:
    // Isolated PIE fixture can model room entry without launching LAN travel.
    friend class FKemLobbyObjectsCommand;
    FKemRoomInfo CurrentRoomInfo;
    FKemRoomInfo PendingHostedRoomInfo;
    FString PendingJoinCode;
    FString PendingJoinHostEndpoint;
    FString PendingJoinMapPath;
    FName PendingJoinAuditoriumId = NAME_None;
    bool bAwaitingClientTravel = false;
    bool bAwaitingTravelBootstrap = false;
    bool bOperationInFlight = false;

    TSharedPtr<class FOnlineSessionSearch> ActiveSessionSearch;

    FDelegateHandle CreateSessionHandle;
    FDelegateHandle FindSessionsHandle;
    FDelegateHandle JoinSessionHandle;
    FDelegateHandle DestroySessionHandle;
    FDelegateHandle PostWorldInitHandle;
    FDelegateHandle TravelFailureHandle;
    FDelegateHandle NetworkFailureHandle;

    TWeakObjectPtr<AKemClassroomStateActor> CachedClassroomState;
};
