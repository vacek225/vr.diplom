#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KemEngineAssembly.generated.h"

class AKemEnginePart;
class UStaticMeshComponent;
class UWidgetComponent;

UENUM(BlueprintType)
enum class EKemEngineLesson : uint8
{
    Free,
    Disassemble,
    Assemble
};

/** Educational geometry, in centimetres; not manufacturing/CAD dimensions. */
struct KEMSUINVR_API FKemEnginePartDefinition
{
    FName Id;
    FString Name;
    FVector Position;
    FLinearColor Color;
    TArray<FName> RemovalBlockers;
    int32 Shape = 0;
    int32 Variant = 0;
};

UCLASS()
class KEMSUINVR_API AKemEngineAssembly : public AActor
{
    GENERATED_BODY()
public:
    AKemEngineAssembly();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    static const TArray<FKemEnginePartDefinition>& Definitions();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Engine training")
    void EnsureParts();
    AKemEnginePart* FindPart(FName Id) const;
    bool CanRemove(const AKemEnginePart* Part, FString& Reason) const;
    bool CanInstall(const AKemEnginePart* Part, FString& Reason) const;
    bool IsNextPart(const AKemEnginePart* Part) const;
    AKemEnginePart* NextPart() const;
    UFUNCTION(BlueprintCallable, Category="Engine training")
    bool ConfigureLesson(EKemEngineLesson Mode, bool bReset);
    FString GetLessonText() const;
    EKemEngineLesson GetLesson() const { return Lesson; }
    bool IsBusy() const;

    UPROPERTY(Replicated)
    TArray<TObjectPtr<AKemEnginePart>> Parts;

    UPROPERTY(EditAnywhere, Category="Engine training", meta=(ClampMin="1", ClampMax="40", Units="cm"))
    float SnapDistance = 16.0f;

    UPROPERTY(EditAnywhere, Category="Engine training", meta=(ClampMin="1", ClampMax="90", Units="deg"))
    float SnapAngleDegrees = 55.0f;

    // Local visual assistance only; collision, placement and replication stay unchanged.
    UPROPERTY(EditAnywhere, Category="Engine training|Assembly visibility")
    bool bAssemblyRevealEnabled = true;
    UPROPERTY(EditAnywhere, Category="Engine training|Assembly visibility", meta=(ClampMin="0", ClampMax="40", Units="cm"))
    float AssemblyRevealDistance = 12.f;
    UPROPERTY(EditAnywhere, Category="Engine training|Assembly visibility", meta=(ClampMin="0.05", ClampMax="0.6"))
    float AssemblyRevealOpacity = .14f;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(Replicated)
    EKemEngineLesson Lesson = EKemEngineLesson::Free;
};
