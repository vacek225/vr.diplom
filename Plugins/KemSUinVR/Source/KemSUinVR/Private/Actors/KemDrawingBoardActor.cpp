#include "Actors/KemDrawingBoardActor.h"
#include "Actors/KemPlayerControlProxy.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const TCHAR* PlaneMeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");
const TCHAR* BoardMaterialPath = TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque_OneSided.Widget3DPassThrough_Opaque_OneSided");
const FName SlateTextureParameter(TEXT("SlateUI"));
const FName BackColorParameter(TEXT("BackColor"));
const FName TintColorParameter(TEXT("TintColorAndOpacity"));
const FName OpacityFromTextureParameter(TEXT("OpacityFromTexture"));
}

AKemDrawingBoardActor::AKemDrawingBoardActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true; bAlwaysRelevant = true;
    SetNetUpdateFrequency(20.f);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    BoardBacking = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardBacking"));
    BoardBacking->SetupAttachment(SceneRoot);
    BoardBacking->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BoardBacking->SetCastShadow(true);

    BoardSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardSurface"));
    BoardSurface->SetupAttachment(SceneRoot);
    BoardSurface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoardSurface->SetCollisionResponseToAllChannels(ECR_Ignore);
    BoardSurface->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    BoardSurface->SetGenerateOverlapEvents(false);
    BoardSurface->SetMobility(EComponentMobility::Movable);
    BoardSurface->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(PlaneMeshPath);
    if (PlaneMesh.Succeeded())
    {
        BoardSurface->SetStaticMesh(PlaneMesh.Object);
        BoardBacking->SetStaticMesh(PlaneMesh.Object);
    }
}

void AKemDrawingBoardActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshBoardMesh();
}

void AKemDrawingBoardActor::BeginPlay()
{
    Super::BeginPlay();

    RefreshBoardMesh();
    InitializeBoardRenderTarget();
    OnRep_Segments();
}

bool AKemDrawingBoardActor::StartDraw(const FString& DrawerId, const FVector2D& BoardUV)
{
    if (DrawerId.IsEmpty())
    {
        return false;
    }

    const FVector2D ClampedUV(FMath::Clamp(BoardUV.X, 0.0f, 1.0f), FMath::Clamp(BoardUV.Y, 0.0f, 1.0f));
    ActiveDrawSessions.Add(DrawerId, ClampedUV);
    DrawDotAtUV(ClampedUV);
    return true;
}

bool AKemDrawingBoardActor::UpdateDraw(const FString& DrawerId, const FVector2D& BoardUV)
{
    if (DrawerId.IsEmpty())
    {
        return false;
    }

    const FVector2D ClampedUV(FMath::Clamp(BoardUV.X, 0.0f, 1.0f), FMath::Clamp(BoardUV.Y, 0.0f, 1.0f));
    const FVector2D* PreviousUV = ActiveDrawSessions.Find(DrawerId);
    if (!PreviousUV)
    {
        return StartDraw(DrawerId, ClampedUV);
    }

    DrawLineBetweenUVs(*PreviousUV, ClampedUV);
    ActiveDrawSessions[DrawerId] = ClampedUV;
    return true;
}

void AKemDrawingBoardActor::StopDraw(const FString& DrawerId)
{
    ActiveDrawSessions.Remove(DrawerId);
}

void AKemDrawingBoardActor::ClearBoard()
{
    if (HasAuthority()) { DrawingHistory.Items.Reset(); DrawingHistory.MarkArrayDirty(); ++ClearRevision; NetworkLastUV.Reset(); ForceNetUpdate(); }
    RenderedSegments = 0; RenderedRevision = ClearRevision;
    ActiveDrawSessions.Reset();
    if (BoardRenderTarget)
    {
        UKismetRenderingLibrary::ClearRenderTarget2D(this, BoardRenderTarget, BoardClearColor);
    }
}

void AKemDrawingBoardActor::SetBrushColor(const FLinearColor& NewBrushColor)
{
    BrushColor = NewBrushColor;
}

void AKemDrawingBoardActor::SetBrushSize(float NewBrushSize)
{
    BrushSize = FMath::Clamp(NewBrushSize, 1.0f, 128.0f);
}

bool AKemDrawingBoardActor::ResolveHitToBoardUV(const FHitResult& Hit, FVector2D& OutBoardUV) const
{
    if (Hit.GetActor() != this || Hit.GetComponent() != BoardSurface || !BoardSurface)
    {
        return false;
    }

    FVector2D CollisionUV = FVector2D::ZeroVector;
    if (UGameplayStatics::FindCollisionUV(Hit, 0, CollisionUV))
    {
        OutBoardUV = FVector2D(
            FMath::Clamp(CollisionUV.X, 0.0f, 1.0f),
            FMath::Clamp(CollisionUV.Y, 0.0f, 1.0f));
        return true;
    }

    FVector LocalMin = FVector::ZeroVector;
    FVector LocalMax = FVector::ZeroVector;
    BoardSurface->GetLocalBounds(LocalMin, LocalMax);

    const FVector LocalHit = BoardSurface->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
    const float WidthRange = LocalMax.X - LocalMin.X;
    const float HeightRange = LocalMax.Y - LocalMin.Y;
    if (FMath::IsNearlyZero(WidthRange) || FMath::IsNearlyZero(HeightRange))
    {
        return false;
    }

    const float U = (LocalHit.X - LocalMin.X) / WidthRange;
    const float V = (LocalHit.Y - LocalMin.Y) / HeightRange;
    if (U < 0.0f || U > 1.0f || V < 0.0f || V > 1.0f)
    {
        return false;
    }

    OutBoardUV = FVector2D(U, V);
    return true;
}

bool AKemDrawingBoardActor::ResolveWorldPointToBoardUV(const FVector& WorldPoint, FVector2D& OutBoardUV) const
{
    if (!BoardSurface)
    {
        return false;
    }

    FVector LocalMin = FVector::ZeroVector;
    FVector LocalMax = FVector::ZeroVector;
    BoardSurface->GetLocalBounds(LocalMin, LocalMax);

    const FVector LocalPoint = BoardSurface->GetComponentTransform().InverseTransformPosition(WorldPoint);
    const float WidthRange = LocalMax.X - LocalMin.X;
    const float HeightRange = LocalMax.Y - LocalMin.Y;
    if (FMath::IsNearlyZero(WidthRange) || FMath::IsNearlyZero(HeightRange))
    {
        return false;
    }

    const float U = (LocalPoint.X - LocalMin.X) / WidthRange;
    const float V = (LocalPoint.Y - LocalMin.Y) / HeightRange;
    if (U < 0.0f || U > 1.0f || V < 0.0f || V > 1.0f)
    {
        return false;
    }

    OutBoardUV = FVector2D(U, V);
    return true;
}

bool AKemDrawingBoardActor::ResolveRayToBoardUV(const FVector& RayOrigin, const FVector& RayDirection, FVector2D& OutBoardUV) const
{
    if (!BoardSurface)
    {
        return false;
    }

    const FVector PlaneNormal = BoardSurface->GetUpVector().GetSafeNormal();
    const float Denominator = FVector::DotProduct(RayDirection, PlaneNormal);
    if (FMath::IsNearlyZero(Denominator))
    {
        return false;
    }

    const FVector PlanePoint = BoardSurface->GetComponentLocation();
    const float DistanceAlongRay = FVector::DotProduct(PlanePoint - RayOrigin, PlaneNormal) / Denominator;
    if (DistanceAlongRay < 0.0f)
    {
        return false;
    }

    const FVector WorldPoint = RayOrigin + RayDirection * DistanceAlongRay;
    return ResolveWorldPointToBoardUV(WorldPoint, OutBoardUV);
}

bool AKemDrawingBoardActor::TraceDrawingSurface(const FVector& Start, const FVector& Direction, float MaxDistance, FHitResult& OutHit) const
{
    if (!BoardSurface || IsHidden()) return false;
    const FVector Normal=BoardSurface->GetUpVector();
    const FVector Ray=Direction.GetSafeNormal();
    const float Denominator=FVector::DotProduct(Ray,Normal);
    if (FMath::Abs(Denominator)<KINDA_SMALL_NUMBER) return false;
    const float Distance=FVector::DotProduct(BoardSurface->GetComponentLocation()-Start,Normal)/Denominator;
    if (Distance<0 || Distance>MaxDistance) return false;
    const FVector Point=Start+Ray*Distance;
    FVector2D UV;
    if (!ResolveWorldPointToBoardUV(Point,UV)) return false;
    OutHit=FHitResult(const_cast<AKemDrawingBoardActor*>(this),BoardSurface,Point,Normal);
    OutHit.bBlockingHit=true; OutHit.Distance=Distance;
    OutHit.TraceStart=Start; OutHit.TraceEnd=Start+Ray*MaxDistance;
    return true;
}

void AKemDrawingBoardActor::InitializeBoardRenderTarget()
{
    if (BoardRenderTarget && BoardMaterialInstance)
    {
        BoardMaterialInstance->SetTextureParameterValue(SlateTextureParameter, BoardRenderTarget);
        BoardMaterialInstance->SetVectorParameterValue(BackColorParameter, BoardClearColor);
        BoardMaterialInstance->SetVectorParameterValue(TintColorParameter, FLinearColor::White);
        BoardMaterialInstance->SetScalarParameterValue(OpacityFromTextureParameter, 1.0f);
        return;
    }

    BoardRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, TextureResolution, TextureResolution);
    if (!BoardRenderTarget)
    {
        return;
    }

    BoardRenderTarget->ClearColor = BoardClearColor;
    BoardRenderTarget->Filter = TF_Bilinear;

    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, BoardMaterialPath);
    if (BaseMaterial)
    {
        BoardMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        if (BoardMaterialInstance)
        {
            BoardMaterialInstance->SetTextureParameterValue(SlateTextureParameter, BoardRenderTarget);
            BoardMaterialInstance->SetVectorParameterValue(BackColorParameter, BoardClearColor);
            BoardMaterialInstance->SetVectorParameterValue(TintColorParameter, FLinearColor::White);
            BoardMaterialInstance->SetScalarParameterValue(OpacityFromTextureParameter, 1.0f);
            BoardSurface->SetMaterial(0, BoardMaterialInstance);
        }
    }

    UKismetRenderingLibrary::ClearRenderTarget2D(this, BoardRenderTarget, BoardClearColor);
}

void AKemDrawingBoardActor::RefreshBoardMesh()
{
    if (BoardSurface)
    {
        BoardSurface->SetRelativeLocation(FVector(1.0f, 0.0f, 0.0f));
        BoardSurface->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
        BoardSurface->SetRelativeScale3D(FVector(BoardWidth / 100.0f, BoardHeight / 100.0f, 1.0f));
    }

    if (BoardBacking)
    {
        BoardBacking->SetRelativeLocation(FVector(-1.0f, 0.0f, 0.0f));
        BoardBacking->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
        BoardBacking->SetRelativeScale3D(FVector((BoardWidth + 12.0f) / 100.0f, (BoardHeight + 12.0f) / 100.0f, 1.0f));
    }
}

void AKemDrawingBoardActor::DrawDotAtUV(const FVector2D& BoardUV)
{
    if (!BoardRenderTarget)
    {
        return;
    }

    UCanvas* Canvas = nullptr;
    FVector2D Size = FVector2D::ZeroVector;
    FDrawToRenderTargetContext Context;
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, BoardRenderTarget, Canvas, Size, Context);
    if (Canvas)
    {
        const FVector2D Pixel = BoardUVToPixel(BoardUV);
        const FVector2D DotSize(BrushSize, BrushSize);
        Canvas->K2_DrawBox(Pixel - DotSize * 0.5f, DotSize, BrushSize * 0.15f, BrushColor);
    }
    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
}

void AKemDrawingBoardActor::DrawLineBetweenUVs(const FVector2D& PreviousBoardUV, const FVector2D& CurrentBoardUV)
{
    if (!BoardRenderTarget)
    {
        return;
    }

    UCanvas* Canvas = nullptr;
    FVector2D Size = FVector2D::ZeroVector;
    FDrawToRenderTargetContext Context;
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, BoardRenderTarget, Canvas, Size, Context);
    if (Canvas)
    {
        Canvas->K2_DrawLine(BoardUVToPixel(PreviousBoardUV), BoardUVToPixel(CurrentBoardUV), BrushSize, BrushColor);
    }
    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
}

FVector2D AKemDrawingBoardActor::BoardUVToPixel(const FVector2D& BoardUV) const
{
    const float MaxIndex = FMath::Max(1.0f, static_cast<float>(TextureResolution - 1));
    return FVector2D(BoardUV.X * MaxIndex, BoardUV.Y * MaxIndex);
}

void AKemDrawingBoardActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AKemDrawingBoardActor, DrawingHistory);
    DOREPLIFETIME(AKemDrawingBoardActor, ClearRevision);
}
void AKemDrawingBoardActor::RequestDraw(APlayerController* PC, FVector2D UV, bool bStart)
{
    if (auto* Proxy=AKemPlayerControlProxy::FindForController(PC))
        Proxy->ServerDrawBoard(this,UV,bStart,BrushColor,BrushSize);
}
bool AKemDrawingBoardActor::AcceptDraw(AActor* Drawer, FVector2D UV, bool bStart, FLinearColor Color, float Size)
{
    if (!HasAuthority() || !IsValid(Drawer) || UV.ContainsNaN() || (!FMath::IsFinite(Color.R) || !FMath::IsFinite(Color.G) || !FMath::IsFinite(Color.B) || !FMath::IsFinite(Color.A)) || !FMath::IsFinite(Size) ||
        UV.X<0 || UV.X>1 || UV.Y<0 || UV.Y>1 || DrawingHistory.Items.Num()>=2048) return false;
    FKemBoardSegment Segment;
    const FVector2D* Last=NetworkLastUV.Find(Drawer);
    Segment.Start = !bStart && Last ? *Last : UV;
    Segment.End=UV; Segment.Color=Color.GetClamped(); Segment.Size=FMath::Clamp(Size,1.f,128.f);
    if (Last && !bStart && Last->Equals(UV,.0005f)) return true;
    NetworkLastUV.Add(Drawer,UV);
    const int32 Index=DrawingHistory.Items.Add(Segment);
    DrawingHistory.MarkItemDirty(DrawingHistory.Items[Index]);
    OnRep_Segments(); ForceNetUpdate(); return true;
}
void AKemDrawingBoardActor::OnRep_Segments()
{
    if (!BoardRenderTarget) return;
    if (RenderedRevision!=ClearRevision || RenderedSegments>DrawingHistory.Items.Num())
    {
        UKismetRenderingLibrary::ClearRenderTarget2D(this,BoardRenderTarget,BoardClearColor);
        RenderedSegments=0; RenderedRevision=ClearRevision;
    }
    const FLinearColor PreviousColor=BrushColor; const float PreviousSize=BrushSize;
    while(RenderedSegments<DrawingHistory.Items.Num())
    {
        const auto& S=DrawingHistory.Items[RenderedSegments++]; BrushColor=S.Color; BrushSize=S.Size;
        if(S.Start.Equals(S.End)) DrawDotAtUV(S.End); else DrawLineBetweenUVs(S.Start,S.End);
    }
    BrushColor=PreviousColor; BrushSize=PreviousSize;
}

void AKemDrawingBoardActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!HasAuthority()) OnRep_Segments();
}
