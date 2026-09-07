// Source/TraceMotive/Private/TraceMotive.cpp

#include "TraceMotive.h"
#include "TraceMotiveLog.h"
#include "TMStyle.h"
#include "TMAudioPlaybackTrace.h"
#include "TMAssetUsageLocator.h"
#include "TMBlueprintRuntimeErrorTrace.h"
#include "TMClassFavorites.h"
#include "TMClickDiagnostics.h"
#include "TMCollisionPairAnalyzer.h"
#include "TMContextShortcutHelper.h"
#include "TMEnhancedOutlinerSearch.h"
#include "TMGlobalSpeedControl.h"
#include "TMInvestigationSession.h"
#include "TMPackageProgress.h"
#include "TMPluginGuide.h"
#include "TMSettings.h"
#include "TMToolLauncher.h"
#include "TMVariableValueTrace.h"
#include "TMVisualRefReport.h"
#include "TMWidgetClickFlowTrace.h"
#include "TMWidgetLifecycleTrace.h"
#include "FunctionCallChainTracer.h"
#include "SCallChainViewer.h"
#include "SGraphAnnotationOverlay.h"
#include "SVisualRefNode.h"
#include "VisualRefGraphDefinition.h"
#include "VisualRefSearcher.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_DelegateSet.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/Paths.h"
#include "ToolMenus.h"
#include "UObject/UnrealType.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "FTraceMotiveModule"

DEFINE_LOG_CATEGORY(LogTraceMotive);

struct FVisualRefNodeFactory : public FGraphPanelNodeFactory
{
    virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* Node) const override
    {
        if (UVisualRefNode* VisualNode = Cast<UVisualRefNode>(Node))
        {
            return SNew(SVisualRefNode, VisualNode);
        }
        return nullptr;
    }
};

void FTraceMotiveModule::StartupModule()
{
    TMStyle::Initialize();
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FTraceMotiveModule::RegisterMenus));
    VisualRefNodeFactory = MakeShareable(new FVisualRefNodeFactory());
    FEdGraphUtilities::RegisterVisualNodeFactory(VisualRefNodeFactory);
}

void FTraceMotiveModule::ShutdownModule()
{
    // Each feature owns any tab spawner, ticker, or editor delegate it creates.
    // Unregister before removing the shared ToolMenus owner during module unload.
    TMWidgetLifecycleTrace::UnregisterMenus();
    TMWidgetClickFlowTrace::UnregisterMenus();
    TMInvestigationSession::UnregisterTab();
    TMToolLauncher::UnregisterMenus();
    TMPluginGuide::UnregisterMenus();
    TMPackageProgress::UnregisterMenus();
    TMGlobalSpeedControl::UnregisterMenus();
    TMEnhancedOutlinerSearch::UnregisterMenus();
    TMContextShortcutHelper::UnregisterMenus();
    TMCollisionPairAnalyzer::UnregisterMenus();
    TMClickDiagnostics::UnregisterMenus();
    TMClassFavorites::UnregisterMenus();
    TMBlueprintRuntimeErrorTrace::UnregisterMenus();
    TMAudioPlaybackTrace::UnregisterMenus();
    TMVariableValueTrace::UnregisterMenus();
    if (VisualRefNodeFactory.IsValid())
    {
        FEdGraphUtilities::UnregisterVisualNodeFactory(VisualRefNodeFactory);
        VisualRefNodeFactory.Reset();
    }
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    TMStyle::Shutdown();
}

void FTraceMotiveModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    // Feature registration owns tab spawners and context menus. The launcher runs last
    // so it can consolidate the main Window/Tools entries into one product menu.
    TMAssetUsageLocator::RegisterMenus();
    TMAudioPlaybackTrace::RegisterMenus();
    TMBlueprintRuntimeErrorTrace::RegisterMenus();
    TMClassFavorites::RegisterMenus();
    TMClickDiagnostics::RegisterMenus();
    TMCollisionPairAnalyzer::RegisterMenus();
    TMContextShortcutHelper::RegisterMenus();
    TMEnhancedOutlinerSearch::RegisterMenus();
    TMGlobalSpeedControl::RegisterMenus();
    TMInvestigationSession::RegisterTab();
    TMPackageProgress::RegisterMenus();
    TMPluginGuide::RegisterMenus();
    TMVariableValueTrace::RegisterMenus();
    TMWidgetClickFlowTrace::RegisterMenus();
    TMWidgetLifecycleTrace::RegisterMenus();
    TMToolLauncher::RegisterMenus();


    TArray<FName> VariableMenus =
    {
        "GraphEditor.GraphNodeContextMenu.K2Node_Variable",
        "GraphEditor.GraphNodeContextMenu.K2Node_VariableSet",
        "GraphEditor.GraphNodeContextMenu.K2Node_FunctionEntry",
        "BlueprintEditor.MyBlueprint.Function",
        "BlueprintEditor.MyBlueprint.Variable"
    };

    for (const FName& MenuName : VariableMenus)
    {
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuName);
        FToolMenuSection& Section = Menu->FindOrAddSection("EdGraphSchemaGeneral");

        Section.AddMenuEntry(
            "VisualFindReferences",
            LOCTEXT("VisualFindRef", "Visual Find References"),
            LOCTEXT("VisualFindRefTooltip", "Show references in a visual graph editor"),
            FSlateIcon(TMStyle::GetStyleSetName(), "TraceMotive.VisualReferenceSearch"),
            FToolMenuExecuteAction::CreateRaw(this, &FTraceMotiveModule::OnVisualFindReferences)
        );
    }


    TArray<FName> FunctionMenus =
    {
        "GraphEditor.GraphNodeContextMenu.K2Node_CallFunction",
        "GraphEditor.GraphNodeContextMenu.K2Node_Event",
        "GraphEditor.GraphNodeContextMenu.K2Node_CustomEvent",
        "BlueprintEditor.MyBlueprint.Function"
    };

    for (const FName& MenuName : FunctionMenus)
    {
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuName);
        FToolMenuSection& Section = Menu->FindOrAddSection("EdGraphSchemaGeneral");

        Section.AddMenuEntry(
            "VisualFindFunctionReferences",
            LOCTEXT("VisualFindFuncRef", "Visual Find Function References"),
            LOCTEXT("VisualFindFuncRefTooltip", "Show function references in a visual graph editor"),
            FSlateIcon(TMStyle::GetStyleSetName(), "TraceMotive.VisualReferenceSearch"),
            FToolMenuExecuteAction::CreateRaw(this, &FTraceMotiveModule::OnVisualFindFunctionReferences)
        );
    }

    const TArray<FName> DispatcherMenus =
    {
        "GraphEditor.GraphNodeContextMenu.K2Node_CallDelegate",
        "GraphEditor.GraphNodeContextMenu.K2Node_AddDelegate",
        "GraphEditor.GraphNodeContextMenu.K2Node_RemoveDelegate",
        "GraphEditor.GraphNodeContextMenu.K2Node_ClearDelegate",
        "GraphEditor.GraphNodeContextMenu.K2Node_AssignDelegate",
        "GraphEditor.GraphNodeContextMenu.K2Node_DelegateSet"
    };
    for (const FName& MenuName : DispatcherMenus)
    {
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuName);
        FToolMenuSection& Section = Menu->FindOrAddSection("EdGraphSchemaGeneral");
        Section.AddMenuEntry(
            "VisualFindDispatcherReferences",
            LOCTEXT("VisualFindDispatcherRef", "Visual Find Dispatcher References"),
            LOCTEXT("VisualFindDispatcherRefTooltip", "Find Call, Bind, Unbind, Assign, and Event references for this Event Dispatcher"),
            FSlateIcon(TMStyle::GetStyleSetName(), "TraceMotive.VisualReferenceSearch"),
            FToolMenuExecuteAction::CreateRaw(this, &FTraceMotiveModule::OnVisualFindReferences));
    }
}

void FTraceMotiveModule::OnVisualFindReferences(const FToolMenuContext& Context)
{
    const UEdGraphNode* ClickedNode = nullptr;
    if (const UGraphNodeContextMenuContext* GraphContext = Context.FindContext<UGraphNodeContextMenuContext>())
    {
        ClickedNode = GraphContext->Node;
    }

    UBlueprint* ContextBlueprint = ClickedNode ? FBlueprintEditorUtils::FindBlueprintForNode(ClickedNode) : nullptr;
    FName DispatcherName = NAME_None;
    UClass* DispatcherOwnerClass = nullptr;

    if (const UK2Node_BaseMCDelegate* DelegateNode = Cast<const UK2Node_BaseMCDelegate>(ClickedNode))
    {
        DispatcherName = DelegateNode->GetPropertyName();
        DispatcherOwnerClass = DelegateNode->DelegateReference.GetMemberParentClass();
        if (!DispatcherOwnerClass)
        {
            if (const FProperty* DelegateProperty = DelegateNode->GetProperty())
            {
                DispatcherOwnerClass = DelegateProperty->GetOwnerClass();
            }
        }
    }
    else if (const UK2Node_DelegateSet* DelegateSetNode = Cast<const UK2Node_DelegateSet>(ClickedNode))
    {
        DispatcherName = DelegateSetNode->DelegatePropertyName;
        DispatcherOwnerClass = DelegateSetNode->DelegatePropertyClass.Get();
    }
    else if (const UK2Node_Variable* VariableNode = Cast<const UK2Node_Variable>(ClickedNode))
    {
        const FName CandidateName = VariableNode->GetVarName();
        UClass* CandidateOwner = VariableNode->VariableReference.GetMemberParentClass();
        if (CandidateOwner)
        {
            if (const FMulticastDelegateProperty* DelegateProperty = FindFProperty<FMulticastDelegateProperty>(CandidateOwner, CandidateName))
            {
                DispatcherName = CandidateName;
                DispatcherOwnerClass = DelegateProperty->GetOwnerClass();
            }
        }
    }

    if (!DispatcherName.IsNone())
    {
        UBlueprint* TargetBlueprint = DispatcherOwnerClass ? Cast<UBlueprint>(DispatcherOwnerClass->ClassGeneratedBy) : nullptr;
        if (!TargetBlueprint)
        {
            TargetBlueprint = ContextBlueprint;
        }
        if (TargetBlueprint)
        {
            OpenVisualReferenceViewer(TargetBlueprint, DispatcherName, ClickedNode, false, DispatcherOwnerClass, true);
            return;
        }
    }

    if (const UK2Node_Variable* VariableNode = Cast<const UK2Node_Variable>(ClickedNode))
    {
        const FName VariableName = VariableNode->GetVarName();
        UBlueprint* TargetBlueprint = nullptr;
        UClass* VariableOwnerClass = VariableNode->VariableReference.GetMemberParentClass();
        if (VariableOwnerClass)
        {
            TargetBlueprint = Cast<UBlueprint>(VariableOwnerClass->ClassGeneratedBy);
        }
        if (!TargetBlueprint)
        {
            TargetBlueprint = FBlueprintEditorUtils::FindBlueprintForNode(VariableNode);
        }
        if (TargetBlueprint && !VariableName.IsNone())
        {
            OpenVisualReferenceViewer(TargetBlueprint, VariableName, ClickedNode, false, VariableOwnerClass);
        }
    }
}
void FTraceMotiveModule::OnVisualFindFunctionReferences(const FToolMenuContext& Context)
{
    const UEdGraphNode* ClickedNode = nullptr;
    if (const UGraphNodeContextMenuContext* GraphContext = Context.FindContext<UGraphNodeContextMenuContext>())
    {
        ClickedNode = GraphContext->Node;
    }

    FName FunctionName = NAME_None;
    UBlueprint* TargetBlueprint = nullptr;
    UClass* TargetFunctionOwnerClass = nullptr;
    UBlueprint* ContextBlueprint = ClickedNode ? FBlueprintEditorUtils::FindBlueprintForNode(ClickedNode) : nullptr;

    // Event Dispatcher event nodes use UK2Node_Event too. Detect the multicast delegate property before treating it as a function.
    if (const UK2Node_Event* EventNode = Cast<const UK2Node_Event>(ClickedNode))
    {
        const FName DispatcherName = EventNode->EventReference.GetMemberName();
        UClass* DispatcherOwnerClass = EventNode->EventReference.GetMemberParentClass();
        if (DispatcherOwnerClass && FindFProperty<FMulticastDelegateProperty>(DispatcherOwnerClass, DispatcherName))
        {
            UBlueprint* DispatcherBlueprint = Cast<UBlueprint>(DispatcherOwnerClass->ClassGeneratedBy);
            OpenVisualReferenceViewer(DispatcherBlueprint ? DispatcherBlueprint : ContextBlueprint, DispatcherName, ClickedNode, false, DispatcherOwnerClass, true);
            return;
        }
    }

    if (const UK2Node_CallFunction* CallNode = Cast<const UK2Node_CallFunction>(ClickedNode))
    {
        FunctionName = CallNode->FunctionReference.GetMemberName();

        if (UFunction* TargetFunction = CallNode->GetTargetFunction())
        {
            TargetFunctionOwnerClass = TargetFunction->GetOwnerClass();
        }

        if (!TargetFunctionOwnerClass)
        {
            TargetFunctionOwnerClass = CallNode->FunctionReference.GetMemberParentClass();
        }

        if (TargetFunctionOwnerClass)
        {
            TargetBlueprint = Cast<UBlueprint>(TargetFunctionOwnerClass->ClassGeneratedBy);
        }

        if (!TargetBlueprint)
        {
            TargetBlueprint = ContextBlueprint;
        }
    }

    else if (const UK2Node_CustomEvent* CustomEvent = Cast<const UK2Node_CustomEvent>(ClickedNode))
    {
        FunctionName = CustomEvent->CustomFunctionName;
        TargetBlueprint = ContextBlueprint;
    }

    else if (const UK2Node_Event* EventNode = Cast<const UK2Node_Event>(ClickedNode))
    {
        FunctionName = EventNode->EventReference.GetMemberName();
        TargetFunctionOwnerClass = EventNode->EventReference.GetMemberParentClass();

        if (TargetFunctionOwnerClass)
        {
            TargetBlueprint = Cast<UBlueprint>(TargetFunctionOwnerClass->ClassGeneratedBy);
        }

        if (!TargetBlueprint)
        {
            TargetBlueprint = ContextBlueprint;
        }
    }

    else if (const UK2Node_FunctionEntry* EntryNode = Cast<const UK2Node_FunctionEntry>(ClickedNode))
    {
        UEdGraph* FuncGraph = EntryNode->GetGraph();
        if (FuncGraph)
        {
            FunctionName = FuncGraph->GetFName();
            TargetBlueprint = ContextBlueprint;
        }
    }

    if (!TargetFunctionOwnerClass && TargetBlueprint)
    {
        TargetFunctionOwnerClass = TargetBlueprint->GeneratedClass ? TargetBlueprint->GeneratedClass : TargetBlueprint->SkeletonGeneratedClass;
    }

    if (TargetBlueprint && !FunctionName.IsNone())
    {
        OpenVisualReferenceViewer(TargetBlueprint, FunctionName, ClickedNode, true, TargetFunctionOwnerClass);
    }
    else
    {
        UE_LOG(LogTraceMotive, Warning, TEXT("VisualFindRef Failed: BP=%s, Func=%s, Owner=%s"),
            TargetBlueprint ? *TargetBlueprint->GetName() : TEXT("NULL"),
            *FunctionName.ToString(),
            TargetFunctionOwnerClass ? *TargetFunctionOwnerClass->GetName() : TEXT("NULL"));
    }
}

static int32 CalculateExecutionOrder(UEdGraphNode* Node)
{
    if (!Node) return 0;


    int32 Order = 0;
    UEdGraphNode* CurrentNode = Node;

    while (CurrentNode)
    {
        UEdGraphPin* ExecInputPin = nullptr;


        for (UEdGraphPin* Pin : CurrentNode->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Input &&
                Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
            {
                ExecInputPin = Pin;
                break;
            }
        }

        if (!ExecInputPin || ExecInputPin->LinkedTo.Num() == 0)
        {
            break;
        }


        CurrentNode = ExecInputPin->LinkedTo[0]->GetOwningNode();
        Order++;


        if (Order > 1000) break;
    }

    return Order;
}

static FString NormalizeVisualBlueprintClassName(FString ClassName)
{
    ClassName.RemoveFromStart(TEXT("SKEL_"));
    ClassName.RemoveFromStart(TEXT("REINST_"));
    ClassName.RemoveFromEnd(TEXT("_C"));

    int32 SuffixIndex = INDEX_NONE;
    if (ClassName.FindLastChar(TEXT('_'), SuffixIndex))
    {
        const FString Suffix = ClassName.Mid(SuffixIndex + 1);
        if (Suffix.IsNumeric())
        {
            ClassName.LeftInline(SuffixIndex);
            ClassName.RemoveFromEnd(TEXT("_C"));
        }
    }

    return ClassName;
}

static FString GetVisualClassDisplayName(UClass* Class)
{
    if (!Class)
    {
        return FString();
    }

    if (UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy))
    {
        return Blueprint->GetName();
    }

    FString ClassName = NormalizeVisualBlueprintClassName(Class->GetName());
    ClassName.RemoveFromStart(TEXT("A"));
    ClassName.RemoveFromStart(TEXT("U"));
    return ClassName;
}

static UClass* GetVisualPinClass(UEdGraphPin* Pin)
{
    if (!Pin || !Pin->PinType.PinSubCategoryObject.IsValid())
    {
        return nullptr;
    }

    return Cast<UClass>(Pin->PinType.PinSubCategoryObject.Get());
}

static FString GetVisualVariableNodeName(UK2Node_Variable* VariableNode)
{
    if (!VariableNode)
    {
        return FString();
    }

    FString DisplayName = VariableNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
    DisplayName.RemoveFromStart(TEXT("Get "));
    DisplayName.RemoveFromStart(TEXT("Set "));

    if (DisplayName.IsEmpty())
    {
        DisplayName = FName::NameToDisplayString(VariableNode->GetVarName().ToString(), false);
    }

    return DisplayName;
}

static void ExtractFunctionTargetInfo(UEdGraphNode* Node, UClass* DefaultOwnerClass, FString& OutObjectName, FString& OutClassName)
{
    OutObjectName.Empty();
    OutClassName.Empty();

    UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
    if (!CallNode)
    {
        return;
    }

    UClass* TargetClass = nullptr;
    UEdGraphPin* SelfPin = CallNode->FindPin(UEdGraphSchema_K2::PN_Self);
    if (SelfPin)
    {
        TargetClass = GetVisualPinClass(SelfPin);

        for (UEdGraphPin* LinkedPin : SelfPin->LinkedTo)
        {
            if (!LinkedPin)
            {
                continue;
            }

            if (UClass* LinkedClass = GetVisualPinClass(LinkedPin))
            {
                TargetClass = LinkedClass;
            }

            UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
            if (UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(LinkedNode))
            {
                OutObjectName = GetVisualVariableNodeName(VariableNode);
            }
            else if (LinkedNode)
            {
                OutObjectName = LinkedNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
                OutObjectName.RemoveFromStart(TEXT("Get "));
                OutObjectName.RemoveFromStart(TEXT("Set "));
            }

            if (!OutObjectName.IsEmpty())
            {
                break;
            }
        }

        if (SelfPin->LinkedTo.Num() == 0)
        {
            OutObjectName = TEXT("Self");
        }
    }

    if (!TargetClass)
    {
        if (UFunction* TargetFunction = CallNode->GetTargetFunction())
        {
            TargetClass = TargetFunction->GetOwnerClass();
        }
    }

    if (!TargetClass)
    {
        TargetClass = CallNode->FunctionReference.GetMemberParentClass();
    }

    if (!TargetClass)
    {
        TargetClass = DefaultOwnerClass;
    }

    if (TargetClass)
    {
        OutClassName = GetVisualClassDisplayName(TargetClass);
    }
}

void FTraceMotiveModule::OpenVisualReferenceViewer(UBlueprint* InBlueprint, FName InName, const UEdGraphNode* InSourceNode, bool bIsFunction, UClass* InTargetOwnerClass, bool bIsDispatcher)
{
    if (!bIsFunction)
    {
        SVisualRefNode::CurrentViewMode = EVisualRefViewMode::Tree;
    }

    UEdGraph* MyGraph = NewObject<UEdGraph>(GetTransientPackage(), NAME_None, RF_Transient);
    MyGraph->AddToRoot();
    MyGraph->Schema = UVisualRefSchema::StaticClass();
    MyGraph->bAllowDeletion = false;
    MyGraph->bAllowRenaming = false;

    UVisualRefNode* RootNode = NewObject<UVisualRefNode>(MyGraph);
    RootNode->CreateNewGuid();
    RootNode->NodePosX = 0;
    RootNode->NodePosY = 0;

    FString RootAssetName = InBlueprint->GetName();
    if ((bIsFunction || bIsDispatcher) && InTargetOwnerClass)
    {
        if (UBlueprint* OwnerBlueprint = Cast<UBlueprint>(InTargetOwnerClass->ClassGeneratedBy))
        {
            RootAssetName = OwnerBlueprint->GetName();
        }
        else
        {
            RootAssetName = InTargetOwnerClass->GetName();
            RootAssetName.RemoveFromEnd(TEXT("_C"));
        }
    }
    FString TypeLabel = bIsDispatcher ? TEXT("Event Dispatcher") : (bIsFunction ? TEXT("Function") : TEXT("Variable"));
    RootNode->AssetName = FText::Format(LOCTEXT("DefTitle", "{0}\n({1} Definition)"),
        FText::FromString(RootAssetName), FText::FromString(TypeLabel));
    RootNode->SourceAsset = InBlueprint;
    RootNode->bIsDefinitionNode = true;
    RootNode->RefType = bIsDispatcher ? EVisualRefType::Dispatcher : (bIsFunction ? EVisualRefType::Function : EVisualRefType::Variable);
    RootNode->CreateVisualPin(EGPD_Input, FName(TEXT("UsedBy")));

    MyGraph->AddNode(RootNode);

    SGraphEditor::FGraphEditorEvents GraphEvents;
    GraphEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateLambda([](UEdGraphNode* Node)
        {
            if (UVisualRefNode* MyNode = Cast<UVisualRefNode>(Node)) MyNode->JumpToDefinition();
        });

    TSharedRef<SGraphEditor> GraphEditor = SNew(SGraphEditor)
        .GraphToEdit(MyGraph)
        .IsEditable(true)
        .TitleBar(SNew(STextBlock).Text(LOCTEXT("GraphTitle", "Visual Reference Viewer")))
        .GraphEvents(GraphEvents);

    static TArray<TSharedPtr<FString>> ViewModeOptions;
    if (ViewModeOptions.Num() == 0)
    {
        ViewModeOptions.Add(MakeShared<FString>("Tree View"));
        ViewModeOptions.Add(MakeShared<FString>("List View"));
    }

    static TArray<TSharedPtr<FString>> LayoutModeOptions;
    if (LayoutModeOptions.Num() == 0)
    {
        LayoutModeOptions.Add(MakeShared<FString>("Frame Grid"));
        LayoutModeOptions.Add(MakeShared<FString>("Row Layout"));
    }

    static TArray<TSharedPtr<FString>> ScopeOptions;
    if (ScopeOptions.Num() == 0)
    {
        ScopeOptions.Add(MakeShared<FString>("Project Content"));
        ScopeOptions.Add(MakeShared<FString>("Current Blueprint"));
        ScopeOptions.Add(MakeShared<FString>("Same Folder"));
    }

    static TArray<TSharedPtr<FString>> ExportFormatOptions;
    if (ExportFormatOptions.Num() == 0)
    {
        ExportFormatOptions.Add(MakeShared<FString>("Markdown"));
        ExportFormatOptions.Add(MakeShared<FString>("CSV"));
        ExportFormatOptions.Add(MakeShared<FString>("JSON"));
        ExportFormatOptions.Add(MakeShared<FString>("Mermaid"));
    }

    enum class EVisualRefLayoutMode : uint8 { FrameGrid, Row };

    struct FGroupContext
    {
        UEdGraph* Graph;
        TWeakPtr<SGraphEditor> WeakEditor;
        TMap<FString, UVisualRefNode*> AssetNodeMap;
        int32 ColumnCount = 0;
        FString RootAssetName;
        UVisualRefNode* RootNode = nullptr;
        TWeakPtr<SWidget> WeakLoadingWidget;
        const UEdGraphNode* SourceNode = nullptr;
        UBlueprint* Blueprint = nullptr;
        FName Name;
        bool bIsFunction = false;
        bool bIsDispatcher = false;
        UClass* TargetOwnerClass = nullptr;
        EVisualRefLayoutMode LayoutMode = EVisualRefLayoutMode::FrameGrid;
        EVisualRefSearchScope SearchScope = EVisualRefSearchScope::ProjectContent;
        EVisualRefExportFormat ExportFormat = EVisualRefExportFormat::Markdown;
        bool bFullScan = false;
        bool bShowReferenceLines = true;
        TWeakPtr<SWidget> WeakGraphEditorWidget;
        TWeakPtr<SWidget> WeakCallChainWidget;
        bool bShowingCallChain = false;
        TWeakPtr<SProgressBar> WeakProgressBar;
        TWeakPtr<STextBlock> WeakProgressText;

        FGroupContext(UEdGraph* InGraph, TSharedRef<SGraphEditor> InEditor, FString InRootName,
            UVisualRefNode* InRootNode, const UEdGraphNode* InSource,
            UBlueprint* InBlueprint, FName InName, bool bInIsFunction, UClass* InTargetOwnerClass, bool bInIsDispatcher)
            : Graph(InGraph), WeakEditor(InEditor), RootAssetName(InRootName), RootNode(InRootNode),
            SourceNode(InSource), Blueprint(InBlueprint), Name(InName), bIsFunction(bInIsFunction), bIsDispatcher(bInIsDispatcher), TargetOwnerClass(InTargetOwnerClass)
        {
        }
    };

    TSharedPtr<FGroupContext> Ctx = MakeShareable(new FGroupContext(
        MyGraph, GraphEditor, RootAssetName, RootNode, InSourceNode, InBlueprint, InName, bIsFunction, InTargetOwnerClass, bIsDispatcher));
    switch (UTraceMotiveSettings::Get()->DefaultReportFormat)
    {
    case ETMDefaultReportFormat::CSV: Ctx->ExportFormat = EVisualRefExportFormat::CSV; break;
    case ETMDefaultReportFormat::JSON: Ctx->ExportFormat = EVisualRefExportFormat::JSON; break;
    case ETMDefaultReportFormat::Mermaid: Ctx->ExportFormat = EVisualRefExportFormat::Mermaid; break;
    default: Ctx->ExportFormat = EVisualRefExportFormat::Markdown; break;
    }

    auto EnsureVisualRefPins = [](UVisualRefNode* Node)
        {
            if (!Node)
            {
                return;
            }

            const EEdGraphPinDirection Direction = Node->bIsDefinitionNode ? EGPD_Input : EGPD_Output;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->Direction == Direction)
                {
                    return;
                }
            }
            Node->CreateVisualPin(Direction, Node->bIsDefinitionNode ? FName(TEXT("UsedBy")) : FName(TEXT("Uses")));
        };

    auto FindVisualRefPin = [](UVisualRefNode* Node, EEdGraphPinDirection Direction) -> UEdGraphPin*
        {
            if (!Node)
            {
                return nullptr;
            }
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->Direction == Direction)
                {
                    return Pin;
                }
            }
            return nullptr;
        };

    auto RebuildReferenceLinks = [Ctx, EnsureVisualRefPins, FindVisualRefPin]()
        {
            if (!Ctx->Graph || !Ctx->RootNode)
            {
                return;
            }

            TArray<UVisualRefNode*> ReferenceNodes;
            for (UEdGraphNode* GraphNode : Ctx->Graph->Nodes)
            {
                if (UVisualRefNode* VisualNode = Cast<UVisualRefNode>(GraphNode))
                {
                    EnsureVisualRefPins(VisualNode);
                    for (UEdGraphPin* Pin : VisualNode->Pins)
                    {
                        if (Pin)
                        {
                            Pin->BreakAllPinLinks();
                        }
                    }

                    if (VisualNode != Ctx->RootNode && !VisualNode->bIsDefinitionNode)
                    {
                        ReferenceNodes.Add(VisualNode);
                    }
                }
            }

            ReferenceNodes.Sort([](const UVisualRefNode& A, const UVisualRefNode& B)
                {
                    return A.AssetName.ToString() < B.AssetName.ToString();
                });

            // UML-style association ends: each source gets an individual target port rather than converging on one point.
            TArray<UEdGraphPin*> RootInputs;
            for (UEdGraphPin* Pin : Ctx->RootNode->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Input)
                {
                    RootInputs.Add(Pin);
                }
            }
            while (RootInputs.Num() < ReferenceNodes.Num())
            {
                Ctx->RootNode->CreateVisualPin(EGPD_Input, FName(*FString::Printf(TEXT("UsedBy_%d"), RootInputs.Num())));
                RootInputs.Add(Ctx->RootNode->Pins.Last());
            }

            if (!Ctx->bShowReferenceLines)
            {
                Ctx->Graph->NotifyGraphChanged();
                return;
            }

            for (int32 ReferenceIndex = 0; ReferenceIndex < ReferenceNodes.Num(); ++ReferenceIndex)
            {
                if (UEdGraphPin* ReferenceOutput = FindVisualRefPin(ReferenceNodes[ReferenceIndex], EGPD_Output))
                {
                    // Every connector terminates at its own port, preventing a dense arrowhead pile-up.
                    ReferenceOutput->MakeLinkTo(RootInputs[ReferenceIndex]);
                }
            }
            Ctx->Graph->NotifyGraphChanged();
        };

    auto ArrangeVisualRefNodes = [Ctx]()
        {
            if (!Ctx->Graph || !Ctx->RootNode)
            {
                return;
            }

            TArray<UVisualRefNode*> ReferenceNodes;
            for (UEdGraphNode* GraphNode : Ctx->Graph->Nodes)
            {
                if (UVisualRefNode* Node = Cast<UVisualRefNode>(GraphNode))
                {
                    if (!Node->bIsDefinitionNode)
                    {
                        ReferenceNodes.Add(Node);
                    }
                }
            }
            ReferenceNodes.Sort([](const UVisualRefNode& A, const UVisualRefNode& B) { return A.AssetName.ToString() < B.AssetName.ToString(); });

            constexpr float HorizontalSpacing = 340.0f;
            constexpr float VerticalSpacing = 310.0f;
            constexpr float StartX = 80.0f;
            if (Ctx->LayoutMode == EVisualRefLayoutMode::FrameGrid && ReferenceNodes.Num() > 0)
            {
                const int32 Columns = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(ReferenceNodes.Num()))));
                Ctx->RootNode->NodePosX = FMath::RoundToInt(StartX + ((Columns - 1) * HorizontalSpacing * 0.5f));
                Ctx->RootNode->NodePosY = 0;
                for (int32 Index = 0; Index < ReferenceNodes.Num(); ++Index)
                {
                    ReferenceNodes[Index]->NodePosX = FMath::RoundToInt(StartX + ((Index % Columns) * HorizontalSpacing));
                    ReferenceNodes[Index]->NodePosY = FMath::RoundToInt((1 + (Index / Columns)) * VerticalSpacing);
                }
            }
            else
            {
                Ctx->RootNode->NodePosX = FMath::RoundToInt(StartX + FMath::Max(0, ReferenceNodes.Num() - 1) * HorizontalSpacing * 0.5f);
                Ctx->RootNode->NodePosY = 0;
                for (int32 Index = 0; Index < ReferenceNodes.Num(); ++Index)
                {
                    ReferenceNodes[Index]->NodePosX = FMath::RoundToInt(StartX + (Index * HorizontalSpacing));
                    ReferenceNodes[Index]->NodePosY = FMath::RoundToInt(VerticalSpacing);
                }
            }
        };

    auto PerformSearch = [this, Ctx, ArrangeVisualRefNodes, RebuildReferenceLinks]()
        {
            TArray<UEdGraphNode*> NodesToRemove;
            for (UEdGraphNode* Node : Ctx->Graph->Nodes)
            {
                if (UVisualRefNode* RefNode = Cast<UVisualRefNode>(Node))
                {
                    if (!RefNode->bIsDefinitionNode)
                    {
                        NodesToRemove.Add(Node);
                    }
                    else
                    {
                        RefNode->References.Empty();
                    }
                }
            }

            for (UEdGraphNode* Node : NodesToRemove)
            {
                Ctx->Graph->RemoveNode(Node);
            }

            Ctx->AssetNodeMap.Empty();
            Ctx->ColumnCount = 0;

            if (TSharedPtr<SWidget> LoadingWidget = Ctx->WeakLoadingWidget.Pin())
            {
                LoadingWidget->SetVisibility(EVisibility::Visible);
            }

            if (TSharedPtr<SProgressBar> ProgressBar = Ctx->WeakProgressBar.Pin())
            {
                ProgressBar->SetPercent(0.0f);
            }
            if (TSharedPtr<STextBlock> ProgressText = Ctx->WeakProgressText.Pin())
            {
                ProgressText->SetText(LOCTEXT("InitializingSearchText", "Initializing Search..."));
            }

            TSharedPtr<VisualRefSearcher> Searcher = MakeShareable(
                new VisualRefSearcher(Ctx->Blueprint, Ctx->Name, Ctx->bIsFunction, Ctx->TargetOwnerClass, Ctx->bIsDispatcher));
            Searcher->OnSearchProgress.BindLambda([Ctx](int32 Current, int32 Total)
                {
                    float Percent = (Total > 0) ? (float)Current / (float)Total : 0.0f;


                    if (TSharedPtr<SProgressBar> ProgressBar = Ctx->WeakProgressBar.Pin())
                    {
                        ProgressBar->SetPercent(Percent);
                    }


                    if (TSharedPtr<STextBlock> ProgressText = Ctx->WeakProgressText.Pin())
                    {
                        FString StatusStr = FString::Printf(TEXT("Searching... %d%% (%d/%d)"),
                            FMath::RoundToInt(Percent * 100.0f), Current, Total);
                        ProgressText->SetText(FText::FromString(StatusStr));
                    }
                });
            Searcher->OnRefFound.BindLambda([Ctx](UEdGraphNode* FoundGraphNode)
                {
                    TSharedPtr<SGraphEditor> PinnedEditor = Ctx->WeakEditor.Pin();
                    if (!PinnedEditor.IsValid() || !FoundGraphNode) return;

                    FString AssetName = TEXT("Unknown");
                    if (UPackage* Package = FoundGraphNode->GetOutermost())
                    {
                        AssetName = FPaths::GetBaseFilename(Package->GetName());
                    }

                    UVisualRefNode* TargetNode = nullptr;
                    const float RefNodeY = 0.0f;


                    bool bIsDefinitionAsset = (AssetName == Ctx->RootAssetName);

                    if (bIsDefinitionAsset)
                    {
                        TargetNode = Ctx->RootNode;
                    }
                    else
                    {

                        if (Ctx->AssetNodeMap.Contains(AssetName))
                        {
                            TargetNode = Ctx->AssetNodeMap[AssetName];
                        }
                        else
                        {
                            TargetNode = NewObject<UVisualRefNode>(Ctx->Graph);
                            TargetNode->CreateNewGuid();
                            TargetNode->NodePosX = 350.0f + (Ctx->ColumnCount * 320.0f);
                            TargetNode->NodePosY = RefNodeY;


                            if (bIsDefinitionAsset)
                            {
                                TargetNode->AssetName = FText::Format(
                                    LOCTEXT("SelfRefTitle", "{0}\n(Self References)"),
                                    FText::FromString(AssetName)
                                );
                            }
                            else
                            {
                                TargetNode->AssetName = FText::FromString(AssetName);
                            }

                            TargetNode->SourceAsset = FoundGraphNode->GetGraph()->GetOuter();
                            TargetNode->bIsDefinitionNode = false;
                            TargetNode->RefType = Ctx->bIsDispatcher ? EVisualRefType::Dispatcher : (Ctx->bIsFunction ? EVisualRefType::Function : EVisualRefType::Variable);
                            TargetNode->CreateVisualPin(EGPD_Output, FName(TEXT("Uses")));

                            Ctx->Graph->AddNode(TargetNode);
                            Ctx->AssetNodeMap.Add(AssetName, TargetNode);
                            Ctx->ColumnCount++;
                        }

                    }


                    if (!TargetNode)
                    {
                        return;
                    }

                    FVisualReferenceInfo NewRef;
                    FString GraphName = FoundGraphNode->GetGraph()->GetName();
                    GraphName.RemoveFromStart("ExecuteUbergraph_");

                    NewRef.GraphName = FString::Printf(TEXT("[%s]"), *GraphName);
                    NewRef.NodeName = FoundGraphNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
                    NewRef.SourceNode = FoundGraphNode;

                    if (Ctx->bIsFunction || Ctx->bIsDispatcher)
                    {
                        NewRef.bIsFunctionCall = true;

                        NewRef.CallOrder = CalculateExecutionOrder(FoundGraphNode);
                        ExtractFunctionTargetInfo(FoundGraphNode, Ctx->TargetOwnerClass, NewRef.TargetObjectName, NewRef.TargetClassName);
                    }
                    else
                    {
                        NewRef.bIsSetter = FoundGraphNode->IsA<UK2Node_VariableSet>();
                    }

                    if (Ctx->SourceNode != nullptr && FoundGraphNode == Ctx->SourceNode)
                    {
                        NewRef.bIsSource = true;
                    }


                    TargetNode->References.Add(NewRef);
                });

            Searcher->OnBatchComplete.BindLambda([Ctx, ArrangeVisualRefNodes, RebuildReferenceLinks]()
                {
                    if (TSharedPtr<SGraphEditor> PinnedEditor = Ctx->WeakEditor.Pin())
                    {

                        if (Ctx->bIsFunction || Ctx->bIsDispatcher)
                        {
                            for (UEdGraphNode* Node : Ctx->Graph->Nodes)
                            {
                                if (UVisualRefNode* RefNode = Cast<UVisualRefNode>(Node))
                                {

                                    RefNode->References.Sort([](const FVisualReferenceInfo& A, const FVisualReferenceInfo& B)
                                        {

                                            if (A.GraphName != B.GraphName)
                                            {
                                                return A.GraphName < B.GraphName;
                                            }

                                            return A.CallOrder < B.CallOrder;
                                        });


                                    TMap<FString, int32> GraphOrderMap;
                                    for (FVisualReferenceInfo& Ref : RefNode->References)
                                    {
                                        int32& CurrentOrder = GraphOrderMap.FindOrAdd(Ref.GraphName, 0);
                                        Ref.CallOrder = CurrentOrder;
                                        CurrentOrder++;
                                    }
                                }
                            }
                        }
                        else
                        {

                            for (UEdGraphNode* Node : Ctx->Graph->Nodes)
                            {
                                if (UVisualRefNode* RefNode = Cast<UVisualRefNode>(Node))
                                {
                                    RefNode->References.Sort([](const FVisualReferenceInfo& A, const FVisualReferenceInfo& B)
                                        {

                                            if (A.GraphName != B.GraphName)
                                            {
                                                return A.GraphName < B.GraphName;
                                            }

                                            if (A.bIsSetter != B.bIsSetter)
                                            {
                                                return A.bIsSetter > B.bIsSetter;
                                            }

                                            return A.NodeName < B.NodeName;
                                        });
                                }
                            }
                        }

                        ArrangeVisualRefNodes();
                        RebuildReferenceLinks();
                        PinnedEditor->NotifyGraphChanged();
                    }
                });

            Searcher->OnSearchFinished.BindLambda([Ctx, Searcher]()
                {
                    if (TSharedPtr<SWidget> LoadingWidget = Ctx->WeakLoadingWidget.Pin())
                    {
                        LoadingWidget->SetVisibility(EVisibility::Collapsed);
                    }

                    Searcher->OnRefFound.Unbind();
                    Searcher->OnBatchComplete.Unbind();
                    Searcher->OnSearchFinished.Unbind();
                });

            Searcher->SetSearchScope(Ctx->SearchScope);
            Searcher->SetExhaustiveSearchEnabled(Ctx->bFullScan);
            Searcher->StartSearch();
        };

    TSharedPtr<SWidget> LoadingIndicator;
    TSharedRef<bool> bSettingsPanelVisible = MakeShared<bool>(true);
    TSharedRef<bool> bDetailsPanelVisible = MakeShared<bool>(true);
    TSharedPtr<SProgressBar> ProgressBarWidget;
    TSharedPtr<STextBlock> ProgressTextWidget;

    TSharedRef<SWidget> HeaderBar = SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
        .BorderBackgroundColor(FLinearColor(0.035f, 0.038f, 0.043f, 0.98f))
        .Padding(FMargin(12.0f, 7.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(LOCTEXT("ViewerHeader", "Visual Reference Viewer"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                    .ColorAndOpacity(FLinearColor(0.92f, 0.95f, 1.0f))
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
            [
                SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ContentPadding(FMargin(10.0f, 4.0f))
                    .ToolTipText(LOCTEXT("RefreshTooltip", "Refresh Search Results"))
                    .OnClicked_Lambda([PerformSearch]() { PerformSearch(); return FReply::Handled(); })
                    [ SNew(STextBlock).Text(LOCTEXT("RefreshButton", "Refresh")) ]
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ContentPadding(FMargin(10.0f, 4.0f))
                    .ToolTipText(LOCTEXT("ExportTooltip", "Export the current report to Project/Saved/TraceMotiveReports"))
                    .OnClicked_Lambda([Ctx]()
                        {
                            FString FilePath;
                            TMVisualRefReport::SaveReport(Ctx->Graph, Ctx->Name.ToString(), Ctx->bIsFunction, Ctx->bIsDispatcher,
                                Ctx->SearchScope, Ctx->ExportFormat, FilePath);
                            return FReply::Handled();
                        })
                    [ SNew(STextBlock).Text(LOCTEXT("ExportButton", "Export")) ]
            ]
        ];

    TSharedRef<SWidget> SettingsPanel = SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
        .BorderBackgroundColor(FLinearColor(0.028f, 0.031f, 0.036f, 0.98f))
        .Padding(8.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
                    [ SNew(STextBlock).Text(LOCTEXT("SearchSection", "SEARCH")).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(0.48f, 0.65f, 0.86f)) ]
                    + SVerticalBox::Slot().AutoHeight()
                    [ SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).Padding(FMargin(7.0f, 5.0f))[SNew(STextBlock).Text(FText::FromName(InName)).ColorAndOpacity(FLinearColor(0.78f, 0.82f, 0.9f))] ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 2.0f)
                    [
                        SNew(SComboBox<TSharedPtr<FString>>)
                            .OptionsSource(&ScopeOptions)
                            .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(FText::FromString(*Item)); })
                            .OnSelectionChanged_Lambda([Ctx](TSharedPtr<FString> Selection, ESelectInfo::Type)
                                {
                                    if (Selection.IsValid())
                                    {
                                        Ctx->SearchScope = *Selection == "Current Blueprint" ? EVisualRefSearchScope::CurrentBlueprint
                                            : (*Selection == "Same Folder" ? EVisualRefSearchScope::SameFolder : EVisualRefSearchScope::ProjectContent);
                                    }
                                })
                            [ SNew(STextBlock).Text_Lambda([Ctx]() { return FText::FromString(Ctx->SearchScope == EVisualRefSearchScope::CurrentBlueprint ? "Current Blueprint" : (Ctx->SearchScope == EVisualRefSearchScope::SameFolder ? "Same Folder" : "Project Content")); }) ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                    [
                        SNew(SCheckBox)
                            .IsChecked_Lambda([Ctx]() { return Ctx->bFullScan ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                            .OnCheckStateChanged_Lambda([Ctx](ECheckBoxState State) { Ctx->bFullScan = State == ECheckBoxState::Checked; })
                            [ SNew(STextBlock).Text(LOCTEXT("FullScanLabel", "Full scan")) ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
                    [ SNew(STextBlock).Text(LOCTEXT("ViewSection", "VIEW")).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(0.48f, 0.65f, 0.86f)) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
                    [
                        SNew(SComboBox<TSharedPtr<FString>>)
                            .OptionsSource(&ViewModeOptions)
                            .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(FText::FromString(*Item)); })
                            .OnSelectionChanged_Lambda([GraphEditor](TSharedPtr<FString> Selection, ESelectInfo::Type)
                                {
                                    if (Selection.IsValid())
                                    {
                                        SVisualRefNode::CurrentViewMode = (*Selection == "List View") ? EVisualRefViewMode::List : EVisualRefViewMode::Tree;
                                        GraphEditor->NotifyGraphChanged();
                                    }
                                })
                            [ SNew(STextBlock).Text_Lambda([]() { return FText::FromString(SVisualRefNode::CurrentViewMode == EVisualRefViewMode::List ? "List View" : "Tree View"); }) ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                    [
                        SNew(SComboBox<TSharedPtr<FString>>)
                            .OptionsSource(&LayoutModeOptions)
                            .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(FText::FromString(*Item)); })
                            .OnSelectionChanged_Lambda([Ctx, ArrangeVisualRefNodes](TSharedPtr<FString> Selection, ESelectInfo::Type)
                                {
                                    if (Selection.IsValid()) { Ctx->LayoutMode = *Selection == "Row Layout" ? EVisualRefLayoutMode::Row : EVisualRefLayoutMode::FrameGrid; ArrangeVisualRefNodes(); }
                                })
                            [ SNew(STextBlock).Text_Lambda([Ctx]() { return FText::FromString(Ctx->LayoutMode == EVisualRefLayoutMode::Row ? "Row Layout" : "Frame Grid"); }) ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                    [
                        SNew(SCheckBox)
                            .IsChecked_Lambda([]() { return SVisualRefNode::bAllowNodeMovement ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                            .OnCheckStateChanged_Lambda([GraphEditor](ECheckBoxState State) { SVisualRefNode::bAllowNodeMovement = State == ECheckBoxState::Checked; GraphEditor->NotifyGraphChanged(); })
                            [ SNew(STextBlock).Text(LOCTEXT("MoveNodes", "Move nodes")) ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                    [
                        SNew(SCheckBox)
                            .IsChecked_Lambda([Ctx]() { return Ctx->bShowReferenceLines ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                            .OnCheckStateChanged_Lambda([Ctx, RebuildReferenceLinks](ECheckBoxState State) { Ctx->bShowReferenceLines = State == ECheckBoxState::Checked; RebuildReferenceLinks(); })
                            [ SNew(STextBlock).Text(LOCTEXT("ShowLines", "Show lines")) ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
                    [ SNew(STextBlock).Text(LOCTEXT("FilterSection", "FILTERS")).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(0.48f, 0.65f, 0.86f)) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                    [
                        SNew(SCheckBox).Visibility((bIsFunction || bIsDispatcher) ? EVisibility::Collapsed : EVisibility::Visible)
                            .IsChecked_Lambda([]() { return SVisualRefNode::bShowGet ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                            .OnCheckStateChanged_Lambda([GraphEditor](ECheckBoxState State) { SVisualRefNode::bShowGet = State == ECheckBoxState::Checked; GraphEditor->NotifyGraphChanged(); })
                            [ SNew(STextBlock).Text(LOCTEXT("ShowGetLabel", "Show Get")) ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                    [
                        SNew(SCheckBox).Visibility((bIsFunction || bIsDispatcher) ? EVisibility::Collapsed : EVisibility::Visible)
                            .IsChecked_Lambda([]() { return SVisualRefNode::bShowSet ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                            .OnCheckStateChanged_Lambda([GraphEditor](ECheckBoxState State) { SVisualRefNode::bShowSet = State == ECheckBoxState::Checked; GraphEditor->NotifyGraphChanged(); })
                            [ SNew(STextBlock).Text(LOCTEXT("ShowSetLabel", "Show Set")) ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                    [
                        SNew(SCheckBox).Visibility((bIsFunction || bIsDispatcher) ? EVisibility::Visible : EVisibility::Collapsed)
                            .IsChecked_Lambda([]() { return SVisualRefNode::bShowCalls ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                            .OnCheckStateChanged_Lambda([GraphEditor](ECheckBoxState State) { SVisualRefNode::bShowCalls = State == ECheckBoxState::Checked; GraphEditor->NotifyGraphChanged(); })
                            [ SNew(STextBlock).Text(LOCTEXT("ShowCalls", "Show calls")) ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
                    [
                        SNew(SCheckBox).Visibility((bIsFunction || bIsDispatcher) ? EVisibility::Visible : EVisibility::Collapsed)
                            .IsChecked_Lambda([]() { return SVisualRefNode::bShowDefinitions ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                            .OnCheckStateChanged_Lambda([GraphEditor](ECheckBoxState State) { SVisualRefNode::bShowDefinitions = State == ECheckBoxState::Checked; GraphEditor->NotifyGraphChanged(); })
                            [ SNew(STextBlock).Text(LOCTEXT("ShowDefinitions", "Show definitions")) ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SButton).Visibility(bIsFunction ? EVisibility::Visible : EVisibility::Collapsed)
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .ContentPadding(FMargin(7.0f, 5.0f))
                        .OnClicked_Lambda([Ctx]()
                            {
                                if (Ctx->bIsFunction && Ctx->Blueprint)
                                {
                                    TSharedPtr<FunctionCallChainTracer> Tracer = MakeShareable(new FunctionCallChainTracer(Ctx->Blueprint, Ctx->Name, Ctx->TargetOwnerClass));
                                    Tracer->OnTraceComplete.BindLambda([](const FCallChainResult& Result)
                                        {
                                            TSharedRef<SWindow> ChainWindow = SNew(SWindow).Title(FText::FromString(FString::Printf(TEXT("Call Chain - %s"), *Result.TargetFunctionName.ToString()))).ClientSize(FVector2D(800, 600));
                                            ChainWindow->SetContent(SNew(SCallChainViewer, Result));
                                            FSlateApplication::Get().AddWindow(ChainWindow);
                                        });
                                    Tracer->StartTrace();
                                }
                                return FReply::Handled();
                            })
                        [ SNew(STextBlock).Text(LOCTEXT("ShowCallChainButton", "Show call chain")) ]
                ]
            ]
        ];

    TSharedRef<SWidget> DetailsPanel = SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
        .BorderBackgroundColor(FLinearColor(0.028f, 0.031f, 0.036f, 0.98f))
        .Padding(10.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 7.0f)
            [ SNew(STextBlock).Text(LOCTEXT("NodeDetails", "NODE DETAILS")).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(0.48f, 0.65f, 0.86f)) ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("%s\n%s\nSelect a graph node to inspect its references."), *RootAssetName, bIsDispatcher ? TEXT("Event Dispatcher") : (bIsFunction ? TEXT("Function") : TEXT("Variable")))))
                    .AutoWrapText(true)
                    .ColorAndOpacity(FLinearColor(0.82f, 0.86f, 0.93f))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 6.0f)
            [ SNew(STextBlock).Text(LOCTEXT("NotesLabel", "NOTES")).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(0.48f, 0.65f, 0.86f)) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(STextBlock).Text(LOCTEXT("NotesHelp", "Use the Notes toolbar to draw, erase, save, and clear graph annotations.")).AutoWrapText(true).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(0.65f, 0.7f, 0.77f)) ]
        ];

    const FString AnnotationPersistenceKey = FString::Printf(TEXT("VisualReference|%s|%s|%s"),
        InBlueprint ? *InBlueprint->GetPathName() : TEXT("Unknown"), *InName.ToString(), bIsDispatcher ? TEXT("Dispatcher") : (bIsFunction ? TEXT("Function") : TEXT("Variable")));
    TSharedRef<SWidget> AnnotatedGraphEditor = SNew(SGraphAnnotationOverlay)
        .GraphEditor(GraphEditor)
        .PersistenceKey(AnnotationPersistenceKey)
        .ToolbarPadding(FMargin(10.0f, 10.0f, 10.0f, 10.0f))
        [
            GraphEditor
        ];

    TSharedRef<SOverlay> MainOverlay = SNew(SOverlay)
        + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [ HeaderBar ]
            + SVerticalBox::Slot().FillHeight(1.0f).Padding(3.0f, 8.0f, 3.0f, 3.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [ SNew(SBox).WidthOverride(220.0f).Visibility_Lambda([bSettingsPanelVisible]() { return *bSettingsPanelVisible ? EVisibility::Visible : EVisibility::Collapsed; })[SettingsPanel] ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
                [
                    SNew(SBox).WidthOverride(32.0f).HeightOverride(40.0f)
                    [
                        SNew(SButton)
                            .ButtonStyle(FAppStyle::Get(), "Button")
                            .ContentPadding(6.0f)
                            .ToolTipText(LOCTEXT("ToggleSettingsSidebarTooltip", "Hide or show the Settings sidebar"))
                            .OnClicked_Lambda([bSettingsPanelVisible]() { *bSettingsPanelVisible = !*bSettingsPanelVisible; return FReply::Handled(); })
                            [
                                SNew(SImage)
                                    .Image_Lambda([bSettingsPanelVisible]() -> const FSlateBrush*
                                        { return FAppStyle::GetBrush(*bSettingsPanelVisible ? "Icons.ChevronLeft" : "Icons.ChevronRight"); })
                                    .ColorAndOpacity(FLinearColor(0.72f, 0.82f, 0.95f))
                            ]
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f)
                [ SNew(SBorder).BorderImage(FAppStyle::GetBrush("Brushes.Panel"))[AnnotatedGraphEditor] ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
                [
                    SNew(SBox).WidthOverride(32.0f).HeightOverride(40.0f)
                    [
                        SNew(SButton)
                            .ButtonStyle(FAppStyle::Get(), "Button")
                            .ContentPadding(6.0f)
                            .ToolTipText(LOCTEXT("ToggleDetailsSidebarTooltip", "Hide or show the Details sidebar"))
                            .OnClicked_Lambda([bDetailsPanelVisible]() { *bDetailsPanelVisible = !*bDetailsPanelVisible; return FReply::Handled(); })
                            [
                                SNew(SImage)
                                    .Image_Lambda([bDetailsPanelVisible]() -> const FSlateBrush*
                                        { return FAppStyle::GetBrush(*bDetailsPanelVisible ? "Icons.ChevronRight" : "Icons.ChevronLeft"); })
                                    .ColorAndOpacity(FLinearColor(0.72f, 0.82f, 0.95f))
                            ]
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [ SNew(SBox).WidthOverride(220.0f).Visibility_Lambda([bDetailsPanelVisible]() { return *bDetailsPanelVisible ? EVisibility::Visible : EVisibility::Collapsed; })[DetailsPanel] ]
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(20.0f)
        [
            SAssignNew(LoadingIndicator, SBorder)
                .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
                .BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.7f))
                .Padding(10.0f)
                .Visibility(EVisibility::Collapsed)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [ SNew(SThrobber).NumPieces(5) ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(10, 0, 0, 0).VAlign(VAlign_Center)
                        [ SAssignNew(ProgressTextWidget, STextBlock).Text(LOCTEXT("SearchingAssetsText", "Searching Assets...")).ColorAndOpacity(FLinearColor::White).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)) ]
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [ SNew(SBox).WidthOverride(200.0f).HeightOverride(10.0f)[SAssignNew(ProgressBarWidget, SProgressBar).Percent(0.0f).FillColorAndOpacity(FLinearColor(0.0f, 0.5f, 1.0f))] ]
                ]
        ];

    Ctx->WeakLoadingWidget = LoadingIndicator;
    Ctx->WeakProgressBar = ProgressBarWidget;
    Ctx->WeakProgressText = ProgressTextWidget;

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("WindowTitle", "Visual Variable/Function References"))
        .ClientSize(FVector2D(1280, 800));

    Window->SetContent(MainOverlay);
    Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([MyGraph](const TSharedRef<SWindow>&)
        {
            if (MyGraph) MyGraph->RemoveFromRoot();
        }));

    FSlateApplication::Get().AddWindow(Window);

    PerformSearch();
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FTraceMotiveModule, TraceMotive)
