#include "UI/KemMenuArtWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"

class SKemMenuArt : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SKemMenuArt) {} SLATE_END_ARGS()
    void Construct(const FArguments&) {}
    FString Symbol;
    FLinearColor Color;
    float Thickness=3.f;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(24,24); }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override
    {
        const FVector2D Size=G.GetLocalSize();
        if(Symbol==TEXT("surface"))
        {
            TArray<FSlateGradientStop> Stops;
            Stops.Add(FSlateGradientStop(FVector2D(0,0),FLinearColor(FColor::FromHex(TEXT("203748")))));
            Stops.Add(FSlateGradientStop(FVector2D(Size.X,0),FLinearColor(FColor::FromHex(TEXT("142132")))));
            FSlateDrawElement::MakeGradient(Out,Layer,G.ToPaintGeometry(),Stops,Orient_Horizontal);
            return Layer;
        }
        auto Line=[&](std::initializer_list<FVector2D> Points,float Width=1.6f)
        {
            TArray<FVector2D> P;
            for(const auto& V:Points) P.Add(FVector2D(V.X*Size.X/24.f,V.Y*Size.Y/24.f));
            FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color*Style.GetColorAndOpacityTint(),true,Width);
        };
        if(Symbol==TEXT("stroke")) { Line({{2,20},{22,4}},Thickness); return Layer; }
        if(Symbol==TEXT("cube") || Symbol==TEXT("objects"))
        { Line({{3,7},{12,2},{21,7},{21,17},{12,22},{3,17},{3,7},{12,12},{21,7}}); Line({{12,12},{12,22}}); }
        else if(Symbol==TEXT("room")) { Line({{3,21},{21,21}}); Line({{7,21},{7,5},{15,2},{15,21}}); Line({{15,5},{19,6},{19,21}}); }
        else if(Symbol==TEXT("tools") || Symbol==TEXT("brush"))
        { Line({{6,14},{16,3},{21,3},{21,8},{10,18},{6,14},{3,16},{3,21},{8,21},{10,18}}); Line({{13,6},{18,11}}); }
        else if(Symbol==TEXT("eraser")) { Line({{3,14},{13,3},{22,12},{13,21},{9,21},{3,14}}); Line({{7,10},{17,18},{22,18}}); }
        else if(Symbol==TEXT("pointer")) { Line({{4,9},{4,4},{9,4}}); Line({{15,4},{20,4},{20,9}}); Line({{20,15},{20,20},{15,20}}); Line({{9,20},{4,20},{4,15}}); Line({{8,12},{16,12}}); }
        else if(Symbol==TEXT("screen")) { Line({{2,4},{22,4},{22,18},{2,18},{2,4}}); Line({{8,22},{16,22}}); Line({{12,18},{12,22}}); Line({{10,8},{15,11},{10,14},{10,8}}); }
        else if(Symbol==TEXT("people")) { Line({{8,12},{4,12},{2,17},{2,22}}); Line({{16,12},{20,12},{22,17},{22,22}}); Line({{7,4},{7,8},{12,10},{17,8},{17,4},{12,2},{7,4}}); }
        else if(Symbol==TEXT("audio")) { Line({{2,6},{22,6}}); Line({{2,18},{22,18}}); Line({{7,3},{7,9}}); Line({{16,15},{16,21}}); }
        else if(Symbol==TEXT("mic")) { Line({{9,3},{15,3},{15,13},{12,16},{9,13},{9,3}}); Line({{5,10},{5,14},{8,19},{16,19},{19,14},{19,10}}); Line({{12,19},{12,23}}); }
        else if(Symbol==TEXT("copy")) { Line({{16,3},{3,3},{3,16}}); Line({{8,8},{21,8},{21,21},{8,21},{8,8}}); }
        else if(Symbol==TEXT("play")) { Line({{6,3},{21,12},{6,21},{6,3}}); }
        else if(Symbol==TEXT("exit")) { Line({{9,3},{3,3},{3,21},{9,21}}); Line({{7,12},{22,12},{17,7}}); Line({{22,12},{17,17}}); }
        else if(Symbol==TEXT("volume")) { Line({{3,9},{7,9},{12,4},{12,20},{7,15},{3,15},{3,9}}); Line({{16,7},{19,10},{19,14},{16,17}}); Line({{20,3},{23,9},{23,15},{20,21}}); }
        else if(Symbol==TEXT("close")) { Line({{5,5},{19,19}}); Line({{19,5},{5,19}}); }
        else { Line({{5,8},{12,3},{19,8},{19,16},{12,21},{5,16},{5,8}}); }
        return Layer;
    }
};
TSharedRef<SWidget> UKemMenuArtWidget::RebuildWidget()
{
    SAssignNew(Art,SKemMenuArt); Refresh(); return Art.ToSharedRef();
}
void UKemMenuArtWidget::Refresh()
{
    if(Art) { Art->Symbol=Symbol; Art->Color=Color; Art->Thickness=Thickness; Art->Invalidate(EInvalidateWidgetReason::Paint); }
}
void UKemMenuArtWidget::ReleaseSlateResources(bool Children) { Super::ReleaseSlateResources(Children); Art.Reset(); }
