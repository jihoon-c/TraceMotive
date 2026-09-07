#include "TMVariableValueTrace.h"
#include "Engine/World.h"
#include "TMStyle.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformTime.h"
#include "Styling/AppStyle.h"
#include "TMEngineCompatibility.h"
#include "TMLocalization.h"
#include "ToolMenus.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FName VariableTraceTabId(TEXT("TraceMotive.VariableValueTrace"));
TWeakPtr<SDockTab> VariableTraceTab;
bool bVariableTraceTabRegistered = false;

FSlateFontInfo VariableTraceFont(const FName Style, int32 Size) { return FCoreStyle::GetDefaultFontStyle(Style, Size); }
FLinearColor VariableTraceColor(int32 Index)
{
    static const FLinearColor Colors[] = {FLinearColor(.16f, .82f, .72f), FLinearColor(1.f, .67f, .2f),
                                          FLinearColor(.76f, .42f, 1.f), FLinearColor(.35f, .64f, 1.f)};
    return Colors[Index % UE_ARRAY_COUNT(Colors)];
}
UClass *ResolveTraceClass(const FString &Query)
{
    if (UClass *Found = FindObject<UClass>(nullptr, *Query))
        return Found;
    for (TObjectIterator<UClass> It; It; ++It)
        if ((*It)->GetName().Equals(Query, ESearchCase::IgnoreCase) ||
            (*It)->GetPathName().Equals(Query, ESearchCase::IgnoreCase))
            return *It;
    return nullptr;
}
FString TraceTypeName(const FProperty* Property)
{
    if (!Property) return TEXT("Value");
    if (const FArrayProperty* Array = CastField<FArrayProperty>(Property)) return FString::Printf(TEXT("Array<%s>"), *TraceTypeName(Array->Inner));
    if (const FSetProperty* Set = CastField<FSetProperty>(Property)) return FString::Printf(TEXT("Set<%s>"), *TraceTypeName(Set->ElementProp));
    if (const FMapProperty* Map = CastField<FMapProperty>(Property)) return FString::Printf(TEXT("Map<%s, %s>"), *TraceTypeName(Map->KeyProp), *TraceTypeName(Map->ValueProp));
    if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property)) return Enum->GetEnum() ? Enum->GetEnum()->GetName() : TEXT("Enum");
    if (const FByteProperty* Byte = CastField<FByteProperty>(Property)) if (Byte->Enum) return Byte->Enum->GetName();
    if (const FStructProperty* Struct = CastField<FStructProperty>(Property)) return Struct->Struct ? Struct->Struct->GetName() : TEXT("Struct");
    if (const FSoftClassProperty* SoftClass = CastField<FSoftClassProperty>(Property)) return FString::Printf(TEXT("SoftClass<%s>"), SoftClass->MetaClass ? *SoftClass->MetaClass->GetName() : TEXT("Object"));
    if (const FSoftObjectProperty* SoftObject = CastField<FSoftObjectProperty>(Property)) return FString::Printf(TEXT("SoftObject<%s>"), SoftObject->PropertyClass ? *SoftObject->PropertyClass->GetName() : TEXT("Object"));
    if (const FClassProperty* Class = CastField<FClassProperty>(Property)) return FString::Printf(TEXT("Class<%s>"), Class->MetaClass ? *Class->MetaClass->GetName() : TEXT("Object"));
    if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property)) return Object->PropertyClass ? Object->PropertyClass->GetName() : TEXT("Object");
    if (const FInterfaceProperty* Interface = CastField<FInterfaceProperty>(Property)) return FString::Printf(TEXT("Interface<%s>"), Interface->InterfaceClass ? *Interface->InterfaceClass->GetName() : TEXT("Object"));
    if (CastField<FBoolProperty>(Property)) return TEXT("Bool");
    if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property)) return Numeric->GetCPPType();
    if (CastField<FNameProperty>(Property)) return TEXT("Name");
    if (CastField<FStrProperty>(Property)) return TEXT("String");
    if (CastField<FTextProperty>(Property)) return TEXT("Text");
    if (CastField<FDelegateProperty>(Property)) return TEXT("Delegate");
    if (CastField<FMulticastDelegateProperty>(Property)) return TEXT("MulticastDelegate");
    return Property->GetCPPType();
}
FString SummarizeTraceText(FString Text, int32 Limit = 120)
{
    Text.ReplaceInline(TEXT("\n"), TEXT(" ")); Text.ReplaceInline(TEXT("\r"), TEXT(" "));
    return Text.Len() > Limit ? Text.Left(Limit - 3) + TEXT("...") : Text;
}
FString DescribeTraceValue(const FProperty* Property, const void* Value, int32 Depth = 0)
{
    if (!Property || !Value) return TEXT("None");
    if (Depth >= 5) return FString::Printf(TEXT("<%s>"), *TraceTypeName(Property));
    if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper Values(Array, Value); TArray<FString> Items;
        const int32 Shown = FMath::Min(Values.Num(), 3);
        for (int32 Index = 0; Index < Shown; ++Index) Items.Add(DescribeTraceValue(Array->Inner, Values.GetRawPtr(Index), Depth + 1));
        if (Values.Num() > Shown) Items.Add(FString::Printf(TEXT("+%d more"), Values.Num() - Shown));
        return FString::Printf(TEXT("Array<%s> [%d] { %s }"), *TraceTypeName(Array->Inner), Values.Num(), *FString::Join(Items, TEXT(", ")));
    }
    if (const FSetProperty* Set = CastField<FSetProperty>(Property))
    {
        FScriptSetHelper Values(Set, Value); TArray<FString> Items;
        for (int32 Index = 0; Index < Values.GetMaxIndex() && Items.Num() < 3; ++Index) if (Values.IsValidIndex(Index)) Items.Add(DescribeTraceValue(Set->ElementProp, Values.GetElementPtr(Index), Depth + 1));
        if (Values.Num() > Items.Num()) Items.Add(FString::Printf(TEXT("+%d more"), Values.Num() - Items.Num()));
        return FString::Printf(TEXT("Set<%s> [%d] { %s }"), *TraceTypeName(Set->ElementProp), Values.Num(), *FString::Join(Items, TEXT(", ")));
    }
    if (const FMapProperty* Map = CastField<FMapProperty>(Property))
    {
        FScriptMapHelper Values(Map, Value); TArray<FString> Items;
        for (int32 Index = 0; Index < Values.GetMaxIndex() && Items.Num() < 3; ++Index) if (Values.IsValidIndex(Index)) Items.Add(DescribeTraceValue(Map->KeyProp, Values.GetKeyPtr(Index), Depth + 1) + TEXT(": ") + DescribeTraceValue(Map->ValueProp, Values.GetValuePtr(Index), Depth + 1));
        if (Values.Num() > Items.Num()) Items.Add(FString::Printf(TEXT("+%d more"), Values.Num() - Items.Num()));
        return FString::Printf(TEXT("Map<%s, %s> [%d] { %s }"), *TraceTypeName(Map->KeyProp), *TraceTypeName(Map->ValueProp), Values.Num(), *FString::Join(Items, TEXT(", ")));
    }
    if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property)) { const UObject* Item = Object->GetObjectPropertyValue(Value); return Item ? Item->GetName() : TEXT("None"); }
    if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property)) return Enum->GetEnum() ? Enum->GetEnum()->GetNameStringByValue(Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value)) : TEXT("None");
    if (const FByteProperty* Byte = CastField<FByteProperty>(Property)) if (Byte->Enum) return Byte->Enum->GetNameStringByValue(Byte->GetPropertyValue(Value));
    FString Result; Property->ExportText_Direct(Result, Value, nullptr, nullptr, PPF_None);
    return Result;
}
FString DescribeTraceDetails(const FProperty* Property, const void* Value, int32 Depth = 0)
{
    if (!Property || !Value || Depth >= 5) return DescribeTraceValue(Property, Value, Depth);
    if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper Items(Array, Value); TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("%s  Num=%d"), *TraceTypeName(Property), Items.Num()));
        for (int32 Index = 0; Index < Items.Num(); ++Index)
            Lines.Add(FString::Printf(TEXT("  [%d] %s"), Index, *DescribeTraceDetails(Array->Inner, Items.GetRawPtr(Index), Depth + 1)));
        return FString::Join(Lines, TEXT("\n"));
    }
    if (const FSetProperty* Set = CastField<FSetProperty>(Property))
    {
        FScriptSetHelper Items(Set, Value); TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("%s  Num=%d"), *TraceTypeName(Property), Items.Num()));
        int32 DisplayIndex = 0;
        for (int32 InternalIndex = 0; InternalIndex < Items.GetMaxIndex(); ++InternalIndex)
        {
            if (!Items.IsValidIndex(InternalIndex)) continue;
            Lines.Add(FString::Printf(TEXT("  [%d] %s"), DisplayIndex, *DescribeTraceDetails(Set->ElementProp, Items.GetElementPtr(InternalIndex), Depth + 1)));
            ++DisplayIndex;
        }
        return FString::Join(Lines, TEXT("\n"));
    }
    if (const FMapProperty* Map = CastField<FMapProperty>(Property))
    {
        FScriptMapHelper Items(Map, Value); TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("%s  Num=%d"), *TraceTypeName(Property), Items.Num()));
        int32 DisplayIndex = 0;
        for (int32 InternalIndex = 0; InternalIndex < Items.GetMaxIndex(); ++InternalIndex)
        {
            if (!Items.IsValidIndex(InternalIndex)) continue;
            const FString Key = DescribeTraceDetails(Map->KeyProp, Items.GetKeyPtr(InternalIndex), Depth + 1);
            const FString MapValue = DescribeTraceDetails(Map->ValueProp, Items.GetValuePtr(InternalIndex), Depth + 1);
            Lines.Add(FString::Printf(TEXT("  [%d] Key: %s\n      Value: %s"), DisplayIndex, *Key, *MapValue));
            ++DisplayIndex;
        }
        return FString::Join(Lines, TEXT("\n"));
    }
    if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
    {
        const UObject* Object = ObjectProperty->GetObjectPropertyValue(Value);
        if (!Object) return TEXT("None");
        TArray<FString> Lines; Lines.Add(FString::Printf(TEXT("%s  (Class: %s)"), *Object->GetName(), *Object->GetClass()->GetName()));
        int32 Count = 0;
        for (TFieldIterator<FProperty> It(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
        {
            const FProperty* Child = *It;
            if (!Child->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible) || Child->HasAnyPropertyFlags(CPF_Parm)) continue;
            Lines.Add(FString::Printf(TEXT("    %s: %s"), *Child->GetName(), *DescribeTraceDetails(Child, Child->ContainerPtrToValuePtr<void>(Object), Depth + 1)));
            ++Count;
        }
        return FString::Join(Lines, TEXT("\n"));
    }
    if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        TArray<FString> Lines; Lines.Add(TraceTypeName(Property));
        int32 Count = 0;
        for (TFieldIterator<FProperty> It(StructProperty->Struct); It; ++It)
        {
            const FProperty* Child = *It;
            Lines.Add(FString::Printf(TEXT("    %s: %s"), *Child->GetName(), *DescribeTraceDetails(Child, Child->ContainerPtrToValuePtr<void>(Value), Depth + 1)));
            ++Count;
        }
        return FString::Join(Lines, TEXT("\n"));
    }
    return DescribeTraceValue(Property, Value, Depth);
}
FString ExportTraceRaw(const FProperty* Property, UObject* Container)
{
    TArray<FString> Parts;
    for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
    {
        FString Part;
        Property->ExportText_InContainer(Index, Part, Container, Container, Container, PPF_None);
        Parts.Add(FString::Printf(TEXT("%d:%s"), Part.Len(), *Part));
    }
    return FString::Join(Parts, TEXT("|"));
}
FString DescribeRootTraceValue(const FProperty* Property, const UObject* Container, bool bDetailed)
{
    if (Property->ArrayDim <= 1)
    {
        const void* Value = Property->ContainerPtrToValuePtr<void>(Container);
        return bDetailed ? DescribeTraceDetails(Property, Value) : DescribeTraceValue(Property, Value);
    }
    TArray<FString> Lines;
    Lines.Add(FString::Printf(TEXT("%s[%d]"), *TraceTypeName(Property), Property->ArrayDim));
    for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
    {
        const void* Value = Property->ContainerPtrToValuePtr<void>(Container, Index);
        Lines.Add(FString::Printf(TEXT("  [%d] %s"), Index,
            *(bDetailed ? DescribeTraceDetails(Property, Value) : DescribeTraceValue(Property, Value))));
    }
    return FString::Join(Lines, bDetailed ? TEXT("\n") : TEXT(", "));
}
struct FVariableChange
{
    FString Lane, ObjectName, OldValue, NewValue, TypeName, OldDetails, NewDetails;
    double Time = 0.0;
    int32 BurstCount = 1;
};
struct FTraceDetailNode
{
    FString Name, Value;
    TArray<TSharedPtr<FTraceDetailNode>> Children;
};

class SVariableValueTrace : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SVariableValueTrace) {}
    SLATE_END_ARGS()
    ~SVariableValueTrace() override { UnregisterPIEDelegates(); }
    void Construct(const FArguments &)
    {
        StartTime = FPlatformTime::Seconds();
        RegisterPIEDelegates();
        RefreshClassOptions();
        ChildSlot[SNew(SBorder).Padding(10).BorderImage(FAppStyle::GetBrush(
            TEXT("Brushes.Panel")))[SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[BuildHeader()] +
                                    SVerticalBox::Slot().AutoHeight().Padding(0, 6)[SAssignNew(ConfigurationPanel, SBox).Visibility(this, &SVariableValueTrace::ConfigurationVisibility)[BuildControls()]] +
                                    SVerticalBox::Slot().FillHeight(1)[SAssignNew(Timeline, SScrollBox).Orientation(Orient_Horizontal).ScrollBarVisibility(EVisibility::Visible)] +
                                    SVerticalBox::Slot().AutoHeight().Padding(0, 6)[BuildDetailsPanel()] +
                                    SVerticalBox::Slot().AutoHeight().Padding(
                                        0, 8, 0, 0)[SAssignNew(Status, STextBlock)
                                                        .Font(VariableTraceFont(TEXT("Regular"), 9))
                                                        .ColorAndOpacity(FLinearColor(.55f, .60f, .66f))]]];
        RebuildTimeline();
        RebuildDetailsPanel();
    }
    void Tick(const FGeometry &Geometry, const double CurrentTime, const float DeltaTime) override
    {
        SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
        const double Now = FPlatformTime::Seconds();
        if (bIsTracing && bPIEActive && !bPaused && TargetClass && Now - LastSample >= 0.10)
        {
            LastSample = Now;
            Sample(Now);
        }
        if (bDirty && Now - LastBuild >= 0.12)
        {
            LastBuild = Now;
            RebuildTimeline();
        }
    }

  private:
    TSharedRef<SWidget> BuildHeader()
    {
        const FLinearColor LiveColor(.93f, .28f, .32f);
        return SNew(SBorder).Padding(FMargin(8, 6)).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [SNew(SBorder).Padding(FMargin(8, 3)).BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox"))).BorderBackgroundColor(LiveColor)
                        [SNew(STextBlock).Text(this, &SVariableValueTrace::LiveLabel).Font(VariableTraceFont(TEXT("Bold"), 9)).ColorAndOpacity(FLinearColor::White)]]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7, 0)
                    [SNew(STextBlock).Text(this, &SVariableValueTrace::FrameLabel).Font(VariableTraceFont(TEXT("Regular"), 8)).ColorAndOpacity(FLinearColor(.56f,.60f,.66f))]
                + SHorizontalBox::Slot().FillWidth(1)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3,0)
                    [SNew(SButton).ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton")).Text(TMLoc::Text(TEXT("Filter"), TEXT("Filter"))).OnClicked(this, &SVariableValueTrace::ToggleConfiguration)]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3,0)
                    [SNew(SButton).ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton")).Text(TMLoc::Text(TEXT("Clear"), TEXT("Clear"))).OnClicked(this, &SVariableValueTrace::Clear)]];
    }
    FText LiveLabel() const { return !bIsTracing ? TMLoc::Text(TEXT("Waiting"), TEXT("Waiting")) : (!bPIEActive ? TMLoc::Text(TEXT("Armed"), TEXT("Armed")) : (bPaused ? TMLoc::Text(TEXT("Paused"), TEXT("Paused")) : TMLoc::Text(TEXT("Live"), TEXT("Live")))); }
    FText FrameLabel() const
    {
        if (!bIsTracing) return TMLoc::Text(TEXT("Select a class and variables"), TEXT("Select a class and variables"));
        const double Elapsed = FPlatformTime::Seconds() - StartTime;
        return FText::FromString(FString::Printf(TEXT("Frame %d - %02d:%02d"), SampleFrame, int32(Elapsed) / 60, int32(Elapsed) % 60));
    }
    EVisibility ConfigurationVisibility() const { return bShowConfiguration ? EVisibility::Visible : EVisibility::Collapsed; }
    FReply ToggleConfiguration() { bShowConfiguration = !bShowConfiguration; return FReply::Handled(); }
    void RegisterPIEDelegates()
    {
        PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddSP(this, &SVariableValueTrace::HandlePostPIEStarted);
        EndPIEHandle = FEditorDelegates::EndPIE.AddSP(this, &SVariableValueTrace::HandleEndPIE);
    }
    void UnregisterPIEDelegates()
    {
        if (PostPIEStartedHandle.IsValid()) FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);
        if (EndPIEHandle.IsValid()) FEditorDelegates::EndPIE.Remove(EndPIEHandle);
    }
    void HandlePostPIEStarted(bool) { bPIEActive = true; Values.Reset(); DisplayValues.Reset(); DetailValues.Reset(); if (Status.IsValid()) Status->SetText(TMLoc::Text(TEXT("PIE started. Trace is active."), TEXT("PIE started. Trace is active."))); }
    void HandleEndPIE(bool) { bPIEActive = false; bPaused = false; Values.Reset(); DisplayValues.Reset(); DetailValues.Reset(); if (Status.IsValid()) Status->SetText(TMLoc::Text(TEXT("PIE ended. Trace is armed for the next session."), TEXT("PIE ended. Trace is armed for the next session."))); }
    TSharedRef<SWidget> BuildDetailsPanel()
    {
        return SNew(SBorder).Padding(5).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))
            [SNew(SBox).HeightOverride(230)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(2)[SNew(SSearchBox).HintText(TMLoc::Text(TEXT("Search details"), TEXT("Search details"))).OnTextChanged(this, &SVariableValueTrace::OnDetailsSearchChanged)]
                    + SVerticalBox::Slot().AutoHeight()[SNew(SBorder).Padding(FMargin(6,3)).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))[SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(.42f)[SNew(STextBlock).Text(TMLoc::Text(TEXT("Name"), TEXT("Name"))).Font(VariableTraceFont(TEXT("Bold"),8)).ColorAndOpacity(FLinearColor(.62f,.65f,.70f))]
                        + SHorizontalBox::Slot().FillWidth(.58f)[SNew(STextBlock).Text(TMLoc::Text(TEXT("Value"), TEXT("Value"))).Font(VariableTraceFont(TEXT("Bold"),8)).ColorAndOpacity(FLinearColor(.62f,.65f,.70f))]]]
                    + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(DetailsContent, SVerticalBox)]]]];
    }
    void OnDetailsSearchChanged(const FText& Text) { DetailsSearchText = Text.ToString(); RebuildDetailsPanel(); }
    TSharedPtr<FTraceDetailNode> ParseDetailSnapshot(const FString& Title, const FString& Snapshot) const
    {
        TSharedPtr<FTraceDetailNode> Root = MakeShared<FTraceDetailNode>(); Root->Name = Title;
        TArray<FString> Lines; Snapshot.ParseIntoArrayLines(Lines, false);
        TArray<TPair<int32, TSharedPtr<FTraceDetailNode>>> Stack; Stack.Add(TPair<int32, TSharedPtr<FTraceDetailNode>>(-1, Root));
        for (const FString& RawLine : Lines)
        {
            int32 Spaces = 0; while (Spaces < RawLine.Len() && RawLine[Spaces] == TCHAR(' ')) ++Spaces;
            const FString Line = RawLine.TrimStartAndEnd(); if (Line.IsEmpty()) continue;
            TSharedPtr<FTraceDetailNode> Node = MakeShared<FTraceDetailNode>();
            int32 Split = INDEX_NONE;
            if (Line.StartsWith(TEXT("[")) && Line.FindChar(TCHAR(']'), Split)) { Node->Name = Line.Left(Split + 1); Node->Value = Line.Mid(Split + 1).TrimStartAndEnd(); }
            else if (Line.Find(TEXT("  Num="), ESearchCase::CaseSensitive, ESearchDir::FromStart) != INDEX_NONE) { Split = Line.Find(TEXT("  Num=")); Node->Name = Line.Left(Split); Node->Value = Line.Mid(Split + 2); }
            else if (Line.FindChar(TCHAR(':'), Split)) { Node->Name = Line.Left(Split); Node->Value = Line.Mid(Split + 1).TrimStartAndEnd(); }
            else { Node->Name = Line; }
            const int32 Indent = Spaces;
            while (Stack.Num() > 1 && Stack.Last().Key >= Indent) Stack.Pop();
            Stack.Last().Value->Children.Add(Node); Stack.Add(TPair<int32, TSharedPtr<FTraceDetailNode>>(Indent, Node));
        }
        return Root;
    }
    bool DetailNodeMatches(const TSharedPtr<FTraceDetailNode>& Node) const
    {
        if (DetailsSearchText.IsEmpty() || Node->Name.Contains(DetailsSearchText, ESearchCase::IgnoreCase) || Node->Value.Contains(DetailsSearchText, ESearchCase::IgnoreCase)) return true;
        for (const TSharedPtr<FTraceDetailNode>& Child : Node->Children) if (DetailNodeMatches(Child)) return true;
        return false;
    }
    TSharedRef<SWidget> BuildDetailNameValueRow(const TSharedPtr<FTraceDetailNode>& Node, int32 Depth) const
    {
        const FLinearColor Marker = Depth == 0 ? FLinearColor(.15f,.55f,.95f) : (Depth == 1 ? FLinearColor(.08f,.78f,.65f) : FLinearColor(.35f,.45f,.62f));
        return SNew(SBorder).Padding(FMargin(4,2)).BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox"))).BorderBackgroundColor(Depth % 2 == 0 ? FLinearColor(.045f,.047f,.052f,1.f) : FLinearColor(.065f,.067f,.072f,1.f))
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(.42f).VAlign(VAlign_Center)[SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(Depth * 10.f,0,6,0))[SNew(SBox).WidthOverride(9).HeightOverride(4)[SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox"))).BorderBackgroundColor(Marker)]]
                    + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(Node->Name)).Font(VariableTraceFont(TEXT("Regular"),9)).ColorAndOpacity(FLinearColor(.78f,.80f,.84f))]]
                + SHorizontalBox::Slot().FillWidth(.58f)[SNew(STextBlock).Text(FText::FromString(Node->Value)).Font(VariableTraceFont(TEXT("Regular"),9)).ColorAndOpacity(FLinearColor(.74f,.76f,.80f)).AutoWrapText(true)]];
    }
    TSharedRef<SWidget> BuildDetailNodeWidget(const TSharedPtr<FTraceDetailNode>& Node, int32 Depth) const
    {
        TSharedRef<SVerticalBox> ChildrenBox = SNew(SVerticalBox);
        for (const TSharedPtr<FTraceDetailNode>& Child : Node->Children) if (DetailNodeMatches(Child)) ChildrenBox->AddSlot().AutoHeight()[BuildDetailNodeWidget(Child, Depth + 1)];
        if (Node->Children.IsEmpty()) return BuildDetailNameValueRow(Node, Depth);
        return SNew(SExpandableArea).InitiallyCollapsed(false).AreaTitleFont(VariableTraceFont(TEXT("Regular"),9)).HeaderPadding(FMargin(0)).BodyBorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
            .HeaderContent()[BuildDetailNameValueRow(Node, Depth)].BodyContent()[ChildrenBox];
    }
    void RebuildDetailsPanel()
    {
        if (!DetailsContent.IsValid()) return; DetailsContent->ClearChildren();
        if (!Changes.IsValidIndex(SelectedChangeIndex))
        {
            TSharedPtr<FTraceDetailNode> Empty = MakeShared<FTraceDetailNode>(); Empty->Name = TMLoc::String(TEXT("Select a change card to inspect its captured value."), TEXT("Select a change card to inspect its captured value."));
            DetailsContent->AddSlot().AutoHeight()[BuildDetailNameValueRow(Empty, 0)]; return;
        }
        const FVariableChange& Change = Changes[SelectedChangeIndex];
        TSharedPtr<FTraceDetailNode> Root = MakeShared<FTraceDetailNode>(); Root->Name = Change.Lane; Root->Value = Change.TypeName;
        Root->Children.Add(ParseDetailSnapshot(TEXT("Before"), Change.OldDetails)); Root->Children.Add(ParseDetailSnapshot(TEXT("After"), Change.NewDetails));
        DetailsContent->AddSlot().AutoHeight()[BuildDetailNodeWidget(Root, 0)];
    }
    void RefreshClassOptions()
    {
        ClassOptions.Reset();
        for (TObjectIterator<UClass> It; It; ++It)
        {
            UClass* Class = *It;
            if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;
            if (Class->ClassGeneratedBy || Class->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))) ClassOptions.Add(MakeShared<FString>(Class->GetPathName()));
        }
        ClassOptions.Sort([](const TSharedPtr<FString>& A, const TSharedPtr<FString>& B) { return *A < *B; });
    }
    void RefreshPropertyOptions()
    {
        PropertyCategories.Reset(); PropertiesByCategory.Reset();
        if (!TargetClass) return;
        for (TFieldIterator<FProperty> It(TargetClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
        {
            FProperty* Property = *It;
            if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible) || Property->HasAnyPropertyFlags(CPF_Parm)) continue;
            FString Category = Property->GetMetaData(TEXT("Category"));
            if (Category.IsEmpty()) Category = TEXT("Default");
            if (!PropertiesByCategory.Contains(Category)) PropertyCategories.Add(Category);
            PropertiesByCategory.FindOrAdd(Category).Add(Property->GetName());
        }
        PropertyCategories.Sort();
        for (const FString& Category : PropertyCategories) PropertiesByCategory.FindChecked(Category).Sort();
    }
    void RefreshClassMenu()
    {
        if (!ClassMenuList.IsValid()) return;
        ClassMenuList->ClearChildren();
        const FString Query = ClassSearchText.TrimStartAndEnd();
        for (const TSharedPtr<FString>& Item : ClassOptions)
        {
            if (!Query.IsEmpty() && !Item->Contains(Query, ESearchCase::IgnoreCase)) continue;
            ClassMenuList->AddSlot().AutoHeight()[SNew(SButton).ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton")).ContentPadding(FMargin(7,3)).Text(FText::FromString(*Item)).OnClicked_Lambda([this, Item](){ OnClassSelected(Item, ESelectInfo::Direct); if (ClassPicker.IsValid()) ClassPicker->SetIsOpen(false); return FReply::Handled(); })];
        }
    }
    void OnClassSearchChanged(const FText& Text) { ClassSearchText = Text.ToString(); RefreshClassMenu(); }
    TSharedRef<SWidget> BuildClassMenu()
    {
        return SNew(SBorder).Padding(3).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))[SNew(SBox).WidthOverride(420).MaxDesiredHeight(360)[SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(3)[SNew(SSearchBox).HintText(TMLoc::Text(TEXT("Search classes"), TEXT("Search classes"))).OnTextChanged(this, &SVariableValueTrace::OnClassSearchChanged)]
            + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(ClassMenuList, SVerticalBox)]]]];
    }
    void OnClassSelected(TSharedPtr<FString> Item, ESelectInfo::Type)
    {
        UClass* NewTargetClass = Item.IsValid() ? ResolveTraceClass(*Item) : nullptr;
        if (NewTargetClass == TargetClass)
        {
            SelectedClass = Item;
            RefreshPropertyOptions();
            return;
        }

        SelectedClass = Item;
        TargetClass = NewTargetClass;
        SelectedVariables.Reset();
        ActiveLanes.Reset();
        Lanes.Reset();
        RefreshPropertyOptions();
        Values.Reset();
        DisplayValues.Reset();
        DetailValues.Reset();
        Changes.Reset();
        SelectedChangeIndex = INDEX_NONE;
        RebuildDetailsPanel();
        bIsTracing = false;
        bDirty = true;
        if (Configuration.IsValid()) Configuration->SetText(TargetClass ? TMLoc::Text(TEXT("Select properties to trace."), TEXT("Select properties to trace.")) : TMLoc::Text(TEXT("Choose a class."), TEXT("Choose a class.")));
    }
    FText SelectedClassLabel() const { return SelectedClass.IsValid() ? FText::FromString(*SelectedClass) : TMLoc::Text(TEXT("Select class"), TEXT("Select class")); }
    FText SelectedVariablesLabel() const
    {
        return SelectedVariables.IsEmpty() ? TMLoc::Text(TEXT("Select variables"), TEXT("Select variables")) : FText::FromString(FString::Printf(TEXT("%d variables selected"), SelectedVariables.Num()));
    }
    FReply ToggleVariable(const FString Name)
    {
        if (SelectedVariables.Contains(Name)) SelectedVariables.Remove(Name); else SelectedVariables.Add(Name);
        SelectedVariables.Sort(); return FReply::Handled();
    }
    void RefreshVariableMenu()
    {
        if (!VariableMenuList.IsValid()) return;
        VariableMenuList->ClearChildren();
        if (!TargetClass || PropertyCategories.IsEmpty())
        {
            VariableMenuList->AddSlot().AutoHeight().Padding(8)
            [SNew(STextBlock)
                .Text(TMLoc::Text(TEXT("No traceable properties"), TEXT("No traceable properties")))
                .Font(VariableTraceFont(TEXT("Regular"), 9))];
            return;
        }

        const FString Query = VariableSearchText.TrimStartAndEnd();
        int32 VisiblePropertyCount = 0;
        for (const FString& Category : PropertyCategories)
        {
            TArray<FProperty*> VisibleProperties;
            for (const FString& Name : PropertiesByCategory.FindChecked(Category))
            {
                FProperty* Property = FindFProperty<FProperty>(TargetClass, *Name);
                if (!Property) continue;

                const FString DisplayName = Property->GetDisplayNameText().ToString();
                const FString TypeName = Property->ArrayDim > 1
                    ? FString::Printf(TEXT("%s[%d]"), *TraceTypeName(Property), Property->ArrayDim)
                    : TraceTypeName(Property);
                const UClass* OwnerClass = Property->GetOwnerClass();
                const FString OwnerName = OwnerClass ? OwnerClass->GetName() : TargetClass->GetName();
                const FString SearchText = FString::Printf(TEXT("%s %s %s %s %s"), *DisplayName, *Name, *TypeName, *Category, *OwnerName);
                if (Query.IsEmpty() || SearchText.Contains(Query, ESearchCase::IgnoreCase))
                {
                    VisibleProperties.Add(Property);
                }
            }
            if (VisibleProperties.IsEmpty()) continue;

            VariableMenuList->AddSlot().AutoHeight().Padding(5, 7, 5, 2)
            [SNew(STextBlock)
                .Text(FText::FromString(Category))
                .Font(VariableTraceFont(TEXT("Bold"), 9))
                .ColorAndOpacity(FLinearColor(.55f, .72f, .95f))];

            for (FProperty* Property : VisibleProperties)
            {
                ++VisiblePropertyCount;
                const FString Name = Property->GetName();
                const FString DisplayName = Property->GetDisplayNameText().ToString();
                const FString TypeName = Property->ArrayDim > 1
                    ? FString::Printf(TEXT("%s[%d]"), *TraceTypeName(Property), Property->ArrayDim)
                    : TraceTypeName(Property);
                const UClass* OwnerClass = Property->GetOwnerClass();
                const FString OwnerName = OwnerClass ? OwnerClass->GetName() : TargetClass->GetName();
                const FString Identity = DisplayName.Equals(Name, ESearchCase::CaseSensitive)
                    ? FString::Printf(TEXT("%s  |  declared in %s"), *TypeName, *OwnerName)
                    : FString::Printf(TEXT("%s  |  %s  |  declared in %s"), *Name, *TypeName, *OwnerName);
                const FString Tooltip = FString::Printf(
                    TEXT("Display name: %s\nVariable name: %s\nType: %s\nCategory: %s\nDeclared in: %s"),
                    *DisplayName, *Name, *TypeName, *Category, *OwnerName);
                const bool bSelected = SelectedVariables.Contains(Name);

                VariableMenuList->AddSlot().AutoHeight()
                [SNew(SCheckBox)
                    .IsChecked(bSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                    .OnCheckStateChanged_Lambda([this, Name](ECheckBoxState) { ToggleVariable(Name); })
                    .Padding(FMargin(9, 4))
                    .ToolTipText(FText::FromString(Tooltip))
                    [SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [SNew(STextBlock)
                            .Text(FText::FromString(DisplayName))
                            .Font(VariableTraceFont(TEXT("Bold"), 9))
                            .HighlightText(FText::FromString(Query))]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 1, 0, 0)
                        [SNew(STextBlock)
                            .Text(FText::FromString(Identity))
                            .Font(VariableTraceFont(TEXT("Regular"), 8))
                            .ColorAndOpacity(FLinearColor(.53f, .57f, .64f))
                            .HighlightText(FText::FromString(Query))]]];
            }
        }

        if (VisiblePropertyCount == 0)
        {
            VariableMenuList->AddSlot().AutoHeight().Padding(8)
            [SNew(STextBlock)
                .Text(TMLoc::Text(TEXT("No matching variables"), TEXT("No matching variables")))
                .Font(VariableTraceFont(TEXT("Regular"), 9))
                .ColorAndOpacity(FLinearColor(.55f, .58f, .64f))];
        }
    }
    void OnVariableSearchChanged(const FText& Text)
    {
        VariableSearchText = Text.ToString();
        RefreshVariableMenu();
    }
    TSharedRef<SWidget> BuildVariableMenu()
    {
        TSharedRef<SWidget> Menu = SNew(SBorder).Padding(3).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
        [SNew(SBox).WidthOverride(430).MaxDesiredHeight(390)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(3)
                [SNew(SSearchBox)
                    .HintText(TMLoc::Text(TEXT("Search variables by name, type, category or class"), TEXT("Search variables by name, type, category or class")))
                    .OnTextChanged(this, &SVariableValueTrace::OnVariableSearchChanged)]
                + SVerticalBox::Slot().FillHeight(1)
                [SNew(SScrollBox)
                    + SScrollBox::Slot()
                    [SAssignNew(VariableMenuList, SVerticalBox)]]]];
        RefreshVariableMenu();
        return Menu;
    }
    TSharedRef<SWidget> BuildControls()
    {
        return SNew(SBorder).Padding(8).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(TMLoc::Text(TEXT("Trace setup"), TEXT("Trace setup"))).Font(VariableTraceFont(TEXT("Bold"), 10))]
                + SVerticalBox::Slot().AutoHeight().Padding(0,5)[SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(.52f).Padding(0,0,6,0)[SAssignNew(ClassPicker, SComboButton).ButtonContent()[SNew(STextBlock).Text(this, &SVariableValueTrace::SelectedClassLabel).Font(VariableTraceFont(TEXT("Regular"), 9))].OnGetMenuContent(this, &SVariableValueTrace::BuildClassMenu).OnComboBoxOpened_Lambda([this](){ ClassSearchText.Reset(); RefreshClassMenu(); })]
                    + SHorizontalBox::Slot().FillWidth(.48f)[SAssignNew(VariablePicker, SComboButton).ButtonContent()[SNew(STextBlock).Text(this, &SVariableValueTrace::SelectedVariablesLabel).Font(VariableTraceFont(TEXT("Regular"), 9))].OnGetMenuContent(this, &SVariableValueTrace::BuildVariableMenu).OnComboBoxOpened_Lambda([this](){ VariableSearchText.Reset(); RefreshVariableMenu(); })]]
                + SVerticalBox::Slot().AutoHeight().Padding(0,5)[SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(TMLoc::Text(TEXT("Start trace"), TEXT("Start trace"))).OnClicked(this, &SVariableValueTrace::Apply)]
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(8,0)[SAssignNew(Configuration, STextBlock).Font(VariableTraceFont(TEXT("Regular"), 9)).ColorAndOpacity(FLinearColor(.60f,.65f,.72f))]]];
    }
    FReply Apply()
    {
        if (!TargetClass) { Configuration->SetText(TMLoc::Text(TEXT("Choose a class."), TEXT("Choose a class."))); return FReply::Handled(); }
        if (SelectedVariables.IsEmpty()) { Configuration->SetText(TMLoc::Text(TEXT("Choose at least one variable."), TEXT("Choose at least one variable."))); return FReply::Handled(); }

        const bool bStartingNewTrace = !bIsTracing;
        const bool bActiveFilterChanged = ActiveLanes != SelectedVariables;
        ActiveLanes = SelectedVariables;
        for (const FString& Lane : ActiveLanes)
        {
            if (!Lanes.Contains(Lane)) Lanes.Add(Lane);
        }
        if (bStartingNewTrace)
        {
            SampleFrame = 0;
            StartTime = FPlatformTime::Seconds();
        }
        if (bStartingNewTrace || bActiveFilterChanged)
        {
            // Establish fresh baselines for the active filter without discarding captured history.
            Values.Reset();
            DisplayValues.Reset();
            DetailValues.Reset();
        }

        bIsTracing = true; bPaused = false; bShowConfiguration = false; bDirty = true;
        Configuration->SetText(FText::Format(TMLoc::Text(TEXT("Tracing {0} - {1} variable(s)."), TEXT("Tracing {0} - {1} variable(s).")), FText::FromString(TargetClass->GetName()), FText::AsNumber(ActiveLanes.Num())));
        return FReply::Handled();
    }
    FReply TogglePause()
    {
        bPaused = !bPaused;
        bDirty = true;
        return FReply::Handled();
    }
    FText PauseLabel() const
    {
        return bPaused ? TMLoc::Text(TEXT("Resume"), TEXT("Resume")) : TMLoc::Text(TEXT("Pause"), TEXT("Pause"));
    }
    FReply Clear()
    {
        Changes.Reset();
        Values.Reset();
        DisplayValues.Reset();
        DetailValues.Reset();
        SelectedChangeIndex = INDEX_NONE;
        RebuildDetailsPanel();
        bDirty = true;
        return FReply::Handled();
    }
    void Sample(double Now)
    {
        ++SampleFrame;
        int32 InstanceCount = 0;
        for (TObjectIterator<UObject> It; It; ++It)
        {
            UObject *Object = *It;
            if (!Object || Object->IsTemplate() || !Object->IsA(TargetClass))
                continue;
            const UWorld *World = Object->GetWorld();
            if (!World || World->WorldType != EWorldType::PIE)
                continue;
            ++InstanceCount;
            for (const FString &Lane : ActiveLanes)
            {
                FProperty *Property = FindFProperty<FProperty>(Object->GetClass(), *Lane);
                if (!Property || Property->HasAnyPropertyFlags(CPF_Parm))
                    continue;
                const FString CurrentRaw = ExportTraceRaw(Property, Object);
                const FString CurrentDisplay = SummarizeTraceText(DescribeRootTraceValue(Property, Object, false));
                const FString Key = FString::Printf(TEXT("%p|%u|%s"), Object, Object->GetUniqueID(), *Lane);
                FString *PreviousRaw = Values.Find(Key);
                if (!PreviousRaw)
                {
                    Values.Add(Key, CurrentRaw);
                    DisplayValues.Add(Key, CurrentDisplay);
                    DetailValues.Add(Key, DescribeRootTraceValue(Property, Object, true));
                    continue;
                }
                if (*PreviousRaw != CurrentRaw)
                {
                    const FString PreviousDisplay = DisplayValues.FindRef(Key);
                    const FString PreviousDetails = DetailValues.FindRef(Key);
                    const FString CurrentDetails = DescribeRootTraceValue(Property, Object, true);
                    const FString TypeName = Property->ArrayDim > 1 ? FString::Printf(TEXT("%s[%d]"), *TraceTypeName(Property), Property->ArrayDim) : TraceTypeName(Property);
                    AddChange(Lane, Object->GetName(), PreviousDisplay, CurrentDisplay, Now, TypeName, PreviousDetails, CurrentDetails);
                    *PreviousRaw = CurrentRaw;
                    DisplayValues.Add(Key, CurrentDisplay);
                    DetailValues.Add(Key, CurrentDetails);
                }
            }
        }
        const FText StateText =
            bPaused ? TMLoc::Text(TEXT("Paused"), TEXT("Paused")) : TMLoc::Text(TEXT("Live"), TEXT("Live"));
        Status->SetText(
            FText::Format(TMLoc::Text(TEXT("{0} | {1} event groups | {2} live instance(s) | visible: last 45 sec"),
                                      TEXT("{0} | {1} event groups | {2} live instance(s) | visible: last 45 sec")),
                          StateText, FText::AsNumber(Changes.Num()), FText::AsNumber(InstanceCount)));
    }
    void AddChange(const FString &Lane, const FString &ObjectName, const FString &OldValue, const FString &NewValue,
                   double Now, const FString& TypeName, const FString& OldDetails, const FString& NewDetails)
    {
        if (Changes.Num() && Changes.Last().Lane == Lane && Changes.Last().ObjectName == ObjectName &&
            Now - Changes.Last().Time <= .25)
        {
            FVariableChange &Burst = Changes.Last();
            Burst.NewValue = NewValue;
            Burst.NewDetails = NewDetails;
            Burst.Time = Now;
            ++Burst.BurstCount;
            RebuildDetailsPanel();
            bDirty = true;
            return;
        }
        FVariableChange Change;
        Change.Lane = Lane;
        Change.ObjectName = ObjectName;
        Change.OldValue = OldValue;
        Change.NewValue = NewValue;
        Change.Time = Now;
        Change.TypeName = TypeName;
        Change.OldDetails = OldDetails;
        Change.NewDetails = NewDetails;
        Changes.Add(MoveTemp(Change));
        if (Changes.Num() > 2000) { const int32 Removed = Changes.Num() - 2000; TMEngineCompatibility::RemoveAtNoShrink(Changes, 0, Removed); SelectedChangeIndex = FMath::Max(-1, SelectedChangeIndex - Removed); }
        else SelectedChangeIndex = Changes.Num() - 1;
        RebuildDetailsPanel();
        bDirty = true;
    }
    FString CompactCardType(const FVariableChange& Change) const
    {
        if (Change.TypeName.StartsWith(TEXT("Array<")))
        {
            FString Inner = Change.TypeName.Mid(6); int32 Close = INDEX_NONE;
            if (Inner.FindLastChar(TCHAR('>'), Close)) Inner = Inner.Left(Close);
            return Inner;
        }
        return Change.TypeName;
    }
    FString ExtractContainerCount(const FString& Value) const
    {
        int32 Open = INDEX_NONE, Close = INDEX_NONE;
        if (Value.FindChar(TCHAR('['), Open) && Value.FindChar(TCHAR(']'), Close) && Close > Open) return Value.Mid(Open + 1, Close - Open - 1);
        return TEXT("?");
    }
    FString CompactCardValue(const FVariableChange& Change) const
    {
        if (Change.TypeName.StartsWith(TEXT("Array<"))) return FString::Printf(TEXT("Num %s > %s%s"), *ExtractContainerCount(Change.OldValue), *ExtractContainerCount(Change.NewValue), Change.BurstCount > 1 ? *FString::Printf(TEXT("  x%d"), Change.BurstCount) : TEXT(""));
        return Change.BurstCount > 1 ? FString::Printf(TEXT("%s > %s  x%d"), *Change.OldValue, *Change.NewValue, Change.BurstCount) : FString::Printf(TEXT("%s > %s"), *Change.OldValue, *Change.NewValue);
    }
    TSharedRef<SWidget> MakeCard(const FVariableChange &Change, FLinearColor Color, int32 ChangeIndex)
    {
        const FString Value = CompactCardValue(Change);
        const FString Type = CompactCardType(Change);
        return SNew(SButton).ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton")).ContentPadding(0).OnClicked_Lambda([this, ChangeIndex](){ SelectedChangeIndex = ChangeIndex; RebuildDetailsPanel(); return FReply::Handled(); })
            [SNew(SBox).WidthOverride(180).HeightOverride(58)
                [SNew(SBorder).Padding(1).BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox"))).BorderBackgroundColor(Color)
                    [SNew(SBorder).Padding(FMargin(7,4)).BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox"))).BorderBackgroundColor(FLinearColor(.035f,.038f,.043f,1.f))
                        [SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Type)).Font(VariableTraceFont(TEXT("Bold"),8)).ColorAndOpacity(Color)]
                            + SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)[SNew(STextBlock).Text(FText::FromString(Value)).Font(VariableTraceFont(TEXT("Regular"),8)).ColorAndOpacity(FLinearColor(.86f,.87f,.89f)).OverflowPolicy(ETextOverflowPolicy::Ellipsis).ToolTipText(FText::FromString(Value))]
                            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Change.ObjectName)).Font(VariableTraceFont(TEXT("Regular"),7)).ColorAndOpacity(FLinearColor(.54f,.57f,.63f))]]]]];
    }
    FString GetLaneType(const FString& Lane) const
    {
        if (TargetClass) if (const FProperty* Property = FindFProperty<FProperty>(TargetClass, *Lane)) return Property->ArrayDim > 1 ? FString::Printf(TEXT("%s[%d]"), *TraceTypeName(Property), Property->ArrayDim) : TraceTypeName(Property);
        return TEXT("Value");
    }
    void RebuildTimeline()
    {
        if (!Timeline.IsValid()) return;
        bDirty = false;
        Timeline->ClearChildren();
        const double WindowStart = FMath::Max(StartTime, FPlatformTime::Seconds() - 45.0);
        TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
        for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
        {
            const FString &Lane = Lanes[LaneIndex];
            const FLinearColor Color = VariableTraceColor(LaneIndex);
            TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
            Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 2)
                [SNew(SBox).MinDesiredWidth(190)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT(">> %s  (%s)"), *Lane, *GetLaneType(Lane)))).Font(VariableTraceFont(TEXT("Bold"), 9)).ColorAndOpacity(Color)]];
            double CursorTime = WindowStart;
            bool bHasVisibleChange = false;
            for (int32 ChangeIndex = 0; ChangeIndex < Changes.Num(); ++ChangeIndex)
            {
                const FVariableChange &Change = Changes[ChangeIndex];
                if (Change.Lane != Lane || Change.Time < WindowStart) continue;
                const float Gap = FMath::Clamp((Change.Time - CursorTime) * 13.0, 8.0, 70.0);
                if (bHasVisibleChange)
                {
                    Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMath::Max(4.f, Gap * .25f), 0)
                    [SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("\u2192"), TEXT("\u2192")))
                        .Font(VariableTraceFont(TEXT("Bold"), 18))
                        .ColorAndOpacity(Color)];
                }
                else
                {
                    Row->AddSlot().AutoWidth()[SNew(SSpacer).Size(FVector2D(Gap, 1))];
                }
                Row->AddSlot().AutoWidth().Padding(0, 1)[MakeCard(Change, Color, ChangeIndex)];
                CursorTime = Change.Time;
                bHasVisibleChange = true;
            }
            Rows->AddSlot().AutoHeight()[SNew(SBorder).Padding(FMargin(3,2)).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))[Row]];
        }
        if (Lanes.IsEmpty()) Rows->AddSlot().AutoHeight().Padding(4)[SNew(STextBlock).Text(TMLoc::Text(TEXT("Use Filter to select a class and variables."), TEXT("Use Filter to select a class and variables."))).Font(VariableTraceFont(TEXT("Regular"), 9)).ColorAndOpacity(FLinearColor(.48f,.52f,.58f))];
        Timeline->AddSlot()[Rows];
    }
    TSharedPtr<SComboButton> ClassPicker;
    TSharedPtr<SVerticalBox> ClassMenuList;
    FString ClassSearchText;
    TSharedPtr<SComboButton> VariablePicker;
    TSharedPtr<SVerticalBox> VariableMenuList;
    FString VariableSearchText;
    TSharedPtr<SBox> ConfigurationPanel;
    TSharedPtr<SScrollBox> Timeline;
    TSharedPtr<SVerticalBox> DetailsContent;
    FString DetailsSearchText;
    TSharedPtr<STextBlock> Status, Configuration;
    UClass *TargetClass = nullptr;
    TArray<TSharedPtr<FString>> ClassOptions;
    TSharedPtr<FString> SelectedClass;
    TArray<FString> PropertyCategories, SelectedVariables, ActiveLanes, Lanes;
    TMap<FString, TArray<FString>> PropertiesByCategory;
    TMap<FString, FString> Values, DisplayValues, DetailValues;
    TArray<FVariableChange> Changes;
    double StartTime = 0, LastSample = 0, LastBuild = 0;
    int32 SampleFrame = 0;
    bool bPaused = false, bDirty = false, bShowConfiguration = true, bIsTracing = false, bPIEActive = false;
    int32 SelectedChangeIndex = INDEX_NONE;
    FDelegateHandle PostPIEStartedHandle, EndPIEHandle;
};
TSharedRef<SDockTab> SpawnVariableTraceTab(const FSpawnTabArgs &)
{
    TSharedRef<SDockTab> Tab =
        SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            .Label(TMLoc::Text(TEXT("Variable Value Trace"), TEXT("Variable Value Trace")))
            .OnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda(
                [](TSharedRef<SDockTab>) { VariableTraceTab.Reset(); }))[SNew(SVariableValueTrace)];
    VariableTraceTab = Tab;
    return Tab;
}
void RegisterVariableTraceTab()
{
    if (bVariableTraceTabRegistered)
        return;
    FGlobalTabmanager::Get()
        ->RegisterNomadTabSpawner(VariableTraceTabId, FOnSpawnTab::CreateStatic(&SpawnVariableTraceTab))
        .SetDisplayName(TMLoc::Text(TEXT("Variable Value Trace"), TEXT("Variable Value Trace")))
        .SetTooltipText(TMLoc::Text(TEXT("Trace PIE property changes in synchronized timeline lanes."),
                                    TEXT("Trace PIE property changes in synchronized timeline lanes.")))
        .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.VariableValueTrace")));
    bVariableTraceTabRegistered = true;
}
} // namespace
namespace TMVariableValueTrace
{
void RegisterMenus()
{
    RegisterVariableTraceTab();
    auto Add = [](UToolMenu *Menu, FName Name)
    {
        if (Menu)
            Menu->FindOrAddSection(TEXT("TraceMotive"))
                .AddMenuEntry(Name, TMLoc::Text(TEXT("Variable Value Trace"), TEXT("Variable Value Trace")),
                              TMLoc::Text(TEXT("Observe class properties during PIE as synchronized timeline lanes."),
                                          TEXT("Observe class properties during PIE as synchronized timeline lanes.")),
                              FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.VariableValueTrace")),
                              FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext &) { OpenWindow(); }));
    };
    Add(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")), TEXT("TMOpenVariableValueTrace"));
    Add(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")), TEXT("TMToolsOpenVariableValueTrace"));
}
void UnregisterMenus()
{
    if (bVariableTraceTabRegistered)
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(VariableTraceTabId);
        bVariableTraceTabRegistered = false;
    }
    VariableTraceTab.Reset();
}
void OpenWindow()
{
    RegisterVariableTraceTab();
    if (VariableTraceTab.IsValid())
    {
        FGlobalTabmanager::Get()->TryInvokeTab(VariableTraceTabId);
        return;
    }
    VariableTraceTab = FGlobalTabmanager::Get()->TryInvokeTab(VariableTraceTabId);
}
} // namespace TMVariableValueTrace
