#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/KemTypes.h"
#include "KemVoiceControlProxy.generated.h"

UCLASS()
class KEMSUINVR_API AKemVoiceControlProxy : public AActor
{
    GENERATED_BODY()

public:
    AKemVoiceControlProxy();

    UFUNCTION(Client, Reliable)
    void ClientSetVoiceTransmitEnabled(bool bEnabled);

    UFUNCTION(Client, Reliable)
    void ClientBootstrapVoice();

private:
    void ApplyVoiceTransmitEnabled(bool bEnabled);
};
