// Source/TraceMotive/Private/VisualRefSearcher.cpp



#include "VisualRefSearcher.h"

#include "TMPerformanceGuard.h"
#include "Runtime/Launch/Resources/Version.h"

#include "AssetRegistry/AssetIdentifier.h"

#include "AssetRegistry/AssetRegistryModule.h"

#include "EdGraph/EdGraph.h"

#include "EdGraphSchema_K2.h"

#include "Engine/Blueprint.h"

#include "HAL/PlatformTime.h"

#include "K2Node.h"

#include "K2Node_AddDelegate.h"

#include "K2Node_AssignDelegate.h"

#include "K2Node_BaseMCDelegate.h"

#include "K2Node_CallFunction.h"

#include "K2Node_CallDelegate.h"

#include "K2Node_ClearDelegate.h"

#include "K2Node_CreateDelegate.h"

#include "K2Node_CustomEvent.h"

#include "K2Node_DelegateSet.h"

#include "K2Node_DynamicCast.h"

#include "K2Node_Event.h"

#include "K2Node_FunctionEntry.h"

#include "K2Node_MacroInstance.h"

#include "K2Node_RemoveDelegate.h"

#include "K2Node_Variable.h"

#include "Kismet2/BlueprintEditorUtils.h"

#include "Modules/ModuleManager.h"

#include "UObject/UObjectIterator.h"



DEFINE_LOG_CATEGORY_STATIC(LogRefSearcher, Log, All);



namespace

{

    // Runtime budgets are centralized in TMPerformanceGuard.h so large projects do not freeze the editor.



    bool IsSearchablePackageName(FName PackageName)

    {

        const FString PackagePath = PackageName.ToString();

        if (!PackagePath.StartsWith(TEXT("/"))) return false;



        return !PackagePath.StartsWith(TEXT("/Engine"))

            && !PackagePath.StartsWith(TEXT("/Script"))

            && !PackagePath.StartsWith(TEXT("/Memory"))

            && !PackagePath.StartsWith(TEXT("/Temp"))

            && !PackagePath.StartsWith(TEXT("/Transient"));

    }



    void SetMatchReason(FString* OutMatchReason, const TCHAR* Reason)

    {

        if (OutMatchReason)

        {

            *OutMatchReason = Reason;

        }

    }



    bool DoesAssetMetadataMentionName(const FAssetData& AssetData, const FString& SearchText)

    {

        if (SearchText.IsEmpty())

        {

            return false;

        }



        FString TagValue;

        static const FName SearchableTagNames[] =

        {

            TEXT("FunctionGraphs"),

            TEXT("UbergraphPages"),

            TEXT("ImplementedInterfaces"),

            TEXT("ParentClass"),

            TEXT("GeneratedClass"),

            TEXT("NativeParentClass"),

            TEXT("BlueprintDescription")

        };



        for (const FName TagName : SearchableTagNames)

        {

            if (AssetData.GetTagValue(TagName, TagValue) && TagValue.Contains(SearchText))

            {

                return true;

            }

        }



        return AssetData.AssetName.ToString().Contains(SearchText)

            || AssetData.PackageName.ToString().Contains(SearchText);

    }



    void AddUniqueAssetToQueue(TArray<FAssetData>& Queue, TSet<FName>& QueuedPackages, const FAssetData& AssetData)

    {

        if (!QueuedPackages.Contains(AssetData.PackageName))

        {

            QueuedPackages.Add(AssetData.PackageName);

            Queue.Add(AssetData);

        }

    }



    FString GetDispatcherActionLabel(const UEdGraphNode* Node)

    {

        if (!Node)

        {

            return TEXT("Dispatcher");

        }



        if (Node->IsA<UK2Node_CallDelegate>()) return TEXT("Dispatcher Call");

        if (Node->IsA<UK2Node_AssignDelegate>()) return TEXT("Dispatcher Assign");

        if (Node->IsA<UK2Node_AddDelegate>()) return TEXT("Dispatcher Bind");

        if (Node->IsA<UK2Node_RemoveDelegate>()) return TEXT("Dispatcher Unbind");

        if (Node->IsA<UK2Node_ClearDelegate>()) return TEXT("Dispatcher Unbind all");

        if (Node->IsA<UK2Node_DelegateSet>()) return TEXT("Dispatcher Event");

        if (Node->IsA<UK2Node_Event>()) return TEXT("Dispatcher Event");

        return TEXT("Dispatcher Reference");

    }

}



VisualRefSearcher::VisualRefSearcher(UBlueprint* InTargetBP, FName InVarOrFuncName, bool bIsFunction, UClass* InTargetOwnerClass, bool bIsDispatcher)

    : VariableName((!bIsFunction && !bIsDispatcher) ? InVarOrFuncName : NAME_None)

    , FunctionName((bIsFunction && !bIsDispatcher) ? InVarOrFuncName : NAME_None)

    , DispatcherName(bIsDispatcher ? InVarOrFuncName : NAME_None)

    , bSearchingFunction(bIsFunction && !bIsDispatcher)

    , bSearchingDispatcher(bIsDispatcher)

{

    TargetBP = InTargetBP;

    TargetOwnerClass = InTargetOwnerClass;

    AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

}



VisualRefSearcher::~VisualRefSearcher()

{

    if (TickerHandle.IsValid())

    {

        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

        TickerHandle.Reset();

    }

}



void VisualRefSearcher::CancelSearch()

{

    bIsSearching = false;
    TraceSession.Cancel();



    if (TickerHandle.IsValid())

    {

        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

        TickerHandle.Reset();

    }



    AssetQueue.Empty();

    LoadedBlueprintQueue.Empty();

    PendingLoadPackages.Empty();

    ActiveLoadHandles.Empty();

    ProcessedNodes.Empty();

    ProcessedPackages.Empty();

    NodeMatchReasons.Empty();



    OnRefFound.Unbind();

    OnBatchComplete.Unbind();

    OnSearchProgress.Unbind();

    OnSearchFinished.Unbind();



    SelfReference.Reset();

}



void VisualRefSearcher::SetSearchScope(EVisualRefSearchScope InSearchScope)

{

    SearchScope = InSearchScope;

}



void VisualRefSearcher::SetExhaustiveSearchEnabled(bool bInEnabled)

{

    bExhaustiveSearchEnabled = bInEnabled;

}



void VisualRefSearcher::FinishSearch()

{

    TSharedPtr<VisualRefSearcher> KeepAlive = SelfReference;

    bIsSearching = false;



    if (TickerHandle.IsValid())

    {

        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

        TickerHandle.Reset();

    }



    TraceSession.Complete();
    NotifyBatchCompleteThrottled(true);

    OnSearchFinished.ExecuteIfBound();



    OnRefFound.Unbind();

    OnBatchComplete.Unbind();

    OnSearchProgress.Unbind();

    OnSearchFinished.Unbind();



    AssetQueue.Empty();

    LoadedBlueprintQueue.Empty();

    PendingLoadPackages.Empty();

    ActiveLoadHandles.Empty();

    SelfReference.Reset();

}



void VisualRefSearcher::StoreMatchReason(UEdGraphNode* Node, const FString& Reason)

{

    if (!Node)

    {

        return;

    }



    NodeMatchReasons.Add(Node->NodeGuid, Reason.IsEmpty() ? TEXT("Structural match") : Reason);

}



FString VisualRefSearcher::GetMatchReason(UEdGraphNode* Node) const

{

    if (!Node)

    {

        return FString();

    }



    if (const FString* Reason = NodeMatchReasons.Find(Node->NodeGuid))

    {

        return *Reason;

    }



    return FString();

}



void VisualRefSearcher::NotifyBatchCompleteThrottled(bool bForce)

{

    if (!bPendingBatchNotify && !bForce)

    {

        return;

    }



    const double NowSeconds = FPlatformTime::Seconds();

    if (!bForce && LastBatchNotifySeconds > 0.0 && NowSeconds - LastBatchNotifySeconds < 0.35)

    {

        return;

    }



    bPendingBatchNotify = false;

    LastBatchNotifySeconds = NowSeconds;

    OnBatchComplete.ExecuteIfBound();

}



void VisualRefSearcher::InitializeSearchKeys()

{

    if (!TargetBP.IsValid())

    {

        return;

    }



    if (bSearchingDispatcher)

    {

        TargetDispatcherGuid = FBlueprintEditorUtils::FindMemberVariableGuidByName(TargetBP.Get(), DispatcherName);

        UE_LOG(LogRefSearcher, Log, TEXT(">>> SEARCH START: Dispatcher [%s] (GUID: %s) <<<"),

            *DispatcherName.ToString(), *TargetDispatcherGuid.ToString());

    }

    else if (bSearchingFunction)

    {

        UClass* BPClass = TargetBP->GeneratedClass ? TargetBP->GeneratedClass : TargetBP->SkeletonGeneratedClass;

        if (BPClass)

        {

            for (UEdGraph* Graph : TargetBP->FunctionGraphs)

            {

                if (Graph && Graph->GetFName() == FunctionName)

                {

                    TargetFuncGuid = Graph->GraphGuid;

                    break;

                }

            }



            if (!TargetFuncGuid.IsValid())

            {

                TArray<UEdGraph*> AllGraphs;

                TargetBP->GetAllGraphs(AllGraphs);

                for (UEdGraph* Graph : AllGraphs)

                {

                    if (!Graph) continue;



                    for (UEdGraphNode* Node : Graph->Nodes)

                    {

                        if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))

                        {

                            if (CustomEvent->GetFunctionName() == FunctionName)

                            {

                                TargetFuncGuid = CustomEvent->NodeGuid;

                                break;

                            }

                        }

                    }

                    if (TargetFuncGuid.IsValid()) break;

                }

            }

        }



        UE_LOG(LogRefSearcher, Log, TEXT(">>> SEARCH START: Function [%s] (GUID: %s) <<<"),

            *FunctionName.ToString(), *TargetFuncGuid.ToString());

    }

    else

    {

        TargetVarGuid = FBlueprintEditorUtils::FindMemberVariableGuidByName(TargetBP.Get(), VariableName);

        UE_LOG(LogRefSearcher, Log, TEXT(">>> SEARCH START: Variable [%s] (GUID: %s) <<<"),

            *VariableName.ToString(), *TargetVarGuid.ToString());

    }

}



void VisualRefSearcher::QueueReferencingBlueprintAssets(TArray<FAssetData>& PriorityAssetQueue, TSet<FName>& QueuedPackages, TSet<FName>& OutReferencerPackages) const

{

    if (!TargetBP.IsValid() || !AssetRegistryModule)

    {

        return;

    }



    TSet<FName> TargetPackages;

    if (UPackage* TargetPackage = TargetBP->GetOutermost())

    {

        TargetPackages.Add(TargetPackage->GetFName());

    }

    if (TargetOwnerClass.IsValid())

    {

        if (UPackage* OwnerPackage = TargetOwnerClass->GetOutermost())

        {

            TargetPackages.Add(OwnerPackage->GetFName());

        }

    }



    for (const FName TargetPackageName : TargetPackages)

    {

        TArray<FAssetIdentifier> Referencers;

        AssetRegistryModule->Get().GetReferencers(FAssetIdentifier(TargetPackageName), Referencers);

        for (const FAssetIdentifier& Referencer : Referencers)

        {

            if (Referencer.PackageName.IsNone() || !IsSearchablePackageName(Referencer.PackageName))

            {

                continue;

            }



            OutReferencerPackages.Add(Referencer.PackageName);



            TArray<FAssetData> PackageAssets;

            AssetRegistryModule->Get().GetAssetsByPackageName(Referencer.PackageName, PackageAssets);

            for (const FAssetData& AssetData : PackageAssets)

            {

                if (AssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName())

                {

                    AddUniqueAssetToQueue(PriorityAssetQueue, QueuedPackages, AssetData);

                }

            }

        }

    }

}



void VisualRefSearcher::QueueLoadedBlueprintAssets(TArray<FAssetData>& PriorityAssetQueue, TSet<FName>& QueuedPackages) const

{

    for (TObjectIterator<UBlueprint> It; It; ++It)

    {

        UBlueprint* Blueprint = *It;

        if (!Blueprint || Blueprint == TargetBP.Get())

        {

            continue;

        }



        UPackage* Package = Blueprint->GetOutermost();

        if (!Package || !IsSearchablePackageName(Package->GetFName()))

        {

            continue;

        }



        AddUniqueAssetToQueue(PriorityAssetQueue, QueuedPackages, FAssetData(Blueprint));

    }

}



void VisualRefSearcher::QueueScopedBlueprintAssets(TArray<FAssetData>& PriorityAssetQueue, TArray<FAssetData>& FallbackAssetQueue, TSet<FName>& QueuedPackages, const TSet<FName>& ReferencerPackages, FName TargetPackagePath) const

{

    if (!AssetRegistryModule)

    {

        return;

    }



    FARFilter Filter;

    Filter.bRecursiveClasses = true;

    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());

    if (SearchScope == EVisualRefSearchScope::SameFolder && !TargetPackagePath.IsNone())

    {

        Filter.PackagePaths.Add(TargetPackagePath);

        Filter.bRecursivePaths = false;

    }

    else

    {

        Filter.bRecursivePaths = true;

    }



    TArray<FAssetData> AllBlueprintAssets;

    AssetRegistryModule->Get().GetAssets(Filter, AllBlueprintAssets);



    FName SearchName = bSearchingDispatcher ? DispatcherName : (bSearchingFunction ? FunctionName : VariableName);

    const FString SearchText = SearchName.ToString();

    int32 TextMatchCount = 0;

    int32 ReferencerMatchCount = 0;



    for (const FAssetData& AssetData : AllBlueprintAssets)

    {

        if (!IsSearchablePackageName(AssetData.PackageName)) continue;

        if (SearchScope == EVisualRefSearchScope::SameFolder && AssetData.PackagePath != TargetPackagePath) continue;



        const bool bMetadataMentionsSearch = DoesAssetMetadataMentionName(AssetData, SearchText);

        if (bMetadataMentionsSearch)

        {

            TextMatchCount++;

        }



        const bool bReferencesTargetPackage = ReferencerPackages.Contains(AssetData.PackageName);

        if (bReferencesTargetPackage)

        {

            ReferencerMatchCount++;

        }



        if (bMetadataMentionsSearch || bReferencesTargetPackage)

        {

            AddUniqueAssetToQueue(PriorityAssetQueue, QueuedPackages, AssetData);

        }

        else if (bExhaustiveSearchEnabled || SearchScope == EVisualRefSearchScope::SameFolder)

        {

            AddUniqueAssetToQueue(FallbackAssetQueue, QueuedPackages, AssetData);

        }

    }



    UE_LOG(LogRefSearcher, Log, TEXT("Scoped asset metadata matched: %d | referencer matched: %d | scoped assets: %d"),

        TextMatchCount, ReferencerMatchCount, AllBlueprintAssets.Num());

}



void VisualRefSearcher::BuildAssetQueue()

{

    bAssetQueuePrepared = true;



    FName TargetPackagePath = NAME_None;

    if (TargetBP.IsValid())

    {

        if (UPackage* TargetPackage = TargetBP->GetOutermost())

        {

            FString TargetPackageName = TargetPackage->GetName();

            int32 LastSlashIndex = INDEX_NONE;

            if (TargetPackageName.FindLastChar(TEXT('/'), LastSlashIndex))

            {

                TargetPackagePath = FName(*TargetPackageName.Left(LastSlashIndex));

            }

        }

    }



    TArray<FAssetData> PriorityAssetQueue;

    TArray<FAssetData> FallbackAssetQueue;

    TSet<FName> QueuedPackages;

    TSet<FName> ReferencerPackages;



    QueueReferencingBlueprintAssets(PriorityAssetQueue, QueuedPackages, ReferencerPackages);

    QueueLoadedBlueprintAssets(PriorityAssetQueue, QueuedPackages);



    if (SearchScope == EVisualRefSearchScope::SameFolder && !TargetPackagePath.IsNone())

    {

        PriorityAssetQueue.RemoveAll([TargetPackagePath](const FAssetData& AssetData)

        {

            return AssetData.PackagePath != TargetPackagePath;

        });

    }



    if (SearchScope == EVisualRefSearchScope::SameFolder || bExhaustiveSearchEnabled)

    {

        QueueScopedBlueprintAssets(PriorityAssetQueue, FallbackAssetQueue, QueuedPackages, ReferencerPackages, TargetPackagePath);

    }



    AssetQueue.Empty(PriorityAssetQueue.Num() + FallbackAssetQueue.Num());

    AssetQueue.Append(FallbackAssetQueue);

    AssetQueue.Append(PriorityAssetQueue);



    TotalAssetsToScan = AssetQueue.Num();

    CurrentScannedCount = 0;

    LastBatchNotifySeconds = 0.0;

    LastProgressNotifySeconds = 0.0;

    bPendingBatchNotify = false;



    UE_LOG(LogRefSearcher, Log, TEXT("Fast reference candidates: %d | Exhaustive fallback: %d | Total deep scan: %d | FullScan=%s"),

        PriorityAssetQueue.Num(), FallbackAssetQueue.Num(), AssetQueue.Num(), bExhaustiveSearchEnabled ? TEXT("true") : TEXT("false"));



    OnSearchProgress.ExecuteIfBound(0, TotalAssetsToScan);



    if (TotalAssetsToScan == 0)

    {

        FinishSearch();

    }

}



bool VisualRefSearcher::SearchBlueprintForReferences(UBlueprint* BP, UClass* TargetGenClass, UClass* TargetSkelClass, double DeadlineSeconds, bool& bOutYielded)

{

    if (!BP)

    {

        return false;

    }



    bOutYielded = false;

    bool bFoundAny = false;

    int32 ScannedNodeCount = 0;



    TArray<UEdGraph*> GraphsToCheck;

    GraphsToCheck.Append(BP->UbergraphPages);

    GraphsToCheck.Append(BP->FunctionGraphs);

    GraphsToCheck.Append(BP->MacroGraphs);



    for (UEdGraph* Graph : BP->FunctionGraphs)

    {

        if (Graph)

        {

            TArray<UEdGraph*> SubGraphs;

            Graph->GetAllChildrenGraphs(SubGraphs);

            GraphsToCheck.Append(SubGraphs);

        }

    }



    for (UEdGraph* Graph : GraphsToCheck)

    {

        if (!Graph) continue;



        for (UEdGraphNode* Node : Graph->Nodes)

        {

            if (++ScannedNodeCount > TMPerf::MaxGraphNodesPerBlueprint() || FPlatformTime::Seconds() >= DeadlineSeconds)

            {

                bOutYielded = true;

                UE_LOG(LogRefSearcher, Warning, TEXT("Reference scan yielded while scanning %s after %d graph node(s). Narrow the scope or target for exhaustive results."), *GetNameSafe(BP), ScannedNodeCount);

                return bFoundAny;

            }



            if (!Node || ProcessedNodes.Contains(Node)) continue;



            bool bIsMatch = false;

            FString MatchReason;



            if (bSearchingDispatcher)

            {

                bIsMatch = CheckDispatcherMatch(Node, BP, TargetGenClass, TargetSkelClass, &MatchReason);

            }

            else if (bSearchingFunction)

            {

                bIsMatch = CheckFunctionMatch(Node, BP, TargetGenClass, TargetSkelClass, &MatchReason);

            }

            else if (UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))

            {

                bIsMatch = CheckVariableMatch(VarNode, BP, TargetGenClass, TargetSkelClass, &MatchReason);

            }



            if (bIsMatch)

            {

                ProcessedNodes.Add(Node);

                StoreMatchReason(Node, MatchReason);

                OnRefFound.ExecuteIfBound(Node);

                bFoundAny = true;



                UE_LOG(LogRefSearcher, VeryVerbose, TEXT("Found reference in [%s] -> Graph [%s] -> Node [%s]"),

                    *BP->GetName(),

                    *Graph->GetName(),

                    *Node->GetNodeTitle(ENodeTitleType::ListView).ToString());

            }

        }

    }



    return bFoundAny;

}



void VisualRefSearcher::QueueAsyncBlueprintLoad(const FAssetData& AssetData)

{

    if (!bIsSearching || PendingLoadPackages.Contains(AssetData.PackageName))

    {

        return;

    }



    const FSoftObjectPath SoftObjectPath = AssetData.ToSoftObjectPath();

    if (!SoftObjectPath.IsValid())

    {

        return;

    }



    PendingLoadPackages.Add(AssetData.PackageName);

    TWeakPtr<VisualRefSearcher> WeakSearcher = AsShared();

    const FName PackageName = AssetData.PackageName;



    TSharedPtr<FStreamableHandle> LoadHandle = StreamableManager.RequestAsyncLoad(

        SoftObjectPath,

        FStreamableDelegate::CreateLambda([WeakSearcher, SoftObjectPath, PackageName]()

        {

            TSharedPtr<VisualRefSearcher> PinnedSearcher = WeakSearcher.Pin();

            if (!PinnedSearcher.IsValid() || !PinnedSearcher->bIsSearching)

            {

                return;

            }



            PinnedSearcher->PendingLoadPackages.Remove(PackageName);

            UObject* LoadedObject = SoftObjectPath.ResolveObject();

            if (UBlueprint* LoadedBlueprint = Cast<UBlueprint>(LoadedObject))

            {

                PinnedSearcher->LoadedBlueprintQueue.Emplace(LoadedBlueprint);

            }

            else

            {

                PinnedSearcher->CurrentScannedCount++;

            }



            PinnedSearcher->CleanupAsyncLoadHandles();

        }));



    if (LoadHandle.IsValid())

    {

        ActiveLoadHandles.Add(LoadHandle);

    }

    else

    {

        PendingLoadPackages.Remove(AssetData.PackageName);

        CurrentScannedCount++;

    }

}



bool VisualRefSearcher::CanQueueAsyncBlueprintLoad() const

{

    const bool bFastSearch = !bExhaustiveSearchEnabled;

    return PendingLoadPackages.Num() < TMPerf::MaxConcurrentAsyncBlueprintLoads(bFastSearch);

}



void VisualRefSearcher::CleanupAsyncLoadHandles()

{

    ActiveLoadHandles.RemoveAll([](const TSharedPtr<FStreamableHandle>& Handle)

    {

        return !Handle.IsValid() || Handle->HasLoadCompleted();

    });

}



bool VisualRefSearcher::HasPendingAsyncLoads() const

{

    return PendingLoadPackages.Num() > 0 || ActiveLoadHandles.Num() > 0 || LoadedBlueprintQueue.Num() > 0;

}



void VisualRefSearcher::StartSearch()

{

    if (TickerHandle.IsValid())

    {

        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

        TickerHandle.Reset();

    }



    SelfReference = AsShared();

    bIsSearching = true;
    TraceSession.Begin();
    CancellationGeneration = TMPerf::GetCancellationGeneration();



    if (!TargetBP.IsValid())

    {

        FinishSearch();

        return;

    }



    ProcessedNodes.Empty();

    ProcessedPackages.Empty();

    NodeMatchReasons.Empty();

    AssetQueue.Empty();

    LoadedBlueprintQueue.Empty();

    PendingLoadPackages.Empty();

    ActiveLoadHandles.Empty();

    bAssetQueuePrepared = false;



    InitializeSearchKeys();



    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(

        FTickerDelegate::CreateSP(this, &VisualRefSearcher::Tick),

        TMPerf::VisualSearchTickerIntervalSeconds(!bExhaustiveSearchEnabled));

}

void VisualRefSearcher::SearchTargetBlueprint()

{

    if (!TargetBP.IsValid()) return;



    UBlueprint* BP = TargetBP.Get();

    UClass* TargetGenClass = BP->GeneratedClass;

    UClass* TargetSkelClass = BP->SkeletonGeneratedClass;



    bool bYieldedBlueprintScan = false;

    SearchBlueprintForReferences(BP, TargetGenClass, TargetSkelClass, FPlatformTime::Seconds() + TMPerf::VisualSearchTickBudgetSeconds(!bExhaustiveSearchEnabled), bYieldedBlueprintScan);

    if (bYieldedBlueprintScan)

    {

        UE_LOG(LogRefSearcher, Warning, TEXT("Target Blueprint scan was time-sliced for editor responsiveness: %s"), *GetNameSafe(BP));

    }



    if (UPackage* Package = BP->GetOutermost())

    {

        ProcessedPackages.Add(Package->GetFName());

    }



    OnBatchComplete.ExecuteIfBound();

}



bool VisualRefSearcher::Tick(float DeltaTime)

{

    if (CancellationGeneration != TMPerf::GetCancellationGeneration())

    {

        CancelSearch();

        return false;

    }

    if (!bIsSearching || !TraceSession.IsActive() || !TargetBP.IsValid())

    {

        FinishSearch();

        return false;

    }



    if (!bAssetQueuePrepared)

    {

        SearchTargetBlueprint();



        if (SearchScope == EVisualRefSearchScope::CurrentBlueprint)

        {

            FinishSearch();

            return false;

        }



        BuildAssetQueue();

        return bIsSearching;

    }



    const double StartTime = FPlatformTime::Seconds();

    const bool bFastSearch = !bExhaustiveSearchEnabled;

    const double TickBudgetSeconds = TMPerf::VisualSearchTickBudgetSeconds(bFastSearch);

    const int32 BatchSize = TMPerf::VisualSearchBatchSize(bFastSearch);

    int32 ProcessedCount = 0;



    UClass* TargetGenClass = TargetBP.IsValid() ? TargetBP->GeneratedClass : nullptr;

    UClass* TargetSkelClass = TargetBP.IsValid() ? TargetBP->SkeletonGeneratedClass : nullptr;



    bool bFoundAnyThisTick = false;



    CleanupAsyncLoadHandles();



    while (LoadedBlueprintQueue.Num() > 0

        && ProcessedCount < BatchSize

        && (FPlatformTime::Seconds() - StartTime) < TickBudgetSeconds)

    {

        TStrongObjectPtr<UBlueprint> LoadedBlueprint = LoadedBlueprintQueue.Pop();

        UBlueprint* BP = LoadedBlueprint.Get();

        if (!BP)

        {

            CurrentScannedCount++;

            continue;

        }



        bool bYieldedBlueprintScan = false;

        bFoundAnyThisTick |= SearchBlueprintForReferences(BP, TargetGenClass, TargetSkelClass, StartTime + TickBudgetSeconds, bYieldedBlueprintScan);

        ProcessedCount++;

        CurrentScannedCount++;



        if (TotalAssetsToScan > 0)

        {

            const double ProgressNowSeconds = FPlatformTime::Seconds();

            if (ProgressNowSeconds - LastProgressNotifySeconds >= 0.05 || CurrentScannedCount >= TotalAssetsToScan)

            {

                LastProgressNotifySeconds = ProgressNowSeconds;

                OnSearchProgress.ExecuteIfBound(CurrentScannedCount, TotalAssetsToScan);

            }

        }

    }



    while (AssetQueue.Num() > 0

        && ProcessedCount < BatchSize

        && (FPlatformTime::Seconds() - StartTime) < TickBudgetSeconds)

    {

        FAssetData CurrentAsset = AssetQueue.Pop();



        if (ProcessedPackages.Contains(CurrentAsset.PackageName)) continue;



        UObject* AssetObj = CurrentAsset.FastGetAsset(false);

        if (!AssetObj)

        {

            if (!CanQueueAsyncBlueprintLoad())

            {

                AssetQueue.Add(CurrentAsset);

                break;

            }



            ProcessedPackages.Add(CurrentAsset.PackageName);

            QueueAsyncBlueprintLoad(CurrentAsset);

            ProcessedCount++;

            continue;

        }



        ProcessedPackages.Add(CurrentAsset.PackageName);

        UBlueprint* BP = Cast<UBlueprint>(AssetObj);

        if (!BP)

        {

            CurrentScannedCount++;

            ProcessedCount++;

            continue;

        }



        bool bYieldedBlueprintScan = false;

        bFoundAnyThisTick |= SearchBlueprintForReferences(BP, TargetGenClass, TargetSkelClass, StartTime + TickBudgetSeconds, bYieldedBlueprintScan);

        ProcessedCount++;

        CurrentScannedCount++;



        if (TotalAssetsToScan > 0)

        {

            const double ProgressNowSeconds = FPlatformTime::Seconds();

            if (ProgressNowSeconds - LastProgressNotifySeconds >= 0.05 || CurrentScannedCount >= TotalAssetsToScan)

            {

                LastProgressNotifySeconds = ProgressNowSeconds;

                OnSearchProgress.ExecuteIfBound(CurrentScannedCount, TotalAssetsToScan);

            }

        }

    }



    if (bFoundAnyThisTick)

    {

        bPendingBatchNotify = true;

        NotifyBatchCompleteThrottled(false);

    }



    if (AssetQueue.Num() == 0 && !HasPendingAsyncLoads())

    {

        FinishSearch();

        return false;

    }



    return true;

}



bool VisualRefSearcher::CheckVariableMatch(UK2Node_Variable* VarNode, UBlueprint* BP, UClass* TargetGenClass, UClass* TargetSkelClass, FString* OutMatchReason)

{

    if (!VarNode || VarNode->GetVarName() != VariableName) return false;



    FGuid NodeVarGuid = VarNode->VariableReference.GetMemberGuid();

    if (TargetVarGuid.IsValid() && NodeVarGuid.IsValid())

    {

        if (TargetVarGuid == NodeVarGuid)

        {

            SetMatchReason(OutMatchReason, TEXT("Variable GUID"));

            return true;

        }

    }

    else

    {

        UClass* SourceClass = VarNode->VariableReference.GetMemberParentClass();

        UClass* BPClass = BP ? (BP->GeneratedClass ? BP->GeneratedClass : BP->SkeletonGeneratedClass) : nullptr;



        if (SourceClass == nullptr || SourceClass == BPClass)

        {

            if (!TargetOwnerClass.IsValid() && BP == TargetBP.Get())

            {

                SetMatchReason(OutMatchReason, TEXT("Variable name in owning Blueprint"));

                return true;

            }

            if (BPClass && TargetGenClass && BPClass->IsChildOf(TargetGenClass))

            {

                SetMatchReason(OutMatchReason, TEXT("Variable owner Blueprint inheritance"));

                return true;

            }

            if (BPClass && TargetSkelClass && BPClass->IsChildOf(TargetSkelClass))

            {

                SetMatchReason(OutMatchReason, TEXT("Variable skeleton class inheritance"));

                return true;

            }

        }

        else

        {

            const bool bIsGenMatch = TargetGenClass && (TargetGenClass->IsChildOf(SourceClass) || SourceClass->IsChildOf(TargetGenClass));

            const bool bIsSkelMatch = TargetSkelClass && (TargetSkelClass->IsChildOf(SourceClass) || SourceClass->IsChildOf(TargetSkelClass));

            if (bIsGenMatch || bIsSkelMatch)

            {

                SetMatchReason(OutMatchReason, TEXT("Variable owner class relationship"));

                return true;

            }

        }

    }



    return false;

}



bool VisualRefSearcher::CheckDispatcherMatch(UEdGraphNode* Node, UBlueprint* BP, UClass* TargetGenClass, UClass* TargetSkelClass, FString* OutMatchReason)

{

    if (!Node || !BP || DispatcherName.IsNone())

    {

        return false;

    }



    const FString ActionLabel = GetDispatcherActionLabel(Node);



    if (UK2Node_BaseMCDelegate* DelegateNode = Cast<UK2Node_BaseMCDelegate>(Node))

    {

        if (DelegateNode->GetPropertyName() != DispatcherName)

        {

            return false;

        }



        const FGuid NodeGuid = DelegateNode->DelegateReference.GetMemberGuid();

        if (TargetDispatcherGuid.IsValid() && NodeGuid.IsValid() && TargetDispatcherGuid == NodeGuid)

        {

            if (OutMatchReason) *OutMatchReason = FString::Printf(TEXT("%s | Dispatcher GUID"), *ActionLabel);

            return true;

        }



        UClass* DelegateOwnerClass = nullptr;

        if (FProperty* DelegateProperty = DelegateNode->GetProperty())

        {

            DelegateOwnerClass = DelegateProperty->GetOwnerClass();

        }



        if (!DelegateOwnerClass)

        {

            DelegateOwnerClass = DelegateNode->DelegateReference.GetMemberParentClass();

        }



        if (CheckClassMatch(DelegateOwnerClass, TargetGenClass, TargetSkelClass, BP))

        {

            if (OutMatchReason) *OutMatchReason = FString::Printf(TEXT("%s | Dispatcher owner class"), *ActionLabel);

            return true;

        }



        if (!DelegateOwnerClass && BP == TargetBP.Get())

        {

            if (OutMatchReason) *OutMatchReason = FString::Printf(TEXT("%s | Owning Blueprint dispatcher"), *ActionLabel);

            return true;

        }



        return false;

    }



    if (UK2Node_DelegateSet* DelegateSetNode = Cast<UK2Node_DelegateSet>(Node))

    {

        if (DelegateSetNode->DelegatePropertyName != DispatcherName)

        {

            return false;

        }



        UClass* DelegateOwnerClass = DelegateSetNode->DelegatePropertyClass.Get();

        if (CheckClassMatch(DelegateOwnerClass, TargetGenClass, TargetSkelClass, BP) || (!DelegateOwnerClass && BP == TargetBP.Get()))

        {

            if (OutMatchReason) *OutMatchReason = FString::Printf(TEXT("%s | Dispatcher event binding"), *ActionLabel);

            return true;

        }

    }



    if (UK2Node_CreateDelegate* CreateDelegateNode = Cast<UK2Node_CreateDelegate>(Node))

    {

        for (UEdGraphPin* Pin : CreateDelegateNode->Pins)

        {

            if (!Pin)

            {

                continue;

            }



            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)

            {

                UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;

                if (!LinkedNode || LinkedNode == Node)

                {

                    continue;

                }



                FString LinkedReason;

                if (CheckDispatcherMatch(LinkedNode, BP, TargetGenClass, TargetSkelClass, &LinkedReason))

                {

                    if (OutMatchReason)

                    {

                        *OutMatchReason = FString::Printf(TEXT("Dispatcher Event | Bound function %s -> %s"),

                            *CreateDelegateNode->GetFunctionName().ToString(),

                            *LinkedReason);

                    }

                    return true;

                }

            }

        }

    }



    if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))

    {

        UEdGraph* OwningGraph = CustomEventNode->GetGraph();

        const FName CustomEventFunctionName = CustomEventNode->GetFunctionName();

        if (OwningGraph && !CustomEventFunctionName.IsNone())

        {

            for (UEdGraphNode* GraphNode : OwningGraph->Nodes)

            {

                UK2Node_CreateDelegate* CreateDelegateNode = Cast<UK2Node_CreateDelegate>(GraphNode);

                if (!CreateDelegateNode || CreateDelegateNode->GetFunctionName() != CustomEventFunctionName)

                {

                    continue;

                }



                FString CreateDelegateReason;

                if (CheckDispatcherMatch(CreateDelegateNode, BP, TargetGenClass, TargetSkelClass, &CreateDelegateReason))

                {

                    if (OutMatchReason)

                    {

                        *OutMatchReason = FString::Printf(TEXT("Dispatcher Event | Bound custom event -> %s"), *CreateDelegateReason);

                    }

                    return true;

                }

            }

        }

    }



    if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))

    {

        return DoesEventNodeMatchDispatcher(EventNode, BP, TargetGenClass, TargetSkelClass, OutMatchReason);

    }



    return false;

}



bool VisualRefSearcher::DoesEventNodeMatchDispatcher(UK2Node_Event* EventNode, UBlueprint* BP, UClass* TargetGenClass, UClass* TargetSkelClass, FString* OutMatchReason)

{

    if (!EventNode || DispatcherName.IsNone())

    {

        return false;

    }



    const FString DispatcherText = DispatcherName.ToString();

    UClass* EventOwnerClass = EventNode->EventReference.GetMemberParentClass();

    const bool bOwnerMatches = CheckClassMatch(EventOwnerClass, TargetGenClass, TargetSkelClass, BP) || (!EventOwnerClass && BP == TargetBP.Get());

    const bool bMemberNameMatches = EventNode->EventReference.GetMemberName() == DispatcherName;



    if (bMemberNameMatches && bOwnerMatches)

    {

        if (OutMatchReason) *OutMatchReason = TEXT("Dispatcher Event | Event reference");

        return true;

    }



    if (EventNode->GetFunctionName() == DispatcherName && bOwnerMatches)

    {

        if (OutMatchReason) *OutMatchReason = TEXT("Dispatcher Event | Event function name");

        return true;

    }



    UFunction* SignatureFunction = EventNode->FindEventSignatureFunction();

    if (SignatureFunction && SignatureFunction->GetName().Contains(DispatcherText) && bOwnerMatches)

    {

        if (OutMatchReason) *OutMatchReason = TEXT("Dispatcher Event | Delegate signature");

        return true;

    }



    #if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
    const FString SearchString = EventNode->GetFindReferenceSearchString(EGetFindReferenceSearchStringFlags::Legacy);
#else
    const FString SearchString = EventNode->GetFindReferenceSearchString();
#endif

    if (SearchString.Contains(DispatcherText) && (bOwnerMatches || BP == TargetBP.Get()))

    {

        if (OutMatchReason) *OutMatchReason = TEXT("Dispatcher Event | Reference search string");

        return true;

    }



    const FString ListTitle = EventNode->GetNodeTitle(ENodeTitleType::ListView).ToString();

    const FString FullTitle = EventNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();

    if ((ListTitle.Contains(DispatcherText) || FullTitle.Contains(DispatcherText)) && (bOwnerMatches || BP == TargetBP.Get()))

    {

        if (OutMatchReason) *OutMatchReason = TEXT("Dispatcher Event | Event node title");

        return true;

    }



    return false;

}



bool VisualRefSearcher::CheckFunctionMatch(UEdGraphNode* Node, UBlueprint* BP, UClass* TargetGenClass, UClass* TargetSkelClass, FString* OutMatchReason, TSet<UEdGraph*>* VisitedMacroGraphs)

{

    if (!Node || !BP)

    {

        return false;

    }



    TSet<UEdGraph*> LocalVisitedMacroGraphs;

    if (!VisitedMacroGraphs)

    {

        VisitedMacroGraphs = &LocalVisitedMacroGraphs;

    }



    FName SearchFuncName = FunctionName;



    if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))

    {

        FName NodeFuncName = CallNode->FunctionReference.GetMemberName();

        if (NodeFuncName != SearchFuncName) return false;



        if (UFunction* TargetFunction = CallNode->GetTargetFunction())

        {

            if (CheckClassMatch(TargetFunction->GetOwnerClass(), TargetGenClass, TargetSkelClass, BP))

            {

                SetMatchReason(OutMatchReason, TEXT("Function target owner class"));

                return true;

            }

        }



        UClass* RefClass = CallNode->FunctionReference.GetMemberParentClass();

        if (CheckClassMatch(RefClass, TargetGenClass, TargetSkelClass, BP))

        {

            SetMatchReason(OutMatchReason, TEXT("Function reference owner class"));

            return true;

        }



        const FGuid NodeGuid = CallNode->FunctionReference.GetMemberGuid();

        if (TargetFuncGuid.IsValid() && NodeGuid.IsValid() && TargetFuncGuid == NodeGuid)

        {

            SetMatchReason(OutMatchReason, TEXT("Function GUID"));

            return true;

        }



        if (!RefClass)

        {

            if (!TargetOwnerClass.IsValid() && BP == TargetBP.Get())

            {

                SetMatchReason(OutMatchReason, TEXT("Self function context"));

                return true;

            }



            UClass* BPClass = BP ? (BP->GeneratedClass ? BP->GeneratedClass : BP->SkeletonGeneratedClass) : nullptr;

            if (CheckClassMatch(BPClass, TargetGenClass, TargetSkelClass, BP))

            {

                SetMatchReason(OutMatchReason, TEXT("Self class context"));

                return true;

            }

        }



        if (UEdGraphPin* SelfPin = CallNode->FindPin(UEdGraphSchema_K2::PN_Self))

        {

            for (UEdGraphPin* LinkedPin : SelfPin->LinkedTo)

            {

                if (!LinkedPin || !LinkedPin->PinType.PinSubCategoryObject.IsValid())

                {

                    continue;

                }



                UClass* PinClass = Cast<UClass>(LinkedPin->PinType.PinSubCategoryObject.Get());

                if (CheckClassMatch(PinClass, TargetGenClass, TargetSkelClass, BP))

                {

                    SetMatchReason(OutMatchReason, TEXT("Connected Self pin class"));

                    return true;

                }

            }

        }



        return false;

    }



    if (UK2Node_CreateDelegate* DelegateNode = Cast<UK2Node_CreateDelegate>(Node))

    {

        if (DelegateNode->GetFunctionName() == SearchFuncName)

        {

            UClass* ScopeClass = DelegateNode->GetScopeClass();

            if (CheckClassMatch(ScopeClass, TargetGenClass, TargetSkelClass, BP))

            {

                SetMatchReason(OutMatchReason, TEXT("Delegate scope class"));

                return true;

            }



            if (!ScopeClass && BP == TargetBP.Get())

            {

                SetMatchReason(OutMatchReason, TEXT("Self delegate binding"));

                return true;

            }

        }

    }

    else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))

    {

        if (EventNode->EventReference.GetMemberName() == SearchFuncName)

        {

            UClass* EventOwner = EventNode->EventReference.GetMemberParentClass();

            if (CheckClassMatch(EventOwner, TargetGenClass, TargetSkelClass, BP))

            {

                SetMatchReason(OutMatchReason, TEXT("Event owner class"));

                return true;

            }

        }



        if (EventNode->bOverrideFunction && EventNode->GetFunctionName() == SearchFuncName)

        {

            if (!TargetOwnerClass.IsValid() && BP == TargetBP.Get())

            {

                SetMatchReason(OutMatchReason, TEXT("Override event self context"));

                return true;

            }

        }

    }

    else if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))

    {

        if (CustomEvent->GetFunctionName() == SearchFuncName)

        {

            if (!TargetOwnerClass.IsValid() && BP == TargetBP.Get())

            {

                SetMatchReason(OutMatchReason, TEXT("Custom event self context"));

                return true;

            }

            if (TargetFuncGuid.IsValid() && CustomEvent->NodeGuid == TargetFuncGuid)

            {

                SetMatchReason(OutMatchReason, TEXT("Custom event GUID"));

                return true;

            }

        }

    }

    else if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))

    {

        if (BP == TargetBP.Get())

        {

            UEdGraph* FuncGraph = EntryNode->GetGraph();

            if (FuncGraph && FuncGraph->GetFName() == SearchFuncName)

            {

                SetMatchReason(OutMatchReason, TEXT("Function definition entry"));

                return true;

            }

        }

    }

    else if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))

    {

        UEdGraph* MacroGraph = MacroNode->GetMacroGraph();

        if (MacroGraph && !VisitedMacroGraphs->Contains(MacroGraph))

        {

            VisitedMacroGraphs->Add(MacroGraph);

            for (UEdGraphNode* InnerNode : MacroGraph->Nodes)

            {

                FString InnerReason;

                if (CheckFunctionMatch(InnerNode, BP, TargetGenClass, TargetSkelClass, &InnerReason, VisitedMacroGraphs))

                {

                    if (OutMatchReason)

                    {

                        *OutMatchReason = InnerReason.IsEmpty()

                            ? TEXT("Macro contains function reference")

                            : FString::Printf(TEXT("Macro contains: %s"), *InnerReason);

                    }

                    return true;

                }

            }

        }

    }



    const FText NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle);

    if (NodeTitle.ToString().Contains(SearchFuncName.ToString()))

    {

        UE_LOG(LogRefSearcher, VeryVerbose, TEXT("Potential match via node title text: %s"), *NodeTitle.ToString());

    }



    return false;

}



bool VisualRefSearcher::CheckClassMatch(UClass* TestClass, UClass* TargetGenClass, UClass* TargetSkelClass, UBlueprint* CurrentBP)

{

    if (!TestClass) return false;



    if (TargetOwnerClass.IsValid())

    {

        UClass* OwnerClass = TargetOwnerClass.Get();

        if (OwnerClass && TestClass == OwnerClass) return true;

        if (OwnerClass && TestClass->IsChildOf(OwnerClass)) return true;

        if (OwnerClass && OwnerClass->IsChildOf(TestClass)) return true;

        if (OwnerClass && TestClass->IsChildOf(UInterface::StaticClass()) && OwnerClass->ImplementsInterface(TestClass)) return true;



        if (OwnerClass)

        {

            FString TestClassName = TestClass->GetName();

            FString OwnerClassName = OwnerClass->GetName();

            TestClassName.RemoveFromStart(TEXT("SKEL_"));

            OwnerClassName.RemoveFromStart(TEXT("SKEL_"));

            TestClassName.RemoveFromEnd(TEXT("_C"));

            OwnerClassName.RemoveFromEnd(TEXT("_C"));

            if (TestClassName == OwnerClassName) return true;

        }

    }



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

    }



    if (TargetBP.IsValid())

    {

        UPackage* TargetPackage = TargetBP->GetOutermost();

        UPackage* TestPackage = TestClass->GetOutermost();

        if (TargetPackage && TestPackage && TargetPackage == TestPackage) return true;

    }



    FString TestClassName = TestClass->GetName();

    FString TargetGenName = TargetGenClass ? TargetGenClass->GetName() : TEXT("");

    FString TargetSkelName = TargetSkelClass ? TargetSkelClass->GetName() : TEXT("");



    TestClassName.RemoveFromEnd(TEXT("_C"));

    TargetGenName.RemoveFromEnd(TEXT("_C"));

    TargetSkelName.RemoveFromEnd(TEXT("_C"));



    if (!TargetGenName.IsEmpty() && TestClassName == TargetGenName) return true;

    if (!TargetSkelName.IsEmpty() && TestClassName == TargetSkelName) return true;



    return false;

}






