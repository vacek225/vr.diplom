#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Interaction/KemVRGrabInteractionComponent.h"
#include "Interaction/KemVRWidgetInteractorComponent.h"
#include "Interaction/KemDrawingBoardInteractionComponent.h"
#include "Actors/KemDrawingBoardActor.h"
#include "Actors/KemPlayerControlProxy.h"
#include "Engine/KemEngineAssembly.h"
#include "Engine/KemEnginePart.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedPlayerInput.h"
#include "MotionControllerComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/Script.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKemRayGrabTest,"KemSUinVR.EscapeMenu.BrushAndRayGrab",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKemRayGrabTest::RunTest(const FString& Parameters)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* PC=World->SpawnActor<APlayerController>();
    auto* Pawn=World->SpawnActor<ADefaultPawn>();
    Pawn->SetActorLocation(FVector(0,0,100)); PC->Possess(Pawn);
    auto* Input=NewObject<UEnhancedPlayerInput>(PC); PC->PlayerInput=Input;
    FActorSpawnParameters ProxyParams; ProxyParams.Owner=PC;
    auto* Proxy=World->SpawnActor<AKemPlayerControlProxy>(ProxyParams);
    auto* Aim=NewObject<UMotionControllerComponent>(Pawn); Aim->MotionSource=TEXT("RightAim");
    Aim->RegisterComponent(); Pawn->AddInstanceComponent(Aim);
    Aim->SetWorldLocation(FVector(0,0,100));
    auto* Grab=NewObject<UKemVRGrabInteractionComponent>(Pawn); Grab->RegisterComponent(); Pawn->AddInstanceComponent(Grab);
    Grab->RightAimMotionController=Aim; Grab->RightGripMotionController=Aim;
    auto* Brush=NewObject<UKemDrawingBoardInteractionComponent>(Pawn); Brush->RegisterComponent(); Pawn->AddInstanceComponent(Brush);
    Brush->RightAimMotionController=Aim;
    auto* Action=LoadObject<UInputAction>(nullptr,TEXT("/Game/KUB/Input/IA_KemUIRight.IA_KemUIRight"));
    if (!TestNotNull(TEXT("Continuous OpenXR trigger action exists"),Action)) return false;
    auto* GripContext=LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/KUB/Input/IMC_KemVRGrip.IMC_KemVRGrip"));
    if (!TestNotNull(TEXT("Grip has a persistent context separate from priority-1000 menu"),GripContext)) return false;
    TestTrue(TEXT("Both Vive grip bindings are registered"),GripContext->GetMappings().ContainsByPredicate([](const auto& M){return M.Key==EKeys::Vive_Left_Grip_Click;}) &&
        GripContext->GetMappings().ContainsByPredicate([](const auto& M){return M.Key==EKeys::Vive_Right_Grip_Click;}));
    // Permit ProcessEvent on test actors without starting an editor play session.
    FEditorScriptExecutionGuard ScriptExecution;
    TGuardValue<TEnumAsByte<EWorldType::Type>> GameWorld(World->WorldType,EWorldType::Game);
    auto Inject=[&](float Value)
    {
        Input->InjectInputForAction(Action,FInputActionValue(Value));
        Input->ProcessInputStack(TArray<UInputComponent*>(),.033f,false);
    };
    // OpenXR injects action values directly; raw Vive key states may stay zero.
    Inject(1.f);
    TestTrue(TEXT("Brush responds to enhanced-only trigger"),Brush->IsVRDrawInputActive(PC));
    TestEqual(TEXT("No raw Vive axis needed"),PC->GetInputAnalogKeyState(EKeys::Vive_Right_Trigger_Axis),0.f);
    auto* Board=World->SpawnActor<AKemDrawingBoardActor>();
    Board->SetActorLocationAndRotation(FVector(200,0,100),FRotator::ZeroRotator);
    World->UpdateWorldComponents(false,false);
    FHitResult BoardHit;
    const bool bBoardTrace=Brush->TraceBoardFromMotionController(BoardHit);
    TestTrue(FString::Printf(TEXT("Brush ray reaches board (hit=%s, start=%s, direction=%s)"),
        *GetNameSafe(BoardHit.GetActor()),*Brush->GetVRTraceStart().ToString(),*Brush->GetVRTraceDirection().ToString()),bBoardTrace && BoardHit.GetActor()==Board);
    FVector2D FirstUV;
    TestTrue(TEXT("Board hit resolves to drawing coordinates"),Board->ResolveHitToBoardUV(BoardHit,FirstUV));
    TestTrue(TEXT("Brush controller resolves correctly"),Brush->ResolvePlayerController()==PC);
    TestTrue(TEXT("Drawing proxy is discoverable"),AKemPlayerControlProxy::FindForController(PC)==Proxy);
    TestTrue(TEXT("Continuous trigger remains down before drawing"),Brush->IsVRDrawInputActive(PC));
    Board->RequestDraw(PC,FirstUV,true);
    TestEqual(FString::Printf(TEXT("Board request RPC reaches authority (role=%d, owner=%s, pawn=%s, uv=%s, time=%f)"),
        int32(Proxy->GetLocalRole()),*GetNameSafe(Proxy->GetOwner()),*GetNameSafe(PC->GetPawn()),*FirstUV.ToString(),World->GetTimeSeconds()),Board->GetSegmentCount(),1);
    Board->ClearBoard();
    World->Tick(LEVELTICK_All,.04f);
    Inject(1.f);
    Brush->UpdateVRDrawing();
    TestEqual(TEXT("VR brush creates first board segment"),Board->GetSegmentCount(),1);
    Aim->SetWorldLocation(FVector(0,10,100));
    World->Tick(LEVELTICK_All,.04f);
    Inject(1.f); Brush->UpdateVRDrawing();
    TestEqual(TEXT("Held trigger continues the stroke"),Board->GetSegmentCount(),2);
    Inject(0.f);
    TestFalse(TEXT("Brush stops on trigger release"),Brush->IsVRDrawInputActive(PC));
    auto* Occluder=World->SpawnActor<AStaticMeshActor>();
    Occluder->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Occluder->GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Occluder->SetActorLocation(FVector(100,0,100));
    Occluder->SetActorScale3D(FVector(.1f,3.f,3.f));
    World->UpdateWorldComponents(false,false);
    TestFalse(TEXT("Analytic drawing cannot pass through a wall"),Brush->TraceBoardFromMotionController(BoardHit));
    Occluder->Destroy();
    Board->Destroy();

    for (const TCHAR* Path : {TEXT("/Game/KUB/lobby/objects/Green_Apple_FBX.Green_Apple_FBX"),
                              TEXT("/Game/Weapons/Pistol/Meshes/SM_Pistol.SM_Pistol"),
                              TEXT("/Game/KUB/lobby/objects/skull.skull")})
    {
        auto* Object=World->SpawnActor<AStaticMeshActor>();
        auto* Mesh=Object->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Path));
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        Object->SetActorLocation(FVector(200,0,100));
        Object->SetActorLocation(Object->GetActorLocation()+FVector(200,0,100)-Mesh->Bounds.Origin);
        TestTrue(TEXT("Lobby prop is eligible"),Grab->IsLobbyProp(Mesh));
        World->UpdateWorldComponents(false,false);
        // Find a surface on the real imported mesh, accounting for empty space
        // in the pistol's trigger guard and the skull's eye sockets.
        bool bFound=false;
        const FBox Box=Mesh->Bounds.GetBox();
        for (int32 Y=0; Y<7 && !bFound; ++Y) for (int32 Z=0; Z<7 && !bFound; ++Z)
        {
            const FVector Point=Box.GetCenter()+FVector(0,(Y-3)*Box.GetExtent().Y/4,(Z-3)*Box.GetExtent().Z/4);
            Aim->SetWorldLocation(FVector(0,0,100));
            Aim->SetWorldRotation((Point-Aim->GetComponentLocation()).Rotation());
            bFound=Grab->TryRayGrab(UKemVRGrabInteractionComponent::EKemGrabHand::Right);
        }
        if (TestTrue(FString::Printf(TEXT("Ray acquires real prop %s"),Path),bFound))
        {
            const FVector Initial=Mesh->GetComponentLocation();
            Grab->UpdatePulledObject(UKemVRGrabInteractionComponent::EKemGrabHand::Right,.016f);
            TestTrue(TEXT("Pull is smooth, no instant teleport"),FVector::Dist(Initial,Mesh->GetComponentLocation())<=8.1f);
            Grab->HandleRightGrabReleased();
            TestTrue(TEXT("Grip release cannot drop a ray-latched prop"),Grab->IsHandOccupied(true));
            for (int32 I=0; I<120; ++I) Grab->UpdatePulledObject(UKemVRGrabInteractionComponent::EKemGrabHand::Right,.016f);
            TestFalse(TEXT("Pull reaches the hand"),Grab->RightGrabState.bPulling);
            auto* GripAction=LoadObject<UInputAction>(nullptr,TEXT("/Game/KUB/Input/IA_KemGripRight.IA_KemGripRight"));
            Input->InjectInputForAction(GripAction,FInputActionValue(1.f));
            Input->ProcessInputStack(TArray<UInputComponent*>(),.016f,false);
            Grab->PollGrip(UKemVRGrabInteractionComponent::EKemGrabHand::Right);
            TestTrue(TEXT("Grip press retains the pulled prop"),Grab->IsHandOccupied(true));
            TestFalse(TEXT("Grip takes over the latched hold"),Grab->RightGrabState.bRayLatched);
            Input->InjectInputForAction(GripAction,FInputActionValue(0.f));
            Input->ProcessInputStack(TArray<UInputComponent*>(),.016f,false);
            Grab->PollGrip(UKemVRGrabInteractionComponent::EKemGrabHand::Right);
            TestFalse(TEXT("Grip release frees the prop"),Grab->IsHandOccupied(true));
            TestEqual(TEXT("Release restores collision"),Mesh->GetCollisionEnabled(),ECollisionEnabled::QueryOnly);
            for (bool bRight : {true,false})
            {
                Grab->LeftGripMotionController=Aim;
                const auto Hand=bRight ? UKemVRGrabInteractionComponent::EKemGrabHand::Right : UKemVRGrabInteractionComponent::EKemGrabHand::Left;
                auto* NearAction=LoadObject<UInputAction>(nullptr,bRight ? TEXT("/Game/KUB/Input/IA_KemGripRight.IA_KemGripRight") : TEXT("/Game/KUB/Input/IA_KemGripLeft.IA_KemGripLeft"));
                Input->InjectInputForAction(NearAction,FInputActionValue(1.f));
                Input->ProcessInputStack(TArray<UInputComponent*>(),.016f,false);
                Grab->PollGrip(Hand);
                TestTrue(TEXT("Enhanced Grip directly grabs a nearby prop"),Grab->IsHandOccupied(bRight));
                Input->InjectInputForAction(NearAction,FInputActionValue(0.f));
                Input->ProcessInputStack(TArray<UInputComponent*>(),.016f,false);
                Grab->PollGrip(Hand);
                TestFalse(TEXT("Direct Grip release frees the prop"),Grab->IsHandOccupied(bRight));
            }
        }
        Object->Destroy();
    }
    auto* Wall=World->SpawnActor<AStaticMeshActor>();
    Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    TestFalse(TEXT("Scene walls are not eligible"),Grab->IsLobbyProp(Wall->GetStaticMeshComponent()));
    Wall->Destroy();
    auto* Engine=World->SpawnActor<AKemEngineAssembly>(); Engine->SetActorLocation(FVector(150,0,50)); Engine->EnsureParts();
    auto* Cover=Engine->FindPart(TEXT("DecorCover"));
    const FVector RayStart=Cover->GetActorLocation()+FVector(0,0,200);
    FTransform Ray((Cover->GetActorLocation()-RayStart).Rotation(),RayStart);
    FString Reason;
    TestFalse(TEXT("Normal grip still requires proximity"),Cover->TryAcquire(Proxy,true,Ray,Reason));
    TestTrue(TEXT("Verified ray can acquire distant cover"),Cover->TryAcquireFromRay(Proxy,true,Ray,Reason));
    TestFalse(TEXT("Ray cannot steal a held part"),Cover->TryAcquireFromRay(Proxy,false,Ray,Reason));
    Cover->Release(Proxy,true,Reason);
    Cover->ResetPart(true);
    FTransform Away(FRotator(0,0,0),RayStart);
    TestFalse(TEXT("Wrong ray direction is rejected"),Cover->TryAcquireFromRay(Proxy,true,Away,Reason));
    auto* Receiver=Engine->FindPart(TEXT("Receiver"));
    TestFalse(TEXT("Ray cannot bypass removal dependencies"),Receiver->TryAcquireFromRay(Proxy,true,Ray,Reason));
    Aim->SetWorldLocation(Cover->GetActorLocation()+FVector(0,0,3));
    auto* EngineGripAction=LoadObject<UInputAction>(nullptr,TEXT("/Game/KUB/Input/IA_KemGripRight.IA_KemGripRight"));
    Input->InjectInputForAction(EngineGripAction,FInputActionValue(1.f));
    Input->ProcessInputStack(TArray<UInputComponent*>(),.016f,false);
    Grab->PollGrip(UKemVRGrabInteractionComponent::EKemGrabHand::Right);
    TestTrue(TEXT("Enhanced Grip acquires nearby engine cover through proxy"),Cover->IsHeldBy(Pawn,true));
    Input->InjectInputForAction(EngineGripAction,FInputActionValue(0.f));
    Input->ProcessInputStack(TArray<UInputComponent*>(),.016f,false);
    Grab->PollGrip(UKemVRGrabInteractionComponent::EKemGrabHand::Right);
    TestFalse(TEXT("Grip release frees engine cover"),Cover->IsHeld());
    PC->Destroy(); Pawn->Destroy(); Engine->Destroy();
    return true;
}
#endif

