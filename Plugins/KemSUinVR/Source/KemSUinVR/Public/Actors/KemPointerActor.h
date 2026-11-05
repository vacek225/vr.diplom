#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KemPointerActor.generated.h"

/** One ephemeral pointer per player, created and updated through their owned RPC proxy. */
UCLASS()
class KEMSUINVR_API AKemPointerActor : public AActor
{
    GENERATED_BODY()
public:
    AKemPointerActor();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    bool IsPointerActive() const { return bActive; }
    void UpdatePointer(FVector Start, FVector End, bool bEnabled);
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Beam;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Dot;
    UPROPERTY(Replicated) FVector_NetQuantize TraceStart;
    UPROPERTY(Replicated) FVector_NetQuantize TraceEnd;
    UPROPERTY(Replicated) bool bActive = false;
    float LastUpdate = -100.f;
};
