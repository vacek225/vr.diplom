#include "Actors/KemPointerActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AKemPointerActor::AKemPointerActor()
{
    PrimaryActorTick.bCanEverTick = true; bReplicates = true; bAlwaysRelevant = true;
    SetNetUpdateFrequency(20.f);
    SetReplicateMovement(false);
    Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam")); SetRootComponent(Beam);
    Dot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dot")); Dot->SetupAttachment(Beam);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Beam->SetStaticMesh(Cube.Object); Dot->SetStaticMesh(Sphere.Object);
    for (auto* Mesh : {Beam.Get(),Dot.Get()})
    { Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCastShadow(false); Mesh->SetHiddenInGame(true); }
}
void AKemPointerActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AKemPointerActor,TraceStart); DOREPLIFETIME(AKemPointerActor,TraceEnd); DOREPLIFETIME(AKemPointerActor,bActive);
}
void AKemPointerActor::UpdatePointer(FVector Start, FVector End, bool bEnabled)
{
    if (!HasAuthority()) return;
    TraceStart=Start; TraceEnd=End; bActive=bEnabled; LastUpdate=GetWorld()->GetTimeSeconds(); ForceNetUpdate();
}
void AKemPointerActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (HasAuthority() && (!IsValid(GetOwner()) || !IsValid(GetOwner()->GetOwner()) || GetWorld()->GetTimeSeconds()-LastUpdate>.4f))
    { bActive=false; if (!IsValid(GetOwner()) || !IsValid(GetOwner()->GetOwner())) { Destroy(); return; } }
    Beam->SetHiddenInGame(!bActive); Dot->SetHiddenInGame(!bActive);
    if (!bActive) return;
    const FVector Start=TraceStart, End=TraceEnd;
    const float Length=FVector::Distance(Start,End);
    Beam->SetWorldLocation((Start+End)*.5f); Beam->SetWorldRotation((End-Start).Rotation());
    Beam->SetWorldScale3D(FVector(Length/100.f,.004f,.004f));
    Dot->SetWorldLocation(End); Dot->SetWorldScale3D(FVector(.025f));
}

void AKemPointerActor::BeginPlay()
{
    Super::BeginPlay();
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if(Material)
        for(auto* Mesh : {Beam.Get(),Dot.Get()})
        {
            auto* Dynamic=UMaterialInstanceDynamic::Create(Material,this);
            Dynamic->SetVectorParameterValue(TEXT("Color"),FLinearColor(.04f,.8f,1.f));
            Mesh->SetMaterial(0,Dynamic);
        }
}
