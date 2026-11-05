#include "Engine/KemEngineAssembly.h"

#include "Engine/KemEnginePart.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

const TArray<FKemEnginePartDefinition>& AKemEngineAssembly::Definitions()
{
    // X is the crankshaft axis, Z points up. Dependencies describe an educational
    // major-assembly sequence, not every fastener/tool operation in the manual.
    static const TArray<FKemEnginePartDefinition> Data = []
    {
        TArray<FKemEnginePartDefinition> D;
        const FLinearColor Alloy(0.58f, 0.60f, 0.62f), Dark(0.012f, 0.014f, 0.016f);
        auto Add = [&](const TCHAR* Id, const FString& Name, int32 Shape, FVector P,
            TArray<FName> Blockers, FLinearColor Color, int32 Variant = 0)
        {
            FKemEnginePartDefinition V;
            V.Id = Id; V.Name = Name; V.Shape = Shape; V.Position = P;
            V.RemovalBlockers = MoveTemp(Blockers); V.Color = Color; V.Variant = Variant;
            D.Add(MoveTemp(V));
        };
        Add(TEXT("DecorCover"), TEXT("Декоративная крышка ВАЗ-2112 • 16V"), 0, {0,0,57}, {}, Dark);
        Add(TEXT("Receiver"), TEXT("Ресивер и впускные каналы"), 1, {0,23,43}, {TEXT("DecorCover")}, Alloy);
        Add(TEXT("Ignition"), TEXT("Высоковольтные провода и модуль зажигания"), 2, {0,0,53}, {TEXT("DecorCover")}, FLinearColor(0.018f,0.020f,0.023f));
        Add(TEXT("TimingCover"), TEXT("Защитная крышка ремня ГРМ"), 3, {34,0,29}, {TEXT("DecorCover")}, Dark);
        Add(TEXT("Belt"), TEXT("Зубчатый ремень ГРМ (условная геометрия)"), 4, {32,0,29}, {TEXT("TimingCover")}, FLinearColor(0.009f,0.011f,0.012f));
        Add(TEXT("InletPulley"), TEXT("Шкив впускного распредвала"), 5, {30,-9,43}, {TEXT("Belt")}, Alloy);
        Add(TEXT("ExhaustPulley"), TEXT("Шкив выпускного распредвала"), 5, {30,9,43}, {TEXT("Belt")}, Alloy);
        Add(TEXT("ValveCover"), TEXT("Крышка головки блока цилиндров"), 6, {0,0,49}, {TEXT("Receiver"),TEXT("Ignition")}, Alloy);
        Add(TEXT("CamBearingHousing"), TEXT("Корпус подшипников распредвалов"), 7, {0,0,46}, {TEXT("ValveCover")}, Alloy);
        Add(TEXT("InletCam"), TEXT("Впускной распределительный вал"), 8, {0,-9,43}, {TEXT("CamBearingHousing"),TEXT("InletPulley")}, FLinearColor(0.38f,0.39f,0.40f));
        Add(TEXT("ExhaustCam"), TEXT("Выпускной распределительный вал"), 8, {0,9,43}, {TEXT("CamBearingHousing"),TEXT("ExhaustPulley")}, FLinearColor(0.38f,0.39f,0.40f));
        for (int32 I=0; I<4; ++I)
        {
            const FString Id = FString::Printf(TEXT("Valves%d"), I);
            Add(*Id, FString::Printf(TEXT("Клапаны цилиндра %d: 2 впускных + 2 выпускных"), I+1), 9,
                {-19.5f+13.0f*I,0,36}, {TEXT("InletCam"),TEXT("ExhaustCam")}, Alloy);
        }
        Add(TEXT("Exhaust"), TEXT("Выпускной коллектор"), 10, {0,-23,31}, {}, FLinearColor(0.09f,0.055f,0.035f));
        Add(TEXT("Head"), TEXT("Головка блока цилиндров • DOHC 16V"), 11, {0,0,32},
            {TEXT("Valves0"),TEXT("Valves1"),TEXT("Valves2"),TEXT("Valves3"),TEXT("Receiver"),TEXT("Exhaust")}, Alloy);
        Add(TEXT("HeadGasket"), TEXT("Прокладка головки блока"), 12, {0,0,25}, {TEXT("Head")}, FLinearColor(0.035f,0.038f,0.040f));
        Add(TEXT("Sump"), TEXT("Масляный поддон"), 13, {0,0,-20}, {}, FLinearColor(0.022f,0.027f,0.032f));
        Add(TEXT("OilPickup"), TEXT("Маслоприёмник"), 14, {5,0,-14}, {TEXT("Sump")}, Alloy);
        Add(TEXT("OilPump"), TEXT("Масляный насос"), 15, {30,0,-5}, {TEXT("Belt"),TEXT("Sump")}, Alloy);
        Add(TEXT("RodCaps"), TEXT("Крышки шатунов (комплект)"), 16, {0,0,-9}, {TEXT("OilPickup")}, Alloy);
        for (int32 I=0; I<4; ++I)
        {
            const FString Id = FString::Printf(TEXT("Piston%d"), I);
            Add(*Id, FString::Printf(TEXT("Поршень и шатун цилиндра %d"), I+1), 17,
                {-19.5f+13.0f*I,0,12}, {TEXT("HeadGasket"),TEXT("RodCaps")}, Alloy, I);
        }
        Add(TEXT("BearingCaps"), TEXT("Крышки коренных подшипников (комплект)"), 18, {0,0,-10},
            {TEXT("RodCaps"),TEXT("OilPickup")}, Alloy);
        Add(TEXT("Flywheel"), TEXT("Маховик"), 19, {-32,0,-3}, {}, Alloy);
        Add(TEXT("Crankshaft"), TEXT("Коленчатый вал"), 20, {0,0,-3},
            {TEXT("Piston0"),TEXT("Piston1"),TEXT("Piston2"),TEXT("Piston3"),TEXT("BearingCaps"),TEXT("OilPump"),TEXT("Flywheel")}, FLinearColor(0.32f,0.33f,0.34f));
        Add(TEXT("Block"), TEXT("Блок цилиндров • рядная четвёрка"), 21, {0,0,10},
            {TEXT("HeadGasket"),TEXT("Crankshaft")}, FLinearColor(0.13f,0.14f,0.145f));
        return D;
    }();
    return Data;
}

AKemEngineAssembly::AKemEngineAssembly()
{
    bReplicates = true;
    SetReplicateMovement(true);
    bAlwaysRelevant = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> StandMaterial(TEXT("/Game/KUB/Engine2112/MI_EngineStand.MI_EngineStand"));
    auto Support = [&](const TCHAR* Name, FVector P, FVector S)
    {
        UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        C->SetupAttachment(SceneRoot); C->SetStaticMesh(Cube.Object);
        C->SetMaterial(0,StandMaterial.Object);
        C->SetRelativeLocation(P); C->SetRelativeScale3D(S/100.0f);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    };
    Support(TEXT("StandBase"), {0,0,-57}, {90,65,5});
    Support(TEXT("StandPostA"), {-22,0,-38}, {5,6,35});
    Support(TEXT("StandPostB"), {22,0,-38}, {5,6,35});
}

void AKemEngineAssembly::BeginPlay()
{
    Super::BeginPlay();
    EnsureParts();
}

void AKemEngineAssembly::EnsureParts()
{
    if (!HasAuthority() || !Parts.IsEmpty()) return;
    for (int32 I=0; I<Definitions().Num(); ++I)
    {
        const FTransform Target=FTransform(Definitions()[I].Position)*GetActorTransform();
        AKemEnginePart* P = GetWorld()->SpawnActorDeferred<AKemEnginePart>(AKemEnginePart::StaticClass(), Target, this);
        if (P)
        {
            P->Initialize(this, I);
            P->FinishSpawning(P->InstalledTransform());
            Parts.Add(P);
        }
    }
    ForceNetUpdate();
}

void AKemEngineAssembly::EndPlay(const EEndPlayReason::Type Reason)
{
    if (HasAuthority()) for (AKemEnginePart* P : Parts) if (IsValid(P)) P->Destroy();
    Super::EndPlay(Reason);
}

void AKemEngineAssembly::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AKemEngineAssembly, Parts);
    DOREPLIFETIME(AKemEngineAssembly, Lesson);
}

AKemEnginePart* AKemEngineAssembly::FindPart(FName Id) const
{
    for (AKemEnginePart* P : Parts) if (IsValid(P) && P->GetPartId()==Id) return P;
    return nullptr;
}

bool AKemEngineAssembly::CanRemove(const AKemEnginePart* Part, FString& Reason) const
{
    if (!Part || Part->GetAssembly()!=this) return false;
    const FKemEnginePartDefinition* D = Definitions().FindByPredicate([&](const FKemEnginePartDefinition& V){ return V.Id==Part->GetPartId(); });
    if (!D) return false;
    for (FName Id : D->RemovalBlockers)
        if (const AKemEnginePart* B = FindPart(Id); !B || B->IsInstalled() || B->IsHeld())
        {
            Reason = B ? TEXT("Сначала снимите и отпустите: ")+B->GetPartName() : TEXT("Модель загружается");
            return false;
        }
    return true;
}

bool AKemEngineAssembly::CanInstall(const AKemEnginePart* Part, FString& Reason) const
{
    if (!CanRemove(Part, Reason)) return false; // No cover may already occupy the target.
    for (const FKemEnginePartDefinition& D : Definitions())
        if (D.RemovalBlockers.Contains(Part->GetPartId()))
            if (const AKemEnginePart* Support = FindPart(D.Id); !Support || !Support->IsInstalled() || Support->IsHeld())
            {
                Reason = Support ? TEXT("Сначала установите: ")+Support->GetPartName() : TEXT("Модель загружается");
                return false;
            }
    return true;
}

AKemEnginePart* AKemEngineAssembly::NextPart() const
{
    FString Ignored;
    if (Lesson==EKemEngineLesson::Assemble)
    {
        for (int32 I=Parts.Num()-1; I>=0; --I)
            if (IsValid(Parts[I]) && !Parts[I]->IsInstalled() && CanInstall(Parts[I],Ignored)) return Parts[I];
    }
    else
    {
        for (AKemEnginePart* P : Parts)
            if (IsValid(P) && P->IsInstalled() && CanRemove(P,Ignored)) return P;
    }
    return nullptr;
}

bool AKemEngineAssembly::IsNextPart(const AKemEnginePart* Part) const { return NextPart()==Part; }
bool AKemEngineAssembly::IsBusy() const
{
    for (const AKemEnginePart* P : Parts) if (IsValid(P) && P->IsHeld()) return true;
    return false;
}

bool AKemEngineAssembly::ConfigureLesson(EKemEngineLesson Mode, bool bReset)
{
    if (!HasAuthority() || IsBusy() || static_cast<uint8>(Mode)>static_cast<uint8>(EKemEngineLesson::Assemble)) return false;
    Lesson = Mode;
    if (bReset) for (AKemEnginePart* P : Parts) if (IsValid(P)) P->ResetPart(Mode!=EKemEngineLesson::Assemble);
    ForceNetUpdate();
    return true;
}

FString AKemEngineAssembly::GetLessonText() const
{
    int32 Installed=0;
    for (const AKemEnginePart* P : Parts) if (IsValid(P) && P->IsInstalled()) ++Installed;
    FString Text = FString::Printf(TEXT("ВАЗ-2112 • 1,5 л • DOHC • 16 клапанов\nУстановлено: %d / %d\n"), Installed, Definitions().Num());
    if (Parts.Num()!=Definitions().Num()) return Text+TEXT("Загрузка деталей…");
    if (Lesson==EKemEngineLesson::Free) Text+=TEXT("Свободный режим: выберите доступную деталь.\n");
    else if (AKemEnginePart* P=NextPart()) Text+=(Lesson==EKemEngineLesson::Assemble ? TEXT("Установите: ") : TEXT("Снимите: "))+P->GetPartName()+TEXT("\n");
    else Text+=TEXT("Задание выполнено!\n");
    return Text+TEXT("Grip: взять / отпустить; вращайте кисть.\nСовместите деталь с подсвеченным местом.\nУчебная модель: крепёж и геометрия упрощены.");
}
