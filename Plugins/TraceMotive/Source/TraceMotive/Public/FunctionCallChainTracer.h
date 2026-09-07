// Source/TraceMotive/Public/FunctionCallChainTracer.h

#pragma once

#include "TMTraceSession.h"

#include "CoreMinimal.h"

#include "Containers/Ticker.h"

#include "UObject/StrongObjectPtr.h"

#include "UObject/WeakObjectPtr.h"

// #include "YourStructHeader.h" 

// ==============================================================================

// ==============================================================================

enum class ECallChainRelationKind : uint8

{

    DirectCall,

    DelegateBinding,

    MacroExpansion,

    EntryPoint,

    Target,

    CppSourceCall,

    Unknown

};

struct FCallChainNode

{

    FString FunctionName;

    FString GraphName;

    FString AssetName;

    FString AssetPath;

    FString TargetObjectName;

    FString TargetClassName;

    FGuid NodeGuid;

    TWeakObjectPtr<class UEdGraphNode> SourceNode;

    TWeakObjectPtr<class UBlueprint> OwnerBlueprint;

    bool bIsTargetFunction = false;

    bool bIsEvent = false;

    ECallChainRelationKind RelationKind = ECallChainRelationKind::Unknown;

    FString MatchReason;

    bool bIsCppSource = false;

    FString SourceFilePath;

    int32 SourceLine = 0;

    int32 SourceColumn = 0;

    FString SourceSnippet;

};

struct FCallChainPath

{

    TArray<FCallChainNode> Nodes;

    bool bTruncated = false;

    bool bContainsCycle = false;

    FString TerminationReason;

};

struct FCallChainResult

{

    FName TargetFunctionName;

    TWeakObjectPtr<UBlueprint> TargetBlueprint;

    TArray<FCallChainPath> Paths;

    int32 ScannedPackageCount = 0;

    int32 DirectCallerCount = 0;

    int32 TruncatedPathCount = 0;

    int32 CyclicPathCount = 0;

    int32 CppSourceCallerCount = 0;

};

// ==============================================================================

// ==============================================================================

enum class ETraceState

{

    Idle,

    FindingReferencers,

    ScanningPackages,

    ProcessingCallChain,

    ScanningCppSources,

    Complete

};

// ==============================================================================

// ==============================================================================

DECLARE_DELEGATE_ThreeParams(FOnTraceProgress, FString /*Message*/, int32 /*Current*/, int32 /*Total*/);

DECLARE_DELEGATE_OneParam(FOnTraceComplete, const FCallChainResult&);

// ==============================================================================

// ==============================================================================

class TRACEMOTIVE_API FunctionCallChainTracer : public TSharedFromThis<FunctionCallChainTracer>

{

public:

    FunctionCallChainTracer(UBlueprint* InTargetBP, FName InTargetFunction, UClass* InTargetOwnerClass = nullptr);

    ~FunctionCallChainTracer();

    void StartTrace();

    void CancelTrace();

    FOnTraceProgress OnTraceProgress;

    FOnTraceComplete OnTraceComplete;

private:

    // 硫붿씤 猷⑦봽 (FTSTicker)

    bool Tick(float DeltaTime);

    // ---------------------------------------------------------

    // ---------------------------------------------------------

    bool TickState_FindingReferencers();

    bool TickState_ScanningPackages(double StartTime, double TimeLimit);

    bool TickState_ProcessingCallChain(double StartTime, double TimeLimit);

    bool TickState_ScanningCppSources(double StartTime, double TimeLimit);

    void AddUniquePath(const FCallChainPath& NewPath);

    void FindCallersInMemory(FName TargetFunc, UBlueprint* TargetBP, UClass* TargetOwnerClass, const FGuid& TargetGuid, TArray<UEdGraphNode*>& OutCallers);

    bool FindCallersInGraph(UEdGraph* Graph, FName TargetFunc, UBlueprint* TargetBP, UClass* TargetOwnerClass, const FGuid& TargetGuid, TArray<UEdGraphNode*>& OutCallers, double DeadlineSeconds, int32& InOutScannedNodeCount);

    bool DoesNodeReferenceFunction(class UEdGraphNode* Node, FName TargetFunc, UBlueprint* TargetBP,

        UClass* TargetOwnerClass, const FGuid& TargetGuid,

        TSet<class UEdGraph*>* VisitedMacroGraphs = nullptr);

    bool IsMatching(class UK2Node_CallFunction* CallNode, FName TargetFunc, UBlueprint* TargetBP,

        UClass* TargetOwnerClass, const FGuid& TargetGuid);

    FCallChainPath CreatePathStruct(const TArray<FCallChainNode>& Nodes);

    // ---------------------------------------------------------

    // ---------------------------------------------------------

    bool SearchCallersInPackage(UPackage* Package, TArray<class UEdGraphNode*>& OutCallers, double DeadlineSeconds);

    void InitializeProcessQueue();

    void FinalizeTrace();

    void BuildCppSourceFileQueue();

    bool ScanCppSourceFile(const FString& SourceFilePath, double DeadlineSeconds);

    void AddCppSourceCallerPath(const FString& SourceFilePath, int32 LineNumber, int32 ColumnNumber, const FString& LineText);

    void KeepObjectAlive(UObject* Object);

    void ReleaseScanObjectReferences();

    bool IsEventNode(class UEdGraphNode* Node);

    FCallChainNode CreateChainNode(class UEdGraphNode* Node, UBlueprint* BP, bool bIsTarget);

private:

    TWeakObjectPtr<UBlueprint> TargetBlueprint;

    TWeakObjectPtr<UClass> TargetFunctionOwnerClass;

    FName TargetFunctionName;

    FGuid TargetFunctionGuid;

    FCallChainResult Result;

    bool bIsTracing;
    bool bFindingReferencersLogged = false;
    FTMTraceSession TraceSession;

    int32 ProcessedCount;

    int32 TotalCount;

    ETraceState CurrentState;

    TSharedPtr<FunctionCallChainTracer> SelfReference;

    FTSTicker::FDelegateHandle TickerHandle;

    // ---------------------------------------------------------

    // ---------------------------------------------------------

    TArray<FName> PackagesToScan;

    int32 LoadedPackageIndex;       // Current package index while scanning.

    TArray<TStrongObjectPtr<UObject>> LoadedObjectsToKeepAlive;

    TArray<class UEdGraphNode*> InitialCallers;

    TArray<TPair<class UEdGraphNode*, TArray<FCallChainNode>>> ProcessQueue;

    TMap<FName, TArray<TWeakObjectPtr<UEdGraphNode>>> CachedCallerMap;

    TArray<FString> CppSourceFilesToScan;

    int32 CppSourceFileIndex = 0;

    TSet<FString> AddedCppSourceCallKeys;

    void BuildCallerCache();

    FGuid GetCallableGuidForGraph(class UEdGraph* Graph) const;

    void FindRootExecSourceNodes(class UEdGraphNode* StartNode, TArray<class UEdGraphNode*>& OutRootNodes) const;

};
