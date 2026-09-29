#include "TMReportFormatter.h"

#include "TMLocalization.h"

#include "Misc/DateTime.h"

namespace TMReportFormatter
{
    FString NowText()
    {
        return FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
    }

    FString SanitizeLine(FString Text)
    {
        Text.ReplaceInline(TEXT("\r"), TEXT(" "));
        Text.ReplaceInline(TEXT("\n"), TEXT(" "));
        Text.TrimStartAndEndInline();
        return Text;
    }

    static void AppendMetadataLine(FString& Out, const FString& Key, const FString& Value)
    {
        Out += FString::Printf(TEXT("- %s: %s\n"), *SanitizeLine(Key), *SanitizeLine(Value));
    }

    static FString L(const TCHAR* English)
    {
        return TMLoc::String(English, English);
    }

    static bool ToolNameContains(const FString& ToolName, const TCHAR* Needle)
    {
        return ToolName.Contains(Needle, ESearchCase::IgnoreCase);
    }

    static void AppendSafetyList(FString& Out, const FString& Label, const TArray<FString>& Items)
    {
        if (Items.IsEmpty())
        {
            return;
        }

        Out += FString::Printf(TEXT("- %s:\n"), *Label);
        for (const FString& Item : Items)
        {
            Out += FString::Printf(TEXT("  - %s\n"), *SanitizeLine(Item));
        }
    }

    FString BuildDiagnosticSafetyBlock(const FDiagnosticSafetySection& Section)
    {
        FString Out;
        if (!Section.Confidence.IsEmpty())
        {
            Out += FString::Printf(TEXT("- %s: %s\n"), *L(TEXT("Confidence")), *SanitizeLine(Section.Confidence));
        }
        if (!Section.Scope.IsEmpty())
        {
            Out += FString::Printf(TEXT("- %s: %s\n"), *L(TEXT("Scope")), *SanitizeLine(Section.Scope));
        }

        AppendSafetyList(Out, L(TEXT("Evidence used")), Section.Evidence);
        AppendSafetyList(Out, L(TEXT("Known limitations")), Section.Limitations);
        AppendSafetyList(Out, L(TEXT("Recommended checks")), Section.NextChecks);

        return Out;
    }

    static void SetDefaultSafety(FDiagnosticSafetySection& Section)
    {
        Section.Confidence = L(TEXT("Medium"));
        Section.Scope = L(TEXT("Best-effort snapshot of the current editor/runtime state."));
        Section.Evidence.Add(L(TEXT("Uses loaded editor objects, asset metadata, Blueprint graph data, and runtime events available when the report was generated.")));
        Section.Limitations.Add(L(TEXT("This report is a diagnostic hint, not proof that no other cause exists.")));
        Section.Limitations.Add(L(TEXT("Unloaded assets, native-only paths, dynamic delegates, reflection by name, and work skipped by performance budgets may be incomplete.")));
        Section.NextChecks.Add(L(TEXT("If gameplay disagrees with the report, reproduce in PIE and inspect Details/log rows before changing project settings.")));
    }

    static void SetCollisionSafety(FDiagnosticSafetySection& Section)
    {
        Section.Confidence = L(TEXT("Medium"));
        Section.Scope = L(TEXT("Selected A/B actors and their primitive component pairs at the sampled frame."));
        Section.Evidence.Add(L(TEXT("Compares Collision Enabled, object channel, response matrix, body setup, bounds, overlap query, and available sweep checks.")));
        Section.Limitations.Add(L(TEXT("Block-capable means the settings can block; it does not guarantee that gameplay movement is actually stopped.")));
        Section.Limitations.Add(L(TEXT("Sweep=false, Teleport, or a MovementComponent using a different UpdatedComponent/root can bypass otherwise valid blocking collision.")));
        Section.Limitations.Add(L(TEXT("Static overlap checks are not a full replacement for the exact swept movement performed by gameplay code.")));
        Section.Limitations.Add(L(TEXT("Chaos physics-only contact, custom primitives, streamed-out actors, or disabled components may require runtime inspection.")));
        Section.NextChecks.Add(L(TEXT("Check Quick Fix Tips and suspicious movement nodes first.")));
        Section.NextChecks.Add(L(TEXT("Reproduce the movement with Sweep=true and inspect the returned FHitResult.")));
        Section.NextChecks.Add(L(TEXT("Confirm the MovementComponent UpdatedComponent or actor root is the actual blocking primitive.")));
    }

    static void SetClickSafety(FDiagnosticSafetySection& Section)
    {
        Section.Confidence = L(TEXT("Medium"));
        Section.Scope = L(TEXT("The clicked or manually selected target, relevant hit-test path, and nearby input/click delivery checks."));
        Section.Evidence.Add(L(TEXT("Uses the click-resolved widget/actor, visibility hit result, selected target, input settings, and reachable Blueprint bindings without project-wide Blueprint loading.")));
        Section.Limitations.Add(L(TEXT("Bindings in unloaded Blueprints, native code, Enhanced Input logic, or dynamic delegates created after the sample may be missed.")));
        Section.Limitations.Add(L(TEXT("A consumed UI click can prevent world actor and bound event stages from being reached, so later stages may be unknown rather than absent.")));
        Section.NextChecks.Add(L(TEXT("Use Pick Next PIE Click on the exact failed click and compare the first blocked stage with Details.")));
        Section.NextChecks.Add(L(TEXT("Verify UMG visibility, hit-testability, input mode, PlayerController click settings, and actor collision visibility responses.")));
    }

    static void SetAudioSafety(FDiagnosticSafetySection& Section)
    {
        Section.Confidence = L(TEXT("Medium"));
        Section.Scope = L(TEXT("Audio playback observed while the trace tab was open, including detectable fire-and-forget playback."));
        Section.Evidence.Add(L(TEXT("Uses live AudioComponent state, owner/component/world data, sound name, and best-effort Blueprint/source attribution.")));
        Section.Limitations.Add(L(TEXT("Very short sounds, native-only PlaySound calls, pooled components, or playback before the tab opens can be missed.")));
        Section.Limitations.Add(L(TEXT("Source attribution is heuristic when playback is detached from a persistent component or actor.")));
        Section.NextChecks.Add(L(TEXT("Keep the tab open before PIE and verify low-confidence source rows in Details.")));
    }

    static void SetCallChainSafety(FDiagnosticSafetySection& Section)
    {
        Section.Confidence = L(TEXT("Medium"));
        Section.Scope = L(TEXT("Static Blueprint/C++ call candidates reachable from the selected function under the current scan budget."));
        Section.Evidence.Add(L(TEXT("Uses Blueprint graph nodes, loaded packages, asset-registry candidates, source-index candidates, caller cache, and incremental scan results.")));
        Section.Limitations.Add(L(TEXT("Dynamic delegates, timers, interfaces, reflection by name, native-only dispatch, and data-driven calls may be incomplete.")));
        Section.Limitations.Add(L(TEXT("If the report says truncated or budget-limited, treat missing paths as unknown, not absent.")));
        Section.NextChecks.Add(L(TEXT("Run a deeper scan when missing a caller would be costly, then inspect truncated paths first.")));
    }

    static void SetOutlinerSafety(FDiagnosticSafetySection& Section)
    {
        Section.Confidence = L(TEXT("High for loaded instances; Low for unloaded assets"));
        Section.Scope = L(TEXT("Currently loaded editor-world actors and their instance details."));
        Section.Evidence.Add(L(TEXT("Matches outliner label/name, class, tags, property names, property values, and actor-reference array contents from scanned actor instances.")));
        Section.Limitations.Add(L(TEXT("Unloaded streaming levels, asset defaults not instantiated in the world, and hidden custom detail providers may not be searched.")));
        Section.NextChecks.Add(L(TEXT("Load the relevant level or partition cell and search again if an expected actor is missing.")));
    }

    static void SetWidgetSafety(FDiagnosticSafetySection& Section)
    {
        Section.Confidence = L(TEXT("Medium"));
        Section.Scope = L(TEXT("Live UUserWidget instances and child widget visibility/lifecycle changes observed while the tab is open."));
        Section.Evidence.Add(L(TEXT("Uses live widget tree snapshots, viewport/outer ownership, visibility transitions, and best-effort controller/source hints.")));
        Section.Limitations.Add(L(TEXT("Controller/source attribution can be heuristic when the change comes from native code, animation, binding, or indirect Blueprint logic.")));
        Section.NextChecks.Add(L(TEXT("Open Details for low-confidence events and compare timestamp/frame with your gameplay action.")));
    }

    static void SetRuntimeErrorSafety(FDiagnosticSafetySection& Section)
    {
        Section.Confidence = L(TEXT("Medium"));
        Section.Scope = L(TEXT("Blueprint runtime errors captured while the panel was open during PIE/editor play."));
        Section.Evidence.Add(L(TEXT("Uses runtime error text, Blueprint class/object names, node hints, PIE instances, and available stack/context data.")));
        Section.Limitations.Add(L(TEXT("Native exceptions, suppressed warnings, or errors emitted before the panel starts listening may not appear.")));
        Section.Limitations.Add(L(TEXT("Instance extraction from completed logs is heuristic when Unreal did not print a concrete object path.")));
        Section.NextChecks.Add(L(TEXT("Open the reported node when available and confirm the live runtime instance before editing defaults.")));
    }

    static void SetPackageSafety(FDiagnosticSafetySection& Section)
    {
        Section.Confidence = L(TEXT("Estimated"));
        Section.Scope = L(TEXT("Current editor packaging session inferred from UAT/editor log phases."));
        Section.Evidence.Add(L(TEXT("Uses detected packaging start/end, stage keywords, platform hints, elapsed time, and recent log work items.")));
        Section.Limitations.Add(L(TEXT("Unreal does not expose one exact universal packaging percent; cook/cache/shader/compression work can shift the estimate.")));
        Section.NextChecks.Add(L(TEXT("Use the stage label and recent log together with the progress bar, not the percent alone.")));
        Section.NextChecks.Add(L(TEXT("For failures, inspect the suspected asset/class and the first real error before later cascading errors.")));
    }

    static void SetReferenceSafety(FDiagnosticSafetySection& Section)
    {
        Section.Confidence = L(TEXT("Medium"));
        Section.Scope = L(TEXT("Reference graph from loaded assets, asset-registry candidates, source candidates, and Blueprint graph references under the selected scan mode."));
        Section.Evidence.Add(L(TEXT("Uses direct Blueprint Get/Set/Call nodes, asset references, loaded packages, source-index candidates, and incremental background scans.")));
        Section.Limitations.Add(L(TEXT("Soft references, data-table paths, reflection by string, native C++ references, and skipped budget slices may be incomplete.")));
        Section.NextChecks.Add(L(TEXT("Use Fast Search for responsiveness and a deeper scan when missing a reference would be costly.")));
    }

    FDiagnosticSafetySection BuildDefaultDiagnosticSafetySection(const FString& ToolName)
    {
        FDiagnosticSafetySection Section;

        if (ToolNameContains(ToolName, TEXT("collision")))
        {
            SetCollisionSafety(Section);
        }
        else if (ToolNameContains(ToolName, TEXT("click")))
        {
            SetClickSafety(Section);
        }
        else if (ToolNameContains(ToolName, TEXT("audio")))
        {
            SetAudioSafety(Section);
        }
        else if (ToolNameContains(ToolName, TEXT("function call")) || ToolNameContains(ToolName, TEXT("call chain")))
        {
            SetCallChainSafety(Section);
        }
        else if (ToolNameContains(ToolName, TEXT("outliner")))
        {
            SetOutlinerSafety(Section);
        }
        else if (ToolNameContains(ToolName, TEXT("widget")))
        {
            SetWidgetSafety(Section);
        }
        else if (ToolNameContains(ToolName, TEXT("runtime error")) || ToolNameContains(ToolName, TEXT("blueprint runtime")) || ToolNameContains(ToolName, TEXT("pie error")))
        {
            SetRuntimeErrorSafety(Section);
        }
        else if (ToolNameContains(ToolName, TEXT("package")))
        {
            SetPackageSafety(Section);
        }
        else if (ToolNameContains(ToolName, TEXT("visual reference")) || ToolNameContains(ToolName, TEXT("reference")))
        {
            SetReferenceSafety(Section);
        }
        else
        {
            SetDefaultSafety(Section);
        }

        return Section;
    }

    FString BuildMarkdownReport(const FReport& Report)
    {
        FString Out;
        const FString ToolName = Report.ToolName.IsEmpty() ? TEXT("TraceMotive — Debug Pathfinder for Unreal") : Report.ToolName;

        Out += FString::Printf(TEXT("# %s\n\n"), *ToolName);
        Out += TEXT("- Format: TraceMotive Diagnostic Report v1\n");
        AppendMetadataLine(Out, TEXT("Generated"), NowText());
        AppendMetadataLine(Out, TEXT("Tool"), ToolName);

        for (const FMetadataItem& Item : Report.Metadata)
        {
            if (!Item.Key.IsEmpty())
            {
                AppendMetadataLine(Out, Item.Key, Item.Value.IsEmpty() ? TEXT("<none>") : Item.Value);
            }
        }

        Out += FString::Printf(TEXT("\n## %s\n"), *L(TEXT("Summary")));
        if (Report.Summary.IsEmpty())
        {
            Out += FString::Printf(TEXT("- %s\n"), *L(TEXT("No summary was provided.")));
        }
        else
        {
            Out += Report.Summary;
            if (!Report.Summary.EndsWith(TEXT("\n")))
            {
                Out += TEXT("\n");
            }
        }

        for (const FSection& Section : Report.Sections)
        {
            Out += FString::Printf(TEXT("\n## %s\n"), *(Section.Title.IsEmpty() ? L(TEXT("Details")) : Section.Title));
            if (Section.Body.IsEmpty())
            {
                Out += FString::Printf(TEXT("- %s\n"), *L(TEXT("No details.")));
            }
            else
            {
                Out += Section.Body;
                if (!Section.Body.EndsWith(TEXT("\n")))
                {
                    Out += TEXT("\n");
                }
            }
        }

        return Out;
    }

    FString BuildWrappedLegacyReport(const FString& ToolName, const FString& Summary, const FString& Details, const TArray<FMetadataItem>& Metadata)
    {
        FReport Report;
        Report.ToolName = ToolName;
        Report.Summary = Summary;
        Report.Metadata = Metadata;

        const FDiagnosticSafetySection SafetySection = BuildDefaultDiagnosticSafetySection(ToolName);
        const FString SafetyBody = BuildDiagnosticSafetyBlock(SafetySection);
        if (!SafetyBody.IsEmpty())
        {
            Report.Sections.Add(FSection(L(TEXT("Diagnostic Safety")), SafetyBody));
        }

        Report.Sections.Add(FSection(L(TEXT("Details")), Details));
        return BuildMarkdownReport(Report);
    }
}
