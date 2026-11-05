#include "UI/KemEngineTrainingWidget.h"

#include "Actors/KemPlayerControlProxy.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Core/KemRoomSubsystem.h"
#include "Engine/KemEngineAssembly.h"
#include "GameFramework/PlayerController.h"

void UKemEnginePartLabelWidget::SetPartName(const FString& Name)
{
    PartName=Name;
    if (NameText) NameText->SetText(FText::FromString(Name));
}

void UKemEnginePartLabelWidget::BuildUI()
{
    UBorder* Border=CreatePanelBorder(FLinearColor(0.02f,0.05f,0.08f,0.9f));
    Border->SetPadding(FMargin(12));
    NameText=CreateTextBlock(PartName,26);
    NameText->SetAutoWrapText(true);
    NameText->SetJustification(ETextJustify::Center);
    Border->SetContent(NameText);
    WidgetTree->RootWidget=Border;
}

void UKemEngineTrainingWidget::BuildUI()
{
    UBorder* Border=CreatePanelBorder(FLinearColor(0.04f,0.08f,0.12f,0.94f));
    UVerticalBox* Box=CreateVerticalBox();
    Border->SetContent(Box); WidgetTree->RootWidget=Border;
    Box->AddChildToVerticalBox(CreateTextBlock(TEXT("Двигатель ВАЗ-2112"),30));
    Status=CreateTextBlock(TEXT("Загрузка…"),22);
    Status->SetAutoWrapText(true);
    Box->AddChildToVerticalBox(Status);
    for(const TPair<FString,FString>& A : TArray<TPair<FString,FString>>{
        {TEXT("Свободная разборка / сборка"),TEXT("free")},
        {TEXT("Урок: разборка с начала"),TEXT("disassemble")},
        {TEXT("Урок: сборка из деталей"),TEXT("assemble")},
        {TEXT("Сбросить текущее упражнение"),TEXT("reset")}})
    {
        UKemActionButton* Button=CreateActionButton(A.Key,A.Value);
        Button->OnKemActionClicked.AddDynamic(this,&UKemEngineTrainingWidget::HandleAction);
        Box->AddChildToVerticalBox(Button); HostButtons.Add(Button);
    }
    Box->AddChildToVerticalBox(CreateTextBlock(TEXT("Режим меняет преподаватель.\nПК: удерживайте F, чтобы взять; R — вращать;\nколесо мыши — расстояние. Отпустите F для установки."),18));
}

void UKemEngineTrainingWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry,DeltaSeconds);
    RefreshTime-=DeltaSeconds;
    if (RefreshTime>0 || !Engine || !Status) return;
    RefreshTime=0.2f;
    AKemPlayerControlProxy* Proxy=AKemPlayerControlProxy::FindForController(GetWorld()->GetFirstPlayerController());
    FString Message=Engine->GetLessonText();
    if (Proxy && !Proxy->GetEngineFeedback().IsEmpty()) Message+=TEXT("\n\n")+Proxy->GetEngineFeedback();
    Status->SetText(FText::FromString(Message));
    const UKemRoomSubsystem* Room=GetGameInstance() ? GetGameInstance()->GetSubsystem<UKemRoomSubsystem>() : nullptr;
    for(UKemActionButton* Button : HostButtons) Button->SetIsEnabled(Proxy && Room && Room->IsHost() && !Engine->IsBusy());
}

void UKemEngineTrainingWidget::HandleAction(const FString& Action)
{
    AKemPlayerControlProxy* Proxy=AKemPlayerControlProxy::FindForController(GetWorld()->GetFirstPlayerController());
    if (!Engine || !Proxy) return;
    EKemEngineLesson Mode=Engine->GetLesson();
    if (Action==TEXT("free")) Mode=EKemEngineLesson::Free;
    else if (Action==TEXT("disassemble")) Mode=EKemEngineLesson::Disassemble;
    else if (Action==TEXT("assemble")) Mode=EKemEngineLesson::Assemble;
    Proxy->ServerConfigureEngine(Engine,Mode,Action!=TEXT("free"));
}
