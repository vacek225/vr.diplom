#pragma once

#include "CoreMinimal.h"
#include "KemTypes.generated.h"

UENUM(BlueprintType)
enum class EKemHubMenuState : uint8
{
    Root,
    AuditoriumSelection,
    JoinCodeEntry,
    RoomHosted,
    InRoom
};

UENUM(BlueprintType)
enum class EKemPresentationPrimitive : uint8
{
    Cube,
    Cylinder,
    Sphere
};

USTRUCT(BlueprintType)
struct FKemAuditoriumDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KemSUinVR")
    FName AuditoriumId = TEXT("DefaultAuditorium");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KemSUinVR")
    FText DisplayName = FText::FromString(TEXT("Default Auditorium"));

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KemSUinVR")
    FString MapPath = TEXT("/Game/VRTemplate/VRTemplateMap");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "KemSUinVR")
    FText Description = FText::FromString(TEXT("Prototype auditorium used by the KemSUinVR mockup."));
};

USTRUCT(BlueprintType)
struct FKemRoomInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "KemSUinVR")
    FString RoomCode;

    UPROPERTY(BlueprintReadOnly, Category = "KemSUinVR")
    FName AuditoriumId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "KemSUinVR")
    FString MapPath;

    UPROPERTY(BlueprintReadOnly, Category = "KemSUinVR")
    bool bIsHost = false;

    UPROPERTY(BlueprintReadOnly, Category = "KemSUinVR")
    bool bIsInRoom = false;
};

USTRUCT(BlueprintType)
struct FKemParticipantInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "KemSUinVR")
    FString ParticipantId;

    UPROPERTY(BlueprintReadOnly, Category = "KemSUinVR")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "KemSUinVR")
    float VolumeMultiplier = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "KemSUinVR")
    bool bForcedMuted = false;

    UPROPERTY(BlueprintReadOnly, Category = "KemSUinVR")
    bool bBanned = false;
};
