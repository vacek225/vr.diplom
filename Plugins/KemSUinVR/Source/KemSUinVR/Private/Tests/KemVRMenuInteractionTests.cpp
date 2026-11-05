#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/KemEscapeMenuSubsystem.h"
#include "Interaction/KemVRWidgetInteractorComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedPlayerInput.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Components/WidgetInteractionComponent.h"
#include "Widgets/Input/SButton.h"
#include "RenderingThread.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKemVRMenuInteractionTest, "KemSUinVR.EscapeMenu.VRInputAndPanelDrag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKemVRMenuInteractionTest::RunTest(const FString& Parameters)
{
    auto* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/KUB/Input/IMC_KemVRUI.IMC_KemVRUI"));
    if (!TestNotNull(TEXT("Persistent OpenXR context is packaged"), Context)) return false;
    bool bLeftMenu=false, bRightMenu=false, bLeftTrigger=false, bRightTrigger=false;
    for (const auto& Mapping : Context->GetMappings())
    {
        TestTrue(TEXT("Valid OpenXR key"), Mapping.Key.IsValid());
        bLeftMenu |= Mapping.Key == EKeys::Vive_Left_Menu_Click;
        bRightMenu |= Mapping.Key == EKeys::Vive_Right_Menu_Click;
        bLeftTrigger |= Mapping.Key == EKeys::Vive_Left_Trigger_Axis;
        bRightTrigger |= Mapping.Key == EKeys::Vive_Right_Trigger_Axis;
        TestFalse(TEXT("System is not an application binding"), Mapping.Key.ToString().Contains(TEXT("System")));
        if (Mapping.Key.ToString().Contains(TEXT("Grip")))
            TestTrue(TEXT("Grip is reserved for object interaction"), Mapping.Action && Mapping.Action->GetName().StartsWith(TEXT("IA_KemGrip")));
    }
    TestTrue(TEXT("Both Vive Menu buttons mapped"), bLeftMenu && bRightMenu);
    TestTrue(TEXT("Both Vive triggers mapped"), bLeftTrigger && bRightTrigger);
    TestTrue(TEXT("Left frame is draggable"), UKemEscapeMenuSubsystem::IsPanelEdge(FVector2D(12,640), FVector2D(1000,1280)));
    TestTrue(TEXT("Bottom frame is draggable"), UKemEscapeMenuSubsystem::IsPanelEdge(FVector2D(500,1270), FVector2D(1000,1280)));
    TestFalse(TEXT("Controls are not drag handles"), UKemEscapeMenuSubsystem::IsPanelEdge(FVector2D(500,500), FVector2D(1000,1280)));
    TestFalse(TEXT("Outside the panel is not a handle"), UKemEscapeMenuSubsystem::IsPanelEdge(FVector2D(-1,640), FVector2D(1000,1280)));

    auto* World = FAutomationEditorCommonUtils::CreateNewMap();
    auto* PC = World->SpawnActor<APlayerController>();
    PC->PlayerInput = NewObject<UEnhancedPlayerInput>(PC);
    auto SetKey = [PC](FKey Key, bool bDown)
    {
        PC->PlayerInput->InputKey(FInputKeyEventArgs(nullptr, INPUTDEVICEID_NONE, Key,
            bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f, false, 0u));
        PC->PlayerInput->ProcessInputStack(TArray<UInputComponent*>(), .016f, false);
    };
    SetKey(EKeys::Vive_Left_Trigger_Click, true);
    TestTrue(TEXT("Left digital trigger detected"), UKemVRWidgetInteractorComponent::GetTriggerStrength(PC,false) > .5f);
    TestTrue(TEXT("Left trigger blocks tools until release"), UKemVRWidgetInteractorComponent::AreTriggersHeld(PC));
    TestEqual(TEXT("Left does not press right"), UKemVRWidgetInteractorComponent::GetTriggerStrength(PC,true), 0.f);
    SetKey(EKeys::Vive_Left_Trigger_Click, false);
    TestFalse(TEXT("Released trigger clears guard"), UKemVRWidgetInteractorComponent::AreTriggersHeld(PC));

    auto* Player = NewObject<ULocalPlayer>(GEngine);
    auto* Menu = NewObject<UKemEscapeMenuSubsystem>(Player);
    Menu->ActiveController = PC;
    Menu->bMenuOpen = true;
    Menu->PanelActor = World->SpawnActor<AActor>();
    auto* Widget = NewObject<UWidgetComponent>(Menu->PanelActor);
    Menu->WorldWidget = Widget;
    Menu->PanelActor->SetRootComponent(Widget);
    Widget->SetDrawSize(FVector2D(1000,1280));
    Widget->SetPivot(FVector2D(.5,.5));
    Widget->SetMobility(EComponentMobility::Movable);
    Widget->RegisterComponent();
    auto* Pawn = World->SpawnActor<APawn>();
    auto* Camera = NewObject<UCameraComponent>(Pawn);
    Pawn->SetRootComponent(Camera);
    Camera->RegisterComponent();
    Camera->bLockToHmd = true;
    Camera->SetActive(true);
    Camera->SetWorldLocationAndRotation(FVector(480,125,180), FRotator(0,90,0));
    PC->Possess(Pawn);
    Menu->RepositionPanel();
    TestTrue(TEXT("Panel appears in front of tracked camera, not pawn origin"),
        Menu->PanelActor->GetActorLocation().Equals(FVector(480,215,175),.01f));
    Menu->PanelActor->SetActorScale3D(FVector(.085));
    Menu->PanelActor->SetActorLocationAndRotation(FVector(145,0,170), FRotator(0,180,0));
    FHitResult Hit(Menu->PanelActor, Widget, Widget->GetComponentTransform().TransformPosition(FVector(0,490,0)), FVector::ForwardVector);
    const FTransform Hand(FQuat::Identity, FVector(100,0,140));
    const FTransform Initial = Menu->PanelActor->GetActorTransform();
    TestTrue(TEXT("Trigger on edge starts drag"), Menu->BeginPanelDrag(true,Hand,Hit));
    FTransform Moved(FRotator(0,35,0), FVector(110,40,150));
    Menu->UpdatePanelDrag(false,Moved);
    TestTrue(TEXT("Other hand cannot steal drag"), Menu->PanelActor->GetActorTransform().Equals(Initial));
    Menu->EndPanelDrag(false);
    TestTrue(TEXT("Other trigger release cannot stop drag"), Menu->IsPanelDragging());
    Menu->UpdatePanelDrag(true,Moved);
    const FTransform Expected = Initial.GetRelativeTransform(Hand) * Moved;
    TestTrue(TEXT("Panel follows translation and rotation without snapping to hand"), Menu->PanelActor->GetActorTransform().Equals(Expected,.001f));
    Menu->EndPanelDrag(true);
    TestFalse(TEXT("Owning trigger release stops drag"), Menu->IsPanelDragging());
    const FTransform Released = Menu->PanelActor->GetActorTransform();
    Menu->UpdatePanelDrag(true,Hand);
    TestTrue(TEXT("Released panel stays put"), Menu->PanelActor->GetActorTransform().Equals(Released));
    Hit.ImpactPoint = Widget->GetComponentTransform().TransformPosition(FVector(0,490,0));
    TestTrue(TEXT("Left hand can start the next drag"), Menu->BeginPanelDrag(false,Moved,Hit));
    Menu->EndPanelDrag(false);
    if (FApp::CanEverRender())
    {
        int32 Clicks = 0;
        Widget->SetSlateWidget(SNew(SButton).OnClicked_Lambda([&Clicks]() { ++Clicks; return FReply::Handled(); }));
        Widget->SetTickWhenOffscreen(true);
        Widget->RequestRedraw();
        Widget->TickComponent(.016f, LEVELTICK_All, nullptr);
        FlushRenderingCommands();
        FHitResult Center(Menu->PanelActor, Widget, Widget->GetComponentLocation(), FVector::ForwardVector);
        Center.bBlockingHit = true;
        Center.TraceStart = Center.ImpactPoint + Widget->GetForwardVector()*100;
        Center.TraceEnd = Center.ImpactPoint - Widget->GetForwardVector()*100;
        for (int32 Pointer : {1,2})
        {
            auto* Interaction = NewObject<UWidgetInteractionComponent>(Menu->PanelActor);
            Interaction->VirtualUserIndex = 1;
            Interaction->PointerIndex = Pointer;
            Interaction->InteractionSource = EWidgetInteractionSource::Custom;
            Interaction->RegisterComponent();
            Interaction->SetCustomHitResult(Center);
            Interaction->TickComponent(.016f, LEVELTICK_All, nullptr);
            TestTrue(TEXT("Controller pointer hits world UI"), Interaction->IsOverHitTestVisibleWidget());
            Interaction->PressPointerKey(EKeys::LeftMouseButton);
            Interaction->ReleasePointerKey(EKeys::LeftMouseButton);
            TestEqual(TEXT("Each controller delivers exactly one click"), Clicks, Pointer);
            Interaction->DestroyComponent();
        }
        Widget->SetSlateWidget(nullptr);
    }
    Hit.ImpactPoint = Widget->GetComponentTransform().TransformPosition(FVector(0,490,0));
    Menu->BeginPanelDrag(false,Moved,Hit);
    Menu->SetMenuOpen(false);
    TestFalse(TEXT("Closing while held cancels drag"), Menu->IsPanelDragging());
    TestNull(TEXT("Closing removes panel"), Menu->PanelActor.Get());
    PC->Destroy();
    Pawn->Destroy();
    return true;
}
#endif
