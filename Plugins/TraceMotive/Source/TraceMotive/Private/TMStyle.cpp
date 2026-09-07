#include "TMStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

namespace
{
    TSharedPtr<FSlateStyleSet> TraceMotiveStyle;

    void AddVectorIcon(const FName Name, const TCHAR* FileName, const FLinearColor& Tint)
    {
        const FString UnifiedFileName = FString(FileName).Replace(TEXT("Icons/"), TEXT("Icons/Unified/"));
        TraceMotiveStyle->Set(Name, new FSlateVectorImageBrush(
            TraceMotiveStyle->RootToContentDir(UnifiedFileName, TEXT(".svg")), FVector2D(20.0f, 20.0f), Tint));

        // Launcher variants use the same high-contrast mask, with their category tint supplied by the tile.
        const FString LauncherName = Name.ToString().Replace(TEXT("TraceMotive."), TEXT("TraceMotive.Launcher."));
        TraceMotiveStyle->Set(FName(*LauncherName), new FSlateVectorImageBrush(
            TraceMotiveStyle->RootToContentDir(UnifiedFileName, TEXT(".svg")), FVector2D(20.0f, 20.0f)));
    }
}

namespace TMStyle
{
    FName GetStyleSetName()
    {
        static const FName StyleSetName(TEXT("TraceMotiveStyle"));
        return StyleSetName;
    }

    FLinearColor GetSearchColor() { return FLinearColor(0.28f, 0.62f, 0.86f); }
    FLinearColor GetRuntimeColor() { return FLinearColor(0.34f, 0.72f, 0.48f); }
    FLinearColor GetWorkflowColor() { return FLinearColor(0.82f, 0.68f, 0.28f); }

    void Initialize()
    {
        if (TraceMotiveStyle.IsValid()) return;

        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TraceMotive"));
        if (!Plugin.IsValid()) return;

        TraceMotiveStyle = MakeShared<FSlateStyleSet>(GetStyleSetName());
        TraceMotiveStyle->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
        AddVectorIcon(TEXT("TraceMotive.AssetUsageLocator"), TEXT("Icons/asset_usage_locator"), GetSearchColor());
        AddVectorIcon(TEXT("TraceMotive.AudioPlaybackTrace"), TEXT("Icons/audio_playback_trace"), GetRuntimeColor());
        AddVectorIcon(TEXT("TraceMotive.CppCallChain"), TEXT("Icons/cpp_call_chain"), GetSearchColor());
        AddVectorIcon(TEXT("TraceMotive.ClassFavorites"), TEXT("Icons/class_favorites"), GetWorkflowColor());
        AddVectorIcon(TEXT("TraceMotive.ClickEventDiagnostics"), TEXT("Icons/click_event_diagnostics"), GetRuntimeColor());
        AddVectorIcon(TEXT("TraceMotive.CollisionPairAnalyzer"), TEXT("Icons/collision_pair_analyzer"), GetRuntimeColor());
        AddVectorIcon(TEXT("TraceMotive.ContextShortcutGuide"), TEXT("Icons/context_shortcut_guide"), GetWorkflowColor());
        AddVectorIcon(TEXT("TraceMotive.EnhancedOutlinerSearch"), TEXT("Icons/enhanced_outliner_search"), GetSearchColor());
        AddVectorIcon(TEXT("TraceMotive.FunctionCallChain"), TEXT("Icons/function_call_chain"), GetSearchColor());
        AddVectorIcon(TEXT("TraceMotive.GlobalSpeedControl"), TEXT("Icons/global_speed_control"), GetWorkflowColor());
        AddVectorIcon(TEXT("TraceMotive.InstanceReferenceTracker"), TEXT("Icons/instance_reference_tracker"), GetRuntimeColor());
        AddVectorIcon(TEXT("TraceMotive.MaterialGraphShortcutGuide"), TEXT("Icons/material_graph_shortcut_guide"), GetWorkflowColor());
        AddVectorIcon(TEXT("TraceMotive.PackageProgress"), TEXT("Icons/package_progress"), GetWorkflowColor());
        AddVectorIcon(TEXT("TraceMotive.PerformanceSearchBoost"), TEXT("Icons/performance_search_boost"), GetSearchColor());
        AddVectorIcon(TEXT("TraceMotive.PIEErrorLogAnalyzer"), TEXT("Icons/pie_error_log_analyzer"), GetRuntimeColor());
        AddVectorIcon(TEXT("TraceMotive.PluginGuide"), TEXT("Icons/plugin_guide"), GetWorkflowColor());
        AddVectorIcon(TEXT("TraceMotive.RuntimeErrorTrace"), TEXT("Icons/runtime_error_trace"), GetRuntimeColor());
        AddVectorIcon(TEXT("TraceMotive.SkipToTarget"), TEXT("Icons/skip_to_target"), GetWorkflowColor());
        AddVectorIcon(TEXT("TraceMotive.ToolLauncher"), TEXT("Icons/tool_launcher"), GetWorkflowColor());
        AddVectorIcon(TEXT("TraceMotive.VariableValueTrace"), TEXT("Icons/variable_value_trace"), GetRuntimeColor());
        AddVectorIcon(TEXT("TraceMotive.VisualReferenceSearch"), TEXT("Icons/visual_reference_search"), GetSearchColor());
        AddVectorIcon(TEXT("TraceMotive.WidgetClickFlowTrace"), TEXT("Icons/widget_click_flow_trace"), GetRuntimeColor());
        AddVectorIcon(TEXT("TraceMotive.WidgetLifecycleTrace"), TEXT("Icons/widget_lifecycle_trace"), GetRuntimeColor());
        FSlateStyleRegistry::RegisterSlateStyle(*TraceMotiveStyle);
    }

    void Shutdown()
    {
        if (!TraceMotiveStyle.IsValid()) return;
        FSlateStyleRegistry::UnRegisterSlateStyle(*TraceMotiveStyle);
        TraceMotiveStyle.Reset();
    }
}
