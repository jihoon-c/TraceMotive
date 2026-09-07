#include "SInstanceReferenceTracker.h"



#include "TMInstanceTraceFormat.h"

#include "TMInstanceTraceGlobalManager.h"
#include "TMTraceNoisePolicy.h"

#include "TMLocalization.h"

#include "EdGraph/EdGraph.h"

#include "EdGraph/EdGraphNode.h"

#include "EdGraph/EdGraphPin.h"

#include "EdGraphSchema_K2.h"

#include "Editor.h"

#include "Components/ActorComponent.h"

#include "Components/ChildActorComponent.h"

#include "Components/PrimitiveComponent.h"

#include "Components/SceneComponent.h"

#include "Engine/Blueprint.h"
#include "Engine/CollisionProfile.h"
#include "Engine/EngineTypes.h"

#include "Engine/Engine.h"

#include "Engine/Level.h"

#include "Engine/LevelScriptActor.h"

#include "Engine/LevelScriptBlueprint.h"

#include "Engine/World.h"

#include "EngineUtils.h"

#include "GameFramework/Actor.h"

#include "K2Node.h"

#include "K2Node_CallFunction.h"

#include "K2Node_Literal.h"

#include "K2Node_Variable.h"

#include "ClassIconFinder.h"

#include "Brushes/SlateRoundedBoxBrush.h"

#include "Framework/Application/SlateApplication.h"

#include "HAL/FileManager.h"

#include "HAL/PlatformApplicationMisc.h"

#include "HAL/PlatformTime.h"

#include "InputCoreTypes.h"

#include "Misc/ConfigCacheIni.h"

#include "Misc/DateTime.h"

#include "Misc/FileHelper.h"

#include "Misc/Paths.h"

#include "Subsystems/AssetEditorSubsystem.h"

#include "Styling/AppStyle.h"

#include "Styling/SlateIconFinder.h"

#include "UObject/Script.h"

#include "UObject/Stack.h"

#include "UObject/UnrealType.h"

#include "UObject/UObjectGlobals.h"

#include "Widgets/Input/SButton.h"

#include "Widgets/Input/SCheckBox.h"

#include "Widgets/Input/SComboButton.h"

#include "Widgets/Input/SSearchBox.h"

#include "Widgets/InvalidateWidgetReason.h"

#include "Widgets/Images/SImage.h"

#include "Widgets/Layout/SBorder.h"

#include "Widgets/Layout/SBox.h"

#include "Widgets/Layout/SExpandableArea.h"

#include "Widgets/Layout/SScrollBox.h"

#include "Widgets/Layout/SSeparator.h"

#include "Widgets/Layout/SSplitter.h"

#include "Widgets/Layout/SWrapBox.h"

#include "Widgets/SBoxPanel.h"

#include "Widgets/Text/STextBlock.h"

#include "Widgets/Views/STableRow.h"



namespace

{

    using namespace TMInstanceTraceFormat;



    constexpr const TCHAR* InstanceTraceVersion = TEXT("TraceMotive Instance Trace v2.6 - attached actor component state monitor");

    constexpr const TCHAR* InstanceTraceLayoutConfigSection = TEXT("TraceMotive.InstanceTraceWidget");

    constexpr float MinHierarchySplitRatio = 0.15f;

    constexpr float MaxHierarchySplitRatio = 0.85f;

    constexpr float MinRawLogDrawerHeight = 24.0f;

    constexpr float DefaultRawLogDrawerHeight = 160.0f;



    struct FInstanceReferenceDisplayRow

    {

        TWeakObjectPtr<AActor> Actor;

        FString InstanceName;

        FString ClassName;

        TArray<FString> PropertyPaths;

    };



    struct FCachedComponentStateProperties

    {

        TArray<const FProperty*> KeywordProperties;

        TArray<const FProperty*> BroadProperties;

    };



    constexpr float StatePollIntervalSeconds = 0.1f;

    constexpr float ReferenceScanIntervalSeconds = 1.0f;

    constexpr float TreeRefreshIntervalSeconds = 0.5f;

    constexpr double TrackerTickBudgetSeconds = 0.006;

    constexpr double ReferenceScanTickBudgetSeconds = 0.0035;

    constexpr double HookStateScanMinIntervalSeconds = 0.05;

    constexpr double DeferredUiRefreshIntervalSeconds = 0.10;

    constexpr double MaxReferenceScanDurationSeconds = 8.0;

    constexpr int32 MaxReferenceContainerElements = 128;

    constexpr int32 MaxReferenceStructDepth = 8;

    constexpr int32 MaxReferencePathsPerActor = 128;

    constexpr int32 MaxBlueprintGraphNodesPerActor = 512;

    constexpr int32 MaxReferencerRowsToRender = 100;

    constexpr int32 MaxReferencerPathsToDescribe = 12;



    FString BoolToText(const bool bValue)

    {

        return bValue ? TEXT("true") : TEXT("false");

    }



    FString VectorToCompactText(const FVector& Value)

    {

        return FString::Printf(TEXT("X=%.2f,Y=%.2f,Z=%.2f"), Value.X, Value.Y, Value.Z);

    }



    FString RotatorToCompactText(const FRotator& Value)

    {

        return FString::Printf(TEXT("P=%.2f,Y=%.2f,R=%.2f"), Value.Pitch, Value.Yaw, Value.Roll);

    }



    FString NameOrNoneToText(const FName Value)

    {

        return Value.IsNone() ? TEXT("None") : Value.ToString();

    }




    FString GetTraceCollisionResponseText(ECollisionResponse Value)

    {

        switch (Value)

        {

        case ECR_Ignore: return TEXT("Ignore");

        case ECR_Overlap: return TEXT("Overlap");

        case ECR_Block: return TEXT("Block");

        default: return TEXT("Unknown");

        }

    }



    FString GetTraceCollisionChannelText(ECollisionChannel Channel)

    {

        if (const UCollisionProfile* Profile = UCollisionProfile::Get())

        {

            const FName ConfiguredName = Profile->ReturnChannelNameFromContainerIndex(static_cast<int32>(Channel));

            if (!ConfiguredName.IsNone())

            {

                return ConfiguredName.ToString();

            }

        }

        if (const UEnum* Enum = StaticEnum<ECollisionChannel>())

        {

            FString Name = Enum->GetNameStringByValue(static_cast<int64>(Channel));

            Name.RemoveFromStart(TEXT("ECC_"));

            return Name;

        }

        return FString::Printf(TEXT("Channel%d"), static_cast<int32>(Channel));

    }



    FString GetActorLabelForTrace(const AActor* Actor);

    FString GetActorClassNameForTrace(const AActor* Actor);



    bool TextContainsTraceKeyword(const FString& Text)

    {

        static const TCHAR* Keywords[] =

        {

            TEXT("Visible"),

            TEXT("Visibility"),

            TEXT("Hidden"),

            TEXT("Hide"),

            TEXT("Dissolve"),

            TEXT("Enable"),

            TEXT("Enabled"),

            TEXT("Active"),

            TEXT("Opacity"),

            TEXT("Alpha"),

            TEXT("Material"),

            TEXT("Render"),

        };



        for (const TCHAR* Keyword : Keywords)

        {

            if (Text.Contains(Keyword, ESearchCase::IgnoreCase))

            {

                return true;

            }

        }



        return false;

    }



    bool IsSupportedComponentStateProperty(const FProperty* Property)

    {

        return Property

            && (CastField<FBoolProperty>(Property)

                || CastField<FEnumProperty>(Property)

                || CastField<FNumericProperty>(Property)

                || CastField<FNameProperty>(Property)

                || CastField<FStrProperty>(Property)

                || CastField<FTextProperty>(Property)

                || CastField<FObjectPropertyBase>(Property));

    }




    bool IsVolatileComponentStatePropertyName(const FString& PropertyName)

    {

        return PropertyName.Contains(TEXT("Tick"), ESearchCase::IgnoreCase)

            || PropertyName.Contains(TEXT("PrimaryActorTick"), ESearchCase::IgnoreCase)

            || PropertyName.Contains(TEXT("PrimaryComponentTick"), ESearchCase::IgnoreCase)

            || PropertyName.Contains(TEXT("TickFunction"), ESearchCase::IgnoreCase)

            || PropertyName.Contains(TEXT("LastPoseTickFrame"), ESearchCase::IgnoreCase)

            || PropertyName.Contains(TEXT("PoseTickFrame"), ESearchCase::IgnoreCase)

            || PropertyName.Contains(TEXT("LastRenderTime"), ESearchCase::IgnoreCase)

            || PropertyName.Contains(TEXT("LastSubmitTime"), ESearchCase::IgnoreCase)

            || PropertyName.Contains(TEXT("LastRenderTimeOnScreen"), ESearchCase::IgnoreCase)

            || PropertyName.Contains(TEXT("CachedFrame"), ESearchCase::IgnoreCase)

            || PropertyName.Contains(TEXT("FrameCounter"), ESearchCase::IgnoreCase);

    }



    const FCachedComponentStateProperties& GetCachedComponentStateProperties(const UClass* ComponentClass)

    {

        static TMap<const UClass*, FCachedComponentStateProperties> Cache;



        if (const FCachedComponentStateProperties* CachedProperties = Cache.Find(ComponentClass))

        {

            return *CachedProperties;

        }



        FCachedComponentStateProperties NewCacheEntry;

        if (ComponentClass)

        {

            for (TFieldIterator<FProperty> PropertyIt(ComponentClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)

            {

                const FProperty* Property = *PropertyIt;

                if (!Property || !IsSupportedComponentStateProperty(Property) || Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_EditorOnly))

                {

                    continue;

                }



                const FString PropertyName = Property->GetName();

                if (IsVolatileComponentStatePropertyName(PropertyName))

                {

                    continue;

                }



                NewCacheEntry.BroadProperties.Add(Property);

                const FString PropertyDisplayName = Property->GetDisplayNameText().ToString();

                if (TextContainsTraceKeyword(PropertyName) || TextContainsTraceKeyword(PropertyDisplayName))

                {

                    NewCacheEntry.KeywordProperties.Add(Property);

                }

            }

        }



        return Cache.Add(ComponentClass, MoveTemp(NewCacheEntry));

    }



    FString ReadComponentStatePropertyValue(const FProperty* Property, const void* ValuePtr)

    {

        if (!Property || !ValuePtr)

        {

            return TEXT("<unreadable>");

        }



        if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))

        {

            return BoolToText(BoolProperty->GetPropertyValue(ValuePtr));

        }



        if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))

        {

            const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();

            const int64 RawValue = UnderlyingProperty ? UnderlyingProperty->GetSignedIntPropertyValue(ValuePtr) : 0;

            return EnumProperty->GetEnum() ? EnumProperty->GetEnum()->GetNameStringByValue(RawValue) : FString::Printf(TEXT("%lld"), RawValue);

        }



        if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))

        {

            const uint8 RawValue = ByteProperty->GetPropertyValue(ValuePtr);

            return ByteProperty->GetIntPropertyEnum() ? ByteProperty->GetIntPropertyEnum()->GetNameStringByValue(RawValue) : FString::FromInt(RawValue);

        }



        if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))

        {

            if (NumericProperty->IsFloatingPoint())

            {

                return FString::SanitizeFloat(NumericProperty->GetFloatingPointPropertyValue(ValuePtr));

            }



            return FString::Printf(TEXT("%lld"), NumericProperty->GetSignedIntPropertyValue(ValuePtr));

        }



        if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))

        {

            return NameOrNoneToText(NameProperty->GetPropertyValue(ValuePtr));

        }



        if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))

        {

            return StringProperty->GetPropertyValue(ValuePtr);

        }



        if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))

        {

            return TextProperty->GetPropertyValue(ValuePtr).ToString();

        }



        if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))

        {

            const UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue(ValuePtr);

            if (!ObjectValue)

            {

                return TEXT("None");

            }



            if (const AActor* ActorValue = Cast<AActor>(ObjectValue))

            {

                return FString::Printf(TEXT("%s(%s)"), *GetActorLabelForTrace(ActorValue), *GetActorClassNameForTrace(ActorValue));

            }



            return FString::Printf(TEXT("%s(%s)"),

                *ObjectValue->GetName(),

                ObjectValue->GetClass() ? *ObjectValue->GetClass()->GetName() : TEXT("<Invalid Class>"));

        }



        return TEXT("<unsupported>");

    }



    void CaptureInterestingComponentProperties(const UActorComponent* Component, const FString& ComponentPrefix, TMap<FString, FString>& State)

    {

        if (!Component || !Component->GetClass())

        {

            return;

        }



        const bool bComponentLooksRelevant = TextContainsTraceKeyword(Component->GetName())

            || (Component->GetClass() && TextContainsTraceKeyword(Component->GetClass()->GetName()));

        const FCachedComponentStateProperties& CachedProperties = GetCachedComponentStateProperties(Component->GetClass());

        const TArray<const FProperty*>& PropertiesToCapture = bComponentLooksRelevant ? CachedProperties.BroadProperties : CachedProperties.KeywordProperties;



        for (const FProperty* Property : PropertiesToCapture)

        {

            const int32 ArrayDim = FMath::Min(Property->ArrayDim, 32);

            for (int32 ArrayIndex = 0; ArrayIndex < ArrayDim; ++ArrayIndex)

            {

                const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Component, ArrayIndex);

                const FString PropertyKey = Property->ArrayDim > 1

                    ? FString::Printf(TEXT("%s.Property.%s[%d]"), *ComponentPrefix, *Property->GetName(), ArrayIndex)

                    : FString::Printf(TEXT("%s.Property.%s"), *ComponentPrefix, *Property->GetName());

                State.Add(PropertyKey, ReadComponentStatePropertyValue(Property, ValuePtr));

            }

        }

    }



    FString JoinStateKeySegments(const TArray<FString>& Segments, const int32 LastInclusiveIndex)

    {

        TArray<FString> UsedSegments;

        for (int32 Index = 0; Index <= LastInclusiveIndex && Segments.IsValidIndex(Index); ++Index)

        {

            UsedSegments.Add(Segments[Index]);

        }



        return FString::Join(UsedSegments, TEXT("."));

    }



    FString ExtractTrackedNodeKeyFromStateKey(const FString& StateKey)

    {

        if (StateKey.StartsWith(TEXT("Actor.")))

        {

            return TEXT("Actor");

        }



        TArray<FString> Segments;

        StateKey.ParseIntoArray(Segments, TEXT("."), true);



        FString BestNodeKey;

        for (int32 Index = 0; Index < Segments.Num(); ++Index)

        {

            if (Segments[Index] == TEXT("Component") && Segments.IsValidIndex(Index + 1))

            {

                BestNodeKey = JoinStateKeySegments(Segments, Index + 1);

                ++Index;

                continue;

            }



            if (Segments[Index] == TEXT("ChildActor") && Segments.IsValidIndex(Index + 2))

            {

                BestNodeKey = JoinStateKeySegments(Segments, Index + 2);

                Index += 2;

                continue;

            }



            if (Segments[Index] == TEXT("AttachedActor") && Segments.IsValidIndex(Index + 1))

            {

                BestNodeKey = JoinStateKeySegments(Segments, Index + 1);

                ++Index;

            }

        }



        return BestNodeKey.IsEmpty() ? TEXT("Actor") : BestNodeKey;

    }



    FString BuildStateLabelForNode(const FString& StateKey, const FString& NodeKey)

    {

        FString Label = StateKey;

        const FString Prefix = NodeKey + TEXT(".");

        if (StateKey.StartsWith(Prefix))

        {

            Label = StateKey.RightChop(Prefix.Len());

        }

        else if (NodeKey == TEXT("Actor") && StateKey.StartsWith(TEXT("Actor.")))

        {

            Label = StateKey.RightChop(6);

        }



        Label.ReplaceInline(TEXT(".Property."), TEXT(" / "));

        Label.ReplaceInline(TEXT("Property."), TEXT(""));

        return Label;

    }


    bool IsNoisyInstanceTraceComponentClass(const UClass* Class)

    {

        if (!Class)

        {

            return false;

        }

        for (const UClass* Current = Class; Current; Current = Current->GetSuperClass())

        {

            const FString Name = Current->GetName();

            if (Name.Contains(TEXT("TextRender"), ESearchCase::IgnoreCase)

                || Name.Contains(TEXT("Arrow"), ESearchCase::IgnoreCase)

                || Name.Contains(TEXT("Billboard"), ESearchCase::IgnoreCase)

                || Name.Contains(TEXT("Audio"), ESearchCase::IgnoreCase)

                || Name.Contains(TEXT("Sound"), ESearchCase::IgnoreCase))

            {

                return true;

            }

        }

        return false;

    }



    bool IsCollisionTraceStateKey(const FString& StateKey)

    {

        return StateKey.Contains(TEXT("Collision"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("Response"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("GenerateOverlapEvents"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("SimulatingPhysics"), ESearchCase::IgnoreCase);

    }



    bool IsCollisionTraceReason(const FString& Reason)

    {

        return Reason.Contains(TEXT("Collision"), ESearchCase::IgnoreCase)

            || Reason.Contains(TEXT("BodyInstance"), ESearchCase::IgnoreCase)

            || Reason.Contains(TEXT("Response"), ESearchCase::IgnoreCase)

            || Reason.Contains(TEXT("Overlap"), ESearchCase::IgnoreCase)

            || Reason.Contains(TEXT("Physics"), ESearchCase::IgnoreCase);

    }



    bool IsIgnoredInstanceTraceReason(const FString& Reason)

    {

        if (IsCollisionTraceReason(Reason))

        {

            return false;

        }

        return Reason.Contains(TEXT("Tick"), ESearchCase::IgnoreCase)

            || Reason.Contains(TEXT("PrimaryActorTick"), ESearchCase::IgnoreCase)

            || Reason.Contains(TEXT("PrimaryComponentTick"), ESearchCase::IgnoreCase)

            || Reason.Contains(TEXT("TickFunction"), ESearchCase::IgnoreCase)

            || Reason.Contains(TEXT("bCanEverTick"), ESearchCase::IgnoreCase)

            || Reason.Contains(TEXT("bStartWithTickEnabled"), ESearchCase::IgnoreCase)

            || Reason.Contains(TEXT("TickInterval"), ESearchCase::IgnoreCase)

            || Reason.Contains(TEXT("LastPoseTickFrame"), ESearchCase::IgnoreCase)

            || Reason.Contains(TEXT("FrameCounter"), ESearchCase::IgnoreCase);

    }



    bool IsNoisyInstanceTraceStateKey(const FString& StateKey)

    {

        if (IsCollisionTraceStateKey(StateKey))

        {

            return false;

        }

        if (StateKey.Contains(TEXT("TextRender"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("Arrow"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("Billboard"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("Audio"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("Sound"), ESearchCase::IgnoreCase))

        {

            return true;

        }

        if (StateKey.Contains(TEXT("Tick"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("PrimaryActorTick"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("PrimaryComponentTick"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("TickFunction"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("LastPoseTickFrame"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("PoseTickFrame"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("LastRenderTime"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("LastSubmitTime"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("LastRenderTimeOnScreen"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("CachedFrame"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("FrameCounter"), ESearchCase::IgnoreCase))

        {

            return true;

        }

        if (StateKey.Contains(TEXT("WorldLocation"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("WorldRotation"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("WorldScale"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("RelativeLocation"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("RelativeRotation"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("RelativeScale"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("AttachParent"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("AttachSocket"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("Mobility"), ESearchCase::IgnoreCase))

        {

            return true;

        }

        return false;

    }



    FString SanitizeSnapshotToken(FString Value)

    {

        Value.ReplaceInline(TEXT(" "), TEXT("_"));

        Value.ReplaceInline(TEXT(":"), TEXT("-"));

        Value.ReplaceInline(TEXT("/"), TEXT("_"));

        Value.ReplaceInline(TEXT("\\"), TEXT("_"));

        return Value;

    }



    const FSlateBrush* GetIconBrushForTrackedObject(const UObject* Object)

    {

        if (const AActor* Actor = Cast<AActor>(Object))

        {

            if (const FSlateBrush* ActorIcon = FClassIconFinder::FindIconForActor(Actor))

            {

                return ActorIcon;

            }

        }



        if (Object && Object->GetClass())

        {

            if (const FSlateBrush* ClassIcon = FSlateIconFinder::FindIconBrushForClass(Object->GetClass(), FName(TEXT("ClassIcon.ActorComponent"))))

            {

                return ClassIcon;

            }

        }



        return FAppStyle::GetBrush(TEXT("ClassIcon.ActorComponent"));

    }



    FLinearColor GetIconTintForTrackedObject(const UObject* Object)

    {

        if (!Object)

        {

            return FLinearColor(0.78f, 0.78f, 0.78f);

        }



        if (Cast<AActor>(Object))

        {

            return FLinearColor(0.86f, 0.86f, 0.86f);

        }



        const FString ClassName = Object->GetClass() ? Object->GetClass()->GetName() : FString();

        if (ClassName.Contains(TEXT("StaticMesh"), ESearchCase::IgnoreCase)

            || ClassName.Contains(TEXT("Mesh"), ESearchCase::IgnoreCase))

        {

            return FLinearColor(0.12f, 0.78f, 0.82f);

        }



        if (ClassName.Contains(TEXT("Audio"), ESearchCase::IgnoreCase)

            || ClassName.Contains(TEXT("Sound"), ESearchCase::IgnoreCase))

        {

            return FLinearColor(0.95f, 0.38f, 0.68f);

        }



        if (ClassName.Contains(TEXT("Light"), ESearchCase::IgnoreCase))

        {

            return FLinearColor(1.0f, 0.78f, 0.26f);

        }



        if (ClassName.Contains(TEXT("Text"), ESearchCase::IgnoreCase))

        {

            return FLinearColor(0.72f, 0.58f, 1.0f);

        }



        if (ClassName.Contains(TEXT("Collision"), ESearchCase::IgnoreCase)

            || ClassName.Contains(TEXT("Box"), ESearchCase::IgnoreCase)

            || ClassName.Contains(TEXT("Sphere"), ESearchCase::IgnoreCase)

            || ClassName.Contains(TEXT("Capsule"), ESearchCase::IgnoreCase))

        {

            return FLinearColor(0.52f, 0.86f, 0.34f);

        }



        if (ClassName.Contains(TEXT("Scene"), ESearchCase::IgnoreCase)

            || ClassName.Contains(TEXT("Transform"), ESearchCase::IgnoreCase))

        {

            return FLinearColor(0.58f, 0.70f, 0.92f);

        }



        if (ClassName.Contains(TEXT("Anim"), ESearchCase::IgnoreCase)

            || ClassName.Contains(TEXT("Timeline"), ESearchCase::IgnoreCase))

        {

            return FLinearColor(1.0f, 0.55f, 0.20f);

        }



        return FLinearColor(0.74f, 0.74f, 0.74f);

    }



    FString BuildReferenceSnapshotSignature(const TMap<TWeakObjectPtr<AActor>, TSet<FString>>& Snapshot)

    {

        TArray<FString> SignatureLines;

        SignatureLines.Reserve(Snapshot.Num());



        for (const TPair<TWeakObjectPtr<AActor>, TSet<FString>>& Pair : Snapshot)

        {

            AActor* Actor = Pair.Key.Get();

            if (!Actor)

            {

                continue;

            }



            TArray<FString> Paths = Pair.Value.Array();

            Paths.Sort();

            SignatureLines.Add(FString::Printf(TEXT("%s|%s|%s"),

                *GetActorLabelForTrace(Actor),

                *GetActorClassNameForTrace(Actor),

                *FString::Join(Paths, TEXT(","))));

        }



        SignatureLines.Sort();

        return FString::Join(SignatureLines, TEXT("\n"));

    }



    FString GetActorLabelForTrace(const AActor* Actor)

    {

        if (!Actor)

        {

            return TEXT("<Invalid Actor>");

        }



#if WITH_EDITOR

        return Actor->GetActorLabel();

#else

        return Actor->GetName();

#endif

    }



    FString GetActorClassNameForTrace(const AActor* Actor)

    {

        return Actor && Actor->GetClass() ? Actor->GetClass()->GetName() : TEXT("<Invalid Class>");

    }



    FString GetWorldTypeText(const UWorld* World)

    {

        if (!World)

        {

            return TEXT("<No World>");

        }



        switch (World->WorldType)

        {

        case EWorldType::PIE:

            return TEXT("PIE");

        case EWorldType::Editor:

            return TEXT("Editor");

        case EWorldType::Game:

            return TEXT("Game");

        case EWorldType::EditorPreview:

            return TEXT("EditorPreview");

        default:

            return TEXT("Other");

        }

    }



    bool IsHiddenInGameStateKey(const FString& StateKey)

    {

        return StateKey.Contains(TEXT("HiddenInGame"), ESearchCase::IgnoreCase);

    }



    const AActor* ResolveActorFromTraceObject(const UObject* Object)

    {

        if (!Object)

        {

            return nullptr;

        }



        if (const AActor* Actor = Cast<AActor>(Object))

        {

            return Actor;

        }



        if (const UActorComponent* Component = Cast<UActorComponent>(Object))

        {

            return Component->GetOwner();

        }



        return Object->GetTypedOuter<AActor>();

    }



    FString GetObjectNameForTrace(const UObject* Object)

    {

        return Object ? Object->GetName() : FString(TEXT("<unknown object>"));

    }



    FString GetTraceInstanceNameForObject(const UObject* Object)

    {

        if (const AActor* Actor = ResolveActorFromTraceObject(Object))

        {

            return GetActorLabelForTrace(Actor);

        }



        return GetObjectNameForTrace(Object);

    }



    FString GetTraceComponentNameForObject(const UObject* Object)

    {

        if (const UActorComponent* Component = Cast<UActorComponent>(Object))

        {

            return Component->GetName();

        }



        return FString();

    }



    FString GetTraceObjectDisplayName(const UObject* Object)

    {

        if (!Object)

        {

            return TEXT("<unknown object>");

        }



        if (const AActor* Actor = Cast<AActor>(Object))

        {

            return GetActorLabelForTrace(Actor);

        }



        if (const UActorComponent* Component = Cast<UActorComponent>(Object))

        {

            if (const AActor* Owner = Component->GetOwner())

            {

                return FString::Printf(TEXT("%s.%s"), *GetActorLabelForTrace(Owner), *Component->GetName());

            }

        }



        if (const AActor* OuterActor = Object->GetTypedOuter<AActor>())

        {

            return FString::Printf(TEXT("%s.%s"), *GetActorLabelForTrace(OuterActor), *Object->GetName());

        }



        return Object->GetName();

    }



    FString GetClassNameForTraceObject(const UObject* Object)

    {

        if (const AActor* Actor = ResolveActorFromTraceObject(Object))

        {

            return GetActorClassNameForTrace(Actor);

        }



        return Object && Object->GetClass() ? Object->GetClass()->GetName() : FString(TEXT("<unknown class>"));

    }



    FString NormalizeBlueprintFunctionName(const UFunction* Function)

    {

        if (!Function)

        {

            return FString();

        }



        FString FunctionName = Function->GetName();

        if (FunctionName == TEXT("ReceiveBeginPlay"))

        {

            return TEXT("BeginPlay");

        }



        if (FunctionName == TEXT("UserConstructionScript"))

        {

            return TEXT("ConstructionScript");

        }



        if (FunctionName.StartsWith(TEXT("ExecuteUbergraph_")))

        {

            return FunctionName;

        }



        return FunctionName;

    }



    bool IsVisibilityNativeCallName(const FString& FunctionName)

    {

        return FunctionName.Contains(TEXT("SetActorHiddenInGame"), ESearchCase::IgnoreCase)

            || FunctionName.Contains(TEXT("SetHiddenInGame"), ESearchCase::IgnoreCase)

            || FunctionName.Contains(TEXT("SetVisibility"), ESearchCase::IgnoreCase)

            || FunctionName.Contains(TEXT("SetVisibleFlag"), ESearchCase::IgnoreCase);

    }



#if DO_BLUEPRINT_GUARD

    FString BuildBlueprintExecutionSummaryFromContext(

        const FBlueprintContextTracker* ContextTracker,

        const UObject* ContextObject,

        const UFunction* ContextFunction,

        const UObject* ChangedObject,

        const FString& Reason)

    {

        const UObject* SourceObject = ContextObject;

        const UFunction* SourceFunction = ContextFunction;

        const UFunction* NativeFunction = nullptr;

        TArray<FString> StackTokens;



        if (ContextTracker)

        {

            const TArrayView<const FFrame* const> ScriptStack = ContextTracker->GetCurrentScriptStack();

            for (int32 Index = ScriptStack.Num() - 1; Index >= 0; --Index)

            {

                const FFrame* Frame = ScriptStack[Index];

                if (!Frame)

                {

                    continue;

                }



                if (!SourceObject && Frame->Object)

                {

                    SourceObject = Frame->Object;

                }



                if (!SourceFunction && Frame->Node)

                {

                    SourceFunction = Frame->Node;

                }



                if (Frame->CurrentNativeFunction)

                {

                    const FString CurrentNativeName = Frame->CurrentNativeFunction->GetName();

                    if (!NativeFunction || IsVisibilityNativeCallName(CurrentNativeName))

                    {

                        NativeFunction = Frame->CurrentNativeFunction;

                    }

                }



                const FString FrameObjectName = Frame->Object ? GetTraceObjectDisplayName(Frame->Object) : FString(TEXT("<frame object>"));

                const FString FrameFunctionName = NormalizeBlueprintFunctionName(Frame->Node);

                if (!FrameFunctionName.IsEmpty())

                {

                    StackTokens.Add(FString::Printf(TEXT("%s.%s"), *FrameObjectName, *FrameFunctionName));

                }

            }

        }



        if (!SourceObject && !SourceFunction && !NativeFunction)

        {

            return FString();

        }



        const AActor* SourceActor = ResolveActorFromTraceObject(SourceObject);

        const FString InstanceName = GetTraceInstanceNameForObject(SourceObject);

        const FString ObjectName = GetTraceObjectDisplayName(SourceObject);

        const FString ComponentName = GetTraceComponentNameForObject(SourceObject);

        const FString ActorName = SourceActor ? GetActorLabelForTrace(SourceActor) : GetObjectNameForTrace(SourceObject);

        const FString ClassName = GetClassNameForTraceObject(SourceObject);

        const FString FunctionName = NormalizeBlueprintFunctionName(SourceFunction);

        const FString NativeCallName = NativeFunction ? NativeFunction->GetName() : FString(TEXT("<unknown native call>"));

        const FString ChangedName = ChangedObject ? GetTraceObjectDisplayName(ChangedObject) : FString(TEXT("<unknown changed object>"));

        const FString ChangedInstanceName = ChangedObject ? GetTraceInstanceNameForObject(ChangedObject) : FString(TEXT("<unknown instance>"));

        const FString ChangedComponentName = ChangedObject ? GetTraceComponentNameForObject(ChangedObject) : FString();

        const FString ConfidenceText = ContextTracker && ContextTracker->GetCurrentScriptStack().Num() > 0

            ? FString(TEXT("ActiveScriptStack"))

            : FString(TEXT("ScriptEntry"));



        FString Summary = FString::Printf(TEXT("Execution=RuntimeBlueprint | Instance=%s | Actor=%s | Object=%s | Component=%s | Class=%s | Function=%s | NativeCall=%s | ChangedInstance=%s | ChangedObject=%s | ChangedComponent=%s | Reason=%s | Confidence=%s"),

            *InstanceName,

            *ActorName,

            *ObjectName,

            ComponentName.IsEmpty() ? TEXT("-") : *ComponentName,

            *ClassName,

            FunctionName.IsEmpty() ? TEXT("<unknown function>") : *FunctionName,

            *NativeCallName,

            *ChangedInstanceName,

            *ChangedName,

            ChangedComponentName.IsEmpty() ? TEXT("-") : *ChangedComponentName,

            Reason.IsEmpty() ? TEXT("<unknown reason>") : *Reason,

            *ConfidenceText);



        if (StackTokens.Num() > 0)

        {

            constexpr int32 MaxStackTokens = 4;

            if (StackTokens.Num() > MaxStackTokens)

            {

                StackTokens.SetNum(MaxStackTokens);

                StackTokens.Add(TEXT("..."));

            }



            Summary += FString::Printf(TEXT(" | ScriptStack=%s"), *FString::Join(StackTokens, TEXT(" > ")));

        }



        return Summary;

    }

#endif



    bool IsStateActionMatch(const FString& StateKey, const FString& ActionSummary)

    {

        if (ActionSummary.IsEmpty())

        {

            return false;

        }



        if (StateKey.Contains(TEXT("Hidden")) || StateKey.Contains(TEXT("Visible")))

        {

            return ActionSummary.Contains(TEXT("Hidden"))

                || ActionSummary.Contains(TEXT("Hide"))

                || ActionSummary.Contains(TEXT("Visibility"))

                || ActionSummary.Contains(TEXT("Visible"));

        }



        if (StateKey.Contains(TEXT("Collision")))

        {

            return ActionSummary.Contains(TEXT("Collision"));

        }





        return false;

    }



    bool IsFunctionRelevantToState(const FString& StateKey, const FName FunctionName)

    {

        return IsStateActionMatch(StateKey, FunctionName.ToString());

    }



    FString GetRootPropertyName(const FString& PropertyPath)

    {

        int32 CutIndex = INDEX_NONE;

        const TCHAR Delimiters[] = { TEXT('.'), TEXT('['), TEXT('{') };

        for (const TCHAR Delimiter : Delimiters)

        {

            int32 FoundIndex = INDEX_NONE;

            if (PropertyPath.FindChar(Delimiter, FoundIndex))

            {

                CutIndex = CutIndex == INDEX_NONE ? FoundIndex : FMath::Min(CutIndex, FoundIndex);

            }

        }



        return CutIndex == INDEX_NONE ? PropertyPath : PropertyPath.Left(CutIndex);

    }



    bool IsReferenceActionCall(const FName FunctionName)

    {

        const FString Name = FunctionName.ToString();

        return Name.Contains(TEXT("Hidden"))

            || Name.Contains(TEXT("Hide"))

            || Name.Contains(TEXT("Visibility"))

            || Name.Contains(TEXT("Visible"))

            || Name.Contains(TEXT("Destroy"))

            || Name.Contains(TEXT("Attach"))

            || Name.Contains(TEXT("Detach"))

            || Name.Contains(TEXT("Activate"))

            || Name.Contains(TEXT("Deactivate"))

            || Name.Contains(TEXT("Collision"));

    }



    FString GetActionNodeName(const UK2Node_CallFunction* CallNode)

    {

        if (!CallNode)

        {

            return FString();

        }



        const FName FunctionName = CallNode->GetFunctionName();

        if (!FunctionName.IsNone())

        {

            return FunctionName.ToString();

        }



        return CallNode->GetNodeTitle(ENodeTitleType::ListView).ToString();

    }



    bool IsCallTargetingSelfOrImplicitSelf(const UK2Node_CallFunction* CallNode)

    {

        if (!CallNode)

        {

            return false;

        }



        const UEdGraphPin* SelfPin = CallNode->FindPin(UEdGraphSchema_K2::PN_Self);

        if (!SelfPin || SelfPin->bHidden || SelfPin->LinkedTo.Num() == 0)

        {

            return true;

        }



        for (const UEdGraphPin* LinkedPin : SelfPin->LinkedTo)

        {

            const UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;

            if (LinkedNode && LinkedNode->GetClass()->GetName().Contains(TEXT("K2Node_Self")))

            {

                return true;

            }

        }



        return false;

    }



    bool IsClassCompatibleWithTrace(const UClass* CandidateClass, const UClass* ExpectedClass)

    {

        if (!ExpectedClass || !CandidateClass)

        {

            return true;

        }



        return CandidateClass == ExpectedClass

            || CandidateClass->IsChildOf(ExpectedClass)

            || CandidateClass->GetFName() == ExpectedClass->GetFName()

            || CandidateClass->ClassGeneratedBy == ExpectedClass->ClassGeneratedBy;

    }



    bool DoesActorMatchTraceTarget(

        const AActor* Candidate,

        const AActor* TargetActor,

        const FName InitialActorName,

        const FString& InitialActorLabel,

        const FGuid& InitialActorGuid,

        const FGuid& InitialActorInstanceGuid,

        const UClass* InitialActorClass)

    {

        if (!Candidate)

        {

            return false;

        }



        if (Candidate == TargetActor)

        {

            return true;

        }



        if ((InitialActorGuid.IsValid() && Candidate->GetActorGuid() == InitialActorGuid)

            || (InitialActorInstanceGuid.IsValid() && Candidate->GetActorInstanceGuid() == InitialActorInstanceGuid))

        {

            return true;

        }



        if (TargetActor)

        {

            const bool bTargetGuidMatch = (TargetActor->GetActorGuid().IsValid() && Candidate->GetActorGuid() == TargetActor->GetActorGuid())

                || (TargetActor->GetActorInstanceGuid().IsValid() && Candidate->GetActorInstanceGuid() == TargetActor->GetActorInstanceGuid());

            if (bTargetGuidMatch)

            {

                return true;

            }

        }



        if (!IsClassCompatibleWithTrace(Candidate->GetClass(), InitialActorClass))

        {

            return false;

        }



        return Candidate->GetFName() == InitialActorName

            || GetActorLabelForTrace(Candidate) == InitialActorLabel

            || (TargetActor && GetActorLabelForTrace(Candidate) == GetActorLabelForTrace(TargetActor));

    }



    bool DoesObjectReferenceMatchTraceTarget(

        UObject* ObjectRef,

        const AActor* TargetActor,

        const FName InitialActorName,

        const FString& InitialActorLabel,

        const FGuid& InitialActorGuid,

        const FGuid& InitialActorInstanceGuid,

        const UClass* InitialActorClass)

    {

        if (!ObjectRef)

        {

            return false;

        }



        if (AActor* ReferencedActor = Cast<AActor>(ObjectRef))

        {

            return DoesActorMatchTraceTarget(ReferencedActor, TargetActor, InitialActorName, InitialActorLabel, InitialActorGuid, InitialActorInstanceGuid, InitialActorClass);

        }



        return ObjectRef == TargetActor;

    }



    bool CanPropertyContainTraceReference(const FProperty* Property, int32 Depth = 0)

    {

        if (!Property || Depth > MaxReferenceStructDepth)

        {

            return false;

        }



        if (CastField<FObjectPropertyBase>(Property))

        {

            return true;

        }



        if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))

        {

            if (!StructProperty->Struct)

            {

                return false;

            }



            for (TFieldIterator<FProperty> It(StructProperty->Struct); It; ++It)

            {

                if (CanPropertyContainTraceReference(*It, Depth + 1))

                {

                    return true;

                }

            }



            return false;

        }



        if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))

        {

            return CanPropertyContainTraceReference(ArrayProperty->Inner, Depth + 1);

        }



        if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))

        {

            return CanPropertyContainTraceReference(SetProperty->ElementProp, Depth + 1);

        }



        if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))

        {

            return CanPropertyContainTraceReference(MapProperty->KeyProp, Depth + 1)

                || CanPropertyContainTraceReference(MapProperty->ValueProp, Depth + 1);

        }



        return false;

    }



    bool DoesNodeReferenceTraceTarget(

        const UEdGraphNode* Node,

        const AActor* TargetActor,

        const FName InitialActorName,

        const FString& InitialActorLabel,

        const FGuid& InitialActorGuid,

        const FGuid& InitialActorInstanceGuid,

        const UClass* InitialActorClass)

    {

        if (!Node)

        {

            return false;

        }



        if (const UK2Node* K2Node = Cast<UK2Node>(Node))

        {

            if (DoesActorMatchTraceTarget(K2Node->GetReferencedLevelActor(), TargetActor, InitialActorName, InitialActorLabel, InitialActorGuid, InitialActorInstanceGuid, InitialActorClass))

            {

                return true;

            }

        }



        if (const UK2Node_Literal* LiteralNode = Cast<UK2Node_Literal>(Node))

        {

            if (DoesObjectReferenceMatchTraceTarget(LiteralNode->GetObjectRef(), TargetActor, InitialActorName, InitialActorLabel, InitialActorGuid, InitialActorInstanceGuid, InitialActorClass))

            {

                return true;

            }

        }



        return false;

    }



    bool DoesPinNetworkReferenceTraceTarget(

        const UEdGraphPin* Pin,

        const AActor* TargetActor,

        const FName InitialActorName,

        const FString& InitialActorLabel,

        const FGuid& InitialActorGuid,

        const FGuid& InitialActorInstanceGuid,

        const UClass* InitialActorClass,

        TSet<const UEdGraphPin*>& VisitedPins,

        int32 Depth)

    {

        if (!Pin || Depth > 6 || VisitedPins.Contains(Pin))

        {

            return false;

        }



        VisitedPins.Add(Pin);



        if (DoesObjectReferenceMatchTraceTarget(Pin->DefaultObject, TargetActor, InitialActorName, InitialActorLabel, InitialActorGuid, InitialActorInstanceGuid, InitialActorClass)

            || DoesNodeReferenceTraceTarget(Pin->GetOwningNode(), TargetActor, InitialActorName, InitialActorLabel, InitialActorGuid, InitialActorInstanceGuid, InitialActorClass))

        {

            return true;

        }



        for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)

        {

            const UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;

            if (DoesNodeReferenceTraceTarget(LinkedNode, TargetActor, InitialActorName, InitialActorLabel, InitialActorGuid, InitialActorInstanceGuid, InitialActorClass))

            {

                return true;

            }



            if (LinkedPin && DoesPinNetworkReferenceTraceTarget(LinkedPin, TargetActor, InitialActorName, InitialActorLabel, InitialActorGuid, InitialActorInstanceGuid, InitialActorClass, VisitedPins, Depth + 1))

            {

                return true;

            }



            if (LinkedNode)

            {

                for (const UEdGraphPin* NeighborPin : LinkedNode->Pins)

                {

                    if (NeighborPin && NeighborPin != LinkedPin

                        && DoesPinNetworkReferenceTraceTarget(NeighborPin, TargetActor, InitialActorName, InitialActorLabel, InitialActorGuid, InitialActorInstanceGuid, InitialActorClass, VisitedPins, Depth + 1))

                    {

                        return true;

                    }

                }

            }

        }



        return false;

    }



    bool DoesPinReferenceTraceTarget(

        const UEdGraphPin* Pin,

        const AActor* TargetActor,

        const FName InitialActorName,

        const FString& InitialActorLabel,

        const FGuid& InitialActorGuid,

        const FGuid& InitialActorInstanceGuid,

        const UClass* InitialActorClass)

    {

        TSet<const UEdGraphPin*> VisitedPins;

        return DoesPinNetworkReferenceTraceTarget(Pin, TargetActor, InitialActorName, InitialActorLabel, InitialActorGuid, InitialActorInstanceGuid, InitialActorClass, VisitedPins, 0);

    }



    bool DoesPinNetworkUseBlueprintVariable(

        const UEdGraphPin* Pin,

        const FName RootPropertyName,

        TSet<const UEdGraphPin*>& VisitedPins,

        TSet<const UEdGraphNode*>& VisitedNodes,

        int32 Depth);



    bool DoesNodeNetworkUseBlueprintVariable(

        const UEdGraphNode* Node,

        const FName RootPropertyName,

        TSet<const UEdGraphPin*>& VisitedPins,

        TSet<const UEdGraphNode*>& VisitedNodes,

        int32 Depth)

    {

        if (!Node || Depth > 12 || VisitedNodes.Contains(Node))

        {

            return false;

        }



        VisitedNodes.Add(Node);



        if (const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node))

        {

            if (VariableNode->GetVarName() == RootPropertyName)

            {

                return true;

            }

        }



        for (const UEdGraphPin* NodePin : Node->Pins)

        {

            if (DoesPinNetworkUseBlueprintVariable(NodePin, RootPropertyName, VisitedPins, VisitedNodes, Depth + 1))

            {

                return true;

            }

        }



        return false;

    }



    bool DoesPinNetworkUseBlueprintVariable(

        const UEdGraphPin* Pin,

        const FName RootPropertyName,

        TSet<const UEdGraphPin*>& VisitedPins,

        TSet<const UEdGraphNode*>& VisitedNodes,

        int32 Depth)

    {

        if (!Pin || Depth > 12 || VisitedPins.Contains(Pin))

        {

            return false;

        }



        VisitedPins.Add(Pin);



        if (DoesNodeNetworkUseBlueprintVariable(Pin->GetOwningNode(), RootPropertyName, VisitedPins, VisitedNodes, Depth + 1))

        {

            return true;

        }



        for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)

        {

            if (!LinkedPin)

            {

                continue;

            }



            if (DoesNodeNetworkUseBlueprintVariable(LinkedPin->GetOwningNode(), RootPropertyName, VisitedPins, VisitedNodes, Depth + 1))

            {

                return true;

            }

        }



        return false;

    }



    bool IsFunctionLikelyToAccessActorReference(const FName FunctionName)

    {

        const FString Name = FunctionName.ToString();

        return IsReferenceActionCall(FunctionName)

            || Name.Contains(TEXT("Transform"))

            || Name.Contains(TEXT("Location"))

            || Name.Contains(TEXT("Rotation"))

            || Name.Contains(TEXT("Scale"))

            || Name.Contains(TEXT("Tag"))

            || Name.Contains(TEXT("Owner"))

            || Name.Contains(TEXT("Possess"))

            || Name.Contains(TEXT("Input"));

    }



    void CollectCallActionsFromNode(UEdGraphNode* Node, UEdGraphNode* SourceNode, UEdGraph* Graph, int32 Depth, TSet<UEdGraphNode*>& VisitedNodes, TSet<FString>& OutActions)

    {

        if (!Node || !Graph || Depth > 12 || VisitedNodes.Contains(Node))

        {

            return;

        }



        VisitedNodes.Add(Node);



        if (Node != SourceNode)

        {

            if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))

            {

                const FString ActionName = GetActionNodeName(CallNode);

                if (!ActionName.IsEmpty())

                {

                    const FString Prefix = IsReferenceActionCall(CallNode->GetFunctionName()) ? TEXT("Action") : TEXT("Call");

                    OutActions.Add(FString::Printf(TEXT("Function=%s | %s=%s"), *Graph->GetName(), *Prefix, *ActionName));

                }

            }

        }



        for (UEdGraphPin* Pin : Node->Pins)

        {

            if (!Pin)

            {

                continue;

            }



            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)

            {

                if (!LinkedPin)

                {

                    continue;

                }



                CollectCallActionsFromNode(LinkedPin->GetOwningNode(), SourceNode, Graph, Depth + 1, VisitedNodes, OutActions);

            }

        }

    }



    void FindBlueprintActionsForProperty(AActor* Referencer, const FString& PropertyPath, TSet<FString>& OutActions)

    {

        if (!Referencer || !Referencer->GetClass())

        {

            return;

        }



        UBlueprint* Blueprint = Cast<UBlueprint>(Referencer->GetClass()->ClassGeneratedBy);

        if (!Blueprint)

        {

            return;

        }



        const FName RootPropertyName(*GetRootPropertyName(PropertyPath));

        if (RootPropertyName.IsNone())

        {

            return;

        }



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

                UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node);

                if (!VariableNode || VariableNode->GetVarName() != RootPropertyName)

                {

                    continue;

                }



                TSet<UEdGraphNode*> VisitedNodes;

                CollectCallActionsFromNode(VariableNode, VariableNode, Graph, 0, VisitedNodes, OutActions);

            }

        }

    }



    void FindBlueprintVisibilityActionsForProperty(AActor* Referencer, const FString& PropertyPath, TSet<FString>& OutActions)

    {

        if (!Referencer || !Referencer->GetClass())

        {

            return;

        }



        UBlueprint* Blueprint = Cast<UBlueprint>(Referencer->GetClass()->ClassGeneratedBy);

        if (!Blueprint)

        {

            return;

        }



        const FName RootPropertyName(*GetRootPropertyName(PropertyPath));

        if (RootPropertyName.IsNone())

        {

            return;

        }



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

                UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);

                if (!CallNode || !IsFunctionRelevantToState(TEXT("Visible"), CallNode->GetFunctionName()))

                {

                    continue;

                }



                for (const UEdGraphPin* Pin : CallNode->Pins)

                {

                    if (!Pin)

                    {

                        continue;

                    }



                    TSet<const UEdGraphPin*> VisitedPins;

                    TSet<const UEdGraphNode*> VisitedNodes;

                    if (DoesPinNetworkUseBlueprintVariable(Pin, RootPropertyName, VisitedPins, VisitedNodes, 0))

                    {

                        OutActions.Add(FString::Printf(TEXT("Function=%s | Action=%s | SourceProperty=%s | MatchPin=%s"),

                            *Graph->GetName(),

                            *GetActionNodeName(CallNode),

                            *RootPropertyName.ToString(),

                            *Pin->PinName.ToString()));

                        break;

                    }

                }

            }

        }

    }



    void FindBlueprintSelfActionsForState(AActor* TargetActor, const FString& StateKey, TSet<FString>& OutActions)

    {

        if (!TargetActor || !TargetActor->GetClass())

        {

            return;

        }



        UBlueprint* Blueprint = Cast<UBlueprint>(TargetActor->GetClass()->ClassGeneratedBy);

        if (!Blueprint)

        {

            return;

        }



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

                UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);

                if (!CallNode || !IsFunctionRelevantToState(StateKey, CallNode->GetFunctionName()))

                {

                    continue;

                }



                if (IsCallTargetingSelfOrImplicitSelf(CallNode))

                {

                    OutActions.Add(FString::Printf(TEXT("Actor=%s | Class=%s | Ref=Self | Function=%s | SelfAction=%s"),

                        *GetActorLabelForTrace(TargetActor),

                        *GetActorClassNameForTrace(TargetActor),

                        *Graph->GetName(),

                        *GetActionNodeName(CallNode)));

                }

            }

        }

    }

}



SInstanceReferenceTracker::~SInstanceReferenceTracker()

{

    FInstanceTraceGlobalManager::Get().RemoveSubscriber(this);

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildMetricCard(const FString& Label, TAttribute<FText> ValueText) const

{

    auto IsZeroValue = [ValueText]()

    {

        return ValueText.Get().ToString() == TEXT("0");

    };



    return SNew(SBorder)

        .Padding(FMargin(10.0f, 7.0f))

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

        .BorderBackgroundColor_Lambda([IsZeroValue]()

        {

            return IsZeroValue()

                ? FLinearColor(1.0f, 1.0f, 1.0f, 0.025f)

                : FLinearColor(0.13f, 0.59f, 0.95f, 0.10f);

        })

        [

            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()

                [

                    SNew(STextBlock)

                        .Text(ValueText)

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 19))

                        .ColorAndOpacity_Lambda([IsZeroValue]()

                        {

                            return IsZeroValue()

                                ? FSlateColor(FLinearColor(0.42f, 0.42f, 0.42f))

                                : FSlateColor(FLinearColor(0.90f, 0.95f, 1.0f));

                        })

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)

                [

                    SNew(STextBlock)

                        .Text(FText::FromString(Label))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                        .ColorAndOpacity_Lambda([IsZeroValue]()

                        {

                            return IsZeroValue()

                                ? FSlateColor(FLinearColor(0.36f, 0.36f, 0.36f))

                                : FSlateColor(FLinearColor(0.68f, 0.76f, 0.86f));

                        })

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildFilterChip(const FString& Label, TAttribute<ECheckBoxState> IsChecked, FOnCheckStateChanged OnChanged) const

{

    return SNew(SCheckBox)

        .Style(FAppStyle::Get(), TEXT("ToggleButtonCheckbox"))

        .Padding(0.0f)

        .IsChecked(IsChecked)

        .OnCheckStateChanged(OnChanged)

        [

            SNew(SBorder)

                .Padding(FMargin(9.0f, 4.0f))

                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                .BorderBackgroundColor_Lambda([IsChecked]()

                {

                    return IsChecked.Get() == ECheckBoxState::Checked

                        ? FLinearColor(0.13f, 0.59f, 0.95f, 0.95f)

                        : FLinearColor(0.08f, 0.09f, 0.11f, 0.95f);

                })

                [

                    SNew(STextBlock)

                        .Text(FText::FromString(Label))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                        .ColorAndOpacity_Lambda([IsChecked]()

                        {

                            return IsChecked.Get() == ECheckBoxState::Checked

                                ? FSlateColor(FLinearColor::White)

                                : FSlateColor(FLinearColor(0.56f, 0.66f, 0.76f));

                        })

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildToolbarButton(const FName IconName, const FString& Label, FOnClicked OnClicked) const

{

    return SNew(SButton)

        .ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))

        .ContentPadding(FMargin(8.0f, 5.0f))

        .OnClicked(OnClicked)

        [

            SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                [

                    SNew(SImage)

                        .Image(FAppStyle::GetBrush(IconName))

                        .ColorAndOpacity(FSlateColor::UseForeground())

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.0f, 0.0f, 0.0f, 0.0f)

                [

                    SNew(STextBlock)

                        .Text(FText::FromString(Label))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildMoreFilterMenu()

{

    auto MakeBoolAttribute = [](const bool* Flag)

    {

        return TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateLambda([Flag]()

        {

            return Flag && *Flag ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

        }));

    };



    auto MakeFilterHandler = [this](bool* Flag)

    {

        return FOnCheckStateChanged::CreateLambda([this, Flag](ECheckBoxState NewState)

        {

            if (Flag)

            {

                *Flag = NewState == ECheckBoxState::Checked;

            }

            RefreshChangeEventList();

            RefreshSummary();

        });

    };



    return SNew(SBorder)

        .Padding(6.0f)

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

        [

            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)

                [

                    BuildFilterChip(TMLoc::String(TEXT("Lifecycle"), TEXT("")), MakeBoolAttribute(&bFilterLifecycle), MakeFilterHandler(&bFilterLifecycle))

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)

                [

                    BuildFilterChip(TMLoc::String(TEXT("Material"), TEXT("")), MakeBoolAttribute(&bFilterMaterial), MakeFilterHandler(&bFilterMaterial))

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)

                [

                    BuildFilterChip(TMLoc::String(TEXT("Pinned Only"), TEXT("")), MakeBoolAttribute(&bPinnedOnly), MakeFilterHandler(&bPinnedOnly))

                ]

                + SVerticalBox::Slot().AutoHeight()

                [

                    BuildFilterChip(TMLoc::String(TEXT("Show Ignored"), TEXT("")), MakeBoolAttribute(&bShowIgnored), MakeFilterHandler(&bShowIgnored))

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildWatchRuleMenu()

{

    auto MakeRuleButton = [this](const FString& Label, EWatchRuleMode Mode)

    {

        return SNew(SButton)

            .ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))

            .ContentPadding(FMargin(8.0f, 5.0f))

            .OnClicked_Lambda([this, Mode]()

            {

                SetWatchRuleMode(Mode);

                return FReply::Handled();

            })

            [

                SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)

                    [

                        SNew(STextBlock)

                            .Text(FText::FromString(WatchRuleMode == Mode ? TEXT("*") : TEXT(" ")))

                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                            .ColorAndOpacity(FLinearColor(0.13f, 0.59f, 0.95f))

                    ]

                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                    [

                        SNew(STextBlock)

                            .Text(FText::FromString(Label))

                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                    ]

            ];

    };



    return SNew(SBorder)

        .Padding(6.0f)

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

        [

            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)

                [

                    MakeRuleButton(TMLoc::String(TEXT("All events"), TEXT("")), EWatchRuleMode::All)

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)

                [

                    MakeRuleButton(TMLoc::String(TEXT("Visibility"), TEXT("")), EWatchRuleMode::Visibility)

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)

                [

                    MakeRuleButton(TMLoc::String(TEXT("Reference"), TEXT("")), EWatchRuleMode::Reference)

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)

                [

                    MakeRuleButton(TMLoc::String(TEXT("Lifecycle"), TEXT("")), EWatchRuleMode::Lifecycle)

                ]

                + SVerticalBox::Slot().AutoHeight()

                [

                    MakeRuleButton(TMLoc::String(TEXT("Transform"), TEXT("")), EWatchRuleMode::Transform)

                ]

        ];

}

void SInstanceReferenceTracker::SetWatchRuleMode(EWatchRuleMode InMode)

{

    WatchRuleMode = InMode;

    if (WatchRuleMode == EWatchRuleMode::Visibility)

    {

        bFilterVisibility = true;

    }

    else if (WatchRuleMode == EWatchRuleMode::Reference)

    {

        bFilterReference = true;

    }

    else if (WatchRuleMode == EWatchRuleMode::Lifecycle)

    {

        bFilterLifecycle = true;

    }

    else if (WatchRuleMode == EWatchRuleMode::Transform)

    {

        bFilterTransform = true;

    }



    RefreshChangeEventList();

    RefreshSummary();

}



FString SInstanceReferenceTracker::GetWatchRuleLabel() const

{

    switch (WatchRuleMode)

    {

    case EWatchRuleMode::Visibility:

        return TMLoc::String(TEXT("Visibility"), TEXT(""));

    case EWatchRuleMode::Reference:

        return TMLoc::String(TEXT("Reference"), TEXT(""));

    case EWatchRuleMode::Lifecycle:

        return TMLoc::String(TEXT("Lifecycle"), TEXT(""));

    case EWatchRuleMode::Transform:

        return TMLoc::String(TEXT("Transform"), TEXT(""));

    case EWatchRuleMode::All:

    default:

        return TMLoc::String(TEXT("All"), TEXT(""));

    }

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildHeaderBar()

{

    auto MakeMetricText = [](TFunction<FText()>&& Getter)

    {

        return TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(MoveTemp(Getter)));

    };



    auto MakeBoolAttribute = [](const bool* Flag)

    {

        return TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateLambda([Flag]()

        {

            return Flag && *Flag ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

        }));

    };



    auto MakeFilterHandler = [this](bool* Flag)

    {

        return FOnCheckStateChanged::CreateLambda([this, Flag](ECheckBoxState NewState)

        {

            if (Flag)

            {

                *Flag = NewState == ECheckBoxState::Checked;

            }

            RefreshChangeEventList();

            RefreshSummary();

        });

    };



    return SNew(SBorder)

        .Padding(10.0f)

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

        [

            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()

                [

                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                        [

                            SNew(SVerticalBox)

                                + SVerticalBox::Slot().AutoHeight()

                                [

                                    SNew(STextBlock)

                                        .Text_Lambda([this]()

                                        {

                                            const FString TargetName = TargetActor.IsValid() ? GetActorLabelSafe(TargetActor.Get()) : InitialActorLabel;

                                            return FText::FromString(TargetName.IsEmpty() ? TMLoc::String(TEXT("<No Target>"), TEXT("")) : TargetName);

                                        })

                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13))

                                        .ColorAndOpacity(FSlateColor::UseForeground())

                                ]

                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)

                                [

                                    SAssignNew(SummaryTextBlock, STextBlock)

                                        .Text(TMLoc::Text(TEXT("Debug Session"), TEXT("")))

                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                                        .ColorAndOpacity(FLinearColor(0.62f, 0.62f, 0.62f))

                                ]

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)

                        [

                            SNew(SHorizontalBox)

                                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

                                [

                                    BuildToolbarButton(FName(TEXT("Icons.Save")), TMLoc::String(TEXT("Save Snapshot"), TEXT("")), FOnClicked::CreateLambda([this]()

                                    {

                                        SaveTraceSnapshot(TEXT("Manual"));

                                        return FReply::Handled();

                                    }))

                                ]

                                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

                                [

                                    BuildToolbarButton(FName(TEXT("Icons.Refresh")), TMLoc::String(TEXT("Reset"), TEXT("")), FOnClicked::CreateLambda([this]()

                                    {

                                        ResetSessionEvents();

                                        return FReply::Handled();

                                    }))

                                ]

                                + SHorizontalBox::Slot().AutoWidth()

                                [

                                    BuildToolbarButton(FName(TEXT("Icons.CaptureFrame")), TMLoc::String(TEXT("Auto Snapshot"), TEXT("")), FOnClicked::CreateLambda([this]()

                                    {

                                        bAutoSaveSnapshots = !bAutoSaveSnapshots;

                                        RefreshSummary();

                                        return FReply::Handled();

                                    }))

                                ]

                        ]

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)

                [

                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            BuildMetricCard(TMLoc::String(TEXT("Events"), TEXT("")), MakeMetricText([this]()
                            {
                                int32 VisibleEventCount = 0;
                                for (const TSharedPtr<FTrackedStateChangeEvent>& Event : RecentStateChangeEvents)
                                {
                                    if (IsStateChangeEventVisible(Event))
                                    {
                                        ++VisibleEventCount;
                                    }
                                }
                                return FText::AsNumber(VisibleEventCount);
                            }))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            BuildMetricCard(TMLoc::String(TEXT("Referencers"), TEXT("")), MakeMetricText([this]() { return FText::AsNumber(PreviousSnapshot.Num()); }))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            BuildMetricCard(TMLoc::String(TEXT("Pinned"), TEXT("")), MakeMetricText([this]() { return FText::AsNumber(PinnedNodeKeys.Num()); }))

                        ]

                        + SHorizontalBox::Slot().AutoWidth()

                        [

                            BuildMetricCard(TMLoc::String(TEXT("Ignored"), TEXT("")), MakeMetricText([this]() { return FText::AsNumber(IgnoredStateKeys.Num()); }))

                        ]

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)

                [

                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            BuildFilterChip(TMLoc::String(TEXT("Visibility"), TEXT("")), MakeBoolAttribute(&bFilterVisibility), MakeFilterHandler(&bFilterVisibility))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            BuildFilterChip(TMLoc::String(TEXT("Reference"), TEXT("")), MakeBoolAttribute(&bFilterReference), MakeFilterHandler(&bFilterReference))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            BuildFilterChip(TMLoc::String(TEXT("Transform"), TEXT("")), MakeBoolAttribute(&bFilterTransform), MakeFilterHandler(&bFilterTransform))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            BuildFilterChip(TMLoc::String(TEXT("Collision"), TEXT("")), MakeBoolAttribute(&bFilterCollision), MakeFilterHandler(&bFilterCollision))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            BuildFilterChip(TMLoc::String(TEXT("Other"), TEXT("")), MakeBoolAttribute(&bFilterOther), MakeFilterHandler(&bFilterOther))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)

                        [

                            SNew(SComboButton)

                                .ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))

                                .ContentPadding(FMargin(8.0f, 3.0f))

                                .ButtonContent()

                                [

                                    SNew(STextBlock)

                                        .Text(TMLoc::Text(TEXT("..."), TEXT("...")))

                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                                ]

                                .MenuContent()

                                [

                                    BuildMoreFilterMenu()

                                ]

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)

                        [

                            SNew(SComboButton)

                                .ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))

                                .ContentPadding(FMargin(8.0f, 3.0f))

                                .ButtonContent()

                                [

                                    SNew(STextBlock)

                                        .Text_Lambda([this]()

                                        {

                                            return FText::FromString(FString::Printf(TEXT("%s %s"), *TMLoc::String(TEXT("Watch:"), TEXT("")), *GetWatchRuleLabel()));

                                        })

                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                                ]

                                .MenuContent()

                                [

                                    BuildWatchRuleMenu()

                                ]

                        ]

                        + SHorizontalBox::Slot().FillWidth(1.0f)

                        [

                            SNew(SBox)

                                .MaxDesiredWidth(320.0f)

                                [

                                    SNew(SSearchBox)

                                        .HintText(TMLoc::Text(TEXT("Search events"), TEXT("")))

                                        .OnTextChanged_Lambda([this](const FText& InText)

                                        {

                                            EventSearchText = InText.ToString();

                                            RefreshChangeEventList();

                                            RefreshSummary();

                                        })

                                ]

                        ]

                ]

        ];

}



void SInstanceReferenceTracker::LoadUserLayoutSettings()

{

    if (!GConfig)

    {

        return;

    }



    float SavedHierarchyRatio = HierarchyPanelSplitRatio;

    float SavedRawLogHeight = RawLogDrawerHeight;

    GConfig->GetFloat(InstanceTraceLayoutConfigSection, TEXT("HierarchySplitRatio"), SavedHierarchyRatio, GEditorPerProjectIni);

    GConfig->GetFloat(InstanceTraceLayoutConfigSection, TEXT("RawLogDrawerHeight"), SavedRawLogHeight, GEditorPerProjectIni);



    HierarchyPanelSplitRatio = FMath::Clamp(SavedHierarchyRatio, MinHierarchySplitRatio, MaxHierarchySplitRatio);

    RawLogDrawerHeight = FMath::Max(MinRawLogDrawerHeight, SavedRawLogHeight);

}

void SInstanceReferenceTracker::SaveUserLayoutSettings() const

{

    if (!GConfig)

    {

        return;

    }



    GConfig->SetFloat(InstanceTraceLayoutConfigSection, TEXT("HierarchySplitRatio"), HierarchyPanelSplitRatio, GEditorPerProjectIni);

    GConfig->SetFloat(InstanceTraceLayoutConfigSection, TEXT("RawLogDrawerHeight"), RawLogDrawerHeight, GEditorPerProjectIni);

    GConfig->Flush(false, GEditorPerProjectIni);

}



void SInstanceReferenceTracker::HandleHierarchySlotResized(float NewSizeCoefficient)

{

    HierarchyPanelSplitRatio = FMath::Clamp(NewSizeCoefficient, MinHierarchySplitRatio, MaxHierarchySplitRatio);

}



void SInstanceReferenceTracker::HandleEventsSlotResized(float NewSizeCoefficient)

{

    HierarchyPanelSplitRatio = FMath::Clamp(1.0f - NewSizeCoefficient, MinHierarchySplitRatio, MaxHierarchySplitRatio);

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildMainPanel()

{

    return SNew(SSplitter)

        .Orientation(Orient_Horizontal)

        .PhysicalSplitterHandleSize(4.0f)

        .HitDetectionSplitterHandleSize(8.0f)

        .OnSplitterFinishedResizing(FSimpleDelegate::CreateSP(this, &SInstanceReferenceTracker::SaveUserLayoutSettings))

        + SSplitter::Slot()

        .Value(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateLambda([this]()

        {

            return HierarchyPanelSplitRatio;

        })))

        .OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SInstanceReferenceTracker::HandleHierarchySlotResized))

        .Resizable(true)

        [

            SNew(SBorder)

                .Padding(6.0f)

                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

                .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.12f))

                [

                    SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)

                        [

                            SNew(STextBlock)

                                .Text(TMLoc::Text(TEXT("Hierarchy"), TEXT("")))

                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                                .ColorAndOpacity(FLinearColor(0.76f, 0.76f, 0.76f))

                        ]

                        + SVerticalBox::Slot().FillHeight(1.0f)

                        [

                            SAssignNew(ComponentTreeView, STreeView<TSharedPtr<FTrackedComponentTreeNode>>)

                                .TreeItemsSource(&ComponentTreeRoots)

                                .OnGenerateRow(this, &SInstanceReferenceTracker::GenerateComponentTreeRow)

                                .OnGetChildren(this, &SInstanceReferenceTracker::GetComponentTreeChildren)

                                .OnSelectionChanged(this, &SInstanceReferenceTracker::HandleComponentTreeSelectionChanged)

                        ]

                ]

        ]

        + SSplitter::Slot()

        .Value(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateLambda([this]()

        {

            return 1.0f - HierarchyPanelSplitRatio;

        })))

        .OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SInstanceReferenceTracker::HandleEventsSlotResized))

        .Resizable(true)

        [

            SNew(SBorder)

                .Padding(1.0f)

                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                .BorderBackgroundColor_Lambda([this]()

                {

                    return SelectedComponentTreeNode.IsValid() && SelectedComponentTreeNode->Key != TEXT("Actor")

                        ? FSlateColor(FLinearColor(GetTraceSelectionAccentColor().R, GetTraceSelectionAccentColor().G, GetTraceSelectionAccentColor().B, 0.78f))

                        : FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));

                })

                [

                    SNew(SBorder)

                        .Padding(6.0f)

                        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                        [

                            SNew(SVerticalBox)

                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)

                                [

                                    SNew(SBorder)

                                        .Padding(FMargin(7.0f, 4.0f))

                                        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                                        .BorderBackgroundColor_Lambda([this]()

                                        {

                                            return SelectedComponentTreeNode.IsValid() && SelectedComponentTreeNode->Key != TEXT("Actor")

                                                ? FLinearColor(0.13f, 0.59f, 0.95f, 0.16f)

                                                : FLinearColor(1.0f, 1.0f, 1.0f, 0.03f);

                                        })

                                        [

                                            SNew(SHorizontalBox)

                                                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                                                [

                                                    SNew(STextBlock)

                                                        .Text(TMLoc::Text(TEXT("Events"), TEXT("")))

                                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                                                        .ColorAndOpacity_Lambda([this]()

                                                        {

                                                            return SelectedComponentTreeNode.IsValid() && SelectedComponentTreeNode->Key != TEXT("Actor")

                                                                ? FSlateColor(FLinearColor(0.74f, 0.88f, 1.0f))

                                                                : FSlateColor(FLinearColor(0.76f, 0.76f, 0.76f));

                                                        })

                                                ]

                                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                                [

                                                    SNew(SCheckBox)

                                                        .Style(FAppStyle::Get(), TEXT("ToggleButtonCheckbox"))

                                                        .Padding(FMargin(8.0f, 3.0f))

                                                        .IsChecked_Lambda([this]()

                                                        {

                                                            return bShowTraceDetails ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

                                                        })

                                                        .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)

                                                        {

                                                            bShowTraceDetails = NewState == ECheckBoxState::Checked;

                                                            RefreshChangeEventList();

                                                        })

                                                        [

                                                            SNew(STextBlock)

                                                                .Text(TMLoc::Text(TEXT("Detail Trace"), TEXT("")))

                                                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                                                                .ColorAndOpacity_Lambda([this]()

                                                                {

                                                                    return bShowTraceDetails

                                                                        ? FSlateColor(FLinearColor::White)

                                                                        : FSlateColor(FLinearColor(0.56f, 0.66f, 0.76f));

                                                                })

                                                        ]

                                                ]

                                        ]

                                ]

                                + SVerticalBox::Slot().FillHeight(1.0f)

                                [

                                    SAssignNew(ChangeEventScrollBox, SScrollBox)

                                        + SScrollBox::Slot()

                                        [

                                            SAssignNew(ChangeEventBox, SVerticalBox)

                                        ]

                                ]

                        ]

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildRawLogDrawer()

{

    return SAssignNew(RawLogDrawerBox, SBox)

        .HeightOverride(GetRawLogDrawerHeightOverride())

        [

            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()

                [

                    SNew(SBorder)

                        .Padding(0.0f)

                        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                        .BorderBackgroundColor(FLinearColor(0.45f, 0.45f, 0.45f, 0.55f))

                        .Cursor(EMouseCursor::ResizeUpDown)

                        .OnMouseButtonDown(this, &SInstanceReferenceTracker::HandleRawLogResizeMouseButtonDown)

                        .OnMouseMove(this, &SInstanceReferenceTracker::HandleRawLogResizeMouseMove)

                        .OnMouseButtonUp(this, &SInstanceReferenceTracker::HandleRawLogResizeMouseButtonUp)

                        [

                            SNew(SBox)

                                .HeightOverride(4.0f)

                        ]

                ]

                + SVerticalBox::Slot().FillHeight(1.0f)

                [

                    SAssignNew(RawLogDrawer, SExpandableArea)

                        .InitiallyCollapsed(true)

                        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                        .OnAreaExpansionChanged_Lambda([this](bool)

                        {

                            UpdateRawLogDrawerHeightOverride();

                            SaveUserLayoutSettings();

                        })

                        .HeaderContent()

                        [

                            SNew(SHorizontalBox)

                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                [

                                    SNew(STextBlock)

                                        .Text(TMLoc::Text(TEXT("Raw Log"), TEXT("")))

                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                                ]

                                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 0.0f, 8.0f, 0.0f)

                                [

                                    SAssignNew(RawLogPreviewText, STextBlock)

                                        .Text(TMLoc::Text(TEXT("No logs yet."), TEXT("")))

                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                                        .ColorAndOpacity(FLinearColor(0.62f, 0.62f, 0.62f))

                                ]

                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                [

                                    SNew(SButton)

                                        .ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))

                                        .ContentPadding(FMargin(7.0f, 3.0f))

                                        .Text(TMLoc::Text(TEXT("Copy Logs"), TEXT("")))

                                        .OnClicked_Lambda([this]()

                                        {

                                            CopyDeduplicatedRawLogToClipboard();

                                            return FReply::Handled();

                                        })

                                ]

                        ]

                        .BodyContent()

                        [

                            SNew(SBorder)

                                .Padding(4.0f)

                                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

                                [

                                    SAssignNew(LogBox, SScrollBox)

                                ]

                        ]

                ]

        ];

}



FOptionalSize SInstanceReferenceTracker::GetRawLogDrawerHeightOverride() const

{

    const bool bExpanded = RawLogDrawer.IsValid() && RawLogDrawer->IsExpanded();

    if (!bExpanded)

    {

        return FOptionalSize(MinRawLogDrawerHeight);

    }



    const float WidgetHeight = GetTickSpaceGeometry().GetLocalSize().Y;

    const float MaxHeight = WidgetHeight > 0.0f

        ? FMath::Max(MinRawLogDrawerHeight, WidgetHeight * 0.40f)

        : FMath::Max(DefaultRawLogDrawerHeight, RawLogDrawerHeight);



    return FOptionalSize(FMath::Clamp(RawLogDrawerHeight, MinRawLogDrawerHeight, MaxHeight));

}



void SInstanceReferenceTracker::UpdateRawLogDrawerHeightOverride()

{

    if (RawLogDrawerBox.IsValid())

    {

        RawLogDrawerBox->SetHeightOverride(GetRawLogDrawerHeightOverride());

    }

}



FReply SInstanceReferenceTracker::HandleRawLogResizeMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)

{

    if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)

    {

        return FReply::Unhandled();

    }



    if (RawLogDrawer.IsValid() && !RawLogDrawer->IsExpanded())

    {

        RawLogDrawer->SetExpanded(true);

    }



    bIsResizingRawLogDrawer = true;

    bRawLogHeightDirty = false;

    RawLogResizeStartHeight = RawLogDrawerHeight;

    RawLogResizeStartScreenPosition = MouseEvent.GetScreenSpacePosition();



    return FReply::Handled().CaptureMouse(AsShared());

}



FReply SInstanceReferenceTracker::HandleRawLogResizeMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)

{

    if (!bIsResizingRawLogDrawer)

    {

        return FReply::Unhandled();

    }



    const float DeltaY = MouseEvent.GetScreenSpacePosition().Y - RawLogResizeStartScreenPosition.Y;

    const float WidgetHeight = GetTickSpaceGeometry().GetLocalSize().Y;

    const float MaxHeight = WidgetHeight > 0.0f

        ? FMath::Max(MinRawLogDrawerHeight, WidgetHeight * 0.40f)

        : FMath::Max(DefaultRawLogDrawerHeight, RawLogResizeStartHeight);



    RawLogDrawerHeight = FMath::Clamp(RawLogResizeStartHeight - DeltaY, MinRawLogDrawerHeight, MaxHeight);

    bRawLogHeightDirty = true;

    UpdateRawLogDrawerHeightOverride();



    return FReply::Handled();

}



FReply SInstanceReferenceTracker::HandleRawLogResizeMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)

{

    if (!bIsResizingRawLogDrawer)

    {

        return FReply::Unhandled();

    }



    bIsResizingRawLogDrawer = false;

    if (bRawLogHeightDirty)

    {

        SaveUserLayoutSettings();

    }

    bRawLogHeightDirty = false;



    return FReply::Handled().ReleaseMouseCapture();

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildConfidenceGraph(const TSharedPtr<FTrackedStateChangeEvent>& Event) const

{

    static const FSlateRoundedBoxBrush ConfidenceChipBrush(FLinearColor(0.165f, 0.165f, 0.165f), 4.0f);



    TArray<FString> Steps;

    if (Event.IsValid())

    {

        FString Source = Event->CallerSummary.IsEmpty() ? Event->Diagnostic : Event->CallerSummary;

        Source.ReplaceInline(TEXT(">"), TEXT("|"));

        Source.ParseIntoArray(Steps, TEXT("|"), true);

        if (Steps.Num() == 0 && !Event->Reason.IsEmpty())

        {

            Steps.Add(Event->Reason);

        }

    }



    if (Steps.Num() == 0)

    {

        Steps.Add(TEXT("No matched call chain yet"));

    }



    TSharedRef<SScrollBox> StepScroll = SNew(SScrollBox).Orientation(Orient_Horizontal);

    for (int32 Index = 0; Index < Steps.Num(); ++Index)

    {

        FString StepText = Steps[Index].TrimStartAndEnd();

        if (StepText.IsEmpty())

        {

            StepText = TEXT("<empty>");

        }



        StepScroll->AddSlot().Padding(0.0f, 0.0f, 4.0f, 0.0f)

        [

            SNew(SButton)

                .ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))

                .ContentPadding(0.0f)

                .ToolTipText(TMLoc::Text(TEXT("Copy this confidence step"), TEXT("Copy this confidence step")))

                .OnClicked_Lambda([StepText]()

                {

                    FPlatformApplicationMisc::ClipboardCopy(*StepText);

                    return FReply::Handled();

                })

                [

                    SNew(SBorder)

                        .Padding(FMargin(7.0f, 3.0f))

                        .BorderImage(&ConfidenceChipBrush)

                        [

                            SNew(STextBlock)

                                .Text(FText::FromString(StepText))

                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                                .ColorAndOpacity(FLinearColor(0.86f, 0.86f, 0.86f))

                        ]

                ]

        ];



        if (Index < Steps.Num() - 1)

        {

            StepScroll->AddSlot().Padding(0.0f, 2.0f, 4.0f, 0.0f)

            [

                SNew(STextBlock)

                    .Text(TMLoc::Text(TEXT("\u25B6"), TEXT("\u25B6")))

                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                    .ColorAndOpacity(FLinearColor(0.42f, 0.42f, 0.42f))

            ];

        }

    }



    return SNew(SBorder)

        .Padding(6.0f)

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

        [

            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)

                [

                    SNew(STextBlock)

                        .Text(FText::FromString(Event.IsValid() ? Event->Confidence : TEXT("Confidence Graph")))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                        .ColorAndOpacity(FLinearColor(0.76f, 0.76f, 0.76f))

                ]

                + SVerticalBox::Slot().AutoHeight()

                [

                    StepScroll

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildChangeOverviewPanel(const TArray<TSharedPtr<FTrackedStateChangeEvent>>& VisibleEvents) const

{

    TMap<FString, int32> CategoryCounts;

    TSharedPtr<FTrackedStateChangeEvent> LatestEvent;

    for (const TSharedPtr<FTrackedStateChangeEvent>& Event : VisibleEvents)

    {

        if (!Event.IsValid())

        {

            continue;

        }



        CategoryCounts.FindOrAdd(Event->Category)++;

        if (!LatestEvent.IsValid() || Event->EventId > LatestEvent->EventId)

        {

            LatestEvent = Event;

        }

    }



    auto Shorten = [](const FString& Text, const int32 MaxLength)

    {

        return Text.Len() > MaxLength ? Text.Left(MaxLength - 3) + TEXT("...") : Text;

    };



    auto BuildCountTile = [&CategoryCounts](const FString& Category)

    {

        const int32 Count = CategoryCounts.FindRef(Category);

        const FLinearColor Accent = GetEventAccentColor(Category);

        const bool bHasCount = Count > 0;



        return SNew(SBorder)

            .Padding(FMargin(8.0f, 6.0f))

            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

            .BorderBackgroundColor(bHasCount

                ? FLinearColor(Accent.R, Accent.G, Accent.B, 0.18f)

                : FLinearColor(1.0f, 1.0f, 1.0f, 0.035f))

            [

                SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                    [

                        SNew(STextBlock)

                            .Text(FText::AsNumber(Count))

                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15))

                            .ColorAndOpacity(bHasCount ? Accent : FLinearColor(0.38f, 0.38f, 0.38f))

                    ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)

                    [

                        SNew(STextBlock)

                            .Text(FText::FromString(Category))

                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                            .ColorAndOpacity(bHasCount ? FLinearColor(0.86f, 0.90f, 0.95f) : FLinearColor(0.42f, 0.42f, 0.42f))

                    ]

            ];

    };



    auto GetValueColor = [](const FString& Value)

    {

        if (IsTraceValueTrue(Value) || Value.Equals(TEXT("added"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("<new>"), ESearchCase::IgnoreCase))

        {

            return FLinearColor(0.12f, 0.62f, 0.27f);

        }



        if (IsTraceValueFalse(Value) || Value.Equals(TEXT("None"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("<removed>"), ESearchCase::IgnoreCase))

        {

            return FLinearColor(0.72f, 0.12f, 0.10f);

        }



        return FLinearColor(0.18f, 0.38f, 0.72f);

    };



    auto BuildValueChip = [Shorten, GetValueColor](const FString& Value)

    {

        const FLinearColor ChipColor = GetValueColor(Value);

        return SNew(SBorder)

            .Padding(FMargin(7.0f, 2.0f))

            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

            .BorderBackgroundColor(ChipColor)

            [

                SNew(STextBlock)

                    .Text(FText::FromString(Shorten(Value, 28)))

                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                    .ColorAndOpacity(FLinearColor::White)

            ];

    };



    return SNew(SBorder)

        .Padding(8.0f)

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

        .BorderBackgroundColor(FLinearColor(0.06f, 0.07f, 0.08f, 0.98f))

        [

            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 7.0f)

                [

                    SNew(SBorder)

                        .Padding(FMargin(8.0f, 5.0f))

                        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

                        .BorderBackgroundColor(bPieActive

                            ? FLinearColor(0.10f, 0.34f, 0.16f, 0.55f)

                            : (bPieStarted ? FLinearColor(0.30f, 0.18f, 0.10f, 0.55f) : FLinearColor(0.08f, 0.09f, 0.11f, 0.65f)))

                        [

                            SNew(STextBlock)

                                .Text_Lambda([this]()

                                {

                                    return FText::FromString(BuildTimelineStatusText());

                                })

                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                                .ColorAndOpacity(bPieActive

                                    ? FLinearColor(0.74f, 1.0f, 0.80f)

                                    : (bPieStarted ? FLinearColor(1.0f, 0.78f, 0.58f) : FLinearColor(0.70f, 0.76f, 0.84f)))

                        ]

                ]

                + SVerticalBox::Slot().AutoHeight()

                [

                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)

                        [

                            BuildCountTile(TEXT("Visibility"))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)

                        [

                            BuildCountTile(TEXT("Reference"))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 0.0f)

                        [

                            BuildCountTile(TEXT("Lifecycle"))

                        ]

                        + SHorizontalBox::Slot().AutoWidth()

                        [

                            BuildCountTile(TEXT("Transform"))

                        ]

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)

                [

                    SNew(SBorder)

                        .Padding(FMargin(9.0f, 7.0f))

                        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

                        .BorderBackgroundColor(LatestEvent.IsValid()

                            ? FLinearColor(GetEventAccentColor(LatestEvent->Category).R, GetEventAccentColor(LatestEvent->Category).G, GetEventAccentColor(LatestEvent->Category).B, 0.12f)

                            : FLinearColor(1.0f, 1.0f, 1.0f, 0.035f))

                        [

                            SNew(SHorizontalBox)

                                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                                [

                                    SNew(SVerticalBox)

                                        + SVerticalBox::Slot().AutoHeight()

                                        [

                                            SNew(STextBlock)

                                                .Text(FText::FromString(LatestEvent.IsValid()

                                                    ? FString::Printf(TEXT("%s%s | %s"),

                                                        *LatestEvent->Category,

                                                        LatestEvent->RepeatCount > 1 ? *FString::Printf(TEXT(" x%d"), LatestEvent->RepeatCount) : TEXT(""),

                                                        *Shorten(LatestEvent->ActionSummary, 70))

                                                    : TEXT("No visible changes")))

                                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))

                                                .ColorAndOpacity(LatestEvent.IsValid() ? FLinearColor(0.92f, 0.94f, 0.96f) : FLinearColor(0.46f, 0.46f, 0.46f))

                                        ]

                                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)

                                        [

                                            SNew(STextBlock)

                                                .Text(FText::FromString(LatestEvent.IsValid()

                                                    ? Shorten(LatestEvent->StateLabel, 86)

                                                    : TEXT("Run PIE or change the tracked instance state.")))

                                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                                                .ColorAndOpacity(FLinearColor(0.64f, 0.68f, 0.74f))

                                                .AutoWrapText(true)

                                        ]

                                ]

                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)

                                [

                                    SNew(SHorizontalBox)

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                        [

                                            LatestEvent.IsValid() ? BuildValueChip(LatestEvent->OldValue) : SNullWidget::NullWidget

                                        ]

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7.0f, 0.0f)

                                        [

                                            SNew(STextBlock)

                                                .Text(TMLoc::Text(TEXT("\u25B6"), TEXT("\u25B6")))

                                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                                                .ColorAndOpacity(FLinearColor(0.58f, 0.64f, 0.72f))

                                                .Visibility(LatestEvent.IsValid() ? EVisibility::Visible : EVisibility::Collapsed)

                                        ]

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                        [

                                            LatestEvent.IsValid() ? BuildValueChip(LatestEvent->NewValue) : SNullWidget::NullWidget

                                        ]

                                ]

                        ]

                ]

        ];

}



FString SInstanceReferenceTracker::ExtractEventMetaValue(const FString& Source, const FString& Token) const

{

    int32 StartIndex = Source.Find(Token, ESearchCase::IgnoreCase);

    if (StartIndex == INDEX_NONE)

    {

        return TEXT("-");

    }



    StartIndex += Token.Len();

    FString Value = Source.Mid(StartIndex).TrimStartAndEnd();



    int32 CutIndex = INDEX_NONE;

    int32 PipeIndex = INDEX_NONE;

    if (Value.FindChar(TEXT('|'), PipeIndex))

    {

        CutIndex = PipeIndex;

    }



    int32 SemiIndex = INDEX_NONE;

    if (Value.FindChar(TEXT(';'), SemiIndex))

    {

        CutIndex = CutIndex == INDEX_NONE ? SemiIndex : FMath::Min(CutIndex, SemiIndex);

    }



    if (CutIndex != INDEX_NONE)

    {

        Value = Value.Left(CutIndex);

    }



    return Value.TrimStartAndEnd();

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildEventMetaCell(const FString& Label, const FString& Value) const

{

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)

        [

            SNew(STextBlock)

                .Text(FText::FromString(Label + TEXT(": ")))

                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                .ColorAndOpacity(FLinearColor(0.52f, 0.52f, 0.52f))

        ]

        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)

        [

            SNew(STextBlock)

                .Text(FText::FromString(Value))

                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                .ColorAndOpacity(FLinearColor(0.74f, 0.74f, 0.74f))

                .AutoWrapText(true)

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildEventValueChip(const FString& Value) const

{

    FLinearColor ChipColor = FLinearColor(0.18f, 0.26f, 0.38f);

    if (IsTraceValueTrue(Value) || Value == TEXT("<new>"))

    {

        ChipColor = FLinearColor(0.08f, 0.48f, 0.20f);

    }

    else if (IsTraceValueFalse(Value) || Value == TEXT("<removed>") || Value.Equals(TEXT("None"), ESearchCase::IgnoreCase))

    {

        ChipColor = FLinearColor(0.64f, 0.08f, 0.08f);

    }



    FString DisplayValue = Value;

    if (DisplayValue.Len() > 24)

    {

        DisplayValue = DisplayValue.Left(21) + TEXT("...");

    }



    return SNew(SBorder)

        .Padding(FMargin(6.0f, 2.0f))

        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

        .BorderBackgroundColor(ChipColor)

        [

            SNew(STextBlock)

                .Text(FText::FromString(DisplayValue))

                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                .ColorAndOpacity(FLinearColor::White)

        ];

}



void SInstanceReferenceTracker::ToggleEventExpansion(const TSharedPtr<FTrackedStateChangeEvent>& Event)

{

    if (!Event.IsValid())

    {

        return;

    }



    if (ExpandedEventIds.Contains(Event->EventId))

    {

        ExpandedEventIds.Remove(Event->EventId);

    }

    else

    {

        ExpandedEventIds.Add(Event->EventId);

    }



    RefreshChangeEventList();

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildEventCardAccentBar(const FLinearColor& AccentColor) const

{

    return SNew(SBox)

        .WidthOverride(4.0f)

        [

            SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                .BorderBackgroundColor(AccentColor)

        ];

}



FString SInstanceReferenceTracker::GetSimpleEventCategoryLabel(const TSharedPtr<FTrackedStateChangeEvent>& Event) const

{

    if (!Event.IsValid())

    {

        return TEXT("Other");

    }



    const FString& Category = Event->Category;

    if (Category == TEXT("Visibility")

        || Category == TEXT("Reference")

        || Category == TEXT("Transform")

        || Category == TEXT("Collision"))

    {

        return Category;

    }



    return TEXT("Other");

}



FString SInstanceReferenceTracker::GetSimpleEventSourceInstance(const TSharedPtr<FTrackedStateChangeEvent>& Event) const

{

    if (!Event.IsValid())

    {

        return TEXT("Unknown instance");

    }



    FString InstanceName = ExtractTraceToken(Event->CallerSummary, TEXT("Instance="));

    if (!InstanceName.IsEmpty())

    {

        return InstanceName;

    }



    InstanceName = ExtractTraceToken(Event->CallerSummary, TEXT("Actor="));

    if (!InstanceName.IsEmpty())

    {

        return InstanceName;

    }



    if (const TSharedPtr<FTrackedComponentTreeNode>* Node = ComponentTreeNodeByKey.Find(Event->NodeKey))

    {

        if (Node->IsValid())

        {

            if (const UObject* Object = (*Node)->Object.Get())

            {

                if (const AActor* Actor = Cast<AActor>(Object))

                {

                    return GetActorLabelSafe(Actor);

                }



                if (const UActorComponent* Component = Cast<UActorComponent>(Object))

                {

                    if (const AActor* Owner = Component->GetOwner())

                    {

                        return GetActorLabelSafe(Owner);

                    }

                }

            }

        }

    }



    return InitialActorLabel.IsEmpty() ? FString(TEXT("Unknown instance")) : InitialActorLabel;

}



FString SInstanceReferenceTracker::GetSimpleEventSourceClass(const TSharedPtr<FTrackedStateChangeEvent>& Event) const

{

    if (!Event.IsValid())

    {

        return TEXT("Unknown class");

    }



    FString ClassName = ExtractTraceToken(Event->CallerSummary, TEXT("Class="));

    if (!ClassName.IsEmpty())

    {

        return ClassName;

    }



    const FString ActorName = ExtractTraceToken(Event->CallerSummary, TEXT("Actor="));

    if (!ActorName.IsEmpty())

    {

        return ActorName;

    }



    if (const TSharedPtr<FTrackedComponentTreeNode>* Node = ComponentTreeNodeByKey.Find(Event->NodeKey))

    {

        if (Node->IsValid())

        {

            if (const UObject* Object = (*Node)->Object.Get())

            {

                if (const AActor* Actor = Cast<AActor>(Object))

                {

                    return GetActorClassNameSafe(Actor);

                }



                if (const UActorComponent* Component = Cast<UActorComponent>(Object))

                {

                    if (const AActor* Owner = Component->GetOwner())

                    {

                        return GetActorClassNameSafe(Owner);

                    }



                    if (Component->GetClass())

                    {

                        return Component->GetClass()->GetName();

                    }

                }

            }



            if (!(*Node)->TypeName.IsEmpty())

            {

                return (*Node)->TypeName;

            }

        }

    }



    if (const UClass* Class = InitialActorClass.Get())

    {

        return Class->GetName();

    }



    return TEXT("Unknown class");

}



FString SInstanceReferenceTracker::GetSimpleEventSourceFunction(const TSharedPtr<FTrackedStateChangeEvent>& Event) const

{

    if (!Event.IsValid())

    {

        return TEXT("Unknown function");

    }



    FString FunctionName = ExtractTraceToken(Event->CallerSummary, TEXT("Function="));

    if (FunctionName.IsEmpty())

    {

        FunctionName = ExtractTraceToken(Event->CallerSummary, TEXT("BlueprintGraph:"));

    }

    if (FunctionName.IsEmpty())

    {

        FunctionName = ExtractTraceToken(Event->CallerSummary, TEXT("NativeCall="));

    }

    if (FunctionName.IsEmpty())

    {

        FunctionName = ExtractTraceToken(Event->CallerSummary, TEXT("SelfAction="));

    }

    if (FunctionName.IsEmpty())

    {

        FunctionName = ExtractTraceToken(Event->CallerSummary, TEXT("Action="));

    }

    if (FunctionName.IsEmpty())

    {

        FunctionName = ExtractTraceToken(Event->CallerSummary, TEXT("Call="));

    }



    return FunctionName.IsEmpty() ? FString(TEXT("Unknown function")) : FunctionName;

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildSimpleEventCard(const TSharedPtr<FTrackedStateChangeEvent>& Event) const

{

    if (!Event.IsValid())

    {

        return SNew(STextBlock).Text(TMLoc::Text(TEXT("<Invalid Event>"), TEXT("<Invalid Event>")));

    }



    const FString Category = GetSimpleEventCategoryLabel(Event);

    const FLinearColor AccentColor = GetEventAccentColor(Category);

    const FString SourceInstance = GetSimpleEventSourceInstance(Event);

    const FString SourceClass = GetSimpleEventSourceClass(Event);

    const FString SourceFunction = GetSimpleEventSourceFunction(Event);

    const FString StateName = Event->StateLabel.IsEmpty() ? Event->StateKey : Event->StateLabel;

    const FString SourceLine = FString::Printf(TEXT("%s  |  %s.%s"),

        *SourceInstance,

        SourceClass.IsEmpty() ? TEXT("Unknown class") : *SourceClass,

        SourceFunction.IsEmpty() ? TEXT("Unknown function") : *SourceFunction);



    return SNew(SBorder)

        .Padding(0.0f)

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

        [

            SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Fill)

                [

                    BuildEventCardAccentBar(AccentColor)

                ]

                + SHorizontalBox::Slot().FillWidth(1.0f)

                [

                    SNew(SBorder)

                        .Padding(FMargin(9.0f, 8.0f))

                        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

                        .BorderBackgroundColor(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, 0.055f))

                        [

                            SNew(SVerticalBox)

                                + SVerticalBox::Slot().AutoHeight()

                                [

                                    SNew(SHorizontalBox)

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                        [

                                            SNew(STextBlock)

                                                .Text(FText::FromString(Event->Category))

                                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                                                .ColorAndOpacity(AccentColor)

                                        ]

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7.0f, 0.0f, 0.0f, 0.0f)

                                        [

                                            SNew(SBorder)

                                                .Padding(FMargin(7.0f, 2.0f))

                                                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                                                .BorderBackgroundColor(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, 0.30f))

                                                [

                                                    SNew(STextBlock)

                                                        .Text(FText::FromString(Category))

                                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                                                        .ColorAndOpacity(FLinearColor::White)

                                                ]

                                        ]

                                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(9.0f, 0.0f, 8.0f, 0.0f)

                                        [

                                            SNew(STextBlock)

                                                .Text(FText::FromString(Event->ActionSummary))

                                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                                                .ColorAndOpacity(FSlateColor::UseForeground())

                                                .AutoWrapText(true)

                                        ]

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                        [

                                            SNew(SBorder)

                                                .Padding(FMargin(6.0f, 1.0f))

                                                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                                                .BorderBackgroundColor(FLinearColor(0.42f, 0.32f, 0.08f, 0.62f))

                                                .Visibility(Event->RepeatCount > 1 ? EVisibility::Visible : EVisibility::Collapsed)

                                                [

                                                    SNew(STextBlock)

                                                        .Text(FText::FromString(FString::Printf(TEXT("x%d"), Event->RepeatCount)))

                                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                                                        .ColorAndOpacity(FLinearColor(1.0f, 0.88f, 0.42f))

                                                ]

                                        ]

                                ]

                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)

                                [

                                    SNew(STextBlock)

                                        .Text(FText::FromString(SourceLine))

                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                                        .ColorAndOpacity(FLinearColor(0.66f, 0.73f, 0.82f))

                                        .AutoWrapText(true)

                                ]

                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)

                                [

                                    SNew(SHorizontalBox)

                                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)

                                        [

                                            SNew(STextBlock)

                                                .Text(FText::FromString(StateName))

                                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                                                .ColorAndOpacity(FLinearColor(0.78f, 0.82f, 0.88f))

                                                .AutoWrapText(true)

                                        ]

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                        [

                                            BuildEventValueChip(Event->OldValue)

                                        ]

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)

                                        [

                                            SNew(STextBlock)

                                                .Text(TMLoc::Text(TEXT("->"), TEXT("->")))

                                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                                                .ColorAndOpacity(FLinearColor(0.58f, 0.64f, 0.72f))

                                        ]

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                        [

                                            BuildEventValueChip(Event->NewValue)

                                        ]

                                ]

                        ]

                ]

        ];

}

TSharedRef<SWidget> SInstanceReferenceTracker::BuildEventCardHeader(const TSharedPtr<FTrackedStateChangeEvent>& Event, const FLinearColor& AccentColor) const

{

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

        [

            SNew(STextBlock)

                .Text(FText::FromString(bShowTraceDetails

                    ? FString::Printf(TEXT("%s | %s"), *Event->Category, *Event->Confidence)

                    : Event->Category))

                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))

                .ColorAndOpacity(FLinearColor(0.68f, 0.68f, 0.68f))

        ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)

        [

            SNew(SBorder)

                .Padding(FMargin(6.0f, 1.0f))

                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                .BorderBackgroundColor(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, 0.30f))

                .Visibility(Event->RepeatCount > 1 ? EVisibility::Visible : EVisibility::Collapsed)

                [

                    SNew(STextBlock)

                        .Text(FText::FromString(FString::Printf(TEXT("x%d"), Event->RepeatCount)))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                        .ColorAndOpacity(FLinearColor(0.92f, 0.96f, 1.0f))

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildEventCardSummary(const TSharedPtr<FTrackedStateChangeEvent>& Event, const FLinearColor& AccentColor, bool bTraceVisible) const

{

    return SNew(SBorder)

        .Padding(FMargin(9.0f, 8.0f))

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

        .BorderBackgroundColor(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, 0.10f))

        [

            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()

                [

                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)

                        [

                            SNew(STextBlock)

                                .Text(TMLoc::Text(TEXT("Action: "), TEXT("Action: ")))

                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                                .ColorAndOpacity(AccentColor)

                        ]

                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)

                        [

                            SNew(STextBlock)

                                .Text(FText::FromString(Event->ActionSummary))

                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                                .ColorAndOpacity(FSlateColor::UseForeground())

                                .AutoWrapText(true)

                        ]

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)

                [

                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                        [

                            SNew(STextBlock)

                                .Text(FText::FromString(Event->StateLabel))

                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                                .ColorAndOpacity(FLinearColor(0.74f, 0.78f, 0.84f))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)

                        [

                            BuildEventValueChip(Event->OldValue)

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)

                        [

                            SNew(STextBlock)

                                .Text(TMLoc::Text(TEXT("\u25B6"), TEXT("\u25B6")))

                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                                .ColorAndOpacity(FLinearColor(0.58f, 0.64f, 0.72f))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                        [

                            BuildEventValueChip(Event->NewValue)

                        ]

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)

                [

                    SNew(STextBlock)

                        .Text(FText::FromString(Event->ActionSource))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                        .ColorAndOpacity(FLinearColor(0.60f, 0.70f, 0.88f))

                        .AutoWrapText(true)

                        .Visibility(bTraceVisible ? EVisibility::Visible : EVisibility::Collapsed)

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildEventTraceDetails(

    const TSharedPtr<FTrackedStateChangeEvent>& Event,

    const FString& ActorMeta,

    const FString& ClassMeta,

    const FString& RefMeta) const

{

    return SNew(SBorder)

        .Padding(8.0f)

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

        [

            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()

                [

                    SNew(STextBlock)

                        .Text(TMLoc::Text(TEXT("Trace Details"), TEXT("Trace Details")))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                        .ColorAndOpacity(FLinearColor(0.62f, 0.62f, 0.62f))

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)

                [

                    SNew(STextBlock)

                        .Text(FText::FromString(FString::Printf(TEXT("Name: %s"), *Event->StateLabel)))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                        .ColorAndOpacity(FSlateColor::UseForeground())

                        .AutoWrapText(true)

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)

                [

                    SNew(STextBlock)

                        .Text(FText::FromString(FString::Printf(TEXT("Change: %s -> %s"), *Event->OldValue, *Event->NewValue)))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))

                        .ColorAndOpacity(FLinearColor(0.82f, 0.82f, 0.82f))

                        .AutoWrapText(true)

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)

                [

                    SNew(STextBlock)

                        .Text(FText::FromString(FString::Printf(TEXT("Occurrences: %d"), Event->RepeatCount)))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                        .ColorAndOpacity(Event->RepeatCount > 1 ? FLinearColor(0.95f, 0.74f, 0.34f) : FLinearColor(0.60f, 0.60f, 0.60f))

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)

                [

                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)

                        [

                            BuildEventMetaCell(TEXT("Instance"), ActorMeta)

                        ]

                        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)

                        [

                            BuildEventMetaCell(TEXT("Class"), ClassMeta)

                        ]

                        + SHorizontalBox::Slot().FillWidth(1.4f)

                        [

                            BuildEventMetaCell(TEXT("Ref"), RefMeta)

                        ]

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)

                [

                    SNew(STextBlock)

                        .Text(FText::FromString(Event->Diagnostic))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                        .ColorAndOpacity(FLinearColor(0.68f, 0.68f, 0.52f))

                        .AutoWrapText(true)

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)

                [

                    BuildConfidenceGraph(Event)

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildEventCardExpandableContent(

    const TSharedPtr<FTrackedStateChangeEvent>& Event,

    const FLinearColor& AccentColor,

    bool bExpanded,

    bool bTraceVisible)

{

    const bool bHasRuntimeSource = HasRuntimeBlueprintExecutionSource(Event->CallerSummary);

    const bool bUnresolvedOnly = IsUnresolvedCallerSummary(Event->CallerSummary) && !bHasRuntimeSource;

    FString ActorMeta = bUnresolvedOnly ? FString(TEXT("Unresolved")) : ExtractEventMetaValue(Event->CallerSummary, TEXT("Instance="));

    if (!bUnresolvedOnly && ActorMeta.IsEmpty())

    {

        ActorMeta = ExtractEventMetaValue(Event->CallerSummary, TEXT("Actor="));

    }

    const FString ClassMeta = bUnresolvedOnly ? FString(TEXT("Unresolved")) : ExtractEventMetaValue(Event->CallerSummary, TEXT("Class="));

    FString RefMeta = ExtractEventMetaValue(Event->CallerSummary, TEXT("Ref="));

    if (RefMeta == TEXT("-"))

    {

        RefMeta = Event->NodeKey;

    }



    return SNew(SButton)

        .ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))

        .ContentPadding(0.0f)

        .OnClicked_Lambda([this, Event]()

        {

            ToggleEventExpansion(Event);

            return FReply::Handled();

        })

        [

            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()

                [

                    BuildEventCardHeader(Event, AccentColor)

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)

                [

                    BuildEventCardSummary(Event, AccentColor, bTraceVisible)

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 0.0f)

                [

                    SNew(SBox)

                        .Visibility(bExpanded ? EVisibility::Visible : EVisibility::Collapsed)

                        [

                            BuildEventTraceDetails(Event, ActorMeta, ClassMeta, RefMeta)

                        ]

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildEventCardActions(const TSharedPtr<FTrackedStateChangeEvent>& Event, bool bExpanded)

{

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

        [

            SNew(SButton)

                .Text(TMLoc::Text(TEXT("Select"), TEXT("")))

                .OnClicked_Lambda([this, Event]()

                {

                    FocusStateChangeEvent(Event);

                    return FReply::Handled();

                })

        ]

        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

        [

            SNew(SBox)

                .Visibility(bExpanded ? EVisibility::Visible : EVisibility::Collapsed)

                [

                    SNew(SButton)

                        .Text(TMLoc::Text(TEXT("Open BP"), TEXT("")))

                        .OnClicked_Lambda([this, Event]()

                        {

                            OpenStateChangeBlueprint(Event);

                            return FReply::Handled();

                        })

                ]

        ]

        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

        [

            SNew(SBox)

                .Visibility(bExpanded ? EVisibility::Visible : EVisibility::Collapsed)

                [

                    SNew(SButton)

                        .Text(PinnedNodeKeys.Contains(Event->NodeKey)

                            ? TMLoc::Text(TEXT("Unpin"), TEXT(""))

                            : TMLoc::Text(TEXT("Pin"), TEXT("")))

                        .OnClicked_Lambda([this, Event]()

                        {

                            TogglePinnedNode(Event->NodeKey);

                            return FReply::Handled();

                        })

                ]

        ]

        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

        [

            SNew(SBox)

                .Visibility(bExpanded ? EVisibility::Visible : EVisibility::Collapsed)

                [

                    SNew(SButton)

                        .Text(TMLoc::Text(TEXT("Ignore State"), TEXT("")))

                        .OnClicked_Lambda([this, Event]()

                        {

                            IgnoreStateKey(Event->StateKey);

                            return FReply::Handled();

                        })

                ]

        ]

        + SHorizontalBox::Slot().AutoWidth()

        [

            SNew(SButton)

                .ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))

                .ButtonColorAndOpacity(bExpanded

                    ? FLinearColor(0.19f, 0.35f, 0.52f, 0.92f)

                    : FLinearColor(0.13f, 0.59f, 0.95f, 0.96f))

                .ContentPadding(FMargin(10.0f, 4.0f))

                .OnClicked_Lambda([this, Event]()

                {

                    ToggleEventExpansion(Event);

                    return FReply::Handled();

                })

                [

                    SNew(STextBlock)

                        .Text(bExpanded

                            ? TMLoc::Text(TEXT("Hide Trace"), TEXT(""))

                            : TMLoc::Text(TEXT("Trace Details"), TEXT("")))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                        .ColorAndOpacity(FLinearColor::White)

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildEventCardBody(

    const TSharedPtr<FTrackedStateChangeEvent>& Event,

    const FLinearColor& AccentColor,

    bool bExpanded,

    bool bTraceVisible)

{

    return SNew(SBorder)

        .Padding(8.0f)

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

        [

            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()

                [

                    BuildEventCardExpandableContent(Event, AccentColor, bExpanded, bTraceVisible)

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 9.0f, 0.0f, 0.0f)

                [

                    BuildEventCardActions(Event, bExpanded)

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildEventCard(const TSharedPtr<FTrackedStateChangeEvent>& Event)

{

    if (!Event.IsValid())

    {

        return SNew(STextBlock).Text(TMLoc::Text(TEXT("<Invalid Event>"), TEXT("<Invalid Event>")));

    }



    if (!bShowTraceDetails)

    {

        return BuildSimpleEventCard(Event);

    }



    const FLinearColor AccentColor = GetEventAccentColor(Event->Category);

    const bool bExpanded = ExpandedEventIds.Contains(Event->EventId);

    const bool bTraceVisible = bExpanded || bShowTraceDetails;



    return SNew(SBorder)

        .Padding(0.0f)

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

        [

            SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Fill)

                [

                    BuildEventCardAccentBar(AccentColor)

                ]

                + SHorizontalBox::Slot().FillWidth(1.0f)

                [

                    BuildEventCardBody(Event, AccentColor, bExpanded, bTraceVisible)

                ]

        ];

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildEventFlowSummary(const TArray<TSharedPtr<FTrackedStateChangeEvent>>& VisibleEvents) const

{

    auto MakeSourceLabel = [this](const TSharedPtr<FTrackedStateChangeEvent>& Event)

    {

        if (!Event.IsValid())

        {

            return FString(TEXT("Unknown"));

        }



        const FString ClassName = ExtractTraceToken(Event->CallerSummary, TEXT("Class="));

        if (!ClassName.IsEmpty())

        {

            return ClassName;

        }



        const FString ActorName = ExtractTraceToken(Event->CallerSummary, TEXT("Actor="));

        if (!ActorName.IsEmpty())

        {

            return ActorName;

        }



        if (const TSharedPtr<FTrackedComponentTreeNode>* Node = ComponentTreeNodeByKey.Find(Event->NodeKey))

        {

            if (Node->IsValid())

            {

                if (const UObject* Object = (*Node)->Object.Get())

                {

                    if (const AActor* Actor = Cast<AActor>(Object))

                    {

                        return GetActorClassNameSafe(Actor);

                    }



                    if (const UActorComponent* Component = Cast<UActorComponent>(Object))

                    {

                        if (const AActor* Owner = Component->GetOwner())

                        {

                            return GetActorClassNameSafe(Owner);

                        }



                        return Component->GetClass() ? Component->GetClass()->GetName() : FString(TEXT("Component"));

                    }

                }



                if (!(*Node)->TypeName.IsEmpty())

                {

                    return (*Node)->TypeName;

                }

            }

        }



        if (const UClass* Class = InitialActorClass.Get())

        {

            return Class->GetName();

        }



        return FString(TEXT("Tracker"));

    };



    auto MakeStateLabel = [](const TSharedPtr<FTrackedStateChangeEvent>& Event)

    {

        if (!Event.IsValid())

        {

            return FString(TEXT("unknown"));

        }



        if (Event->Category == TEXT("Visibility"))

        {

            if (Event->StateKey.Contains(TEXT("HiddenInGame"), ESearchCase::IgnoreCase))

            {

                if (IsTraceValueTrue(Event->NewValue))

                {

                    return FString(TEXT("off"));

                }



                if (IsTraceValueFalse(Event->NewValue))

                {

                    return FString(TEXT("on"));

                }

            }



            if (Event->StateKey.Contains(TEXT("IsVisible"), ESearchCase::IgnoreCase)

                || Event->StateKey.Contains(TEXT("VisibleFlag"), ESearchCase::IgnoreCase))

            {

                if (IsTraceValueTrue(Event->NewValue))

                {

                    return FString(TEXT("on"));

                }



                if (IsTraceValueFalse(Event->NewValue))

                {

                    return FString(TEXT("off"));

                }

            }

        }



        if (Event->Category == TEXT("Reference"))

        {

            if (Event->NewValue == TEXT("None") || Event->NewValue == TEXT("<removed>"))

            {

                return FString(TEXT("removed"));

            }



            if (Event->OldValue == TEXT("None") || Event->OldValue == TEXT("<new>"))

            {

                return FString(TEXT("added"));

            }



            return FString(TEXT("changed"));

        }



        if (Event->Category == TEXT("Lifecycle"))

        {

            if (Event->StateKey.Contains(TEXT("Active"), ESearchCase::IgnoreCase))

            {

                return IsTraceValueTrue(Event->NewValue) ? FString(TEXT("active")) : FString(TEXT("inactive"));

            }



            if (Event->StateKey.Contains(TEXT("Registered"), ESearchCase::IgnoreCase))

            {

                return IsTraceValueTrue(Event->NewValue) ? FString(TEXT("registered")) : FString(TEXT("unregistered"));

            }



        }



        if (Event->Category == TEXT("Transform"))

        {

            if (Event->StateKey.Contains(TEXT("Location"), ESearchCase::IgnoreCase))

            {

                return FString(TEXT("moved"));

            }



            if (Event->StateKey.Contains(TEXT("Rotation"), ESearchCase::IgnoreCase))

            {

                return FString(TEXT("rotated"));

            }



            if (Event->StateKey.Contains(TEXT("Scale"), ESearchCase::IgnoreCase))

            {

                return FString(TEXT("scaled"));

            }



            if (Event->StateKey.Contains(TEXT("Attach"), ESearchCase::IgnoreCase))

            {

                return FString(TEXT("attached"));

            }

        }



        return Event->NewValue;

    };



    struct FSummaryStep

    {

        FString Source;

        FString State;

        FString Timeline;

        int32 RepeatCount = 1;

    };



    struct FSummaryFlow

    {

        FString Category;

        TArray<FSummaryStep> Steps;

        FString CurrentState;

    };



    auto BuildFlow = [this, &VisibleEvents, &MakeSourceLabel, &MakeStateLabel](const FString& Category)

    {

        FSummaryFlow Flow;

        Flow.Category = Category;



        TArray<TSharedPtr<FTrackedStateChangeEvent>> CategoryEvents;

        for (const TSharedPtr<FTrackedStateChangeEvent>& Event : VisibleEvents)

        {

            if (Event.IsValid() && Event->Category == Category)

            {

                CategoryEvents.Add(Event);

            }

        }



        if (CategoryEvents.Num() == 0)

        {

            return Flow;

        }



        const int32 MaxSteps = 6;

        const int32 StartIndex = FMath::Max(0, CategoryEvents.Num() - MaxSteps);

        for (int32 Index = StartIndex; Index < CategoryEvents.Num(); ++Index)

        {

            const TSharedPtr<FTrackedStateChangeEvent>& Event = CategoryEvents[Index];

            FSummaryStep Step;

            Step.Source = MakeSourceLabel(Event);

            if (Step.Source.Len() > 34)

            {

                Step.Source = Step.Source.Left(31) + TEXT("...");

            }

            Step.State = MakeStateLabel(Event);

            Step.Timeline.Reset();

            Step.RepeatCount = Event->RepeatCount;

            Flow.Steps.Add(Step);

        }



        Flow.CurrentState = MakeStateLabel(CategoryEvents.Last());

        return Flow;

    };



    const FString PriorityCategories[] =

    {

        TEXT("Visibility"),

        TEXT("Reference"),

        TEXT("Lifecycle"),

        TEXT("Transform")

    };



    TArray<FSummaryFlow> SummaryFlows;

    for (const FString& Category : PriorityCategories)

    {

        const FSummaryFlow Flow = BuildFlow(Category);

        if (Flow.Steps.Num() > 0)

        {

            SummaryFlows.Add(Flow);

        }

    }



    if (SummaryFlows.Num() > 3)

    {

        SummaryFlows.SetNum(3);

    }



    auto GetStateColor = [](const FString& State)

    {

        if (State == TEXT("on") || State == TEXT("added") || State == TEXT("active") || State == TEXT("registered"))

        {

            return FLinearColor(0.04f, 0.46f, 0.18f);

        }



        if (State == TEXT("off") || State == TEXT("removed") || State == TEXT("inactive") || State == TEXT("unregistered"))

        {

            return FLinearColor(0.66f, 0.08f, 0.07f);

        }



        if (State == TEXT("changed"))

        {

            return FLinearColor(0.10f, 0.35f, 0.82f);

        }



        if (State == TEXT("moved") || State == TEXT("rotated") || State == TEXT("scaled") || State == TEXT("attached"))

        {

            return FLinearColor(0.82f, 0.36f, 0.04f);

        }



        return FLinearColor(0.45f, 0.45f, 0.45f);

    };



    auto BuildStateChip = [GetStateColor](const FString& State)

    {

        return SNew(SBorder)

            .Padding(FMargin(7.0f, 2.0f))

            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

            .BorderBackgroundColor(GetStateColor(State))

            [

                SNew(STextBlock)

                    .Text(FText::FromString(State))

                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                    .ColorAndOpacity(FLinearColor::White)

            ];

    };



    auto BuildFlowArrow = []()

    {

        return SNew(SBorder)

            .Padding(FMargin(8.0f, 0.0f))

            .BorderImage(FAppStyle::GetBrush(TEXT("NoBrush")))

            [

                SNew(STextBlock)

                    .Text(TMLoc::Text(TEXT("\u25B6"), TEXT("\u25B6")))

                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                    .ColorAndOpacity(FLinearColor(0.56f, 0.62f, 0.70f))

            ];

    };



    TSharedRef<SVerticalBox> SummaryBox = SNew(SVerticalBox);



    if (SummaryFlows.Num() == 0)

    {

        for (const TSharedPtr<FTrackedStateChangeEvent>& Event : VisibleEvents)

        {

            if (Event.IsValid())

            {

                SummaryBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)

                [

                    SNew(STextBlock)

                        .Text(FText::FromString(FString::Printf(TEXT("%s: %s"),

                            *Event->Category,

                            *Event->ActionImpact)))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                        .ColorAndOpacity(FLinearColor(0.82f, 0.86f, 0.92f))

                        .AutoWrapText(true)

                ];

                break;

            }

        }

    }

    else

    {

        for (const FSummaryFlow& Flow : SummaryFlows)

        {

            TSharedRef<SScrollBox> FlowScroll = SNew(SScrollBox).Orientation(Orient_Horizontal);



            FlowScroll->AddSlot().Padding(0.0f, 0.0f, 6.0f, 0.0f)

            [

                SNew(STextBlock)

                    .Text(FText::FromString(Flow.Category + TEXT(" Flow:")))

                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                    .ColorAndOpacity(FLinearColor(0.72f, 0.72f, 0.72f))

            ];



            for (int32 Index = 0; Index < Flow.Steps.Num(); ++Index)

            {

                const FSummaryStep& Step = Flow.Steps[Index];

                FlowScroll->AddSlot().Padding(0.0f, 0.0f, 4.0f, 0.0f)

                [

                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                        [

                            SNew(STextBlock)

                                .Text(FText::FromString(FString::Printf(TEXT("[%s] %s"), *Step.Timeline, *Step.Source)))

                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                                .ColorAndOpacity(FLinearColor(0.78f, 0.80f, 0.84f))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)

                        [

                            BuildStateChip(Step.State)

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)

                        [

                            SNew(SBorder)

                                .Padding(FMargin(5.0f, 1.0f))

                                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                                .BorderBackgroundColor(FLinearColor(0.95f, 0.66f, 0.16f, 0.26f))

                                .Visibility(Step.RepeatCount > 1 ? EVisibility::Visible : EVisibility::Collapsed)

                                [

                                    SNew(STextBlock)

                                        .Text(FText::FromString(FString::Printf(TEXT("x%d"), Step.RepeatCount)))

                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                                        .ColorAndOpacity(FLinearColor(1.0f, 0.86f, 0.38f))

                                ]

                        ]

                ];



                FlowScroll->AddSlot().Padding(2.0f, 1.0f, 6.0f, 0.0f)

                [

                    BuildFlowArrow()

                ];

            }



            FlowScroll->AddSlot().Padding(0.0f, 0.0f, 4.0f, 0.0f)

            [

                SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                    [

                        SNew(STextBlock)

                            .Text(TMLoc::Text(TEXT("Current"), TEXT("Current")))

                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                            .ColorAndOpacity(FLinearColor(0.82f, 0.82f, 0.82f))

                    ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)

                    [

                        BuildStateChip(Flow.CurrentState)

                    ]

            ];



            SummaryBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 4.0f)

            [

                FlowScroll

            ];

        }

    }



    return SNew(SBorder)

        .Padding(8.0f)

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

        [

            SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()

                [

                    SNew(STextBlock)

                        .Text(TMLoc::Text(TEXT("Summary"), TEXT("Summary")))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                        .ColorAndOpacity(FLinearColor(0.72f, 0.72f, 0.72f))

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)

                [

                    SummaryBox

                ]

        ];

}



void SInstanceReferenceTracker::Construct(const FArguments& InArgs, AActor* InTargetActor)

{

    TargetActor = InTargetActor;

    InitialActorName = InTargetActor ? InTargetActor->GetFName() : NAME_None;

    InitialActorLabel = GetActorLabelSafe(InTargetActor);

    InitialActorGuid = InTargetActor ? InTargetActor->GetActorGuid() : FGuid();

    InitialActorInstanceGuid = InTargetActor ? InTargetActor->GetActorInstanceGuid() : FGuid();

    InitialActorClass = InTargetActor ? InTargetActor->GetClass() : nullptr;

    SessionStartSeconds = FPlatformTime::Seconds();

    LoadUserLayoutSettings();



    ChildSlot

    [

        SNew(SBorder)

            .Padding(8.0f)

            .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

            [

                SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)

                    [

                        BuildHeaderBar()

                    ]

                    + SVerticalBox::Slot().FillHeight(1.0f)

                    [

                        BuildMainPanel()

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)

                    [

                        BuildRawLogDrawer()

                    ]

            ]

    ];



    AppendLogLine(FString::Printf(TEXT("Tracker started. %s"), InstanceTraceVersion), FLinearColor(0.7f, 0.9f, 1.0f));

    if (AActor* ResolvedTarget = ResolveTargetActor())

    {

        RefreshTrackedComponentTree(ResolvedTarget);

        RebuildSnapshot(ResolvedTarget);

        DetectTargetStateChanges(ResolvedTarget);

    }

    RefreshChangeEventList();

    RefreshSummary();

    StatePollElapsed = 0.0f;

    ReferenceScanElapsed = 0.0f;

    TreeRefreshElapsed = 0.0f;

    bNeedsReferenceScan = false;

    bNeedsTreeRefresh = false;



    FInstanceTraceGlobalManager::Get().AddSubscriber(SharedThis(this));

}



FReply SInstanceReferenceTracker::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)

{

    if (bIsResizingRawLogDrawer)

    {

        return HandleRawLogResizeMouseMove(MyGeometry, MouseEvent);

    }



    return SCompoundWidget::OnMouseMove(MyGeometry, MouseEvent);

}



FReply SInstanceReferenceTracker::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)

{

    if (bIsResizingRawLogDrawer)

    {

        return HandleRawLogResizeMouseButtonUp(MyGeometry, MouseEvent);

    }



    return SCompoundWidget::OnMouseButtonUp(MyGeometry, MouseEvent);

}



void SInstanceReferenceTracker::RunTrackerTick(float InDeltaTime)

{

    const double TickStartSeconds = FPlatformTime::Seconds();

    AActor* ResolvedTarget = ResolveTargetActor();

    if (!ResolvedTarget)

    {

        if (!bReportedTargetDestroyed)

        {

            bReportedTargetDestroyed = true;

            AppendLogLine(TEXT("Target actor was destroyed or became invalid."), FLinearColor(1.0f, 0.35f, 0.25f));

        }

        return;

    }



    bReportedTargetDestroyed = false;

    StatePollElapsed += InDeltaTime;

    ReferenceScanElapsed += InDeltaTime;

    TreeRefreshElapsed += InDeltaTime;



    if ((bNeedsTreeRefresh || TreeRefreshElapsed >= TreeRefreshIntervalSeconds)

        && HasTrackerTickBudget(TickStartSeconds, TrackerTickBudgetSeconds))

    {

        RefreshTrackedComponentTree(ResolvedTarget);

        TreeRefreshElapsed = 0.0f;

        bNeedsTreeRefresh = false;

    }



    if (bPieActive && !bPieRuntimeBaselineReady)

    {

        return;

    }



    const double NowSeconds = FPlatformTime::Seconds();

    if (bPendingHookStateScan

        && NowSeconds - LastHookStateScanSeconds >= HookStateScanMinIntervalSeconds

        && HasTrackerTickBudget(TickStartSeconds, TrackerTickBudgetSeconds))

    {

        FString Reason = PendingHookReason.IsEmpty() ? TEXT("RuntimeHook") : PendingHookReason;

        if (PendingHookScanCount > 1)

        {

            Reason += FString::Printf(TEXT(" | Coalesced=%d"), PendingHookScanCount);

        }



        const FString RuntimeExecutionSummary = PendingHookRuntimeExecutionSummary;

        PendingHookReason.Empty();

        PendingHookRuntimeExecutionSummary.Empty();

        PendingHookScanCount = 0;

        bPendingHookStateScan = false;

        LastHookStateScanSeconds = NowSeconds;



        DetectTargetStateChangesFromHook(ResolvedTarget, Reason, RuntimeExecutionSummary);

        StatePollElapsed = 0.0f;

    }



    if (StatePollElapsed >= StatePollIntervalSeconds

        && HasTrackerTickBudget(TickStartSeconds, TrackerTickBudgetSeconds))

    {

        DetectTargetStateChanges(ResolvedTarget);

        StatePollElapsed = 0.0f;

    }



    if (bReferenceScanInProgress)

    {

        ProcessReferenceSnapshotScan(TickStartSeconds, ReferenceScanTickBudgetSeconds);

    }

    else if ((bNeedsReferenceScan || ReferenceScanElapsed >= ReferenceScanIntervalSeconds)

        && HasTrackerTickBudget(TickStartSeconds, TrackerTickBudgetSeconds))

    {

        RebuildSnapshot(ResolvedTarget);

        ReferenceScanElapsed = 0.0f;

        bNeedsReferenceScan = false;

    }



    FlushDeferredUiRefresh(false);

}



void SInstanceReferenceTracker::HandlePreBeginPIE(bool bIsSimulating)

{

    AActor* ResolvedTarget = ResolveTargetActor();

    if (!ResolvedTarget)

    {

        return;

    }



    CancelReferenceSnapshotScan();

    PieStartSeconds = FPlatformTime::Seconds();

    PieStartTimestamp = FDateTime::Now();

    PieEndSeconds = 0.0;

    PieEndTimestamp = FDateTime();

    bPieActive = true;

    bPieStarted = true;

    bPieRuntimeBaselineReady = false;

    PreviousTargetState = CaptureTargetState(ResolvedTarget);

    bHasTargetStateSnapshot = true;

    RefreshTrackedComponentTree(ResolvedTarget);

    StatePollElapsed = 0.0f;

    ReferenceScanElapsed = 0.0f;

    TreeRefreshElapsed = 0.0f;

    bNeedsReferenceScan = true;

    bNeedsTreeRefresh = true;

    ActionSummaryCache.Empty();

    StateCallerSummaryCache.Empty();

    RecentBlueprintExecutionSources.Empty();

    PendingHookReason.Empty();

    PendingHookRuntimeExecutionSummary.Empty();

    PendingHookScanCount = 0;

    bPendingHookStateScan = false;

    AddTimelineMarkerEvent(TEXT("Timeline.PIE.Start"), TEXT("Editor"), TEXT("Starting"), TEXT("PIEStart"), FLinearColor(0.18f, 0.72f, 0.36f));

    AppendLogLine(FString::Printf(TEXT("PIE starting. Time=%s | Session=%.2fs | Pre-play target state captured: Actor=%s | Class=%s | World=%s"),

        *PieStartTimestamp.ToString(TEXT("%H:%M:%S")),

        FMath::Max(0.0, PieStartSeconds - SessionStartSeconds),

        *GetActorLabelSafe(ResolvedTarget),

        *GetActorClassNameSafe(ResolvedTarget),

        *GetWorldTypeText(ResolvedTarget->GetWorld())), FLinearColor(0.7f, 0.9f, 1.0f));



    if (bAutoSaveSnapshots)

    {

        SaveTraceSnapshot(TEXT("PIE_Start"));

    }

}



void SInstanceReferenceTracker::HandlePostPIEStarted(bool bIsSimulating)

{

    AActor* ResolvedTarget = ResolveTargetActor();

    if (!ResolvedTarget)

    {

        AppendLogLine(TEXT("PIE started, but the matching runtime target actor could not be resolved."), FLinearColor(1.0f, 0.55f, 0.25f));

        return;

    }



    RefreshTrackedComponentTree(ResolvedTarget);

    RebuildSnapshot(ResolvedTarget);

    PreviousTargetState = CaptureTargetState(ResolvedTarget);

    bHasTargetStateSnapshot = true;

    bPieRuntimeBaselineReady = true;

    AddTimelineMarkerEvent(TEXT("Timeline.PIE.Ready"), TEXT("Starting"), TEXT("RuntimeBaseline"), TEXT("PIEReady"), FLinearColor(0.22f, 0.56f, 0.95f));

    AppendLogLine(FString::Printf(TEXT("PIE runtime baseline captured. Time=%s | PIE=%.2fs | Actor=%s | Class=%s | World=%s | Initialization transform diffs suppressed"),

        *FDateTime::Now().ToString(TEXT("%H:%M:%S")),

        FMath::Max(0.0, FPlatformTime::Seconds() - PieStartSeconds),

        *GetActorLabelSafe(ResolvedTarget),

        *GetActorClassNameSafe(ResolvedTarget),

        *GetWorldTypeText(ResolvedTarget->GetWorld())), FLinearColor(0.62f, 0.82f, 1.0f));

    StatePollElapsed = 0.0f;

    ReferenceScanElapsed = 0.0f;

    TreeRefreshElapsed = 0.0f;

    bNeedsReferenceScan = false;

    bNeedsTreeRefresh = false;

}



void SInstanceReferenceTracker::HandleEndPIE(bool bIsSimulating)

{

    CancelReferenceSnapshotScan();

    PieEndSeconds = FPlatformTime::Seconds();

    PieEndTimestamp = FDateTime::Now();

    AddTimelineMarkerEvent(TEXT("Timeline.PIE.End"), TEXT("Running"), TEXT("Ended"), TEXT("PIEEnd"), FLinearColor(0.82f, 0.46f, 0.24f));

    bPieActive = false;



    if (bAutoSaveSnapshots)

    {

        SaveTraceSnapshot(TEXT("PIE_End"));

    }



    bHasTargetStateSnapshot = false;

    bPieRuntimeBaselineReady = false;

    PreviousTargetState.Empty();

    PreviousSnapshot.Empty();

    LastReferencerListSignature.Empty();

    LastComponentTreeSignature.Empty();

    ActionSummaryCache.Empty();

    StateCallerSummaryCache.Empty();

    RecentBlueprintExecutionSources.Empty();

    PendingHookReason.Empty();

    PendingHookRuntimeExecutionSummary.Empty();

    PendingHookScanCount = 0;

    bPendingHookStateScan = false;

    bNeedsReferenceScan = true;

    bNeedsTreeRefresh = true;

    AppendLogLine(FString::Printf(TEXT("PIE ended. Time=%s | Duration=%.2fs | Runtime state tracking reset."),

        *PieEndTimestamp.ToString(TEXT("%H:%M:%S")),

        FMath::Max(0.0, PieEndSeconds - PieStartSeconds)), FLinearColor(0.7f, 0.7f, 0.7f));

}



void SInstanceReferenceTracker::HandleRenderStateDirty(UActorComponent& Component)

{

    AActor* ResolvedTarget = ResolveTargetActor();

    if (!ResolvedTarget || !IsTargetOrOwnedComponent(&Component, ResolvedTarget))

    {

        return;

    }



    const FString Reason = FString::Printf(TEXT("RenderStateDirty:%s"), *Component.GetName());

    if (!IsCollisionTraceReason(Reason))

    {

        return;

    }

    const FString RuntimeExecutionSummary = BuildCurrentBlueprintExecutionSummary(&Component, Reason);

    DetectTargetStateChangesFromHook(ResolvedTarget, Reason, RuntimeExecutionSummary);

    QueueHookStateScan(Reason, RuntimeExecutionSummary);

    bNeedsTreeRefresh = true;

}



void SInstanceReferenceTracker::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)

{

    AActor* ResolvedTarget = ResolveTargetActor();

    if (!ResolvedTarget || !IsTargetOrOwnedComponent(Object, ResolvedTarget))

    {

        return;

    }



    const FName PropertyName = PropertyChangedEvent.GetPropertyName();

    const FString Reason = FString::Printf(TEXT("PropertyChanged:%s"), PropertyName.IsNone() ? TEXT("<unknown>") : *PropertyName.ToString());

    if (IsIgnoredInstanceTraceReason(Reason))

    {

        return;

    }

    const FString RuntimeExecutionSummary = BuildCurrentBlueprintExecutionSummary(Object, Reason);

    if (IsCollisionTraceReason(Reason))

    {

        DetectTargetStateChangesFromHook(ResolvedTarget, Reason, RuntimeExecutionSummary);

    }

    QueueHookStateScan(Reason, RuntimeExecutionSummary);

    bNeedsTreeRefresh = true;

}



#if DO_BLUEPRINT_GUARD

void SInstanceReferenceTracker::HandleBlueprintScriptEnter(const FBlueprintContextTracker& ContextTracker, const UObject* ContextObject, const UFunction* ContextFunction)

{

    if (!IsInGameThread())

    {

        return;

    }



    const FString Summary = BuildBlueprintExecutionSummaryFromContext(&ContextTracker, ContextObject, ContextFunction, nullptr, TEXT("ScriptEnter"));

    if (Summary.IsEmpty())

    {

        return;

    }



    FRecentBlueprintExecutionSource RecentSource;

    RecentSource.Summary = Summary;

    RecentSource.InstanceName = ExtractTraceToken(Summary, TEXT("Instance="));

    RecentSource.ObjectName = ExtractTraceToken(Summary, TEXT("Object="));

    RecentSource.ComponentName = ExtractTraceToken(Summary, TEXT("Component="));

    RecentSource.ActorName = ExtractTraceToken(Summary, TEXT("Actor="));

    RecentSource.ClassName = ExtractTraceToken(Summary, TEXT("Class="));

    RecentSource.FunctionName = ExtractTraceToken(Summary, TEXT("Function="));
    if (TMTraceNoise::IsTickLike(RecentSource.FunctionName) || TMTraceNoise::IsTickLike(Summary))
    {
        return;
    }

    RecentSource.TimestampSeconds = FPlatformTime::Seconds();

    RecentBlueprintExecutionSources.Add(MoveTemp(RecentSource));



    constexpr int32 MaxRecentScriptSources = 64;

    while (RecentBlueprintExecutionSources.Num() > MaxRecentScriptSources)

    {

        RecentBlueprintExecutionSources.RemoveAt(0);

    }

}



void SInstanceReferenceTracker::HandleBlueprintScriptExit(const FBlueprintContextTracker& ContextTracker)

{

}

#endif



AActor* SInstanceReferenceTracker::ResolveTargetActor()

{

    AActor* CurrentTarget = TargetActor.Get();

    if (CurrentTarget && !CurrentTarget->IsPendingKillPending() && CurrentTarget->GetWorld() && CurrentTarget->GetWorld()->WorldType == EWorldType::PIE)

    {

        return CurrentTarget;

    }



    if (GEngine)

    {

        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())

        {

            UWorld* World = WorldContext.World();

            if (!World || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game))

            {

                continue;

            }



            if (AActor* Match = FindMatchingTargetInWorld(World))

            {

                if (TargetActor.Get() != Match)

                {

                    TargetActor = Match;

                    AppendLogLine(FString::Printf(TEXT("Tracking resolved target in %s world: Actor=%s | Class=%s"),

                        *GetWorldTypeText(World),

                        *GetActorLabelSafe(Match),

                        *GetActorClassNameSafe(Match)), FLinearColor(0.45f, 0.75f, 1.0f));

                }

                return Match;

            }

        }

    }



    if (CurrentTarget && !CurrentTarget->IsPendingKillPending())

    {

        return CurrentTarget;

    }



    if (GEngine)

    {

        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())

        {

            UWorld* World = WorldContext.World();

            if (!World || World->WorldType != EWorldType::Editor)

            {

                continue;

            }



            if (AActor* Match = FindMatchingTargetInWorld(World))

            {

                TargetActor = Match;

                AppendLogLine(FString::Printf(TEXT("Tracking resolved target in Editor world: Actor=%s | Class=%s"),

                    *GetActorLabelSafe(Match),

                    *GetActorClassNameSafe(Match)), FLinearColor(0.45f, 0.75f, 1.0f));

                return Match;

            }

        }

    }



    return nullptr;

}



AActor* SInstanceReferenceTracker::FindMatchingTargetInWorld(UWorld* World) const

{

    if (!World)

    {

        return nullptr;

    }



    AActor* LabelMatch = nullptr;

    TArray<AActor*> CompatibleCandidates;

    for (TActorIterator<AActor> It(World); It; ++It)

    {

        AActor* Candidate = *It;

        if (!IsMatchingTrackedActor(Candidate))

        {

            if (Candidate && !Candidate->IsPendingKillPending() && IsClassCompatibleWithTrace(Candidate->GetClass(), InitialActorClass.Get()))

            {

                CompatibleCandidates.Add(Candidate);

            }

            continue;

        }



        if (Candidate->GetFName() == InitialActorName)

        {

            return Candidate;

        }



        if (!LabelMatch)

        {

            LabelMatch = Candidate;

        }

    }



    if (LabelMatch)

    {

        return LabelMatch;

    }



    if (CompatibleCandidates.Num() == 1)

    {

        return CompatibleCandidates[0];

    }



    return nullptr;

}



bool SInstanceReferenceTracker::IsMatchingTrackedActor(const AActor* Candidate) const

{

    if (!Candidate || Candidate->IsPendingKillPending())

    {

        return false;

    }



    if ((InitialActorGuid.IsValid() && Candidate->GetActorGuid() == InitialActorGuid)

        || (InitialActorInstanceGuid.IsValid() && Candidate->GetActorInstanceGuid() == InitialActorInstanceGuid))

    {

        return true;

    }



    UClass* CandidateClass = Candidate->GetClass();

    UClass* ExpectedClass = InitialActorClass.Get();

    if (ExpectedClass && CandidateClass)

    {

        const bool bSameGeneratedClass = CandidateClass == ExpectedClass

            || CandidateClass->IsChildOf(ExpectedClass)

            || CandidateClass->GetFName() == ExpectedClass->GetFName()

            || CandidateClass->ClassGeneratedBy == ExpectedClass->ClassGeneratedBy;

        if (!bSameGeneratedClass)

        {

            return false;

        }

    }



    return Candidate->GetFName() == InitialActorName || GetActorLabelSafe(Candidate) == InitialActorLabel;

}



void SInstanceReferenceTracker::RebuildSnapshot(AActor* Target)

{

    StartReferenceSnapshotScan(Target);

    ProcessReferenceSnapshotScan(FPlatformTime::Seconds(), ReferenceScanTickBudgetSeconds);

}



void SInstanceReferenceTracker::StartReferenceSnapshotScan(AActor* Target)

{

    if (!Target)

    {

        return;

    }



    UWorld* World = Target->GetWorld();

    if (!World)

    {

        return;

    }



    if (bReferenceScanInProgress)

    {

        if (ReferenceScanTarget.Get() == Target)

        {

            bPendingReferenceScanRestart = true;

            return;

        }



        CancelReferenceSnapshotScan();

    }



    PendingReferenceSnapshot.Empty();

    ReferenceScanQueue.Reset();

    ReferenceScanQueueIndex = 0;

    ReferenceScanTarget = Target;

    ReferenceScanStartedSeconds = FPlatformTime::Seconds();

    bReferenceScanInProgress = true;

    bPendingReferenceScanRestart = false;



    for (TActorIterator<AActor> It(World); It; ++It)

    {

        AActor* Actor = *It;

        if (Actor && !Actor->IsPendingKillPending())

        {

            ReferenceScanQueue.Add(Actor);

        }

    }



    if (ULevel* TargetLevel = Target->GetLevel())

    {

        ALevelScriptActor* LevelScriptActor = TargetLevel->GetLevelScriptActor();

        if (!LevelScriptActor)

        {

            LevelScriptActor = World->GetLevelScriptActor(TargetLevel);

        }



        if (LevelScriptActor && !LevelScriptActor->IsPendingKillPending())

        {

            ReferenceScanQueue.AddUnique(LevelScriptActor);

        }

    }

}



void SInstanceReferenceTracker::ProcessReferenceSnapshotScan(double TickStartSeconds, double TimeBudgetSeconds)

{

    if (!bReferenceScanInProgress)

    {

        return;

    }



    AActor* Target = ReferenceScanTarget.Get();

    if (!Target)

    {

        CancelReferenceSnapshotScan();

        return;

    }



    const double NowSeconds = FPlatformTime::Seconds();

    if (NowSeconds - ReferenceScanStartedSeconds > MaxReferenceScanDurationSeconds)

    {

        AppendLogLine(FString::Printf(

            TEXT("Reference scan safety stop: processed %d/%d actors over %.2fs. Partial result kept to avoid editor overload."),

            ReferenceScanQueueIndex,

            ReferenceScanQueue.Num(),

            NowSeconds - ReferenceScanStartedSeconds),

            FLinearColor(1.0f, 0.62f, 0.22f));

        FinishReferenceSnapshotScan();

        return;

    }



    int32 ProcessedThisTick = 0;

    while (ReferenceScanQueue.IsValidIndex(ReferenceScanQueueIndex))

    {

        if (ProcessedThisTick > 0 && !HasTrackerTickBudget(TickStartSeconds, TimeBudgetSeconds))

        {

            break;

        }



        AActor* Actor = ReferenceScanQueue[ReferenceScanQueueIndex++].Get();

        if (!Actor || Actor->IsPendingKillPending())

        {

            continue;

        }



        TSet<FString> PropertyPaths;

        ScanObjectForTargetReferences(Actor, Target, PropertyPaths);

        ScanBlueprintGraphForTargetReferences(Actor, Target, PropertyPaths, TickStartSeconds + TimeBudgetSeconds);

        AddSnapshotEntry(PendingReferenceSnapshot, Actor, MoveTemp(PropertyPaths));

        ++ProcessedThisTick;

    }



    if (!ReferenceScanQueue.IsValidIndex(ReferenceScanQueueIndex))

    {

        FinishReferenceSnapshotScan();

    }

}



void SInstanceReferenceTracker::FinishReferenceSnapshotScan()

{

    const FString NewSnapshotSignature = BuildReferenceSnapshotSignature(PendingReferenceSnapshot);

    const bool bSnapshotChanged = NewSnapshotSignature != LastReferencerListSignature;



    if (bSnapshotChanged)

    {

        ActionSummaryCache.Empty();

        StateCallerSummaryCache.Empty();



        for (const TPair<TWeakObjectPtr<AActor>, TSet<FString>>& Pair : PendingReferenceSnapshot)

        {

            const TSet<FString>* PreviousPaths = PreviousSnapshot.Find(Pair.Key);

            for (const FString& Path : Pair.Value)

            {

                if (!PreviousPaths || !PreviousPaths->Contains(Path))

                {

                    AddReferenceEvent(TEXT("Added"), Pair.Key.Get(), Path, FLinearColor(0.35f, 1.0f, 0.45f));

                }

            }

        }



        for (const TPair<TWeakObjectPtr<AActor>, TSet<FString>>& Pair : PreviousSnapshot)

        {

            const TSet<FString>* NewPaths = PendingReferenceSnapshot.Find(Pair.Key);

            for (const FString& Path : Pair.Value)

            {

                if (!NewPaths || !NewPaths->Contains(Path))

                {

                    AddReferenceEvent(TEXT("Removed"), Pair.Key.Get(), Path, FLinearColor(1.0f, 0.55f, 0.25f));

                }

            }

        }



        RefreshCurrentReferencerList(PendingReferenceSnapshot);

        LastReferencerListSignature = NewSnapshotSignature;

        RequestDeferredUiRefresh(true, true);

    }



    PreviousSnapshot = MoveTemp(PendingReferenceSnapshot);

    ReferenceScanQueue.Reset();

    ReferenceScanQueueIndex = 0;

    ReferenceScanTarget.Reset();

    bReferenceScanInProgress = false;

    ReferenceScanElapsed = 0.0f;



    if (bPendingReferenceScanRestart)

    {

        bPendingReferenceScanRestart = false;

        bNeedsReferenceScan = true;

    }

}



void SInstanceReferenceTracker::CancelReferenceSnapshotScan()

{

    PendingReferenceSnapshot.Empty();

    ReferenceScanQueue.Reset();

    ReferenceScanQueueIndex = 0;

    ReferenceScanTarget.Reset();

    bReferenceScanInProgress = false;

    bPendingReferenceScanRestart = false;

}



void SInstanceReferenceTracker::AddSnapshotEntry(TMap<TWeakObjectPtr<AActor>, TSet<FString>>& Snapshot, AActor* SourceActor, TSet<FString>&& PropertyPaths) const

{

    if (!SourceActor || PropertyPaths.Num() == 0)

    {

        return;

    }



    TSet<FString>& ExistingPaths = Snapshot.FindOrAdd(SourceActor);

    for (FString& PropertyPath : PropertyPaths)

    {

        ExistingPaths.Add(MoveTemp(PropertyPath));

    }

}



void SInstanceReferenceTracker::DetectTargetStateChanges(AActor* Target)

{

    if (!Target)

    {

        return;

    }



    TMap<FString, FString> NewState = CaptureTargetState(Target);

    if (!bHasTargetStateSnapshot)

    {

        bHasTargetStateSnapshot = true;

        PreviousTargetState = MoveTemp(NewState);



        const FString* HiddenState = PreviousTargetState.Find(TEXT("Actor.HiddenInGame"));

        const FString* CollisionState = PreviousTargetState.Find(TEXT("Actor.EnableCollision"));

        AppendLogLine(FString::Printf(TEXT("Initial target state: Actor=%s | Class=%s | World=%s | HiddenInGame=%s | Collision=%s"),

            *GetActorLabelSafe(Target),

            *GetActorClassNameSafe(Target),

            *GetWorldTypeText(Target->GetWorld()),

            HiddenState ? **HiddenState : TEXT("<unknown>"),

            CollisionState ? **CollisionState : TEXT("<unknown>")), FLinearColor(0.7f, 0.9f, 1.0f));

        return;

    }



    for (const TPair<FString, FString>& Pair : NewState)

    {

        const FString* OldValue = PreviousTargetState.Find(Pair.Key);

        if (!OldValue)

        {

            if (IsNoisyInstanceTraceStateKey(Pair.Key))

            {

                continue;

            }

            AddStateChangeEvent(Pair.Key, TEXT("<new>"), Pair.Value, TEXT("State scan"), FString(), FLinearColor(0.85f, 0.85f, 0.45f));

            AppendLogLine(FString::Printf(TEXT("Target state added: Actor=%s | State=%s | Value=%s"),

                *GetActorLabelSafe(Target),

                *Pair.Key,

                *Pair.Value), FLinearColor(0.85f, 0.85f, 0.45f));

            continue;

        }



        if (*OldValue != Pair.Value)

        {

            if (IsNoisyInstanceTraceStateKey(Pair.Key))

            {

                continue;

            }

            FString CallerSummary = BuildStateChangeCallerSummary(Pair.Key);

            if (IsHiddenInGameStateKey(Pair.Key) || IsCollisionTraceStateKey(Pair.Key))

            {

                CallerSummary = MergeRuntimeExecutionSummary(BuildCurrentBlueprintExecutionSummary(Target, TEXT("State scan")), CallerSummary);

            }

            AddStateChangeEvent(Pair.Key, *OldValue, Pair.Value, TEXT("State scan"), CallerSummary, FLinearColor(1.0f, 0.85f, 0.25f));

            AppendLogLine(FString::Printf(TEXT("Target state changed: Actor=%s | Class=%s | State=%s | %s -> %s%s%s"),

                *GetActorLabelSafe(Target),

                *GetActorClassNameSafe(Target),

                *Pair.Key,

                **OldValue,

                *Pair.Value,

                CallerSummary.IsEmpty() ? TEXT("") : TEXT(" | Possible caller: "),

                CallerSummary.IsEmpty() ? TEXT("") : *CallerSummary), FLinearColor(1.0f, 0.85f, 0.25f));

        }

    }



    for (const TPair<FString, FString>& Pair : PreviousTargetState)

    {

        if (!NewState.Contains(Pair.Key))

        {

            if (IsNoisyInstanceTraceStateKey(Pair.Key))

            {

                continue;

            }

            AddStateChangeEvent(Pair.Key, Pair.Value, TEXT("<removed>"), TEXT("State scan"), FString(), FLinearColor(1.0f, 0.55f, 0.25f));

            AppendLogLine(FString::Printf(TEXT("Target state removed: Actor=%s | State=%s | Previous=%s"),

                *GetActorLabelSafe(Target),

                *Pair.Key,

                *Pair.Value), FLinearColor(1.0f, 0.55f, 0.25f));

        }

    }



    PreviousTargetState = MoveTemp(NewState);

}



void SInstanceReferenceTracker::DetectTargetStateChangesFromHook(AActor* Target, const FString& Reason, const FString& RuntimeExecutionSummary)

{

    if (!Target)

    {

        return;

    }



    if (!bHasTargetStateSnapshot)

    {

        PreviousTargetState = CaptureTargetState(Target);

        bHasTargetStateSnapshot = true;

        AppendLogLine(FString::Printf(TEXT("Hook baseline captured: Actor=%s | Class=%s | Reason=%s"),

            *GetActorLabelSafe(Target),

            *GetActorClassNameSafe(Target),

            *Reason), FLinearColor(0.65f, 0.85f, 1.0f));

        return;

    }



    const TMap<FString, FString> NewState = CaptureTargetState(Target);

    bool bAnyChange = false;



    for (const TPair<FString, FString>& Pair : NewState)

    {

        const FString* OldValue = PreviousTargetState.Find(Pair.Key);

        if (!OldValue || *OldValue != Pair.Value)

        {

            if (IsNoisyInstanceTraceStateKey(Pair.Key))

            {

                continue;

            }

            bAnyChange = true;

            FString CallerSummary = BuildStateChangeCallerSummary(Pair.Key);

            if (IsHiddenInGameStateKey(Pair.Key) || IsCollisionTraceStateKey(Pair.Key))

            {

                CallerSummary = MergeRuntimeExecutionSummary(RuntimeExecutionSummary, CallerSummary);

            }

            AddStateChangeEvent(Pair.Key, OldValue ? *OldValue : FString(TEXT("<new>")), Pair.Value, Reason, CallerSummary, FLinearColor(1.0f, 0.92f, 0.25f));

            AppendLogLine(FString::Printf(TEXT("Hooked target state changed: Actor=%s | Class=%s | Reason=%s | State=%s | %s -> %s%s%s"),

                *GetActorLabelSafe(Target),

                *GetActorClassNameSafe(Target),

                *Reason,

                *Pair.Key,

                OldValue ? **OldValue : TEXT("<new>"),

                *Pair.Value,

                CallerSummary.IsEmpty() ? TEXT("") : TEXT(" | Possible caller: "),

                CallerSummary.IsEmpty() ? TEXT("") : *CallerSummary), FLinearColor(1.0f, 0.92f, 0.25f));

        }

    }



    for (const TPair<FString, FString>& Pair : PreviousTargetState)

    {

        if (!NewState.Contains(Pair.Key))

        {

            if (IsNoisyInstanceTraceStateKey(Pair.Key))

            {

                continue;

            }

            bAnyChange = true;

            AddStateChangeEvent(Pair.Key, Pair.Value, TEXT("<removed>"), Reason, FString(), FLinearColor(1.0f, 0.55f, 0.25f));

            AppendLogLine(FString::Printf(TEXT("Hooked target state removed: Actor=%s | Reason=%s | State=%s | Previous=%s"),

                *GetActorLabelSafe(Target),

                *Reason,

                *Pair.Key,

                *Pair.Value), FLinearColor(1.0f, 0.55f, 0.25f));

        }

    }



    if (bAnyChange)

    {

        PreviousTargetState = NewState;

    }

}



void SInstanceReferenceTracker::QueueHookStateScan(const FString& Reason, const FString& RuntimeExecutionSummary)

{

    ++PendingHookScanCount;

    bPendingHookStateScan = true;



    if (PendingHookReason.IsEmpty())

    {

        PendingHookReason = Reason;

    }

    else if (!Reason.IsEmpty() && !PendingHookReason.Contains(Reason))

    {

        if (PendingHookReason.Len() < 220)

        {

            PendingHookReason += TEXT("; ");

            PendingHookReason += Reason;

        }

        else if (!PendingHookReason.EndsWith(TEXT("...")))

        {

            PendingHookReason += TEXT("; ...");

        }

    }



    if (!RuntimeExecutionSummary.IsEmpty())

    {

        PendingHookRuntimeExecutionSummary = RuntimeExecutionSummary;

    }

}



bool SInstanceReferenceTracker::IsTargetOrOwnedComponent(const UObject* Object, AActor* Target) const

{

    if (!Object || !Target)

    {

        return false;

    }



    if (Object == Target)

    {

        return true;

    }



    auto IsActorInTrackedHierarchy = [Target](const AActor* Candidate)

    {

        if (!Candidate)

        {

            return false;

        }



        if (Candidate == Target || Candidate->GetParentActor() == Target)

        {

            return true;

        }



        const AActor* ParentActor = Candidate->GetAttachParentActor();

        int32 Depth = 0;

        while (ParentActor && Depth++ < 8)

        {

            if (ParentActor == Target || ParentActor->GetParentActor() == Target)

            {

                return true;

            }



            ParentActor = ParentActor->GetAttachParentActor();

        }



        return false;

    };



    if (const AActor* Actor = Cast<AActor>(Object))

    {

        return IsActorInTrackedHierarchy(Actor);

    }



    const UActorComponent* Component = Cast<UActorComponent>(Object);

    return Component && IsActorInTrackedHierarchy(Component->GetOwner());

}



void SInstanceReferenceTracker::RefreshTrackedComponentTree(AActor* Target)

{

    if (!Target || !ComponentTreeView.IsValid())

    {

        return;

    }



    TArray<TSharedPtr<FTrackedComponentTreeNode>> NewRoots;

    TMap<FString, TSharedPtr<FTrackedComponentTreeNode>> NewNodeMap;

    TArray<FString> SignatureLines;

    SignatureLines.Add(FString::Printf(TEXT("Target=%llu|World=%s"),

        static_cast<uint64>((UPTRINT)Target),

        *GetWorldTypeText(Target->GetWorld())));



    auto MakeNode = [this, &NewNodeMap, &SignatureLines](const FString& Key, const FString& DisplayName, const FString& TypeName, UObject* Object)

    {

        TSharedPtr<FTrackedComponentTreeNode> Node = MakeShared<FTrackedComponentTreeNode>();

        Node->Key = Key;

        Node->DisplayName = DisplayName;

        Node->TypeName = TypeName;

        Node->Object = Object;



        if (const TArray<TSharedPtr<FTrackedStateChangeEvent>>* ExistingEvents = ChangeEventsByNodeKey.Find(Key))

        {

            Node->ChangeCount = ExistingEvents->Num();

        }



        NewNodeMap.Add(Key, Node);

        SignatureLines.Add(FString::Printf(TEXT("%s|%s|%s"), *Key, *DisplayName, *TypeName));

        return Node;

    };



    auto AddChild = [](const TSharedPtr<FTrackedComponentTreeNode>& ParentNode, const TSharedPtr<FTrackedComponentTreeNode>& ChildNode)

    {

        if (ParentNode.IsValid() && ChildNode.IsValid())

        {

            ParentNode->Children.Add(ChildNode);

        }

    };



    TSharedPtr<FTrackedComponentTreeNode> RootNode = MakeNode(TEXT("Actor"),

        FString::Printf(TEXT("%s (Self)"), *GetActorLabelSafe(Target)),

        GetActorClassNameSafe(Target),

        Target);

    NewRoots.Add(RootNode);



    TSet<const AActor*> VisitedActors;

    VisitedActors.Add(Target);



    TFunction<void(const AActor*, const FString&, const TSharedPtr<FTrackedComponentTreeNode>&)> AddComponentNodesForActor;

    TFunction<void(const AActor*, const FString&, const TSharedPtr<FTrackedComponentTreeNode>&, int32)> AddRelatedActorNode;



    AddComponentNodesForActor = [&MakeNode, &AddChild, &AddRelatedActorNode](const AActor* Actor, const FString& ActorPrefix, const TSharedPtr<FTrackedComponentTreeNode>& ActorNode)

    {

        if (!ActorNode.IsValid() || !Actor)

        {

            return;

        }



        TInlineComponentArray<UActorComponent*> Components(Actor);

        Components.Sort([](const UActorComponent& Left, const UActorComponent& Right)

        {

            const FString LeftKey = FString::Printf(TEXT("%s.%s"), *Left.GetName(), *Left.GetClass()->GetName());

            const FString RightKey = FString::Printf(TEXT("%s.%s"), *Right.GetName(), *Right.GetClass()->GetName());

            return LeftKey < RightKey;

        });



        TMap<const UActorComponent*, TSharedPtr<FTrackedComponentTreeNode>> LocalComponentNodes;

        for (UActorComponent* Component : Components)

        {

            if (!Component)

            {

                continue;

            }



            const FString NodeKey = ActorPrefix.IsEmpty()

                ? FString::Printf(TEXT("Component.%s"), *Component->GetName())

                : FString::Printf(TEXT("%s.Component.%s"), *ActorPrefix, *Component->GetName());

            const FString TypeName = Component->GetClass() ? Component->GetClass()->GetName() : TEXT("<Invalid Class>");

            LocalComponentNodes.Add(Component, MakeNode(NodeKey, Component->GetName(), TypeName, Component));

        }



        for (UActorComponent* Component : Components)

        {

            TSharedPtr<FTrackedComponentTreeNode> ComponentNode = LocalComponentNodes.FindRef(Component);

            if (!ComponentNode.IsValid())

            {

                continue;

            }



            TSharedPtr<FTrackedComponentTreeNode> ParentNode = ActorNode;

            if (const USceneComponent* SceneComponent = Cast<USceneComponent>(Component))

            {

                if (const USceneComponent* AttachParent = SceneComponent->GetAttachParent())

                {

                    if (AttachParent->GetOwner() == Actor)

                    {

                        if (TSharedPtr<FTrackedComponentTreeNode> AttachParentNode = LocalComponentNodes.FindRef(AttachParent))

                        {

                            ParentNode = AttachParentNode;

                        }

                    }

                }

            }



            AddChild(ParentNode, ComponentNode);

        }



        for (UActorComponent* Component : Components)

        {

            UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(Component);

            AActor* ChildActor = ChildActorComponent ? ChildActorComponent->GetChildActor() : nullptr;

            if (!ChildActor)

            {

                continue;

            }



            const FString ChildActorKey = ActorPrefix.IsEmpty()

                ? FString::Printf(TEXT("ChildActor.%s.%s"), *ChildActorComponent->GetName(), *ChildActor->GetName())

                : FString::Printf(TEXT("%s.ChildActor.%s.%s"), *ActorPrefix, *ChildActorComponent->GetName(), *ChildActor->GetName());

            TSharedPtr<FTrackedComponentTreeNode> ParentNode = LocalComponentNodes.FindRef(ChildActorComponent);

            AddRelatedActorNode(ChildActor, ChildActorKey, ParentNode.IsValid() ? ParentNode : ActorNode, 0);

        }

    };



    AddRelatedActorNode = [&MakeNode, &AddChild, &AddComponentNodesForActor, &AddRelatedActorNode, &VisitedActors](const AActor* RelatedActor, const FString& RelatedKey, const TSharedPtr<FTrackedComponentTreeNode>& ParentNode, const int32 Depth)

    {

        if (!RelatedActor || !ParentNode.IsValid() || Depth > 4 || VisitedActors.Contains(RelatedActor))

        {

            return;

        }



        VisitedActors.Add(RelatedActor);

        TSharedPtr<FTrackedComponentTreeNode> ActorNode = MakeNode(RelatedKey, GetActorLabelForTrace(RelatedActor), GetActorClassNameForTrace(RelatedActor), const_cast<AActor*>(RelatedActor));

        AddChild(ParentNode, ActorNode);

        AddComponentNodesForActor(RelatedActor, RelatedKey, ActorNode);



        TArray<AActor*> AttachedActors;

        RelatedActor->GetAttachedActors(AttachedActors);

        AttachedActors.Sort([](const AActor& Left, const AActor& Right)

        {

            return Left.GetName() < Right.GetName();

        });



        for (const AActor* AttachedActor : AttachedActors)

        {

            AddRelatedActorNode(AttachedActor, FString::Printf(TEXT("%s.AttachedActor.%s"), *RelatedKey, *AttachedActor->GetName()), ActorNode, Depth + 1);

        }

    };



    AddComponentNodesForActor(Target, FString(), RootNode);



    TArray<AActor*> AttachedActors;

    Target->GetAttachedActors(AttachedActors);

    AttachedActors.Sort([](const AActor& Left, const AActor& Right)

    {

        return Left.GetName() < Right.GetName();

    });



    for (const AActor* AttachedActor : AttachedActors)

    {

        AddRelatedActorNode(AttachedActor, FString::Printf(TEXT("AttachedActor.%s"), *AttachedActor->GetName()), RootNode, 0);

    }



    TFunction<void(const TSharedPtr<FTrackedComponentTreeNode>&, int32)> AssignDepth;

    AssignDepth = [&AssignDepth](const TSharedPtr<FTrackedComponentTreeNode>& Node, const int32 Depth)

    {

        if (!Node.IsValid())

        {

            return;

        }



        Node->Depth = Depth;

        for (const TSharedPtr<FTrackedComponentTreeNode>& Child : Node->Children)

        {

            AssignDepth(Child, Depth + 1);

        }

    };



    for (const TSharedPtr<FTrackedComponentTreeNode>& Root : NewRoots)

    {

        AssignDepth(Root, 0);

    }



    SignatureLines.Sort();

    const FString NewSignature = FString::Join(SignatureLines, TEXT("\n"));

    if (NewSignature == LastComponentTreeSignature)

    {

        return;

    }



    const FString PreviousSelectedKey = SelectedComponentTreeNode.IsValid() ? SelectedComponentTreeNode->Key : FString(TEXT("Actor"));

    LastComponentTreeSignature = NewSignature;

    ComponentTreeRoots = MoveTemp(NewRoots);

    ComponentTreeNodeByKey = MoveTemp(NewNodeMap);

    SelectedComponentTreeNode = ComponentTreeNodeByKey.FindRef(PreviousSelectedKey);

    if (!SelectedComponentTreeNode.IsValid())

    {

        SelectedComponentTreeNode = ComponentTreeNodeByKey.FindRef(TEXT("Actor"));

    }



    ComponentTreeView->RequestTreeRefresh();



    TFunction<void(const TSharedPtr<FTrackedComponentTreeNode>&)> ExpandNode;

    ExpandNode = [this, &ExpandNode](const TSharedPtr<FTrackedComponentTreeNode>& Node)

    {

        if (!Node.IsValid() || !ComponentTreeView.IsValid())

        {

            return;

        }



        ComponentTreeView->SetItemExpansion(Node, true);

        for (const TSharedPtr<FTrackedComponentTreeNode>& Child : Node->Children)

        {

            ExpandNode(Child);

        }

    };



    for (const TSharedPtr<FTrackedComponentTreeNode>& Root : ComponentTreeRoots)

    {

        ExpandNode(Root);

    }



    RefreshChangeEventList();

}



TSharedRef<ITableRow> SInstanceReferenceTracker::GenerateComponentTreeRow(TSharedPtr<FTrackedComponentTreeNode> InNode, const TSharedRef<STableViewBase>& OwnerTable)

{

    const int32 ChangeCount = GetVisibleChangeCountForTreeNode(InNode);

    const bool bIsSelected = InNode.IsValid() && SelectedComponentTreeNode.IsValid() && SelectedComponentTreeNode->Key == InNode->Key;

    const FLinearColor NameColor = InNode.IsValid() && ChangeCount > 0

        ? FLinearColor(1.0f, 0.86f, 0.35f)

        : FLinearColor(0.86f, 0.86f, 0.86f);

    const FString TypeName = InNode.IsValid() ? InNode->TypeName : FString();

    const bool bIsPinned = InNode.IsValid() && PinnedNodeKeys.Contains(InNode->Key);

    const int32 Depth = InNode.IsValid() ? InNode->Depth : 0;

    UObject* NodeObject = InNode.IsValid() ? InNode->Object.Get() : nullptr;



    TSharedRef<SHorizontalBox> RowContent = SNew(SHorizontalBox);

    for (int32 Index = 0; Index < Depth; ++Index)

    {

        RowContent->AddSlot().AutoWidth().VAlign(VAlign_Fill)

        [

            SNew(SBox)

                .WidthOverride(13.0f)

                [

                    SNew(SOverlay)

                        + SOverlay::Slot().HAlign(HAlign_Center)

                        [

                            SNew(SBox)

                                .WidthOverride(1.0f)

                                [

                                    SNew(SBorder)

                                        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                                        .BorderBackgroundColor(FLinearColor(0.45f, 0.55f, 0.65f, 0.22f))

                                ]

                        ]

                ]

        ];

    }



    RowContent->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 5.0f, 0.0f)

    [

        SNew(SImage)

            .Image(GetIconBrushForTrackedObject(NodeObject))

            .ColorAndOpacity(GetIconTintForTrackedObject(NodeObject))

    ];



    RowContent->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)

    [

        SNew(STextBlock)

            .Text(FText::FromString(InNode.IsValid()

                ? FString::Printf(TEXT("%s%s"), bIsPinned ? TEXT("[Pinned] ") : TEXT(""), *InNode->DisplayName)

                : FString(TEXT("<Invalid>"))))

            .ColorAndOpacity(bIsSelected ? FLinearColor(0.78f, 0.90f, 1.0f) : NameColor)

            .ToolTipText(FText::FromString(TypeName))

    ];



    RowContent->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)

    [

        SNew(SBorder)

            .Padding(FMargin(5.0f, 1.0f))

            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

            .BorderBackgroundColor(ChangeCount > 0 ? FLinearColor(0.95f, 0.66f, 0.16f, 0.28f) : FLinearColor::Transparent)

            .Visibility(ChangeCount > 0 ? EVisibility::Visible : EVisibility::Collapsed)

            [

                SNew(STextBlock)

                    .Text(FText::FromString(FString::Printf(TEXT("%d"), ChangeCount)))

                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                    .ColorAndOpacity(FLinearColor(1.0f, 0.84f, 0.30f))

            ]

    ];



    return SNew(STableRow<TSharedPtr<FTrackedComponentTreeNode>>, OwnerTable)

        .Padding(FMargin(2.0f, 1.0f))

        [

            SNew(SBorder)

                .Padding(FMargin(3.0f, 2.0f))

                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                .BorderBackgroundColor(bIsSelected ? FLinearColor(0.13f, 0.59f, 0.95f, 0.18f) : FLinearColor::Transparent)

                [

                    RowContent

                ]

        ];

}



void SInstanceReferenceTracker::GetComponentTreeChildren(TSharedPtr<FTrackedComponentTreeNode> InNode, TArray<TSharedPtr<FTrackedComponentTreeNode>>& OutChildren) const

{

    if (InNode.IsValid())

    {

        OutChildren.Append(InNode->Children);

    }

}



void SInstanceReferenceTracker::HandleComponentTreeSelectionChanged(TSharedPtr<FTrackedComponentTreeNode> InNode, ESelectInfo::Type SelectInfo)

{

    SelectedComponentTreeNode = InNode;

    RefreshChangeEventList();

}



void SInstanceReferenceTracker::CollectEventsForTreeNode(const TSharedPtr<FTrackedComponentTreeNode>& InNode, TArray<TSharedPtr<FTrackedStateChangeEvent>>& OutEvents) const

{

    if (!InNode.IsValid())

    {

        return;

    }



    if (const TArray<TSharedPtr<FTrackedStateChangeEvent>>* DirectEvents = ChangeEventsByNodeKey.Find(InNode->Key))

    {

        OutEvents.Append(*DirectEvents);

    }



    for (const TSharedPtr<FTrackedComponentTreeNode>& ChildNode : InNode->Children)

    {

        CollectEventsForTreeNode(ChildNode, OutEvents);

    }

}



int32 SInstanceReferenceTracker::GetVisibleChangeCountForTreeNode(const TSharedPtr<FTrackedComponentTreeNode>& InNode) const

{

    if (!InNode.IsValid())

    {

        return 0;

    }



    TArray<TSharedPtr<FTrackedStateChangeEvent>> NodeEvents;

    CollectEventsForTreeNode(InNode, NodeEvents);



    int32 VisibleCount = 0;

    for (const TSharedPtr<FTrackedStateChangeEvent>& Event : NodeEvents)

    {

        if (IsStateChangeEventVisible(Event))

        {

            ++VisibleCount;

        }

    }



    return VisibleCount;

}



void SInstanceReferenceTracker::AddStateChangeEvent(const FString& StateKey, const FString& OldValue, const FString& NewValue, const FString& Reason, const FString& CallerSummary, const FLinearColor& Color)

{

    if (IsNoisyInstanceTraceStateKey(StateKey) || IsIgnoredInstanceTraceReason(Reason)
        || TMTraceNoise::IsTickLike(StateKey) || TMTraceNoise::IsTickLike(Reason))

    {

        return;

    }

    const FString NodeKey = ExtractTrackedNodeKeyFromStateKey(StateKey);

    const FString Category = ClassifyStateChange(StateKey, Reason);

    const FString EnrichedCallerSummary = WatchRuleMode == EWatchRuleMode::Visibility && Category == TEXT("Visibility")

        ? BuildWatchRuleVisibilityCallerSummary(StateKey, NewValue, CallerSummary)

        : CallerSummary;

    const FString ActionSummary = BuildActionSummaryText(StateKey, OldValue, NewValue, Reason, EnrichedCallerSummary);

    const FString ActionSource = BuildActionSourceText(Reason, EnrichedCallerSummary);

    const FString ActionImpact = BuildActionImpactText(StateKey, OldValue, NewValue);

    const FString Diagnostic = BuildDiagnosticText(StateKey, OldValue, NewValue, Reason, EnrichedCallerSummary);

    const FString CoalesceSignature = FString::Printf(TEXT("%s|%s|%s|%s|%s|%s|%s"),

        *NodeKey,

        *StateKey,

        *OldValue,

        *NewValue,

        *Reason,

        *ActionSummary,

        *EnrichedCallerSummary);



    const double NowSeconds = FPlatformTime::Seconds();

    const double RelativeSeconds = FMath::Max(0.0, NowSeconds - SessionStartSeconds);

    const FDateTime NowTimestamp = FDateTime::Now();

    const double PieRelativeSeconds = bPieStarted

        ? FMath::Max(0.0, NowSeconds - PieStartSeconds)

        : 0.0;

    const FString TimelineLabel = BuildTimelineLabel(NowSeconds, NowTimestamp);

    const bool bTransformNoise = Category == TEXT("Transform");
    const bool bVisibilityNoise = Category == TEXT("Visibility");
    if (bTransformNoise || bVisibilityNoise)
    {
        const double MergeWindow = TMTraceNoise::DuplicateWindowSeconds(Category);
        const int32 FirstCandidate = FMath::Max(0, RecentStateChangeEvents.Num() - 48);
        for (int32 Index = RecentStateChangeEvents.Num() - 1; Index >= FirstCandidate; --Index)
        {
            const TSharedPtr<FTrackedStateChangeEvent>& ExistingEvent = RecentStateChangeEvents[Index];
            if (!ExistingEvent.IsValid() || ExistingEvent->NodeKey != NodeKey || ExistingEvent->StateKey != StateKey)
            {
                continue;
            }
            if (RelativeSeconds - ExistingEvent->LastRelativeSeconds > MergeWindow)
            {
                break;
            }
            if (bVisibilityNoise && ExistingEvent->NewValue != NewValue)
            {
                continue;
            }

            ++ExistingEvent->RepeatCount;
            ExistingEvent->NewValue = NewValue;
            ExistingEvent->Reason = Reason;
            ExistingEvent->CallerSummary = EnrichedCallerSummary;
            ExistingEvent->Confidence = BuildConfidenceText(Reason, EnrichedCallerSummary);
            ExistingEvent->Diagnostic = Diagnostic;
            ExistingEvent->ActionSummary = ActionSummary;
            ExistingEvent->ActionSource = ActionSource;
            ExistingEvent->ActionImpact = ActionImpact;
            ExistingEvent->LastTimestamp = NowTimestamp;
            ExistingEvent->LastRelativeSeconds = RelativeSeconds;
            ExistingEvent->LastPieRelativeSeconds = PieRelativeSeconds;
            ExistingEvent->LastTimelineLabel = TimelineLabel;
            ExistingEvent->Color = Color;
            ExistingEvent->EventId = NextEventId++;
            RequestDeferredUiRefresh(true, true);
            return;
        }
    }

    for (int32 Index = RecentStateChangeEvents.Num() - 1; Index >= 0; --Index)

    {

        const TSharedPtr<FTrackedStateChangeEvent>& ExistingEvent = RecentStateChangeEvents[Index];

        if (!ExistingEvent.IsValid())

        {

            continue;

        }



        if (ExistingEvent->CoalesceSignature == CoalesceSignature)

        {

            ++ExistingEvent->RepeatCount;

            ExistingEvent->LastTimestamp = NowTimestamp;

            ExistingEvent->LastRelativeSeconds = RelativeSeconds;

            ExistingEvent->LastPieRelativeSeconds = PieRelativeSeconds;

            ExistingEvent->LastTimelineLabel = TimelineLabel;

            ExistingEvent->Color = Color;

            ExistingEvent->EventId = NextEventId++;



            if (ComponentTreeView.IsValid())

            {

                ComponentTreeView->RequestTreeRefresh();

            }



            RequestDeferredUiRefresh(true, true);

            return;

        }

    }



    TSharedPtr<FTrackedStateChangeEvent> Event = MakeShared<FTrackedStateChangeEvent>();

    Event->NodeKey = NodeKey;

    Event->StateKey = StateKey;

    Event->StateLabel = BuildStateLabelForNode(StateKey, NodeKey);

    Event->Category = Category;

    Event->Confidence = BuildConfidenceText(Reason, EnrichedCallerSummary);

    Event->Diagnostic = Diagnostic;

    Event->ActionSummary = ActionSummary;

    Event->ActionSource = ActionSource;

    Event->ActionImpact = ActionImpact;

    Event->OldValue = OldValue;

    Event->NewValue = NewValue;

    Event->Reason = Reason;

    Event->CallerSummary = EnrichedCallerSummary;

    Event->Timestamp = NowTimestamp;

    Event->LastTimestamp = NowTimestamp;

    Event->RelativeSeconds = RelativeSeconds;

    Event->LastRelativeSeconds = RelativeSeconds;

    Event->PieRelativeSeconds = PieRelativeSeconds;

    Event->LastPieRelativeSeconds = PieRelativeSeconds;

    Event->TimelineLabel = TimelineLabel;

    Event->LastTimelineLabel = TimelineLabel;

    Event->Color = Color;

    Event->CoalesceSignature = CoalesceSignature;

    Event->EventId = NextEventId++;



    TArray<TSharedPtr<FTrackedStateChangeEvent>>& NodeEvents = ChangeEventsByNodeKey.FindOrAdd(NodeKey);

    NodeEvents.Add(Event);

    while (NodeEvents.Num() > 80)

    {

        NodeEvents.RemoveAt(0);

    }



    RecentStateChangeEvents.Add(Event);

    while (RecentStateChangeEvents.Num() > 200)

    {

        RecentStateChangeEvents.RemoveAt(0);

    }



    if (TSharedPtr<FTrackedComponentTreeNode>* Node = ComponentTreeNodeByKey.Find(NodeKey))

    {

        (*Node)->ChangeCount = NodeEvents.Num();

        if (ComponentTreeView.IsValid())

        {

            ComponentTreeView->RequestTreeRefresh();

        }

    }



    RequestDeferredUiRefresh(true, true);

}



FString SInstanceReferenceTracker::BuildTimelineLabel(double NowSeconds, const FDateTime& NowTimestamp) const

{

    const FString WallClock = NowTimestamp.ToString(TEXT("%H:%M:%S"));

    if (bPieActive)

    {

        return FString::Printf(TEXT("PIE +%.2fs | %s"), FMath::Max(0.0, NowSeconds - PieStartSeconds), *WallClock);

    }



    if (bPieStarted)

    {

        return FString::Printf(TEXT("After PIE +%.2fs | %s"), FMath::Max(0.0, NowSeconds - PieEndSeconds), *WallClock);

    }



    return FString::Printf(TEXT("Editor +%.2fs | %s"), FMath::Max(0.0, NowSeconds - SessionStartSeconds), *WallClock);

}



FString SInstanceReferenceTracker::BuildTimelineStatusText() const

{

    if (bPieActive)

    {

        return FString::Printf(TEXT("Timeline: PIE running | Start %s | Runtime %.2fs"),

            *PieStartTimestamp.ToString(TEXT("%H:%M:%S")),

            FMath::Max(0.0, FPlatformTime::Seconds() - PieStartSeconds));

    }



    if (bPieStarted)

    {

        return FString::Printf(TEXT("Timeline: PIE ended | Start %s | End %s | Duration %.2fs"),

            *PieStartTimestamp.ToString(TEXT("%H:%M:%S")),

            *PieEndTimestamp.ToString(TEXT("%H:%M:%S")),

            FMath::Max(0.0, PieEndSeconds - PieStartSeconds));

    }



    return FString::Printf(TEXT("Timeline: Editor session | Open %.2fs ago"), FMath::Max(0.0, FPlatformTime::Seconds() - SessionStartSeconds));

}



void SInstanceReferenceTracker::AddTimelineMarkerEvent(const FString& MarkerKey, const FString& OldValue, const FString& NewValue, const FString& Reason, const FLinearColor& Color)

{

    AddStateChangeEvent(MarkerKey, OldValue, NewValue, Reason, FString(), Color);

}



void SInstanceReferenceTracker::RequestDeferredUiRefresh(bool bRefreshEvents, bool bRefreshSummary)

{

    bNeedsChangeEventListRefresh = bNeedsChangeEventListRefresh || bRefreshEvents;

    bNeedsSummaryRefresh = bNeedsSummaryRefresh || bRefreshSummary;

}



void SInstanceReferenceTracker::FlushDeferredUiRefresh(bool bForce)

{

    if (!bNeedsChangeEventListRefresh && !bNeedsSummaryRefresh)

    {

        return;

    }



    const double NowSeconds = FPlatformTime::Seconds();

    if (!bForce && NowSeconds - LastDeferredUiRefreshSeconds < DeferredUiRefreshIntervalSeconds)

    {

        return;

    }



    const bool bRefreshEvents = bNeedsChangeEventListRefresh;

    const bool bRefreshSummary = bNeedsSummaryRefresh;

    bNeedsChangeEventListRefresh = false;

    bNeedsSummaryRefresh = false;

    LastDeferredUiRefreshSeconds = NowSeconds;



    if (bRefreshEvents)

    {

        RefreshChangeEventList();

    }



    if (bRefreshSummary)

    {

        RefreshSummary();

    }

}



bool SInstanceReferenceTracker::HasTrackerTickBudget(double TickStartSeconds, double TimeBudgetSeconds) const

{

    return FPlatformTime::Seconds() - TickStartSeconds < TimeBudgetSeconds;

}



bool SInstanceReferenceTracker::IsStateChangeEventVisible(const TSharedPtr<FTrackedStateChangeEvent>& Event) const

{

    if (!Event.IsValid())

    {

        return false;

    }

    if (Event->StateKey.Contains(TEXT("Tick"), ESearchCase::IgnoreCase)

        || Event->Reason.Equals(TEXT("Tick"), ESearchCase::IgnoreCase)

        || Event->ActionSummary.Contains(TEXT("tick"), ESearchCase::IgnoreCase)

        || IsNoisyInstanceTraceStateKey(Event->StateKey))

    {

        return false;

    }



    const bool bCriticalCollisionEvent = Event->Category == TEXT("Collision") || IsCollisionTraceStateKey(Event->StateKey);



    if (!bShowIgnored && IgnoredStateKeys.Contains(Event->StateKey) && !bCriticalCollisionEvent)

    {

        return false;

    }



    if (bPinnedOnly && !PinnedNodeKeys.Contains(Event->NodeKey) && !bCriticalCollisionEvent)

    {

        return false;

    }



    if (!bCriticalCollisionEvent && !DoesEventMatchWatchRule(Event))

    {

        return false;

    }



    const FString Category = bShowTraceDetails ? Event->Category : GetSimpleEventCategoryLabel(Event);

    if (!bCriticalCollisionEvent

        && ((Category == TEXT("Visibility") && !bFilterVisibility)

            || (Category == TEXT("Collision") && !bFilterCollision)

            || (Category == TEXT("Transform") && !bFilterTransform)

            || (Category == TEXT("Material") && !bFilterMaterial)

            || (Category == TEXT("Lifecycle") && !bFilterLifecycle)

            || (Category == TEXT("Reference") && !bFilterReference)

            || (Category == TEXT("Other") && !bFilterOther)))

    {

        return false;

    }



    if (!EventSearchText.IsEmpty())

    {

        const TSharedPtr<FTrackedComponentTreeNode>* Node = ComponentTreeNodeByKey.Find(Event->NodeKey);

        const FString Haystack = FString::Printf(TEXT("%s %s %s %s %s %s %s %s %s %s"),

            *Event->StateKey,

            *Event->StateLabel,

            *Event->Category,

            *Event->Confidence,

            *Event->ActionSummary,

            *Event->ActionSource,

            *Event->ActionImpact,

            *Event->Reason,

            *Event->CallerSummary,

            Node && Node->IsValid() ? *(*Node)->DisplayName : TEXT(""));

        if (!Haystack.Contains(EventSearchText, ESearchCase::IgnoreCase))

        {

            return false;

        }

    }



    return true;

}



bool SInstanceReferenceTracker::DoesEventMatchWatchRule(const TSharedPtr<FTrackedStateChangeEvent>& Event) const

{

    if (!Event.IsValid() || WatchRuleMode == EWatchRuleMode::All)

    {

        return true;

    }



    switch (WatchRuleMode)

    {

    case EWatchRuleMode::Visibility:

        return Event->Category == TEXT("Visibility");

    case EWatchRuleMode::Reference:

        return Event->Category == TEXT("Reference");

    case EWatchRuleMode::Lifecycle:

        return Event->Category == TEXT("Lifecycle");

    case EWatchRuleMode::Transform:

        return Event->Category == TEXT("Transform");

    case EWatchRuleMode::All:

    default:

        return true;

    }

}



void SInstanceReferenceTracker::RefreshChangeEventList()

{

    if (!ChangeEventBox.IsValid())

    {

        return;

    }



    float PreviousEventScrollOffset = 0.0f;

    bool bShouldAutoScrollToEnd = true;

    if (ChangeEventScrollBox.IsValid())

    {

        PreviousEventScrollOffset = ChangeEventScrollBox->GetScrollOffset();

        const float PreviousScrollEnd = ChangeEventScrollBox->GetScrollOffsetOfEnd();

        constexpr float AutoScrollThreshold = 24.0f;

        bShouldAutoScrollToEnd = PreviousScrollEnd <= AutoScrollThreshold

            || PreviousScrollEnd - PreviousEventScrollOffset <= AutoScrollThreshold;

    }



    ChangeEventBox->ClearChildren();

    if (ComponentTreeView.IsValid())

    {

        ComponentTreeView->RequestTreeRefresh();

    }



    const FString SelectedName = SelectedComponentTreeNode.IsValid() ? SelectedComponentTreeNode->DisplayName : FString(TEXT("Recent Changes"));

    TArray<TSharedPtr<FTrackedStateChangeEvent>> ScopedEvents;

    const bool bHasSelectedNode = SelectedComponentTreeNode.IsValid();

    if (bHasSelectedNode)

    {

        CollectEventsForTreeNode(SelectedComponentTreeNode, ScopedEvents);

    }



    const TArray<TSharedPtr<FTrackedStateChangeEvent>>* Events = bHasSelectedNode ? &ScopedEvents : &RecentStateChangeEvents;



    auto CollectVisibleEvents = [this](const TArray<TSharedPtr<FTrackedStateChangeEvent>>& SourceEvents, TArray<TSharedPtr<FTrackedStateChangeEvent>>& OutVisibleEvents)

    {

        for (const TSharedPtr<FTrackedStateChangeEvent>& Event : SourceEvents)

        {

            if (IsStateChangeEventVisible(Event))

            {

                OutVisibleEvents.Add(Event);

            }

        }

    };



    TArray<TSharedPtr<FTrackedStateChangeEvent>> VisibleEvents;

    if (Events)

    {

        CollectVisibleEvents(*Events, VisibleEvents);

    }




    VisibleEvents.Sort([](const TSharedPtr<FTrackedStateChangeEvent>& Left, const TSharedPtr<FTrackedStateChangeEvent>& Right)

    {

        if (!Left.IsValid() || !Right.IsValid())

        {

            return Right.IsValid();

        }



        return Left->EventId < Right->EventId;

    });



    const FText EventScopeText = bHasSelectedNode

        ? FText::Format(TMLoc::Text(TEXT("{0} changes"), TEXT("")), FText::FromString(SelectedName))

        : TMLoc::Text(TEXT("Recent changes"), TEXT(""));



    ChangeEventBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)

    [

        SNew(STextBlock)

            .Text(EventScopeText)

            .ColorAndOpacity(FLinearColor(0.72f, 0.72f, 0.72f))

    ];



    if (VisibleEvents.Num() == 0)

    {

        const FText EmptyStateText = RecentStateChangeEvents.Num() == 0

            ? TMLoc::Text(TEXT("No component changes captured yet."), TEXT(""))

            : (bHasSelectedNode && ScopedEvents.Num() == 0

                ? TMLoc::Text(TEXT("This selected item has no captured changes."), TEXT(""))

                : TMLoc::Text(TEXT("No changes match the current filters."), TEXT("")));



        ChangeEventBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)

        [

            SNew(STextBlock)

                .Text(EmptyStateText)

                .ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))

        ];

        return;

    }



    if (bShowTraceDetails)

    {

        ChangeEventBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 7.0f)

        [

            BuildChangeOverviewPanel(VisibleEvents)

        ];



        ChangeEventBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)

        [

            BuildEventFlowSummary(VisibleEvents)

        ];

    }



    for (const TSharedPtr<FTrackedStateChangeEvent>& Event : VisibleEvents)

    {

        if (!Event.IsValid())

        {

            continue;

        }



        ChangeEventBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)

        [

            BuildEventCard(Event)

        ];

    }



    if (ChangeEventScrollBox.IsValid())

    {

        if (bShouldAutoScrollToEnd)

        {

            ChangeEventScrollBox->ScrollToEnd();

        }

        else

        {

            ChangeEventScrollBox->SetScrollOffset(PreviousEventScrollOffset);

        }

    }

}



void SInstanceReferenceTracker::RefreshSummary()

{

    if (!SummaryTextBlock.IsValid())

    {

        return;

    }



    int32 VisibleCount = 0;


    for (const TSharedPtr<FTrackedStateChangeEvent>& Event : RecentStateChangeEvents)

    {

        if (IsStateChangeEventVisible(Event))

        {

            ++VisibleCount;

        }



    }



    const FString TargetName = TargetActor.IsValid() ? GetActorLabelSafe(TargetActor.Get()) : InitialActorLabel;

    SummaryTextBlock->SetText(FText::FromString(FString::Printf(

        TEXT("Debug Session | Target=%s | Watch=%s | Visible Events=%d | Hidden Noise=%d | Pinned=%d | Ignored Rules=%d | Referencers=%d | AutoSnapshot=%s"),

        *TargetName,

        *GetWatchRuleLabel(),

        VisibleCount,

        FMath::Max(0, RecentStateChangeEvents.Num() - VisibleCount),

        PinnedNodeKeys.Num(),

        IgnoredStateKeys.Num(),

        PreviousSnapshot.Num(),

        bAutoSaveSnapshots ? TEXT("On") : TEXT("Off"))));

}



void SInstanceReferenceTracker::FocusStateChangeEvent(const TSharedPtr<FTrackedStateChangeEvent>& Event)

{

    if (!Event.IsValid())

    {

        return;

    }



    if (TSharedPtr<FTrackedComponentTreeNode> Node = ComponentTreeNodeByKey.FindRef(Event->NodeKey))

    {

        SelectedComponentTreeNode = Node;

        if (ComponentTreeView.IsValid())

        {

            ComponentTreeView->SetSelection(Node);

            ComponentTreeView->RequestScrollIntoView(Node);

        }



        UObject* Object = Node->Object.Get();

        AActor* ActorToSelect = Cast<AActor>(Object);

        if (!ActorToSelect)

        {

            if (UActorComponent* Component = Cast<UActorComponent>(Object))

            {

                ActorToSelect = Component->GetOwner();

            }

        }



        if (GEditor && ActorToSelect)

        {

            GEditor->SelectNone(false, true);

            GEditor->SelectActor(ActorToSelect, true, true);

            GEditor->NoteSelectionChange();

            AppendLogLine(FString::Printf(TEXT("Selected related actor/component: %s | State=%s"), *GetActorLabelSafe(ActorToSelect), *Event->StateKey), FLinearColor(0.55f, 0.8f, 1.0f));

        }

    }

}



void SInstanceReferenceTracker::OpenStateChangeBlueprint(const TSharedPtr<FTrackedStateChangeEvent>& Event)

{

    if (!Event.IsValid() || !GEditor)

    {

        return;

    }



    UObject* Object = nullptr;

    if (TSharedPtr<FTrackedComponentTreeNode> Node = ComponentTreeNodeByKey.FindRef(Event->NodeKey))

    {

        Object = Node->Object.Get();

    }



    UClass* ObjectClass = nullptr;

    if (const AActor* Actor = Cast<AActor>(Object))

    {

        ObjectClass = Actor->GetClass();

    }

    else if (const UActorComponent* Component = Cast<UActorComponent>(Object))

    {

        ObjectClass = Component->GetClass();

    }

    else if (TargetActor.IsValid())

    {

        ObjectClass = TargetActor->GetClass();

    }



    UBlueprint* Blueprint = ObjectClass ? Cast<UBlueprint>(ObjectClass->ClassGeneratedBy) : nullptr;

    if (!Blueprint)

    {

        AppendLogLine(FString::Printf(TEXT("No Blueprint asset found for state: %s"), *Event->StateKey), FLinearColor(0.75f, 0.75f, 0.75f));

        return;

    }



    if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())

    {

        AssetEditorSubsystem->OpenEditorForAsset(Blueprint);

        AppendLogLine(FString::Printf(TEXT("Opened Blueprint for state: %s | Asset=%s"), *Event->StateKey, *Blueprint->GetName()), FLinearColor(0.55f, 0.8f, 1.0f));

    }

}



void SInstanceReferenceTracker::TogglePinnedNode(const FString& NodeKey)

{

    if (PinnedNodeKeys.Contains(NodeKey))

    {

        PinnedNodeKeys.Remove(NodeKey);

    }

    else

    {

        PinnedNodeKeys.Add(NodeKey);

    }



    if (ComponentTreeView.IsValid())

    {

        ComponentTreeView->RequestTreeRefresh();

    }

    RefreshChangeEventList();

    RefreshSummary();

}



void SInstanceReferenceTracker::IgnoreStateKey(const FString& StateKey)

{

    IgnoredStateKeys.Add(StateKey);

    RefreshChangeEventList();

    RefreshSummary();

    AppendLogLine(FString::Printf(TEXT("Ignored state key: %s"), *StateKey), FLinearColor(0.72f, 0.72f, 0.72f));

}



void SInstanceReferenceTracker::ResetSessionEvents()

{

    CancelReferenceSnapshotScan();

    ChangeEventsByNodeKey.Empty();

    RecentStateChangeEvents.Empty();

    PinnedNodeKeys.Empty();

    IgnoredStateKeys.Empty();

    ExpandedEventIds.Empty();

    NextEventId = 1;

    SessionStartSeconds = FPlatformTime::Seconds();

    PendingHookReason.Empty();

    PendingHookRuntimeExecutionSummary.Empty();

    PendingHookScanCount = 0;

    bPendingHookStateScan = false;

    bNeedsChangeEventListRefresh = false;

    bNeedsSummaryRefresh = false;



    for (const TPair<FString, TSharedPtr<FTrackedComponentTreeNode>>& Pair : ComponentTreeNodeByKey)

    {

        if (Pair.Value.IsValid())

        {

            Pair.Value->ChangeCount = 0;

        }

    }



    if (ComponentTreeView.IsValid())

    {

        ComponentTreeView->RequestTreeRefresh();

    }



    RefreshChangeEventList();

    RefreshSummary();

    AppendLogLine(TEXT("Trace session events reset."), FLinearColor(0.7f, 0.9f, 1.0f));

}



FString SInstanceReferenceTracker::BuildTraceSnapshotMarkdown(const FString& Reason) const

{

    TArray<FString> Lines;

    TArray<TSharedPtr<FTrackedStateChangeEvent>> VisibleSnapshotEvents;
    for (const TSharedPtr<FTrackedStateChangeEvent>& Event : RecentStateChangeEvents)
    {
        if (IsStateChangeEventVisible(Event))
        {
            VisibleSnapshotEvents.Add(Event);
        }
    }

    Lines.Add(FString::Printf(TEXT("# TraceMotive — Debug Pathfinder for Unreal Trace Snapshot - %s"), *Reason));

    Lines.Add(FString::Printf(TEXT("- Target: %s"), TargetActor.IsValid() ? *GetActorLabelSafe(TargetActor.Get()) : *InitialActorLabel));

    Lines.Add(FString::Printf(TEXT("- Class: %s"), TargetActor.IsValid() ? *GetActorClassNameSafe(TargetActor.Get()) : TEXT("<unknown>")));

    Lines.Add(FString::Printf(TEXT("- Time: %s"), *FDateTime::Now().ToString()));

    Lines.Add(FString::Printf(TEXT("- Visible Events: %d"), VisibleSnapshotEvents.Num()));

    Lines.Add(FString::Printf(TEXT("- Referencers: %d"), PreviousSnapshot.Num()));

    Lines.Add(FString::Printf(TEXT("- Pinned Nodes: %d"), PinnedNodeKeys.Num()));

    Lines.Add(FString());

    Lines.Add(TEXT("## Recent Changes"));

    Lines.Add(TEXT("| Count | Category | Action | Impact | Source | State | Reason |"));

    Lines.Add(TEXT("|---:|---|---|---|---|---|---|"));



    const int32 StartIndex = FMath::Max(0, VisibleSnapshotEvents.Num() - 200);

    for (int32 Index = StartIndex; Index < VisibleSnapshotEvents.Num(); ++Index)

    {

        const TSharedPtr<FTrackedStateChangeEvent>& Event = VisibleSnapshotEvents[Index];

        if (!Event.IsValid())

        {

            continue;

        }



        Lines.Add(FString::Printf(TEXT("| x%d | %s | %s | %s | %s | %s | %s |"),

            Event->RepeatCount,

            *Event->Category.Replace(TEXT("|"), TEXT("/")),

            *Event->ActionSummary.Replace(TEXT("|"), TEXT("/")),

            *Event->ActionImpact.Replace(TEXT("|"), TEXT("/")),

            *Event->ActionSource.Replace(TEXT("|"), TEXT("/")),

            *Event->StateKey.Replace(TEXT("|"), TEXT("/")),

            *Event->Reason.Replace(TEXT("|"), TEXT("/"))));

    }



    Lines.Add(FString());

    Lines.Add(TEXT("## Diagnostics"));

    for (const TSharedPtr<FTrackedStateChangeEvent>& Event : VisibleSnapshotEvents)

    {

        if (Event.IsValid())

        {

            Lines.Add(FString::Printf(TEXT("- `x%d %s`: %s | %s"), Event->RepeatCount, *Event->StateKey, *Event->ActionSummary, *Event->Diagnostic));

        }

    }



    Lines.Add(FString());

    Lines.Add(TEXT("## Reference Confidence Graph"));

    for (const TPair<TWeakObjectPtr<AActor>, TSet<FString>>& Pair : PreviousSnapshot)

    {

        AActor* Actor = Pair.Key.Get();

        if (!Actor)

        {

            continue;

        }



        Lines.Add(FString::Printf(TEXT("- %s (%s): %d reference path(s)"),

            *GetActorLabelSafe(Actor),

            *GetActorClassNameSafe(Actor),

            Pair.Value.Num()));

    }



    return FString::Join(Lines, TEXT("\n"));

}



void SInstanceReferenceTracker::SaveTraceSnapshot(const FString& Reason) const

{

    const FString SnapshotDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TraceMotive"), TEXT("TraceSnapshots"));

    IFileManager::Get().MakeDirectory(*SnapshotDirectory, true);



    const FString TargetToken = SanitizeSnapshotToken(TargetActor.IsValid() ? GetActorLabelSafe(TargetActor.Get()) : InitialActorLabel);

    const FString FileName = FString::Printf(TEXT("%s_%s_%s.md"),

        *SanitizeSnapshotToken(Reason),

        *TargetToken,

        *SanitizeSnapshotToken(FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));

    const FString FilePath = FPaths::Combine(SnapshotDirectory, FileName);

    const FString Markdown = BuildTraceSnapshotMarkdown(Reason);



    if (FFileHelper::SaveStringToFile(Markdown, *FilePath))

    {

        const_cast<SInstanceReferenceTracker*>(this)->AppendLogLine(FString::Printf(TEXT("Trace snapshot saved: %s"), *FilePath), FLinearColor(0.55f, 0.9f, 0.65f));

    }

    else

    {

        const_cast<SInstanceReferenceTracker*>(this)->AppendLogLine(FString::Printf(TEXT("Failed to save trace snapshot: %s"), *FilePath), FLinearColor(1.0f, 0.45f, 0.35f));

    }

}



void SInstanceReferenceTracker::RefreshCurrentReferencerList(const TMap<TWeakObjectPtr<AActor>, TSet<FString>>& Snapshot)

{

    if (!CurrentReferencerBox.IsValid())

    {

        return;

    }



    TArray<FInstanceReferenceDisplayRow> Rows;

    TArray<FString> SignatureLines;

    Rows.Reserve(Snapshot.Num());

    SignatureLines.Reserve(Snapshot.Num());



    for (const TPair<TWeakObjectPtr<AActor>, TSet<FString>>& Pair : Snapshot)

    {

        FInstanceReferenceDisplayRow Row;

        Row.Actor = Pair.Key;

        Row.InstanceName = GetActorLabelSafe(Pair.Key.Get());

        Row.ClassName = GetActorClassNameSafe(Pair.Key.Get());

        Row.PropertyPaths = Pair.Value.Array();

        Row.PropertyPaths.Sort();



        SignatureLines.Add(FString::Printf(TEXT("%s|%s|%s"), *Row.InstanceName, *Row.ClassName, *FString::Join(Row.PropertyPaths, TEXT(","))));

        Rows.Add(MoveTemp(Row));

    }



    SignatureLines.Sort();

    const FString NewSignature = FString::Join(SignatureLines, TEXT("\n"));

    if (NewSignature == LastReferencerListSignature)

    {

        return;

    }



    LastReferencerListSignature = NewSignature;

    CurrentReferencerBox->ClearChildren();



    if (Snapshot.Num() == 0)

    {

        CurrentReferencerBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)

        [

            SNew(STextBlock)

                .Text(TMLoc::Text(TEXT("No live actor references found."), TEXT("No live actor references found.")))

                .ColorAndOpacity(FLinearColor(0.62f, 0.62f, 0.62f))

        ];

        return;

    }



    Rows.Sort([](const FInstanceReferenceDisplayRow& Left, const FInstanceReferenceDisplayRow& Right)

    {

        return Left.InstanceName < Right.InstanceName;

    });



    const int32 RowsToRender = FMath::Min(Rows.Num(), MaxReferencerRowsToRender);

    for (int32 RowIndex = 0; RowIndex < RowsToRender; ++RowIndex)

    {

        const FInstanceReferenceDisplayRow& Row = Rows[RowIndex];

        TArray<FString> PropertySummaries;

        PropertySummaries.Reserve(FMath::Min(Row.PropertyPaths.Num(), MaxReferencerPathsToDescribe));



        const int32 PathsToDescribe = FMath::Min(Row.PropertyPaths.Num(), MaxReferencerPathsToDescribe);

        for (int32 PathIndex = 0; PathIndex < PathsToDescribe; ++PathIndex)

        {

            const FString& PropertyPath = Row.PropertyPaths[PathIndex];

            if (PropertyPath.StartsWith(TEXT("BlueprintGraph:")))

            {

                PropertySummaries.Add(PropertyPath);

                continue;

            }



            const FString ActionSummary = BuildActionSummary(Row.Actor.Get(), PropertyPath);

            PropertySummaries.Add(ActionSummary.IsEmpty()

                ? FString::Printf(TEXT("%s -> Action=<not found in Blueprint graph>"), *PropertyPath)

                : FString::Printf(TEXT("%s -> %s"), *PropertyPath, *ActionSummary));

        }



        if (Row.PropertyPaths.Num() > PathsToDescribe)

        {

            PropertySummaries.Add(FString::Printf(TEXT("... %d more refs hidden for UI safety"), Row.PropertyPaths.Num() - PathsToDescribe));

        }



        CurrentReferencerBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)

        [

            SNew(STextBlock)

                .Text(FText::FromString(FString::Printf(TEXT("Actor=%s | Class=%s | Ref=%s"), *Row.InstanceName, *Row.ClassName, *FString::Join(PropertySummaries, TEXT(" ; ")))))

                .AutoWrapText(true)

        ];

    }



    if (Rows.Num() > RowsToRender)

    {

        CurrentReferencerBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)

        [

            SNew(STextBlock)

                .Text(FText::FromString(FString::Printf(TEXT("... %d more referencing actors hidden for UI safety."), Rows.Num() - RowsToRender)))

                .ColorAndOpacity(FLinearColor(0.82f, 0.68f, 0.42f))

        ];

    }

}



FString SInstanceReferenceTracker::BuildDeduplicatedRawLogText() const

{

    TArray<FString> Lines;


    Lines.Add(TEXT("# TraceMotive Instance Trace Raw Log"));

    Lines.Add(FString::Printf(TEXT("- Target: %s"), TargetActor.IsValid() ? *GetActorLabelSafe(TargetActor.Get()) : *InitialActorLabel));

    Lines.Add(FString::Printf(TEXT("- Generated: %s"), *FDateTime::Now().ToString()));

    Lines.Add(FString::Printf(TEXT("- Unique Lines: %d"), RawLogCopyOrder.Num()));

    Lines.Add(TEXT(""));



    for (const FString& LogLine : RawLogCopyOrder)

    {

        const int32 Count = RawLogCopyCounts.FindRef(LogLine);

        if (Count > 1)

        {

            Lines.Add(FString::Printf(TEXT("%s (x%d)"), *LogLine, Count));

        }

        else

        {

            Lines.Add(LogLine);

        }

    }



    return FString::Join(Lines, TEXT("\n"));

}



void SInstanceReferenceTracker::CopyDeduplicatedRawLogToClipboard() const

{

    FPlatformApplicationMisc::ClipboardCopy(*BuildDeduplicatedRawLogText());

}



TSharedRef<SWidget> SInstanceReferenceTracker::BuildHighlightedLogLine(const FString& Line, const FLinearColor& FallbackColor, const FString& CopyKey) const

{

    TSharedRef<SWrapBox> WrapBox = SNew(SWrapBox)

        .UseAllottedSize(true);



    auto AddTextRun = [&WrapBox](const FString& Text, const FLinearColor& RunColor, const bool bBold)

    {

        if (Text.IsEmpty())

        {

            return;

        }



        WrapBox->AddSlot().Padding(0.0f)

        [

            SNew(STextBlock)

                .Text(FText::FromString(Text))

                .Font(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), 8))

                .ColorAndOpacity(RunColor)

        ];

    };



    const FString HighlightKeys[] =

    {

        TEXT("Actor="),

        TEXT("Class="),

        TEXT("World="),

        TEXT("State="),

        TEXT("Reason="),

        TEXT("Value=")

    };



    int32 ParseCursor = 0;

    while (ParseCursor < Line.Len())

    {

        int32 BestIndex = INDEX_NONE;

        FString BestKey;

        for (const FString& Key : HighlightKeys)

        {

            const int32 FoundIndex = Line.Find(Key, ESearchCase::CaseSensitive, ESearchDir::FromStart, ParseCursor);

            if (FoundIndex != INDEX_NONE && (BestIndex == INDEX_NONE || FoundIndex < BestIndex))

            {

                BestIndex = FoundIndex;

                BestKey = Key;

            }

        }



        if (BestIndex == INDEX_NONE)

        {

            AddTextRun(Line.Mid(ParseCursor), FallbackColor, false);

            break;

        }



        if (BestIndex > ParseCursor)

        {

            AddTextRun(Line.Mid(ParseCursor, BestIndex - ParseCursor), FallbackColor, false);

        }



        AddTextRun(BestKey, FLinearColor(0.95f, 0.72f, 0.34f), true);



        const int32 ValueStart = BestIndex + BestKey.Len();

        int32 ValueEnd = Line.Len();

        const int32 PipeIndex = Line.Find(TEXT("|"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);

        if (PipeIndex != INDEX_NONE)

        {

            ValueEnd = PipeIndex;

        }



        for (const FString& Key : HighlightKeys)

        {

            const int32 NextKeyIndex = Line.Find(Key, ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);

            if (NextKeyIndex != INDEX_NONE && NextKeyIndex > ValueStart)

            {

                ValueEnd = FMath::Min(ValueEnd, NextKeyIndex);

            }

        }



        AddTextRun(Line.Mid(ValueStart, ValueEnd - ValueStart), FLinearColor(0.54f, 0.82f, 1.0f), true);

        ParseCursor = ValueEnd;

    }



    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

        [

            WrapBox

        ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)

        [

            SNew(SBorder)

                .Padding(FMargin(6.0f, 1.0f))

                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                .BorderBackgroundColor(FLinearColor(0.95f, 0.66f, 0.16f, 0.25f))

                .Visibility_Lambda([this, CopyKey]()

                {

                    return RawLogCopyCounts.FindRef(CopyKey) > 1 ? EVisibility::Visible : EVisibility::Collapsed;

                })

                [

                    SNew(STextBlock)

                        .Text_Lambda([this, CopyKey]()

                        {

                            return FText::FromString(FString::Printf(TEXT("x%d"), RawLogCopyCounts.FindRef(CopyKey)));

                        })

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                        .ColorAndOpacity(FLinearColor(1.0f, 0.86f, 0.38f))

                ]

        ];

}



void SInstanceReferenceTracker::AppendLogLine(const FString& Message, const FLinearColor& Color)

{

    if (!LogBox.IsValid())

    {

        return;

    }



    FString CopyKey = Message;

    CopyKey.TrimStartAndEndInline();

    if (CopyKey.IsEmpty())

    {

        CopyKey = TEXT("<empty log line>");

    }



    int32& CopyCount = RawLogCopyCounts.FindOrAdd(CopyKey);

    if (CopyCount == 0)

    {

        RawLogCopyOrder.Add(CopyKey);

    }

    ++CopyCount;



    const FString Timestamp = FDateTime::Now().ToString(TEXT("%H:%M:%S"));

    LastRawLogLine = FString::Printf(TEXT("[%s] %s%s"),

        *Timestamp,

        *Message,

        CopyCount > 1 ? *FString::Printf(TEXT(" (x%d)"), CopyCount) : TEXT(""));

    if (RawLogPreviewText.IsValid())

    {

        RawLogPreviewText->SetText(FText::FromString(LastRawLogLine));

    }



    if (CopyCount > 1)

    {

        LogBox->Invalidate(EInvalidateWidgetReason::Paint);

        return;

    }



    LogBox->AddSlot()

        [

            BuildHighlightedLogLine(LastRawLogLine, Color, CopyKey)

        ];



    LogLineCount++;

    if (LogLineCount > 250)

    {

        LogBox->ClearChildren();

        LogLineCount = 0;

        AppendLogLine(TEXT("Log trimmed after 250 entries."), FLinearColor(0.8f, 0.8f, 0.8f));

    }



    LogBox->ScrollToEnd();

}



void SInstanceReferenceTracker::AddReferenceEvent(const FString& Verb, AActor* Referencer, const FString& PropertyPath, const FLinearColor& Color)

{

    const FString ReferencerName = GetActorLabelSafe(Referencer);

    const FString ReferencerClassName = GetActorClassNameSafe(Referencer);

    const FString ReferenceStateKey = FString::Printf(TEXT("Reference.%s.%s"),

        *ReferencerName,

        *GetRootPropertyName(PropertyPath));

    AddStateChangeEvent(ReferenceStateKey,

        Verb == TEXT("Added") ? TEXT("None") : PropertyPath,

        Verb == TEXT("Added") ? PropertyPath : TEXT("None"),

        FString::Printf(TEXT("Reference%s"), *Verb),

        FString::Printf(TEXT("Actor=%s | Class=%s | Ref=%s"), *ReferencerName, *ReferencerClassName, *PropertyPath),

        Color);



    if (PropertyPath.StartsWith(TEXT("BlueprintGraph:")))

    {

        AppendLogLine(FString::Printf(TEXT("%s blueprint reference/action: Actor=%s | Class=%s | %s"),

            *Verb,

            *ReferencerName,

            *ReferencerClassName,

            *PropertyPath), Color);

        return;

    }



    const FString ActionSummary = BuildActionSummary(Referencer, PropertyPath);

    AppendLogLine(FString::Printf(TEXT("%s reference: Actor=%s | Class=%s | Ref=%s | %s"),

        *Verb,

        *ReferencerName,

        *ReferencerClassName,

        *PropertyPath,

        ActionSummary.IsEmpty() ? TEXT("Action=<not found in Blueprint graph>") : *ActionSummary), Color);

}



FString SInstanceReferenceTracker::BuildActionSummary(AActor* Referencer, const FString& PropertyPath) const

{

    if (PropertyPath.StartsWith(TEXT("BlueprintGraph:")))

    {

        return PropertyPath;

    }



    const FString CacheKey = FString::Printf(TEXT("%llu|%s"), static_cast<uint64>((UPTRINT)Referencer), *PropertyPath);

    if (const FString* CachedSummary = ActionSummaryCache.Find(CacheKey))

    {

        return *CachedSummary;

    }



    TSet<FString> Actions;

    FindBlueprintActionsForProperty(Referencer, PropertyPath, Actions);

    FindBlueprintVisibilityActionsForProperty(Referencer, PropertyPath, Actions);



    if (Actions.Num() == 0)

    {

        ActionSummaryCache.Add(CacheKey, FString());

        return FString();

    }



    TArray<FString> SortedActions = Actions.Array();

    SortedActions.Sort([](const FString& Left, const FString& Right)

    {

        const bool bLeftAction = Left.Contains(TEXT("| Action="));

        const bool bRightAction = Right.Contains(TEXT("| Action="));

        if (bLeftAction != bRightAction)

        {

            return bLeftAction;

        }



        return Left < Right;

    });



    const int32 MaxActionsToShow = 5;

    if (SortedActions.Num() > MaxActionsToShow)

    {

        SortedActions.SetNum(MaxActionsToShow);

        SortedActions.Add(TEXT("..."));

    }



    const FString Result = FString::Join(SortedActions, TEXT(", "));

    ActionSummaryCache.Add(CacheKey, Result);

    return Result;

}



FString SInstanceReferenceTracker::BuildStateChangeCallerSummary(const FString& StateKey) const

{

    const FString CacheKey = FString::Printf(TEXT("%s|%s"), *LastReferencerListSignature, *StateKey);

    if (const FString* CachedSummary = StateCallerSummaryCache.Find(CacheKey))

    {

        return *CachedSummary;

    }



    TArray<FString> MatchedCallers;

    TArray<FString> ReferencingActors;



    TSet<FString> SelfActions;

    FindBlueprintSelfActionsForState(TargetActor.Get(), StateKey, SelfActions);

    for (const FString& SelfAction : SelfActions)

    {

        MatchedCallers.Add(SelfAction);

    }



    for (const TPair<TWeakObjectPtr<AActor>, TSet<FString>>& Pair : PreviousSnapshot)

    {

        AActor* Referencer = Pair.Key.Get();

        if (!Referencer)

        {

            continue;

        }



        const FString ReferencerPrefix = FString::Printf(TEXT("Actor=%s | Class=%s"),

            *GetActorLabelSafe(Referencer),

            *GetActorClassNameSafe(Referencer));

        const FString ReferencerOnlyPrefix = FString::Printf(TEXT("ReferencerActor=%s | ReferencerClass=%s"),

            *GetActorLabelSafe(Referencer),

            *GetActorClassNameSafe(Referencer));



        ReferencingActors.Add(ReferencerOnlyPrefix);



        for (const FString& PropertyPath : Pair.Value)

        {

            const FString ActionSummary = BuildActionSummary(Referencer, PropertyPath);

            if (IsStateActionMatch(StateKey, ActionSummary))

            {

                if (PropertyPath.StartsWith(TEXT("BlueprintGraph:")))

                {

                    MatchedCallers.Add(FString::Printf(TEXT("%s | %s"), *ReferencerPrefix, *PropertyPath));

                }

                else

                {

                    MatchedCallers.Add(FString::Printf(TEXT("%s | Ref=%s | %s"), *ReferencerPrefix, *PropertyPath, *ActionSummary));

                }

            }

        }

    }



    MatchedCallers.Sort();

    if (MatchedCallers.Num() > 0)

    {

        const int32 MaxCallersToShow = 3;

        if (MatchedCallers.Num() > MaxCallersToShow)

        {

            MatchedCallers.SetNum(MaxCallersToShow);

            MatchedCallers.Add(TEXT("..."));

        }



        const FString Result = FString::Join(MatchedCallers, TEXT(" ; "));

        StateCallerSummaryCache.Add(CacheKey, Result);

        return Result;

    }



    ReferencingActors.Sort();

    if (ReferencingActors.Num() > 0)

    {

        const int32 MaxActorsToShow = 3;

        if (ReferencingActors.Num() > MaxActorsToShow)

        {

            ReferencingActors.SetNum(MaxActorsToShow);

            ReferencingActors.Add(TEXT("..."));

        }



        const FString Result = FString::Printf(TEXT("No matching action node found; current referencers: %s"), *FString::Join(ReferencingActors, TEXT(" ; ")));

        StateCallerSummaryCache.Add(CacheKey, Result);

        return Result;

    }



    StateCallerSummaryCache.Add(CacheKey, FString());

    return FString();

}



FString SInstanceReferenceTracker::BuildWatchRuleVisibilityCallerSummary(const FString& StateKey, const FString& NewValue, const FString& CallerSummary) const

{

    const bool bVisibilityOff = IsVisibilityOffValue(StateKey, NewValue);

    const FString WatchToken = bVisibilityOff ? TEXT("VisibilityOff") : TEXT("VisibilityOn");

    AActor* Target = TargetActor.Get();



    TSet<FString> CandidateSet;

    auto AddCandidate = [&CandidateSet](const FString& Candidate)

    {

        FString CleanCandidate = Candidate;

        CleanCandidate.TrimStartAndEndInline();

        if (!CleanCandidate.IsEmpty() && CleanCandidate != TEXT("..."))

        {

            CandidateSet.Add(CleanCandidate);

        }

    };



    AddCandidate(CallerSummary);



    TSet<FString> SelfActions;

    FindBlueprintSelfActionsForState(Target, StateKey, SelfActions);

    for (const FString& SelfAction : SelfActions)

    {

        AddCandidate(SelfAction);

    }



    for (const TPair<TWeakObjectPtr<AActor>, TSet<FString>>& Pair : PreviousSnapshot)

    {

        AActor* Referencer = Pair.Key.Get();

        if (!Referencer)

        {

            continue;

        }



        const FString ReferencerPrefix = FString::Printf(TEXT("Actor=%s | Class=%s"),

            *GetActorLabelSafe(Referencer),

            *GetActorClassNameSafe(Referencer));



        for (const FString& PropertyPath : Pair.Value)

        {

            const FString ActionSummary = BuildActionSummary(Referencer, PropertyPath);

            if (PropertyPath.StartsWith(TEXT("BlueprintGraph:")) && IsStateActionMatch(StateKey, PropertyPath))

            {

                AddCandidate(FString::Printf(TEXT("%s | %s"), *ReferencerPrefix, *PropertyPath));

            }

            else if (IsStateActionMatch(StateKey, ActionSummary))

            {

                AddCandidate(FString::Printf(TEXT("%s | Ref=%s | %s"), *ReferencerPrefix, *PropertyPath, *ActionSummary));

            }

        }

    }



    if (Target)

    {

        if (UWorld* World = Target->GetWorld())

        {

            for (TActorIterator<AActor> It(World); It; ++It)

            {

                AActor* SourceActor = *It;

                if (!SourceActor)

                {

                    continue;

                }



                TSet<FString> GraphPropertyPaths;

                ScanBlueprintGraphForTargetReferences(SourceActor, Target, GraphPropertyPaths);

                if (GraphPropertyPaths.Num() == 0)

                {

                    continue;

                }



                const FString SourcePrefix = FString::Printf(TEXT("Actor=%s | Class=%s"),

                    *GetActorLabelSafe(SourceActor),

                    *GetActorClassNameSafe(SourceActor));



                for (const FString& GraphPropertyPath : GraphPropertyPaths)

                {

                    if (IsStateActionMatch(StateKey, GraphPropertyPath))

                    {

                        AddCandidate(FString::Printf(TEXT("%s | %s"), *SourcePrefix, *GraphPropertyPath));

                    }

                }

            }

        }

    }



    TArray<FString> Candidates = CandidateSet.Array();

    Candidates.Sort();



    constexpr int32 MaxVisibilityWatchCandidates = 6;

    if (Candidates.Num() > MaxVisibilityWatchCandidates)

    {

        Candidates.SetNum(MaxVisibilityWatchCandidates);

        Candidates.Add(TEXT("..."));

    }



    if (Candidates.Num() == 0)

    {

        return FString::Printf(TEXT("WatchRule=%s | Source=<not found>"), *WatchToken);

    }



    return FString::Printf(TEXT("WatchRule=%s | %s"), *WatchToken, *FString::Join(Candidates, TEXT(" ; ")));

}



FString SInstanceReferenceTracker::BuildCurrentBlueprintExecutionSummary(const UObject* ChangedObject, const FString& Reason) const

{

#if DO_BLUEPRINT_GUARD

    if (const FBlueprintContextTracker* ContextTracker = FBlueprintContextTracker::TryGet())

    {

        const FString ActiveSummary = BuildBlueprintExecutionSummaryFromContext(ContextTracker, nullptr, nullptr, ChangedObject, Reason);

        if (!ActiveSummary.IsEmpty())

        {

            return ActiveSummary;

        }

    }

#endif



    const FString RecentSummary = FindRecentBlueprintExecutionSummary();

    if (!RecentSummary.IsEmpty())

    {

        return RecentSummary + FString::Printf(TEXT(" | ChangedObject=%s | HookReason=%s | Match=RecentScriptEntry"),

            ChangedObject ? *ChangedObject->GetName() : TEXT("<unknown changed object>"),

            Reason.IsEmpty() ? TEXT("<unknown reason>") : *Reason);

    }



    return FString();

}



FString SInstanceReferenceTracker::FindRecentBlueprintExecutionSummary() const

{

    const double NowSeconds = FPlatformTime::Seconds();

    constexpr double RecentExecutionWindowSeconds = 0.35;



    for (int32 Index = RecentBlueprintExecutionSources.Num() - 1; Index >= 0; --Index)

    {

        const FRecentBlueprintExecutionSource& RecentSource = RecentBlueprintExecutionSources[Index];

        if (NowSeconds - RecentSource.TimestampSeconds <= RecentExecutionWindowSeconds)

        {

            return RecentSource.Summary;

        }

    }



    return FString();

}



FString SInstanceReferenceTracker::MergeRuntimeExecutionSummary(const FString& RuntimeExecutionSummary, const FString& CallerSummary) const

{

    if (RuntimeExecutionSummary.IsEmpty())

    {

        return CallerSummary;

    }



    if (CallerSummary.IsEmpty() || IsUnresolvedCallerSummary(CallerSummary))

    {

        return RuntimeExecutionSummary;

    }



    if (CallerSummary.Contains(RuntimeExecutionSummary))

    {

        return CallerSummary;

    }



    return RuntimeExecutionSummary + TEXT(" ; ") + CallerSummary;

}



void SInstanceReferenceTracker::ScanBlueprintGraphForTargetReferences(AActor* SourceActor, AActor* Target, TSet<FString>& OutPropertyPaths, double DeadlineSeconds) const

{

    if (!SourceActor || !Target || !SourceActor->GetClass())

    {

        return;

    }



    UBlueprint* Blueprint = Cast<UBlueprint>(SourceActor->GetClass()->ClassGeneratedBy);

    if (!Blueprint)

    {

        return;

    }



    UClass* InitialClass = InitialActorClass.Get();

    const FGuid LocalInitialActorGuid = InitialActorGuid;

    const FGuid LocalInitialActorInstanceGuid = InitialActorInstanceGuid;

    TArray<UEdGraph*> Graphs;

    Blueprint->GetAllGraphs(Graphs);



    int32 ScannedNodeCount = 0;

    for (UEdGraph* Graph : Graphs)

    {

        if (!Graph)

        {

            continue;

        }



        for (UEdGraphNode* Node : Graph->Nodes)

        {

            if (!Node || OutPropertyPaths.Num() >= MaxReferencePathsPerActor)

            {

                return;

            }



            ++ScannedNodeCount;

            if (ScannedNodeCount > MaxBlueprintGraphNodesPerActor

                || (DeadlineSeconds > 0.0 && FPlatformTime::Seconds() >= DeadlineSeconds))

            {

                return;

            }



            if (UK2Node_Literal* LiteralNode = Cast<UK2Node_Literal>(Node))

            {

                if (DoesObjectReferenceMatchTraceTarget(LiteralNode->GetObjectRef(), Target, InitialActorName, InitialActorLabel, LocalInitialActorGuid, LocalInitialActorInstanceGuid, InitialClass))

                {

                    OutPropertyPaths.Add(FString::Printf(TEXT("BlueprintGraph:%s | Ref=LiteralActor"), *Graph->GetName()));

                }

            }



            if (DoesNodeReferenceTraceTarget(Node, Target, InitialActorName, InitialActorLabel, LocalInitialActorGuid, LocalInitialActorInstanceGuid, InitialClass))

            {

                OutPropertyPaths.Add(FString::Printf(TEXT("BlueprintGraph:%s | Ref=%s"),

                    *Graph->GetName(),

                    *Node->GetNodeTitle(ENodeTitleType::ListView).ToString()));

            }



            UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);

            if (!CallNode)

            {

                continue;

            }



            const FName FunctionName = CallNode->GetFunctionName();

            if (DoesActorMatchTraceTarget(SourceActor, Target, InitialActorName, InitialActorLabel, LocalInitialActorGuid, LocalInitialActorInstanceGuid, InitialClass)

                && IsCallTargetingSelfOrImplicitSelf(CallNode)

                && IsReferenceActionCall(FunctionName))

            {

                OutPropertyPaths.Add(FString::Printf(TEXT("BlueprintGraph:%s | Ref=Self | SelfAction=%s"),

                    *Graph->GetName(),

                    *GetActionNodeName(CallNode)));

                continue;

            }



            for (UEdGraphPin* Pin : CallNode->Pins)

            {

                if (!Pin)

                {

                    continue;

                }



                if (DoesPinReferenceTraceTarget(Pin, Target, InitialActorName, InitialActorLabel, LocalInitialActorGuid, LocalInitialActorInstanceGuid, InitialClass))

                {

                    OutPropertyPaths.Add(FString::Printf(TEXT("BlueprintGraph:%s | %s=%s | Pin=%s"),

                        *Graph->GetName(),

                        IsReferenceActionCall(FunctionName) ? TEXT("Action") : TEXT("Call"),

                        *GetActionNodeName(CallNode),

                        *Pin->PinName.ToString()));

                    break;

                }

            }

        }

    }

}



void SInstanceReferenceTracker::ScanObjectForTargetReferences(UObject* SourceObject, AActor* TargetActor, TSet<FString>& OutPropertyPaths)

{

    if (!SourceObject || !TargetActor)

    {

        return;

    }



    ScanStructForTargetReferences(SourceObject->GetClass(), SourceObject, TargetActor, FString(), OutPropertyPaths);

}



void SInstanceReferenceTracker::ScanStructForTargetReferences(UStruct* StructType, const void* ContainerPtr, AActor* TargetActor, const FString& PathPrefix, TSet<FString>& OutPropertyPaths, int32 Depth)

{

    if (!StructType || !ContainerPtr || !TargetActor || Depth > MaxReferenceStructDepth || OutPropertyPaths.Num() >= MaxReferencePathsPerActor)

    {

        return;

    }



    for (TFieldIterator<FProperty> It(StructType); It; ++It)

    {

        FProperty* Property = *It;

        if (!Property)

        {

            continue;

        }



        if (!CanPropertyContainTraceReference(Property))

        {

            continue;

        }



        const FString PropertyPath = PathPrefix.IsEmpty()

            ? Property->GetName()

            : FString::Printf(TEXT("%s.%s"), *PathPrefix, *Property->GetName());



        for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)

        {

            if (OutPropertyPaths.Num() >= MaxReferencePathsPerActor)

            {

                return;

            }



            const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr, ArrayIndex);

            const FString IndexedPath = Property->ArrayDim > 1

                ? FString::Printf(TEXT("%s[%d]"), *PropertyPath, ArrayIndex)

                : PropertyPath;



            ScanPropertyForTargetReferences(Property, ValuePtr, TargetActor, IndexedPath, OutPropertyPaths, Depth);

        }

    }

}



void SInstanceReferenceTracker::ScanPropertyForTargetReferences(FProperty* Property, const void* ValuePtr, AActor* TargetActor, const FString& PropertyPath, TSet<FString>& OutPropertyPaths, int32 Depth)

{

    if (!Property || !ValuePtr || !TargetActor || Depth > MaxReferenceStructDepth || OutPropertyPaths.Num() >= MaxReferencePathsPerActor)

    {

        return;

    }



    if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))

    {

        UObject* ReferencedObject = ObjectProperty->GetObjectPropertyValue(ValuePtr);

        if (ReferencedObject == TargetActor)

        {

            OutPropertyPaths.Add(PropertyPath);

        }

        return;

    }



    if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))

    {

        ScanStructForTargetReferences(StructProperty->Struct, ValuePtr, TargetActor, PropertyPath, OutPropertyPaths, Depth + 1);

        return;

    }



    if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))

    {

        FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);

        const int32 ElementCount = FMath::Min(ArrayHelper.Num(), MaxReferenceContainerElements);

        for (int32 Index = 0; Index < ElementCount; ++Index)

        {

            if (OutPropertyPaths.Num() >= MaxReferencePathsPerActor)

            {

                return;

            }



            ScanPropertyForTargetReferences(ArrayProperty->Inner, ArrayHelper.GetRawPtr(Index), TargetActor,

                FString::Printf(TEXT("%s[%d]"), *PropertyPath, Index), OutPropertyPaths, Depth + 1);

        }

        return;

    }



    if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))

    {

        FScriptSetHelper SetHelper(SetProperty, ValuePtr);

        int32 LogicalIndex = 0;

        for (int32 Index = 0; Index < SetHelper.GetMaxIndex(); ++Index)

        {

            if (!SetHelper.IsValidIndex(Index))

            {

                continue;

            }



            if (LogicalIndex >= MaxReferenceContainerElements || OutPropertyPaths.Num() >= MaxReferencePathsPerActor)

            {

                return;

            }



            ScanPropertyForTargetReferences(SetProperty->ElementProp, SetHelper.GetElementPtr(Index), TargetActor,

                FString::Printf(TEXT("%s{%d}"), *PropertyPath, LogicalIndex++), OutPropertyPaths, Depth + 1);

        }

        return;

    }



    if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))

    {

        FScriptMapHelper MapHelper(MapProperty, ValuePtr);

        int32 LogicalIndex = 0;

        for (int32 Index = 0; Index < MapHelper.GetMaxIndex(); ++Index)

        {

            if (!MapHelper.IsValidIndex(Index))

            {

                continue;

            }



            if (LogicalIndex >= MaxReferenceContainerElements || OutPropertyPaths.Num() >= MaxReferencePathsPerActor)

            {

                return;

            }



            const FString EntryPath = FString::Printf(TEXT("%s{%d}"), *PropertyPath, LogicalIndex++);

            ScanPropertyForTargetReferences(MapProperty->KeyProp, MapHelper.GetKeyPtr(Index), TargetActor, EntryPath + TEXT(".Key"), OutPropertyPaths, Depth + 1);

            ScanPropertyForTargetReferences(MapProperty->ValueProp, MapHelper.GetValuePtr(Index), TargetActor, EntryPath + TEXT(".Value"), OutPropertyPaths, Depth + 1);

        }

    }

}



TMap<FString, FString> SInstanceReferenceTracker::CaptureTargetState(AActor* Target)

{

    TMap<FString, FString> State;

    if (!Target)

    {

        return State;

    }



    State.Add(TEXT("Actor.HiddenInGame"), BoolToText(Target->IsHidden()));

    State.Add(TEXT("Actor.EnableCollision"), BoolToText(Target->GetActorEnableCollision()));



    auto AddComponentState = [&State](const UActorComponent* Component, const FString& ComponentPrefix)

    {

        if (!Component)

        {

            return;

        }



        const bool bNoisyHelperComponent = IsNoisyInstanceTraceComponentClass(Component->GetClass());

        const USceneComponent* SceneComponent = Cast<USceneComponent>(Component);



        if (!bNoisyHelperComponent)

        {

            State.Add(ComponentPrefix + TEXT(".Exists"), TEXT("true"));

            State.Add(ComponentPrefix + TEXT(".Class"), Component->GetClass() ? Component->GetClass()->GetName() : TEXT("<Invalid Class>"));

            State.Add(ComponentPrefix + TEXT(".Active"), BoolToText(Component->IsActive()));

            State.Add(ComponentPrefix + TEXT(".Registered"), BoolToText(Component->IsRegistered()));

            CaptureInterestingComponentProperties(Component, ComponentPrefix, State);

        }



        if (!SceneComponent)

        {

            return;

        }



        if (!bNoisyHelperComponent)

        {

            State.Add(ComponentPrefix + TEXT(".VisibleFlag"), BoolToText(SceneComponent->GetVisibleFlag()));

            State.Add(ComponentPrefix + TEXT(".IsVisible"), BoolToText(SceneComponent->IsVisible()));

            State.Add(ComponentPrefix + TEXT(".HiddenInGame"), BoolToText(SceneComponent->bHiddenInGame));

        }



        State.Add(ComponentPrefix + TEXT(".CollisionEnabled"), FString::FromInt(static_cast<int32>(SceneComponent->GetCollisionEnabled())));



        const UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(SceneComponent);

        if (!PrimitiveComponent)

        {

            return;

        }



        State.Add(ComponentPrefix + TEXT(".CollisionProfile"), PrimitiveComponent->GetCollisionProfileName().ToString());

        State.Add(ComponentPrefix + TEXT(".CollisionObjectType"), FString::FromInt(static_cast<int32>(PrimitiveComponent->GetCollisionObjectType())));

        for (int32 ChannelIndex = 0; ChannelIndex < static_cast<int32>(ECC_MAX); ++ChannelIndex)

        {

            const ECollisionChannel Channel = static_cast<ECollisionChannel>(ChannelIndex);

            const FString ChannelName = GetTraceCollisionChannelText(Channel);

            if (!ChannelName.IsEmpty())

            {

                State.Add(ComponentPrefix + TEXT(".CollisionResponse.") + ChannelName, GetTraceCollisionResponseText(PrimitiveComponent->GetCollisionResponseToChannel(Channel)));

            }

        }

        State.Add(ComponentPrefix + TEXT(".GenerateOverlapEvents"), BoolToText(PrimitiveComponent->GetGenerateOverlapEvents()));

        State.Add(ComponentPrefix + TEXT(".SimulatingPhysics"), BoolToText(PrimitiveComponent->IsSimulatingPhysics()));

    };



    TInlineComponentArray<UActorComponent*> Components(Target);

    Components.Sort([](const UActorComponent& Left, const UActorComponent& Right)

    {

        const FString LeftKey = FString::Printf(TEXT("%s.%s"), *Left.GetName(), *Left.GetClass()->GetName());

        const FString RightKey = FString::Printf(TEXT("%s.%s"), *Right.GetName(), *Right.GetClass()->GetName());

        return LeftKey < RightKey;

    });



    for (const UActorComponent* Component : Components)

    {

        if (!Component)

        {

            continue;

        }



        AddComponentState(Component, FString::Printf(TEXT("Component.%s"), *Component->GetName()));

    }



    TSet<const AActor*> CapturedRelatedActors;

    TFunction<void(const AActor*, const FString&, int32)> AddRelatedActorState;

    AddRelatedActorState = [&State, &AddComponentState, &CapturedRelatedActors, &AddRelatedActorState](const AActor* RelatedActor, const FString& RelatedPrefix, const int32 Depth)

    {

        if (!RelatedActor || Depth > 4 || CapturedRelatedActors.Contains(RelatedActor))

        {

            return;

        }



        CapturedRelatedActors.Add(RelatedActor);

        State.Add(RelatedPrefix + TEXT(".Exists"), TEXT("true"));

        State.Add(RelatedPrefix + TEXT(".Class"), RelatedActor->GetClass() ? RelatedActor->GetClass()->GetName() : TEXT("<Invalid Class>"));

        State.Add(RelatedPrefix + TEXT(".HiddenInGame"), BoolToText(RelatedActor->IsHidden()));

        State.Add(RelatedPrefix + TEXT(".EnableCollision"), BoolToText(RelatedActor->GetActorEnableCollision()));



        TInlineComponentArray<UActorComponent*> RelatedComponents(RelatedActor);

        RelatedComponents.Sort([](const UActorComponent& Left, const UActorComponent& Right)

        {

            const FString LeftKey = FString::Printf(TEXT("%s.%s"), *Left.GetName(), *Left.GetClass()->GetName());

            const FString RightKey = FString::Printf(TEXT("%s.%s"), *Right.GetName(), *Right.GetClass()->GetName());

            return LeftKey < RightKey;

        });



        for (const UActorComponent* RelatedComponent : RelatedComponents)

        {

            AddComponentState(RelatedComponent, FString::Printf(TEXT("%s.Component.%s"), *RelatedPrefix, *RelatedComponent->GetName()));

        }



        TArray<AActor*> AttachedChildren;

        RelatedActor->GetAttachedActors(AttachedChildren);

        AttachedChildren.Sort([](const AActor& Left, const AActor& Right)

        {

            return Left.GetName() < Right.GetName();

        });



        for (const AActor* AttachedChild : AttachedChildren)

        {

            AddRelatedActorState(AttachedChild, FString::Printf(TEXT("%s.AttachedActor.%s"), *RelatedPrefix, *AttachedChild->GetName()), Depth + 1);

        }

    };



    TInlineComponentArray<UChildActorComponent*> ChildActorComponents(Target);

    for (const UChildActorComponent* ChildActorComponent : ChildActorComponents)

    {

        const AActor* ChildActor = ChildActorComponent ? ChildActorComponent->GetChildActor() : nullptr;

        if (ChildActor)

        {

            AddRelatedActorState(ChildActor, FString::Printf(TEXT("ChildActor.%s.%s"), *ChildActorComponent->GetName(), *ChildActor->GetName()), 0);

        }

    }



    TArray<AActor*> AttachedActors;

    Target->GetAttachedActors(AttachedActors);

    AttachedActors.Sort([](const AActor& Left, const AActor& Right)

    {

        return Left.GetName() < Right.GetName();

    });



    for (const AActor* AttachedActor : AttachedActors)

    {

        AddRelatedActorState(AttachedActor, FString::Printf(TEXT("AttachedActor.%s"), *AttachedActor->GetName()), 0);

    }



    return State;

}



FString SInstanceReferenceTracker::GetActorLabelSafe(const AActor* Actor)

{

    if (!Actor)

    {

        return TEXT("<Invalid Actor>");

    }



#if WITH_EDITOR

    return Actor->GetActorLabel();

#else

    return Actor->GetName();

#endif

}



FString SInstanceReferenceTracker::GetActorClassNameSafe(const AActor* Actor)

{

    if (!Actor || !Actor->GetClass())

    {

        return TEXT("<Invalid Class>");

    }



    return Actor->GetClass()->GetName();

}
