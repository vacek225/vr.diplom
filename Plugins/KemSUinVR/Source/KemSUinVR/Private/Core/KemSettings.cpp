#include "Core/KemSettings.h"

UKemSUinVRSettings::UKemSUinVRSettings()
{
    if (Auditoriums.IsEmpty())
    {
        FKemAuditoriumDefinition ThirdBlockAuditorium;
        ThirdBlockAuditorium.AuditoriumId = TEXT("3_Block");
        ThirdBlockAuditorium.DisplayName = FText::FromString(TEXT("3_Block"));
        ThirdBlockAuditorium.MapPath = TEXT("/Game/VRTemplate/3_Block");
        ThirdBlockAuditorium.Description = FText::FromString(TEXT("Third block classroom map with lecture wall and podium."));
        Auditoriums.Add(ThirdBlockAuditorium);
    }
}

const FKemAuditoriumDefinition* UKemSUinVRSettings::FindAuditoriumById(FName AuditoriumId) const
{
    return Auditoriums.FindByPredicate([AuditoriumId](const FKemAuditoriumDefinition& Auditorium)
    {
        return Auditorium.AuditoriumId == AuditoriumId;
    });
}

FName UKemSUinVRSettings::GetCategoryName() const
{
    return TEXT("Plugins");
}
