#include "TMEnhancedOutlinerSearch.h"
#include "TMStyle.h"



#include "TMLocalization.h"

#include "TMPerformanceGuard.h"

#include "TMReportFormatter.h"

#include "Components/ActorComponent.h"

#include "Containers/Ticker.h"

#include "Editor.h"

#include "Engine/Selection.h"

#include "Engine/World.h"

#include "EngineUtils.h"

#include "Framework/Docking/TabManager.h"

#include "HAL/PlatformApplicationMisc.h"

#include "LevelEditor.h"

#include "Styling/AppStyle.h"

#include "ToolMenus.h"

#include "UObject/FieldIterator.h"

#include "UObject/UnrealType.h"

#include "Widgets/Docking/SDockTab.h"

#include "Widgets/Input/SButton.h"

#include "Widgets/Input/SCheckBox.h"

#include "Widgets/Input/SEditableTextBox.h"

#include "Widgets/Input/SSearchBox.h"

#include "Widgets/Layout/SBorder.h"

#include "Widgets/Layout/SScrollBox.h"

#include "Widgets/Layout/SWrapBox.h"

#include "Widgets/SBoxPanel.h"

#include "Widgets/Text/STextBlock.h"



namespace

{

const FName EnhancedOutlinerSearchTabId(TEXT("TraceMotive.EnhancedOutlinerSearch"));

bool bEnhancedOutlinerSearchTabRegistered = false;

// Runtime budgets are centralized in TMPerformanceGuard.h so large maps do not freeze the editor.

constexpr int32 MaxValuePreviewChars = 220;

constexpr int32 MaxExportedValueChars = 2048;



enum class ETMOutlinerResultKind : uint8 { Actor, Component, Tag, VariableName, VariableValue, VariablePair };



struct FTMOutlinerSearchResult

{

    ETMOutlinerResultKind Kind = ETMOutlinerResultKind::Actor;

    TWeakObjectPtr<AActor> Actor;

    TWeakObjectPtr<UActorComponent> Component;

    FString Title;

    FString Subtitle;

    FString Reason;

    FString Name;

    FString Value;



    FString KindToString() const

    {

        switch (Kind)

        {

        case ETMOutlinerResultKind::Actor: return TEXT("Actor");

        case ETMOutlinerResultKind::Component: return TEXT("Component");

        case ETMOutlinerResultKind::Tag: return TEXT("Tag");

        case ETMOutlinerResultKind::VariableName: return TEXT("VariableName");

        case ETMOutlinerResultKind::VariableValue: return TEXT("VariableValue");

        case ETMOutlinerResultKind::VariablePair: return TEXT("VariablePair");

        default: return TEXT("Result");

        }

    }



    FString ToClipboardLine() const

    {

        const FString ActorName = Actor.IsValid() ? Actor->GetActorLabel() : TEXT("<invalid actor>");

        const FString ComponentName = Component.IsValid() ? Component->GetName() : TEXT("");

        return FString::Printf(TEXT("[%s] Actor=%s%s | %s | %s | %s"), *KindToString(), *ActorName,

            ComponentName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" Component=%s"), *ComponentName), *Name, *Value, *Reason);

    }

};



static bool ContainsQuery(const FString& Text, const FString& Query)

{

    return !Query.IsEmpty() && Text.Contains(Query, ESearchCase::IgnoreCase);

}



static FString TrimPreview(FString Text)

{

    Text.ReplaceInline(TEXT("\r"), TEXT(" "));

    Text.ReplaceInline(TEXT("\n"), TEXT(" "));

    Text.TrimStartAndEndInline();

    if (Text.Len() > MaxValuePreviewChars)

    {

        Text.LeftInline(MaxValuePreviewChars);

        Text += TEXT("...");

    }

    return Text;

}



static FText ResultKindText(ETMOutlinerResultKind Kind)

{

    switch (Kind)

    {

    case ETMOutlinerResultKind::Actor: return TMLoc::Text(TEXT("Actor"), TEXT("Actor"));

    case ETMOutlinerResultKind::Component: return TMLoc::Text(TEXT("Component"), TEXT("Component"));

    case ETMOutlinerResultKind::Tag: return TMLoc::Text(TEXT("Tag"), TEXT("Tag"));

    case ETMOutlinerResultKind::VariableName: return TMLoc::Text(TEXT("Variable name"), TEXT("Variable name"));

    case ETMOutlinerResultKind::VariableValue: return TMLoc::Text(TEXT("Variable value"), TEXT("Variable value"));

    case ETMOutlinerResultKind::VariablePair: return TMLoc::Text(TEXT("Variable pair"), TEXT("Variable pair"));

    default: return TMLoc::Text(TEXT("Result"), TEXT("Result"));

    }

}



static FSlateColor ResultKindColor(ETMOutlinerResultKind Kind)

{

    switch (Kind)

    {

    case ETMOutlinerResultKind::Actor: return FSlateColor(FLinearColor(0.50f, 0.72f, 1.00f));

    case ETMOutlinerResultKind::Component: return FSlateColor(FLinearColor(0.65f, 0.85f, 0.65f));

    case ETMOutlinerResultKind::Tag: return FSlateColor(FLinearColor(1.00f, 0.76f, 0.38f));

    case ETMOutlinerResultKind::VariableName: return FSlateColor(FLinearColor(0.78f, 0.66f, 1.00f));

    case ETMOutlinerResultKind::VariableValue: return FSlateColor(FLinearColor(1.00f, 0.62f, 0.72f));

    case ETMOutlinerResultKind::VariablePair: return FSlateColor(FLinearColor(0.45f, 0.90f, 1.00f));

    default: return FSlateColor::UseForeground();

    }

}



static bool IsSupportedSearchPropertyRecursive(const FProperty* Property);

static bool IsSupportedSearchLeafProperty(const FProperty* Property)

{

    return CastField<FBoolProperty>(Property)

        || CastField<FEnumProperty>(Property)

        || CastField<FNumericProperty>(Property)

        || CastField<FNameProperty>(Property)

        || CastField<FStrProperty>(Property)

        || CastField<FTextProperty>(Property)

        || CastField<FObjectPropertyBase>(Property);

}

static bool IsSupportedSearchPropertyRecursive(const FProperty* Property)

{

    if (!Property) return false;

    if (IsSupportedSearchLeafProperty(Property)) return true;

    if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))

    {

        return IsSupportedSearchPropertyRecursive(ArrayProperty->Inner);

    }

    if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))

    {

        return IsSupportedSearchPropertyRecursive(SetProperty->ElementProp);

    }

    if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))

    {

        return IsSupportedSearchPropertyRecursive(MapProperty->KeyProp) || IsSupportedSearchPropertyRecursive(MapProperty->ValueProp);

    }

    return false;

}

static bool ShouldInspectProperty(const FProperty* Property)

{

    if (!Property) return false;

    const EPropertyFlags SkipFlags = CPF_Deprecated | CPF_Transient | CPF_DuplicateTransient | CPF_SkipSerialization;

    if (Property->HasAnyPropertyFlags(SkipFlags)) return false;

    const EPropertyFlags VisibleFlags = CPF_Edit | CPF_BlueprintVisible | CPF_BlueprintReadOnly | CPF_Interp;

    if (!Property->HasAnyPropertyFlags(VisibleFlags)) return false;

    return IsSupportedSearchPropertyRecursive(Property);

}

static void AppendSearchToken(TArray<FString>& Tokens, FString Token)

{

    Token.TrimStartAndEndInline();

    if (!Token.IsEmpty())

    {

        Tokens.AddUnique(Token);

    }

}



static FString GetActorOutlinerLabel(const AActor* Actor)

{

    return Actor ? Actor->GetActorLabel() : FString(TEXT("<invalid actor>"));

}



static FString GetObjectOwnerText(UObject* Object)

{

    if (!Object)

    {

        return FString();

    }



    if (const AActor* Actor = Cast<AActor>(Object))

    {

        return FString::Printf(TEXT("%s / %s"), *GetActorOutlinerLabel(Actor), *Actor->GetClass()->GetName());

    }



    if (const UActorComponent* Component = Cast<UActorComponent>(Object))

    {

        const AActor* Owner = Component->GetOwner();

        const FString OwnerLabel = Owner ? GetActorOutlinerLabel(Owner) : FString(TEXT("<no owner>"));

        return FString::Printf(TEXT("%s > %s / %s"), *OwnerLabel, *Component->GetName(), *Component->GetClass()->GetName());

    }



    if (const AActor* OuterActor = Object->GetTypedOuter<AActor>())

    {

        return FString::Printf(TEXT("%s > %s / %s"), *GetActorOutlinerLabel(OuterActor), *Object->GetName(), *Object->GetClass()->GetName());

    }



    return FString::Printf(TEXT("%s / %s"), *Object->GetName(), *Object->GetClass()->GetName());

}


static void AppendObjectReferenceSearchTokens(TArray<FString>& Tokens, const UObject* Object)

{

    if (!Object)

    {

        AppendSearchToken(Tokens, TEXT("None"));

        return;

    }



    AppendSearchToken(Tokens, Object->GetName());

    AppendSearchToken(Tokens, Object->GetPathName());

    if (Object->GetClass())

    {

        AppendSearchToken(Tokens, Object->GetClass()->GetName());

    }



    if (const AActor* ReferencedActor = Cast<AActor>(Object))

    {

        AppendSearchToken(Tokens, GetActorOutlinerLabel(ReferencedActor));

        AppendSearchToken(Tokens, ReferencedActor->GetFName().ToString());

        return;

    }



    if (const UActorComponent* ReferencedComponent = Cast<UActorComponent>(Object))

    {

        AppendSearchToken(Tokens, ReferencedComponent->GetName());

        if (const AActor* Owner = ReferencedComponent->GetOwner())

        {

            AppendSearchToken(Tokens, GetActorOutlinerLabel(Owner));

            AppendSearchToken(Tokens, Owner->GetName());

            AppendSearchToken(Tokens, Owner->GetPathName());

        }

    }

}

static void AppendPropertyValueSearchTokens(const FProperty* Property, const void* ValuePtr, TArray<FString>& Tokens, int32 Depth = 0)

{

    if (!Property || !ValuePtr || Depth > 4) return;



    if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))

    {

        AppendObjectReferenceSearchTokens(Tokens, ObjectProperty->GetObjectPropertyValue(ValuePtr));

        return;

    }



    if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))

    {

        FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);

        const int32 NumToScan = FMath::Min(ArrayHelper.Num(), 64);

        for (int32 Index = 0; Index < NumToScan; ++Index)

        {

            AppendPropertyValueSearchTokens(ArrayProperty->Inner, ArrayHelper.GetRawPtr(Index), Tokens, Depth + 1);

        }

        if (ArrayHelper.Num() > NumToScan)

        {

            AppendSearchToken(Tokens, FString::Printf(TEXT("%d more array item(s)"), ArrayHelper.Num() - NumToScan));

        }

        return;

    }



    if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))

    {

        FScriptSetHelper SetHelper(SetProperty, ValuePtr);

        int32 Scanned = 0;

        for (int32 Index = 0; Index < SetHelper.GetMaxIndex() && Scanned < 64; ++Index)

        {

            if (!SetHelper.IsValidIndex(Index)) continue;

            AppendPropertyValueSearchTokens(SetProperty->ElementProp, SetHelper.GetElementPtr(Index), Tokens, Depth + 1);

            ++Scanned;

        }

        return;

    }



    if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))

    {

        FScriptMapHelper MapHelper(MapProperty, ValuePtr);

        int32 Scanned = 0;

        for (int32 Index = 0; Index < MapHelper.GetMaxIndex() && Scanned < 64; ++Index)

        {

            if (!MapHelper.IsValidIndex(Index)) continue;

            AppendPropertyValueSearchTokens(MapProperty->KeyProp, MapHelper.GetKeyPtr(Index), Tokens, Depth + 1);

            AppendPropertyValueSearchTokens(MapProperty->ValueProp, MapHelper.GetValuePtr(Index), Tokens, Depth + 1);

            ++Scanned;

        }

        return;

    }



    FString ExportedItem;

    Property->ExportTextItem_Direct(ExportedItem, ValuePtr, nullptr, nullptr, PPF_None);

    AppendSearchToken(Tokens, ExportedItem);

}

static FString BuildExpandedPropertyValueSearchText(const FProperty* Property, UObject* Object, FString* OutPreview)

{

    FString ExportedValue;

    if (Property && Object)

    {

        Property->ExportText_InContainer(0, ExportedValue, Object, Object, Object, PPF_None);

    }

    if (ExportedValue.Len() > MaxExportedValueChars)

    {

        ExportedValue.LeftInline(MaxExportedValueChars);

    }



    TArray<FString> Tokens;

    AppendSearchToken(Tokens, ExportedValue);

    if (Property && Object)

    {

        AppendPropertyValueSearchTokens(Property, Property->ContainerPtrToValuePtr<void>(Object), Tokens);

    }



    const FString SearchText = FString::Join(Tokens, TEXT(" | "));

    if (OutPreview)

    {

        *OutPreview = TrimPreview(SearchText.IsEmpty() ? ExportedValue : SearchText);

    }

    return SearchText;

}



class SEnhancedOutlinerSearchWidget : public SCompoundWidget

{

public:

    SLATE_BEGIN_ARGS(SEnhancedOutlinerSearchWidget) {}

    SLATE_END_ARGS()



    void Construct(const FArguments& InArgs)

    {

        RefreshCandidateActors();

        ChildSlot

        [

            SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel"))).Padding(12.0f)

            [

                SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()[BuildHeader()]

                + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,6)[BuildSearchBar()]

                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[BuildPairSearchBar()]

                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[BuildFilterBar()]

                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)

                [SNew(STextBlock).Text(this, &SEnhancedOutlinerSearchWidget::GetStatusText).ColorAndOpacity(FSlateColor(FLinearColor(0.72f,0.72f,0.72f)))]

                + SVerticalBox::Slot().FillHeight(1.0f)[SAssignNew(ResultList, SScrollBox)]

            ]

        ];

        bResultsDirty = true;

        RefreshResultsPanel();

    }



    ~SEnhancedOutlinerSearchWidget()

    {

        StopScan();

    }



private:

    TSharedPtr<SScrollBox> ResultList;

    TArray<TWeakObjectPtr<AActor>> CandidateActors;

    TArray<FTMOutlinerSearchResult> Results;

    FString Query;

    FString PairVariableNameQuery;

    FString PairVariableValueQuery;

    int32 ScanIndex = 0;

    int32 SelectedResultIndex = INDEX_NONE;

    bool bScanning = false;

    bool bResultsDirty = false;

    bool bHitResultCap = false;

    bool bSearchActors = true;

    bool bSearchComponents = true;

    bool bSearchTags = true;

    bool bSearchVariableNames = true;

    bool bSearchVariableValues = true;

    bool bSelectedOnly = false;

    bool bIncludeComponentDetails = true;

    FTSTicker::FDelegateHandle TickerHandle;

    uint64 CancellationGeneration = 0;



    TSharedRef<SWidget> BuildHeader()

    {

        return SNew(SHorizontalBox)

            + SHorizontalBox::Slot().FillWidth(1.0f)

            [

                SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()

                [SNew(STextBlock).Text(TMLoc::Text(TEXT("Enhanced Outliner Search"), TEXT("Enhanced Outliner Search"))).TextStyle(FAppStyle::Get(), TEXT("DetailsView.CategoryTextStyle"))]

                + SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)

                [SNew(STextBlock).Text(TMLoc::Text(TEXT("Search loaded editor-world instances without loading every Blueprint. Includes tags, Details variable names, and variable values."), TEXT("Search loaded editor-world instances without loading every Blueprint. Includes tags, Details variable names, and variable values."))).ColorAndOpacity(FSlateColor(FLinearColor(0.65f,0.65f,0.65f)))]

            ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

            [

                SNew(SButton)

                .ToolTipText(TMLoc::Text(TEXT("Use more editor time per tick to finish searches faster."), TEXT("")))

                .Text_Lambda([]()

                {

                    return TMPerf::IsSearchBoostEnabled()

                        ? TMLoc::Text(TEXT("Search Boost: ON"), TEXT(""))

                        : TMLoc::Text(TEXT("Search Boost: OFF"), TEXT(""));

                })

                .ButtonColorAndOpacity_Lambda([]()

                {

                    return TMPerf::IsSearchBoostEnabled() ? FSlateColor(FLinearColor(0.12f, 0.42f, 0.16f, 1.0f)) : FSlateColor::UseForeground();

                })

                .OnClicked_Lambda([this]()

                {

                    TMPerf::ToggleSearchBoost();

                    RestartScan();

                    return FReply::Handled();

                })

            ]

            + SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0).VAlign(VAlign_Center)

            [SNew(SButton).Text(TMLoc::Text(TEXT("Refresh"), TEXT(""))).OnClicked(this, &SEnhancedOutlinerSearchWidget::OnRefreshClicked)]

            + SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0).VAlign(VAlign_Center)

            [SNew(SButton).Text(TMLoc::Text(TEXT("Copy Results"), TEXT(""))).OnClicked(this, &SEnhancedOutlinerSearchWidget::OnCopyAllClicked)];

    }



    TSharedRef<SWidget> BuildSearchBar()

    {

        return SNew(SSearchBox)

            .HintText(TMLoc::Text(TEXT("Search actor, component, tag, variable name, or variable value"), TEXT("Search actor, component, tag, variable name, or variable value")))

            .OnTextChanged(this, &SEnhancedOutlinerSearchWidget::OnSearchChanged);

    }



    TSharedRef<SWidget> BuildPairSearchBar()

    {

        return SNew(SBorder)

            .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

            .Padding(8.0f)

            [

                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)

                [SNew(STextBlock).Text(TMLoc::Text(TEXT("Variable pair"), TEXT("Variable pair"))).ColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.90f, 1.00f)))]

                + SHorizontalBox::Slot().FillWidth(0.5f).Padding(0,0,6,0)

                [

                    SNew(SEditableTextBox)

                    .HintText(TMLoc::Text(TEXT("Variable name (ex: a)"), TEXT("Variable name (ex: a)")))

                    .Text_Lambda([this]() { return FText::FromString(PairVariableNameQuery); })

                    .OnTextChanged(this, &SEnhancedOutlinerSearchWidget::OnPairVariableNameChanged)

                ]

                + SHorizontalBox::Slot().FillWidth(0.5f).Padding(0,0,6,0)

                [

                    SNew(SEditableTextBox)

                    .HintText(TMLoc::Text(TEXT("Variable value (ex: abc)"), TEXT("Variable value (ex: abc)")))

                    .Text_Lambda([this]() { return FText::FromString(PairVariableValueQuery); })

                    .OnTextChanged(this, &SEnhancedOutlinerSearchWidget::OnPairVariableValueChanged)

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                [

                    SNew(SButton)

                    .Text(TMLoc::Text(TEXT("Clear pair"), TEXT("Clear pair")))

                    .OnClicked_Lambda([this]()

                    {

                        PairVariableNameQuery.Reset();

                        PairVariableValueQuery.Reset();

                        RestartScan();

                        return FReply::Handled();

                    })

                ]

            ];

    }



    TSharedRef<SWidget> MakeFilterCheckBox(const FText& Label, bool& Flag)

    {

        return SNew(SCheckBox)

            .IsChecked_Lambda([&Flag]() { return Flag ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })

            .OnCheckStateChanged_Lambda([this, &Flag](ECheckBoxState State) { Flag = State == ECheckBoxState::Checked; RestartScan(); })

            [SNew(STextBlock).Text(Label)];

    }



    TSharedRef<SWidget> BuildFilterBar()

    {

        return SNew(SWrapBox).UseAllottedSize(true)

            + SWrapBox::Slot().Padding(0,0,8,6)[MakeFilterCheckBox(TMLoc::Text(TEXT("Actors"), TEXT("Actors")), bSearchActors)]

            + SWrapBox::Slot().Padding(0,0,8,6)[MakeFilterCheckBox(TMLoc::Text(TEXT("Components"), TEXT("Components")), bSearchComponents)]

            + SWrapBox::Slot().Padding(0,0,8,6)[MakeFilterCheckBox(TMLoc::Text(TEXT("Tags"), TEXT("Tags")), bSearchTags)]

            + SWrapBox::Slot().Padding(0,0,8,6)[MakeFilterCheckBox(TMLoc::Text(TEXT("Variable names"), TEXT("Variable names")), bSearchVariableNames)]

            + SWrapBox::Slot().Padding(0,0,8,6)[MakeFilterCheckBox(TMLoc::Text(TEXT("Variable values"), TEXT("Variable values")), bSearchVariableValues)]

            + SWrapBox::Slot().Padding(0,0,8,6)[MakeFilterCheckBox(TMLoc::Text(TEXT("Selected only"), TEXT("Selected only")), bSelectedOnly)]

            + SWrapBox::Slot().Padding(0,0,8,6)[MakeFilterCheckBox(TMLoc::Text(TEXT("Component Details"), TEXT("Component Details")), bIncludeComponentDetails)];

    }



    FText GetStatusText() const

    {

        if (!HasActiveSearch())

        {

            if (HasPartialPairSearch())

            {

                return TMLoc::Text(TEXT("Enter both variable name and value to run a pair search."), TEXT("Enter both variable name and value to run a pair search."));

            }

            return TMLoc::Text(TEXT("Type a search term. The scan runs in small batches so the editor does not freeze."), TEXT("Type a search term. The scan runs in small batches so the editor does not freeze."));

        }

        if (bScanning)

        {

            return TMLoc::IsKoreanEditor()

                ? FText::FromString(FString::Printf(TEXT("검색 중: %d / %d 액터, 결과 %d개. 최대 %d개까지만 표시합니다."), ScanIndex, CandidateActors.Num(), Results.Num(), TMPerf::MaxSearchResults()))

                : FText::FromString(FString::Printf(TEXT("Scanning %d / %d actors, %d result(s). Results are capped at %d."), ScanIndex, CandidateActors.Num(), Results.Num(), TMPerf::MaxSearchResults()));

        }

        if (bHitResultCap)

        {

            return TMLoc::IsKoreanEditor()

                ? FText::FromString(FString::Printf(TEXT("완료: 결과가 너무 많아 %d개에서 중단했습니다. 검색어를 더 구체적으로 입력하세요."), TMPerf::MaxSearchResults()))

                : FText::FromString(FString::Printf(TEXT("Done: stopped at %d results. Make the query more specific."), TMPerf::MaxSearchResults()));

        }

        return TMLoc::IsKoreanEditor()

            ? FText::FromString(FString::Printf(TEXT("완료: %d개 결과 / %d개 액터 검색."), Results.Num(), CandidateActors.Num()))

            : FText::FromString(FString::Printf(TEXT("Done: %d result(s) from %d actor(s)."), Results.Num(), CandidateActors.Num()));

    }



    FReply OnRefreshClicked()

    {

        RefreshCandidateActors();

        RestartScan();

        return FReply::Handled();

    }



    FReply OnCopyAllClicked()

    {

        FString Details;

        for (const FTMOutlinerSearchResult& Result : Results)

        {

            Details += TEXT("- ");

            Details += Result.ToClipboardLine();

            Details += LINE_TERMINATOR;

        }



        TArray<TMReportFormatter::FMetadataItem> Metadata;

        Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Query"), Query));

        Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Pair Variable Name"), PairVariableNameQuery));

        Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Pair Variable Value"), PairVariableValueQuery));

        Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Results"), FString::FromInt(Results.Num())));

        Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Candidate Actors"), FString::FromInt(CandidateActors.Num())));

        Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Selected Only"), bSelectedOnly ? TEXT("true") : TEXT("false")));



        const FString ClipboardText = TMReportFormatter::BuildWrappedLegacyReport(

            TEXT("Enhanced Outliner Search"),

            FString::Printf(TEXT("- Query: %s\n- Pair name: %s\n- Pair value: %s\n- Results: %d\n- Candidate actors: %d\n- Scope: current loaded editor-world instances."), *Query, *PairVariableNameQuery, *PairVariableValueQuery, Results.Num(), CandidateActors.Num()),

            Details.IsEmpty() ? TEXT("- No visible results.") : Details,

            Metadata);



        FPlatformApplicationMisc::ClipboardCopy(*ClipboardText);

        return FReply::Handled();

    }



    void OnSearchChanged(const FText& NewText)

    {

        Query = NewText.ToString();

        Query.TrimStartAndEndInline();

        RestartScan();

    }



    void OnPairVariableNameChanged(const FText& NewText)

    {

        PairVariableNameQuery = NewText.ToString();

        PairVariableNameQuery.TrimStartAndEndInline();

        RestartScan();

    }



    void OnPairVariableValueChanged(const FText& NewText)

    {

        PairVariableValueQuery = NewText.ToString();

        PairVariableValueQuery.TrimStartAndEndInline();

        RestartScan();

    }



    bool IsPairSearchActive() const

    {

        return !PairVariableNameQuery.IsEmpty() && !PairVariableValueQuery.IsEmpty();

    }



    bool HasPartialPairSearch() const

    {

        return PairVariableNameQuery.IsEmpty() != PairVariableValueQuery.IsEmpty();

    }



    bool HasActiveSearch() const

    {

        return !Query.IsEmpty() || IsPairSearchActive();

    }



    FText GetHighlightText() const

    {

        if (!Query.IsEmpty()) return FText::FromString(Query);

        if (!PairVariableValueQuery.IsEmpty()) return FText::FromString(PairVariableValueQuery);

        return FText::FromString(PairVariableNameQuery);

    }



    void RefreshCandidateActors()

    {

        CandidateActors.Reset();

        if (!GEditor) return;



        if (bSelectedOnly)

        {

            if (USelection* Selection = GEditor->GetSelectedActors())

            {

                for (FSelectionIterator It(*Selection); It; ++It)

                {

                    if (AActor* Actor = Cast<AActor>(*It)) CandidateActors.Add(Actor);

                }

            }

            return;

        }



        UWorld* World = GEditor->GetEditorWorldContext().World();

        if (!World) return;

        for (TActorIterator<AActor> It(World); It; ++It) CandidateActors.Add(*It);

    }



    void RestartScan()

    {

        StopScan();

        Results.Reset();

        ScanIndex = 0;

        SelectedResultIndex = INDEX_NONE;

        bHitResultCap = false;

        bResultsDirty = true;

        RefreshCandidateActors();

        if (!HasActiveSearch())

        {

            RefreshResultsPanel();

            return;

        }

        bScanning = true;
        CancellationGeneration = TMPerf::GetCancellationGeneration();

        TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &SEnhancedOutlinerSearchWidget::TickScan), TMPerf::OutlinerSearchTickerIntervalSeconds());

        RefreshResultsPanel();

    }



    void StopScan()

    {

        if (TickerHandle.IsValid())

        {

            FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

            TickerHandle.Reset();

        }

        bScanning = false;

    }



    bool TickScan(float DeltaTime)

    {

        if (CancellationGeneration != TMPerf::GetCancellationGeneration())

        {

            bScanning = false;

            TickerHandle.Reset();

            RefreshResultsPanel();

            return false;

        }

        const TMPerf::FTickBudget Budget(TMPerf::OutlinerSearchTickBudgetSeconds());

        int32 Processed = 0;

        while (Processed < TMPerf::OutlinerActorsPerTick() && ScanIndex < CandidateActors.Num() && Results.Num() < TMPerf::MaxSearchResults() && Budget.HasTime())

        {

            if (AActor* Actor = CandidateActors[ScanIndex].Get()) ScanActor(Actor);

            ++ScanIndex;

            ++Processed;

        }

        if (Results.Num() >= TMPerf::MaxSearchResults()) bHitResultCap = true;

        bResultsDirty = true;

        RefreshResultsPanel();

        if (ScanIndex >= CandidateActors.Num() || Results.Num() >= TMPerf::MaxSearchResults())

        {

            bScanning = false;

            TickerHandle.Reset();

            return false;

        }

        return true;

    }



    void ScanActor(AActor* Actor)

    {

        if (!Actor) return;

        const FString ActorLabel = Actor->GetActorLabel();

        const FString ActorClass = Actor->GetClass() ? Actor->GetClass()->GetName() : TEXT("");

        const FString ActorPath = Actor->GetPathName();



        if (bSearchActors && (ContainsQuery(ActorLabel, Query) || ContainsQuery(ActorClass, Query) || ContainsQuery(ActorPath, Query)))

        {

            AddResult(ETMOutlinerResultKind::Actor, Actor, nullptr, ActorLabel, ActorClass,

                TMLoc::String(TEXT("Actor label, class, or path matched."), TEXT("Actor label, class, or path matched.")), TEXT("Class"), ActorClass);

        }



        if (bSearchTags)

        {

            for (const FName& ActorTag : Actor->Tags)

            {

                const FString TagText = ActorTag.ToString();

                if (ContainsQuery(TagText, Query))

                {

                    AddResult(ETMOutlinerResultKind::Tag, Actor, nullptr, ActorLabel, ActorClass,

                        TMLoc::String(TEXT("Actor tag matched."), TEXT("Actor tag matched.")), TEXT("Actor Tag"), TagText);

                }

            }

        }



        if (bSearchVariableNames || bSearchVariableValues || IsPairSearchActive()) ScanObjectProperties(Actor, Actor, nullptr);



        TInlineComponentArray<UActorComponent*> Components;

        Actor->GetComponents(Components);

        for (UActorComponent* Component : Components)

        {

            if (!Component) continue;

            const FString ComponentName = Component->GetName();

            const FString ComponentClass = Component->GetClass() ? Component->GetClass()->GetName() : TEXT("");

            const FString ComponentPath = Component->GetPathName();



            if (bSearchComponents && (ContainsQuery(ComponentName, Query) || ContainsQuery(ComponentClass, Query) || ContainsQuery(ComponentPath, Query)))

            {

                AddResult(ETMOutlinerResultKind::Component, Actor, Component, FString::Printf(TEXT("%s > %s"), *ActorLabel, *ComponentName), ComponentClass,

                    TMLoc::String(TEXT("Component name, class, or path matched."), TEXT("Component name, class, or path matched.")), TEXT("Component Class"), ComponentClass);

            }



            if (bSearchTags)

            {

                for (const FName& ComponentTag : Component->ComponentTags)

                {

                    const FString TagText = ComponentTag.ToString();

                    if (ContainsQuery(TagText, Query))

                    {

                        AddResult(ETMOutlinerResultKind::Tag, Actor, Component, FString::Printf(TEXT("%s > %s"), *ActorLabel, *ComponentName), ActorLabel,

                            TMLoc::String(TEXT("Component tag matched."), TEXT("Component tag matched.")), TEXT("Component Tag"), TagText);

                    }

                }

            }



            if (bIncludeComponentDetails && (bSearchVariableNames || bSearchVariableValues || IsPairSearchActive())) ScanObjectProperties(Component, Actor, Component);

        }

    }



    void ScanObjectProperties(UObject* Object, AActor* Actor, UActorComponent* Component)

    {

        if (!Object || !Object->GetClass()) return;

        for (TFieldIterator<FProperty> PropIt(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)

        {

            FProperty* Property = *PropIt;

            if (!ShouldInspectProperty(Property)) continue;



            const FString PropertyName = Property->GetName();

            const FString DisplayName = Property->GetDisplayNameText().ToString();

            const FString Category = Property->GetMetaData(TEXT("Category"));

            const FString OwnerText = GetObjectOwnerText(Object);

            const FString SearchableName = FString::Printf(TEXT("%s %s %s"), *PropertyName, *DisplayName, *Category);

            const FString SearchablePairName = FString::Printf(TEXT("%s %s"), *PropertyName, *DisplayName);



            if (bSearchVariableNames && ContainsQuery(SearchableName, Query))

            {

                AddResult(ETMOutlinerResultKind::VariableName, Actor, Component, DisplayName.IsEmpty() ? PropertyName : DisplayName, OwnerText,

                    TMLoc::String(TEXT("Details variable name/display/category matched."), TEXT("Details variable name/display/category matched.")), PropertyName, Category);

            }



            if (!bSearchVariableValues && !IsPairSearchActive()) continue;

            FString PreviewValue;

            const FString SearchableValue = BuildExpandedPropertyValueSearchText(Property, Object, &PreviewValue);

            if (IsPairSearchActive() && ContainsQuery(SearchablePairName, PairVariableNameQuery) && ContainsQuery(SearchableValue, PairVariableValueQuery))

            {

                const FString MatchedPropertyLabel = DisplayName.IsEmpty()

                    ? PropertyName

                    : FString::Printf(TEXT("%s (%s)"), *DisplayName, *PropertyName);

                const FString PairMatchValue = FString::Printf(

                    TEXT("Search pair: name contains \"%s\" + value contains \"%s\" | Matched property: %s | Matched value: %s"),

                    *PairVariableNameQuery,

                    *PairVariableValueQuery,

                    *MatchedPropertyLabel,

                    *PreviewValue);

                const FString PairReason = FString::Printf(

                    TEXT("Details variable pair matched. Name query=\"%s\" matched property text \"%s\"; value query=\"%s\" matched the exported/expanded property value."),

                    *PairVariableNameQuery,

                    *SearchablePairName,

                    *PairVariableValueQuery);

                AddResult(ETMOutlinerResultKind::VariablePair, Actor, Component, DisplayName.IsEmpty() ? PropertyName : DisplayName, OwnerText,

                    PairReason, TEXT("Pair Search"), PairMatchValue);

            }



            if (bSearchVariableValues && ContainsQuery(SearchableValue, Query))

            {

                AddResult(ETMOutlinerResultKind::VariableValue, Actor, Component, DisplayName.IsEmpty() ? PropertyName : DisplayName, OwnerText,

                    TMLoc::String(TEXT("Details variable value matched."), TEXT("Details variable value matched.")), PropertyName, PreviewValue);

            }

            if (Results.Num() >= TMPerf::MaxSearchResults()) return;

        }

    }



    void AddResult(ETMOutlinerResultKind Kind, AActor* Actor, UActorComponent* Component, const FString& Title, const FString& Subtitle, const FString& Reason, const FString& Name, const FString& Value)

    {

        if (Results.Num() >= TMPerf::MaxSearchResults())

        {

            bHitResultCap = true;

            return;

        }

        FTMOutlinerSearchResult& Result = Results.AddDefaulted_GetRef();

        Result.Kind = Kind;

        Result.Actor = Actor;

        Result.Component = Component;

        Result.Title = Title;

        Result.Subtitle = Subtitle;

        Result.Reason = Reason;

        Result.Name = Name;

        Result.Value = Value;

        bResultsDirty = true;

    }



    void RefreshResultsPanel()

    {

        if (!ResultList.IsValid() || !bResultsDirty) return;

        bResultsDirty = false;

        ResultList->ClearChildren();



        if (!HasActiveSearch() || (Results.Num() == 0 && !bScanning))

        {

            const FText Message = !HasActiveSearch()

                ? (HasPartialPairSearch()

                    ? TMLoc::Text(TEXT("Enter both variable name and value to run a pair search."), TEXT("Enter both variable name and value to run a pair search."))

                    : TMLoc::Text(TEXT("Enter a query to search currently loaded level instances."), TEXT("Enter a query to search currently loaded level instances.")))

                : TMLoc::Text(TEXT("No matches found in the current editor world."), TEXT("No matches found in the current editor world."));

            ResultList->AddSlot()[SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed"))).Padding(14.0f)[SNew(STextBlock).Text(Message).ColorAndOpacity(FSlateColor(FLinearColor(0.70f,0.70f,0.70f)))]];

            return;

        }



        for (int32 Index = 0; Index < Results.Num(); ++Index)

        {

            ResultList->AddSlot().Padding(0,0,0,6)[BuildResultRow(Index)];

        }

    }



    TSharedRef<SWidget> BuildResultRow(int32 ResultIndex)

    {

        const FTMOutlinerSearchResult& Result = Results[ResultIndex];

        const bool bIsSelectedResult = (ResultIndex == SelectedResultIndex);

        const FSlateColor RowTint = bIsSelectedResult

            ? FSlateColor(FLinearColor(0.12f, 0.32f, 0.60f, 0.92f))

            : FSlateColor(FLinearColor::White);



        return SNew(SBorder)

        .BorderImage(FAppStyle::GetBrush(bIsSelectedResult ? TEXT("Brushes.Header") : TEXT("Brushes.Recessed")))

        .BorderBackgroundColor(RowTint)

        .Padding(8.0f)

        [

            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight()

            [

                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(ResultKindText(Result.Kind)).ColorAndOpacity(ResultKindColor(Result.Kind))]

                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(10,0,0,0).VAlign(VAlign_Center)

                [

                    SNew(SEditableTextBox)

                    .Text(FText::FromString(Result.Title))

                    .IsReadOnly(true)

                    .SelectAllTextWhenFocused(false)

                ]

                + SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0)[SNew(SButton).Text(TMLoc::Text(TEXT("Select"), TEXT("선택"))).OnClicked_Lambda([this, ResultIndex]() { return SelectResult(ResultIndex); })]

            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)

            [

                SNew(SEditableTextBox)

                .Text(FText::FromString(Result.Subtitle))

                .IsReadOnly(true)

                .SelectAllTextWhenFocused(false)

            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%s: %s"), *Result.Name, *Result.Value)))
                .HighlightText(GetHighlightText())
                .AutoWrapText(true)
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)[SNew(STextBlock).Text(FText::FromString(Result.Reason)).ColorAndOpacity(FSlateColor(FLinearColor(0.70f,0.70f,0.70f)))]

        ];

    }



    FReply SelectResult(int32 ResultIndex)

    {

        if (Results.IsValidIndex(ResultIndex) && GEditor)

        {

            SelectedResultIndex = ResultIndex;

            bResultsDirty = true;

            RefreshResultsPanel();



            if (AActor* Actor = Results[ResultIndex].Actor.Get())

            {

                GEditor->SelectNone(false, true);

                GEditor->SelectActor(Actor, true, true);

                GEditor->NoteSelectionChange();

                GEditor->MoveViewportCamerasToActor(*Actor, true);

            }

        }

        return FReply::Handled();

    }



    FReply CopyResult(int32 ResultIndex)

    {

        if (Results.IsValidIndex(ResultIndex))

        {

            const FString ClipboardText = Results[ResultIndex].ToClipboardLine();

            FPlatformApplicationMisc::ClipboardCopy(*ClipboardText);

        }

        return FReply::Handled();

    }

};



TSharedRef<SDockTab> SpawnEnhancedOutlinerSearchTab(const FSpawnTabArgs& Args)

{

    return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SEnhancedOutlinerSearchWidget)];

}



void RegisterEnhancedOutlinerSearchTab()

{

    if (bEnhancedOutlinerSearchTabRegistered) return;

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(EnhancedOutlinerSearchTabId, FOnSpawnTab::CreateStatic(&SpawnEnhancedOutlinerSearchTab))

        .SetDisplayName(TMLoc::Text(TEXT("Enhanced Outliner Search"), TEXT("Enhanced Outliner Search")))

        .SetTooltipText(TMLoc::Text(TEXT("Search loaded level instances, tags, and Details variables without freezing the editor."), TEXT("Search loaded level instances, tags, and Details variables without freezing the editor.")))

        .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.EnhancedOutlinerSearch")));

    bEnhancedOutlinerSearchTabRegistered = true;

}

}



namespace TMEnhancedOutlinerSearch

{

void RegisterMenus()

{

    RegisterEnhancedOutlinerSearchTab();

    auto AddEntry = [](UToolMenu* Menu, const FName EntryName)

    {

        if (!Menu) return;

        FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("TraceMotive"));

        Section.AddMenuEntry(

            EntryName,

            TMLoc::Text(TEXT("Enhanced Outliner Search"), TEXT("Enhanced Outliner Search")),

            TMLoc::Text(TEXT("Search actor tags, variable names, and variable values in current level instances."), TEXT("Search actor tags, variable names, and variable values in current level instances.")),

            FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.EnhancedOutlinerSearch")),

            FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&) { TMEnhancedOutlinerSearch::OpenWindow(); }));

    };

    if (UToolMenus::IsToolMenuUIEnabled())

    {

        AddEntry(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")), TEXT("TMOpenEnhancedOutlinerSearch"));

        AddEntry(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")), TEXT("TMToolsOpenEnhancedOutlinerSearch"));

    }

}



void UnregisterMenus()

{

    if (bEnhancedOutlinerSearchTabRegistered)

    {

        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(EnhancedOutlinerSearchTabId);

        bEnhancedOutlinerSearchTabRegistered = false;

    }

}



void OpenWindow()

{

    RegisterEnhancedOutlinerSearchTab();

    FGlobalTabmanager::Get()->TryInvokeTab(EnhancedOutlinerSearchTabId);

}

}







