#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KemEnginePart.generated.h"

class AKemEngineAssembly;
class AKemPlayerControlProxy;
class APawn;
class UStaticMeshComponent;
class UWidgetComponent;
class UMaterialInterface;

USTRUCT()
struct FKemEnginePartState
{
    GENERATED_BODY()
    UPROPERTY() FTransform Pose;
    UPROPERTY() bool bInstalled = true;
    UPROPERTY() TObjectPtr<APawn> HolderPawn = nullptr;
    UPROPERTY() bool bRightHand = false;
    // Identifies whose local view should remain revealed after a failed placement.
    UPROPERTY() TObjectPtr<APawn> PlacementPawn = nullptr;
};

UCLASS()
class KEMSUINVR_API AKemEnginePart : public AActor
{
    GENERATED_BODY()
public:
    AKemEnginePart();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void Initialize(AKemEngineAssembly* Assembly, int32 Index);
    bool TryAcquire(AKemPlayerControlProxy* Proxy, bool bRightHand, const FTransform& Hand, FString& Reason);
    bool TryAcquireFromRay(AKemPlayerControlProxy* Proxy, bool bRightHand, const FTransform& Ray, FString& Reason);
    bool MoveHeld(AKemPlayerControlProxy* Proxy, bool bRightHand, const FTransform& Pose);
    bool Release(AKemPlayerControlProxy* Proxy, bool bRightHand, FString& Reason);
    void ResetPart(bool bInstalled);
    void SetLocallyFocused(bool bFocused) { bLocalFocus = bFocused; }
    FTransform InstalledTransform() const;
    FTransform TrayTransform() const;
    FName GetPartId() const;
    FString GetPartName() const;
    bool IsInstalled() const { return State.bInstalled; }
    UFUNCTION(BlueprintPure, Category="Engine training")
    bool IsLocallyTransparent() const { return LocalRevealOpacity < .999f; }
    bool IsHeld() const { return State.HolderPawn != nullptr; }
    bool IsHeldBy(const APawn* Pawn, bool bRight) const { return Pawn && State.HolderPawn == Pawn && State.bRightHand == bRight; }
    bool CanSnapAt(const FTransform& Pose, FString& Reason) const;
    bool IsOwnedByProxy(const AKemPlayerControlProxy* Proxy) const;
    AKemEngineAssembly* GetAssembly() const { return Engine; }
    const FTransform& GetReplicatedPose() const { return State.Pose; }

private:
    bool AcquireValidated(AKemPlayerControlProxy* Proxy, bool bRightHand, const FTransform& Hand, FString& Reason, bool bFromRay);
    UFUNCTION() void RebuildVisuals();
    UFUNCTION() void OnRep_State();
    bool ValidatePose(const AKemPlayerControlProxy* Proxy, const FTransform& Pose) const;
    UStaticMeshComponent* AddShape(const FString& MeshName, const FVector& Position, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator);
    void BuildGeometry();
    void ApplyState();
    void UpdateAssemblyTransparency(float DeltaSeconds);
    FBox SolidBounds() const;
    bool IsInsideConstruction() const;

    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> Label;
    UPROPERTY(ReplicatedUsing=RebuildVisuals) TObjectPtr<AKemEngineAssembly> Engine;
    UPROPERTY(ReplicatedUsing=RebuildVisuals) int32 PartIndex = INDEX_NONE;
    UPROPERTY(ReplicatedUsing=OnRep_State) FKemEnginePartState State;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Solids;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Ghosts;
    UPROPERTY() TObjectPtr<UMaterialInterface> SolidMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> GhostMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> RevealMaterial;
    UPROPERTY() TObjectPtr<class UStaticMesh> CubeMesh;
    UPROPERTY() TObjectPtr<class UStaticMesh> CylinderMesh;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> PartMaterial;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> TargetMaterial;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> LocalRevealMaterial;
    bool bRevealRequested = false;
    float LocalRevealOpacity = 1.f;
    TWeakObjectPtr<AKemPlayerControlProxy> HolderProxy;
    float LastPoseTime = 0.0f;
    bool bLocalFocus = false;
    bool bVisualsBuilt = false;
    TArray<FTransform> GeometryTransforms;
};
