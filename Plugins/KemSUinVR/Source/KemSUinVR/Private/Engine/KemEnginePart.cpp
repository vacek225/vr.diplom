#include "Engine/KemEnginePart.h"

#include "Engine/KemEngineAssembly.h"
#include "Actors/KemPlayerControlProxy.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/KemEngineTrainingWidget.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AKemEnginePart::AKemEnginePart()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false); // Pose + lock + installed flag are one replicated snapshot.
    PrimaryActorTick.bCanEverTick = true;
    SetNetUpdateFrequency(30.0f);
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;
    Label = CreateDefaultSubobject<UWidgetComponent>(TEXT("PartName"));
    Label->SetupAttachment(SceneRoot);
    Label->SetRelativeLocation(FVector(0,0,14));
    Label->SetWidgetSpace(EWidgetSpace::World);
    Label->SetWidgetClass(UKemEnginePartLabelWidget::StaticClass());
    Label->SetDrawSize(FVector2D(850,140));
    Label->SetRelativeScale3D(FVector(0.045f));
    Label->SetTwoSided(true);
    Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label->SetVisibility(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Solid(TEXT("/Game/KUB/Engine2112/M_EngineTraining.M_EngineTraining"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ghost(TEXT("/Game/KUB/Engine2112/M_EngineTarget.M_EngineTarget"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Reveal(TEXT("/Game/KUB/Engine2112/M_EngineAssemblyXRay.M_EngineAssemblyXRay"));
    RevealMaterial=Reveal.Object;
    CubeMesh=Cube.Object; CylinderMesh=Cylinder.Object;
    SolidMaterial=Solid.Object; GhostMaterial=Ghost.Object;
}

void AKemEnginePart::BeginPlay() { Super::BeginPlay(); RebuildVisuals(); ApplyState(); }
void AKemEnginePart::Initialize(AKemEngineAssembly* Assembly, int32 Index)
{
    Engine=Assembly; PartIndex=Index;
    State.Pose=InstalledTransform();
    RebuildVisuals();
    ApplyState();
}

void AKemEnginePart::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AKemEnginePart, Engine);
    DOREPLIFETIME(AKemEnginePart, PartIndex);
    DOREPLIFETIME(AKemEnginePart, State);
}

FName AKemEnginePart::GetPartId() const
{
    return AKemEngineAssembly::Definitions().IsValidIndex(PartIndex) ? AKemEngineAssembly::Definitions()[PartIndex].Id : NAME_None;
}
FString AKemEnginePart::GetPartName() const
{
    return AKemEngineAssembly::Definitions().IsValidIndex(PartIndex) ? AKemEngineAssembly::Definitions()[PartIndex].Name : FString();
}
FTransform AKemEnginePart::InstalledTransform() const
{
    if (!Engine || !AKemEngineAssembly::Definitions().IsValidIndex(PartIndex)) return GetActorTransform();
    return FTransform(AKemEngineAssembly::Definitions()[PartIndex].Position)*Engine->GetActorTransform();
}
FTransform AKemEnginePart::TrayTransform() const
{
    // Labelled parts remain suspended on a stable grid; they cannot fall through a floor.
    return FTransform(FVector(-150.0f+(PartIndex%6)*60.0f, 75.0f+(PartIndex/6)*38.0f, -15.0f))*Engine->GetActorTransform();
}

UStaticMeshComponent* AKemEnginePart::AddShape(const FString& MeshName, const FVector& Position, const FVector& Size, const FRotator& Rotation)
{
    const FTransform Local(Rotation,Position,Size/100.0f);
    UStaticMesh* Mesh=MeshName==TEXT("Cylinder") ? CylinderMesh.Get() : CubeMesh.Get();
    auto Make = [&](bool bGhost)
    {
        UStaticMeshComponent* C=NewObject<UStaticMeshComponent>(this);
        AddInstanceComponent(C);
        C->SetupAttachment(SceneRoot);
        C->SetStaticMesh(Mesh);
        C->SetRelativeTransform(Local);
        C->SetMobility(EComponentMobility::Movable);
        C->SetCollisionEnabled(bGhost ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
        C->SetCollisionObjectType(ECC_WorldDynamic);
        C->SetCollisionResponseToAllChannels(ECR_Ignore);
        if (!bGhost)
        {
            C->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
            C->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Overlap);
        }
        C->SetGenerateOverlapEvents(false);
        C->SetCastShadow(!bGhost);
        C->SetVisibility(!bGhost);
        C->SetMaterial(0,bGhost ? TargetMaterial.Get() : PartMaterial.Get());
        C->RegisterComponent();
        return C;
    };
    UStaticMeshComponent* Solid=Make(false);
    Solids.Add(Solid); Ghosts.Add(Make(true)); GeometryTransforms.Add(Local);
    return Solid;
}

void AKemEnginePart::RebuildVisuals()
{
    if (bVisualsBuilt || !Engine || !AKemEngineAssembly::Definitions().IsValidIndex(PartIndex)) return;
    bVisualsBuilt=true;
    PartMaterial=UMaterialInstanceDynamic::Create(SolidMaterial ? SolidMaterial.Get() : UMaterial::GetDefaultMaterial(MD_Surface),this);
    PartMaterial->SetVectorParameterValue(TEXT("Tint"),AKemEngineAssembly::Definitions()[PartIndex].Color);
    // Per-surface PBR properties: non-metals must not share aluminium reflectance.
    float Metallic=.95f, Roughness=.48f, Variation=.08f, Bump=.55f, Brushed=0.f;
    switch(AKemEngineAssembly::Definitions()[PartIndex].Shape)
    {
    case 0: case 3: // Moulded plastic covers.
        Metallic=0.f; Roughness=.72f; Variation=.06f; Bump=.3f; break;
    case 2: case 4: // Insulation and timing-belt rubber.
        Metallic=0.f; Roughness=.88f; Variation=.05f; Bump=.4f; break;
    case 5: case 16: case 18: // Cast/forged steel.
        Metallic=.94f; Roughness=.46f; Bump=.45f; break;
    case 8: case 9: case 14: case 19: case 20: // Ground steel journals/valves.
        Metallic=1.f; Roughness=.25f; Variation=.05f; Bump=.22f; Brushed=1.f; break;
    case 10: // Heat-oxidised exhaust casting.
        Metallic=.30f; Roughness=.88f; Variation=.14f; Bump=.85f; break;
    case 12: // Dark composite head gasket.
        Metallic=.08f; Roughness=.85f; Variation=.1f; Bump=.15f; break;
    case 13: // Painted pressed-steel oil pan.
        Metallic=.04f; Roughness=.40f; Variation=.1f; Bump=.18f; break;
    case 17: // Machined piston assembly.
        Metallic=1.f; Roughness=.30f; Variation=.06f; Bump=.25f; Brushed=1.f; break;
    case 21: // Rough cast-iron block.
        Metallic=.88f; Roughness=.68f; Variation=.12f; Bump=.85f; break;
    default: break; // Cast aluminium receiver, head and housings.
    }
    PartMaterial->SetScalarParameterValue(TEXT("Metallic"),Metallic);
    PartMaterial->SetScalarParameterValue(TEXT("Roughness"),Roughness);
    PartMaterial->SetScalarParameterValue(TEXT("Variation"),Variation);
    PartMaterial->SetScalarParameterValue(TEXT("BumpStrength"),Bump);
    PartMaterial->SetScalarParameterValue(TEXT("Brushed"),Brushed);
    TargetMaterial=UMaterialInstanceDynamic::Create(GhostMaterial ? GhostMaterial.Get() : PartMaterial.Get(),this);
    if(RevealMaterial) LocalRevealMaterial=UMaterialInstanceDynamic::Create(RevealMaterial,this);
    BuildGeometry();
    Label->InitWidget();
    if (UKemEnginePartLabelWidget* W=Cast<UKemEnginePartLabelWidget>(Label->GetUserWidgetObject())) W->SetPartName(GetPartName());
}

void AKemEnginePart::BuildGeometry()
{
    auto Box=[&](FVector P,FVector S,FRotator R=FRotator::ZeroRotator){ AddShape(TEXT("Cube"),P,S,R); };
    auto Cyl=[&](FVector P,float Diameter,float Length,FRotator R=FRotator::ZeroRotator){ AddShape(TEXT("Cylinder"),P,{Diameter,Diameter,Length},R); };
    auto Ring=[&](FVector P,float Radius,float Thickness,float Height)
    {
        for (int32 I=0; I<20; ++I)
        {
            float A=I*2.0f*PI/20;
            Box(P+FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,0),{Thickness,Radius*0.33f,Height},FRotator(0,FMath::RadiansToDegrees(A),0));
        }
    };
    const int32 Shape=AKemEngineAssembly::Definitions()[PartIndex].Shape;
    switch (Shape)
    {
    case 0:
        Box({0,0,0},{50,28,3});
        for(int32 I=0;I<4;++I) Box({-18.0f+12*I,0,2},{7,25,1});
        break;
    case 1: // Plenum, throttle body and four bent intake runners.
        Cyl({0,2,5},13,48,{90,0,0});
        Cyl({-28,2,5},10,9,{90,0,0});
        for(int32 I=0;I<4;++I)
        {
            const float X=-19.5f+13*I;
            Cyl({X,-8,1},6,20,{0,0,65}); Cyl({X,-16,-7},6,15,{0,0,25});
        }
        break;
    case 2:
        Box({-23,14,-10},{10,8,9});
        for(int32 I=0;I<4;++I)
        {
            float X=-19.5f+13*I;
            Cyl({X,0,-1},2.0f,12); Cyl({X,7,4},1.5f,14,{0,0,90});
        }
        break;
    case 3:
        Box({0,0,0},{3,32,39}); Box({0,0,-21},{3,20,9}); break;
    case 4: // A schematic toothed loop, along the timing side (YZ plane).
        for(int32 I=0;I<24;++I)
        {
            float A=I*2*PI/24;
            Box({0,15*FMath::Cos(A),23*FMath::Sin(A)}, {2,4,2}, {FMath::RadiansToDegrees(A),0,0});
        }
        break;
    case 5:
        Cyl({0,0,0},15,2,{90,0,0}); Cyl({2,0,0},5,3,{90,0,0});
        for(int32 I=0;I<24;++I)
        {
            float A=I*2*PI/24;
            Box({0,7.7f*FMath::Cos(A),7.7f*FMath::Sin(A)}, {2.5f,1.5f,1.5f});
        }
        break;
    case 6:
        Box({0,-13,-4},{55,3,14}); Box({0,13,-4},{55,3,14});
        Box({-26,0,-4},{3,26,14}); Box({26,0,-4},{3,26,14});
        Box({0,-8,3},{53,7,2}); Box({0,8,3},{53,7,2});
        Cyl({-21,9,5},5,3); break;
    case 7:
        for(int32 I=0;I<5;++I) Box({-25.0f+12.5f*I,0,0},{3,28,3});
        Box({0,-13,0},{54,2,3}); Box({0,13,0},{54,2,3}); break;
    case 8:
        Cyl({0,0,0},2.8f,57,{90,0,0});
        for(int32 I=0;I<8;++I)
        {
            const float X=-22.0f+6*I;
            Cyl({X,0,0},5,2,{90,0,0}); Box({X,0,2},{2,3,4});
        }
        break;
    case 9:
        for(int32 X=-1;X<=1;X+=2) for(int32 Y=-1;Y<=1;Y+=2)
        {
            FVector P(X*2.7f,Y*6.0f,0);
            Cyl(P,0.9f,12); Cyl(P+FVector(0,0,-6),3.8f,0.8f);
            for(int32 J=0;J<4;++J) Cyl(P+FVector(0,0,2+J),3.0f,0.45f);
        }
        break;
    case 10:
        for(int32 I=0;I<4;++I) Cyl({-19.5f+13*I,0,0},5,17,{0,0,55});
        Cyl({0,-7,-9},7,48,{90,0,0}); break;
    case 11:
        Box({0,-14,0},{55,3,12}); Box({0,14,0},{55,3,12});
        for(int32 I=0;I<5;++I) Box({-26.0f+13*I,0,0},{3,27,12});
        for(int32 I=0;I<4;++I) Box({-19.5f+13*I,0,-5},{10,27,2});
        break;
    case 12:
        Box({0,-11,0},{56,3,0.6f}); Box({0,11,0},{56,3,0.6f});
        for(int32 I=0;I<5;++I) Box({-26.0f+13*I,0,0},{2,24,0.6f}); break;
    case 13:
        Box({0,0,-4},{55,28,2}); Box({0,-13,0},{55,2,10}); Box({0,13,0},{55,2,10});
        Box({-26,0,0},{2,28,10}); Box({26,0,0},{2,28,10}); break;
    case 14: Cyl({0,0,0},1.6f,24,{0,65,0}); Cyl({8,0,-3},9,2); break;
    case 15: Box({0,0,0},{4,19,19}); Cyl({3,0,0},9,4,{90,0,0}); break;
    case 16:
        for(int32 I=0;I<4;++I) Box({-19.5f+13*I,0,0},{5,9,3}); break;
    case 17:
        Cyl({0,0,4},8.2f,6); // Nominal 82 mm bore of the 1.5 L engine.
        for(int32 I=0;I<3;++I) Ring({0,0,5.0f+I},4.15f,0.3f,0.3f);
        Box({0,0,-7},{2,3,16});
        Cyl({0,0,-16},6,3,{0,0,90}); break;
    case 18:
        for(int32 I=0;I<5;++I) Box({-26.0f+13*I,0,0},{4,12,5}); break;
    case 19:
        Cyl({0,0,0},27,4,{90,0,0}); Cyl({-3,0,0},10,3,{90,0,0}); break;
    case 20:
        Cyl({0,0,0},4,61,{90,0,0});
        for(int32 I=0;I<4;++I)
        {
            float X=-19.5f+13*I; float Offset=(I==0 || I==3) ? 3.55f : -3.55f;
            Cyl({X,0,Offset},4,7,{90,0,0});
            Box({X-4,0,Offset/2},{2,8,11}); Box({X+4,0,Offset/2},{2,8,11});
        }
        break;
    case 21:
        Box({0,-13,-6},{56,3,40}); Box({0,13,-6},{56,3,40});
        Box({-27,0,-6},{3,27,40}); Box({27,0,-6},{3,27,40});
        for(int32 I=0;I<4;++I) Ring({-19.5f+13*I,0,0},4.7f,1.0f,27);
        for(int32 I=0;I<5;++I) Box({-26.0f+13*I,-15,-6},{2,3,40});
        break;
    default: break;
    }
}

bool AKemEnginePart::ValidatePose(const AKemPlayerControlProxy* Proxy, const FTransform& Pose) const
{
    const APlayerController* PC=Proxy ? Cast<APlayerController>(Proxy->GetOwner()) : nullptr;
    const APawn* Pawn=PC ? PC->GetPawn() : nullptr;
    return Engine && Pawn && !Pose.ContainsNaN() && Pose.GetRotation().IsNormalized()
        && FVector::DistSquared(Pose.GetLocation(),Pawn->GetActorLocation())<FMath::Square(400.0f)
        && FVector::DistSquared(Pose.GetLocation(),Engine->GetActorLocation())<FMath::Square(550.0f);
}

bool AKemEnginePart::IsOwnedByProxy(const AKemPlayerControlProxy* Proxy) const { return HolderProxy.Get()==Proxy && IsHeld(); }

bool AKemEnginePart::TryAcquire(AKemPlayerControlProxy* Proxy, bool bRightHand, const FTransform& Hand, FString& Reason)
{
    return AcquireValidated(Proxy,bRightHand,Hand,Reason,false);
}

bool AKemEnginePart::TryAcquireFromRay(AKemPlayerControlProxy* Proxy, bool bRightHand, const FTransform& Ray, FString& Reason)
{
    if (!HasAuthority() || !ValidatePose(Proxy,Ray) || !ValidatePose(Proxy,GetActorTransform()))
    { Reason=TEXT("Подойдите ближе к детали"); return false; }
    auto* PC=Cast<APlayerController>(Proxy->GetOwner());
    FCollisionQueryParams Query(SCENE_QUERY_STAT(KemEngineRayGrab),true,PC->GetPawn());
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit,Ray.GetLocation(),Ray.GetLocation()+Ray.GetRotation().GetForwardVector()*400.f,ECC_Visibility,Query) || Hit.GetActor()!=this)
    { Reason=TEXT("Наведите луч прямо на деталь"); return false; }
    return AcquireValidated(Proxy,bRightHand,Ray,Reason,true);
}

bool AKemEnginePart::AcquireValidated(AKemPlayerControlProxy* Proxy, bool bRightHand, const FTransform& Hand, FString& Reason, bool bFromRay)
{
    if (!HasAuthority() || !ValidatePose(Proxy,Hand)) { Reason=TEXT("Подойдите ближе к двигателю"); return false; }
    if (IsHeld()) { Reason=TEXT("Деталь уже держит другой участник или другая рука"); return false; }
    // Use actual mesh distance, so large covers can be picked up at their edges.
    float Closest=FVector::Distance(Hand.GetLocation(),GetActorLocation());
    for (const UStaticMeshComponent* C : Solids)
    {
        FVector Point;
        const float Distance=C->GetClosestPointOnCollision(Hand.GetLocation(),Point);
        if (Distance>=0) Closest=FMath::Min(Closest,Distance);
    }
    if (!bFromRay && Closest>30.0f) { Reason=TEXT("Поднесите руку к детали"); return false; }
    if (State.bInstalled)
    {
        if (Engine->GetLesson()==EKemEngineLesson::Assemble) { Reason=TEXT("Режим сборки: устанавливайте снятые детали"); return false; }
        if (!Engine->CanRemove(this,Reason)) return false;
        if (Engine->GetLesson()==EKemEngineLesson::Disassemble && !Engine->IsNextPart(this))
        { Reason=TEXT("Следуйте подсказке на панели двигателя"); return false; }
    }
    APawn* InteractorPawn=Cast<APlayerController>(Proxy->GetOwner())->GetPawn();
    if(State.PlacementPawn!=InteractorPawn) State.PlacementPawn=nullptr;
    HolderProxy=Proxy;
    State.HolderPawn=InteractorPawn;
    State.bRightHand=bRightHand;
    State.bInstalled=false;
    LastPoseTime=GetWorld()->GetTimeSeconds();
    ForceNetUpdate();
    return true;
}

bool AKemEnginePart::MoveHeld(AKemPlayerControlProxy* Proxy, bool bRightHand, const FTransform& Pose)
{
    if (!HasAuthority() || !IsOwnedByProxy(Proxy) || State.bRightHand!=bRightHand || !ValidatePose(Proxy,Pose)) return false;
    State.Pose=FTransform(Pose.GetRotation(),Pose.GetLocation(),InstalledTransform().GetScale3D());
    LastPoseTime=GetWorld()->GetTimeSeconds();
    ApplyState();
    if(State.PlacementPawn && !IsInsideConstruction()) State.PlacementPawn=nullptr;
    return true;
}

bool AKemEnginePart::CanSnapAt(const FTransform& Pose,FString& Reason) const
{
    Reason.Empty();
    if(!IsValid(Engine)) return false;
    const FTransform Target=InstalledTransform();
    if(FVector::DistSquared(Target.GetLocation(),Pose.GetLocation())>FMath::Square(Engine->SnapDistance))
    { Reason=TEXT("Поднесите деталь ближе к подсвеченному месту"); return false; }
    const float Angle=FMath::RadiansToDegrees(Target.GetRotation().AngularDistance(Pose.GetRotation()));
    if(Angle>Engine->SnapAngleDegrees)
    { Reason=TEXT("Поверните деталь ближе к подсвеченному образцу"); return false; }
    if(Engine->GetLesson()==EKemEngineLesson::Disassemble ||
       (Engine->GetLesson()==EKemEngineLesson::Assemble && !Engine->IsNextPart(this)))
    { Reason=TEXT("Проверьте порядок сборки в меню"); return false; }
    return Engine->CanInstall(this,Reason);
}

bool AKemEnginePart::Release(AKemPlayerControlProxy* Proxy, bool bRightHand, FString& Reason)
{
    if (!HasAuthority() || !IsOwnedByProxy(Proxy) || State.bRightHand!=bRightHand) return false;
    State.PlacementPawn=nullptr;
    if(CanSnapAt(State.Pose,Reason))
    {
        // Forgiving acquisition zone, exact final pose: rotation and translation
        // are corrected together by the server, with the same hint on clients.
        State.bInstalled=true; State.Pose=InstalledTransform(); Reason=TEXT("Установлено: ")+GetPartName();
    }
    else if(Engine->GetLesson()==EKemEngineLesson::Disassemble)
    {
        if(FVector::DistSquared(InstalledTransform().GetLocation(),State.Pose.GetLocation())<=FMath::Square(Engine->SnapDistance))
            State.Pose=TrayTransform();
        Reason=TEXT("Снято: ")+GetPartName();
    }
    else
    {
        // Keep the actual dropped pose, so a failed placement can be picked up
        // and corrected in place instead of silently teleporting to the tray.
        if(IsInsideConstruction()) State.PlacementPawn=State.HolderPawn;
    }
    State.HolderPawn=nullptr; HolderProxy.Reset();
    ApplyState(); ForceNetUpdate();
    return true;
}

void AKemEnginePart::ResetPart(bool bInstalled)
{
    if (!HasAuthority() || IsHeld()) return;
    State.PlacementPawn=nullptr;
    State.bInstalled=bInstalled;
    State.Pose=bInstalled ? InstalledTransform() : TrayTransform();
    ApplyState(); ForceNetUpdate();
}

void AKemEnginePart::OnRep_State()
{
    if (IsHeld()) SetActorScale3D(State.Pose.GetScale3D());
    else ApplyState();
}
void AKemEnginePart::ApplyState() { SetActorTransform(State.Pose); }

FBox AKemEnginePart::SolidBounds() const
{
    FBox Box(ForceInit);
    for(const UStaticMeshComponent* Component:Solids)
        if(IsValid(Component)) Box+=Component->Bounds.GetBox();
    return Box;
}

bool AKemEnginePart::IsInsideConstruction() const
{
    if(!IsValid(Engine) || IsInstalled()) return false;
    FBox Construction(ForceInit);
    for(const AKemEnginePart* Part:Engine->Parts)
        if(IsValid(Part) && Part!=this && Part->IsInstalled() && !Part->IsHeld())
            Construction+=Part->SolidBounds();
    const FBox Bounds=SolidBounds();
    // The envelope deliberately includes hollow interiors (cylinders, crankcase),
    // where a collision-overlap check would miss a dropped internal part.
    return Construction.IsValid && Bounds.IsValid && Construction.ExpandBy(3.f).IsInsideOrOn(Bounds.GetCenter());
}

void AKemEnginePart::UpdateAssemblyTransparency(float DeltaSeconds)
{
    if(!bVisualsBuilt || !LocalRevealMaterial || GetNetMode()==NM_DedicatedServer) return;
    bool bNearLocalHand=false;
    if(IsValid(Engine) && Engine->bAssemblyRevealEnabled && IsInstalled() && !IsHeld() &&
       Engine->GetLesson()!=EKemEngineLesson::Disassemble)
    {
        // Hysteresis prevents flicker when the hand rests near the threshold.
        const float Range=FMath::Max(0.f,Engine->AssemblyRevealDistance)+(bRevealRequested?6.f:0.f);
        const FBox Bounds=SolidBounds();
        for(const AKemEnginePart* Held:Engine->Parts)
        {
            if(!IsValid(Held) || Held==this) continue;
            // One unresolved local drop keeps the entire installed construction
            // visible through. Multiple dropped parts are resolved independently.
            if(!Held->IsInstalled() && IsValid(Held->State.PlacementPawn) &&
               Held->State.PlacementPawn->IsLocallyControlled() && Held->IsInsideConstruction())
            { bNearLocalHand=true; break; }
            if(!Held->IsHeld() || !IsValid(Held->State.HolderPawn) ||
               !Held->State.HolderPawn->IsLocallyControlled()) continue;
            const FBox HeldBounds=Held->SolidBounds();
            if(!Bounds.IsValid || !HeldBounds.IsValid || Bounds.ComputeSquaredDistanceToBox(HeldBounds)>FMath::Square(Range)) continue;
            // Test actual solid components, excluding distant target ghosts and labels.
            for(const UStaticMeshComponent* Component:Solids)
                if(IsValid(Component) && Component->Bounds.GetBox().ComputeSquaredDistanceToBox(HeldBounds)<=FMath::Square(Range))
                { bNearLocalHand=true; break; }
            if(bNearLocalHand) break;
        }
    }
    bRevealRequested=bNearLocalHand;
    const float Target=bNearLocalHand?FMath::Clamp(Engine->AssemblyRevealOpacity,.05f,.6f):1.f;
    const bool bWasTransparent=IsLocallyTransparent();
    LocalRevealOpacity=IsHeld()?1.f:FMath::FInterpConstantTo(LocalRevealOpacity,Target,DeltaSeconds,5.f);
    const bool bTransparent=IsLocallyTransparent();
    if(bTransparent!=bWasTransparent)
        for(UStaticMeshComponent* Component:Solids)
        {
            Component->SetMaterial(0,bTransparent?LocalRevealMaterial.Get():PartMaterial.Get());
            Component->SetCastShadow(!bTransparent);
        }
    if(bTransparent)
    {
        LocalRevealMaterial->SetScalarParameterValue(TEXT("RevealOpacity"),LocalRevealOpacity);
        const FLinearColor Base=AKemEngineAssembly::Definitions()[PartIndex].Color;
        LocalRevealMaterial->SetVectorParameterValue(TEXT("Tint"),FMath::Lerp(Base,FLinearColor(.12f,.38f,.48f),.35f));
    }
}

void AKemEnginePart::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateAssemblyTransparency(DeltaSeconds);
    if (!Engine || !bVisualsBuilt) return;
    if (!HasAuthority() && IsHeld() && !State.HolderPawn->IsLocallyControlled())
        SetActorLocationAndRotation(FMath::VInterpTo(GetActorLocation(),State.Pose.GetLocation(),DeltaSeconds,22.0f),
            FMath::QInterpTo(GetActorQuat(),State.Pose.GetRotation(),DeltaSeconds,22.0f));
    if (HasAuthority() && IsHeld() && (!HolderProxy.IsValid() || !IsValid(State.HolderPawn) || GetWorld()->GetTimeSeconds()-LastPoseTime>1.5f))
    {
        State.HolderPawn=nullptr; State.PlacementPawn=nullptr; HolderProxy.Reset(); State.Pose=TrayTransform();
        ApplyState(); ForceNetUpdate();
    }
    const bool bNext=Engine->GetLesson()!=EKemEngineLesson::Free && Engine->IsNextPart(this);
    const bool bShowGhost=!State.bInstalled && (IsHeld() || bLocalFocus || bNext);
    FString Reason;
    const bool bCanSnap=Engine->CanInstall(this,Reason) && Engine->GetLesson()!=EKemEngineLesson::Disassemble &&
        (Engine->GetLesson()!=EKemEngineLesson::Assemble || bNext);
    const bool bAligned=CanSnapAt(GetActorTransform(),Reason);
    const FLinearColor TargetColor=bCanSnap ? (bAligned ? FLinearColor(0.1f,1,0.25f) : FLinearColor(0,0.65f,1)) : FLinearColor(1,0.15f,0.05f);
    TargetMaterial->SetVectorParameterValue(TEXT("Tint"),TargetColor);
    const FLinearColor Base=AKemEngineAssembly::Definitions()[PartIndex].Color;
    PartMaterial->SetVectorParameterValue(TEXT("Tint"),bLocalFocus || bNext ? FMath::Lerp(Base,FLinearColor(0.1f,0.8f,1),0.5f) : Base);
    for(int32 I=0;I<Ghosts.Num();++I)
    {
        Ghosts[I]->SetVisibility(bShowGhost);
        if(bShowGhost) Ghosts[I]->SetWorldTransform(GeometryTransforms[I]*InstalledTransform());
    }
    APlayerController* PC=GetWorld()->GetFirstPlayerController();
    FVector Eye; FRotator Rotation;
    if(PC)
    {
        PC->GetPlayerViewPoint(Eye,Rotation);
        const bool bNearby=FVector::DistSquared(Eye,GetActorLocation())<FMath::Square(350.0f);
        Label->SetVisibility(bNearby && (bLocalFocus || IsHeld() || bNext));
        Label->SetWorldRotation((Eye-Label->GetComponentLocation()).Rotation());
    }
}
