#include "TMTraceSession.h"
#include "TMInvestigationSession.h"
#include "TMLocalization.h"
#include "TMPerformanceGuard.h"
#include "TMSettings.h"
#include "TMSupportBundle.h"
#include "VisualRefSearcher.h"
#include "FunctionCallChainTracer.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMTraceSessionLifecycleTest,
    "TraceMotive.TraceSession.Lifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMTraceSessionLifecycleTest::RunTest(const FString&)
{
    FTMTraceSession Session;
    TestFalse(TEXT("A new session is inactive"), Session.IsActive());

    Session.Begin();
    const uint64 FirstGeneration = Session.GetGeneration();
    TestTrue(TEXT("Begin activates the session"), Session.IsActive());
    TestTrue(TEXT("First generation is non-zero"), FirstGeneration > 0);

    TestTrue(TEXT("First UI refresh is allowed"), Session.ShouldRefreshUi(10.0, 0.2));
    TestFalse(TEXT("Refresh is throttled inside the interval"), Session.ShouldRefreshUi(10.1, 0.2));
    TestTrue(TEXT("Refresh is allowed after the interval"), Session.ShouldRefreshUi(10.2, 0.2));

    Session.Cancel();
    TestFalse(TEXT("Cancel deactivates the session"), Session.IsActive());
    TestTrue(TEXT("Cancel invalidates queued work generation"), Session.GetGeneration() > FirstGeneration);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMVisualRefResumeTest,
    "TraceMotive.Search.VisualReferenceResumesAfterBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMVisualRefResumeTest::RunTest(const FString&)
{
    UBlueprint* Blueprint = NewObject<UBlueprint>(GetTransientPackage());
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint);
    Blueprint->UbergraphPages.Add(Graph);
    for (int32 Index = 0; Index < 8; ++Index) Graph->AddNode(NewObject<UEdGraphNode>(Graph));

    TSharedRef<VisualRefSearcher> Searcher = MakeShared<VisualRefSearcher>(Blueprint, TEXT("MissingVariable"));
    bool bYielded = false;
    Searcher->SearchBlueprintForReferences(Blueprint, nullptr, nullptr, 0.0, bYielded);
    TestTrue(TEXT("Expired budget yields the Blueprint"), bYielded);
    TestTrue(TEXT("Yielded Blueprint stores a resume position"), Searcher->BlueprintResumeNodeOffsets.Contains(Blueprint));

    bYielded = false;
    Searcher->SearchBlueprintForReferences(Blueprint, nullptr, nullptr, TNumericLimits<double>::Max(), bYielded);
    TestFalse(TEXT("A later slice completes the Blueprint"), bYielded);
    TestFalse(TEXT("Completed Blueprint clears its resume position"), Searcher->BlueprintResumeNodeOffsets.Contains(Blueprint));
    UEdGraph* Nested = NewObject<UEdGraph>(Graph);
    Graph->SubGraphs.Add(Nested);
    UK2Node_VariableGet* Read = NewObject<UK2Node_VariableGet>(Nested);
    Read->VariableReference.SetSelfMember(TEXT("Health"));
    Nested->AddNode(Read);
    UK2Node_VariableSet* Write = NewObject<UK2Node_VariableSet>(Graph);
    Write->VariableReference.SetSelfMember(TEXT("Health"));
    Graph->AddNode(Write);
    TSharedRef<VisualRefSearcher> Actual = MakeShared<VisualRefSearcher>(Blueprint, TEXT("Health"));
    TArray<UEdGraphNode*> Matches;
    Actual->OnRefFound.BindLambda([&Matches](UEdGraphNode* Node) { Matches.Add(Node); });
    Actual->SearchBlueprintForReferences(Blueprint, nullptr, nullptr, TNumericLimits<double>::Max(), bYielded);
    TestEqual(TEXT("Finds both actual read and write references including a nested graph"), Matches.Num(), 2);
    TestTrue(TEXT("Read result retains its navigation target"), Matches.Contains(Read));
    TestTrue(TEXT("Write result retains its navigation target"), Matches.Contains(Write));
    UBlueprint* Other = NewObject<UBlueprint>(GetTransientPackage());
    UEdGraph* OtherGraph = NewObject<UEdGraph>(Other);
    Other->UbergraphPages.Add(OtherGraph);
    UK2Node_VariableGet* Unrelated = NewObject<UK2Node_VariableGet>(OtherGraph);
    Unrelated->VariableReference.SetSelfMember(TEXT("Health"));
    OtherGraph->AddNode(Unrelated);
    Actual->SearchBlueprintForReferences(Other, nullptr, nullptr, TNumericLimits<double>::Max(), bYielded);
    TestEqual(TEXT("Same variable name in unrelated Blueprint is excluded"), Matches.Num(), 2);
    Actual->SearchBlueprintForReferences(Blueprint, nullptr, nullptr, TNumericLimits<double>::Max(), bYielded);
    TestEqual(TEXT("Rescanning does not duplicate results"), Matches.Num(), 2);

    const FString CppFixture = TEXT(
        "int32 Health = 100;\n"
        "Health -= Damage;\n"
        "const int32 Snapshot = Health;\n"
        "int32 HealthBonus = 5;\n"
        "// Health = 0;\n"
        "const TCHAR* Label = TEXT(\"Health\");\n"
        "++Health;\n");
    TArray<FCppVariableReference> CppReferences;
    VisualRefSearcher::FindCppVariableReferencesInText(CppFixture, TEXT("Health"), CppReferences);
    TestEqual(TEXT("C++ variable scan finds exact identifiers and ignores comments, strings, and longer names"), CppReferences.Num(), 4);
    TestTrue(TEXT("C++ declaration assignment is classified as a write"), CppReferences.IsValidIndex(0) && CppReferences[0].bIsWrite);
    TestTrue(TEXT("C++ compound assignment is classified as a write"), CppReferences.IsValidIndex(1) && CppReferences[1].bIsWrite);
    TestTrue(TEXT("C++ read is classified as a read"), CppReferences.IsValidIndex(2) && !CppReferences[2].bIsWrite);
    TestTrue(TEXT("C++ prefix increment is classified as a write"), CppReferences.IsValidIndex(3) && CppReferences[3].bIsWrite);
    VisualRefSearcher::FindCppVariableReferencesInText(CppFixture, TEXT("Health"), CppReferences, 2);
    TestEqual(TEXT("C++ parser respects the result limit within a source file"), CppReferences.Num(), 2);

    // StartSearch resolves the variable through the Blueprint parent chain.
    Blueprint->ParentClass = UObject::StaticClass();
    TSharedRef<VisualRefSearcher> CurrentScope = MakeShared<VisualRefSearcher>(Blueprint, TEXT("Health"));
    CurrentScope->SetSearchScope(EVisualRefSearchScope::CurrentBlueprint);
    CurrentScope->SetCppSourceSearchEnabled(true);
    CurrentScope->StartSearch();
    TestEqual(TEXT("Current Blueprint does not queue project C++ source"), CurrentScope->CppSourceFilesToScan.Num(), 0);
    CurrentScope->CancelSearch();

    TSharedRef<VisualRefSearcher> FolderScope = MakeShared<VisualRefSearcher>(Blueprint, TEXT("Health"));
    FolderScope->SetSearchScope(EVisualRefSearchScope::SameFolder);
    FolderScope->SetCppSourceSearchEnabled(true);
    FolderScope->StartSearch();
    TestEqual(TEXT("Same Folder does not queue project C++ source"), FolderScope->CppSourceFilesToScan.Num(), 0);
    FolderScope->CancelSearch();
    TSharedRef<VisualRefSearcher> BlueprintOnly = MakeShared<VisualRefSearcher>(Blueprint, TEXT("Health"));
    BlueprintOnly->StartSearch();
    TestEqual(TEXT("Default Project Content search does not queue C++ candidates"), BlueprintOnly->CppSourceFilesToScan.Num(), 0);
    BlueprintOnly->CancelSearch();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMVisualRefCurrentBlueprintCompletionTest,
    "TraceMotive.Search.CurrentBlueprintCompletesAllSlices",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMVisualRefCurrentBlueprintCompletionTest::RunTest(const FString&)
{
    UBlueprint* Blueprint = NewObject<UBlueprint>();
    Blueprint->ParentClass = UObject::StaticClass();
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint);
    Blueprint->UbergraphPages.Add(Graph);
    const int32 FillerCount = TMPerf::MaxGraphNodesPerBlueprint() + 1;
    for (int32 Index = 0; Index < FillerCount; ++Index) Graph->AddNode(NewObject<UEdGraphNode>(Graph));
    UK2Node_VariableGet* Tail = NewObject<UK2Node_VariableGet>(Graph);
    Tail->VariableReference.SetSelfMember(TEXT("TailValue"));
    Graph->AddNode(Tail);

    TSharedRef<VisualRefSearcher> Searcher = MakeShared<VisualRefSearcher>(Blueprint, TEXT("TailValue"));
    Searcher->SetSearchScope(EVisualRefSearchScope::CurrentBlueprint);
    int32 MatchCount = 0;
    bool bFinished = false;
    Searcher->OnRefFound.BindLambda([&](UEdGraphNode* Node) { if (Node == Tail) ++MatchCount; });
    Searcher->OnSearchFinished.BindLambda([&]() { bFinished = true; });
    Searcher->StartSearch();
    const double Started = FPlatformTime::Seconds();
    Searcher->Tick(0.0f);
    double MaxTickMs = (FPlatformTime::Seconds() - Started) * 1000.0;
    TestTrue(TEXT("Current Blueprint remains active after its first bounded slice"), Searcher->bIsSearching);
    TestFalse(TEXT("A partial Blueprint scan does not emit completion"), bFinished);
    int32 TickCount = 1;
    while (Searcher->bIsSearching && TickCount++ < 10000)
    {
        const double TickStarted = FPlatformTime::Seconds();
        Searcher->Tick(0.0f);
        MaxTickMs = FMath::Max(MaxTickMs, (FPlatformTime::Seconds() - TickStarted) * 1000.0);
    }
    TestFalse(TEXT("All Blueprint slices complete"), Searcher->bIsSearching);
    TestTrue(TEXT("Completion is emitted after all slices"), bFinished);
    TestEqual(TEXT("The final graph node is found exactly once"), MatchCount, 1);
    AddInfo(FString::Printf(TEXT("Blueprint fixture: %d nodes, %d ticks, %.3f ms scan loop, %.3f ms max continuation tick (no Slate rendering)."),
        FillerCount + 1, TickCount, (FPlatformTime::Seconds() - Started) * 1000.0, MaxTickMs));
    if (Searcher->bIsSearching) Searcher->CancelSearch();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMVisualRefSymbolIdentityTest,
    "TraceMotive.Search.BlueprintSymbolIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMVisualRefSymbolIdentityTest::RunTest(const FString&)
{
    const FString Root = TEXT("/Temp/TraceMotiveIdentity_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    auto MakeBlueprint = [&Root](const TCHAR* Folder)
    {
        UPackage* Package = CreatePackage(*(Root / Folder));
        UBlueprint* BP = NewObject<UBlueprint>(Package, TEXT("BP_SameName"));
        BP->ParentClass = UObject::StaticClass();
        UBlueprintGeneratedClass* Class = NewObject<UBlueprintGeneratedClass>(Package, TEXT("BP_SameName_C"));
        Class->SetSuperStruct(UObject::StaticClass());
        Class->ClassGeneratedBy = BP;
        BP->GeneratedClass = Class;
        UEdGraph* Graph = NewObject<UEdGraph>(BP);
        Graph->Schema = UEdGraphSchema_K2::StaticClass();
        BP->UbergraphPages.Add(Graph);
        return BP;
    };
    UBlueprint* Target = MakeBlueprint(TEXT("A"));
    UBlueprint* Other = MakeBlueprint(TEXT("B"));
    UClass* Owner = Target->GeneratedClass;
    UClass* OtherOwner = Other->GeneratedClass;
    UEdGraph* Graph = Target->UbergraphPages[0];
    TSharedRef<VisualRefSearcher> Functions = MakeShared<VisualRefSearcher>(Target, TEXT("DoWork"), true, Owner);
    TestTrue(TEXT("Searches are exhaustive by default"), Functions->bExhaustiveSearchEnabled);
    UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(Graph);
    Call->FunctionReference.SetExternalMember(TEXT("DoWork"), Owner);
    TestTrue(TEXT("A function reference to its owner matches"), Functions->CheckFunctionMatch(Call, Target, Owner, nullptr));
    Call->FunctionReference.SetExternalMember(TEXT("DoWork"), OtherOwner);
    TestFalse(TEXT("A same-named class in another package does not match"), Functions->CheckFunctionMatch(Call, Target, Owner, nullptr));
    Functions->TargetFuncGuid = FGuid::NewGuid();
    Call->FunctionReference.SetExternalMember(TEXT("DoWork"), OtherOwner, Functions->TargetFuncGuid);
    TestFalse(TEXT("A copied function GUID does not override owner identity"), Functions->CheckFunctionMatch(Call, Other, Owner, nullptr));
    UBlueprintGeneratedClass* Skeleton = NewObject<UBlueprintGeneratedClass>(Target->GetOutermost(), TEXT("SKEL_BP_SameName_C"));
    Skeleton->SetSuperStruct(UObject::StaticClass());
    Skeleton->ClassGeneratedBy = Target;
    TestTrue(TEXT("Skeleton class identity is preserved"), Functions->CheckClassMatch(Skeleton, Owner, nullptr, Target));
    UBlueprintGeneratedClass* Child = NewObject<UBlueprintGeneratedClass>();
    Child->SetSuperStruct(Owner);
    TestTrue(TEXT("Inherited function owner relationships are preserved"), Functions->CheckClassMatch(Child, Owner, nullptr, Target));

    UEdGraph* MacroGraph = NewObject<UEdGraph>(Target);
    MacroGraph->Schema = UEdGraphSchema_K2::StaticClass();
    Call = NewObject<UK2Node_CallFunction>(MacroGraph);
    Call->FunctionReference.SetExternalMember(TEXT("DoWork"), Owner);
    MacroGraph->AddNode(Call);
    UK2Node_MacroInstance* Macro = NewObject<UK2Node_MacroInstance>(Graph);
    Macro->SetMacroGraph(MacroGraph);
    TestTrue(TEXT("A macro containing the function reference matches"), Functions->CheckFunctionMatch(Macro, Target, Owner, nullptr));

    UK2Node_CustomEvent* Event = NewObject<UK2Node_CustomEvent>(Graph);
    Event->CustomFunctionName = TEXT("DoWork");
    Graph->AddNode(Event);
    TestTrue(TEXT("A custom event definition matches with an explicit owner"), Functions->CheckFunctionMatch(Event, Target, Owner, nullptr));
    TestFalse(TEXT("An unrelated Blueprint's custom event is excluded"), Functions->CheckFunctionMatch(Event, Other, Owner, nullptr));

    TSharedRef<VisualRefSearcher> Dispatchers = MakeShared<VisualRefSearcher>(Target, TEXT("Changed"), false, Owner, true);
    UK2Node_CallDelegate* Dispatch = NewObject<UK2Node_CallDelegate>(Graph);
    Dispatch->DelegateReference.SetExternalMember(TEXT("Changed"), Owner);
    TestTrue(TEXT("Dispatcher call uses its owner's identity"), Dispatchers->CheckDispatcherMatch(Dispatch, Target, Owner, nullptr));
    Dispatch->DelegateReference.SetExternalMember(TEXT("Changed"), OtherOwner);
    TestFalse(TEXT("Same-named dispatcher on an unrelated class is excluded"), Dispatchers->CheckDispatcherMatch(Dispatch, Target, Owner, nullptr));
    Dispatchers->TargetDispatcherGuid = FGuid::NewGuid();
    Dispatch->DelegateReference.SetExternalMember(TEXT("Changed"), OtherOwner, Dispatchers->TargetDispatcherGuid);
    TestFalse(TEXT("A copied dispatcher GUID does not override owner identity"), Dispatchers->CheckDispatcherMatch(Dispatch, Other, Owner, nullptr));
    Event->CustomFunctionName = TEXT("ChangedExtra");
    TestFalse(TEXT("An unbound event title is not dispatcher evidence"), Dispatchers->CheckDispatcherMatch(Event, Target, Owner, nullptr));

    const FGuid LocalGuid = FGuid::NewGuid();
    UK2Node_VariableGet* Local = NewObject<UK2Node_VariableGet>(Graph);
    Local->VariableReference.SetLocalMember(TEXT("Value"), FString(TEXT("FunctionA")), LocalGuid);
    TSharedRef<VisualRefSearcher> Variables = MakeShared<VisualRefSearcher>(Target, TEXT("Value"));
    Variables->TargetVarGuid = FGuid::NewGuid();
    UK2Node_VariableGet* Copied = NewObject<UK2Node_VariableGet>(Other->UbergraphPages[0]);
    Copied->VariableReference.SetSelfMember(TEXT("Value"), Variables->TargetVarGuid);
    TestFalse(TEXT("A copied member GUID does not match an unrelated Blueprint"), Variables->CheckVariableMatch(Copied, Other, Owner, nullptr));
    TestFalse(TEXT("A local variable does not match a same-named member"), Variables->CheckVariableMatch(Local, Target, Owner, nullptr));
    Variables->SetLocalVariableScope(TEXT("FunctionA"), LocalGuid);
    TestTrue(TEXT("A local variable matches its selected function scope"), Variables->CheckVariableMatch(Local, Target, Owner, nullptr));
    TestFalse(TEXT("A local variable from another Blueprint is excluded"), Variables->CheckVariableMatch(Local, Other, Owner, nullptr));
    Local->VariableReference.SetLocalMember(TEXT("Value"), FString(TEXT("FunctionB")), FGuid::NewGuid());
    TestFalse(TEXT("A same-named local in another function is excluded"), Variables->CheckVariableMatch(Local, Target, Owner, nullptr));
    Local->VariableReference.SetSelfMember(TEXT("Value"));
    TestFalse(TEXT("A member does not match the selected local variable"), Variables->CheckVariableMatch(Local, Target, Owner, nullptr));

    // Native properties may be referenced as self members in sibling Blueprint classes.
    Owner->SetSuperStruct(AActor::StaticClass());
    OtherOwner->SetSuperStruct(AActor::StaticClass());
    UK2Node_VariableGet* Native = NewObject<UK2Node_VariableGet>(Other->UbergraphPages[0]);
    Native->VariableReference.SetSelfMember(TEXT("Tags"));
    TSharedRef<VisualRefSearcher> NativeVariables = MakeShared<VisualRefSearcher>(Target, TEXT("Tags"), false, AActor::StaticClass());
    TestTrue(TEXT("Native property references in sibling Blueprints share the declaring owner"),
        NativeVariables->CheckVariableMatch(Native, Other, Owner, nullptr));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMVisualRefPerformanceTest,
    "TraceMotive.Performance.VisualReferenceCpp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMVisualRefPerformanceTest::RunTest(const FString&)
{
    FString FixtureRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/TraceMotivePerfFixture"));
    FPaths::NormalizeFilename(FixtureRoot);
    TArray<FString> FixtureFiles;
    IFileManager::Get().FindFilesRecursive(FixtureFiles, *FixtureRoot, TEXT("*.cpp"), true, false, false);
    if (!TestTrue(TEXT("Generate the C++ source fixture before running this opt-in performance test"), FixtureFiles.Num() > 0))
    {
        return false;
    }

    UBlueprint* Blueprint = NewObject<UBlueprint>(GetTransientPackage());
    Blueprint->ParentClass = UObject::StaticClass();
    TSharedRef<VisualRefSearcher> Searcher = MakeShared<VisualRefSearcher>(Blueprint, TEXT("TMPerfValue"));
    Searcher->SetSearchScope(EVisualRefSearchScope::ProjectContent);
    int32 FixtureMatches = 0;
    Searcher->OnCppVariableRefFound.BindLambda([&FixtureMatches, FixtureRoot](const FCppVariableReference& Reference)
    {
        if (Reference.SourceFilePath.StartsWith(FixtureRoot)) ++FixtureMatches;
    });

    Searcher->SetCppSourceSearchEnabled(true);
    const double StartSeconds = FPlatformTime::Seconds();
    Searcher->StartSearch();
    const double QueueMilliseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
    const int32 QueuedSourceFiles = Searcher->CppSourceFilesToScan.Num();
    // Measure the source phase in isolation; asset-registry and async Blueprint loading
    // have separate performance paths and would make this fixture nondeterministic.
    Searcher->bAssetQueuePrepared = true;
    Searcher->TotalAssetsToScan = QueuedSourceFiles;
    Searcher->CurrentScannedCount = 0;

    double MaxTickMilliseconds = 0.0;
    int32 TickCount = 0;
    while (Searcher->bIsSearching && TickCount < 10000)
    {
        const double TickStartSeconds = FPlatformTime::Seconds();
        Searcher->Tick(0.0f);
        MaxTickMilliseconds = FMath::Max(MaxTickMilliseconds, (FPlatformTime::Seconds() - TickStartSeconds) * 1000.0);
        ++TickCount;
    }
    const double TotalMilliseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
    UE_LOG(LogTemp, Display,
        TEXT("TraceMotive VisualRef C++ perf: fixtureFiles=%d queuedFiles=%d matches=%d queueMs=%.3f maxTickMs=%.3f totalMs=%.3f ticks=%d"),
        FixtureFiles.Num(), QueuedSourceFiles, FixtureMatches, QueueMilliseconds, MaxTickMilliseconds, TotalMilliseconds, TickCount);
    TestFalse(TEXT("C++ source scan completes"), Searcher->bIsSearching);
    const int32 ExpectedMatches = FMath::Min(FixtureFiles.Num(), TMPerf::MaxSearchResults());
    TestEqual(TEXT("C++ scan respects the configured result limit"), FixtureMatches, ExpectedMatches);
    TestEqual(TEXT("Result-limit status reflects an early stop"), Searcher->WasCppSourceScanTruncated(), FixtureFiles.Num() >= TMPerf::MaxSearchResults());
    TestTrue(TEXT("All generated source files are queued"), QueuedSourceFiles >= FixtureFiles.Num());
    if (Searcher->bIsSearching) Searcher->CancelSearch();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMPersistedBlueprintReferenceTest,
    "TraceMotive.ProjectFixture.BlueprintVariableReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMPersistedBlueprintReferenceTest::RunTest(const FString&)
{
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr,
        TEXT("/Game/TraceMotiveTests/Blueprints/BP_TM_VariableReferenceSample.BP_TM_VariableReferenceSample"));
    if (!TestNotNull(TEXT("Saved Blueprint fixture loads from project Content"), Blueprint)) return false;
    if (!TestNotNull(TEXT("Fixture has a generated class"), Blueprint->GeneratedClass.Get())) return false;
    if (!TestNotNull(TEXT("Fixture has a native parent class"), Blueprint->ParentClass.Get())) return false;
    TestEqual(TEXT("Fixture parent class"), Blueprint->ParentClass->GetName(), FString(TEXT("TraceMotiveFeatureLabActor")));

    auto VerifyVariable = [this, Blueprint](FName VariableName, UClass* OwnerClass)
    {
        TSharedRef<VisualRefSearcher> Searcher = MakeShared<VisualRefSearcher>(Blueprint, VariableName, false, OwnerClass);
        Searcher->SetSearchScope(EVisualRefSearchScope::CurrentBlueprint);
        TArray<UEdGraphNode*> Matches;
        Searcher->OnRefFound.BindLambda([&Matches](UEdGraphNode* Node) { Matches.Add(Node); });
        Searcher->StartSearch();
        for (int32 TickIndex = 0; Searcher->bIsSearching && TickIndex < 100; ++TickIndex)
        {
            Searcher->Tick(0.0f);
        }
        TestFalse(FString::Printf(TEXT("%s search completes"), *VariableName.ToString()), Searcher->bIsSearching);
        TestEqual(FString::Printf(TEXT("%s finds one GET and one SET"), *VariableName.ToString()), Matches.Num(), 2);
        int32 Reads = 0;
        int32 Writes = 0;
        for (UEdGraphNode* Node : Matches)
        {
            Reads += Node->IsA<UK2Node_VariableGet>() ? 1 : 0;
            Writes += Node->IsA<UK2Node_VariableSet>() ? 1 : 0;
        }
        TestEqual(FString::Printf(TEXT("%s GET count"), *VariableName.ToString()), Reads, 1);
        TestEqual(FString::Printf(TEXT("%s SET count"), *VariableName.ToString()), Writes, 1);
        if (Searcher->bIsSearching) Searcher->CancelSearch();
    };

    VerifyVariable(TEXT("TMBlueprintSampleValue"), nullptr);
    VerifyVariable(TEXT("TMReferenceSampleValue"), Blueprint->ParentClass.Get());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMCrossBlueprintReferenceTest,
    "TraceMotive.ProjectFixture.CrossBlueprintVariableReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMCrossBlueprintReferenceTest::RunTest(const FString&)
{
    constexpr const TCHAR* SourcePath = TEXT("/Game/TraceMotiveTests/Blueprints/BP_TM_VariableReferenceSample");
    constexpr const TCHAR* ConsumerPath = TEXT("/Game/TraceMotiveTests/Blueprints/BP_TM_VariableReferenceConsumer");
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr,
        TEXT("/Game/TraceMotiveTests/Blueprints/BP_TM_VariableReferenceSample.BP_TM_VariableReferenceSample"));
    if (!TestNotNull(TEXT("Source Blueprint fixture loads"), Blueprint)) return false;
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().SearchAllAssets(true);

    auto VerifyProjectReferences = [this, Blueprint, SourcePath, ConsumerPath](FName VariableName, UClass* OwnerClass)
    {
        TSharedRef<VisualRefSearcher> Searcher = MakeShared<VisualRefSearcher>(Blueprint, VariableName, false, OwnerClass);
        Searcher->SetSearchScope(EVisualRefSearchScope::ProjectContent);
        Searcher->SetExhaustiveSearchEnabled(true);
        TMap<FString, int32> MatchesByPackage;
        Searcher->OnRefFound.BindLambda([&MatchesByPackage](UEdGraphNode* Node)
        {
            if (Node) ++MatchesByPackage.FindOrAdd(Node->GetOutermost()->GetName());
        });
        Searcher->StartSearch();
        Searcher->Tick(0.0f);
        TestTrue(FString::Printf(TEXT("%s consumer BP is queued by full scan"), *VariableName.ToString()),
            Searcher->AssetQueue.ContainsByPredicate([ConsumerPath](const FAssetData& AssetData)
            {
                return AssetData.PackageName.ToString() == ConsumerPath;
            }));
        // The synchronous fixture keeps asset loading out of the result-matching assertion.
        for (const FAssetData& AssetData : Searcher->AssetQueue) AssetData.GetAsset();
        for (int32 TickIndex = 0; Searcher->bIsSearching && TickIndex < 10000; ++TickIndex)
        {
            Searcher->Tick(0.0f);
        }
        TestFalse(FString::Printf(TEXT("%s project search completes"), *VariableName.ToString()), Searcher->bIsSearching);
        TestEqual(FString::Printf(TEXT("%s source BP refs"), *VariableName.ToString()), MatchesByPackage.FindRef(SourcePath), 2);
        TestEqual(FString::Printf(TEXT("%s consumer BP refs"), *VariableName.ToString()), MatchesByPackage.FindRef(ConsumerPath), 2);
        if (Searcher->bIsSearching) Searcher->CancelSearch();
    };

    VerifyProjectReferences(TEXT("TMBlueprintSampleValue"), nullptr);
    VerifyProjectReferences(TEXT("TMReferenceSampleValue"), Blueprint->ParentClass.Get());
    return true;
}

struct FTMAsyncBlueprintReferenceState
{
    TStrongObjectPtr<UBlueprint> Blueprint;
    TSharedPtr<VisualRefSearcher> Searcher;
    TMap<FString, int32> MatchesByPackage;
    double Started = 0.0;
    bool bFinished = false;
};

class FTMWaitForAsyncBlueprintReferences : public IAutomationLatentCommand
{
public:
    FTMWaitForAsyncBlueprintReferences(FAutomationTestBase* InTest, TSharedRef<FTMAsyncBlueprintReferenceState> InState)
        : Test(InTest), State(InState) {}

    virtual bool Update() override
    {
        if (!State->bFinished && FPlatformTime::Seconds() - State->Started < 30.0) return false;
        Test->TestTrue(TEXT("Project search finishes through editor ticks and asynchronous loading"), State->bFinished);
        Test->TestEqual(TEXT("Async search finds the source Blueprint GET and SET"),
            State->MatchesByPackage.FindRef(TEXT("/Game/TraceMotiveTests/Blueprints/BP_TM_VariableReferenceSample")), 2);
        Test->TestEqual(TEXT("Async search finds the consumer Blueprint GET and SET"),
            State->MatchesByPackage.FindRef(TEXT("/Game/TraceMotiveTests/Blueprints/BP_TM_VariableReferenceConsumer")), 2);
        Test->AddInfo(FString::Printf(TEXT("Async Blueprint project search: %.3f ms (includes editor tick intervals and source candidate scan)."),
            (FPlatformTime::Seconds() - State->Started) * 1000.0));
        State->Searcher->CancelSearch();
        return true;
    }
private:
    FAutomationTestBase* Test;
    TSharedRef<FTMAsyncBlueprintReferenceState> State;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMAsyncBlueprintReferenceTest,
    "TraceMotive.ProjectFixture.AsyncBlueprintVariableReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMAsyncBlueprintReferenceTest::RunTest(const FString&)
{
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr,
        TEXT("/Game/TraceMotiveTests/Blueprints/BP_TM_VariableReferenceSample.BP_TM_VariableReferenceSample"));
    if (!TestNotNull(TEXT("Saved source Blueprint loads"), Blueprint)) return false;
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().SearchAllAssets(true);
    AddInfo(FString::Printf(TEXT("Consumer Blueprint is unloaded at search start: %s"),
        FindObject<UBlueprint>(nullptr, TEXT("/Game/TraceMotiveTests/Blueprints/BP_TM_VariableReferenceConsumer.BP_TM_VariableReferenceConsumer"))
            ? TEXT("false (warm run)") : TEXT("true (cold run)")));
    TSharedRef<FTMAsyncBlueprintReferenceState> State = MakeShared<FTMAsyncBlueprintReferenceState>();
    State->Blueprint.Reset(Blueprint);
    State->Searcher = MakeShared<VisualRefSearcher>(Blueprint, TEXT("TMBlueprintSampleValue"));
    State->Searcher->SetSearchScope(EVisualRefSearchScope::ProjectContent);
    TWeakPtr<FTMAsyncBlueprintReferenceState> WeakState = State;
    State->Searcher->OnRefFound.BindLambda([WeakState](UEdGraphNode* Node)
    {
        if (TSharedPtr<FTMAsyncBlueprintReferenceState> Pinned = WeakState.Pin())
        {
            if (Node) ++Pinned->MatchesByPackage.FindOrAdd(Node->GetOutermost()->GetName());
        }
    });
    State->Searcher->OnSearchFinished.BindLambda([WeakState]()
    {
        if (TSharedPtr<FTMAsyncBlueprintReferenceState> Pinned = WeakState.Pin()) Pinned->bFinished = true;
    });
    State->Started = FPlatformTime::Seconds();
    State->Searcher->StartSearch();
    ADD_LATENT_AUTOMATION_COMMAND(FTMWaitForAsyncBlueprintReferences(this, State));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMCallChainResumeTest,
    "TraceMotive.Search.CallChainPackageResumesAfterBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMCallChainResumeTest::RunTest(const FString&)
{
    UBlueprint* Blueprint = NewObject<UBlueprint>(GetTransientPackage());
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint);
    Blueprint->UbergraphPages.Add(Graph);
    for (int32 Index = 0; Index < 8; ++Index) Graph->AddNode(NewObject<UEdGraphNode>(Graph));

    TSharedRef<FunctionCallChainTracer> Tracer = MakeShared<FunctionCallChainTracer>(Blueprint, TEXT("MissingFunction"));
    Tracer->PackagesToScan.Add(GetTransientPackage()->GetFName());
    Tracer->TotalCount = 1;
    Tracer->ActiveScanPackage.Reset(GetTransientPackage());
    Tracer->ActivePackageBlueprints.Add(Blueprint);

    Tracer->TickState_ScanningPackages(FPlatformTime::Seconds(), 0.0);
    TestEqual(TEXT("Expired budget keeps the current package"), Tracer->LoadedPackageIndex, 0);

    Tracer->TickState_ScanningPackages(FPlatformTime::Seconds(), 1.0);
    TestEqual(TEXT("A later slice completes the package"), Tracer->LoadedPackageIndex, 1);
    TestTrue(TEXT("Completed package advances to caller-cache state"), Tracer->CurrentState == ETraceState::BuildingCallerCache);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMSettingsSafetyDefaultsTest,
    "TraceMotive.Settings.SafetyDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMSettingsSafetyDefaultsTest::RunTest(const FString&)
{
    const UTraceMotiveSettings* Settings = UTraceMotiveSettings::Get();
    TestNotNull(TEXT("TraceMotive developer settings are available"), Settings);
    if (!Settings) return false;

    TestTrue(TEXT("Large-project safe mode is enabled by default"), Settings->bLargeProjectSafeMode);
    TestTrue(TEXT("Search result limit is bounded"), TMPerf::MaxSearchResults() >= 50 && TMPerf::MaxSearchResults() <= 10000);
    TestTrue(TEXT("Snapshot field limit is bounded"), TMPerf::MaxSnapshotFields() >= 100 && TMPerf::MaxSnapshotFields() <= 20000);

    const uint64 PreviousGeneration = TMPerf::GetCancellationGeneration();
    TMPerf::RequestCancelAll();
    TestEqual(TEXT("Stop-all invalidates running work generations"), TMPerf::GetCancellationGeneration(), PreviousGeneration + 1);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMLocalizationRuntimeCoverageTest,
    "TraceMotive.Localization.RuntimeCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMLocalizationRuntimeCoverageTest::RunTest(const FString&)
{
    const bool bOriginalKorean = TMLoc::UseKorean();
    TMLoc::SetUseKorean(true);
    TestEqual(TEXT("Existing tool label is translated"), TMLoc::String(TEXT("Collision Pair Analyzer"), TEXT("")), FString(TEXT("충돌 쌍 분석기")));
    TestEqual(TEXT("New tool label is translated"), TMLoc::String(TEXT("Variable Value Trace"), TEXT("")), FString(TEXT("변수 값 추적")));
    TestEqual(TEXT("Guide chrome is translated"), TMLoc::String(TEXT("Annotated live Slate example"), TEXT("")), FString(TEXT("주석이 포함된 라이브 Slate 예시")));
    TMLoc::SetUseKorean(false);
    TestEqual(TEXT("English selection remains available"), TMLoc::String(TEXT("Collision Pair Analyzer"), TEXT("")), FString(TEXT("Collision Pair Analyzer")));
    TMLoc::SetUseKorean(bOriginalKorean);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMInvestigationSnapshotDiffTest,
    "TraceMotive.Investigation.SnapshotDiff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMInvestigationSnapshotDiffTest::RunTest(const FString&)
{
    TMap<FString, FString> Before;
    Before.Add(TEXT("Actor.Health"), TEXT("100"));
    Before.Add(TEXT("Actor.OldOnly"), TEXT("Present"));

    TMap<FString, FString> After;
    After.Add(TEXT("Actor.Health"), TEXT("75"));
    After.Add(TEXT("Actor.NewOnly"), TEXT("Present"));

    const FTMSnapshotDiff Diff = TMInvestigationSession::CompareSnapshots(Before, After);
    TestEqual(TEXT("One changed field is detected"), Diff.Changed, 1);
    TestEqual(TEXT("One added field is detected"), Diff.Added, 1);
    TestEqual(TEXT("One removed field is detected"), Diff.Removed, 1);
    TestEqual(TEXT("All differences have readable lines"), Diff.Lines.Num(), 3);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMSupportBundlePrivacyAndZipTest,
    "TraceMotive.SupportBundle.PrivacyAndZip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMSupportBundlePrivacyAndZipTest::RunTest(const FString&)
{
    const FString Sensitive = TEXT("C:\\Users\\TestPerson\\Project\\File.uasset user@example.com api_key=secret-value");
    int32 Redactions = 0;
    const FString Redacted = TMSupportBundle::RedactSensitiveText(Sensitive, &Redactions);
    TestTrue(TEXT("User home is removed"), Redacted.Contains(TEXT("<USER_HOME>")));
    TestTrue(TEXT("Email is removed"), Redacted.Contains(TEXT("<EMAIL>")));
    TestTrue(TEXT("Common secret assignment is removed"), Redacted.Contains(TEXT("<REDACTED_SECRET>")));
    TestFalse(TEXT("Original username is absent"), Redacted.Contains(TEXT("TestPerson")));
    TestFalse(TEXT("Original email is absent"), Redacted.Contains(TEXT("user@example.com")));
    TestFalse(TEXT("Original secret is absent"), Redacted.Contains(TEXT("secret-value")));
    TestTrue(TEXT("Redactions are reported"), Redactions >= 3);

    const FString ZipPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TraceMotive"), TEXT("Automation"), TEXT("SupportBundleTest.zip"));
    TMap<FString, FString> Entries;
    Entries.Add(TEXT("README.md"), TEXT("TraceMotive support test"));
    Entries.Add(TEXT("metadata.json"), TEXT("{\"ok\":true}"));
    FString Error;
    TestTrue(TEXT("Stored ZIP is written"), TMSupportBundle::WriteStoredZip(ZipPath, Entries, Error));
    TArray<uint8> Bytes;
    TestTrue(TEXT("Stored ZIP can be read back"), FFileHelper::LoadFileToArray(Bytes, *ZipPath));
    TestTrue(TEXT("ZIP has a local-file signature"), Bytes.Num() >= 4 && Bytes[0] == 0x50 && Bytes[1] == 0x4b && Bytes[2] == 0x03 && Bytes[3] == 0x04);
    TestTrue(TEXT("ZIP has an end-of-central-directory signature"), Bytes.Num() >= 22 && Bytes[Bytes.Num() - 22] == 0x50 && Bytes[Bytes.Num() - 21] == 0x4b && Bytes[Bytes.Num() - 20] == 0x05 && Bytes[Bytes.Num() - 19] == 0x06);
    IFileManager::Get().Delete(*ZipPath, false, true);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMWindowMenuRegistrationTest,
    "TraceMotive.Registration.ConsolidatedMainMenus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMWindowMenuRegistrationTest::RunTest(const FString&)
{
    UToolMenu* WindowMenu = UToolMenus::Get()->FindMenu(TEXT("LevelEditor.MainMenu.Window"));
    TestNotNull(TEXT("The Level Editor Window menu exists"), WindowMenu);
    if (!WindowMenu)
    {
        return false;
    }

    TestTrue(
        TEXT("Window menu exposes only the TraceMotiveTools launcher"),
        WindowMenu->ContainsEntry(TEXT("TMOpenTraceMotiveTools")));

    UToolMenu* ToolsMenu = UToolMenus::Get()->FindMenu(TEXT("LevelEditor.MainMenu.Tools"));
    TestNotNull(TEXT("The Level Editor Tools menu exists"), ToolsMenu);
    if (!ToolsMenu)
    {
        return false;
    }

    TestTrue(
        TEXT("Tools menu exposes the TraceMotive submenu"),
        ToolsMenu->ContainsEntry(TEXT("TraceMotive")));

    UToolMenu* EntryMenu = UToolMenus::Get()->FindMenu(TEXT("GraphEditor.GraphNodeContextMenu.K2Node_FunctionEntry"));
    TestNotNull(TEXT("Function entry context menu exists"), EntryMenu);
    if (EntryMenu)
    {
        TestTrue(TEXT("Function entries dispatch through function reference search"), EntryMenu->ContainsEntry(TEXT("VisualFindFunctionReferences")));
        TestFalse(TEXT("Function entries do not expose the variable-only action"), EntryMenu->ContainsEntry(TEXT("VisualFindReferences")));
    }

    return true;
}

#endif
