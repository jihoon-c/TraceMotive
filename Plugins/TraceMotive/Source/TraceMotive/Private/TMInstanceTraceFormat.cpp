#include "TMInstanceTraceFormat.h"



namespace TMInstanceTraceFormat

{

    FString ClassifyStateChange(const FString& StateKey, const FString& Reason)

    {

        if (StateKey.StartsWith(TEXT("Timeline."), ESearchCase::IgnoreCase) || Reason.StartsWith(TEXT("PIE"), ESearchCase::IgnoreCase))

        {

            return TEXT("Lifecycle");

        }



        if (StateKey.StartsWith(TEXT("Reference.")) || Reason.Contains(TEXT("Reference")))

        {

            return TEXT("Reference");

        }



        if (StateKey.Contains(TEXT("Hidden")) || StateKey.Contains(TEXT("Visible")) || StateKey.Contains(TEXT("Visibility")))

        {

            return TEXT("Visibility");

        }



        if (StateKey.Contains(TEXT("Collision")) || StateKey.Contains(TEXT("Overlap")) || StateKey.Contains(TEXT("Physics")))

        {

            return TEXT("Collision");

        }



        if (StateKey.Contains(TEXT("Location")) || StateKey.Contains(TEXT("Rotation")) || StateKey.Contains(TEXT("Scale")) || StateKey.Contains(TEXT("Attach")))

        {

            return TEXT("Transform");

        }



        if (StateKey.Contains(TEXT("Material")) || StateKey.Contains(TEXT("Opacity")) || StateKey.Contains(TEXT("Alpha")) || StateKey.Contains(TEXT("Dissolve")) || StateKey.Contains(TEXT("Render")))

        {

            return TEXT("Material");

        }



        if (StateKey.Contains(TEXT("Active")) || StateKey.Contains(TEXT("Registered")) || StateKey.Contains(TEXT("Tick")) || StateKey.Contains(TEXT("Exists")))

        {

            return TEXT("Lifecycle");

        }



        return TEXT("Other");

    }



    FString BuildConfidenceText(const FString& Reason, const FString& CallerSummary)

    {

        if (HasRuntimeBlueprintExecutionSource(CallerSummary))

        {

            return TEXT("Runtime Blueprint execution");

        }



        if (CallerSummary.Contains(TEXT("WatchRule=")))

        {

            return TEXT("Watch rule match");

        }



        if (Reason.StartsWith(TEXT("RenderStateDirty")) || Reason.StartsWith(TEXT("PropertyChanged")))

        {

            return TEXT("Confirmed runtime hook");

        }



        if (CallerSummary.Contains(TEXT("SelfAction=")) || CallerSummary.Contains(TEXT("| Action=")))

        {

            return TEXT("Blueprint action match");

        }



        if (CallerSummary.Contains(TEXT("current referencers")))

        {

            return TEXT("Reference holder");

        }



        return TEXT("State polling observation");

    }



    FString ExtractTraceToken(const FString& Source, const FString& Token)

    {

        int32 StartIndex = Source.Find(Token, ESearchCase::IgnoreCase);

        if (StartIndex == INDEX_NONE)

        {

            return FString();

        }



        StartIndex += Token.Len();

        FString Value = Source.Mid(StartIndex).TrimStartAndEnd();



        int32 CutIndex = INDEX_NONE;

        const TCHAR Delimiters[] = { TEXT('|'), TEXT(';'), TEXT(',') };

        for (const TCHAR Delimiter : Delimiters)

        {

            int32 FoundIndex = INDEX_NONE;

            if (Value.FindChar(Delimiter, FoundIndex))

            {

                CutIndex = CutIndex == INDEX_NONE ? FoundIndex : FMath::Min(CutIndex, FoundIndex);

            }

        }



        if (CutIndex != INDEX_NONE)

        {

            Value = Value.Left(CutIndex);

        }



        return Value.TrimStartAndEnd();

    }



    FString ExtractStatePropertyName(const FString& StateKey)

    {

        const FString PropertyMarker = TEXT(".Property.");

        const int32 PropertyMarkerIndex = StateKey.Find(PropertyMarker, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

        if (PropertyMarkerIndex != INDEX_NONE)

        {

            return StateKey.Mid(PropertyMarkerIndex + PropertyMarker.Len());

        }



        int32 LastDotIndex = INDEX_NONE;

        if (StateKey.FindLastChar(TEXT('.'), LastDotIndex))

        {

            return StateKey.Mid(LastDotIndex + 1);

        }



        return StateKey;

    }



    bool IsTraceValueTrue(const FString& Value)

    {

        return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)

            || Value.Equals(TEXT("1"), ESearchCase::IgnoreCase);

    }



    bool IsTraceValueFalse(const FString& Value)

    {

        return Value.Equals(TEXT("false"), ESearchCase::IgnoreCase)

            || Value.Equals(TEXT("0"), ESearchCase::IgnoreCase)

            || Value.Equals(TEXT("None"), ESearchCase::IgnoreCase);

    }



    bool IsVisibilityOffValue(const FString& StateKey, const FString& NewValue)

    {

        if (StateKey.Contains(TEXT("HiddenInGame"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("Hidden"), ESearchCase::IgnoreCase))

        {

            return IsTraceValueTrue(NewValue);

        }



        if (StateKey.Contains(TEXT("IsVisible"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("VisibleFlag"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("Visibility"), ESearchCase::IgnoreCase))

        {

            return IsTraceValueFalse(NewValue);

        }



        return false;

    }



    FString FormatCollisionEnabledValue(const FString& Value)

    {

        if (Value == TEXT("0"))

        {

            return TEXT("NoCollision");

        }

        if (Value == TEXT("1"))

        {

            return TEXT("QueryOnly");

        }

        if (Value == TEXT("2"))

        {

            return TEXT("PhysicsOnly");

        }

        if (Value == TEXT("3"))

        {

            return TEXT("QueryAndPhysics");

        }

        return Value;

    }



    FString FormatTraceValueForState(const FString& StateKey, const FString& Value)

    {

        if (StateKey.Contains(TEXT("CollisionEnabled"), ESearchCase::IgnoreCase))

        {

            return FormatCollisionEnabledValue(Value);

        }

        return Value;

    }



    FString FormatTraceValueChangeForState(const FString& StateKey, const FString& OldValue, const FString& NewValue)

    {

        const FString OldDisplay = FormatTraceValueForState(StateKey, OldValue);

        const FString NewDisplay = FormatTraceValueForState(StateKey, NewValue);

        if (OldValue == TEXT("<new>"))

        {

            return FString::Printf(TEXT("Created with value %s"), *NewDisplay);

        }



        if (NewValue == TEXT("<removed>"))

        {

            return FString::Printf(TEXT("Removed; previous value was %s"), *OldDisplay);

        }



        return FString::Printf(TEXT("%s -> %s"), *OldDisplay, *NewDisplay);

    }



    FString FormatTraceValueChange(const FString& OldValue, const FString& NewValue)

    {

        return FormatTraceValueChangeForState(FString(), OldValue, NewValue);

    }



    FString BuildActionSummaryText(const FString& StateKey, const FString& OldValue, const FString& NewValue, const FString& Reason, const FString& CallerSummary)

    {

        const FString Category = ClassifyStateChange(StateKey, Reason);

        const FString PropertyName = ExtractStatePropertyName(StateKey);



        if (StateKey == TEXT("Timeline.PIE.Start"))

        {

            return TEXT("PIE session started");

        }



        if (StateKey == TEXT("Timeline.PIE.Ready"))

        {

            return TEXT("PIE runtime baseline captured");

        }



        if (StateKey == TEXT("Timeline.PIE.End"))

        {

            return TEXT("PIE session ended");

        }



        if (Category == TEXT("Reference"))

        {

            if (NewValue.Equals(TEXT("None"), ESearchCase::IgnoreCase) || NewValue == TEXT("<removed>"))

            {

                return TEXT("Reference removed from tracked target");

            }



            if (OldValue.Equals(TEXT("None"), ESearchCase::IgnoreCase) || OldValue == TEXT("<new>"))

            {

                return TEXT("Reference added to tracked target");

            }



            return TEXT("Reference path changed");

        }



        if (StateKey.Contains(TEXT("HiddenInGame"), ESearchCase::IgnoreCase))

        {

            if (IsTraceValueTrue(NewValue))

            {

                return TEXT("Hidden in game was enabled");

            }



            if (IsTraceValueFalse(NewValue))

            {

                return TEXT("Hidden in game was disabled");

            }

        }



        if (StateKey.Contains(TEXT("IsVisible"), ESearchCase::IgnoreCase) || StateKey.Contains(TEXT("VisibleFlag"), ESearchCase::IgnoreCase))

        {

            if (IsTraceValueTrue(NewValue))

            {

                return TEXT("Visibility was turned on");

            }



            if (IsTraceValueFalse(NewValue))

            {

                return TEXT("Visibility was turned off");

            }

        }



        if (StateKey.Contains(TEXT("CollisionEnabled"), ESearchCase::IgnoreCase))

        {

            const FString NewCollision = FormatCollisionEnabledValue(NewValue);

            if (NewCollision == TEXT("NoCollision"))

            {

                return FString::Printf(TEXT("%s changed to NoCollision"), *PropertyName);

            }

            return FString::Printf(TEXT("%s changed to %s"), *PropertyName, *NewCollision);

        }



        if (StateKey.Contains(TEXT("EnableCollision"), ESearchCase::IgnoreCase)

            || StateKey.Contains(TEXT("GenerateOverlapEvents"), ESearchCase::IgnoreCase))

        {

            if (IsTraceValueTrue(NewValue))

            {

                return FString::Printf(TEXT("%s was enabled"), *PropertyName);

            }



            if (IsTraceValueFalse(NewValue))

            {

                return FString::Printf(TEXT("%s was disabled"), *PropertyName);

            }

        }



        if (StateKey.Contains(TEXT("Active"), ESearchCase::IgnoreCase))

        {

            return IsTraceValueTrue(NewValue) ? TEXT("Component was activated") : TEXT("Component was deactivated");

        }



        if (StateKey.Contains(TEXT("Registered"), ESearchCase::IgnoreCase))

        {

            return IsTraceValueTrue(NewValue) ? TEXT("Component was registered") : TEXT("Component was unregistered");

        }



        if (StateKey.Contains(TEXT("TickEnabled"), ESearchCase::IgnoreCase))

        {

            return IsTraceValueTrue(NewValue) ? TEXT("Tick was enabled") : TEXT("Tick was disabled");

        }



        if (StateKey.Contains(TEXT("Exists"), ESearchCase::IgnoreCase))

        {

            return NewValue == TEXT("<removed>") ? TEXT("Tracked object disappeared") : TEXT("Tracked object appeared");

        }



        if (StateKey.Contains(TEXT("AttachParent"), ESearchCase::IgnoreCase) || StateKey.Contains(TEXT("AttachSocket"), ESearchCase::IgnoreCase))

        {

            return TEXT("Attachment relationship changed");

        }



        if (Category == TEXT("Transform"))

        {

            return FString::Printf(TEXT("%s transform value changed"), *PropertyName);

        }



        if (Category == TEXT("Collision"))

        {

            return FString::Printf(TEXT("%s collision value changed"), *PropertyName);

        }



        if (Category == TEXT("Material"))

        {

            return FString::Printf(TEXT("%s render/material value changed"), *PropertyName);

        }



        return FString::Printf(TEXT("%s changed"), *PropertyName);

    }



    FString BuildActionSourceText(const FString& Reason, const FString& CallerSummary)

    {

        if (HasRuntimeBlueprintExecutionSource(CallerSummary))

        {

            const FString FunctionName = ExtractTraceToken(CallerSummary, TEXT("Function="));

            const FString NativeCall = ExtractTraceToken(CallerSummary, TEXT("NativeCall="));

            FString InstanceName = ExtractTraceToken(CallerSummary, TEXT("Instance="));

            if (InstanceName.IsEmpty())

            {

                InstanceName = ExtractTraceToken(CallerSummary, TEXT("Actor="));

            }

            const FString ClassName = ExtractTraceToken(CallerSummary, TEXT("Class="));



            if (NativeCall.IsEmpty() || NativeCall == TEXT("<unknown native call>"))

            {

                return FString::Printf(TEXT("Runtime Blueprint source near change: %s.%s on instance %s"),

                    ClassName.IsEmpty() ? TEXT("<unknown class>") : *ClassName,

                    FunctionName.IsEmpty() ? TEXT("<unknown function>") : *FunctionName,

                    InstanceName.IsEmpty() ? TEXT("<unknown instance>") : *InstanceName);

            }



            return FString::Printf(TEXT("Runtime Blueprint source: %s.%s called %s on instance %s"),

                ClassName.IsEmpty() ? TEXT("<unknown class>") : *ClassName,

                FunctionName.IsEmpty() ? TEXT("<unknown function>") : *FunctionName,

                NativeCall.IsEmpty() ? TEXT("<unknown native call>") : *NativeCall,

                InstanceName.IsEmpty() ? TEXT("<unknown instance>") : *InstanceName);

        }



        if (IsUnresolvedCallerSummary(CallerSummary))

        {

            return TEXT("Source unresolved: current referencers are holders only, not confirmed callers");

        }



        const FString WatchRule = ExtractTraceToken(CallerSummary, TEXT("WatchRule="));

        if (!WatchRule.IsEmpty())

        {

            FString FunctionName = ExtractTraceToken(CallerSummary, TEXT("Function="));

            if (FunctionName.IsEmpty())

            {

                FunctionName = ExtractTraceToken(CallerSummary, TEXT("BlueprintGraph:"));

            }

            const FString SelfAction = ExtractTraceToken(CallerSummary, TEXT("SelfAction="));

            const FString BlueprintAction = ExtractTraceToken(CallerSummary, TEXT("Action="));

            FString ActorName = ExtractTraceToken(CallerSummary, TEXT("Instance="));

            if (ActorName.IsEmpty())

            {

                ActorName = ExtractTraceToken(CallerSummary, TEXT("Actor="));

            }

            const FString ClassName = ExtractTraceToken(CallerSummary, TEXT("Class="));

            const FString SourceAction = !SelfAction.IsEmpty() ? SelfAction : BlueprintAction;



            if (!SourceAction.IsEmpty())

            {

                return FString::Printf(TEXT("Watch %s hit: %s.%s called %s"),

                    *WatchRule,

                    ClassName.IsEmpty() ? TEXT("<unknown class>") : *ClassName,

                    FunctionName.IsEmpty() ? TEXT("<unknown function>") : *FunctionName,

                    *SourceAction);

            }



            if (!ActorName.IsEmpty() || !ClassName.IsEmpty())

            {

                return FString::Printf(TEXT("Watch %s hit: likely source Instance=%s Class=%s"),

                    *WatchRule,

                    ActorName.IsEmpty() ? TEXT("-") : *ActorName,

                    ClassName.IsEmpty() ? TEXT("-") : *ClassName);

            }

        }



        FString FunctionName = ExtractTraceToken(CallerSummary, TEXT("Function="));

        if (FunctionName.IsEmpty())

        {

            FunctionName = ExtractTraceToken(CallerSummary, TEXT("BlueprintGraph:"));

        }

        FString ActorName = ExtractTraceToken(CallerSummary, TEXT("Instance="));

        if (ActorName.IsEmpty())

        {

            ActorName = ExtractTraceToken(CallerSummary, TEXT("Actor="));

        }

        const FString ClassName = ExtractTraceToken(CallerSummary, TEXT("Class="));



        auto FormatBlueprintSource = [&FunctionName, &ActorName, &ClassName](const FString& Kind, const FString& ActionName)

        {

            const FString SourceClass = ClassName.IsEmpty() ? TEXT("<unknown class>") : ClassName;

            const FString SourceFunction = FunctionName.IsEmpty() ? TEXT("<unknown function>") : FunctionName;

            const FString SourceActor = ActorName.IsEmpty() ? FString() : FString::Printf(TEXT(" on instance %s"), *ActorName);

            return FString::Printf(TEXT("%s: %s.%s called %s%s"), *Kind, *SourceClass, *SourceFunction, *ActionName, *SourceActor);

        };



        const FString SelfAction = ExtractTraceToken(CallerSummary, TEXT("SelfAction="));

        if (!SelfAction.IsEmpty())

        {

            return FormatBlueprintSource(TEXT("Likely Blueprint self action"), SelfAction);

        }



        const FString BlueprintAction = ExtractTraceToken(CallerSummary, TEXT("Action="));

        if (!BlueprintAction.IsEmpty())

        {

            return FormatBlueprintSource(TEXT("Likely Blueprint action"), BlueprintAction);

        }



        if (!FunctionName.IsEmpty())

        {

            return FString::Printf(TEXT("Likely Blueprint function: %s"), *FunctionName);

        }



        const FString BlueprintCall = ExtractTraceToken(CallerSummary, TEXT("Call="));

        if (!BlueprintCall.IsEmpty())

        {

            return FormatBlueprintSource(TEXT("Likely Blueprint call"), BlueprintCall);

        }



        if (Reason.StartsWith(TEXT("RenderStateDirty"), ESearchCase::IgnoreCase))

        {

            return FString::Printf(TEXT("Runtime hook detected render-state change: %s"), *Reason);

        }



        if (Reason.StartsWith(TEXT("PropertyChanged"), ESearchCase::IgnoreCase))

        {

            return FString::Printf(TEXT("Runtime property change hook: %s"), *Reason);

        }



        if (Reason.StartsWith(TEXT("Reference"), ESearchCase::IgnoreCase))

        {

            return FString::Printf(TEXT("Reference scanner detected %s"), *Reason);

        }



        if (!CallerSummary.IsEmpty())

        {

            return CallerSummary;

        }



        return Reason.IsEmpty() || Reason == TEXT("Tick")

            ? TEXT("State polling detected the value change")

            : FString::Printf(TEXT("Detected via %s"), *Reason);

    }



    FString BuildActionImpactText(const FString& StateKey, const FString& OldValue, const FString& NewValue)

    {

        const FString PropertyName = ExtractStatePropertyName(StateKey);

        return FString::Printf(TEXT("%s: %s"), *PropertyName, *FormatTraceValueChangeForState(StateKey, OldValue, NewValue));

    }



    FString BuildDiagnosticText(const FString& StateKey, const FString& OldValue, const FString& NewValue, const FString& Reason, const FString& CallerSummary)

    {

        const FString Category = ClassifyStateChange(StateKey, Reason);

        if (Category == TEXT("Visibility"))

        {

            if (HasRuntimeBlueprintExecutionSource(CallerSummary))

            {

                return TEXT("Visibility changed during a Blueprint VM call. Details show the class, function, and native Hidden/Visibility function captured at the hook moment.");

            }



            if (CallerSummary.Contains(TEXT("WatchRule=Visibility")))

            {

                return IsVisibilityOffValue(StateKey, NewValue)

                    ? TEXT("Visibility Watch Rule matched an OFF transition. Details list the best Blueprint class/function candidates found from self actions, referencers, and level graph references.")

                    : TEXT("Visibility Watch Rule matched an ON transition. Details list the best Blueprint class/function candidates found from self actions, referencers, and level graph references.");

            }



            if (CallerSummary.IsEmpty())

            {

                return TEXT("Visibility changed without a matched caller. Check BeginPlay, Construction Script, parent hidden state, and Blueprint Set Visibility/Hidden nodes.");

            }



            return TEXT("Visibility-related state changed. The caller summary shows the best matched Blueprint/action source.");

        }



        if (Category == TEXT("Collision"))

        {

            if (StateKey.Contains(TEXT("CollisionEnabled"), ESearchCase::IgnoreCase)

                && FormatCollisionEnabledValue(NewValue) == TEXT("NoCollision"))

            {

                return CallerSummary.IsEmpty()

                    ? TEXT("Collision Enabled changed to NoCollision. The exact caller was not confirmed; check Blueprint Set Collision Enabled/Set Actor Enable Collision nodes, construction/runtime scripts, and external actors that hold a reference to this instance.")

                    : TEXT("Collision Enabled changed to NoCollision. Details show the best matched runtime Blueprint source or referencing actor candidate that may have changed it.");

            }

            return TEXT("Collision-related state changed. Check Collision Enabled, profile, object channel, overlap events, and physics simulation settings.");

        }



        if (Category == TEXT("Transform"))

        {

            return TEXT("Transform or attachment changed. Check Attach/Detach, parent transform, timeline, or component animation logic.");

        }



        if (Category == TEXT("Collision"))

        {

            return TEXT("Collision state changed. Check collision profile changes, overlap generation, and physics setup nodes.");

        }



        if (Category == TEXT("Material"))

        {

            return TEXT("Render/material-like state changed. Check dissolve/material parameter logic and custom Blueprint component variables.");

        }



        if (Category == TEXT("Lifecycle"))

        {

            return TEXT("Lifecycle state changed. Check Activate/Deactivate, Register/Unregister, component creation, or tick toggles.");

        }



        return FString::Printf(TEXT("Observed change via %s."), Reason.IsEmpty() ? TEXT("tracker") : *Reason);

    }



    FLinearColor GetEventAccentColor(const FString& Category)

    {

        if (Category == TEXT("Lifecycle"))

        {

            return FLinearColor(0.30f, 0.69f, 0.31f);

        }



        if (Category == TEXT("Reference"))

        {

            return FLinearColor(0.13f, 0.59f, 0.95f);

        }



        if (Category == TEXT("Transform"))

        {

            return FLinearColor(1.0f, 0.60f, 0.0f);

        }



        if (Category == TEXT("Visibility"))

        {

            return FLinearColor(0.61f, 0.15f, 0.69f);

        }



        if (Category == TEXT("Collision"))

        {

            return FLinearColor(0.0f, 0.72f, 0.68f);

        }



        return FLinearColor(0.53f, 0.53f, 0.53f);

    }



    FLinearColor GetTraceSelectionAccentColor()

    {

        return FLinearColor(0.13f, 0.59f, 0.95f);

    }



    bool IsUnresolvedCallerSummary(const FString& CallerSummary)

    {

        return CallerSummary.Contains(TEXT("No matching action node found"), ESearchCase::IgnoreCase)

            || CallerSummary.Contains(TEXT("Source=<not found>"), ESearchCase::IgnoreCase);

    }



    bool HasRuntimeBlueprintExecutionSource(const FString& CallerSummary)

    {

        return CallerSummary.Contains(TEXT("Execution=RuntimeBlueprint"), ESearchCase::IgnoreCase);

    }

}

