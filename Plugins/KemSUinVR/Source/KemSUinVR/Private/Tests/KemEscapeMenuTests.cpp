#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UI/KemEscapeMenuSubsystem.h"
#include "UI/KemEscapeMenuWidget.h"
#include "Actors/KemDesktopCharacter.h"
#include "Actors/KemPlayerControlProxy.h"
#include "Actors/KemPointerActor.h"
#include "Actors/KemDrawingBoardActor.h"
#include "Engine/KemEngineAssembly.h"
#include "Engine/KemEnginePart.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetComponent.h"
#include "Components/ScrollBox.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/KemRoomSubsystem.h"
#include "Actors/KemClassroomStateActor.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "RenderingThread.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"

class FKemEscapeLifecycleCommand : public IAutomationLatentCommand
{
public:
    explicit FKemEscapeLifecycleCommand(FAutomationTestBase* InTest) : Test(InTest)
    {
        auto* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
        Settings->GetPlayNetMode(OldMode); Settings->GetPlayNumberOfClients(OldClients);
        Settings->GetRunUnderOneProcess(bOldSingleProcess);
        Settings->SetPlayNetMode(PIE_ListenServer); Settings->SetPlayNumberOfClients(2);
        Settings->SetRunUnderOneProcess(true);
        FRequestPlaySessionParams Params; Params.SessionDestination=EPlaySessionDestinationType::InProcess;
        GEditor->RequestPlaySession(Params); Started=FPlatformTime::Seconds();
    }
    virtual bool Update() override
    {
        if (FPlatformTime::Seconds()-Started>35) { Test->AddError(FString::Printf(TEXT("Escape menu PIE test timed out at stage %d"),Stage)); return Finish(); }
        UWorld* Server=nullptr; UWorld* Client=nullptr;
        for (const auto& C : GEngine->GetWorldContexts())
            if(C.WorldType==EWorldType::PIE && C.World() && C.World()->HasBegunPlay())
            {
                if(C.World()->GetNetMode()==NM_ListenServer) Server=C.World();
                if(C.World()->GetNetMode()==NM_Client) Client=C.World();
            }
        if(!Server || !Client) return false;
        auto* PC=Client->GetFirstPlayerController();
        if(!PC || (Stage<5 && !PC->GetPawn())) return false;
        auto* Menu=UKemEscapeMenuSubsystem::Get(PC);
        if(!Menu) return false;
        if(Stage==0)
        {
            for(auto It=Server->GetPlayerControllerIterator();It;++It)
            { FActorSpawnParameters P; P.Owner=It->Get(); Server->SpawnActor<AKemPlayerControlProxy>(P); }
            Board=Server->SpawnActor<AKemDrawingBoardActor>();
            Board->AcceptDraw(Board.Get(),FVector2D(.1f,.1f),true,FLinearColor::Red,4.f);
            Menu->SetTool(EKemTool::Brush);
            PC->SetIgnoreMoveInput(true); // Existing lock must survive a menu open/close.
            Menu->SetMenuOpen(true);
            Test->TestTrue(TEXT("Menu opens"),Menu->IsMenuOpen());
            for(bool bVR : {false,true})
            {
                auto* Layout=CreateWidget<UKemEscapeMenuWidget>(PC,UKemEscapeMenuWidget::StaticClass());
                Layout->SetVRLayout(bVR); Layout->TakeWidget();
                for(const FString& Section : {FString(TEXT("tools")),FString(TEXT("objects")),FString(TEXT("audio"))})
                {
                    Layout->SelectSection(Section);
                    int32 ExitCount=0;
                    Layout->WidgetTree->ForEachWidget([&](UWidget* Widget)
                    {
                        auto* Action=Cast<UKemActionButton>(Widget);
                        if(!Action || (Action->ActionId!=TEXT("confirm:quit") && Action->ActionId!=TEXT("confirm:leave"))) return;
                        ++ExitCount;
                        Test->TestTrue(TEXT("Exit is always visible"),Action->GetVisibility()==ESlateVisibility::Visible);
                        for(auto* Parent=Action->GetParent(); Parent; Parent=Parent->GetParent())
                            Test->TestFalse(TEXT("Exit buttons never scroll with page"),Parent->IsA<UScrollBox>());
                    });
                    Test->TestEqual(TEXT("Both exits are present on every tab and layout"),ExitCount,2);
                }
            }

            Test->TestTrue(TEXT("PC cursor becomes visible"),PC->bShowMouseCursor);
            Test->TestTrue(TEXT("Gameplay tools blocked"),!Menu->CanUseTool());
            Test->TestFalse(TEXT("Network simulation is not paused"),PC->IsPaused());
            Menu->SetMenuOpen(true); Menu->SetMenuOpen(false);
            Test->TestTrue(TEXT("Previous movement lock preserved"),PC->IsMoveInputIgnored());
            PC->SetIgnoreMoveInput(false);
            Test->TestFalse(TEXT("Menu adds no duplicate movement lock"),PC->IsMoveInputIgnored());
            Test->TestTrue(TEXT("Selected tool survives closing"),Menu->Tool==EKemTool::Brush);
            Test->TestFalse(TEXT("Cursor restored"),PC->bShowMouseCursor);
            Menu->ToggleMenu(); Menu->ToggleMenu();
            Test->TestFalse(TEXT("Repeated toggle closes"),Menu->IsMenuOpen());
            ServerClock=Server->GetTimeSeconds(); Stage=1; return false;
        }
        auto* Proxy=AKemPlayerControlProxy::FindForController(PC); if(!Proxy) return false;
        if(Stage==1)
        {
            if(FApp::CanEverRender() && CaptureIndex<6)
            {
                const TArray<FString> Sections={TEXT("room"),TEXT("tools"),TEXT("objects"),TEXT("screen"),TEXT("people"),TEXT("audio")};
                // One layout per frame: Slate caches invalidation roots within a frame.
                Menu->SetMenuOpen(true);
                if(!bCapturePending)
                { Menu->GetMenuWidget()->SelectSection(Sections[CaptureIndex]); bCapturePending=true; return false; }
                CaptureMenu(Menu,Sections[CaptureIndex++]);
                bCapturePending=false;
                if(CaptureIndex==6) Menu->SetMenuOpen(false);
                return false;
            }
            if(Server->GetTimeSeconds()<ServerClock+.5f) return false;
            Test->TestTrue(TEXT("Tools recover after release"),Menu->CanUseTool());
            FVector Start=PC->GetPawn()->GetActorLocation();
            Proxy->ServerUpdatePointer(Start,Start+FVector(100,0,0),true);
            Stage=2; return false;
        }
        if(Stage==2)
        {
            AKemPointerActor* Pointer=nullptr;
            for(TActorIterator<AKemPointerActor> It(Client);It;++It) Pointer=*It;
            if(!Pointer) return false;
            Test->TestTrue(TEXT("Student pointer replicates through owned proxy"),Pointer->GetIsReplicated());
            AKemDrawingBoardActor* ClientBoard=nullptr;
            for(TActorIterator<AKemDrawingBoardActor> It(Client);It;++It) ClientBoard=*It;
            if(!ClientBoard || ClientBoard->GetSegmentCount()!=1) return false;
            Test->TestEqual(TEXT("Initial board history reaches client"),ClientBoard->GetSegmentCount(),1);
            Proxy->ServerDrawBoard(ClientBoard,FVector2D(.2f,.2f),true,FLinearColor::Blue,8.f);
            Stage=3; return false;
        }
        if(Stage==3)
        {
            AKemDrawingBoardActor* ClientBoard=nullptr;
            for(TActorIterator<AKemDrawingBoardActor> It(Client);It;++It) ClientBoard=*It;
            if(!Board.IsValid() || Board->GetSegmentCount()!=2 || !ClientBoard || ClientBoard->GetSegmentCount()!=2) return false;
            Test->AddInfo(TEXT("Client drawing RPC replicated to host and client"));
            Board->ClearBoard(); Stage=4; return false;
        }
        if(Stage==4)
        {
            for(TActorIterator<AKemDrawingBoardActor> It(Client);It;++It) if(It->GetSegmentCount()!=0) return false;
            Menu->SetMenuOpen(true);
            for(auto It=Server->GetPlayerControllerIterator();It;++It)
                if(!It->Get()->IsLocalController() && It->Get()->GetPawn()) It->Get()->GetPawn()->Destroy();
            Stage=5; return false;
        }
        if(PC->GetPawn() || Menu->IsMenuOpen()) return false;
        Test->TestFalse(TEXT("Pawn destruction closes menu"),Menu->IsMenuOpen());
        Test->TestFalse(TEXT("Pawn change restores input lock"),PC->IsMoveInputIgnored());
        return Finish();
    }
private:
    void CaptureMenu(UKemEscapeMenuSubsystem* Menu, const FString& Section)
    {
        // A separate widget avoids reusing the live viewport's cached DPI geometry.
        auto* Widget=CreateWidget<UKemEscapeMenuWidget>(Menu->GetMenuWidget()->GetOwningPlayer(), UKemEscapeMenuWidget::StaticClass());
        if(!Test->TestNotNull(TEXT("Native menu widget"),Widget)) return;
        Widget->TakeWidget();
        Widget->SelectSection(Section);
        Widget->NativeTick(FGeometry(),.5f);
        auto* Renderer=new FWidgetRenderer(false);
        {
            auto* Target=Renderer->DrawWidget(Widget->TakeWidget(),Widget->GetDesignSize());
            FlushRenderingCommands();
            TArray<FColor> Pixels;
            FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
            const bool Read=Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
            // The offscreen target stores linear channels; PNG viewers expect sRGB.
            if(Read) for(FColor& Pixel:Pixels) Pixel=FLinearColor(Pixel.R/255.f,Pixel.G/255.f,Pixel.B/255.f,Pixel.A/255.f).ToFColorSRGB();
            TArray64<uint8> Data;
            if(Read) FImageUtils::PNGCompressImageArray(Widget->GetDesignSize().X,Widget->GetDesignSize().Y,Pixels,Data);
            const FString Path=FPaths::ProjectSavedDir()/TEXT("Screenshots/EscapeMenu")/(Section+TEXT(".png"));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
            Test->TestTrue(TEXT("Export native UMG screenshot"),Read && FFileHelper::SaveArrayToFile(Data,*Path));
        }
        BeginCleanup(Renderer);
    }
    bool Finish()
    {
        GEditor->RequestEndPlayMap();
        auto* S=GetMutableDefault<ULevelEditorPlaySettings>();
        S->SetPlayNetMode(OldMode); S->SetPlayNumberOfClients(OldClients); S->SetRunUnderOneProcess(bOldSingleProcess);
        return true;
    }
    FAutomationTestBase* Test;
    int32 CaptureIndex=0; bool bCapturePending=false;
    TWeakObjectPtr<AKemDrawingBoardActor> Board;
    double Started=0; float ServerClock=0; int32 Stage=0;
    EPlayNetMode OldMode=PIE_Standalone; int32 OldClients=1; bool bOldSingleProcess=true;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKemEscapeMenuLifecycleTest,"KemSUinVR.EscapeMenu.LifecycleAndNetworkPointer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKemEscapeMenuLifecycleTest::RunTest(const FString& Parameters)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();
    World->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
    ADD_LATENT_AUTOMATION_COMMAND(FKemEscapeLifecycleCommand(this)); return true;
}

class FKemLobbyObjectsCommand : public IAutomationLatentCommand
{
public:
    explicit FKemLobbyObjectsCommand(FAutomationTestBase* InTest) : Test(InTest)
    {
        auto* S=GetMutableDefault<ULevelEditorPlaySettings>();
        S->GetPlayNetMode(OldMode); S->GetPlayNumberOfClients(OldClients); S->GetRunUnderOneProcess(OldSingle);
        S->SetPlayNetMode(PIE_Standalone); S->SetPlayNumberOfClients(1); S->SetRunUnderOneProcess(true);
        FRequestPlaySessionParams P; P.SessionDestination=EPlaySessionDestinationType::InProcess;
        GEditor->RequestPlaySession(P); Started=FPlatformTime::Seconds();
    }
    virtual bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>35) { Test->AddError(TEXT("Lobby objects test timed out")); return Finish(); }
        UWorld* World=nullptr;
        for(const auto& C : GEngine->GetWorldContexts())
            if(C.WorldType==EWorldType::PIE && C.World() && C.World()->HasBegunPlay() && C.World()->GetNetMode()==NM_Standalone) World=C.World();
        auto* PC=World ? World->GetFirstPlayerController() : nullptr;
        auto* M=UKemEscapeMenuSubsystem::Get(PC);
        if(!M || !PC->GetPawn()) return false;
        if(Stage==0)
        {
            Test->TestTrue(TEXT("Lobby allows objects without a LAN session"),M->CanManageSceneObjects());
            M->SetMenuOpen(true); auto* W=M->GetMenuWidget();
            W->SelectSection(TEXT("objects"));
            if(FApp::CanEverRender()) Capture(PC,TEXT("objects"),false);
            W->SelectCatalogObject(TEXT("engine"));
            auto* Engine=Cast<AKemEngineAssembly>(W->GetSelectedSceneObject());
            if(!Test->TestNotNull(TEXT("Selecting catalog creates the engine in lobby"),Engine)) return Finish();
            Test->TestEqual(TEXT("Spawned engine has all parts"),Engine->Parts.Num(),AKemEngineAssembly::Definitions().Num());
            TArray<UWidgetComponent*> Panels; Engine->GetComponents(Panels);
            Test->TestEqual(TEXT("Engine spawns without a control placard"),Panels.Num(),0);
            Test->TestTrue(TEXT("Selecting again reuses the object"),M->SpawnSceneObject(TEXT("engine"))==Engine);
            AActor* Foreign=World->SpawnActor<AActor>();
            Test->TestFalse(TEXT("Menu cannot delete arbitrary scene actors"),M->RemoveSceneObject(Foreign));
            auto* OtherEngine=World->SpawnActor<AKemEngineAssembly>(FVector(1000,0,0),FRotator::ZeroRotator);
            Click(W,TEXT("lesson:2"));
            Test->TestTrue(TEXT("Lesson changes selected engine"),Engine->GetLesson()==EKemEngineLesson::Assemble);
            Test->TestTrue(TEXT("Other engine is unchanged"),OtherEngine->GetLesson()==EKemEngineLesson::Free);
            OtherEngine->Destroy();
            if(FApp::CanEverRender()) Capture(PC,TEXT("object_engine"),true);
            Test->TestTrue(TEXT("Engine moves with replication enabled"),Engine->IsReplicatingMovement());
            const FVector Before=Engine->GetActorLocation();
            PC->GetPawn()->AddActorWorldOffset(FVector(0,100,0));
            PC->PlayerCameraManager->UpdateCamera(0.f);
            Test->TestTrue(TEXT("Selected engine can be repositioned in lobby"),M->MoveSceneObjectInFront(Engine));
            Test->TestFalse(TEXT("Stand position changes"),Engine->GetActorLocation().Equals(Before));
            Test->TestTrue(TEXT("Cube can be created in lobby"),M->SpawnSceneObject(TEXT("cube"))!=nullptr);
            Test->TestTrue(TEXT("Sphere can be created in lobby"),M->SpawnSceneObject(TEXT("sphere"))!=nullptr);
            // Exercise the auditorium sidebar without performing a real map travel.
            auto* Room=World->GetGameInstance()->GetSubsystem<UKemRoomSubsystem>();
            {
                auto* Info=&Room->CurrentRoomInfo; const FKemRoomInfo Saved=*Info;
                Info->bIsHost=true; Info->bIsInRoom=true; Info->RoomCode=TEXT("TEST42");
                auto* State=World->SpawnActor<AKemClassroomStateActor>(); State->InitializeHostedRoom(*Info,TEXT("https://example.com"));
                W->SelectSection(TEXT("room")); W->NativeTick(FGeometry(),.5f);
                TArray<UWidget*> Widgets; W->WidgetTree->GetAllWidgets(Widgets); int32 ExitButtons=0;
                for(UWidget* Child : Widgets) if(auto* Button=Cast<UKemActionButton>(Child))
                    if(Button->ActionId==TEXT("confirm:leave") && Button->GetVisibility()==ESlateVisibility::Visible) ++ExitButtons;
                Test->TestEqual(TEXT("Auditorium has one persistent header exit-to-lobby action"),ExitButtons,1);
                if(FApp::CanEverRender()) { Capture(PC,TEXT("room_auditorium"),false); Capture(PC,TEXT("vr_room"),false); Capture(PC,TEXT("vr_tools"),false); }
                *Info=Saved; State->Destroy();
            }
            M->SetTool(EKemTool::Brush); W->SelectSection(TEXT("tools"));
            Click(W,TEXT("pointer"));
            Test->TestTrue(TEXT("Pointer is an independent switch"),M->IsPointerEnabled() && M->Tool==EKemTool::Brush);
            W->SelectSection(TEXT("tools")); M->SetMenuOpen(false); M->SetMenuOpen(true);
            Test->TestEqual(TEXT("Reopening restores Tools tab"),M->GetMenuWidget()->GetSelectedSection(),FString(TEXT("tools")));
            Test->TestNull(TEXT("Reopening clears object selection"),M->GetMenuWidget()->GetSelectedSceneObject());
            M->SetMenuOpen(false); Stage=1; SwitchTime=World->GetTimeSeconds(); return false;
        }
        if(World->GetTimeSeconds()-SwitchTime<.3f) return false;
        AKemPointerActor* Pointer=nullptr;
        for(TActorIterator<AKemPointerActor> It(World);It;++It) Pointer=*It;
        if(Stage==1)
        {
            if(!Pointer || !Pointer->IsPointerActive()) return false;
            Test->TestTrue(TEXT("Switched-on pointer works without holding trigger"),Pointer->IsPointerActive());
            M->TogglePointer(); Stage=2; SwitchTime=World->GetTimeSeconds(); return false;
        }
        if(Pointer && Pointer->IsPointerActive()) return false;
        Test->TestTrue(TEXT("Switching pointer off retains brush"),!M->IsPointerEnabled() && M->Tool==EKemTool::Brush);
        AActor* Engine=M->SpawnSceneObject(TEXT("engine"));
        Test->TestTrue(TEXT("Selected lobby object can be deleted"),M->RemoveSceneObject(Engine));
        Test->TestFalse(TEXT("Deleted object is no longer managed"),M->CanManageSceneObject(Engine));
        return Finish();
    }
private:
    void Click(UKemEscapeMenuWidget* Widget,const FString& Id)
    {
        TArray<UWidget*> Widgets; Widget->WidgetTree->GetAllWidgets(Widgets);
        for(UWidget* Child : Widgets) if(auto* Button=Cast<UKemActionButton>(Child))
            if(Button->ActionId==Id) { Button->OnKemActionClicked.Broadcast(Id); return; }
        Test->AddError(TEXT("Missing menu action: ")+Id);
    }
    void Capture(APlayerController* PC,const FString& Name,bool Selected)
    {
        auto* W=CreateWidget<UKemEscapeMenuWidget>(PC,UKemEscapeMenuWidget::StaticClass());
        W->SetVRLayout(Name.StartsWith(TEXT("vr_")));
        W->TakeWidget(); W->SelectSection(Name==TEXT("vr_tools") ? TEXT("tools") : Name==TEXT("room_auditorium") || Name==TEXT("vr_room") ? TEXT("room") : TEXT("objects"));
        if(Selected) W->SelectCatalogObject(TEXT("engine"));
        W->NativeTick(FGeometry(),.5f);
        auto* Renderer=new FWidgetRenderer(false);
        auto* Target=Renderer->DrawWidget(W->TakeWidget(),W->GetDesignSize()); FlushRenderingCommands();
        TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
        bool Read=Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
        if(Read) for(FColor& Pixel:Pixels) Pixel=FLinearColor(Pixel.R/255.f,Pixel.G/255.f,Pixel.B/255.f,Pixel.A/255.f).ToFColorSRGB();
        TArray64<uint8> Data; if(Read) FImageUtils::PNGCompressImageArray(W->GetDesignSize().X,W->GetDesignSize().Y,Pixels,Data);
        Test->TestTrue(TEXT("Save object menu preview"),Read && FFileHelper::SaveArrayToFile(Data,*(FPaths::ProjectSavedDir()/TEXT("Screenshots/EscapeMenu")/(Name+TEXT(".png")))));
        BeginCleanup(Renderer);
    }
    bool Finish()
    {
        GEditor->RequestEndPlayMap(); auto* S=GetMutableDefault<ULevelEditorPlaySettings>();
        S->SetPlayNetMode(OldMode); S->SetPlayNumberOfClients(OldClients); S->SetRunUnderOneProcess(OldSingle); return true;
    }
    FAutomationTestBase* Test; double Started=0; float SwitchTime=0; int32 Stage=0;
    EPlayNetMode OldMode=PIE_Standalone; int32 OldClients=1; bool OldSingle=true;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKemLobbyObjectsTest,"KemSUinVR.EscapeMenu.LobbyObjectsAndPointerSwitch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKemLobbyObjectsTest::RunTest(const FString& Parameters)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();
    World->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
    ADD_LATENT_AUTOMATION_COMMAND(FKemLobbyObjectsCommand(this)); return true;
}
#endif
