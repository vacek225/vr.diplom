#include "UI/KemEscapeMenuWidget.h"
#include "UI/KemMenuArtWidget.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "UI/KemEscapeMenuSubsystem.h"
#include "Core/KemRoomSubsystem.h"
#include "Actors/KemClassroomStateActor.h"
#include "Actors/KemDrawingBoardActor.h"
#include "Engine/KemEngineAssembly.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformApplicationMisc.h"

namespace
{
UKemRoomSubsystem* Room(const UUserWidget* Widget)
{
    return Widget->GetGameInstance() ? Widget->GetGameInstance()->GetSubsystem<UKemRoomSubsystem>() : nullptr;
}
FLinearColor Hex(const TCHAR* Value) { return FLinearColor(FColor::FromHex(Value)); }
FLinearColor Ink=Hex(TEXT("F0F5FA"));
FLinearColor Accent=Hex(TEXT("8BDCF5"));
FLinearColor Muted=Hex(TEXT("A6B7C9"));
const TCHAR* Palette[]={TEXT("8BDCF5"),TEXT("F4C978"),TEXT("C7B6ED"),TEXT("9FD1BD"),TEXT("F0F5FA"),TEXT("18212C")};
}

UKemActionButton* UKemEscapeMenuWidget::MakeButton(const FString& Text, const FString& Id)
{
    auto* Button = CreateActionButton(Text, Id);
    FButtonStyle Style = Button->GetStyle();
    auto Brush = [](FLinearColor Color)
    {
        FSlateBrush B; B.DrawAs = ESlateBrushDrawType::RoundedBox;
        B.TintColor = Color; B.OutlineSettings.CornerRadii = FVector4(16,16,16,16);
        B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        return B;
    };
    Style.SetNormal(Brush(Hex(TEXT("23364A"))));
    Style.SetHovered(Brush(Hex(TEXT("304D61"))));
    Style.SetPressed(Brush(Hex(TEXT("365D73"))));
    Style.NormalPadding = FMargin(16, 15); Style.PressedPadding = FMargin(16,15);
    Style.Normal.OutlineSettings.Width=1; Style.Normal.OutlineSettings.Color=Hex(TEXT("3D586B"));
    Style.SetDisabled(Brush(FLinearColor(.025f,.045f,.06f)));
    Button->SetBackgroundColor(FLinearColor::White);
    const bool Primary=Id==TEXT("join") || Id==TEXT("accept") || Text==TEXT("Продолжить занятие");
    if(Primary) { Style.Normal.TintColor=Accent; Style.Hovered.TintColor=Hex(TEXT("B0E9FA")); }
    Button->SetStyle(Style);
    if (auto* Label = Cast<UTextBlock>(Button->GetChildAt(0)))
    {
        auto Font = Label->GetFont(); Font.Size = 19; Label->SetFont(Font);
        Label->SetColorAndOpacity(Primary?Hex(TEXT("102330")):Id.StartsWith(TEXT("confirm:"))?Hex(TEXT("F2B5B8")):Ink);
        Label->SetShadowOffset(FVector2D::ZeroVector);
    }
    Button->OnKemActionClicked.AddDynamic(this, &UKemEscapeMenuWidget::OnAction);
    return Button;
}

UKemMenuArtWidget* UKemEscapeMenuWidget::MakeIcon(const FString& Symbol,float IconSize)
{
    auto* Icon=WidgetTree->ConstructWidget<UKemMenuArtWidget>();
    Icon->Symbol=Symbol; Icon->Color=Muted;
    Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    return Icon;
}
UBorder* UKemEscapeMenuWidget::MakeSurface(FLinearColor Color,float Radius)
{
    auto* Surface=CreatePanelBorder(Color);
    FSlateBrush Brush; Brush.DrawAs=ESlateBrushDrawType::RoundedBox;
    Brush.OutlineSettings.CornerRadii=FVector4(Radius,Radius,Radius,Radius);
    Brush.OutlineSettings.RoundingType=ESlateBrushRoundingType::FixedRadius;
    Brush.OutlineSettings.Width=1; Brush.OutlineSettings.Color=Hex(TEXT("344559"));
    Surface->SetBrush(Brush); Surface->SetPadding(FMargin(24)); return Surface;
}
void UKemEscapeMenuWidget::BuildUI()
{
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>(); Scale->SetStretch(EStretch::ScaleToFit);
    WidgetTree->RootWidget=Scale;
    auto* FrameSize=WidgetTree->ConstructWidget<USizeBox>();
    FrameSize->SetWidthOverride(GetDesignSize().X); FrameSize->SetHeightOverride(GetDesignSize().Y); Scale->AddChild(FrameSize);
    auto* Frame=MakeSurface(Hex(TEXT("142132")),28); Frame->SetPadding(FMargin(bVRLayout?24:1)); FrameSize->AddChild(Frame);
    auto* Root=CreateVerticalBox(); Frame->AddChild(Root);
    auto* Header=MakeSurface(Hex(TEXT("132032")),26); Header->SetPadding(FMargin(28,22)); Root->AddChildToVerticalBox(Header);
    auto* HeaderRow=WidgetTree->ConstructWidget<UHorizontalBox>(); Header->AddChild(HeaderRow);
    auto* LogoSize=WidgetTree->ConstructWidget<USizeBox>(); LogoSize->SetWidthOverride(56); LogoSize->SetHeightOverride(56);
    auto* Logo=MakeSurface(Hex(TEXT("263D4E")),16); Logo->SetPadding(FMargin(15)); LogoSize->AddChild(Logo); Logo->AddChild(MakeIcon(TEXT("cube")));
    HeaderRow->AddChildToHorizontalBox(LogoSize)->SetPadding(FMargin(0,0,16,0));
    auto* Brand=CreateVerticalBox(); HeaderRow->AddChildToHorizontalBox(Brand)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Brand->AddChildToVerticalBox(CreateTextBlock(TEXT("КУБ"),21,Ink));
    RoomLabel=CreateTextBlock(TEXT("Киберуниверситет будущего"),16,Muted);
    Brand->AddChildToVerticalBox(RoomLabel)->SetPadding(FMargin(0,5,0,0));
    auto* Mic=MakeButton(TEXT(""),TEXT("mic")); MicrophoneButton=Mic; Mic->SetToolTipText(FText::FromString(TEXT("Включить / выключить микрофон")));
    auto* MicSize=WidgetTree->ConstructWidget<USizeBox>(); MicSize->SetWidthOverride(26); MicSize->SetHeightOverride(26); MicSize->AddChild(MakeIcon(TEXT("mic"))); Mic->SetContent(MicSize);
    HeaderRow->AddChildToHorizontalBox(Mic)->SetPadding(FMargin(8,0));
    if(!bVRLayout)
    {
        auto* Badge=MakeSurface(Hex(TEXT("263E50")),10); Badge->SetPadding(FMargin(14,15));
        RoleLabel=CreateTextBlock(TEXT("ЛОББИ"),15,Muted); Badge->AddChild(RoleLabel);
        HeaderRow->AddChildToHorizontalBox(Badge)->SetPadding(FMargin(6,0,12,0));
    }
    auto* Close=MakeButton(TEXT("×"),TEXT("close")); Close->SetToolTipText(FText::FromString(TEXT("Закрыть меню"))); HeaderRow->AddChildToHorizontalBox(Close);
    auto* ExitRow=WidgetTree->ConstructWidget<UHorizontalBox>();
    Root->AddChildToVerticalBox(ExitRow)->SetPadding(FMargin(24,12,24,12));
    LobbyButton=MakeButton(TEXT("Выйти в лобби"),TEXT("confirm:leave"));
    ExitRow->AddChildToHorizontalBox(LobbyButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* QuitButton=MakeButton(TEXT("Выйти из игры"),TEXT("confirm:quit"));
    auto* QuitSlot=ExitRow->AddChildToHorizontalBox(QuitButton); QuitSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); QuitSlot->SetPadding(FMargin(12,0,0,0));
    auto* Body=WidgetTree->ConstructWidget<UHorizontalBox>(); Root->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    const TArray<TPair<FString,FString>> Tabs={{TEXT("Комната"),TEXT("room")},{TEXT("Инструменты"),TEXT("tools")},{TEXT("Объекты"),TEXT("objects")},{TEXT("Экран"),TEXT("screen")},{TEXT("Участники"),TEXT("people")},{TEXT("Настройки"),TEXT("audio")}};
    auto* Nav=CreateVerticalBox();
    auto* BottomNav=WidgetTree->ConstructWidget<UHorizontalBox>();
    if(!bVRLayout)
    {
        auto* NavSize=WidgetTree->ConstructWidget<USizeBox>(); NavSize->SetWidthOverride(220); Body->AddChildToHorizontalBox(NavSize);
        auto* NavSurface=MakeSurface(Hex(TEXT("111D2D")),0); NavSurface->SetPadding(FMargin(12,28)); NavSize->AddChild(NavSurface); NavSurface->AddChild(Nav);
    }
    SectionButtons.Reset();
    for(const auto& Item:Tabs)
    {
        auto* Button=MakeButton(TEXT(""),TEXT("tab:")+Item.Value); SectionButtons.Add(Button);
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>(); Button->SetContent(Row);
        auto* IconSize=WidgetTree->ConstructWidget<USizeBox>(); IconSize->SetWidthOverride(22); IconSize->SetHeightOverride(22); IconSize->AddChild(MakeIcon(Item.Value));
        Row->AddChildToHorizontalBox(IconSize)->SetPadding(FMargin(0,0,10,0));
        auto* Label=CreateTextBlock(Item.Key,bVRLayout?12:15,Muted);
        if(bVRLayout)
        {
            auto* Stack=CreateVerticalBox(); Button->SetContent(Stack);
            IconSize->RemoveFromParent(); Stack->AddChildToVerticalBox(IconSize)->SetHorizontalAlignment(HAlign_Center);
            Stack->AddChildToVerticalBox(Label)->SetPadding(FMargin(0,7,0,0)); Label->SetJustification(ETextJustify::Center);
            FButtonStyle NavStyle=Button->GetStyle(); NavStyle.NormalPadding=NavStyle.PressedPadding=FMargin(6,10); Button->SetStyle(NavStyle);
        }
        else Row->AddChildToHorizontalBox(Label)->SetVerticalAlignment(VAlign_Center);
        if(bVRLayout) { auto* NavSlot=BottomNav->AddChildToHorizontalBox(Button); NavSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NavSlot->SetPadding(FMargin(3)); }
        else Nav->AddChildToVerticalBox(Button)->SetPadding(FMargin(0,0,0,12));
    }
    auto* Content=MakeSurface(Hex(TEXT("1B2D3F")),0); Content->SetPadding(FMargin(bVRLayout?32:28,32));
    Body->AddChildToHorizontalBox(Content)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Layers=WidgetTree->ConstructWidget<UOverlay>(); Content->SetContent(Layers); Content->SetPadding(FMargin(0));
    auto* Backdrop=MakeIcon(TEXT("surface")); auto* BackdropSlot=Layers->AddChildToOverlay(Backdrop); BackdropSlot->SetHorizontalAlignment(HAlign_Fill); BackdropSlot->SetVerticalAlignment(VAlign_Fill);
    PageScroll=WidgetTree->ConstructWidget<UScrollBox>(); auto* ScrollSlot=Layers->AddChildToOverlay(PageScroll); ScrollSlot->SetHorizontalAlignment(HAlign_Fill); ScrollSlot->SetVerticalAlignment(VAlign_Fill); ScrollSlot->SetPadding(FMargin(bVRLayout?32:28,32));
    Page=CreateVerticalBox(); PageScroll->AddChild(Page);
    Status=CreateTextBlock(TEXT(""),16,Accent); Status->SetAutoWrapText(true); Root->AddChildToVerticalBox(Status)->SetPadding(FMargin(28,4));
    auto* Footer=MakeSurface(Hex(TEXT("111D2D")),26); Footer->SetPadding(FMargin(28,20)); Root->AddChildToVerticalBox(Footer);
    if(bVRLayout)
    {
        auto* Guidance=CreateVerticalBox(); Footer->AddChild(Guidance);
        Guidance->AddChildToVerticalBox(CreateTextBlock(TEXT("VIVE · КУРОК НА КРАЮ — ПЕРЕМЕЩЕНИЕ МЕНЮ"),16,Accent))->SetPadding(FMargin(0,0,0,14));
        const TArray<TPair<FString,FString>> Hints={{TEXT("1"),TEXT("Кнопка меню\nОткрыть / закрыть панель")},{TEXT("7"),TEXT("Курок\nВыбор в меню и выбранный инструмент")},{TEXT("8"),TEXT("Боковой захват — всегда\nВзять, повернуть и отпустить предмет")}};
        for(const auto& Hint:Hints)
        {
            auto* HintRow=WidgetTree->ConstructWidget<UHorizontalBox>(); Guidance->AddChildToVerticalBox(HintRow)->SetPadding(FMargin(0,0,0,12));
            auto* Badge=MakeSurface(Hex(TEXT("263E50")),12); Badge->SetPadding(FMargin(10,7)); Badge->AddChild(CreateTextBlock(Hint.Key,18,Accent));
            auto* BadgeSlot=HintRow->AddChildToHorizontalBox(Badge); BadgeSlot->SetVerticalAlignment(VAlign_Top); BadgeSlot->SetPadding(FMargin(0,0,16,0));
            auto* HintText=CreateTextBlock(Hint.Value,14,Muted); HintText->SetAutoWrapText(true); HintRow->AddChildToHorizontalBox(HintText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }
        Root->AddChildToVerticalBox(BottomNav)->SetPadding(FMargin(12,8));
    }
    else
    {
        auto* FooterRow=WidgetTree->ConstructWidget<UHorizontalBox>(); Footer->AddChild(FooterRow);
        for(const auto& Key:TArray<TPair<FString,FString>>{{TEXT("Esc"),TEXT("Меню")},{TEXT("F"),TEXT("Захват доступен всегда")},{TEXT("R + мышь"),TEXT("Вращение")}})
        {
            auto* Cap=MakeSurface(Hex(TEXT("26394B")),6); Cap->SetPadding(FMargin(10,8)); Cap->AddChild(CreateTextBlock(Key.Key,12,Ink));
            FooterRow->AddChildToHorizontalBox(Cap)->SetPadding(FMargin(0,0,8,0));
            auto* Hint=CreateTextBlock(Key.Value,12,Muted); auto* HintSlot=FooterRow->AddChildToHorizontalBox(Hint); HintSlot->SetVerticalAlignment(VAlign_Center); HintSlot->SetPadding(FMargin(0,0,20,0)); HintSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }
        auto* SpeakerSize=WidgetTree->ConstructWidget<USizeBox>(); SpeakerSize->SetWidthOverride(22); SpeakerSize->SetHeightOverride(22); SpeakerSize->AddChild(MakeIcon(TEXT("volume"))); FooterRow->AddChildToHorizontalBox(SpeakerSize)->SetVerticalAlignment(VAlign_Center);
        auto* M=UKemEscapeMenuSubsystem::Get(GetOwningPlayer());
        auto* VolumeBox=WidgetTree->ConstructWidget<USizeBox>(); VolumeBox->SetWidthOverride(130);
        auto* Volume=MakeMenuSlider(M?M->MasterVolume:1.f); Volume->OnValueChanged.AddDynamic(this,&UKemEscapeMenuWidget::OnVolume); VolumeBox->AddChild(Volume); FooterRow->AddChildToHorizontalBox(VolumeBox)->SetPadding(FMargin(12,0));
        VolumeLabel=CreateTextBlock(FString::Printf(TEXT("%d%%"),FMath::RoundToInt((M?M->MasterVolume:1.f)*100)),16,Muted); FooterRow->AddChildToHorizontalBox(VolumeLabel);
    }
    RefreshPage();
}

void UKemEscapeMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    if (auto* R = Room(this))
    {
        R->OnJoinOperationFinished.AddDynamic(this,&UKemEscapeMenuWidget::OnRoomResult);
        R->OnHostOperationFinished.AddDynamic(this,&UKemEscapeMenuWidget::OnRoomResult);
    }
}
void UKemEscapeMenuWidget::NativeDestruct()
{
    if (auto* R = Room(this))
    {
        R->OnJoinOperationFinished.RemoveDynamic(this,&UKemEscapeMenuWidget::OnRoomResult);
        R->OnHostOperationFinished.RemoveDynamic(this,&UKemEscapeMenuWidget::OnRoomResult);
    }
    Super::NativeDestruct();
}
void UKemEscapeMenuWidget::OnRoomResult(bool bSuccess, const FString& Message)
{
    if (Status) Status->SetText(FText::FromString(Message));
    if (bSuccess) RefreshPage();
}

void UKemEscapeMenuWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    RefreshTime += DeltaTime; if (RefreshTime < .5f) return; RefreshTime = 0;
    auto* R = Room(this); if (!R || !RoomLabel) return;
    auto* State = R->GetCurrentClassroomState();
    FString Code = State ? State->GetRoomCode() : R->GetCurrentRoomCode();
    RoomLabel->SetText(FText::FromString(R->GetCurrentRoomInfo().bIsInRoom
        ? FString::Printf(TEXT("%s · LAN · %s"),bVRLayout?TEXT("VIVE"):TEXT("Занятие"),*Code)
        : TEXT("Киберуниверситет будущего")));
    if(RoleLabel) RoleLabel->SetText(FText::FromString(R->GetCurrentRoomInfo().bIsInRoom ? R->IsHost()?TEXT("ПРЕПОДАВАТЕЛЬ"):TEXT("СТУДЕНТ"):TEXT("ЛОББИ")));
    if(auto* M=UKemEscapeMenuSubsystem::Get(GetOwningPlayer()))
        if(MicrophoneButton)
        {
            MicrophoneButton->SetIsEnabled(!M->bForcedMuted);
            MicrophoneButton->SetToolTipText(FText::FromString(M->bForcedMuted?TEXT("Микрофон отключён преподавателем"):M->bMicrophoneEnabled?TEXT("Микрофон включён · нажмите, чтобы выключить"):TEXT("Микрофон выключен · нажмите, чтобы включить")));
            FButtonStyle MicStyle=MicrophoneButton->GetStyle(); MicStyle.Normal.TintColor=Hex(M->bMicrophoneEnabled && !M->bForcedMuted?TEXT("263E50"):TEXT("452B39")); MicrophoneButton->SetStyle(MicStyle);
        }
    FString Signature = FString::Printf(TEXT("%d:%d:%d:%s"), R->GetCurrentRoomInfo().bIsInRoom, R->IsHost(), IsValid(State), *Code);
    if (Tab == TEXT("people") && State)
        for (const auto& P : State->GetParticipants()) Signature += P.ParticipantId + P.DisplayName + (P.bForcedMuted ? TEXT("1") : TEXT("0"));
    if (Signature != LastRoomSignature) { LastRoomSignature = Signature; if (Confirmation.IsEmpty()) RefreshPage(); }
}

void UKemEscapeMenuWidget::AddLabel(const FString& Text, int32 Size)
{
    auto* Label = CreateTextBlock(Text,Size,Size>=26?Ink:Muted); Label->SetAutoWrapText(true);
    if(Size>=26) Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold",Size));
    Page->AddChildToVerticalBox(Label)->SetPadding(FMargin(0,0,0,14));
}
void UKemEscapeMenuWidget::AddAction(const FString& Text, const FString& Id, bool bEnabled)
{
    auto* Button = MakeButton(Text,Id); Button->SetIsEnabled(bEnabled);
    Page->AddChildToVerticalBox(Button)->SetPadding(FMargin(0,0,0,10));
}
void UKemEscapeMenuWidget::AddKeyboard(UEditableTextBox* Target, const FString& Characters)
{
    KeyboardTarget = Target;
    for (int32 Start = 0; Start < Characters.Len(); Start += 8)
    {
        auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        Page->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,0,0,5));
        for (int32 I = Start; I < FMath::Min(Start+8,Characters.Len()); ++I)
            Row->AddChildToHorizontalBox(MakeButton(Characters.Mid(I,1),TEXT("key:")+Characters.Mid(I,1)))
                ->SetPadding(FMargin(2));
    }
    AddAction(TEXT("Удалить символ"), TEXT("backspace"));
}

void UKemEscapeMenuWidget::RefreshPage()
{
    if (!Page) return;
    if(PageScroll) PageScroll->ScrollToStart();
    if(LobbyButton)
    {
        auto* CurrentRoom=Room(this);
        LobbyButton->SetIsEnabled(CurrentRoom && CurrentRoom->GetCurrentRoomInfo().bIsInRoom);
    }
    ThicknessLabel=nullptr; StrokePreview=nullptr;
    Page->ClearChildren(); TextEntry = nullptr; KeyboardTarget = nullptr;
    for(UKemActionButton* Button : SectionButtons)
    {
        const bool Selected=Button->ActionId==TEXT("tab:")+Tab;
        FButtonStyle Style=Button->GetStyle();
        Style.Normal.TintColor=Selected ? Hex(TEXT("263E50")) : Hex(TEXT("111D2D"));
        Style.Normal.OutlineSettings.Width=Selected ? 1.5f : 0.f;
        Style.Normal.OutlineSettings.Color=Selected?Accent:FLinearColor::Transparent;
        Button->SetStyle(Style);
        if(auto* Stack=Cast<UVerticalBox>(Button->GetContent()))
            for(UWidget* Child:Stack->GetAllChildren()) if(auto* Label=Cast<UTextBlock>(Child)) Label->SetColorAndOpacity(Selected?Accent:Muted);
        if(auto* Row=Cast<UHorizontalBox>(Button->GetContent()))
            for(UWidget* Child:Row->GetAllChildren()) if(auto* Label=Cast<UTextBlock>(Child)) Label->SetColorAndOpacity(Selected?Accent:Muted);
    }
    auto* R = Room(this);
    auto* M = UKemEscapeMenuSubsystem::Get(GetOwningPlayer());
    auto* State = R ? R->GetCurrentClassroomState() : nullptr;
    bool Host = R && R->IsHost() && State && State->HasAuthority();
    if (!Confirmation.IsEmpty())
    {
        AddLabel(Confirmation == TEXT("quit") ? TEXT("Завершить игру?") :
            Confirmation == TEXT("delete_object") ? TEXT("Удалить выбранный объект?") :
            Confirmation == TEXT("clear") ? TEXT("Удалить все созданные учебные объекты?") :
            Confirmation.StartsWith(TEXT("kick:")) ? TEXT("Исключить участника из комнаты?") :
            Confirmation == TEXT("board") ? TEXT("Очистить рисунки на всех досках?") :
            TEXT("Выйти из аудитории в лобби?"),30);
        if ((Confirmation==TEXT("leave") || Confirmation==TEXT("quit")) && R && R->IsHost()) AddLabel(TEXT("При выходе преподавателя занятие завершается для всех участников."));
        AddAction(TEXT("Подтвердить"),TEXT("accept")); AddAction(TEXT("Отмена"),TEXT("cancel")); return;
    }
    if (Tab == TEXT("room"))
    {
        AddLabel(R && R->GetCurrentRoomInfo().bIsInRoom ? TEXT("Ваша аудитория") : TEXT("Комната"),32);
        if (R && R->GetCurrentRoomInfo().bIsInRoom)
        {
            AddLabel(TEXT("Занятие продолжается, пока открыто меню."),18);
            auto* CodeCard=MakeSurface(Hex(TEXT("25465B")),22); Page->AddChildToVerticalBox(CodeCard)->SetPadding(FMargin(0,20,0,24));
            auto* CodeContent=CreateVerticalBox(); CodeCard->AddChild(CodeContent);
            CodeContent->AddChildToVerticalBox(CreateTextBlock(TEXT("Код для подключения · LAN"),18,Muted));
            const FString Code=State?State->GetRoomCode():R->GetCurrentRoomCode();
            CodeContent->AddChildToVerticalBox(CreateTextBlock(Code,46,Ink))->SetPadding(FMargin(0,20));
            CodeContent->AddChildToVerticalBox(CreateTextBlock(TEXT("Сообщите код участникам в вашей локальной сети."),17,Muted));
            CodeContent->AddChildToVerticalBox(MakeButton(TEXT("Скопировать код"),TEXT("copy")))->SetPadding(FMargin(0,18,0,0));
            AddAction(M && M->bMicrophoneEnabled ? TEXT("Ваш микрофон: включён") : TEXT("Ваш микрофон: выключен"),TEXT("mic"),M && !M->bForcedMuted);
            auto ActionPair=[&](const FString& A,const FString& AId,const FString& B,const FString& BId)
            {
                auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>(); Page->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,8,0,12));
                Row->AddChildToHorizontalBox(MakeButton(A,AId))->SetPadding(FMargin(0,0,14,0)); Row->AddChildToHorizontalBox(MakeButton(B,BId));
            };
            ActionPair(TEXT("Продолжить занятие"),TEXT("close"),TEXT("Инструменты"),TEXT("tab:tools"));
        }
        else if (R)
        {
            AddLabel(TEXT("Создать аудиторию"));
            for (const auto& A : R->GetAuditoriums()) AddAction(A.DisplayName.ToString(), TEXT("host:")+A.AuditoriumId.ToString());
            AddLabel(TEXT("Подключиться по коду"));
            TextEntry = MakeEntry(TEXT("Код комнаты")); Page->AddChildToVerticalBox(TextEntry);
            AddAction(TEXT("Подключиться"),TEXT("join"));
            if (M && M->IsVR()) AddKeyboard(TextEntry, TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789"));
        }
        if (M && M->IsVR()) AddAction(TEXT("Переместить меню перед собой"), TEXT("recenter"));
    }
    else if (Tab == TEXT("tools"))
    {
        BuildToolsPage();
    }
    else if (Tab == TEXT("objects"))
    {
        BuildObjectPage();
    }
    else if (Tab == TEXT("screen"))
    {
        AddLabel(TEXT("Общий экран"),32);
        AddLabel(TEXT("Откройте веб-страницу, опубликованную презентацию или веб-видео по HTTPS-ссылке. Адрес синхронизируется с общим экраном."));
        TextEntry = MakeEntry(TEXT("https://…"));
        if (State) TextEntry->SetText(FText::FromString(State->GetLectureUrl()));
        TextEntry->SetIsEnabled(Host); Page->AddChildToVerticalBox(TextEntry);
        AddAction(TEXT("Открыть на общем экране"),TEXT("url"),Host);
        AddAction(TEXT("Вставить ссылку из буфера"),TEXT("paste"),Host);
        AddLabel(TEXT("Локальные PDF/PPTX и видеофайлы пока не загружаются через это меню. Для них нужна публикация по доступной участникам ссылке."),18);
        if (M && M->IsVR() && Host) AddKeyboard(TextEntry,TEXT("abcdefghijklmnopqrstuvwxyz0123456789:/.-_?=&%"));
    }
    else if (Tab == TEXT("people"))
    {
        AddLabel(TEXT("Участники"),32);
        if (!State) AddLabel(TEXT("Сначала подключитесь к комнате."));
        else for (const auto& P : State->GetParticipants())
        {
            AddLabel(P.DisplayName + (P.bForcedMuted ? TEXT(" · микрофон выключен") : TEXT("")));
            if (Host)
            {
                AddAction(P.bForcedMuted ? TEXT("Разрешить микрофон") : TEXT("Выключить микрофон"),TEXT("mute:")+P.ParticipantId);
                AddAction(TEXT("Исключить…"),TEXT("kick:")+P.ParticipantId);
            }
        }
    }
    else if (Tab == TEXT("audio") && M)
    {
        AddLabel(TEXT("Звук"),32); AddLabel(TEXT("Общая громкость"));
        auto* Slider = MakeMenuSlider(M->MasterVolume); Slider->OnValueChanged.AddDynamic(this,&UKemEscapeMenuWidget::OnVolume);
        Page->AddChildToVerticalBox(Slider)->SetPadding(FMargin(8,16));
        AddLabel(TEXT("Громкость голосов участников"));
        auto* Voice = MakeMenuSlider(M->VoiceVolume); Voice->OnValueChanged.AddDynamic(this,&UKemEscapeMenuWidget::OnVoiceVolume);
        Page->AddChildToVerticalBox(Voice)->SetPadding(FMargin(8,16));
        AddLabel(M->bForcedMuted ? TEXT("Микрофон отключён преподавателем") : M->bMicrophoneEnabled ? TEXT("Микрофон включён") : TEXT("Микрофон выключен"));
        AddAction(M->bMicrophoneEnabled ? TEXT("Выключить микрофон") : TEXT("Включить микрофон"), TEXT("mic"),!M->bForcedMuted);
        AddLabel(TEXT("Настройки громкости и микрофона сохраняются на этом компьютере."),18);
    }
}

void UKemEscapeMenuWidget::BuildToolsPage()
{
    auto* M=UKemEscapeMenuSubsystem::Get(GetOwningPlayer()); if(!M) return;
    AddLabel(TEXT("Инструменты"),32);
    AddLabel(bVRLayout ? TEXT("Выберите действие для курка. Боковой захват доступен всегда.") : TEXT("Выберите действие для мыши. Захват клавишей F доступен постоянно."),18);
    auto* Cards=WidgetTree->ConstructWidget<UHorizontalBox>(); Page->AddChildToVerticalBox(Cards)->SetPadding(FMargin(0,20,0,28));
    const TArray<TPair<FString,FString>> Tools={{TEXT("Кисть"),TEXT("brush")},{TEXT("Указатель"),TEXT("pointer")},{TEXT("Ластик"),TEXT("eraser")}};
    for(const auto& Item:Tools)
    {
        bool Active=Item.Value==TEXT("brush") ? M->Tool==EKemTool::Brush : Item.Value==TEXT("eraser") ? M->Tool==EKemTool::Eraser : M->IsPointerEnabled();
        auto* Button=MakeButton(TEXT(""),Item.Value);
        FButtonStyle Style=Button->GetStyle(); Style.Normal.TintColor=Active?Hex(TEXT("274456")):Hex(TEXT("1B2C3F"));
        Style.Normal.OutlineSettings.Width=Active?1.5f:1.f; Style.Normal.OutlineSettings.Color=Active?Accent:Hex(TEXT("344B5E")); Button->SetStyle(Style);
        auto* CardSize=WidgetTree->ConstructWidget<USizeBox>(); CardSize->SetHeightOverride(115); Button->SetContent(CardSize);
        auto* Column=CreateVerticalBox(); CardSize->AddChild(Column);
        auto* IconSize=WidgetTree->ConstructWidget<USizeBox>(); IconSize->SetWidthOverride(26); IconSize->SetHeightOverride(26);
        auto* Icon=MakeIcon(Item.Value); Icon->Color=Active?Accent:Muted; IconSize->AddChild(Icon);
        auto* IconSlot=Column->AddChildToVerticalBox(IconSize); IconSlot->SetHorizontalAlignment(HAlign_Center); IconSlot->SetPadding(FMargin(0,8,0,16));
        auto* Label=CreateTextBlock(Item.Key,20,Ink); Label->SetJustification(ETextJustify::Center); Column->AddChildToVerticalBox(Label);
        if(Item.Value==TEXT("pointer")) Column->AddChildToVerticalBox(CreateTextBlock(Active?TEXT("Включён"):TEXT("Выключен"),14,Muted))->SetHorizontalAlignment(HAlign_Center);
        auto* CardSlot=Cards->AddChildToHorizontalBox(Button); CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); CardSlot->SetPadding(FMargin(0,0,12,0));
    }
    auto* Options=MakeSurface(Hex(TEXT("17283A")),22); Options->SetPadding(FMargin(20)); Page->AddChildToVerticalBox(Options)->SetPadding(FMargin(0,0,12,22));
    auto* Content=CreateVerticalBox(); Options->AddChild(Content);
    Content->AddChildToVerticalBox(CreateTextBlock(M->Tool==EKemTool::Eraser?TEXT("Ластик"):TEXT("Кисть"),22,Ink));
    auto* Description=CreateTextBlock(TEXT("Рисуйте на общей доске. Штрихи видны всей группе."),16,Muted); Description->SetAutoWrapText(true);
    Content->AddChildToVerticalBox(Description)->SetPadding(FMargin(0,10,0,18));
    Content->AddChildToVerticalBox(CreateTextBlock(TEXT("Цвет"),16,Ink))->SetPadding(FMargin(0,0,0,10));
    auto* Swatches=WidgetTree->ConstructWidget<UHorizontalBox>(); Content->AddChildToVerticalBox(Swatches)->SetPadding(FMargin(0,0,0,18));
    for(int32 I=0;I<6;++I)
    {
        auto* Swatch=MakeButton(TEXT(""),FString::Printf(TEXT("color:%d"),I));
        const FLinearColor Color=Hex(Palette[I]); bool Selected=M->BrushColor.Equals(Color,.001f);
        FButtonStyle Style=Swatch->GetStyle();
        for(auto* B:{&Style.Normal,&Style.Hovered,&Style.Pressed})
        { B->TintColor=Color; B->OutlineSettings.CornerRadii=FVector4(24,24,24,24); B->OutlineSettings.Width=Selected?3:1; B->OutlineSettings.Color=Selected?Ink:Hex(TEXT("41576C")); }
        Style.NormalPadding=Style.PressedPadding=FMargin(0); Swatch->SetStyle(Style);
        Swatch->SetToolTipText(FText::FromString(FString::Printf(TEXT("Цвет %d"),I+1)));
        auto* SwatchSize=WidgetTree->ConstructWidget<USizeBox>(); SwatchSize->SetWidthOverride(bVRLayout?44:36); SwatchSize->SetHeightOverride(bVRLayout?44:36); Swatch->SetContent(SwatchSize);
        Swatches->AddChildToHorizontalBox(Swatch)->SetPadding(FMargin(0,0,12,0));
    }
    ThicknessLabel=CreateTextBlock(FString::Printf(TEXT("Толщина линии                                         %d px"),FMath::RoundToInt(M->BrushSize)),18,Ink);
    Content->AddChildToVerticalBox(ThicknessLabel);
    auto* Slider=MakeMenuSlider((M->BrushSize-1)/31); Slider->OnValueChanged.AddDynamic(this,&UKemEscapeMenuWidget::OnThickness);
    Content->AddChildToVerticalBox(Slider)->SetPadding(FMargin(4,14,4,16));
    auto* Preview=MakeSurface(Hex(TEXT("0D1A2A")),14); Preview->SetPadding(FMargin(30,8));
    auto* PreviewSize=WidgetTree->ConstructWidget<USizeBox>(); PreviewSize->SetHeightOverride(60); Content->AddChildToVerticalBox(PreviewSize); PreviewSize->AddChild(Preview);
    StrokePreview=MakeIcon(TEXT("stroke")); StrokePreview->Color=M->BrushColor; StrokePreview->Thickness=M->BrushSize; Preview->AddChild(StrokePreview);
    AddLabel(bVRLayout?TEXT("Удерживайте правый курок, чтобы рисовать на доске."):TEXT("Удерживайте ЛКМ, чтобы рисовать на доске."),16);
    if(bVRLayout) AddLabel(TEXT("Указатель: курок притягивает. Зажмите Grip для удержания, отпустите для освобождения."),16);

    auto* R=Room(this); if(R && R->IsHost()) AddAction(TEXT("Очистить доски…"),TEXT("confirm:board"));
}

void UKemEscapeMenuWidget::OnVolume(float Value) { if (auto* M = UKemEscapeMenuSubsystem::Get(GetOwningPlayer())) M->SetMasterVolume(Value); if(VolumeLabel) VolumeLabel->SetText(FText::FromString(FString::Printf(TEXT("%d%%"),FMath::RoundToInt(Value*100)))); }
void UKemEscapeMenuWidget::OnThickness(float Value) { if (auto* M = UKemEscapeMenuSubsystem::Get(GetOwningPlayer())) { M->BrushSize = 1.f+Value*31.f;
    if(ThicknessLabel) ThicknessLabel->SetText(FText::FromString(FString::Printf(TEXT("Толщина линии                                         %d px"),FMath::RoundToInt(M->BrushSize))));
    if(StrokePreview) { StrokePreview->Thickness=M->BrushSize; StrokePreview->Refresh(); }
} }

void UKemEscapeMenuWidget::OnAction(const FString& Id)
{
    auto* M = UKemEscapeMenuSubsystem::Get(GetOwningPlayer());
    auto* R = Room(this); auto* State = R ? R->GetCurrentClassroomState() : nullptr;
    const bool Host = R && R->IsHost() && State && State->HasAuthority();
    if (Id == TEXT("close")) { if (M) M->SetMenuOpen(false); return; }
    if (Id.StartsWith(TEXT("tab:"))) { SelectSection(Id.Mid(4)); return; }
    if (Id.StartsWith(TEXT("confirm:"))) { Confirmation = Id.Mid(8); RefreshPage(); return; }
    if (Id == TEXT("cancel")) { Confirmation.Empty(); RefreshPage(); return; }
    if (Id.StartsWith(TEXT("kick:"))) { Confirmation = Id; RefreshPage(); return; }
    if (Id == TEXT("accept"))
    {
        FString Action = Confirmation; Confirmation.Empty();
        if (Action == TEXT("board") && Host)
            for(TActorIterator<AKemDrawingBoardActor> It(GetWorld());It;++It) It->ClearBoard();
        if (Action == TEXT("delete_object") && M)
        {
            const bool Removed=M->RemoveSceneObject(SelectedObject.Get());
            Status->SetText(FText::FromString(Removed ? TEXT("Объект удалён") : TEXT("Отпустите детали объекта перед удалением")));
            if(Removed) { SelectedObject.Reset(); SelectedObjectId.Empty(); }
        }
        if (Action == TEXT("clear") && Host) State->ClearSpawnedPresentationActors();
        if (Action.StartsWith(TEXT("kick:")) && Host) State->KickParticipant(Action.Mid(5));
        if (Action == TEXT("leave")) { if(M) M->SetMenuOpen(false); if(R) R->LeaveCurrentRoom(); return; }
        if (Action == TEXT("quit"))
        {
            if(M) M->SetMenuOpen(false);
            UKismetSystemLibrary::QuitGame(this,GetOwningPlayer(),EQuitPreference::Quit,false); return;
        }
        RefreshPage(); return;
    }
    if (Id == TEXT("recenter")) { if(M) M->RepositionPanel(); return; }
    if (Id == TEXT("copy"))
    {
        FPlatformApplicationMisc::ClipboardCopy(*(State ? State->GetRoomCode() : R ? R->GetCurrentRoomCode() : FString()));
        Status->SetText(FText::FromString(TEXT("Код скопирован"))); return;
    }
    if (Id.StartsWith(TEXT("host:")) && R) { Status->SetText(FText::FromString(TEXT("Создание комнаты…"))); R->HostAuditorium(FName(*Id.Mid(5))); return; }
    if (Id == TEXT("join") && R && TextEntry) { Status->SetText(FText::FromString(TEXT("Подключение…"))); R->JoinRoomByCode(TextEntry->GetText().ToString().TrimStartAndEnd().ToUpper()); return; }
    if (Id.StartsWith(TEXT("key:")) && KeyboardTarget)
    { KeyboardTarget->SetText(FText::FromString((KeyboardTarget->GetText().ToString()+Id.Mid(4)).Left(2048))); return; }
    if (Id == TEXT("backspace") && KeyboardTarget)
    { KeyboardTarget->SetText(FText::FromString(KeyboardTarget->GetText().ToString().LeftChop(1))); return; }
    if (M)
    {
        if (Id == TEXT("brush")) M->SetTool(EKemTool::Brush);
        if (Id == TEXT("eraser")) M->SetTool(EKemTool::Eraser);
        if (Id == TEXT("pointer")) M->TogglePointer();
        if (Id == TEXT("mic")) M->SetMicrophoneEnabled(!M->bMicrophoneEnabled);
        if (Id.StartsWith(TEXT("color:")))
        {
            M->BrushColor = Hex(Palette[FMath::Clamp(FCString::Atoi(*Id.Mid(6)),0,5)]);
            Status->SetText(FText::FromString(TEXT("Цвет кисти изменён")));
        }
    }
    if (Id == TEXT("paste") && TextEntry && Host)
    { FString Value; FPlatformApplicationMisc::ClipboardPaste(Value); TextEntry->SetText(FText::FromString(Value.Left(2048))); return; }
    if (Id == TEXT("url") && Host && TextEntry)
    {
        FString Url = TextEntry->GetText().ToString().TrimStartAndEnd();
        if (!(Url.StartsWith(TEXT("https://")) || Url.StartsWith(TEXT("http://"))) || Url.Len()>2048)
            Status->SetText(FText::FromString(TEXT("Введите HTTP- или HTTPS-ссылку")));
        else { State->SetLectureUrl(Url); Status->SetText(FText::FromString(TEXT("Адрес передан на общий экран"))); }
        return;
    }
    if(Id.StartsWith(TEXT("catalog:"))) { SelectCatalogObject(FName(*Id.Mid(8))); return; }
    if(Id==TEXT("catalog_back")) { SelectedObject.Reset(); SelectedObjectId.Empty(); RefreshPage(); return; }
    if(M && M->CanManageSceneObject(SelectedObject.Get()))
    {
        auto* Engine=Cast<AKemEngineAssembly>(SelectedObject.Get());
        if(Id.StartsWith(TEXT("lesson:")) && Engine)
        {
            const auto Mode=static_cast<EKemEngineLesson>(FMath::Clamp(FCString::Atoi(*Id.Mid(7)),0,2));
            bool Applied=Engine->ConfigureLesson(Mode,Mode!=EKemEngineLesson::Free);
            Status->SetText(FText::FromString(Applied ? TEXT("Режим объекта изменён") : TEXT("Сначала отпустите удерживаемые детали")));
        }
        if(Id==TEXT("object_reset") && Engine)
        {
            const bool Applied=Engine->ConfigureLesson(Engine->GetLesson(),true);
            Status->SetText(FText::FromString(Applied ? TEXT("Упражнение сброшено") : TEXT("Сначала отпустите удерживаемые детали")));
        }
        if(Id==TEXT("object_move"))
        {
            const bool Applied=M->MoveSceneObjectInFront(SelectedObject.Get());
            Status->SetText(FText::FromString(Applied ? TEXT("Объект перемещён перед вами") : TEXT("Сначала отпустите удерживаемые детали")));
        }
    }
    if (Host)
    {
        if (Id.StartsWith(TEXT("mute:")))
        {
            const FString ParticipantId = Id.Mid(5);
            for (const auto& P : State->GetParticipants()) if(P.ParticipantId==ParticipantId)
            { State->SetParticipantMuted(ParticipantId,!P.bForcedMuted); break; }
        }
    }
    RefreshPage();
}

void UKemEscapeMenuWidget::SelectSection(const FString& Section)
{
    static const TArray<FString> Sections={TEXT("room"),TEXT("tools"),TEXT("objects"),TEXT("screen"),TEXT("people"),TEXT("audio")};
    if(!Sections.Contains(Section)) return;
    Tab=Section; Confirmation.Empty(); SelectedObject.Reset(); SelectedObjectId.Empty(); RefreshPage();
}
void UKemEscapeMenuWidget::OnVoiceVolume(float Value)
{
    if(auto* M=UKemEscapeMenuSubsystem::Get(GetOwningPlayer())) M->SetVoiceVolume(Value);
}

USlider* UKemEscapeMenuWidget::MakeMenuSlider(float Value)
{
    auto* Slider=CreateSlider(Value);
    FSliderStyle Style=Slider->GetWidgetStyle();
    Style.BarThickness=6.f;
    FSlateBrush Bar; Bar.DrawAs=ESlateBrushDrawType::RoundedBox; Bar.TintColor=FLinearColor::White;
    Bar.OutlineSettings.CornerRadii=FVector4(3,3,3,3); Bar.OutlineSettings.RoundingType=ESlateBrushRoundingType::FixedRadius;
    Style.NormalBarImage=Style.HoveredBarImage=Style.DisabledBarImage=Bar;
    FSlateBrush Thumb=Bar; Thumb.ImageSize=FVector2D(24,24); Thumb.OutlineSettings.CornerRadii=FVector4(12,12,12,12);
    Style.NormalThumbImage=Style.HoveredThumbImage=Style.DisabledThumbImage=Thumb;
    Style.NormalThumbImage.ImageSize=FVector2D(24,24);
    Style.HoveredThumbImage.ImageSize=FVector2D(24,24);
    Style.DisabledThumbImage.ImageSize=FVector2D(24,24);
    Slider->SetWidgetStyle(Style);
    Slider->SetSliderHandleColor(Accent);
    Slider->SetSliderBarColor(Hex(TEXT("516577")));
    return Slider;
}
UEditableTextBox* UKemEscapeMenuWidget::MakeEntry(const FString& Hint)
{
    auto* Entry=CreateTextBox(Hint);
    FEditableTextBoxStyle Style=Entry->GetWidgetStyle();
    Style.SetFont(FCoreStyle::GetDefaultFontStyle("Regular",22));
    Style.SetPadding(FMargin(12,10));
    FSlateBrush Background;
    Background.DrawAs=ESlateBrushDrawType::RoundedBox;
    Background.TintColor=FLinearColor(.025f,.055f,.08f);
    Background.OutlineSettings.CornerRadii=FVector4(12,12,12,12);
    Background.OutlineSettings.RoundingType=ESlateBrushRoundingType::FixedRadius;
    Background.OutlineSettings.Width=1.f;
    Background.OutlineSettings.Color=Accent;
    Style.SetBackgroundImageNormal(Background);
    Style.SetBackgroundImageHovered(Background);
    Style.SetBackgroundImageFocused(Background);
    Style.SetBackgroundImageReadOnly(Background);
    Style.SetForegroundColor(Ink);
    Style.SetReadOnlyForegroundColor(Ink);
    Style.SetFocusedForegroundColor(Ink);
    Entry->SetWidgetStyle(Style);
    return Entry;
}

void UKemEscapeMenuWidget::ResetNavigation()
{
    Tab=TEXT("room"); Confirmation.Empty(); SelectedObject.Reset(); SelectedObjectId.Empty();
    LastRoomSignature.Empty();
    if(Status) Status->SetText(FText::GetEmpty());
    RefreshPage();
}
void UKemEscapeMenuWidget::AddPreview(UPanelWidget* Parent, const FString& Id, float Height)
{
    const FString Path=TEXT("/Game/KUB/UI/ObjectPreviews/T_")+Id+TEXT(".T_")+Id;
    auto* Texture=LoadObject<UTexture2D>(nullptr,*Path);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>(); Size->SetHeightOverride(Height);
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>(); Scale->SetStretch(EStretch::ScaleToFit);
    auto* Image=WidgetTree->ConstructWidget<UImage>();
    Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    if(Texture) Image->SetBrushFromTexture(Texture,true);
    Scale->AddChild(Image); Size->AddChild(Scale); Parent->AddChild(Size);
}
void UKemEscapeMenuWidget::AddObjectCard(UPanelWidget* Parent, const FString& Name, const FString& Id, bool bEnabled)
{
    auto* Button=MakeButton(Name,TEXT("catalog:")+Id); Button->SetIsEnabled(bEnabled);
    auto* Content=CreateVerticalBox();
    AddPreview(Content,Id,150.f);
    auto* Label=CreateTextBlock(Name,24,Ink); Label->SetJustification(ETextJustify::Center); Label->SetAutoWrapText(true); Label->SetWrapTextAt(200.f);
    Content->AddChildToVerticalBox(Label)->SetPadding(FMargin(0,8,0,0));
    Button->SetContent(Content);
    auto* CardSize=WidgetTree->ConstructWidget<USizeBox>(); CardSize->SetHeightOverride(260.f); CardSize->AddChild(Button);
    auto* CardSlot=Cast<UHorizontalBoxSlot>(Parent->AddChild(CardSize));
    if(CardSlot) { CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); CardSlot->SetPadding(FMargin(0,0,12,0)); }
}
void UKemEscapeMenuWidget::BuildObjectPage()
{
    auto* M=UKemEscapeMenuSubsystem::Get(GetOwningPlayer());
    if(!SelectedObject.IsValid())
    {
        SelectedObject.Reset(); SelectedObjectId.Empty();
        AddLabel(TEXT("Объекты"),32);
        const bool Allowed=M && M->CanManageSceneObjects();
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>(); Page->AddChildToVerticalBox(Row);
        AddObjectCard(Row,TEXT("Двигатель ВАЗ-2112"),TEXT("engine"),Allowed);
        AddObjectCard(Row,TEXT("Куб"),TEXT("cube"),Allowed);
        AddObjectCard(Row,TEXT("Сфера"),TEXT("sphere"),Allowed);
        return;
    }
    const bool Allowed=M && M->CanManageSceneObject(SelectedObject.Get());
    auto* Engine=Cast<AKemEngineAssembly>(SelectedObject.Get());
    AddAction(TEXT("‹ К списку объектов"),TEXT("catalog_back"));
    AddLabel(Engine ? TEXT("Двигатель ВАЗ-2112") : SelectedObjectId==TEXT("cube") ? TEXT("Куб") : TEXT("Сфера"),30);
    AddPreview(Page,SelectedObjectId,95.f);
    auto ActionRow=[&](const FString& A,const FString& AId,const FString& B,const FString& BId)
    {
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();
        Page->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,0,0,10));
        for(const auto& Pair : TArray<TPair<FString,FString>>{{A,AId},{B,BId}})
        {
            auto* Button=MakeButton(Pair.Key,Pair.Value); Button->SetIsEnabled(Allowed);
            if(auto* Label=Cast<UTextBlock>(Button->GetChildAt(0)))
            { auto Font=Label->GetFont(); Font.Size=20; Label->SetFont(Font); Label->SetAutoWrapText(true); }
            auto* ActionSlot=Row->AddChildToHorizontalBox(Button); ActionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ActionSlot->SetPadding(FMargin(0,0,8,0));
        }
    };
    if(Engine)
    {
        TArray<FString> Lines; Engine->GetLessonText().ParseIntoArrayLines(Lines);
        if(Lines.Num()>2) AddLabel(Lines[1]+TEXT("\n")+Lines[2],18);
        AddAction(TEXT("Свободная разборка / сборка"),TEXT("lesson:0"),Allowed);
        ActionRow(TEXT("Учебная разборка"),TEXT("lesson:1"),TEXT("Учебная сборка"),TEXT("lesson:2"));
        ActionRow(TEXT("Сбросить упражнение"),TEXT("object_reset"),TEXT("Переместить и сбросить"),TEXT("object_move"));
    }
    else AddAction(TEXT("Переместить перед собой"),TEXT("object_move"),Allowed);
    AddAction(TEXT("Удалить объект…"),TEXT("confirm:delete_object"),Allowed);

}
void UKemEscapeMenuWidget::SelectCatalogObject(FName ObjectId)
{
    auto* M=UKemEscapeMenuSubsystem::Get(GetOwningPlayer());
    AActor* Object=M ? M->SpawnSceneObject(ObjectId) : nullptr;
    if(!Object)
    {
        if(Status) Status->SetText(FText::FromString(TEXT("Создание объекта сейчас недоступно")));
        return;
    }
    SelectedObject=Object; SelectedObjectId=ObjectId.ToString(); Tab=TEXT("objects");
    Confirmation.Empty();
    if(Status) Status->SetText(FText::GetEmpty());
    RefreshPage();
}
