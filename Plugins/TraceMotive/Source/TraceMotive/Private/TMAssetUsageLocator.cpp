#include "TMAssetUsageLocator.h"
#include "UObject/StrongObjectPtr.h"
#include "TMEngineCompatibility.h"
#include "TMStyle.h"

#include "TMPerformanceGuard.h"
#include "TMTraceSession.h"
#include "TMLocalization.h"
#include "Runtime/Launch/Resources/Version.h"



#include "TMDockTabHelper.h"

#include "AssetRegistry/AssetIdentifier.h"

#include "AssetRegistry/AssetRegistryModule.h"

#include "ContentBrowserMenuContexts.h"

#include "ContentBrowserModule.h"

#include "EdGraph/EdGraph.h"

#include "EdGraph/EdGraphNode.h"

#include "EdGraph/EdGraphPin.h"

#include "Engine/Blueprint.h"

#include "Engine/BlueprintGeneratedClass.h"

#include "Engine/StreamableManager.h"

#include "Engine/InheritableComponentHandler.h"

#include "Engine/SCS_Node.h"

#include "Engine/SimpleConstructionScript.h"

#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "IContentBrowserSingleton.h"

#include "Kismet2/KismetEditorUtilities.h"

#include "Modules/ModuleManager.h"

#include "Styling/AppStyle.h"

#include "Subsystems/AssetEditorSubsystem.h"

#include "ToolMenus.h"

#include "UObject/SoftObjectPath.h"

#include "UObject/UnrealType.h"

#include "UObject/UObjectGlobals.h"

#include "Widgets/Input/SButton.h"

#include "Widgets/Input/SSearchBox.h"

#include "Widgets/Layout/SBorder.h"

#include "Widgets/SBoxPanel.h"

#include "Widgets/Text/STextBlock.h"

#include "Widgets/Views/SListView.h"

#include "Widgets/Views/STableRow.h"

#include "Widgets/Views/STableViewBase.h"



namespace

{

    // Runtime budget comes from TMPerformanceGuard.h.
    constexpr double AssetUsageUiRefreshIntervalSeconds = 0.20;
    thread_local TSet<FString>* ActiveAssetUsageFindingKeys = nullptr;
    thread_local bool* ActiveAssetUsageFindingsTruncated = nullptr;

    class FScopedAssetUsageFindingStore
    {
    public:
        FScopedAssetUsageFindingStore(TSet<FString>& InKeys, bool& bInTruncated)
            : PreviousKeys(ActiveAssetUsageFindingKeys)
            , PreviousTruncated(ActiveAssetUsageFindingsTruncated)
        {
            ActiveAssetUsageFindingKeys = &InKeys;
            ActiveAssetUsageFindingsTruncated = &bInTruncated;
        }

        ~FScopedAssetUsageFindingStore()
        {
            ActiveAssetUsageFindingKeys = PreviousKeys;
            ActiveAssetUsageFindingsTruncated = PreviousTruncated;
        }

    private:
        TSet<FString>* PreviousKeys = nullptr;
        bool* PreviousTruncated = nullptr;
    };



    struct FTMAssetUsageFinding

    {

        TWeakObjectPtr<UBlueprint> Blueprint;

        TWeakObjectPtr<UEdGraphNode> Node;

        FString BlueprintName;

        FString BlueprintPath;

        FString GraphName;

        FString FunctionName;

        FString NodeTitle;

        FString LocationKind;

        FString Detail;

        FString Reason;

        bool IsCandidate() const { return LocationKind == TEXT("Reference Viewer Candidate") || Reason.Contains(TEXT("contains target")) || Detail.StartsWith(TEXT("[Text candidate]")); }



        FString BuildSearchText() const

        {

            return BlueprintName + TEXT(" ") + BlueprintPath + TEXT(" ") + GraphName + TEXT(" ") + FunctionName + TEXT(" ") + NodeTitle + TEXT(" ") + LocationKind + TEXT(" ") + Detail + TEXT(" ") + Reason;

        }

    };



    using FFindingPtr = TSharedPtr<FTMAssetUsageFinding>;



    bool IsSearchablePackageName(FName PackageName)

    {

        const FString PackagePath = PackageName.ToString();

        return PackagePath.StartsWith(TEXT("/")) && !PackagePath.StartsWith(TEXT("/Engine")) && !PackagePath.StartsWith(TEXT("/Script")) && !PackagePath.StartsWith(TEXT("/Memory")) && !PackagePath.StartsWith(TEXT("/Temp")) && !PackagePath.StartsWith(TEXT("/Transient"));

    }



    bool PathMatchesTarget(const FString& Candidate, const FString& ObjectPath, const FString& PackageName, const FString& AssetName)

    {

        return !Candidate.IsEmpty() && ((!ObjectPath.IsEmpty() && Candidate.Contains(ObjectPath)) || (!PackageName.IsEmpty() && Candidate.Contains(PackageName)) || (!AssetName.IsEmpty() && Candidate.Contains(AssetName)));

    }



    bool ObjectMatchesTarget(const UObject* Object, const UObject* TargetObject, const FString& ObjectPath, const FString& PackageName)

    {

        if (!Object)

        {

            return false;

        }

        if (TargetObject && Object == TargetObject)

        {

            return true;

        }

        if (!ObjectPath.IsEmpty())

        {

            return Object->GetPathName() == ObjectPath;

        }

        const UPackage* Package = Object->GetOutermost();

        return Package && !PackageName.IsEmpty() && Package->GetName() == PackageName;

    }



    bool IsBlueprintAssetData(const FAssetData& AssetData)

    {

        return AssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName()

            || AssetData.AssetClassPath.ToString().Contains(TEXT("Blueprint"));

    }

    FString GraphNameOf(const UEdGraph* Graph)

    {

        if (!Graph)

        {

            return TEXT("Class Defaults");

        }

        FString Name = Graph->GetName();

        Name.RemoveFromStart(TEXT("ExecuteUbergraph_"));

        return Name;

    }



    FString FunctionNameOf(const UEdGraph* Graph)

    {

        if (!Graph)

        {

            return TEXT("Class Defaults");

        }

        return Graph->GetName().StartsWith(TEXT("ExecuteUbergraph_")) ? TEXT("Event Graph") : Graph->GetName();

    }



    void AddFinding(TArray<FFindingPtr>& Findings, UBlueprint* Blueprint, UEdGraphNode* Node, const FString& Kind, const FString& Detail, const FString& Reason)

    {

        if (!Blueprint)

        {

            return;

        }

        const UEdGraph* Graph = Node ? Node->GetGraph() : nullptr;

        FTMAssetUsageFinding Finding;

        Finding.Blueprint = Blueprint;

        Finding.Node = Node;

        Finding.BlueprintName = Blueprint->GetName();

        Finding.BlueprintPath = Blueprint->GetPathName();

        Finding.GraphName = GraphNameOf(Graph);

        Finding.FunctionName = FunctionNameOf(Graph);

        Finding.NodeTitle = Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("Blueprint Defaults");

        Finding.LocationKind = Kind;

        Finding.Detail = Detail;

        Finding.Reason = Reason;

        const FString NewKey = Finding.BuildSearchText();
        if (ActiveAssetUsageFindingKeys)
        {
            if (ActiveAssetUsageFindingKeys->Contains(NewKey))
            {
                return;
            }

            if (Findings.Num() >= TMPerf::MaxAssetUsageFindings())
            {
                if (ActiveAssetUsageFindingsTruncated)
                {
                    *ActiveAssetUsageFindingsTruncated = true;
                }
                return;
            }

            ActiveAssetUsageFindingKeys->Add(NewKey);
            Findings.Add(MakeShared<FTMAssetUsageFinding>(Finding));
            return;
        }

        for (const FFindingPtr& Existing : Findings)

        {

            if (Existing.IsValid() && Existing->BuildSearchText() == NewKey)

            {

                return;

            }

        }

        Findings.Add(MakeShared<FTMAssetUsageFinding>(Finding));

    }



    bool ScanPropertyValue(FProperty* Property, const void* ValuePtr, const FString& Path, const UObject* TargetObject, const FString& ObjectPath, const FString& PackageName, TArray<FString>& OutMatches, int32 Depth);



    bool ScanStructValue(UStruct* Struct, const void* ContainerPtr, const FString& Prefix, const UObject* TargetObject, const FString& ObjectPath, const FString& PackageName, TArray<FString>& OutMatches, int32 Depth)

    {

        if (!Struct || !ContainerPtr || Depth > 5)

        {

            return false;

        }



        bool bFound = false;

        for (TFieldIterator<FProperty> It(Struct); It; ++It)

        {

            FProperty* Property = *It;

            const void* ValuePtr = Property ? Property->ContainerPtrToValuePtr<void>(ContainerPtr) : nullptr;

            if (!Property || !ValuePtr)

            {

                continue;

            }

            const FString Path = Prefix.IsEmpty() ? Property->GetName() : Prefix + TEXT(".") + Property->GetName();

            bFound |= ScanPropertyValue(Property, ValuePtr, Path, TargetObject, ObjectPath, PackageName, OutMatches, Depth + 1);

        }

        return bFound;

    }



    bool ScanPropertyValue(FProperty* Property, const void* ValuePtr, const FString& Path, const UObject* TargetObject, const FString& ObjectPath, const FString& PackageName, TArray<FString>& OutMatches, int32 Depth)

    {

        if (!Property || !ValuePtr || Depth > 5)

        {

            return false;

        }



        bool bFound = false;

        if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))

        {

            const FString SoftPath = SoftObjectProperty->GetPropertyValue(ValuePtr).ToSoftObjectPath().ToString();

            if ((!ObjectPath.IsEmpty() ? SoftPath == ObjectPath : FSoftObjectPath(SoftPath).GetLongPackageName() == PackageName))

            {

                OutMatches.Add(FString::Printf(TEXT("%s = %s"), *Path, *SoftPath));

                return true;

            }

        }

        else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))

        {

            const UObject* Value = ObjectProperty->GetObjectPropertyValue(ValuePtr);

            if (ObjectMatchesTarget(Value, TargetObject, ObjectPath, PackageName))

            {

                OutMatches.Add(FString::Printf(TEXT("%s = %s"), *Path, *GetNameSafe(Value)));

                return true;

            }

        }

        else if (const FStrProperty* StrProperty = CastField<FStrProperty>(Property))

        {

            const FString Value = StrProperty->GetPropertyValue(ValuePtr);

            if (PathMatchesTarget(Value, ObjectPath, PackageName, FString()))

            {

                OutMatches.Add(FString::Printf(TEXT("[Text candidate] %s = %s"), *Path, *Value));

                return true;

            }

        }

        else if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))

        {

            const FString Value = NameProperty->GetPropertyValue(ValuePtr).ToString();

            if (PathMatchesTarget(Value, ObjectPath, PackageName, FString()))

            {

                OutMatches.Add(FString::Printf(TEXT("[Text candidate] %s = %s"), *Path, *Value));

                return true;

            }

        }

        else if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))

        {

            const FString Value = TextProperty->GetPropertyValue(ValuePtr).ToString();

            if (PathMatchesTarget(Value, ObjectPath, PackageName, FString()))

            {

                OutMatches.Add(FString::Printf(TEXT("[Text candidate] %s = %s"), *Path, *Value));

                return true;

            }

        }

        else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))

        {

            bFound |= ScanStructValue(StructProperty->Struct, ValuePtr, Path, TargetObject, ObjectPath, PackageName, OutMatches, Depth + 1);

        }

        else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))

        {

            FScriptArrayHelper Helper(ArrayProperty, ValuePtr);

            const int32 Count = FMath::Min(Helper.Num(), 256);

            for (int32 Index = 0; Index < Count; ++Index)

            {

                bFound |= ScanPropertyValue(ArrayProperty->Inner, Helper.GetRawPtr(Index), FString::Printf(TEXT("%s[%d]"), *Path, Index), TargetObject, ObjectPath, PackageName, OutMatches, Depth + 1);

            }

        }

        else if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))

        {

            FScriptSetHelper Helper(SetProperty, ValuePtr);

            int32 OutputIndex = 0;

            for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex() && OutputIndex < 256; ++SparseIndex)

            {

                if (Helper.IsValidIndex(SparseIndex))

                {

                    bFound |= ScanPropertyValue(SetProperty->ElementProp, Helper.GetElementPtr(SparseIndex), FString::Printf(TEXT("%s{%d}"), *Path, OutputIndex++), TargetObject, ObjectPath, PackageName, OutMatches, Depth + 1);

                }

            }

        }

        else if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))

        {

            FScriptMapHelper Helper(MapProperty, ValuePtr);

            int32 OutputIndex = 0;

            for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex() && OutputIndex < 256; ++SparseIndex)

            {

                if (!Helper.IsValidIndex(SparseIndex))

                {

                    continue;

                }

                bFound |= ScanPropertyValue(MapProperty->KeyProp, Helper.GetKeyPtr(SparseIndex), FString::Printf(TEXT("%s{%d}.Key"), *Path, OutputIndex), TargetObject, ObjectPath, PackageName, OutMatches, Depth + 1);

                bFound |= ScanPropertyValue(MapProperty->ValueProp, Helper.GetValuePtr(SparseIndex), FString::Printf(TEXT("%s{%d}.Value"), *Path, OutputIndex), TargetObject, ObjectPath, PackageName, OutMatches, Depth + 1);

                ++OutputIndex;

            }

        }



        return bFound;

    }



    void ScanUObjectProperties(UObject* Object, const FString& Prefix, const UObject* TargetObject, const FString& ObjectPath, const FString& PackageName, TArray<FString>& OutMatches)

    {

        if (!Object)

        {

            return;

        }

        ScanStructValue(Object->GetClass(), Object, Prefix, TargetObject, ObjectPath, PackageName, OutMatches, 0);

    }

    bool DoesObjectArrayReferenceTarget(const TArray<UObject*>& Objects, const UObject* TargetObject, const FString& ObjectPath, const FString& PackageName, FString& OutMatchedObject)

    {

        for (UObject* Object : Objects)

        {

            if (ObjectMatchesTarget(Object, TargetObject, ObjectPath, PackageName))

            {

                OutMatchedObject = GetNameSafe(Object);

                return true;

            }

        }

        return false;

    }



    bool ScanNodeReferences(UEdGraphNode* Node, const UObject* TargetObject, const FString& ObjectPath, const FString& PackageName, FString& OutDetail)

    {

        if (!Node)

        {

            return false;

        }



        TArray<UObject*> ReferencedObjects;

        FReferenceFinder Finder(ReferencedObjects, nullptr, false, true, false, false);

        Finder.FindReferences(Node);



        FString MatchedObject;

        if (DoesObjectArrayReferenceTarget(ReferencedObjects, TargetObject, ObjectPath, PackageName, MatchedObject))

        {

            OutDetail = FString::Printf(TEXT("Node serialized reference = %s"), *MatchedObject);

            return true;

        }



        return false;

    }



    bool ScanPinExportText(const UEdGraphPin* Pin, const FString& ObjectPath, const FString& PackageName, const FString& AssetName, FString& OutDetail)

    {

        if (!Pin)

        {

            return false;

        }



        FString ExportedPin;

        if (Pin->ExportTextItem(ExportedPin, PPF_None) && PathMatchesTarget(ExportedPin, ObjectPath, PackageName, AssetName))

        {

            OutDetail = FString::Printf(TEXT("Pin '%s' export text contains target"), *Pin->PinName.ToString());

            return true;

        }



        return false;

    }

    void ScanTemplateObjectUsage(UBlueprint* Blueprint, UObject* TemplateObject, const FString& LocationKind, const FString& LocationLabel, const UObject* TargetObject, const FString& ObjectPath, const FString& PackageName, TArray<FFindingPtr>& OutFindings)

    {

        if (!Blueprint || !TemplateObject)

        {

            return;

        }



        TArray<FString> PropertyMatches;

        ScanUObjectProperties(TemplateObject, LocationLabel, TargetObject, ObjectPath, PackageName, PropertyMatches);

        for (const FString& Match : PropertyMatches)

        {

            AddFinding(OutFindings, Blueprint, nullptr, LocationKind, Match, TEXT("Template/default property references target"));

        }



        TArray<UObject*> ReferencedObjects;

        FReferenceFinder Finder(ReferencedObjects, nullptr, false, true, false, false);

        Finder.FindReferences(TemplateObject);



        FString MatchedObject;

        if (DoesObjectArrayReferenceTarget(ReferencedObjects, TargetObject, ObjectPath, PackageName, MatchedObject))

        {

            AddFinding(

                OutFindings,

                Blueprint,

                nullptr,

                LocationKind,

                FString::Printf(TEXT("%s serialized reference = %s"), *LocationLabel, *MatchedObject),

                TEXT("FReferenceFinder found target on template/default object"));

        }

    }



    FString BuildComponentLocationLabel(const TCHAR* Prefix, const UActorComponent* Component)

    {

        if (!Component)

        {

            return FString(Prefix);

        }



        return FString::Printf(TEXT("%s.%s (%s)"), Prefix, *Component->GetName(), *GetNameSafe(Component->GetClass()));

    }

    bool ScanBlueprint(UBlueprint* Blueprint, UObject* TargetObject, const FString& ObjectPath, const FString& PackageName, const FString& AssetName, TArray<FFindingPtr>& OutFindings, int32& ResumeOffset, double Deadline)

    {

        if (!Blueprint)

        {

            return true;

        }



        int32 NodeOffset = 0;
        TArray<UEdGraph*> Graphs;

        Blueprint->GetAllGraphs(Graphs);

        for (UEdGraph* Graph : Graphs)

        {

            if (!Graph)

            {

                continue;

            }

            for (UEdGraphNode* Node : Graph->Nodes)

            {

                const int32 CurrentOffset = NodeOffset++;
                if (CurrentOffset < ResumeOffset) continue;
                if (FPlatformTime::Seconds() >= Deadline) { ResumeOffset = CurrentOffset; return false; }
                if (!Node)

                {

                    continue;

                }

                for (UEdGraphPin* Pin : Node->Pins)

                {

                    if (!Pin)

                    {

                        continue;

                    }

                    const FString PinName = Pin->PinName.ToString();

                    if (ObjectMatchesTarget(Pin->DefaultObject, TargetObject, ObjectPath, PackageName))

                    {

                        AddFinding(OutFindings, Blueprint, Node, TEXT("Graph Node Pin"), FString::Printf(TEXT("Pin '%s' default object = %s"), *PinName, *GetNameSafe(Pin->DefaultObject)), TEXT("Exact asset object stored on node pin"));

                    }

                    if (Pin->PinType.PinSubCategoryObject.IsValid() && ObjectMatchesTarget(Pin->PinType.PinSubCategoryObject.Get(), TargetObject, ObjectPath, PackageName))

                    {

                        AddFinding(OutFindings, Blueprint, Node, TEXT("Graph Node Pin"), FString::Printf(TEXT("Pin '%s' type references target"), *PinName), TEXT("Pin type object matches target"));

                    }

                    const FString PinText = Pin->DefaultValue + TEXT(" ") + Pin->AutogeneratedDefaultValue + TEXT(" ") + Pin->DefaultTextValue.ToString();

                    if (PathMatchesTarget(PinText, ObjectPath, PackageName, AssetName))

                    {

                        AddFinding(OutFindings, Blueprint, Node, TEXT("Graph Node Pin"), FString::Printf(TEXT("Pin '%s' default text contains target"), *PinName), TEXT("Serialized pin default contains target"));

                    }



                    FString PinExportDetail;

                    if (ScanPinExportText(Pin, ObjectPath, PackageName, AssetName, PinExportDetail))

                    {

                        AddFinding(OutFindings, Blueprint, Node, TEXT("Graph Node Pin Export"), PinExportDetail, TEXT("Pin ExportText contains target"));

                    }

                }



                #if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
                const FString NodeSearchText = Node->GetFindReferenceSearchString(EGetFindReferenceSearchStringFlags::Legacy);
#else
                const FString NodeSearchText = Node->GetFindReferenceSearchString();
#endif
                const FString NodeText = NodeSearchText + TEXT(" ") + Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString() + TEXT(" ") + Node->GetNodeTitle(ENodeTitleType::ListView).ToString();

                if (PathMatchesTarget(NodeText, ObjectPath, PackageName, AssetName))

                {

                    AddFinding(OutFindings, Blueprint, Node, TEXT("Graph Node"), TEXT("Node searchable text contains target path/name"), TEXT("Node metadata contains target"));

                }



                FString NodeReferenceDetail;

                if (ScanNodeReferences(Node, TargetObject, ObjectPath, PackageName, NodeReferenceDetail))

                {

                    AddFinding(OutFindings, Blueprint, Node, TEXT("Graph Node Reference"), NodeReferenceDetail, TEXT("FReferenceFinder found target on node"));

                }



                TArray<FString> NodeMatches;

                ScanUObjectProperties(Node, TEXT("Node"), TargetObject, ObjectPath, PackageName, NodeMatches);

                for (const FString& Match : NodeMatches)

                {

                    AddFinding(OutFindings, Blueprint, Node, TEXT("Graph Node Property"), Match, TEXT("Node UObject property references target"));

                }

            }

        }



if (NodeOffset++ >= ResumeOffset)
        {
            if (FPlatformTime::Seconds() >= Deadline) { ResumeOffset = NodeOffset - 1; return false; }
        ScanTemplateObjectUsage(

            Blueprint,

            Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr,

            TEXT("Class Default"),

            TEXT("CDO"),

            TargetObject,

            ObjectPath,

            PackageName,

            OutFindings);
        }



        for (UActorComponent* ComponentTemplate : Blueprint->ComponentTemplates)

        {

if (NodeOffset++ >= ResumeOffset)
        {
            if (FPlatformTime::Seconds() >= Deadline) { ResumeOffset = NodeOffset - 1; return false; }
            ScanTemplateObjectUsage(

                Blueprint,

                ComponentTemplate,

                TEXT("Blueprint Component Template"),

                BuildComponentLocationLabel(TEXT("BlueprintComponent"), ComponentTemplate),

                TargetObject,

                ObjectPath,

                PackageName,

                OutFindings);
        }

        }



        if (UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))

        {

            for (UActorComponent* ComponentTemplate : GeneratedClass->ComponentTemplates)

            {

if (NodeOffset++ >= ResumeOffset)
        {
            if (FPlatformTime::Seconds() >= Deadline) { ResumeOffset = NodeOffset - 1; return false; }
                ScanTemplateObjectUsage(

                    Blueprint,

                    ComponentTemplate,

                    TEXT("Generated Component Template"),

                    BuildComponentLocationLabel(TEXT("GeneratedComponent"), ComponentTemplate),

                    TargetObject,

                    ObjectPath,

                    PackageName,

                    OutFindings);
        }

            }



            if (UInheritableComponentHandler* ComponentHandler = GeneratedClass->GetInheritableComponentHandler(false))

            {

                TArray<UActorComponent*> OverrideTemplates;

                ComponentHandler->GetAllTemplates(OverrideTemplates, true);

                for (UActorComponent* ComponentTemplate : OverrideTemplates)

                {

if (NodeOffset++ >= ResumeOffset)
        {
            if (FPlatformTime::Seconds() >= Deadline) { ResumeOffset = NodeOffset - 1; return false; }
                    ScanTemplateObjectUsage(

                        Blueprint,

                        ComponentTemplate,

                        TEXT("Inherited Component Override"),

                        BuildComponentLocationLabel(TEXT("InheritedComponent"), ComponentTemplate),

                        TargetObject,

                        ObjectPath,

                        PackageName,

                        OutFindings);
        }

                }

            }

        }



        if (Blueprint->SimpleConstructionScript)

        {

            const TArray<USCS_Node*>& Nodes = Blueprint->SimpleConstructionScript->GetAllNodes();

            for (USCS_Node* SCSNode : Nodes)

            {

                if (!SCSNode)

                {

                    continue;

                }



                UActorComponent* ComponentTemplate = SCSNode->GetActualComponentTemplate(Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass));

                const FString ComponentLabel = FString::Printf(

                    TEXT("SCSComponent.%s (%s)"),

                    *SCSNode->GetVariableName().ToString(),

                    *GetNameSafe(ComponentTemplate ? ComponentTemplate->GetClass() : nullptr));



if (NodeOffset++ >= ResumeOffset)
        {
            if (FPlatformTime::Seconds() >= Deadline) { ResumeOffset = NodeOffset - 1; return false; }
                ScanTemplateObjectUsage(

                    Blueprint,

                    ComponentTemplate,

                    TEXT("SCS Component Template"),

                    ComponentLabel,

                    TargetObject,

                    ObjectPath,

                    PackageName,

                    OutFindings);
        }

            }

        }

        ResumeOffset = 0;
        return true;
    }

    TArray<FAssetData> GetAssetUsageSelectedContentBrowserAssets(const FToolMenuContext& Context)

    {

        if (const UContentBrowserAssetContextMenuContext* AssetContext = Context.FindContext<UContentBrowserAssetContextMenuContext>())

        {

            if (!AssetContext->SelectedAssets.IsEmpty())

            {

                return AssetContext->SelectedAssets;

            }

        }

        TArray<FAssetData> SelectedAssets;

        if (FModuleManager::Get().IsModuleLoaded(TEXT("ContentBrowser")))

        {

            FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

            ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

        }

        return SelectedAssets;

    }



    TArray<FAssetData> BuildCandidateQueue(const FAssetData& TargetAsset)

    {

        TArray<FAssetData> Result;

        TSet<FName> AddedPackages;

        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));



        auto AddBlueprintAssetsFromPackage = [&Result, &AddedPackages, &AssetRegistryModule](FName PackageName)

        {

            if (PackageName.IsNone() || AddedPackages.Contains(PackageName) || !IsSearchablePackageName(PackageName))

            {

                return;

            }



            TArray<FAssetData> PackageAssets;

            AssetRegistryModule.Get().GetAssetsByPackageName(PackageName, PackageAssets);

            for (const FAssetData& PackageAsset : PackageAssets)

            {

                if (IsBlueprintAssetData(PackageAsset))

                {

                    AddedPackages.Add(PackageAsset.PackageName);

                    Result.Add(PackageAsset);

                    return;

                }

            }

        };



        FAssetRegistryDependencyOptions ReferenceOptions;

        ReferenceOptions.bIncludeHardPackageReferences = true;

        ReferenceOptions.bIncludeSoftPackageReferences = true;

        ReferenceOptions.bIncludeSearchableNames = true;

        ReferenceOptions.bIncludeHardManagementReferences = false;

        ReferenceOptions.bIncludeSoftManagementReferences = false;



        TArray<FName> ReferencerPackages;

        AssetRegistryModule.Get().K2_GetReferencers(TargetAsset.PackageName, ReferenceOptions, ReferencerPackages);

        for (const FName& ReferencerPackage : ReferencerPackages)

        {

            AddBlueprintAssetsFromPackage(ReferencerPackage);

        }



        return Result;

    }

    class STMAssetUsageLocator : public SCompoundWidget

    {

    public:

        SLATE_BEGIN_ARGS(STMAssetUsageLocator) {}

            SLATE_ARGUMENT(FAssetData, TargetAsset)

        SLATE_END_ARGS()



        void Construct(const FArguments& InArgs)

        {

            TargetAsset = InArgs._TargetAsset;

            TargetObjectPath = TargetAsset.GetSoftObjectPath().ToString();

            TargetPackageName = TargetAsset.PackageName.ToString();

            TargetAssetName = TargetAsset.AssetName.ToString();

            StartScan();

            ChildSlot

            [

                SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight().Padding(8.0f)

                [

                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().FillWidth(1.0f)

                    [

                        SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Asset Usage Locator - %s"), *TargetAssetName))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))]

                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SNew(STextBlock).Text(FText::FromString(TargetObjectPath)).ColorAndOpacity(FLinearColor(0.68f, 0.68f, 0.68f)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))]

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
                            StartScan();
                            return FReply::Handled();
                        })
                    ]

                    + SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f).VAlign(VAlign_Center)[SNew(SButton).Text(TMLoc::Text(TEXT("Rescan"), TEXT("Rescan"))).OnClicked(this, &STMAssetUsageLocator::OnRescanClicked)]

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f)[SNew(SButton).Text(TMLoc::Text(TEXT("Cancel search (keep results)"), TEXT("검색 취소 (결과 유지)"))).IsEnabled_Lambda([this]() { return !bScanFinished; }).OnClicked_Lambda([this]() { bScanCancelled = true; return FReply::Handled(); })]
                + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f)[SNew(STextBlock).AutoWrapText(true).Text(TMLoc::Text(TEXT("Scope: Asset Registry Blueprint candidates. Property inspection is bounded (depth 5, first 256 array entries). Text matches require confirmation."), TEXT("범위: Asset Registry의 BP 참조 후보. 속성 검사는 깊이 5, 배열 앞 256개로 제한됩니다. 텍스트 일치는 실제 참조 확인이 필요합니다.")))]
                + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f)[SNew(STextBlock).AutoWrapText(true).Text(this, &STMAssetUsageLocator::GetStatusText).ColorAndOpacity(FLinearColor(0.75f, 0.85f, 1.0f))]

                + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 6.0f)[SNew(SSearchBox).HintText(TMLoc::Text(TEXT("Filter by Blueprint, graph, node, property, reason"), TEXT("Filter by Blueprint, graph, node, property, reason"))).OnTextChanged(this, &STMAssetUsageLocator::OnFilterTextChanged)]

                + SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f, 0.0f, 8.0f, 8.0f)

                [

                    SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).Padding(2.0f)

                    [

                        SAssignNew(ResultListView, SListView<FFindingPtr>)

                            .ListItemsSource(&FilteredFindings)

                            .OnGenerateRow(this, &STMAssetUsageLocator::GenerateRow)

                            .OnMouseButtonDoubleClick(this, &STMAssetUsageLocator::OpenFinding)

                    ]

                ]

            ];

        }



        ~STMAssetUsageLocator()

        {

            if (TickerHandle.IsValid())

            {

                FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

            }

            if (ActiveLoadHandle.IsValid()) ActiveLoadHandle->CancelHandle();

        }



    private:

        void StartScan()

        {

            if (TickerHandle.IsValid())

            {

                FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

            }

            Findings.Reset();

            FilteredFindings.Reset();
            FindingKeys.Reset();
            bFindingsTruncated = false;
            bResultsDirty = false;
            TraceSession.Begin();
            CancellationGeneration = TMPerf::GetCancellationGeneration();

            CandidateQueue = BuildCandidateQueue(TargetAsset);

            TotalCandidates = CandidateQueue.Num();

            ScannedCandidates = 0;
            FailedCandidates = 0;
            bScanCancelled = false;
            ActiveBlueprint.Reset();
            ActiveNodeOffset = 0;

            bScanFinished = false;

            if (ActiveLoadHandle.IsValid()) ActiveLoadHandle->CancelHandle();
            ActiveLoadHandle.Reset();
            PendingCandidate = FAssetData();

            TargetObject = TargetAsset.FastGetAsset(false);

            TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &STMAssetUsageLocator::TickScan), TMPerf::AssetUsageTickerIntervalSeconds());

        }



        bool TickScan(float)
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(TraceMotive_AssetUsageTick);
            if (CancellationGeneration != TMPerf::GetCancellationGeneration()) bScanCancelled = true;
            if (bScanCancelled)
            {
                if (ActiveLoadHandle.IsValid()) ActiveLoadHandle->CancelHandle();
                ActiveLoadHandle.Reset();
                ActiveBlueprint.Reset();
                bScanFinished = true;
                TraceSession.Cancel();
                RebuildFilter();
                TickerHandle.Reset();
                return false;
            }
            const double Deadline = FPlatformTime::Seconds() + TMPerf::AssetUsageTickBudgetSeconds();
            const int32 InitialFindings = Findings.Num();
            while (FPlatformTime::Seconds() < Deadline)
            {
                if (ActiveLoadHandle.IsValid())
                {
                    if (!ActiveLoadHandle->HasLoadCompleted()) break;
                    ActiveBlueprint.Reset(Cast<UBlueprint>(PendingCandidate.FastGetAsset(false)));
                    ActiveLoadHandle.Reset();
                    PendingCandidate = FAssetData();
                    if (!ActiveBlueprint.IsValid()) { ++FailedCandidates; ++ScannedCandidates; }
                }
                if (ActiveBlueprint.IsValid())
                {
                    FScopedAssetUsageFindingStore FindingStore(FindingKeys, bFindingsTruncated);
                    if (!ScanBlueprint(ActiveBlueprint.Get(), TargetObject.Get(), TargetObjectPath, TargetPackageName, TargetAssetName, Findings, ActiveNodeOffset, Deadline)) break;
                    if (Findings.Num() == CandidateFindingStart && !bFindingsTruncated)
                        AddFinding(Findings, ActiveBlueprint.Get(), nullptr, TEXT("Reference Viewer Candidate"), TEXT("Reference exists; exact location was not resolved."), TEXT("Asset Registry referencer"));
                    ActiveBlueprint.Reset();
                    ++ScannedCandidates;
                }
                if (CandidateQueue.IsEmpty()) break;
                const FAssetData Candidate = CandidateQueue.Pop();
                CandidateFindingStart = Findings.Num();
                ActiveNodeOffset = 0;
                ActiveBlueprint.Reset(Cast<UBlueprint>(Candidate.FastGetAsset(false)));
                if (!ActiveBlueprint.IsValid())
                {
                    PendingCandidate = Candidate;
                    ActiveLoadHandle = StreamableManager.RequestAsyncLoad(Candidate.ToSoftObjectPath());
                    if (!ActiveLoadHandle.IsValid()) { ++FailedCandidates; ++ScannedCandidates; PendingCandidate = FAssetData(); }
                }
            }
            bResultsDirty |= InitialFindings != Findings.Num();
            if (bResultsDirty && TraceSession.ShouldRefreshUi(FPlatformTime::Seconds(), AssetUsageUiRefreshIntervalSeconds))
            {
                RebuildFilter();
                bResultsDirty = false;
            }
            if (CandidateQueue.IsEmpty() && !ActiveLoadHandle.IsValid() && !ActiveBlueprint.IsValid())
            {
                bScanFinished = true;
                TraceSession.Complete();
                RebuildFilter();
                TickerHandle.Reset();
                return false;
            }
            return true;
        }

        FText GetStatusText() const
        {
            int32 Candidates = 0;
            for (const FFindingPtr& Finding : Findings)
                if (Finding->IsCandidate()) ++Candidates;
            const FText State = bScanCancelled ? TMLoc::Text(TEXT("Cancelled - partial results"), TEXT("취소됨 - 일부 결과"))
                : !bScanFinished ? TMLoc::Text(TEXT("Scanning"), TEXT("검색 중"))
                : (bFindingsTruncated || FailedCandidates > 0) ? TMLoc::Text(TEXT("Completed with gaps"), TEXT("일부 누락 상태로 완료"))
                : TMLoc::Text(TEXT("Completed"), TEXT("완료"));
            return FText::Format(TMLoc::Text(TEXT("{0} | BP {1}/{2} | Verified {3} | Candidates {4} | Load failures {5} | Result limit reached: {6}"), TEXT("{0} | BP {1}/{2} | 확인됨 {3} | 후보 {4} | 로딩 실패 {5} | 결과 제한 도달: {6}")),
                State, FText::AsNumber(ScannedCandidates), FText::AsNumber(TotalCandidates),
                FText::AsNumber(Findings.Num()-Candidates), FText::AsNumber(Candidates), FText::AsNumber(FailedCandidates),
                bFindingsTruncated ? TMLoc::Text(TEXT("Yes"), TEXT("예")) : TMLoc::Text(TEXT("No"), TEXT("아니요")));
        }

        void OnFilterTextChanged(const FText& InText)

        {

            FilterText = InText.ToString();

            RebuildFilter();

        }



        void RebuildFilter()

        {

            FilteredFindings.Reset();

            for (const FFindingPtr& Finding : Findings)

            {

                if (Finding.IsValid() && (FilterText.IsEmpty() || Finding->BuildSearchText().Contains(FilterText)))

                {

                    FilteredFindings.Add(Finding);

                }

            }

            if (ResultListView.IsValid())

            {

                ResultListView->RequestListRefresh();

            }

        }



        TSharedRef<ITableRow> GenerateRow(FFindingPtr Finding, const TSharedRef<STableViewBase>& OwnerTable)

        {

            return SNew(STableRow<FFindingPtr>, OwnerTable)

            [

                SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.055f, 0.055f, 0.055f, 1.0f)).Padding(FMargin(8.0f, 6.0f))

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Finding->IsCandidate() ? TMLoc::Text(TEXT("Candidate - confirm this reference"), TEXT("후보 - 실제 참조를 확인하세요")) : TMLoc::Text(TEXT("Verified reference"), TEXT("확인된 참조"))).ColorAndOpacity(Finding->IsCandidate() ? FLinearColor(1.f, .65f, .2f) : FLinearColor(.3f, .85f, .55f))]
                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Finding.IsValid() ? FString::Printf(TEXT("%s / %s    [%s]"), *Finding->BlueprintName, *Finding->FunctionName, *Finding->LocationKind) : TEXT("Invalid"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))]

                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SNew(STextBlock).Text(FText::FromString(Finding.IsValid() ? FString::Printf(TEXT("Graph: %s   Node: %s"), *Finding->GraphName, *Finding->NodeTitle) : FString())).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))]

                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SNew(STextBlock).Text(FText::FromString(Finding.IsValid() ? Finding->Detail : FString())).ColorAndOpacity(FLinearColor(0.78f, 0.78f, 0.78f)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))]

                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SNew(STextBlock).Text(FText::FromString(Finding.IsValid() ? FString::Printf(TEXT("Reason: %s"), *Finding->Reason) : FString())).ColorAndOpacity(FLinearColor(0.62f, 0.82f, 0.64f)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))]

                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(TMLoc::Text(TEXT("Open Location"), TEXT("Open Location"))).OnClicked(this, &STMAssetUsageLocator::OnOpenFindingClicked, Finding)]+SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)[SNew(SButton).Text(TMLoc::Text(TEXT("Open BP"), TEXT("Open BP"))).OnClicked(this, &STMAssetUsageLocator::OnOpenBlueprintClicked, Finding)]]

                ]

            ];

        }



        FReply OnOpenFindingClicked(FFindingPtr Finding) { OpenFinding(Finding); return FReply::Handled(); }

        FReply OnOpenBlueprintClicked(FFindingPtr Finding) { OpenBlueprint(Finding); return FReply::Handled(); }

        FReply OnRescanClicked() { StartScan(); RebuildFilter(); return FReply::Handled(); }



        void OpenBlueprint(const FFindingPtr& Finding) const

        {

            if (Finding.IsValid() && Finding->Blueprint.IsValid() && GEditor)

            {

                GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Finding->Blueprint.Get());

            }

        }



        void OpenFinding(FFindingPtr Finding) const

        {

            if (!Finding.IsValid()) return;

            if (UEdGraphNode* Node = Finding->Node.Get())

            {

                FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node);

                return;

            }

            OpenBlueprint(Finding);

        }



        FAssetData TargetAsset;

        FString TargetObjectPath;

        FString TargetPackageName;

        FString TargetAssetName;

        TWeakObjectPtr<UObject> TargetObject;

        TArray<FAssetData> CandidateQueue;

        TArray<FFindingPtr> Findings;

        TArray<FFindingPtr> FilteredFindings;
        TSet<FString> FindingKeys;

        TSharedPtr<SListView<FFindingPtr>> ResultListView;

        FTSTicker::FDelegateHandle TickerHandle;
        FStreamableManager StreamableManager;
        TSharedPtr<FStreamableHandle> ActiveLoadHandle;
        FAssetData PendingCandidate;
        uint64 CancellationGeneration = 0;

        FString FilterText;

        int32 TotalCandidates = 0;

        TStrongObjectPtr<UBlueprint> ActiveBlueprint;
        int32 ActiveNodeOffset = 0;
        int32 CandidateFindingStart = 0;
        int32 FailedCandidates = 0;
        bool bScanCancelled = false;
        int32 ScannedCandidates = 0;

        bool bScanFinished = false;
        bool bFindingsTruncated = false;
        bool bResultsDirty = false;
        FTMTraceSession TraceSession;

    };

}



#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTMAssetUsageEvidenceTest, "TraceMotive.Search.AssetUsageEvidenceAndResume", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTMAssetUsageEvidenceTest::RunTest(const FString&)
{
    UObject* Target = NewObject<UBlueprint>();
    UObject* Other = NewObject<UBlueprint>();
    TestTrue(TEXT("Exact object is matched"), ObjectMatchesTarget(Target, Target, Target->GetPathName(), Target->GetOutermost()->GetName()));
    TestFalse(TEXT("Other object in same package is excluded"), ObjectMatchesTarget(Other, Target, Target->GetPathName(), Target->GetOutermost()->GetName()));
    UBlueprint* BP = NewObject<UBlueprint>();
    UEdGraph* Graph = NewObject<UEdGraph>(BP);
    BP->UbergraphPages.Add(Graph);
    UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph);
    Graph->AddNode(Node);
    UEdGraphPin* Pin = Node->CreatePin(EGPD_Input, TEXT("object"), TEXT("Asset"));
    Pin->DefaultObject = Target;
    TArray<FFindingPtr> Findings;
    int32 Offset = 0;
    TestFalse(TEXT("Expired budget pauses scan"), ScanBlueprint(BP, Target, Target->GetPathName(), Target->GetOutermost()->GetName(), Target->GetName(), Findings, Offset, 0.0));
    TestTrue(TEXT("Later slice completes"), ScanBlueprint(BP, Target, Target->GetPathName(), Target->GetOutermost()->GetName(), Target->GetName(), Findings, Offset, TNumericLimits<double>::Max()));
    bool bFoundExactPin = false;
    for (const FFindingPtr& Finding : Findings)
        if (!Finding->IsCandidate() && Finding->Node.Get() == Node && Finding->Reason == TEXT("Exact asset object stored on node pin")) bFoundExactPin = true;
    TestTrue(TEXT("Result points to the actual pin owner node"), bFoundExactPin);
    FTMAssetUsageFinding Candidate;
    Candidate.Reason = TEXT("Node metadata contains target");
    TestTrue(TEXT("Text-only match remains a candidate"), Candidate.IsCandidate());
    TUniquePtr<FSoftObjectProperty> SoftProperty = MakeUnique<FSoftObjectProperty>(FFieldVariant(), FName(TEXT("Asset")));
    FSoftObjectPtr SoftValue(FSoftObjectPath(TEXT("/Game/TMFixture.Target")));
    TArray<FString> SoftMatches;
    TestTrue(TEXT("Unloaded soft object path resolves without loading"), ScanPropertyValue(SoftProperty.Get(), &SoftValue, TEXT("Asset"), nullptr, TEXT("/Game/TMFixture.Target"), TEXT("/Game/TMFixture"), SoftMatches, 0));
    SoftMatches.Reset();
    TestFalse(TEXT("Soft reference in same package with different object is excluded"), ScanPropertyValue(SoftProperty.Get(), &SoftValue, TEXT("Asset"), nullptr, TEXT("/Game/TMFixture.Other"), TEXT("/Game/TMFixture"), SoftMatches, 0));
    return true;
}
#endif

namespace TMAssetUsageLocator

{

    void OpenWindowForAsset(const FAssetData& AssetData)

    {

        if (!AssetData.IsValid()) return;

        TMDockTab::OpenDockTab(TEXT("AssetUsageLocator"), FText::FromString(FString::Printf(TEXT("Asset Usage - %s"), *AssetData.AssetName.ToString())), SNew(STMAssetUsageLocator).TargetAsset(AssetData));

    }



    void RegisterMenus()

    {

        if (UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.AssetContextMenu")))

        {

            FToolMenuSection& Section = AssetMenu->FindOrAddSection(TEXT("TraceMotive"));

            Section.AddMenuEntry(TEXT("TMFindExactBlueprintAssetUsage"), TMLoc::Text(TEXT("Find Exact Blueprint Usage"), TEXT("Find Exact Blueprint Usage")), TMLoc::Text(TEXT("Find exact Blueprint graph node, pin, or default property usage for this asset."), TEXT("Find exact Blueprint graph node, pin, or default property usage for this asset.")), FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.AssetUsageLocator")), FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& Context)

            {

                const TArray<FAssetData> SelectedAssets = GetAssetUsageSelectedContentBrowserAssets(Context);

                if (SelectedAssets.Num() > 0)

                {

                    OpenWindowForAsset(SelectedAssets[0]);

                }

            }));

        }

    }

}

















