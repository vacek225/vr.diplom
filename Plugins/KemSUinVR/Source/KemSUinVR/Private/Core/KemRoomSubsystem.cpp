#include "Core/KemRoomSubsystem.h"
#include "UI/KemEscapeMenuSubsystem.h"

#include "Core/KemSettings.h"
#include "Actors/KemClassroomStateActor.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "SocketSubsystem.h"
#include "Misc/PackageName.h"

const FName UKemRoomSubsystem::SessionName(TEXT("KemSUinVRSession"));
const FName UKemRoomSubsystem::RoomCodeKey(TEXT("KEM_ROOM_CODE"));
const FName UKemRoomSubsystem::AuditoriumKey(TEXT("KEM_AUDITORIUM"));
const FName UKemRoomSubsystem::HostEndpointKey(TEXT("KEM_HOST_ENDPOINT"));

DEFINE_LOG_CATEGORY_STATIC(LogKemRoomSubsystem, Log, All);

namespace
{
FString NormalizeMapPath(const FString& RawMapPath)
{
    FString Normalized = RawMapPath;
    Normalized.TrimStartAndEndInline();

    if (Normalized.IsEmpty())
    {
        return FString(TEXT("/Game/VRTemplate/3_Block"));
    }

    int32 DotIndex = INDEX_NONE;
    if (Normalized.FindChar(TEXT('.'), DotIndex))
    {
        Normalized = Normalized.Left(DotIndex);
    }

    if (!Normalized.StartsWith(TEXT("/Game/")))
    {
        return FString(TEXT("/Game/VRTemplate/3_Block"));
    }

    return Normalized;
}
}

void UKemRoomSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    PostWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UKemRoomSubsystem::HandlePostWorldInitialization);

    if (GEngine)
    {
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UKemRoomSubsystem::HandleTravelFailure);
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UKemRoomSubsystem::HandleNetworkFailure);
    }
}

void UKemRoomSubsystem::Deinitialize()
{
    if (GEngine)
    {
        if (TravelFailureHandle.IsValid())
        {
            GEngine->OnTravelFailure().Remove(TravelFailureHandle);
            TravelFailureHandle.Reset();
        }

        if (NetworkFailureHandle.IsValid())
        {
            GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
            NetworkFailureHandle.Reset();
        }
    }

    if (PostWorldInitHandle.IsValid())
    {
        FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitHandle);
        PostWorldInitHandle.Reset();
    }

    ClearSessionDelegates();
    Super::Deinitialize();
}

TArray<FKemAuditoriumDefinition> UKemRoomSubsystem::GetAuditoriums() const
{
    return GetDefault<UKemSUinVRSettings>()->Auditoriums;
}

void UKemRoomSubsystem::HostAuditorium(FName AuditoriumId)
{
    if (bOperationInFlight)
    {
        OnHostOperationFinished.Broadcast(false, TEXT("Another room operation is already in progress."));
        return;
    }

    const UKemSUinVRSettings* Settings = GetDefault<UKemSUinVRSettings>();
    const FKemAuditoriumDefinition* Auditorium = Settings->FindAuditoriumById(AuditoriumId);
    if (!Auditorium)
    {
        OnHostOperationFinished.Broadcast(false, TEXT("Auditorium definition was not found."));
        return;
    }

    StartHostingAuditorium(*Auditorium);
}

void UKemRoomSubsystem::JoinRoomByCode(const FString& RoomCode)
{
    if (bOperationInFlight)
    {
        OnJoinOperationFinished.Broadcast(false, TEXT("Another room operation is already in progress."));
        return;
    }

    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    if (!OnlineSubsystem)
    {
        OnJoinOperationFinished.Broadcast(false, TEXT("No online subsystem is active. Check OnlineSubsystemNull settings."));
        return;
    }

    IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        OnJoinOperationFinished.Broadcast(false, TEXT("Session interface is not available."));
        return;
    }

    PendingJoinCode = SanitizeRoomCode(RoomCode);
    ActiveSessionSearch = MakeShared<FOnlineSessionSearch>();
    ActiveSessionSearch->bIsLanQuery = true;
    ActiveSessionSearch->MaxSearchResults = 128;
    ActiveSessionSearch->PingBucketSize = 50;

    bOperationInFlight = true;
    FindSessionsHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &UKemRoomSubsystem::HandleFindSessionsComplete));

    if (!SessionInterface->FindSessions(0, ActiveSessionSearch.ToSharedRef()))
    {
        ClearSessionDelegates();
        bOperationInFlight = false;
        OnJoinOperationFinished.Broadcast(false, TEXT("Failed to start LAN session search."));
    }
}

void UKemRoomSubsystem::LeaveCurrentRoom()
{
    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;

    if (SessionInterface.IsValid())
    {
        DestroySessionHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
            FOnDestroySessionCompleteDelegate::CreateUObject(this, &UKemRoomSubsystem::HandleDestroySessionComplete));
        SessionInterface->DestroySession(SessionName);
    }

    CachedClassroomState = nullptr;
    CurrentRoomInfo = FKemRoomInfo();
    PendingHostedRoomInfo = FKemRoomInfo();
    bAwaitingTravelBootstrap = false;
    BroadcastRoomInfo();

    const FString FallbackMap = TEXT("/Game/VRTemplate/Lobby");
    if (UWorld* World = GetWorld())
    {
        UGameplayStatics::OpenLevel(World, FName(*FallbackMap));
    }
}

AKemClassroomStateActor* UKemRoomSubsystem::GetCurrentClassroomState() const
{
    return CachedClassroomState.Get();
}

void UKemRoomSubsystem::NotifyClassroomStateReady(AKemClassroomStateActor* InStateActor)
{
    CachedClassroomState = InStateActor;
}

void UKemRoomSubsystem::StartHostingAuditorium(const FKemAuditoriumDefinition& Auditorium)
{
    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    if (!OnlineSubsystem)
    {
        OnHostOperationFinished.Broadcast(false, TEXT("No online subsystem is active. Check OnlineSubsystemNull settings."));
        return;
    }

    IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        OnHostOperationFinished.Broadcast(false, TEXT("Session interface is not available."));
        return;
    }

    FOnlineSessionSettings SessionSettings;
    SessionSettings.bIsLANMatch = true;
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bAllowJoinViaPresence = true;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.bUsesPresence = true;
    SessionSettings.NumPublicConnections = GetDefault<UKemSUinVRSettings>()->DefaultPublicConnections;
    SessionSettings.Set(RoomCodeKey, GenerateRoomCode(GetDefault<UKemSUinVRSettings>()->RoomCodeLength), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    SessionSettings.Set(AuditoriumKey, Auditorium.AuditoriumId.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    SessionSettings.Set(FName(TEXT("MAPNAME")), Auditorium.MapPath, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    const FString AdvertisedHostEndpoint = ResolveAdvertisedHostEndpoint();
    if (!AdvertisedHostEndpoint.IsEmpty())
    {
        SessionSettings.Set(HostEndpointKey, AdvertisedHostEndpoint, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
        UE_LOG(LogKemRoomSubsystem, Log, TEXT("Advertising host endpoint for LAN clients: %s"), *AdvertisedHostEndpoint);
    }

    PendingHostedRoomInfo = FKemRoomInfo();
    SessionSettings.Get(RoomCodeKey, PendingHostedRoomInfo.RoomCode);
    PendingHostedRoomInfo.AuditoriumId = Auditorium.AuditoriumId;
    PendingHostedRoomInfo.MapPath = Auditorium.MapPath;
    PendingHostedRoomInfo.bIsHost = true;
    PendingHostedRoomInfo.bIsInRoom = true;

    bOperationInFlight = true;
    CreateSessionHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &UKemRoomSubsystem::HandleCreateSessionComplete));

    if (!SessionInterface->CreateSession(0, SessionName, SessionSettings))
    {
        ClearSessionDelegates();
        bOperationInFlight = false;
        PendingHostedRoomInfo = FKemRoomInfo();
        OnHostOperationFinished.Broadcast(false, TEXT("Failed to start LAN room creation."));
    }
}

FString UKemRoomSubsystem::GenerateRoomCode(int32 CodeLength) const
{
    static const TCHAR Alphabet[] = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    FString Generated;
    Generated.Reserve(CodeLength);

    for (int32 Index = 0; Index < CodeLength; ++Index)
    {
        const int32 AlphabetIndex = FMath::RandRange(0, UE_ARRAY_COUNT(Alphabet) - 2);
        Generated.AppendChar(Alphabet[AlphabetIndex]);
    }

    return Generated;
}

FString UKemRoomSubsystem::SanitizeRoomCode(const FString& RoomCode) const
{
    FString Sanitized = RoomCode.ToUpper();
    Sanitized.RemoveSpacesInline();
    Sanitized.ReplaceInline(TEXT("-"), TEXT(""));
    return Sanitized;
}

FString UKemRoomSubsystem::BuildTravelUrl(const FString& ResolvedConnectString, const FString& AdvertisedHostEndpoint) const
{
    auto IsUsableTravelEndpoint = [](const FString& Endpoint) -> bool
    {
        FString Candidate = Endpoint;
        Candidate.TrimStartAndEndInline();
        if (Candidate.IsEmpty())
        {
            return false;
        }

        FString HostPart = Candidate;
        int32 QuestionMarkIndex = INDEX_NONE;
        if (HostPart.FindChar(TEXT('?'), QuestionMarkIndex))
        {
            HostPart = HostPart.Left(QuestionMarkIndex);
        }

        int32 ColonIndex = INDEX_NONE;
        if (HostPart.FindChar(TEXT(':'), ColonIndex))
        {
            HostPart = HostPart.Left(ColonIndex);
        }

        HostPart.TrimStartAndEndInline();
        return !HostPart.IsEmpty() &&
            HostPart != TEXT("0.0.0.0") &&
            HostPart != TEXT("localhost") &&
            !HostPart.StartsWith(TEXT("127."));
    };

    const FString NormalizedResolvedConnectString = NormalizeEndpoint(ResolvedConnectString);
    const FString NormalizedAdvertisedEndpoint = NormalizeEndpoint(AdvertisedHostEndpoint);

    // Prefer the endpoint resolved by OSS first (it includes the real session address/port).
    if (IsUsableTravelEndpoint(NormalizedResolvedConnectString))
    {
        return NormalizedResolvedConnectString;
    }

    if (IsUsableTravelEndpoint(NormalizedAdvertisedEndpoint))
    {
        return NormalizedAdvertisedEndpoint;
    }

    // Keep best-effort fallback behavior even if endpoint quality is poor.
    if (!NormalizedResolvedConnectString.IsEmpty())
    {
        return NormalizedResolvedConnectString;
    }

    return NormalizedAdvertisedEndpoint;
}

FString UKemRoomSubsystem::NormalizeEndpoint(const FString& Endpoint) const
{
    FString Normalized = Endpoint;
    Normalized.TrimStartAndEndInline();
    if (Normalized.IsEmpty())
    {
        return FString();
    }

    if (!Normalized.Contains(TEXT(":")) && !Normalized.Contains(TEXT("?")))
    {
        Normalized += TEXT(":7777");
    }

    return Normalized;
}

FString UKemRoomSubsystem::ResolveAdvertisedHostEndpoint() const
{
    const UKemSUinVRSettings* Settings = GetDefault<UKemSUinVRSettings>();
    if (Settings && !Settings->PreferredHostEndpointOverride.IsEmpty())
    {
        return NormalizeEndpoint(Settings->PreferredHostEndpointOverride);
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return FString();
    }

    TArray<TSharedPtr<FInternetAddr>> Addresses;
    if (!SocketSubsystem->GetLocalAdapterAddresses(Addresses))
    {
        return FString();
    }

    FString BestAddress;
    int32 BestScore = TNumericLimits<int32>::Lowest();

    for (const TSharedPtr<FInternetAddr>& Address : Addresses)
    {
        if (!Address.IsValid())
        {
            continue;
        }

        const FString Candidate = Address->ToString(false);
        if (Candidate.IsEmpty() || Candidate.Contains(TEXT(":")) || Candidate.StartsWith(TEXT("127.")) || Candidate == TEXT("0.0.0.0") || Candidate.StartsWith(TEXT("169.254.")))
        {
            continue;
        }

        const int32 Score = ScoreHostAddress(Candidate);
        if (Score > BestScore)
        {
            BestScore = Score;
            BestAddress = Candidate;
        }
    }

    return NormalizeEndpoint(BestAddress);
}

int32 UKemRoomSubsystem::ScoreHostAddress(const FString& Address) const
{
    if (Address.StartsWith(TEXT("192.168.")))
    {
        return 1000;
    }

    if (Address.StartsWith(TEXT("10.")))
    {
        return 900;
    }

    if (Address.StartsWith(TEXT("172.")))
    {
        TArray<FString> Parts;
        Address.ParseIntoArray(Parts, TEXT("."));
        if (Parts.Num() >= 2)
        {
            const int32 SecondOctet = FCString::Atoi(*Parts[1]);
            if (SecondOctet >= 16 && SecondOctet <= 31)
            {
                return 850;
            }
        }
    }

    if (Address.StartsWith(TEXT("26.")))
    {
        return 700;
    }

    return 500;
}

void UKemRoomSubsystem::BroadcastRoomInfo()
{
    OnRoomInfoChanged.Broadcast(CurrentRoomInfo);
}

void UKemRoomSubsystem::BootstrapRoomAfterTravel(UWorld* World)
{
    if (!World || !World->IsGameWorld())
    {
        return;
    }

    EnsureVoiceEnabledForLocalPlayer(World);

    if (!bAwaitingTravelBootstrap || !CurrentRoomInfo.bIsHost)
    {
        return;
    }

    if (World->GetNetMode() == NM_Client)
    {
        return;
    }

    AKemClassroomStateActor* ExistingState = nullptr;
    for (TActorIterator<AKemClassroomStateActor> It(World); It; ++It)
    {
        ExistingState = *It;
        break;
    }

    if (!ExistingState)
    {
        ExistingState = World->SpawnActor<AKemClassroomStateActor>();
    }

    if (ExistingState)
    {
        ExistingState->InitializeHostedRoom(CurrentRoomInfo, GetDefault<UKemSUinVRSettings>()->DefaultLectureUrl);
        CachedClassroomState = ExistingState;
        bAwaitingTravelBootstrap = false;
    }
}

void UKemRoomSubsystem::EnsureVoiceEnabledForLocalPlayer(UWorld* World) const
{
    if (!World)
    {
        return;
    }

    if (APlayerController* PlayerController = World->GetFirstPlayerController())
    {
        if (auto* Menu = UKemEscapeMenuSubsystem::Get(PlayerController))
            Menu->SetMicrophoneEnabled(Menu->bMicrophoneEnabled);
        else PlayerController->ConsoleCommand(TEXT("ToggleSpeaking 1"), true);
    }
}

void UKemRoomSubsystem::HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS)
{
    if (!World || !World->IsGameWorld())
    {
        return;
    }

    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UKemRoomSubsystem::BootstrapRoomAfterTravel, World));

    if (bAwaitingClientTravel && World->GetNetMode() == NM_Client)
    {
        bAwaitingClientTravel = false;
        CurrentRoomInfo.bIsInRoom = true;
        BroadcastRoomInfo();
        OnJoinOperationFinished.Broadcast(true, FString::Printf(TEXT("Joined room %s"), *CurrentRoomInfo.RoomCode));
        ResetPendingJoinAttempt();
        UE_LOG(LogKemRoomSubsystem, Log, TEXT("Client travel completed successfully for room %s"), *CurrentRoomInfo.RoomCode);
    }
}

void UKemRoomSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
    if (!bAwaitingClientTravel)
    {
        return;
    }

    UE_LOG(LogKemRoomSubsystem, Error, TEXT("Travel failure while joining room %s. Failure=%s Error=%s"),
        *PendingJoinCode,
        *UEnum::GetValueAsString(FailureType),
        *ErrorString);

    bAwaitingClientTravel = false;
    CurrentRoomInfo = FKemRoomInfo();
    BroadcastRoomInfo();
    ResetPendingJoinAttempt();
    OnJoinOperationFinished.Broadcast(false, FString::Printf(TEXT("Failed to travel to the host: %s"), *ErrorString));
}

void UKemRoomSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
    const bool bClientJoinAffected = bAwaitingClientTravel || (CurrentRoomInfo.bIsInRoom && !CurrentRoomInfo.bIsHost);
    if (!bClientJoinAffected)
    {
        return;
    }

    UE_LOG(LogKemRoomSubsystem, Error, TEXT("Network failure in room flow. Failure=%s Error=%s"),
        *UEnum::GetValueAsString(FailureType),
        *ErrorString);

    bAwaitingClientTravel = false;
    CachedClassroomState = nullptr;
    CurrentRoomInfo = FKemRoomInfo();
    BroadcastRoomInfo();
    ResetPendingJoinAttempt();
    OnJoinOperationFinished.Broadcast(false, FString::Printf(TEXT("Network connection to the host failed: %s"), *ErrorString));
}

void UKemRoomSubsystem::ResetPendingJoinAttempt()
{
    PendingJoinCode.Reset();
    PendingJoinHostEndpoint.Reset();
    PendingJoinMapPath.Reset();
    PendingJoinAuditoriumId = NAME_None;
    ActiveSessionSearch.Reset();
}

void UKemRoomSubsystem::HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful)
{
    bOperationInFlight = false;

    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    if (IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr)
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
    }
    CreateSessionHandle.Reset();

    if (!bWasSuccessful)
    {
        PendingHostedRoomInfo = FKemRoomInfo();
        OnHostOperationFinished.Broadcast(false, TEXT("LAN room creation failed."));
        return;
    }

    CurrentRoomInfo = PendingHostedRoomInfo;
    PendingHostedRoomInfo = FKemRoomInfo();
    CurrentRoomInfo.MapPath = NormalizeMapPath(CurrentRoomInfo.MapPath);

    if (!FPackageName::DoesPackageExist(CurrentRoomInfo.MapPath))
    {
        const FString MissingMapPath = CurrentRoomInfo.MapPath;
        UE_LOG(LogKemRoomSubsystem, Error, TEXT("Host map package does not exist: %s"), *CurrentRoomInfo.MapPath);
        CurrentRoomInfo = FKemRoomInfo();
        BroadcastRoomInfo();
        OnHostOperationFinished.Broadcast(false, FString::Printf(TEXT("Configured map is missing in build: %s"), *MissingMapPath));
        return;
    }

    bAwaitingTravelBootstrap = true;
    BroadcastRoomInfo();
    OnHostOperationFinished.Broadcast(true, FString::Printf(TEXT("Room created. Code: %s"), *CurrentRoomInfo.RoomCode));

    if (UWorld* World = GetWorld())
    {
        UGameplayStatics::OpenLevel(World, FName(*CurrentRoomInfo.MapPath), true, TEXT("listen"));
    }
}

void UKemRoomSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
    bOperationInFlight = false;

    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
    }
    FindSessionsHandle.Reset();

    if (!bWasSuccessful || !ActiveSessionSearch.IsValid())
    {
        OnJoinOperationFinished.Broadcast(false, TEXT("LAN session search failed."));
        return;
    }

    for (const FOnlineSessionSearchResult& Result : ActiveSessionSearch->SearchResults)
    {
        FString FoundCode;
        if (Result.Session.SessionSettings.Get(RoomCodeKey, FoundCode) && SanitizeRoomCode(FoundCode) == PendingJoinCode)
        {
            Result.Session.SessionSettings.Get(HostEndpointKey, PendingJoinHostEndpoint);
            Result.Session.SessionSettings.Get(FName(TEXT("MAPNAME")), PendingJoinMapPath);

            FString FoundAuditoriumId;
            if (Result.Session.SessionSettings.Get(AuditoriumKey, FoundAuditoriumId))
            {
                PendingJoinAuditoriumId = FName(*FoundAuditoriumId);
            }

            JoinSessionHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
                FOnJoinSessionCompleteDelegate::CreateUObject(this, &UKemRoomSubsystem::HandleJoinSessionComplete));

            if (!SessionInterface->JoinSession(0, SessionName, Result))
            {
                SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
                JoinSessionHandle.Reset();
                ResetPendingJoinAttempt();
                OnJoinOperationFinished.Broadcast(false, TEXT("JoinSession failed to start."));
                return;
            }

            return;
        }
    }

    OnJoinOperationFinished.Broadcast(false, TEXT("Room with this code was not found on LAN."));
}

void UKemRoomSubsystem::HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
    }
    JoinSessionHandle.Reset();

    if (!SessionInterface.IsValid() || Result != EOnJoinSessionCompleteResult::Success)
    {
        OnJoinOperationFinished.Broadcast(false, TEXT("Joining the room failed."));
        return;
    }

    FString ConnectString;
    SessionInterface->GetResolvedConnectString(SessionName, ConnectString);

    const FString TravelUrl = BuildTravelUrl(ConnectString, PendingJoinHostEndpoint);
    if (TravelUrl.IsEmpty())
    {
        ResetPendingJoinAttempt();
        OnJoinOperationFinished.Broadcast(false, TEXT("Could not resolve the host address."));
        return;
    }

    CurrentRoomInfo.RoomCode = PendingJoinCode;
    CurrentRoomInfo.AuditoriumId = PendingJoinAuditoriumId;
    CurrentRoomInfo.MapPath = PendingJoinMapPath;
    CurrentRoomInfo.bIsHost = false;
    CurrentRoomInfo.bIsInRoom = false;
    bAwaitingClientTravel = true;

    UE_LOG(LogKemRoomSubsystem, Log, TEXT("JoinSession resolved. ConnectString=%s AdvertisedEndpoint=%s FinalTravelUrl=%s"),
        *ConnectString,
        *PendingJoinHostEndpoint,
        *TravelUrl);

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PlayerController = World->GetFirstPlayerController())
        {
            OnJoinOperationFinished.Broadcast(true, FString::Printf(TEXT("Connecting to room %s..."), *PendingJoinCode));
            PlayerController->ClientTravel(TravelUrl, ETravelType::TRAVEL_Absolute);
            return;
        }
    }

    bAwaitingClientTravel = false;
    CurrentRoomInfo = FKemRoomInfo();
    ResetPendingJoinAttempt();
    OnJoinOperationFinished.Broadcast(false, TEXT("Could not find the local player controller for client travel."));
}

void UKemRoomSubsystem::HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful)
{
    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
    }
    DestroySessionHandle.Reset();
}

void UKemRoomSubsystem::ClearSessionDelegates()
{
    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    IOnlineSessionPtr SessionInterface = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
    if (!SessionInterface.IsValid())
    {
        return;
    }

    if (CreateSessionHandle.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
        CreateSessionHandle.Reset();
    }

    if (FindSessionsHandle.IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
        FindSessionsHandle.Reset();
    }

    if (JoinSessionHandle.IsValid())
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
        JoinSessionHandle.Reset();
    }

    if (DestroySessionHandle.IsValid())
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
        DestroySessionHandle.Reset();
    }
}
