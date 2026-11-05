#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "KemDrawingBoardActor.generated.h"

class UStaticMeshComponent;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;

USTRUCT()
struct FKemBoardSegment : public FFastArraySerializerItem
{
    GENERATED_BODY()
    UPROPERTY() FVector2D Start = FVector2D::ZeroVector;
    UPROPERTY() FVector2D End = FVector2D::ZeroVector;
    UPROPERTY() FLinearColor Color = FLinearColor::Black;
    UPROPERTY() float Size = 6.f;
};

USTRUCT()
struct FKemBoardHistory : public FFastArraySerializer
{
    GENERATED_BODY()
    UPROPERTY() TArray<FKemBoardSegment> Items;
    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
    { return FastArrayDeltaSerialize<FKemBoardSegment,FKemBoardHistory>(Items,DeltaParams,*this); }
};
template<> struct TStructOpsTypeTraits<FKemBoardHistory> : TStructOpsTypeTraitsBase2<FKemBoardHistory>
{ enum { WithNetDeltaSerializer = true }; };

UCLASS()
class KEMSUINVR_API AKemDrawingBoardActor : public AActor
{
    GENERATED_BODY()

public:
    AKemDrawingBoardActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void RequestDraw(APlayerController* PC, FVector2D UV, bool bStart);
    bool AcceptDraw(AActor* Drawer, FVector2D UV, bool bStart, FLinearColor Color, float Size);
    int32 GetSegmentCount() const { return DrawingHistory.Items.Num(); }
    bool TraceDrawingSurface(const FVector& Start, const FVector& Direction, float MaxDistance, FHitResult& OutHit) const;

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR|Drawing Board")
    bool StartDraw(const FString& DrawerId, const FVector2D& BoardUV);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR|Drawing Board")
    bool UpdateDraw(const FString& DrawerId, const FVector2D& BoardUV);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR|Drawing Board")
    void StopDraw(const FString& DrawerId);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR|Drawing Board")
    void ClearBoard();

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR|Drawing Board")
    void SetBrushColor(const FLinearColor& NewBrushColor);

    UFUNCTION(BlueprintCallable, Category = "KemSUinVR|Drawing Board")
    void SetBrushSize(float NewBrushSize);

    UFUNCTION(BlueprintPure, Category = "KemSUinVR|Drawing Board")
    bool ResolveHitToBoardUV(const FHitResult& Hit, FVector2D& OutBoardUV) const;

    UFUNCTION(BlueprintPure, Category = "KemSUinVR|Drawing Board")
    bool ResolveWorldPointToBoardUV(const FVector& WorldPoint, FVector2D& OutBoardUV) const;

    UFUNCTION(BlueprintPure, Category = "KemSUinVR|Drawing Board")
    bool ResolveRayToBoardUV(const FVector& RayOrigin, const FVector& RayDirection, FVector2D& OutBoardUV) const;

    UFUNCTION(BlueprintPure, Category = "KemSUinVR|Drawing Board")
    FLinearColor GetClearColor() const { return BoardClearColor; }

    float GetBrushSize() const { return BrushSize; }

    UFUNCTION(BlueprintPure, Category = "KemSUinVR|Drawing Board")
    FLinearColor GetBrushColor() const { return BrushColor; }

    UFUNCTION(BlueprintPure, Category = "KemSUinVR|Drawing Board")
    UStaticMeshComponent* GetBoardSurface() const { return BoardSurface; }

protected:
    void InitializeBoardRenderTarget();
    void RefreshBoardMesh();
    void DrawDotAtUV(const FVector2D& BoardUV);
    void DrawLineBetweenUVs(const FVector2D& PreviousBoardUV, const FVector2D& CurrentBoardUV);
    FVector2D BoardUVToPixel(const FVector2D& BoardUV) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Drawing Board")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Drawing Board")
    TObjectPtr<UStaticMeshComponent> BoardSurface;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Drawing Board")
    TObjectPtr<UStaticMeshComponent> BoardBacking;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Drawing Board")
    TObjectPtr<UTextureRenderTarget2D> BoardRenderTarget;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> BoardMaterialInstance;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Drawing Board", meta = (ClampMin = "100.0"))
    float BoardWidth = 260.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Drawing Board", meta = (ClampMin = "100.0"))
    float BoardHeight = 160.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Drawing Board", meta = (ClampMin = "256", ClampMax = "4096"))
    int32 TextureResolution = 1024;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Drawing Board", meta = (ClampMin = "1.0", ClampMax = "128.0"))
    float BrushSize = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Drawing Board")
    FLinearColor BrushColor = FLinearColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KemSUinVR|Drawing Board")
    FLinearColor BoardClearColor = FLinearColor::White;

private:
    UFUNCTION() void OnRep_Segments();
    UPROPERTY(Replicated) FKemBoardHistory DrawingHistory;
    UPROPERTY(ReplicatedUsing=OnRep_Segments) int32 ClearRevision = 0;
    TMap<TWeakObjectPtr<AActor>, FVector2D> NetworkLastUV;
    int32 RenderedSegments = 0;
    int32 RenderedRevision = 0;
    TMap<FString, FVector2D> ActiveDrawSessions;
};
