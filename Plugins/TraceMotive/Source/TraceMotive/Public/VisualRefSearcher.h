// Source/TraceMotive/Public/VisualRefSearcher.h

#pragma once

#include "TMTraceSession.h"

#include "CoreMinimal.h"

#include "AssetRegistry/AssetData.h"

#include "Containers/Ticker.h"

#include "Engine/StreamableManager.h"

#include "UObject/StrongObjectPtr.h"



class UBlueprint;

class UEdGraph;

class UEdGraphNode;

class UK2Node_Variable;

class UK2Node_Event;

class UClass;

class FAssetRegistryModule;



DECLARE_DELEGATE(FOnBatchCompleteDelegate);

DECLARE_DELEGATE(FOnSearchFinishedDelegate);

DECLARE_DELEGATE_OneParam(FOnRefFoundDelegate, UEdGraphNode*);

DECLARE_DELEGATE_TwoParams(FOnSearchProgress, int32 /*Current*/, int32 /*Total*/);



enum class EVisualRefSearchScope : uint8

{

    CurrentBlueprint,

    SameFolder,

    ProjectContent

};



class VisualRefSearcher : public TSharedFromThis<VisualRefSearcher>

{

public:

    VisualRefSearcher(UBlueprint* InTargetBP, FName InVarOrFuncName, bool bIsFunction = false, UClass* InTargetOwnerClass = nullptr, bool bIsDispatcher = false);

    ~VisualRefSearcher();



    void StartSearch();

    void CancelSearch();

    void SetSearchScope(EVisualRefSearchScope InSearchScope);

    void SetExhaustiveSearchEnabled(bool bInEnabled);

    FString GetMatchReason(UEdGraphNode* Node) const;



    FOnRefFoundDelegate OnRefFound;

    FOnBatchCompleteDelegate OnBatchComplete;

    FOnSearchFinishedDelegate OnSearchFinished;

    FOnSearchProgress OnSearchProgress;

    FSimpleDelegate OnRefFound_OneParam; // (기존 코드에 맞춰 조정)



private:

    bool Tick(float DeltaTime);

    void FinishSearch();

    void InitializeSearchKeys();

    void BuildAssetQueue();

    void QueueReferencingBlueprintAssets(TArray<FAssetData>& PriorityAssetQueue, TSet<FName>& QueuedPackages, TSet<FName>& OutReferencerPackages) const;

    void QueueLoadedBlueprintAssets(TArray<FAssetData>& PriorityAssetQueue, TSet<FName>& QueuedPackages) const;

    void QueueScopedBlueprintAssets(TArray<FAssetData>& PriorityAssetQueue, TArray<FAssetData>& FallbackAssetQueue, TSet<FName>& QueuedPackages, const TSet<FName>& ReferencerPackages, FName TargetPackagePath) const;

    bool SearchBlueprintForReferences(UBlueprint* BP, UClass* TargetGenClass, UClass* TargetSkelClass, double DeadlineSeconds, bool& bOutYielded);

    void QueueAsyncBlueprintLoad(const FAssetData& AssetData);

    bool CanQueueAsyncBlueprintLoad() const;

    void CleanupAsyncLoadHandles();

    bool HasPendingAsyncLoads() const;



    // 변수 매칭 체크

    bool CheckVariableMatch(UK2Node_Variable* VarNode, UBlueprint* BP, UClass* TargetGenClass, UClass* TargetSkelClass, FString* OutMatchReason = nullptr);



    bool CheckClassMatch(UClass* TestClass, UClass* TargetGenClass, UClass* TargetSkelClass, UBlueprint* CurrentBP);



    // 함수 매칭 체크

    bool CheckFunctionMatch(UEdGraphNode* Node, UBlueprint* BP, UClass* TargetGenClass, UClass* TargetSkelClass, FString* OutMatchReason = nullptr, TSet<UEdGraph*>* VisitedMacroGraphs = nullptr);



    // 이벤트 디스패처 액션(Call/Bind/Unbind/Event/Assign) 매칭 체크

    bool CheckDispatcherMatch(UEdGraphNode* Node, UBlueprint* BP, UClass* TargetGenClass, UClass* TargetSkelClass, FString* OutMatchReason = nullptr);

    bool DoesEventNodeMatchDispatcher(UK2Node_Event* EventNode, UBlueprint* BP, UClass* TargetGenClass, UClass* TargetSkelClass, FString* OutMatchReason = nullptr);



    void SearchTargetBlueprint();

    void StoreMatchReason(UEdGraphNode* Node, const FString& Reason);

    void NotifyBatchCompleteThrottled(bool bForce = false);



    TWeakObjectPtr<UBlueprint> TargetBP;

    TWeakObjectPtr<UClass> TargetOwnerClass;

    FName VariableName;

    FName FunctionName;

    FName DispatcherName;

    bool bSearchingFunction;

    bool bSearchingDispatcher;

    FGuid TargetVarGuid;

    FGuid TargetFuncGuid;

    FGuid TargetDispatcherGuid;



    TArray<FAssetData> AssetQueue;

    TArray<TStrongObjectPtr<UBlueprint>> LoadedBlueprintQueue;

    TSet<TWeakObjectPtr<UEdGraphNode>> ProcessedNodes;

    TSet<FName> ProcessedPackages;

    TSet<FName> PendingLoadPackages;

    TMap<FGuid, FString> NodeMatchReasons;

    EVisualRefSearchScope SearchScope = EVisualRefSearchScope::ProjectContent;

    bool bExhaustiveSearchEnabled = false;

    bool bAssetQueuePrepared = false;



    FAssetRegistryModule* AssetRegistryModule;

    FStreamableManager StreamableManager;

    TArray<TSharedPtr<FStreamableHandle>> ActiveLoadHandles;

    FTSTicker::FDelegateHandle TickerHandle;

    TSharedPtr<VisualRefSearcher> SelfReference;

    bool bIsSearching = false;
    FTMTraceSession TraceSession;
    uint64 CancellationGeneration = 0;



    int32 TotalAssetsToScan = 0;

    int32 CurrentScannedCount = 0;

    double LastBatchNotifySeconds = 0.0;

    double LastProgressNotifySeconds = 0.0;

    bool bPendingBatchNotify = false;

};



