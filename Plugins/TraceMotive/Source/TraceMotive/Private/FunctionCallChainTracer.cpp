// Source/TraceMotive/Private/FunctionCallChainTracer.cpp

#include "FunctionCallChainTracer.h"

#include "TMPerformanceGuard.h"

#include "AssetRegistry/AssetRegistryModule.h"

#include "EdGraph/EdGraph.h"

#include "EdGraphSchema_K2.h"

#include "Engine/Blueprint.h"

#include "Engine/World.h"

#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"

#include "K2Node_CallFunction.h"

#include "K2Node_CreateDelegate.h"

#include "K2Node_CustomEvent.h"

#include "K2Node_Event.h"

#include "K2Node_FunctionEntry.h"

#include "K2Node_MacroInstance.h"

#include "K2Node_Variable.h"

#include "Kismet2/BlueprintEditorUtils.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#include "Modules/ModuleManager.h"

#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogCallChainTracer, Log, All);

namespace

{

    constexpr int32 MaxCppSourceCallChainMatches = 300;

    UClass* GetBestBlueprintClass(const UBlueprint* Blueprint)

    {

        if (!Blueprint)

        {

            return nullptr;

        }

        return Blueprint->GeneratedClass ? Blueprint->GeneratedClass : Blueprint->SkeletonGeneratedClass;

    }

    void AddGraphAndChildren(UEdGraph* Graph, TArray<UEdGraph*>& OutGraphs)

    {

        if (!Graph || OutGraphs.Contains(Graph))

        {

            return;

        }

        OutGraphs.Add(Graph);

        TArray<UEdGraph*> ChildGraphs;

        Graph->GetAllChildrenGraphs(ChildGraphs);

        for (UEdGraph* ChildGraph : ChildGraphs)

        {

            AddGraphAndChildren(ChildGraph, OutGraphs);

        }

    }

    void GetBlueprintGraphs(UBlueprint* Blueprint, TArray<UEdGraph*>& OutGraphs)

    {

        if (!Blueprint)

        {

            return;

        }

        for (UEdGraph* Graph : Blueprint->UbergraphPages)

        {

            AddGraphAndChildren(Graph, OutGraphs);

        }

        for (UEdGraph* Graph : Blueprint->FunctionGraphs)

        {

            AddGraphAndChildren(Graph, OutGraphs);

        }

        for (UEdGraph* Graph : Blueprint->MacroGraphs)

        {

            AddGraphAndChildren(Graph, OutGraphs);

        }

    }

    bool IsTraceablePackageName(FName PackageName)

    {

        const FString PackagePath = PackageName.ToString();

        if (!PackagePath.StartsWith(TEXT("/"))) return false;

        return !PackagePath.StartsWith(TEXT("/Engine"))

            && !PackagePath.StartsWith(TEXT("/Script"))

            && !PackagePath.StartsWith(TEXT("/Memory"))

            && !PackagePath.StartsWith(TEXT("/Temp"))

            && !PackagePath.StartsWith(TEXT("/Transient"));

    }

    FString NormalizeBlueprintClassName(FString ClassName)

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

    void AddCandidateNeedle(TArray<FString>& OutNeedles, const FString& Needle)

    {

        if (!Needle.IsEmpty())

        {

            OutNeedles.AddUnique(Needle);

        }

    }

    bool ContainsAnyCandidateNeedle(const FString& Haystack, const TArray<FString>& Needles)

    {

        if (Haystack.IsEmpty())

        {

            return false;

        }

        for (const FString& Needle : Needles)

        {

            if (!Needle.IsEmpty() && Haystack.Contains(Needle, ESearchCase::IgnoreCase))

            {

                return true;

            }

        }

        return false;

    }

    bool DoesBlueprintAssetMentionTraceTarget(const FAssetData& AssetData, FName TargetFunctionName, UBlueprint* TargetBP, UClass* TargetOwnerClass)

    {

        TArray<FString> Needles;

        AddCandidateNeedle(Needles, TargetFunctionName.ToString());

        if (TargetOwnerClass)

        {

            AddCandidateNeedle(Needles, NormalizeBlueprintClassName(TargetOwnerClass->GetName()));

            AddCandidateNeedle(Needles, TargetOwnerClass->GetName());

        }

        if (TargetBP)

        {

            AddCandidateNeedle(Needles, TargetBP->GetName());

            if (UClass* GeneratedClass = GetBestBlueprintClass(TargetBP))

            {

                AddCandidateNeedle(Needles, NormalizeBlueprintClassName(GeneratedClass->GetName()));

                AddCandidateNeedle(Needles, GeneratedClass->GetName());

            }

        }

        if (ContainsAnyCandidateNeedle(AssetData.AssetName.ToString(), Needles)

            || ContainsAnyCandidateNeedle(AssetData.PackageName.ToString(), Needles))

        {

            return true;

        }

        static const FName CandidateTagNames[] =

        {

            TEXT("ParentClass"),

            TEXT("GeneratedClass"),

            TEXT("SkeletonClass"),

            TEXT("NativeParentClass"),

            TEXT("ImplementedInterfaces"),

            TEXT("FunctionGraphs"),

            TEXT("UbergraphPages"),

            TEXT("BlueprintDescription"),

            TEXT("Tags")

        };

        for (FName TagName : CandidateTagNames)

        {

            FString TagValue;

            if (AssetData.GetTagValue(TagName, TagValue)

                && ContainsAnyCandidateNeedle(TagValue, Needles))

            {

                return true;

            }

        }

        return false;

    }

    bool IsClassCompatibleWithTarget(UClass* TestClass, UBlueprint* TargetBP, UBlueprint* CurrentBP, UClass* TargetOwnerClass)

    {

        if (TargetOwnerClass && TestClass == TargetOwnerClass) return true;

        if (TargetOwnerClass && TestClass && TestClass->IsChildOf(TargetOwnerClass)) return true;

        if (TargetOwnerClass && TestClass && TargetOwnerClass->IsChildOf(TestClass)) return true;

        if (TargetOwnerClass && TestClass && TestClass->IsChildOf(UInterface::StaticClass()) && TargetOwnerClass->ImplementsInterface(TestClass)) return true;

        if (TargetOwnerClass && TestClass)

        {

            FString TestClassName = NormalizeBlueprintClassName(TestClass->GetName());

            FString OwnerClassName = NormalizeBlueprintClassName(TargetOwnerClass->GetName());

            if (!OwnerClassName.IsEmpty() && TestClassName == OwnerClassName) return true;

        }

        if (!TargetBP)

        {

            return false;

        }

        if (!TestClass)

        {

            return CurrentBP == TargetBP;

        }

        UClass* TargetGenClass = TargetBP->GeneratedClass;

        UClass* TargetSkelClass = TargetBP->SkeletonGeneratedClass;

        if (TargetGenClass && TestClass == TargetGenClass) return true;

        if (TargetSkelClass && TestClass == TargetSkelClass) return true;

        if (TargetGenClass)

        {

            if (TestClass->IsChildOf(TargetGenClass)) return true;

            if (TargetGenClass->IsChildOf(TestClass)) return true;

        }

        if (TargetSkelClass)

        {

            if (TestClass->IsChildOf(TargetSkelClass)) return true;

            if (TargetSkelClass->IsChildOf(TestClass)) return true;

        }

        if (TestClass->IsChildOf(UInterface::StaticClass()))

        {

            if (TargetGenClass && TargetGenClass->ImplementsInterface(TestClass)) return true;

            if (TargetSkelClass && TargetSkelClass->ImplementsInterface(TestClass)) return true;

        }

        UPackage* TargetPackage = TargetBP->GetOutermost();

        UPackage* TestPackage = TestClass->GetOutermost();

        if (TargetPackage && TestPackage && TargetPackage == TestPackage) return true;

        const FString TestClassName = NormalizeBlueprintClassName(TestClass->GetName());

        const FString TargetGenName = TargetGenClass ? NormalizeBlueprintClassName(TargetGenClass->GetName()) : FString();

        const FString TargetSkelName = TargetSkelClass ? NormalizeBlueprintClassName(TargetSkelClass->GetName()) : FString();

        if (!TargetGenName.IsEmpty() && TestClassName == TargetGenName) return true;

        if (!TargetSkelName.IsEmpty() && TestClassName == TargetSkelName) return true;

        return false;

    }

    void AddReferencedFunctionNamesInternal(UEdGraphNode* Node, TArray<FName>& OutNames, TSet<UEdGraph*>& VisitedMacroGraphs)

    {

        if (!Node)

        {

            return;

        }

        if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))

        {

            OutNames.AddUnique(CallNode->FunctionReference.GetMemberName());

        }

        else if (UK2Node_CreateDelegate* DelegateNode = Cast<UK2Node_CreateDelegate>(Node))

        {

            OutNames.AddUnique(DelegateNode->GetFunctionName());

        }

        else if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))

        {

            if (UEdGraph* MacroGraph = MacroNode->GetMacroGraph())

            {

                if (VisitedMacroGraphs.Contains(MacroGraph))

                {

                    return;

                }

                VisitedMacroGraphs.Add(MacroGraph);

                for (UEdGraphNode* InnerNode : MacroGraph->Nodes)

                {

                    AddReferencedFunctionNamesInternal(InnerNode, OutNames, VisitedMacroGraphs);

                }

            }

        }

    }

    void AddReferencedFunctionNames(UEdGraphNode* Node, TArray<FName>& OutNames)

    {

        TSet<UEdGraph*> VisitedMacroGraphs;

        AddReferencedFunctionNamesInternal(Node, OutNames, VisitedMacroGraphs);

    }

    bool IsExecutionPin(UEdGraphPin* Pin, EEdGraphPinDirection Direction)

    {

        return Pin

            && Pin->Direction == Direction

            && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;

    }

    bool IsRootExecutionGraph(UEdGraph* Graph, UBlueprint* Blueprint)

    {

        if (!Graph) return false;

        const FString GraphName = Graph->GetName();

        if (GraphName == TEXT("EventGraph") || GraphName.StartsWith(TEXT("ExecuteUbergraph_"))) return true;

        if (Graph->GetFName() == TEXT("ReceiveBeginPlay") || Graph->GetFName() == TEXT("UserConstructionScript")) return true;

        return Blueprint && Blueprint->UbergraphPages.Contains(Graph);

    }

    FString GetClassDisplayName(UClass* Class)

    {

        if (!Class) return FString();

        if (UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy))

        {

            return Blueprint->GetName();

        }

        FString ClassName = NormalizeBlueprintClassName(Class->GetName());

        ClassName.RemoveFromStart(TEXT("A"));

        ClassName.RemoveFromStart(TEXT("U"));

        return ClassName;

    }

    UClass* GetPinClass(UEdGraphPin* Pin)

    {

        if (!Pin || !Pin->PinType.PinSubCategoryObject.IsValid())

        {

            return nullptr;

        }

        return Cast<UClass>(Pin->PinType.PinSubCategoryObject.Get());

    }

    FString GetReadableVariableNodeName(UK2Node_Variable* VariableNode)

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

    void FillCallTargetInfo(UK2Node_CallFunction* CallNode, UBlueprint* BP, UClass* DefaultOwnerClass, FCallChainNode& ChainNode)

    {

        if (!CallNode)

        {

            return;

        }

        UClass* TargetClass = nullptr;

        UEdGraphPin* SelfPin = CallNode->FindPin(UEdGraphSchema_K2::PN_Self);

        if (SelfPin)

        {

            TargetClass = GetPinClass(SelfPin);

            for (UEdGraphPin* LinkedPin : SelfPin->LinkedTo)

            {

                if (!LinkedPin)

                {

                    continue;

                }

                if (UClass* LinkedClass = GetPinClass(LinkedPin))

                {

                    TargetClass = LinkedClass;

                }

                UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();

                if (UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(LinkedNode))

                {

                    ChainNode.TargetObjectName = GetReadableVariableNodeName(VariableNode);

                }

                else if (LinkedNode)

                {

                    ChainNode.TargetObjectName = LinkedNode->GetNodeTitle(ENodeTitleType::ListView).ToString();

                    ChainNode.TargetObjectName.RemoveFromStart(TEXT("Get "));

                }

                if (!ChainNode.TargetObjectName.IsEmpty() || TargetClass)

                {

                    break;

                }

            }

            if (SelfPin->LinkedTo.Num() == 0 && BP)

            {

                ChainNode.TargetObjectName = TEXT("Self");

                if (!TargetClass)

                {

                    TargetClass = GetBestBlueprintClass(BP);

                }

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

            ChainNode.TargetClassName = GetClassDisplayName(TargetClass);

        }

    }

}

// =====================================================

    bool IsLikelyCppSourceFile(const FString& FilePath)
    {
        return FilePath.EndsWith(TEXT(".cpp"), ESearchCase::IgnoreCase)
            || FilePath.EndsWith(TEXT(".cc"), ESearchCase::IgnoreCase)
            || FilePath.EndsWith(TEXT(".cxx"), ESearchCase::IgnoreCase)
            || FilePath.EndsWith(TEXT(".h"), ESearchCase::IgnoreCase)
            || FilePath.EndsWith(TEXT(".hpp"), ESearchCase::IgnoreCase)
            || FilePath.EndsWith(TEXT(".inl"), ESearchCase::IgnoreCase);
    }

    bool IsWordBoundaryForFunctionName(const FString& Text, int32 Index)
    {
        if (!Text.IsValidIndex(Index)) return true;
        const TCHAR Ch = Text[Index];
        return !(FChar::IsAlnum(Ch) || Ch == TEXT('_'));
    }

    bool LooksLikeCppCallSite(const FString& Line, const FString& FunctionName, int32& OutColumn)
    {
        OutColumn = INDEX_NONE;
        if (FunctionName.IsEmpty()) return false;

        FString Trimmed = Line;
        Trimmed.TrimStartAndEndInline();
        if (Trimmed.IsEmpty()) return false;
        if (Trimmed.StartsWith(TEXT("//")) || Trimmed.StartsWith(TEXT("*")) || Trimmed.StartsWith(TEXT("/*"))) return false;
        if (Trimmed.StartsWith(TEXT("DECLARE_")) || Trimmed.StartsWith(TEXT("DEFINE_LOG_CATEGORY"))) return false;
        if (Trimmed.Contains(TEXT("UFUNCTION(")) || Trimmed.Contains(TEXT("UPROPERTY("))) return false;

        int32 SearchFrom = 0;
        while (true)
        {
            int32 FoundIndex = INDEX_NONE;
            if (!Line.FindChar(FunctionName[0], FoundIndex))
            {
                return false;
            }
            FoundIndex = Line.Find(FunctionName, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
            if (FoundIndex == INDEX_NONE) return false;

            const int32 BeforeIndex = FoundIndex - 1;
            const int32 AfterNameIndex = FoundIndex + FunctionName.Len();
            if (!IsWordBoundaryForFunctionName(Line, BeforeIndex) || !IsWordBoundaryForFunctionName(Line, AfterNameIndex))
            {
                SearchFrom = AfterNameIndex;
                continue;
            }

            int32 Cursor = AfterNameIndex;
            while (Line.IsValidIndex(Cursor) && FChar::IsWhitespace(Line[Cursor])) ++Cursor;
            if (!Line.IsValidIndex(Cursor) || Line[Cursor] != TEXT('('))
            {
                SearchFrom = AfterNameIndex;
                continue;
            }

            const FString Prefix = Line.Left(FoundIndex);
            FString TrimmedPrefix = Prefix;
            TrimmedPrefix.TrimStartAndEndInline();
            const bool bLineLooksLikeStatement = Trimmed.EndsWith(TEXT(";"))
                || Trimmed.Contains(TEXT("="))
                || Trimmed.StartsWith(TEXT("return"))
                || Trimmed.StartsWith(TEXT("if"))
                || Trimmed.StartsWith(TEXT("while"))
                || Trimmed.StartsWith(TEXT("for"))
                || Trimmed.StartsWith(TEXT("else if"));

            if (Prefix.Contains(TEXT("::")) && !bLineLooksLikeStatement)
            {
                // Common definition form: ReturnType Class::Function(...)
                SearchFrom = AfterNameIndex;
                continue;
            }

            if (!TrimmedPrefix.IsEmpty()
                && TrimmedPrefix.Contains(TEXT(" "))
                && !bLineLooksLikeStatement)
            {
                // Common declaration/definition form: ReturnType Function(...)
                SearchFrom = AfterNameIndex;
                continue;
            }

            if (Trimmed.StartsWith(FunctionName + TEXT("(")) && !bLineLooksLikeStatement)
            {
                SearchFrom = AfterNameIndex;
                continue;
            }

            OutColumn = FoundIndex + 1;
            return true;
        }
    }

    FString MakeDisplaySourcePath(const FString& AbsolutePath)
    {
        FString Result = AbsolutePath;
        FPaths::MakePathRelativeTo(Result, *FPaths::ProjectDir());
        return Result;
    }

FunctionCallChainTracer::FunctionCallChainTracer(UBlueprint* InTargetBP, FName InTargetFunction, UClass* InTargetOwnerClass)

    : TargetBlueprint(InTargetBP)

    , TargetFunctionOwnerClass(InTargetOwnerClass)

    , TargetFunctionName(InTargetFunction)

    , bIsTracing(false)

    , ProcessedCount(0)

    , TotalCount(0)

    , CurrentState(ETraceState::Idle)

    , LoadedPackageIndex(0)

{

    if (!TargetFunctionOwnerClass.IsValid() && TargetBlueprint.IsValid())

    {

        TargetFunctionOwnerClass = TargetBlueprint->GeneratedClass ? TargetBlueprint->GeneratedClass : TargetBlueprint->SkeletonGeneratedClass;

    }

    if (TargetBlueprint.IsValid())

    {

        for (UEdGraph* Graph : TargetBlueprint->FunctionGraphs)

        {

            if (Graph && Graph->GetFName() == TargetFunctionName)

            {

                TargetFunctionGuid = Graph->GraphGuid;

                break;

            }

        }

        if (!TargetFunctionGuid.IsValid())

        {

            TArray<UEdGraph*> Graphs;

            GetBlueprintGraphs(TargetBlueprint.Get(), Graphs);

            for (UEdGraph* Graph : Graphs)

            {

                if (!Graph) continue;

                for (UEdGraphNode* Node : Graph->Nodes)

                {

                    if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))

                    {

                        if (CustomEvent->GetFunctionName() == TargetFunctionName)

                        {

                            TargetFunctionGuid = CustomEvent->NodeGuid;

                            break;

                        }

                    }

                }

                if (TargetFunctionGuid.IsValid()) break;

            }

        }

    }

    Result.TargetFunctionName = InTargetFunction;

    Result.TargetBlueprint = InTargetBP;

}

FunctionCallChainTracer::~FunctionCallChainTracer()

{

    if (TickerHandle.IsValid())

    {

        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

    }

}

void FunctionCallChainTracer::StartTrace()

{

    if (!TargetBlueprint.IsValid())

    {

        OnTraceComplete.ExecuteIfBound(Result);

        return;

    }

    UE_LOG(LogCallChainTracer, Display, TEXT(">>> START TRACE: [%s] :: %s <<<"), *TargetBlueprint->GetName(), *TargetFunctionName.ToString());

    if (TickerHandle.IsValid())

    {

        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

        TickerHandle.Reset();

    }

    SelfReference = AsShared();

    bIsTracing = true;
    TraceSession.Begin();

    ProcessQueue.Empty();

    Result.Paths.Empty();

    Result.ScannedPackageCount = 0;

    Result.DirectCallerCount = 0;

    Result.TruncatedPathCount = 0;

    Result.CyclicPathCount = 0;

    Result.CppSourceCallerCount = 0;

    CppSourceFilesToScan.Empty();

    CppSourceFileIndex = 0;

    AddedCppSourceCallKeys.Empty();

    InitialCallers.Empty();

    PackagesToScan.Empty();

    CachedCallerMap.Empty();

    CppSourceFilesToScan.Empty();

    AddedCppSourceCallKeys.Empty();

    ReleaseScanObjectReferences();

    LoadedPackageIndex = 0;

    ProcessedCount = 0;

    TotalCount = 0;

    CurrentState = ETraceState::FindingReferencers;

    bFindingReferencersLogged = false;

    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(

        FTickerDelegate::CreateSP(this, &FunctionCallChainTracer::Tick),

        TMPerf::CallChainTickerIntervalSeconds());

}

void FunctionCallChainTracer::CancelTrace()

{

    bIsTracing = false;
    TraceSession.Cancel();

    if (TickerHandle.IsValid())

    {

        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

        TickerHandle.Reset();

    }

    ProcessQueue.Empty();

    InitialCallers.Empty();

    PackagesToScan.Empty();

    CachedCallerMap.Empty();

    ReleaseScanObjectReferences();

    OnTraceProgress.Unbind();

    OnTraceComplete.Unbind();

    SelfReference.Reset();

}

bool FunctionCallChainTracer::Tick(float DeltaTime)

{

    if (!bIsTracing || !TraceSession.IsActive()) return false;

    double StartTime = FPlatformTime::Seconds();

    double TimeLimit = TMPerf::CallChainTickBudgetSeconds(); // Background-friendly per-tick budget.

    while ((FPlatformTime::Seconds() - StartTime) < TimeLimit)

    {

        switch (CurrentState)

        {

        case ETraceState::FindingReferencers:
            if (!bFindingReferencersLogged)
            {
                UE_LOG(LogCallChainTracer, Log, TEXT(">>> [1/3] Finding Referencers started..."));
                bFindingReferencersLogged = true;
            }

            if (!TickState_FindingReferencers()) return false;

            break;

        case ETraceState::ScanningPackages:

            UE_LOG(LogCallChainTracer, Verbose, TEXT(">>> [2/3] Scanning Assets... (%d / %d)"), LoadedPackageIndex, TotalCount);

            if (!TickState_ScanningPackages(StartTime, TimeLimit)) return true;

            break;

        case ETraceState::ProcessingCallChain:

            if (!TickState_ProcessingCallChain(StartTime, TimeLimit)) return true;

            break;

        case ETraceState::ScanningCppSources:

            if (!TickState_ScanningCppSources(StartTime, TimeLimit)) return true;

            break;

        case ETraceState::Complete:

            UE_LOG(LogCallChainTracer, Log, TEXT(">>> Trace Complete!"));

            {

                TSharedPtr<FunctionCallChainTracer> KeepAlive = SelfReference;

                FinalizeTrace();

                SelfReference.Reset();

            }

            return false;

        }

    }

    return true;

}

// =====================================================

// =====================================================

bool FunctionCallChainTracer::TickState_FindingReferencers()

{

    UBlueprint* TargetBP = TargetBlueprint.Get();

    UClass* TargetClass = TargetFunctionOwnerClass.IsValid() ? TargetFunctionOwnerClass.Get() : GetBestBlueprintClass(TargetBP);

    if (!TargetBP || !TargetClass)

    {

        CurrentState = ETraceState::Complete;

        return true;

    }

    UPackage* TargetPackage = TargetBP->GetOutermost();

    if (!TargetPackage)

    {

        CurrentState = ETraceState::Complete;

        return true;

    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

    FARFilter CallableAssetFilter;

    CallableAssetFilter.bRecursivePaths = true;

    CallableAssetFilter.bRecursiveClasses = true;

    CallableAssetFilter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());

    CallableAssetFilter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());

    TArray<FAssetData> CallableAssets;

    AssetRegistryModule.Get().GetAssets(CallableAssetFilter, CallableAssets);

    TSet<FName> CallablePackageNames;

    TArray<FAssetData> BlueprintAssets;

    for (const FAssetData& AssetData : CallableAssets)

    {

        if (!IsTraceablePackageName(AssetData.PackageName))

        {

            continue;

        }

        CallablePackageNames.Add(AssetData.PackageName);

        if (AssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName()

            || AssetData.AssetClassPath.ToString().Contains(TEXT("Blueprint")))

        {

            BlueprintAssets.Add(AssetData);

        }

    }

    TArray<FName> ReverseDependencyQueue;

    TSet<FName> QueuedPackages;

    auto QueuePackage = [&ReverseDependencyQueue, &QueuedPackages, &CallablePackageNames](FName PackageName)

    {

        if (IsTraceablePackageName(PackageName)

            && (CallablePackageNames.Contains(PackageName) || ReverseDependencyQueue.Num() == 0)

            && !QueuedPackages.Contains(PackageName))

        {

            QueuedPackages.Add(PackageName);

            ReverseDependencyQueue.Add(PackageName);

        }

    };

    QueuePackage(TargetPackage->GetFName());

    const bool bNativeFunctionOwner = TargetFunctionOwnerClass.IsValid()

        && Cast<UBlueprint>(TargetFunctionOwnerClass->ClassGeneratedBy) == nullptr;

    if (bNativeFunctionOwner)

    {

        // Native UFUNCTION calls reference a /Script package rather than the

        // context Blueprint. Scan every callable asset so uses in unrelated

        // Blueprints and Level Blueprints are not silently omitted.

        for (const FName PackageName : CallablePackageNames)

        {

            QueuePackage(PackageName);

        }

    }

    else

    {

        // A call typed as a Blueprint parent can still dispatch to an override

        // on the selected child class, so include Blueprint parent packages.

        for (UClass* CurrentClass = TargetClass->GetSuperClass(); CurrentClass; CurrentClass = CurrentClass->GetSuperClass())

        {

            if (CurrentClass == UObject::StaticClass() || CurrentClass == AActor::StaticClass())

            {

                break;

            }

            if (UBlueprint* ParentBlueprint = Cast<UBlueprint>(CurrentClass->ClassGeneratedBy))

            {

                if (UPackage* ParentPackage = ParentBlueprint->GetOutermost())

                {

                    QueuePackage(ParentPackage->GetFName());

                }

            }

        }

    }

    for (const FAssetData& BlueprintAsset : BlueprintAssets)

    {

        if (DoesBlueprintAssetMentionTraceTarget(BlueprintAsset, TargetFunctionName, TargetBP, TargetFunctionOwnerClass.Get()))

        {

            QueuePackage(BlueprintAsset.PackageName);

        }

    }

    // Follow the complete reverse hard-dependency closure. This is what lets a

    // B -> A -> Target chain include B even when B never references Target directly.

    for (int32 QueueIndex = 0; QueueIndex < ReverseDependencyQueue.Num(); ++QueueIndex)

    {

        TArray<FName> Referencers;

        AssetRegistryModule.Get().GetReferencers(

            ReverseDependencyQueue[QueueIndex], Referencers,

            UE::AssetRegistry::EDependencyCategory::Package,

            UE::AssetRegistry::EDependencyQuery::Hard);

        for (const FName Referencer : Referencers)

        {

            QueuePackage(Referencer);

        }

    }

    PackagesToScan = MoveTemp(ReverseDependencyQueue);

    TotalCount = PackagesToScan.Num();

    LoadedPackageIndex = 0;

    Result.ScannedPackageCount = TotalCount;

    UE_LOG(LogCallChainTracer, Display, TEXT("Found %d transitive caller package candidate(s)."), TotalCount);

    OnTraceProgress.ExecuteIfBound(TEXT("Collecting transitive callers..."), 0, TotalCount);

    CurrentState = ETraceState::ScanningPackages;

    return true;

}

// =====================================================

bool FunctionCallChainTracer::TickState_ScanningPackages(double StartTime, double TimeLimit)

{

    int32 ProcessedThisTick = 0;

    while (LoadedPackageIndex < PackagesToScan.Num() && ProcessedThisTick < TMPerf::CallChainPackagesPerTick())

    {

        if ((FPlatformTime::Seconds() - StartTime) > TimeLimit) break;

        FName PkgName = PackagesToScan[LoadedPackageIndex];

        UPackage* Package = FindPackage(nullptr, *PkgName.ToString());

        if (!Package) Package = LoadPackage(nullptr, *PkgName.ToString(), LOAD_None);

        if (Package)

        {

            KeepObjectAlive(Package);

            SearchCallersInPackage(Package, InitialCallers, StartTime + TimeLimit);

        }

        LoadedPackageIndex++;

        ProcessedThisTick++;

        if (LoadedPackageIndex % 10 == 0)

        {

            OnTraceProgress.ExecuteIfBound(TEXT("Scanning Assets..."), LoadedPackageIndex, TotalCount);

        }

    }

    if (LoadedPackageIndex >= PackagesToScan.Num())

    {

        InitializeProcessQueue();

        CurrentState = ETraceState::ProcessingCallChain;

        ProcessedCount = 0;

    }

    return true;

}

// =====================================================

// =====================================================

bool FunctionCallChainTracer::TickState_ProcessingCallChain(double StartTime, double TimeLimit)

{

    if (ProcessQueue.Num() == 0)

    {

        BuildCppSourceFileQueue();

        CurrentState = CppSourceFilesToScan.Num() > 0 ? ETraceState::ScanningCppSources : ETraceState::Complete;

        return true;

    }

    constexpr int32 MaxDepth = 32;

    int32 ProcessedThisTick = 0;

    while (ProcessQueue.Num() > 0 && ProcessedThisTick < TMPerf::CallChainPathsPerTick())

    {

        if ((FPlatformTime::Seconds() - StartTime) > TimeLimit)

        {

            break;

        }

        TPair<UEdGraphNode*, TArray<FCallChainNode>> CurrentItem = ProcessQueue.Pop();

        UEdGraphNode* CurrentNode = CurrentItem.Key;

        TArray<FCallChainNode> CurrentPath = MoveTemp(CurrentItem.Value);

        if (!CurrentNode)

        {

            continue;

        }

        ++ProcessedCount;

        ++ProcessedThisTick;

        UEdGraph* Graph = CurrentNode->GetGraph();

        UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForNode(CurrentNode);

        if (!Graph || !BP)

        {

            continue;

        }

        if (CurrentPath.Num() >= MaxDepth)

        {

            FCallChainPath TruncatedPath = CreatePathStruct(CurrentPath);

            TruncatedPath.bTruncated = true;

            TruncatedPath.TerminationReason = FString::Printf(TEXT("Stopped at the safety depth limit (%d nodes)."), MaxDepth);

            AddUniquePath(TruncatedPath);

            continue;

        }

        const FName ContainerFunctionName = Graph->GetFName();

        const FString GraphName = ContainerFunctionName.ToString();

        if (IsRootExecutionGraph(Graph, BP))

        {

            TArray<UEdGraphNode*> RootSourceNodes;

            FindRootExecSourceNodes(CurrentNode, RootSourceNodes);

            if (RootSourceNodes.Num() == 0)

            {

                FCallChainNode RootNode = CreateChainNode(nullptr, BP, false);

                RootNode.FunctionName = GraphName;

                RootNode.GraphName = GraphName;

                RootNode.bIsEvent = true;

                RootNode.RelationKind = ECallChainRelationKind::EntryPoint;

                RootNode.MatchReason = TEXT("The execution root could not be resolved (pure or disconnected execution path).");

                TArray<FCallChainNode> FinalNodes;

                FinalNodes.Add(RootNode);

                FinalNodes.Append(CurrentPath);

                FCallChainPath FinalPath = CreatePathStruct(FinalNodes);

                FinalPath.TerminationReason = RootNode.MatchReason;

                AddUniquePath(FinalPath);

                continue;

            }

            for (UEdGraphNode* RootSourceNode : RootSourceNodes)

            {

                TArray<FCallChainNode> FinalNodes;

                const bool bRootAlreadyInPath = CurrentPath.Num() > 0 && CurrentPath[0].SourceNode == RootSourceNode;

                if (!bRootAlreadyInPath)

                {

                    FCallChainNode RootNode = CreateChainNode(RootSourceNode, BP, false);

                    RootNode.bIsEvent = true;

                    RootNode.RelationKind = ECallChainRelationKind::EntryPoint;

                    RootNode.MatchReason = TEXT("Reachable execution entry point.");

                    FinalNodes.Add(RootNode);

                }

                FinalNodes.Append(CurrentPath);

                AddUniquePath(CreatePathStruct(FinalNodes));

            }

            continue;

        }

        UClass* ContainerOwnerClass = GetBestBlueprintClass(BP);

        const FGuid ContainerFunctionGuid = GetCallableGuidForGraph(Graph);

        TArray<UEdGraphNode*> NextCallers;

        FindCallersInMemory(ContainerFunctionName, BP, ContainerOwnerClass, ContainerFunctionGuid, NextCallers);

        if (NextCallers.Num() == 0)

        {

            FCallChainNode EntryNode = CreateChainNode(nullptr, BP, false);

            EntryNode.FunctionName = GraphName;

            EntryNode.GraphName = GraphName;

            EntryNode.bIsEvent = true;

            EntryNode.RelationKind = ECallChainRelationKind::EntryPoint;

            EntryNode.MatchReason = TEXT("No Blueprint caller was found in the scanned dependency closure.");

            TArray<FCallChainNode> FinalNodes;

            FinalNodes.Add(EntryNode);

            FinalNodes.Append(CurrentPath);

            FCallChainPath FinalPath = CreatePathStruct(FinalNodes);

            FinalPath.TerminationReason = EntryNode.MatchReason;

            AddUniquePath(FinalPath);

            continue;

        }

        for (UEdGraphNode* NextNode : NextCallers)

        {

            if (!NextNode)

            {

                continue;

            }

            bool bLoop = false;

            for (const FCallChainNode& ExistingNode : CurrentPath)

            {

                if (ExistingNode.SourceNode == NextNode)

                {

                    bLoop = true;

                    break;

                }

            }

            UBlueprint* NextBP = FBlueprintEditorUtils::FindBlueprintForNode(NextNode);

            if (!NextBP)

            {

                continue;

            }

            FCallChainNode NextChainNode = CreateChainNode(NextNode, NextBP, false);

            TArray<FCallChainNode> NewPath;

            NewPath.Add(NextChainNode);

            NewPath.Append(CurrentPath);

            if (bLoop)

            {

                FCallChainPath CyclePath = CreatePathStruct(NewPath);

                CyclePath.bContainsCycle = true;

                CyclePath.TerminationReason = TEXT("Cycle detected; expansion stopped at the repeated call site.");

                AddUniquePath(CyclePath);

                continue;

            }

            ProcessQueue.Add(TPair<UEdGraphNode*, TArray<FCallChainNode>>(NextNode, MoveTemp(NewPath)));

        }

    }

    OnTraceProgress.ExecuteIfBound(TEXT("Analyzing call paths..."), ProcessedCount, ProcessedCount + ProcessQueue.Num());

    return true;

}

bool FunctionCallChainTracer::SearchCallersInPackage(UPackage* Package, TArray<UEdGraphNode*>& OutCallers, double DeadlineSeconds)

{

    if (!Package)

    {

        return false;

    }

    TArray<UBlueprint*> PackageBlueprints;

    ForEachObjectWithPackage(Package, [&PackageBlueprints](UObject* Object)

    {

        if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))

        {

            PackageBlueprints.AddUnique(Blueprint);

        }

        return true;

    });

    if (UBlueprint* MainBlueprint = Cast<UBlueprint>(Package->FindAssetInPackage()))

    {

        PackageBlueprints.AddUnique(MainBlueprint);

    }

    int32 ScannedNodeCount = 0;

    for (UBlueprint* BP : PackageBlueprints)

    {

        if (!BP)

        {

            continue;

        }

        if (FPlatformTime::Seconds() >= DeadlineSeconds)

        {

            UE_LOG(LogCallChainTracer, Warning, TEXT("Call chain package scan yielded before scanning Blueprint %s in %s."), *GetNameSafe(BP), *Package->GetName());

            return false;

        }

        KeepObjectAlive(BP);

        TArray<UEdGraph*> AllGraphs;

        GetBlueprintGraphs(BP, AllGraphs);

        for (UEdGraph* Graph : AllGraphs)

        {

            KeepObjectAlive(Graph);

            if (!FindCallersInGraph(Graph, TargetFunctionName, TargetBlueprint.Get(), TargetFunctionOwnerClass.Get(), TargetFunctionGuid, OutCallers, DeadlineSeconds, ScannedNodeCount))

            {

                UE_LOG(LogCallChainTracer, Warning, TEXT("Call chain scan yielded while scanning package %s after %d graph node(s)."), *Package->GetName(), ScannedNodeCount);

                return false;

            }

        }

    }

    return true;

}

bool FunctionCallChainTracer::FindCallersInGraph(UEdGraph* Graph, FName TargetFunc, UBlueprint* TargetBP,

    UClass* TargetOwnerClass, const FGuid& TargetGuid, TArray<UEdGraphNode*>& OutCallers, double DeadlineSeconds, int32& InOutScannedNodeCount)

{

    if (!Graph)

    {

        return true;

    }

    for (UEdGraphNode* Node : Graph->Nodes)

    {

        if (++InOutScannedNodeCount > TMPerf::MaxGraphNodesPerBlueprint() || FPlatformTime::Seconds() >= DeadlineSeconds)

        {

            return false;

        }

        if (DoesNodeReferenceFunction(Node, TargetFunc, TargetBP, TargetOwnerClass, TargetGuid))

        {

            OutCallers.AddUnique(Node);

        }

    }

    return true;

}

bool FunctionCallChainTracer::DoesNodeReferenceFunction(UEdGraphNode* Node, FName TargetFunc, UBlueprint* TargetBP,

    UClass* TargetOwnerClass, const FGuid& TargetGuid, TSet<UEdGraph*>* VisitedMacroGraphs)

{

    if (!Node || !TargetBP || TargetFunc.IsNone())

    {

        return false;

    }

    TSet<UEdGraph*> LocalVisitedMacroGraphs;

    if (!VisitedMacroGraphs)

    {

        VisitedMacroGraphs = &LocalVisitedMacroGraphs;

    }

    if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))

    {

        return IsMatching(CallNode, TargetFunc, TargetBP, TargetOwnerClass, TargetGuid);

    }

    if (UK2Node_CreateDelegate* DelegateNode = Cast<UK2Node_CreateDelegate>(Node))

    {

        if (DelegateNode->GetFunctionName() != TargetFunc)

        {

            return false;

        }

        UBlueprint* NodeBP = FBlueprintEditorUtils::FindBlueprintForNode(DelegateNode);

        return IsClassCompatibleWithTarget(DelegateNode->GetScopeClass(), TargetBP, NodeBP, TargetOwnerClass);

    }

    if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))

    {

        if (UEdGraph* MacroGraph = MacroNode->GetMacroGraph())

        {

            if (VisitedMacroGraphs->Contains(MacroGraph))

            {

                return false;

            }

            VisitedMacroGraphs->Add(MacroGraph);

            for (UEdGraphNode* InnerNode : MacroGraph->Nodes)

            {

                if (DoesNodeReferenceFunction(InnerNode, TargetFunc, TargetBP, TargetOwnerClass, TargetGuid, VisitedMacroGraphs))

                {

                    return true;

                }

            }

        }

    }

    return false;

}

bool FunctionCallChainTracer::IsMatching(UK2Node_CallFunction* CallNode, FName TargetFunc, UBlueprint* TargetBP,

    UClass* TargetOwnerClass, const FGuid& TargetGuid)

{

    if (!CallNode || !TargetBP || CallNode->FunctionReference.GetMemberName() != TargetFunc)

    {

        return false;

    }

    const FGuid ReferencedGuid = CallNode->FunctionReference.GetMemberGuid();

    if (TargetGuid.IsValid() && ReferencedGuid.IsValid() && TargetGuid == ReferencedGuid)

    {

        return true;

    }

    UBlueprint* NodeBP = FBlueprintEditorUtils::FindBlueprintForNode(CallNode);

    if (UFunction* ReferencedFunction = CallNode->GetTargetFunction())

    {

        if (IsClassCompatibleWithTarget(ReferencedFunction->GetOwnerClass(), TargetBP, NodeBP, TargetOwnerClass))

        {

            return true;

        }

    }

    UClass* ReferencedClass = CallNode->FunctionReference.GetMemberParentClass();

    if (IsClassCompatibleWithTarget(ReferencedClass, TargetBP, NodeBP, TargetOwnerClass))

    {

        return true;

    }

    // Local self-calls can have no serialized parent class until the Blueprint is compiled.

    if (!ReferencedClass && NodeBP == TargetBP)

    {

        return true;

    }

    if (UEdGraphPin* SelfPin = CallNode->FindPin(UEdGraphSchema_K2::PN_Self))

    {

        for (UEdGraphPin* LinkedPin : SelfPin->LinkedTo)

        {

            if (!LinkedPin)

            {

                continue;

            }

            if (UClass* PinClass = GetPinClass(LinkedPin))

            {

                if (IsClassCompatibleWithTarget(PinClass, TargetBP, NodeBP, TargetOwnerClass))

                {

                    return true;

                }

            }

        }

    }

    return false;

}

// =====================================================

// Queue initialization

// =====================================================

void FunctionCallChainTracer::InitializeProcessQueue()

{

    BuildCallerCache();

    Result.DirectCallerCount = InitialCallers.Num();

    UE_LOG(LogCallChainTracer, Display, TEXT("Found %d direct caller node(s) for %s."), InitialCallers.Num(), *TargetFunctionName.ToString());

    TSet<FString> ProcessedContexts;

    for (UEdGraphNode* Caller : InitialCallers)

    {

        UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForNode(Caller);

        UEdGraph* Graph = Caller->GetGraph();

        if (!BP || !Graph) continue;

        FString ContextKey = FString::Printf(TEXT("%s::%s::%s"),

            *BP->GetName(),

            *Graph->GetName(),

            *Caller->NodeGuid.ToString());

        if (ProcessedContexts.Contains(ContextKey)) continue;

        ProcessedContexts.Add(ContextKey);

        FCallChainNode CallerNode = CreateChainNode(Caller, BP, false);

        FCallChainNode TargetNode = CreateChainNode(nullptr, TargetBlueprint.Get(), true);

        TargetNode.FunctionName = TargetFunctionName.ToString();

        TargetNode.GraphName = TargetFunctionName.ToString();

        TargetNode.NodeGuid = TargetFunctionGuid;

        if (TargetFunctionOwnerClass.IsValid())

        {

            TargetNode.AssetName = GetClassDisplayName(TargetFunctionOwnerClass.Get());

        }

        TargetNode.bIsTargetFunction = true;

        TargetNode.RelationKind = ECallChainRelationKind::Target;

        TargetNode.MatchReason = TEXT("Selected trace target.");

        TArray<FCallChainNode> Path;

        Path.Add(CallerNode);

        Path.Add(TargetNode);

        ProcessQueue.Add(TPair<UEdGraphNode*, TArray<FCallChainNode>>(Caller, Path));

    }

    if (ProcessQueue.Num() == 0)

    {

        FCallChainNode TargetNode = CreateChainNode(nullptr, TargetBlueprint.Get(), true);

        TargetNode.FunctionName = TargetFunctionName.ToString();

        TargetNode.GraphName = TargetFunctionName.ToString();

        TargetNode.NodeGuid = TargetFunctionGuid;

        TargetNode.RelationKind = ECallChainRelationKind::Target;

        TargetNode.MatchReason = TEXT("No Blueprint call site was found.");

        FCallChainPath NoCallerPath;

        NoCallerPath.Nodes.Add(TargetNode);

        NoCallerPath.TerminationReason = TEXT("No Blueprint caller was found in the scanned dependency closure.");

        AddUniquePath(NoCallerPath);

    }

    UE_LOG(LogCallChainTracer, Display, TEXT("Queued %d call chain seed(s) for %s."), ProcessQueue.Num(), *TargetFunctionName.ToString());

}

void FunctionCallChainTracer::BuildCppSourceFileQueue()
{
    CppSourceFilesToScan.Reset();
    CppSourceFileIndex = 0;
    AddedCppSourceCallKeys.Reset();

    if (TargetFunctionName.ToString().Len() < 3)
    {
        UE_LOG(LogCallChainTracer, Warning, TEXT("Skipping C++ source scan because target function name is too short: %s"), *TargetFunctionName.ToString());
        return;
    }

    TArray<FString> Roots;
    Roots.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source")));
    Roots.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins")));

    const FString PluginSourceRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins"), TEXT("TraceMotive"), TEXT("Source"));
    Roots.AddUnique(PluginSourceRoot);

    for (const FString& Root : Roots)
    {
        if (!FPaths::DirectoryExists(Root)) continue;
        TArray<FString> FoundFiles;
        IFileManager::Get().FindFilesRecursive(FoundFiles, *Root, TEXT("*.*"), true, false, false);
        for (FString& FilePath : FoundFiles)
        {
            FPaths::NormalizeFilename(FilePath);
            if (FilePath.Contains(TEXT("/Intermediate/"))
                || FilePath.Contains(TEXT("/Binaries/"))
                || FilePath.Contains(TEXT("/Saved/"))
                || FilePath.Contains(TEXT("/DerivedDataCache/"))
                || FilePath.EndsWith(TEXT(".generated.h"), ESearchCase::IgnoreCase))
            {
                continue;
            }
            if (IsLikelyCppSourceFile(FilePath))
            {
                CppSourceFilesToScan.AddUnique(FilePath);
            }
        }
    }

    UE_LOG(LogCallChainTracer, Display, TEXT("Queued %d C++ source file(s) for call-chain text scan."), CppSourceFilesToScan.Num());
    OnTraceProgress.ExecuteIfBound(TEXT("Scanning C++ source files..."), 0, CppSourceFilesToScan.Num());
}

bool FunctionCallChainTracer::TickState_ScanningCppSources(double StartTime, double TimeLimit)
{
    int32 ProcessedThisTick = 0;
    const double DeadlineSeconds = StartTime + TimeLimit;
    while (CppSourceFileIndex < CppSourceFilesToScan.Num() && ProcessedThisTick < TMPerf::CallChainPathsPerTick())
    {
        if (FPlatformTime::Seconds() >= DeadlineSeconds) break;
        ScanCppSourceFile(CppSourceFilesToScan[CppSourceFileIndex], DeadlineSeconds);
        ++CppSourceFileIndex;
        ++ProcessedThisTick;
    }

    OnTraceProgress.ExecuteIfBound(TEXT("Scanning C++ source files..."), CppSourceFileIndex, CppSourceFilesToScan.Num());
    if (CppSourceFileIndex >= CppSourceFilesToScan.Num())
    {
        CurrentState = ETraceState::Complete;
    }
    return true;
}

bool FunctionCallChainTracer::ScanCppSourceFile(const FString& SourceFilePath, double DeadlineSeconds)
{
    FString FileText;
    if (!FFileHelper::LoadFileToString(FileText, *SourceFilePath))
    {
        return false;
    }

    const FString FunctionName = TargetFunctionName.ToString();
    if (!FileText.Contains(FunctionName, ESearchCase::CaseSensitive))
    {
        return false;
    }

    TArray<FString> Lines;
    FileText.ParseIntoArrayLines(Lines, false);
    for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
    {
        if (FPlatformTime::Seconds() >= DeadlineSeconds) return true;
        int32 ColumnNumber = INDEX_NONE;
        if (LooksLikeCppCallSite(Lines[LineIndex], FunctionName, ColumnNumber))
        {
            AddCppSourceCallerPath(SourceFilePath, LineIndex + 1, ColumnNumber, Lines[LineIndex]);
        }
    }
    return true;
}

void FunctionCallChainTracer::AddCppSourceCallerPath(const FString& SourceFilePath, int32 LineNumber, int32 ColumnNumber, const FString& LineText)
{
    if (Result.CppSourceCallerCount >= MaxCppSourceCallChainMatches)
    {
        return;
    }

    const FString Key = FString::Printf(TEXT("%s:%d:%d"), *SourceFilePath, LineNumber, ColumnNumber);
    if (AddedCppSourceCallKeys.Contains(Key))
    {
        return;
    }
    AddedCppSourceCallKeys.Add(Key);

    FCallChainNode CppNode;
    CppNode.bIsCppSource = true;
    CppNode.RelationKind = ECallChainRelationKind::CppSourceCall;
    CppNode.FunctionName = TargetFunctionName.ToString();
    CppNode.AssetPath = SourceFilePath;
    CppNode.SourceFilePath = SourceFilePath;
    CppNode.SourceLine = LineNumber;
    CppNode.SourceColumn = ColumnNumber;
    CppNode.SourceSnippet = LineText.TrimStartAndEnd();
    CppNode.AssetName = FPaths::GetCleanFilename(SourceFilePath);
    CppNode.GraphName = FString::Printf(TEXT("line %d"), LineNumber);
    CppNode.MatchReason = FString::Printf(TEXT("C++ source text references %s(). Text scan result; verify overloads/macros manually."), *TargetFunctionName.ToString());

    FCallChainNode TargetNode = CreateChainNode(nullptr, TargetBlueprint.Get(), true);
    TargetNode.FunctionName = TargetFunctionName.ToString();
    if (TargetFunctionOwnerClass.IsValid())
    {
        TargetNode.TargetClassName = GetClassDisplayName(TargetFunctionOwnerClass.Get());
    }

    FCallChainPath Path;
    Path.Nodes.Add(CppNode);
    Path.Nodes.Add(TargetNode);
    AddUniquePath(Path);
    ++Result.CppSourceCallerCount;
}

// =====================================================

void FunctionCallChainTracer::AddUniquePath(const FCallChainPath& NewPath)

{

    for (const FCallChainPath& Existing : Result.Paths)

    {

        if (Existing.Nodes.Num() != NewPath.Nodes.Num()

            || Existing.bTruncated != NewPath.bTruncated

            || Existing.bContainsCycle != NewPath.bContainsCycle)

        {

            continue;

        }

        bool bSame = true;

        for (int32 Index = 0; Index < NewPath.Nodes.Num(); ++Index)

        {

            if (Existing.Nodes[Index].FunctionName != NewPath.Nodes[Index].FunctionName

                || Existing.Nodes[Index].AssetPath != NewPath.Nodes[Index].AssetPath

                || Existing.Nodes[Index].GraphName != NewPath.Nodes[Index].GraphName

                || Existing.Nodes[Index].NodeGuid != NewPath.Nodes[Index].NodeGuid)

            {

                bSame = false;

                break;

            }

        }

        if (bSame)

        {

            return;

        }

    }

    Result.Paths.Add(NewPath);

    if (NewPath.bTruncated)

    {

        ++Result.TruncatedPathCount;

    }

    if (NewPath.bContainsCycle)

    {

        ++Result.CyclicPathCount;

    }

}

FCallChainPath FunctionCallChainTracer::CreatePathStruct(const TArray<FCallChainNode>& Nodes)

{

    FCallChainPath P;

    P.Nodes = Nodes;

    return P;

}

void FunctionCallChainTracer::FinalizeTrace()

{

    bIsTracing = false;

    if (TickerHandle.IsValid())

    {

        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

        TickerHandle.Reset();

    }

    TraceSession.Complete();

    Result.Paths.Sort([](const FCallChainPath& Left, const FCallChainPath& Right)

    {

        const FString LeftKey = Left.Nodes.Num() > 0

            ? Left.Nodes[0].AssetPath + TEXT("|") + Left.Nodes[0].FunctionName

            : FString();

        const FString RightKey = Right.Nodes.Num() > 0

            ? Right.Nodes[0].AssetPath + TEXT("|") + Right.Nodes[0].FunctionName

            : FString();

        if (LeftKey == RightKey)

        {

            return Left.Nodes.Num() < Right.Nodes.Num();

        }

        return LeftKey < RightKey;

    });

    OnTraceComplete.ExecuteIfBound(Result);

    ReleaseScanObjectReferences();

}

void FunctionCallChainTracer::KeepObjectAlive(UObject* Object)

{

    if (!Object)

    {

        return;

    }

    for (const TStrongObjectPtr<UObject>& ExistingObject : LoadedObjectsToKeepAlive)

    {

        if (ExistingObject.Get() == Object)

        {

            return;

        }

    }

    LoadedObjectsToKeepAlive.Emplace(Object);

}

void FunctionCallChainTracer::ReleaseScanObjectReferences()

{

    LoadedObjectsToKeepAlive.Empty();

}

bool FunctionCallChainTracer::IsEventNode(UEdGraphNode* Node) { return Node && (Node->IsA<UK2Node_Event>() || Node->IsA<UK2Node_CustomEvent>() || Node->IsA<UK2Node_FunctionEntry>()); }

FCallChainNode FunctionCallChainTracer::CreateChainNode(UEdGraphNode* Node, UBlueprint* BP, bool bIsTarget)

{

    FCallChainNode ChainNode;

    if (!BP)

    {

        return ChainNode;

    }

    if (UPackage* Package = BP->GetOutermost())

    {

        ChainNode.AssetName = FPaths::GetBaseFilename(Package->GetName());

        ChainNode.AssetPath = Package->GetName();

    }

    ChainNode.OwnerBlueprint = BP;

    ChainNode.bIsTargetFunction = bIsTarget;

    if (bIsTarget)

    {

        ChainNode.RelationKind = ECallChainRelationKind::Target;

        ChainNode.MatchReason = TEXT("Selected trace target.");

    }

    if (!Node)

    {

        return ChainNode;

    }

    ChainNode.SourceNode = Node;

    ChainNode.NodeGuid = Node->NodeGuid;

    ChainNode.bIsEvent = IsEventNode(Node);

    if (UEdGraph* Graph = Node->GetGraph())

    {

        ChainNode.GraphName = Graph->GetName();

        ChainNode.GraphName.RemoveFromStart(TEXT("ExecuteUbergraph_"));

    }

    if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))

    {

        ChainNode.FunctionName = EventNode->EventReference.GetMemberName().ToString();

        ChainNode.RelationKind = ECallChainRelationKind::EntryPoint;

        ChainNode.MatchReason = TEXT("Blueprint event entry point.");

    }

    else if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))

    {

        ChainNode.FunctionName = CustomEventNode->CustomFunctionName.ToString();

        ChainNode.RelationKind = ECallChainRelationKind::EntryPoint;

        ChainNode.MatchReason = TEXT("Blueprint custom-event entry point.");

    }

    else if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))

    {

        ChainNode.FunctionName = CallNode->FunctionReference.GetMemberName().ToString();

        ChainNode.RelationKind = ECallChainRelationKind::DirectCall;

        ChainNode.MatchReason = TEXT("Direct Blueprint function call.");

        FillCallTargetInfo(CallNode, BP, TargetFunctionOwnerClass.Get(), ChainNode);

    }

    else if (UK2Node_CreateDelegate* DelegateNode = Cast<UK2Node_CreateDelegate>(Node))

    {

        ChainNode.FunctionName = DelegateNode->GetFunctionName().ToString();

        ChainNode.RelationKind = ECallChainRelationKind::DelegateBinding;

        ChainNode.MatchReason = TEXT("Delegate binding reference; invocation occurs when the delegate executes.");

        ChainNode.TargetClassName = GetClassDisplayName(DelegateNode->GetScopeClass());

    }

    else if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))

    {

        ChainNode.FunctionName = MacroNode->GetNodeTitle(ENodeTitleType::ListView).ToString();

        ChainNode.RelationKind = ECallChainRelationKind::MacroExpansion;

        ChainNode.MatchReason = TEXT("The expanded macro graph contains the matching call.");

    }

    else if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))

    {

        ChainNode.FunctionName = FunctionEntry->GetGraph() ? FunctionEntry->GetGraph()->GetName() : TEXT("Function Entry");

        ChainNode.RelationKind = ECallChainRelationKind::EntryPoint;

        ChainNode.MatchReason = TEXT("Blueprint function entry point.");

    }

    else

    {

        ChainNode.FunctionName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();

        ChainNode.RelationKind = ECallChainRelationKind::Unknown;

        ChainNode.MatchReason = TEXT("Matched Blueprint graph node.");

    }

    return ChainNode;

}

void FunctionCallChainTracer::BuildCallerCache()

{

    CachedCallerMap.Empty();

    int32 CachedBlueprintCount = 0;

    int32 CachedNodeCount = 0;

    const double CacheDeadlineSeconds = FPlatformTime::Seconds() + TMPerf::CallChainTickBudgetSeconds();

    for (TObjectIterator<UBlueprint> It; It; ++It)

    {

        UBlueprint* BP = *It;

        if (++CachedBlueprintCount > TMPerf::MaxCallerCacheBlueprintsPerBuild() || FPlatformTime::Seconds() >= CacheDeadlineSeconds)

        {

            UE_LOG(LogCallChainTracer, Warning, TEXT("Caller cache build yielded after %d loaded Blueprint(s), %d node(s)."), CachedBlueprintCount, CachedNodeCount);

            break;

        }

        if (!BP || BP->HasAnyFlags(RF_Transient | RF_ClassDefaultObject)) continue;

        UPackage* Package = BP->GetOutermost();

        if (!Package || !IsTraceablePackageName(Package->GetFName())) continue;

        KeepObjectAlive(Package);

        KeepObjectAlive(BP);

        TArray<UEdGraph*> GraphsToCheck;

        GetBlueprintGraphs(BP, GraphsToCheck);

        for (UEdGraph* Graph : GraphsToCheck)

        {

            if (!Graph) continue;

            KeepObjectAlive(Graph);

            for (UEdGraphNode* Node : Graph->Nodes)

            {

                if (++CachedNodeCount > TMPerf::MaxGraphNodesPerBlueprint() || FPlatformTime::Seconds() >= CacheDeadlineSeconds)

                {

                    UE_LOG(LogCallChainTracer, Warning, TEXT("Caller cache node scan yielded after %d node(s)."), CachedNodeCount);

                    return;

                }

                TArray<FName> ReferencedFunctionNames;

                AddReferencedFunctionNames(Node, ReferencedFunctionNames);

                for (const FName& FunctionName : ReferencedFunctionNames)

                {

                    if (!FunctionName.IsNone())

                    {

                        CachedCallerMap.FindOrAdd(FunctionName).AddUnique(Node);

                    }

                }

            }

        }

    }

}

void FunctionCallChainTracer::FindCallersInMemory(FName TargetFunc, UBlueprint* TargetBP,

    UClass* TargetOwnerClass, const FGuid& TargetGuid, TArray<UEdGraphNode*>& OutCallers)

{

    if (TArray<TWeakObjectPtr<UEdGraphNode>>* FoundNodes = CachedCallerMap.Find(TargetFunc))

    {

        for (const TWeakObjectPtr<UEdGraphNode>& WeakNode : *FoundNodes)

        {

            UEdGraphNode* Node = WeakNode.Get();

            if (Node && DoesNodeReferenceFunction(Node, TargetFunc, TargetBP, TargetOwnerClass, TargetGuid))

            {

                OutCallers.AddUnique(Node);

            }

        }

    }

}

FGuid FunctionCallChainTracer::GetCallableGuidForGraph(UEdGraph* Graph) const

{

    return Graph ? Graph->GraphGuid : FGuid();

}

void FunctionCallChainTracer::FindRootExecSourceNodes(UEdGraphNode* StartNode, TArray<UEdGraphNode*>& OutRootNodes) const

{

    OutRootNodes.Reset();

    if (!StartNode)

    {

        return;

    }

    TArray<UEdGraphNode*> PendingNodes;

    TSet<UEdGraphNode*> VisitedNodes;

    PendingNodes.Add(StartNode);

    constexpr int32 MaxTraversalNodes = 4096;

    while (PendingNodes.Num() > 0 && VisitedNodes.Num() < MaxTraversalNodes)

    {

        UEdGraphNode* CurrentNode = PendingNodes.Pop();

        if (!CurrentNode || VisitedNodes.Contains(CurrentNode))

        {

            continue;

        }

        VisitedNodes.Add(CurrentNode);

        if (CurrentNode->IsA<UK2Node_Event>()

            || CurrentNode->IsA<UK2Node_CustomEvent>()

            || CurrentNode->IsA<UK2Node_FunctionEntry>())

        {

            OutRootNodes.AddUnique(CurrentNode);

            continue;

        }

        for (UEdGraphPin* Pin : CurrentNode->Pins)

        {

            if (!IsExecutionPin(Pin, EGPD_Input))

            {

                continue;

            }

            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)

            {

                if (LinkedPin)

                {

                    PendingNodes.Add(LinkedPin->GetOwningNode());

                }

            }

        }

    }

}
