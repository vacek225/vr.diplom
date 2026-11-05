#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/KemEngineAssembly.h"
#include "Engine/KemEnginePart.h"
#include "Actors/KemPlayerControlProxy.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"

class FKemEngineNetworkCommand : public IAutomationLatentCommand
{
public:
    explicit FKemEngineNetworkCommand(FAutomationTestBase* InTest) : Test(InTest)
    {
        ULevelEditorPlaySettings* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
        Settings->GetPlayNetMode(OldMode);
        Settings->GetRunUnderOneProcess(bOldSingleProcess);
        Settings->GetPlayNumberOfClients(OldClients);
        Settings->SetPlayNetMode(PIE_ListenServer);
        Settings->SetRunUnderOneProcess(true);
        Settings->SetPlayNumberOfClients(2);
        FRequestPlaySessionParams Params;
        Params.SessionDestination=EPlaySessionDestinationType::InProcess;
        GEditor->RequestPlaySession(Params);
        StageStarted=FPlatformTime::Seconds();
    }
    virtual bool Update() override
    {
        if(FPlatformTime::Seconds()-StageStarted>30)
        {
            Test->AddError(FString::Printf(TEXT("Network test timed out at stage %d"),Stage));
            return Finish();
        }
        UWorld* Server=nullptr;
        UWorld* Client=nullptr;
        for(const FWorldContext& Context : GEngine->GetWorldContexts())
            if(Context.WorldType==EWorldType::PIE && Context.World() && Context.World()->HasBegunPlay())
            {
                if(Context.World()->GetNetMode()==NM_ListenServer) Server=Context.World();
                if(Context.World()->GetNetMode()==NM_Client) Client=Context.World();
            }
        if(!Server || !Client) return false;
        APlayerController* ClientPC=Client->GetFirstPlayerController();
        if(!ClientPC || !ClientPC->GetPawn()) return false;
        if(Stage==0)
        {
            int32 Count=0;
            for(FConstPlayerControllerIterator It=Server->GetPlayerControllerIterator();It;++It) ++Count;
            if(Count<2) return false;
            ServerEngine=Server->SpawnActor<AKemEngineAssembly>(FVector(0,0,100),FRotator::ZeroRotator);
            for(FConstPlayerControllerIterator It=Server->GetPlayerControllerIterator();It;++It)
            {
                FActorSpawnParameters P; P.Owner=It->Get();
                Server->SpawnActor<AKemPlayerControlProxy>(P);
            }
            Advance(); return false;
        }
        AKemEngineAssembly* SE=ServerEngine.Get();
        AKemEngineAssembly* CE=nullptr;
        for(TActorIterator<AKemEngineAssembly> It(Client);It;++It) CE=*It;
        if(Stage==10)
        {
            if(CE) return false;
            for(TActorIterator<AKemEnginePart> It(Client);It;++It) return false;
            Test->AddInfo(TEXT("Server and client replicated create, lock, move, release, reset, snap and delete successfully."));
            return Finish();
        }
        if(!SE || !CE || CE->Parts.Num()!=SE->Parts.Num()) return false;
        for(AKemEnginePart* P : CE->Parts) if(!IsValid(P) || P->GetPartId().IsNone()) return false;
        AKemPlayerControlProxy* CP=AKemPlayerControlProxy::FindForController(ClientPC);
        if(!CP) return false;
        AKemEnginePart* SC=SE->FindPart(TEXT("DecorCover"));
        AKemEnginePart* CC=CE->FindPart(TEXT("DecorCover"));
        if(Stage==1)
        {
            Test->TestEqual(TEXT("Replicated part count"),CE->Parts.Num(),AKemEngineAssembly::Definitions().Num());
            CP->ServerGrabEnginePart(CC,true,CC->GetActorTransform());
            Advance(); return false;
        }
        if(Stage==2)
        {
            if(!SC->IsHeld() || !CC->IsHeldBy(ClientPC->GetPawn(),true)) return false;
            // An actual client RPC acquired the server lock. The host cannot steal it.
            AKemPlayerControlProxy* HostProxy=nullptr;
            for(TActorIterator<AKemPlayerControlProxy> It(Server);It;++It)
                if(Cast<APlayerController>(It->GetOwner())->IsLocalController()) HostProxy=*It;
            FString Reason;
            Test->TestFalse(TEXT("Host cannot steal client-held part"),SC->TryAcquire(HostProxy,true,SC->GetActorTransform(),Reason));
            auto* LocalReceiver=CE->FindPart(TEXT("Receiver")); LocalReceiver->Tick(.3f);
            auto* HostReceiver=SE->FindPart(TEXT("Receiver")); HostReceiver->Tick(.3f);
            Test->TestTrue(TEXT("Client hand reveals only client-side geometry"),LocalReceiver->IsLocallyTransparent());
            Test->TestFalse(TEXT("Remote client grab does not fade host view"),HostReceiver->IsLocallyTransparent());
            MovedPose=CC->GetActorTransform(); MovedPose.AddToTranslation(FVector(0,-45,30));
            CP->ServerMoveEnginePart(CC,true,MovedPose);
            Advance(); return false;
        }
        if(Stage==3)
        {
            // The local holder predicts its mesh; check the received authoritative
            // snapshot independently of a motion-controller component.
            if(!SC->GetActorLocation().Equals(MovedPose.GetLocation(),0.1) || !CC->GetReplicatedPose().GetLocation().Equals(MovedPose.GetLocation(),0.1)) return false;
            CP->ServerReleaseEnginePart(CC,true,MovedPose);
            Advance(); return false;
        }
        if(Stage==4)
        {
            if(SC->IsHeld() || CC->IsHeld() || CC->IsInstalled()) return false;
            CE->FindPart(TEXT("Receiver"))->Tick(.3f);
            Test->TestFalse(TEXT("Replicated release restores client opacity"),CE->FindPart(TEXT("Receiver"))->IsLocallyTransparent());
            CP->ServerConfigureEngine(CE,EKemEngineLesson::Assemble,true);
            Advance(); return false;
        }
        if(Stage==5)
        {
            if(FPlatformTime::Seconds()-StageStarted<0.5) return false;
            Test->TestTrue(TEXT("Student cannot change lesson via RPC"),SE->GetLesson()==EKemEngineLesson::Free);
            Test->TestTrue(TEXT("Host starts assembly lesson"),SE->ConfigureLesson(EKemEngineLesson::Assemble,true));
            Advance(); return false;
        }
        if(Stage==6)
        {
            if(CE->GetLesson()!=EKemEngineLesson::Assemble) return false;
            AKemEnginePart* CB=CE->FindPart(TEXT("Block"));
            if(CB->IsInstalled() || !CB->GetActorLocation().Equals(SE->FindPart(TEXT("Block"))->GetActorLocation(),0.1)) return false;
            CP->ServerGrabEnginePart(CB,false,CB->GetActorTransform());
            // Reliable release carries the final pose, even if the unreliable move is lost.
            CP->ServerReleaseEnginePart(CB,false,CB->InstalledTransform());
            Advance(); return false;
        }
        if(Stage==7)
        {
            if(!SE->FindPart(TEXT("Block"))->IsInstalled() || !CE->FindPart(TEXT("Block"))->IsInstalled()) return false;
            Test->TestTrue(TEXT("Client snap agrees with server"),CE->FindPart(TEXT("Block"))->GetActorTransform().Equals(SE->FindPart(TEXT("Block"))->GetActorTransform(),0.1));
            auto* Crank=CE->FindPart(TEXT("Crankshaft"));
            RejectedPose=Crank->InstalledTransform(); RejectedPose.SetRotation(FRotator(0,90,0).Quaternion());
            CP->ServerGrabEnginePart(Crank,false,Crank->GetActorTransform());
            CP->ServerReleaseEnginePart(Crank,false,RejectedPose);
            Advance(); return false;
        }
        if(Stage==8)
        {
            auto* ClientCrank=CE->FindPart(TEXT("Crankshaft"));
            auto* ServerCrank=SE->FindPart(TEXT("Crankshaft"));
            if(ServerCrank->IsHeld() || ClientCrank->IsHeld() || ClientCrank->IsInstalled() ||
               !ClientCrank->GetActorTransform().Equals(RejectedPose,.1f)) return false;
            auto* ClientBlock=CE->FindPart(TEXT("Block")); ClientBlock->Tick(.3f);
            auto* HostBlock=SE->FindPart(TEXT("Block")); HostBlock->Tick(.3f);
            Test->TestFalse(TEXT("Server does not install a rejected drop"),ServerCrank->IsInstalled());
            Test->TestTrue(TEXT("Client retains reveal after replicated inside drop"),ClientBlock->IsLocallyTransparent());
            Test->TestFalse(TEXT("Dropped client part does not pin host transparency"),HostBlock->IsLocallyTransparent());
            FTransform Assisted=ClientCrank->InstalledTransform(); Assisted.AddToTranslation(FVector(10,7,3));
            Assisted.SetRotation(FRotator(0,40,0).Quaternion());
            CP->ServerGrabEnginePart(ClientCrank,false,ClientCrank->GetActorTransform());
            CP->ServerReleaseEnginePart(ClientCrank,false,Assisted);
            Advance(); return false;
        }
        if(Stage==9)
        {
            auto* Crank=CE->FindPart(TEXT("Crankshaft"));
            if(!SE->FindPart(TEXT("Crankshaft"))->IsInstalled() || !Crank->IsInstalled()) return false;
            Test->TestTrue(TEXT("Forgiving client release replicates exact socket transform"),Crank->GetActorTransform().Equals(Crank->InstalledTransform(),.1f));
            auto* Block=CE->FindPart(TEXT("Block")); Block->Tick(.3f);
            Test->TestFalse(TEXT("Successful retry clears persistent transparency"),Block->IsLocallyTransparent());
            SE->Destroy(); Advance(); return false;
        }
        return false;
    }
private:
    void Advance() { ++Stage; StageStarted=FPlatformTime::Seconds(); }
    bool Finish()
    {
        GEditor->RequestEndPlayMap();
        ULevelEditorPlaySettings* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
        Settings->SetPlayNetMode(OldMode); Settings->SetPlayNumberOfClients(OldClients);
        Settings->SetRunUnderOneProcess(bOldSingleProcess);
        return true;
    }
    FAutomationTestBase* Test;
    TWeakObjectPtr<AKemEngineAssembly> ServerEngine;
    FTransform MovedPose;
    FTransform RejectedPose;
    int32 Stage=0;
    double StageStarted=0;
    EPlayNetMode OldMode=PIE_Standalone;
    int32 OldClients=1;
    bool bOldSingleProcess=true;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKemEngineNetworkTest,"KemSUinVR.Engine2112.NetworkPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKemEngineNetworkTest::RunTest(const FString& Parameters)
{
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
    World->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
    ADD_LATENT_AUTOMATION_COMMAND(FKemEngineNetworkCommand(this));
    return true;
}
#endif
