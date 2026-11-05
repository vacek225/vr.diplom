#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/KemEngineAssembly.h"
#include "Engine/KemEnginePart.h"
#include "Actors/KemPlayerControlProxy.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKemEngineDefinitionsTest,"KemSUinVR.Engine2112.Dependencies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKemEngineDefinitionsTest::RunTest(const FString& Parameters)
{
    const auto& D=AKemEngineAssembly::Definitions();
    TSet<FName> Ids;
    for(const auto& V : D)
    {
        TestFalse(TEXT("Unique part identifier"),Ids.Contains(V.Id)); Ids.Add(V.Id);
        TestFalse(TEXT("Named part"),V.Name.IsEmpty());
    }
    TSet<FName> Removed;
    while(Removed.Num()<D.Num())
    {
        bool Progress=false;
        for(const auto& V : D)
        {
            for(FName Blocker : V.RemovalBlockers) TestTrue(TEXT("Dependency exists"),Ids.Contains(Blocker));
            if(Removed.Contains(V.Id)) continue;
            bool Ready=true;
            for(FName Blocker : V.RemovalBlockers) Ready &= Removed.Contains(Blocker);
            if(Ready) { Removed.Add(V.Id); Progress=true; }
        }
        if(!Progress) { AddError(TEXT("Dependency cycle prevents complete disassembly")); break; }
    }
    TestEqual(TEXT("Four piston assemblies"),D.FilterByPredicate([](const auto& V){ return V.Shape==17; }).Num(),4);
    TestEqual(TEXT("Four groups of four valves"),D.FilterByPredicate([](const auto& V){ return V.Shape==9; }).Num(),4);
    TestEqual(TEXT("Two camshafts"),D.FilterByPredicate([](const auto& V){ return V.Shape==8; }).Num(),2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKemEngineInteractionTest,"KemSUinVR.Engine2112.AuthorityAndLessons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKemEngineInteractionTest::RunTest(const FString& Parameters)
{
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
    if(!TestNotNull(TEXT("Test world"),World)) return false;
    AKemEngineAssembly* Engine=World->SpawnActor<AKemEngineAssembly>();
    Engine->EnsureParts();
    TestEqual(TEXT("All parts spawned"),Engine->Parts.Num(),AKemEngineAssembly::Definitions().Num());
    for (const AKemEnginePart* P : Engine->Parts)
        TestTrue(TEXT("Initial part pose matches its socket, including editor previews"),P->GetActorTransform().Equals(P->InstalledTransform(),0.01f));
    auto MakeProxy=[&]()
    {
        APlayerController* PC=World->SpawnActor<APlayerController>();
        ADefaultPawn* Pawn=World->SpawnActor<ADefaultPawn>();
        Pawn->SetActorLocation(FVector(-120,0,0)); PC->Possess(Pawn);
        FActorSpawnParameters Params; Params.Owner=PC;
        return World->SpawnActor<AKemPlayerControlProxy>(Params);
    };
    AKemPlayerControlProxy* A=MakeProxy();
    AKemPlayerControlProxy* B=MakeProxy();
    AKemEnginePart* Cover=Engine->FindPart(TEXT("DecorCover"));
    AKemEnginePart* Receiver=Engine->FindPart(TEXT("Receiver"));
    FString Reason;
    TestFalse(TEXT("Cover blocks receiver"),Receiver->TryAcquire(A,true,Receiver->GetActorTransform(),Reason));
    TestTrue(TEXT("First participant acquires cover"),Cover->TryAcquire(A,true,Cover->GetActorTransform(),Reason));
    TestFalse(TEXT("Second participant cannot acquire held part"),Cover->TryAcquire(B,true,Cover->GetActorTransform(),Reason));
    TestFalse(TEXT("Reset cannot interrupt a grab"),Engine->ConfigureLesson(EKemEngineLesson::Free,true));
    TestFalse(TEXT("Other participant cannot move part"),Cover->MoveHeld(B,true,Cover->TrayTransform()));
    TestFalse(TEXT("Other hand cannot release lock"),Cover->Release(A,false,Reason));
    TestFalse(TEXT("Other participant cannot release part"),Cover->Release(B,true,Reason));
    FTransform Far=Cover->TrayTransform(); Far.SetLocation(FVector(10000,0,0));
    TestFalse(TEXT("Out-of-reach movement rejected"),Cover->MoveHeld(A,true,Far));
    Far=Cover->InstalledTransform(); Far.SetLocation(FVector(std::numeric_limits<double>::quiet_NaN(),0,0));
    TestFalse(TEXT("Non-finite pose rejected"),Cover->MoveHeld(A,true,Far));
    TestTrue(TEXT("Holder moves part to tray"),Cover->MoveHeld(A,true,Cover->TrayTransform()));
    Cover->Release(A,true,Reason);
    TestTrue(TEXT("Receiver becomes accessible"),Engine->CanRemove(Receiver,Reason));
    Receiver->TryAcquire(A,true,Receiver->GetActorTransform(),Reason);
    Receiver->MoveHeld(A,true,Receiver->TrayTransform()); Receiver->Release(A,true,Reason);
    TestFalse(TEXT("Outer cover cannot install over missing receiver"),Engine->CanInstall(Cover,Reason));
    Receiver->TryAcquire(A,true,Receiver->GetActorTransform(),Reason);
    Receiver->MoveHeld(A,true,Receiver->InstalledTransform()); Receiver->Release(A,true,Reason);
    // Placement must satisfy both angular and positional tolerances.
    Cover->TryAcquire(A,true,Cover->GetActorTransform(),Reason);
    FTransform WrongAngle=Cover->InstalledTransform(); WrongAngle.SetRotation(FRotator(0,90,0).Quaternion());
    Cover->MoveHeld(A,true,WrongAngle); Cover->Release(A,true,Reason);
    TestFalse(TEXT("Wrong rotation does not snap"),Cover->IsInstalled());
    Cover->TryAcquire(A,true,Cover->GetActorTransform(),Reason);
    FTransform TooFar=Cover->InstalledTransform(); TooFar.AddToTranslation(FVector(Engine->SnapDistance+1,0,0));
    Cover->MoveHeld(A,true,TooFar); Cover->Release(A,true,Reason);
    TestFalse(TEXT("Outside distance tolerance does not snap"),Cover->IsInstalled());
    Cover->TryAcquire(A,true,Cover->GetActorTransform(),Reason);
    Cover->MoveHeld(A,true,Cover->InstalledTransform()); Cover->Release(A,true,Reason);
    TestTrue(TEXT("Correct pose snaps into place"),Cover->IsInstalled());

    TestTrue(TEXT("Begin disassembly lesson"),Engine->ConfigureLesson(EKemEngineLesson::Disassemble,true));
    TestFalse(TEXT("Guided mode enforces next step"),Engine->FindPart(TEXT("Exhaust"))->TryAcquire(A,true,Engine->FindPart(TEXT("Exhaust"))->GetActorTransform(),Reason));
    int32 Removed=0;
    while(AKemEnginePart* P=Engine->NextPart())
    {
        if(!P->TryAcquire(A,true,P->GetActorTransform(),Reason)) { AddError(Reason); break; }
        if(!P->MoveHeld(A,true,P->TrayTransform())) { AddError(TEXT("Tray out of reach")); break; }
        P->Release(A,true,Reason);
        TestFalse(TEXT("Lesson removes next part"),P->IsInstalled());
        if(++Removed>Engine->Parts.Num()) { AddError(TEXT("Lesson failed to advance")); break; }
    }
    TestEqual(TEXT("Complete disassembly"),Removed,Engine->Parts.Num());
    TestTrue(TEXT("Begin assembly lesson"),Engine->ConfigureLesson(EKemEngineLesson::Assemble,true));
    int32 Installed=0;
    while(AKemEnginePart* P=Engine->NextPart())
    {
        if(!P->TryAcquire(B,false,P->GetActorTransform(),Reason)) { AddError(Reason); break; }
        P->MoveHeld(B,false,P->InstalledTransform()); P->Release(B,false,Reason);
        if(!TestTrue(TEXT("Next part installs"),P->IsInstalled())) { AddError(Reason); break; }
        if(++Installed>Engine->Parts.Num()) { AddError(TEXT("Assembly failed to advance")); break; }
    }
    TestEqual(TEXT("Complete reassembly"),Installed,Engine->Parts.Num());
    const TArray<TObjectPtr<AKemEnginePart>> Parts=Engine->Parts;
    Engine->Destroy();
    // Editor test worlds do not dispatch BeginPlay/EndPlay; destroy children explicitly.
    for(AKemEnginePart* P : Parts) if(IsValid(P)) P->Destroy();
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKemEngineRevealTest,"KemSUinVR.Engine2112.LocalAssemblyTransparency",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKemEngineRevealTest::RunTest(const FString& Parameters)
{
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* Engine=World->SpawnActor<AKemEngineAssembly>(); Engine->EnsureParts();
    auto* OtherEngine=World->SpawnActor<AKemEngineAssembly>(); OtherEngine->EnsureParts();
    auto* PC=World->SpawnActor<APlayerController>();
    // An editor test world has no NetDriver: identify this controller with a
    // real LocalPlayer, just as the standalone runtime does.
    PC->Player=NewObject<ULocalPlayer>(GEngine);
    auto* Pawn=World->SpawnActor<ADefaultPawn>(); Pawn->SetActorLocation(FVector(-100,0,0)); PC->Possess(Pawn);
    TestTrue(TEXT("Test hand belongs to a local player"),Pawn->IsLocallyControlled());
    FActorSpawnParameters Params; Params.Owner=PC;
    auto* Proxy=World->SpawnActor<AKemPlayerControlProxy>(Params);
    auto* Cover=Engine->FindPart(TEXT("DecorCover"));
    auto* Receiver=Engine->FindPart(TEXT("Receiver"));
    auto* Foreign=OtherEngine->FindPart(TEXT("Receiver"));
    TInlineComponentArray<UStaticMeshComponent*> Meshes(Receiver);
    UStaticMeshComponent* ReceiverMesh=nullptr;
    for(UStaticMeshComponent* Component:Meshes)
        if(Component->GetCollisionEnabled()==ECollisionEnabled::QueryOnly) { ReceiverMesh=Component; break; }
    if(!TestNotNull(TEXT("Receiver has solid geometry"),ReceiverMesh)) return false;
    UMaterialInterface* OriginalMaterial=ReceiverMesh->GetMaterial(0);
    FString Reason;
    TestTrue(TEXT("Local hand can acquire cover"),Cover->TryAcquire(Proxy,true,Cover->GetActorTransform(),Reason));
    Receiver->Tick(.3f); Cover->Tick(.1f); Foreign->Tick(.3f);
    TestTrue(TEXT("Nearby installed geometry becomes transparent"),Receiver->IsLocallyTransparent());
    TestTrue(TEXT("Local reveal changes the actual render material"),ReceiverMesh->GetMaterial(0)!=OriginalMaterial);
    TestFalse(TEXT("Held part stays opaque"),Cover->IsLocallyTransparent());
    TestFalse(TEXT("Other engines stay opaque"),Foreign->IsLocallyTransparent());
    bool bSolidStillCollides=false;
    for(UStaticMeshComponent* Component:Meshes)
        bSolidStillCollides|=Component->GetCollisionEnabled()==ECollisionEnabled::QueryOnly;
    TestTrue(TEXT("Transparency preserves grab/trace collision"),bSolidStillCollides);
    Cover->MoveHeld(Proxy,true,Cover->TrayTransform()); Receiver->Tick(.3f);
    TestFalse(TEXT("Moving away restores opaque material"),Receiver->IsLocallyTransparent());
    TestTrue(TEXT("Original PBR material is restored"),ReceiverMesh->GetMaterial(0)==OriginalMaterial);
    Cover->MoveHeld(Proxy,true,Cover->InstalledTransform()); Receiver->Tick(.3f);
    TestTrue(TEXT("Approaching again reveals target"),Receiver->IsLocallyTransparent());
    Engine->bAssemblyRevealEnabled=false; Receiver->Tick(.3f);
    TestFalse(TEXT("Disabling assistance restores material"),Receiver->IsLocallyTransparent());
    Engine->bAssemblyRevealEnabled=true; Receiver->Tick(.3f);
    Cover->Release(Proxy,true,Reason); Receiver->Tick(.3f);
    TestFalse(TEXT("Releasing restores opacity"),Receiver->IsLocallyTransparent());
    // Failed placement keeps the actual pose and reveals even distant installed
    // pieces of this construction, only for the participant who dropped it.
    auto* Sump=Engine->FindPart(TEXT("Sump"));
    Cover->TryAcquire(Proxy,true,Cover->GetActorTransform(),Reason);
    FTransform Rejected=Cover->InstalledTransform(); Rejected.SetRotation(FRotator(0,90,0).Quaternion());
    Cover->MoveHeld(Proxy,true,Rejected); Cover->Release(Proxy,true,Reason);
    TestFalse(TEXT("Large angle is still rejected"),Cover->IsInstalled());
    TestTrue(TEXT("Failed drop stays at released pose"),Cover->GetActorTransform().Equals(Rejected,.01f));
    Receiver->Tick(.3f); Sump->Tick(.3f); Foreign->Tick(.3f); Cover->Tick(.3f);
    TestTrue(TEXT("Failed inside drop keeps nearby construction transparent"),Receiver->IsLocallyTransparent());
    TestTrue(TEXT("Failed inside drop reveals the entire installed construction"),Sump->IsLocallyTransparent());
    TestFalse(TEXT("Dropped part remains visible"),Cover->IsLocallyTransparent());
    TestFalse(TEXT("Failed placement does not affect another engine"),Foreign->IsLocallyTransparent());
    Cover->TryAcquire(Proxy,true,Cover->GetActorTransform(),Reason); Sump->Tick(.3f);
    TestTrue(TEXT("Regrabbing inside keeps the construction revealed"),Sump->IsLocallyTransparent());
    Cover->MoveHeld(Proxy,true,Cover->TrayTransform()); Cover->Release(Proxy,true,Reason);
    Receiver->Tick(.3f); Sump->Tick(.3f);
    TestFalse(TEXT("Removing loose part clears persistent transparency"),Sump->IsLocallyTransparent());
    TestFalse(TEXT("Outside drop does not retain transparency"),Receiver->IsLocallyTransparent());
    Cover->TryAcquire(Proxy,true,Cover->GetActorTransform(),Reason);
    FTransform Assisted=Cover->InstalledTransform(); Assisted.AddToTranslation(FVector(10,7,3));
    Assisted.SetRotation(FRotator(0,40,0).Quaternion());
    TestTrue(TEXT("Forgiving pose is offered by hint and snap helper"),Cover->CanSnapAt(Assisted,Reason));
    Cover->MoveHeld(Proxy,true,Assisted); Cover->Release(Proxy,true,Reason);
    TestTrue(TEXT("Approximate position and angle snap"),Cover->IsInstalled());
    TestTrue(TEXT("Assistance corrects to exact socket pose"),Cover->GetActorTransform().Equals(Cover->InstalledTransform(),.01f));
    Engine->ConfigureLesson(EKemEngineLesson::Disassemble,true);
    Cover->TryAcquire(Proxy,false,Cover->GetActorTransform(),Reason); Receiver->Tick(.3f);
    TestFalse(TEXT("Disassembly keeps original appearance"),Receiver->IsLocallyTransparent());
    Cover->Release(Proxy,false,Reason);
    Engine->ConfigureLesson(EKemEngineLesson::Assemble,true);
    auto* Block=Engine->FindPart(TEXT("Block")); Block->ResetPart(true);
    auto* Crank=Engine->FindPart(TEXT("Crankshaft"));
    TestTrue(TEXT("Assembly hand acquires next part"),Crank->TryAcquire(Proxy,false,Crank->GetActorTransform(),Reason));
    FTransform WrongCrank=Crank->InstalledTransform(); WrongCrank.SetRotation(FRotator(0,90,0).Quaternion());
    Crank->MoveHeld(Proxy,false,WrongCrank); Crank->Release(Proxy,false,Reason); Block->Tick(.3f);
    TestFalse(TEXT("Unaligned assembly drop is not installed"),Crank->IsInstalled());
    TestTrue(TEXT("Assembly drop keeps support revealed"),Block->IsLocallyTransparent());
    Crank->TryAcquire(Proxy,false,Crank->GetActorTransform(),Reason);
    FTransform AssistedCrank=Crank->InstalledTransform(); AssistedCrank.AddToTranslation(FVector(10,7,3));
    AssistedCrank.SetRotation(FRotator(0,40,0).Quaternion());
    Crank->MoveHeld(Proxy,false,AssistedCrank); Block->Tick(.3f);
    TestTrue(TEXT("Assembly exercise reveals the installed block"),Block->IsLocallyTransparent());
    Crank->Release(Proxy,false,Reason); Block->Tick(.3f);
    TestTrue(TEXT("Transparent support does not affect snapping"),Crank->IsInstalled());
    TestFalse(TEXT("Successful snap restores support opacity"),Block->IsLocallyTransparent());
    for(auto* Assembly:{Engine,OtherEngine})
    {
        const auto Parts=Assembly->Parts; Assembly->Destroy();
        for(AKemEnginePart* Part:Parts) if(IsValid(Part)) Part->Destroy();
    }
    return true;
}
#endif

