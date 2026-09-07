#include "TMContextShortcutHelper.h"
#include "TMStyle.h"

#include <initializer_list>

#include "TMLocalization.h"

#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const FName ContextShortcutHelperTabId(TEXT("TraceMotive.ContextShortcutHelper"));
    bool bContextShortcutHelperRegistered = false;
    TWeakPtr<SDockTab> ExistingContextShortcutTab;

    enum class ETMShortcutContext : uint8
    {
        MaterialGraph,
        BlueprintGraph,
        LevelViewport,
        ContentBrowser,
        WidgetDesigner,
        GenericGraph,
        GenericEditor
    };

    struct FTMShortcutItem
    {
        FString Category;
        FString Chord;
        FString Action;
        FString Note;
    };

    static FString ContextToString(ETMShortcutContext Context)
    {
        switch (Context)
        {
        case ETMShortcutContext::MaterialGraph: return TEXT("Material Graph");
        case ETMShortcutContext::BlueprintGraph: return TEXT("Blueprint Graph");
        case ETMShortcutContext::LevelViewport: return TEXT("Level Viewport");
        case ETMShortcutContext::ContentBrowser: return TEXT("Content Browser");
        case ETMShortcutContext::WidgetDesigner: return TEXT("UMG / Widget Designer");
        case ETMShortcutContext::GenericGraph: return TEXT("Graph Editor");
        default: return TEXT("Editor");
        }
    }

    static bool ContainsAny(const FString& Text, std::initializer_list<const TCHAR*> Needles)
    {
        for (const TCHAR* Needle : Needles)
        {
            if (Text.Contains(Needle, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    static void AddShortcut(TArray<FTMShortcutItem>& Items, const TCHAR* Chord, const TCHAR* Action, const TCHAR* Note = TEXT(""), const TCHAR* Category = TEXT("General"))
    {
        Items.Add({ FString(Category), FString(Chord), FString(Action), FString(Note) });
    }

    static void AddMaterialShortcut(TArray<FTMShortcutItem>& Items, const TCHAR* Category, const TCHAR* Chord, const TCHAR* Action, const TCHAR* Note = TEXT(""))
    {
        AddShortcut(Items, Chord, Action, Note, Category);
    }

    static TArray<FTMShortcutItem> BuildShortcutsForContext(ETMShortcutContext Context)
    {
        TArray<FTMShortcutItem> Items;

        switch (Context)
        {
        case ETMShortcutContext::MaterialGraph:
            AddMaterialShortcut(Items, TEXT("0. Capture / Guide"), TEXT("Click graph once"), TEXT("Lock this panel to the Material Graph"), TEXT("The list no longer changes just because the mouse hovers elsewhere."));

            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("1 + Click"), TEXT("Constant"), TEXT("Single float value."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("2 + Click"), TEXT("Constant2Vector"), TEXT("2-channel Vector2 value."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("3 + Click"), TEXT("Constant3Vector"), TEXT("RGB color / Vector3."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("4 + Click"), TEXT("Constant4Vector"), TEXT("RGBA / Vector4."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("A + Click"), TEXT("Add"), TEXT("Addition."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("B + Click"), TEXT("BumpOffset"), TEXT("Parallax / bump offset."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("C with nodes selected"), TEXT("Comment"), TEXT("Create a comment box."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("D + Click"), TEXT("Divide"), TEXT("Division."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("F + Click"), TEXT("Material Function Call"), TEXT("Call a material function."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("I + Click"), TEXT("If"), TEXT("Conditional branch."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("L + Click"), TEXT("Linear Interpolate / Lerp"), TEXT("Interpolate between two values."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("M + Click"), TEXT("Multiply"), TEXT("Multiplication."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("N + Click"), TEXT("Normalize"), TEXT("Normalize vector."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("O + Click"), TEXT("OneMinus"), TEXT("Calculate 1-x."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("P + Click"), TEXT("Panner"), TEXT("Scroll/move UVs."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("R + Click"), TEXT("ReflectionVectorWS"), TEXT("World-space reflection vector."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("S + Click"), TEXT("Scalar Parameter"), TEXT("Scalar parameter."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("T + Click"), TEXT("Texture Sample"), TEXT("Texture sample."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("U + Click"), TEXT("Texture Coordinate"), TEXT("UV coordinate."));
            AddMaterialShortcut(Items, TEXT("1. Single Key + Left Click / Node Creation"), TEXT("V + Click"), TEXT("Vector Parameter"), TEXT("Vector/color parameter."));

            AddMaterialShortcut(Items, TEXT("2. Multi-Key Shortcuts / Editing"), TEXT("Ctrl + W"), TEXT("Duplicate selected nodes"));
            AddMaterialShortcut(Items, TEXT("2. Multi-Key Shortcuts / Editing"), TEXT("Ctrl + C"), TEXT("Copy"));
            AddMaterialShortcut(Items, TEXT("2. Multi-Key Shortcuts / Editing"), TEXT("Ctrl + V"), TEXT("Paste"));
            AddMaterialShortcut(Items, TEXT("2. Multi-Key Shortcuts / Editing"), TEXT("Ctrl + X"), TEXT("Cut"));
            AddMaterialShortcut(Items, TEXT("2. Multi-Key Shortcuts / Editing"), TEXT("Ctrl + Z"), TEXT("Undo"));
            AddMaterialShortcut(Items, TEXT("2. Multi-Key Shortcuts / Editing"), TEXT("Ctrl + Y"), TEXT("Redo"));
            AddMaterialShortcut(Items, TEXT("2. Multi-Key Shortcuts / Editing"), TEXT("Ctrl + A"), TEXT("Select all"));

            AddMaterialShortcut(Items, TEXT("3. Multi-Key Shortcuts / View & Bookmarks"), TEXT("Ctrl + 0~9"), TEXT("Save graph bookmark"), TEXT("Save current graph location to the number."));
            AddMaterialShortcut(Items, TEXT("3. Multi-Key Shortcuts / View & Bookmarks"), TEXT("0~9"), TEXT("Jump to graph bookmark"));
            AddMaterialShortcut(Items, TEXT("3. Multi-Key Shortcuts / View & Bookmarks"), TEXT("Ctrl + Space"), TEXT("Toggle Content Drawer"));

            AddMaterialShortcut(Items, TEXT("4. Mouse Combos / Wires"), TEXT("Alt + Pin Click"), TEXT("Break all connections on that pin"));
            AddMaterialShortcut(Items, TEXT("4. Mouse Combos / Wires"), TEXT("Double-click wire"), TEXT("Create Reroute Node"));

            AddMaterialShortcut(Items, TEXT("5. Mouse Combos / Node Creation"), TEXT("Right Click"), TEXT("Open node search menu"));
            AddMaterialShortcut(Items, TEXT("5. Mouse Combos / Node Creation"), TEXT("Space"), TEXT("Open node search menu"));

            AddMaterialShortcut(Items, TEXT("6. Preview"), TEXT("Node Right Click > Start Previewing Node"), TEXT("Preview result up to this node"));
            AddMaterialShortcut(Items, TEXT("6. Preview"), TEXT("Node Right Click > Stop Previewing Node"), TEXT("Stop node preview"));

            AddMaterialShortcut(Items, TEXT("7. Navigation"), TEXT("F"), TEXT("Focus selected node"));
            AddMaterialShortcut(Items, TEXT("7. Navigation"), TEXT("Home"), TEXT("Move to material Output node / frame graph"));

            AddMaterialShortcut(Items, TEXT("8. Node Editing"), TEXT("F2"), TEXT("Rename selected node"));
            AddMaterialShortcut(Items, TEXT("8. Node Editing"), TEXT("Delete"), TEXT("Delete selected node"));
            break;

        case ETMShortcutContext::BlueprintGraph:
            AddShortcut(Items, TEXT("Right Click"), TEXT("Open Blueprint action menu"));
            AddShortcut(Items, TEXT("B + LMB"), TEXT("Create Branch"), TEXT("Works in most Blueprint graphs."));
            AddShortcut(Items, TEXT("S + LMB"), TEXT("Create Sequence"), TEXT("Works in most Blueprint graphs."));
            AddShortcut(Items, TEXT("F + LMB"), TEXT("Create ForEachLoop"), TEXT("Availability depends on graph type."));
            AddShortcut(Items, TEXT("C"), TEXT("Comment selected nodes"));
            AddShortcut(Items, TEXT("Ctrl + W"), TEXT("Duplicate selected nodes"));
            AddShortcut(Items, TEXT("Ctrl + F"), TEXT("Find in Blueprint"));
            AddShortcut(Items, TEXT("Home"), TEXT("Frame all nodes"));
            AddShortcut(Items, TEXT("Delete"), TEXT("Delete selected nodes"));
            break;

        case ETMShortcutContext::LevelViewport:
            AddShortcut(Items, TEXT("Ctrl + N"), TEXT("New Level"), TEXT(""), TEXT("1. File"));
            AddShortcut(Items, TEXT("Ctrl + O"), TEXT("Open Level"), TEXT(""), TEXT("1. File"));
            AddShortcut(Items, TEXT("Ctrl + S"), TEXT("Save Current Level"), TEXT(""), TEXT("1. File"));
            AddShortcut(Items, TEXT("Ctrl + Shift + S"), TEXT("Save All"), TEXT(""), TEXT("1. File"));
            AddShortcut(Items, TEXT("Ctrl + Alt + Shift + S"), TEXT("Choose Files to Save"), TEXT(""), TEXT("1. File"));
            AddShortcut(Items, TEXT("Ctrl + P"), TEXT("Open Asset / Quick Asset Search"), TEXT(""), TEXT("1. File"));
            AddShortcut(Items, TEXT("Ctrl + Tab"), TEXT("Switch Open Editor Tabs"), TEXT(""), TEXT("1. File"));

            AddShortcut(Items, TEXT("Ctrl + Z"), TEXT("Undo"), TEXT(""), TEXT("2. Edit"));
            AddShortcut(Items, TEXT("Ctrl + Y"), TEXT("Redo"), TEXT(""), TEXT("2. Edit"));
            AddShortcut(Items, TEXT("Ctrl + X"), TEXT("Cut"), TEXT(""), TEXT("2. Edit"));
            AddShortcut(Items, TEXT("Ctrl + C"), TEXT("Copy"), TEXT(""), TEXT("2. Edit"));
            AddShortcut(Items, TEXT("Ctrl + V"), TEXT("Paste"), TEXT(""), TEXT("2. Edit"));
            AddShortcut(Items, TEXT("Ctrl + W"), TEXT("Duplicate"), TEXT(""), TEXT("2. Edit"));
            AddShortcut(Items, TEXT("Delete"), TEXT("Delete"), TEXT(""), TEXT("2. Edit"));
            AddShortcut(Items, TEXT("Ctrl + A"), TEXT("Select All"), TEXT(""), TEXT("2. Edit"));
            AddShortcut(Items, TEXT("F2"), TEXT("Rename"), TEXT(""), TEXT("2. Edit"));

            AddShortcut(Items, TEXT("Esc"), TEXT("Clear Selection"), TEXT(""), TEXT("3. Selection"));
            AddShortcut(Items, TEXT("Ctrl + Click"), TEXT("Add / Remove Selection"), TEXT(""), TEXT("3. Selection"));
            AddShortcut(Items, TEXT("Shift + Click"), TEXT("Add to Selection"), TEXT(""), TEXT("3. Selection"));
            AddShortcut(Items, TEXT("Ctrl + Shift + A"), TEXT("Select All Actors of the Same Class"), TEXT(""), TEXT("3. Selection"));

            AddShortcut(Items, TEXT("Q"), TEXT("Select Tool"), TEXT(""), TEXT("4. Transform"));
            AddShortcut(Items, TEXT("W"), TEXT("Move Tool"), TEXT(""), TEXT("4. Transform"));
            AddShortcut(Items, TEXT("E"), TEXT("Rotate Tool"), TEXT(""), TEXT("4. Transform"));
            AddShortcut(Items, TEXT("R"), TEXT("Scale Tool"), TEXT(""), TEXT("4. Transform"));
            AddShortcut(Items, TEXT("Space"), TEXT("Cycle Q / W / E / R Tools"), TEXT(""), TEXT("4. Transform"));

            AddShortcut(Items, TEXT("RMB + W / S"), TEXT("Move Camera Forward / Backward"), TEXT("Hold the right mouse button."), TEXT("5. Viewport Navigation"));
            AddShortcut(Items, TEXT("RMB + A / D"), TEXT("Move Camera Left / Right"), TEXT("Hold the right mouse button."), TEXT("5. Viewport Navigation"));
            AddShortcut(Items, TEXT("RMB + Q / E"), TEXT("Move Camera Down / Up"), TEXT("Hold the right mouse button."), TEXT("5. Viewport Navigation"));
            AddShortcut(Items, TEXT("RMB + Shift"), TEXT("Increase Camera Movement Speed"), TEXT("Hold the right mouse button."), TEXT("5. Viewport Navigation"));
            AddShortcut(Items, TEXT("RMB + Mouse Wheel"), TEXT("Adjust Camera Speed"), TEXT("Hold the right mouse button."), TEXT("5. Viewport Navigation"));

            AddShortcut(Items, TEXT("F"), TEXT("Focus Selected Actor"), TEXT(""), TEXT("6. Viewport"));
            AddShortcut(Items, TEXT("G"), TEXT("Toggle Game View"), TEXT(""), TEXT("6. Viewport"));
            AddShortcut(Items, TEXT("F11"), TEXT("Toggle Immersive Viewport"), TEXT(""), TEXT("6. Viewport"));
            AddShortcut(Items, TEXT("Ctrl + R"), TEXT("Toggle Realtime"), TEXT(""), TEXT("6. Viewport"));
            AddShortcut(Items, TEXT("F9"), TEXT("Take Viewport Screenshot"), TEXT(""), TEXT("6. Viewport"));
            AddShortcut(Items, TEXT("~"), TEXT("Open Console"), TEXT(""), TEXT("6. Viewport"));

            AddShortcut(Items, TEXT("Alt + 2"), TEXT("Wireframe"), TEXT(""), TEXT("7. View Mode"));
            AddShortcut(Items, TEXT("Alt + 3"), TEXT("Unlit"), TEXT(""), TEXT("7. View Mode"));
            AddShortcut(Items, TEXT("Alt + 4"), TEXT("Lit"), TEXT(""), TEXT("7. View Mode"));
            AddShortcut(Items, TEXT("Alt + 5"), TEXT("Detail Lighting"), TEXT(""), TEXT("7. View Mode"));
            AddShortcut(Items, TEXT("Alt + 6"), TEXT("Lighting Only"), TEXT(""), TEXT("7. View Mode"));
            AddShortcut(Items, TEXT("Alt + 7"), TEXT("Light Complexity"), TEXT(""), TEXT("7. View Mode"));
            AddShortcut(Items, TEXT("Alt + 8"), TEXT("Shader Complexity"), TEXT(""), TEXT("7. View Mode"));

            AddShortcut(Items, TEXT("["), TEXT("Decrease Translation Snap"), TEXT(""), TEXT("8. Grid / Snap"));
            AddShortcut(Items, TEXT("]"), TEXT("Increase Translation Snap"), TEXT(""), TEXT("8. Grid / Snap"));
            AddShortcut(Items, TEXT("Shift + ["), TEXT("Decrease Rotation Snap"), TEXT(""), TEXT("8. Grid / Snap"));
            AddShortcut(Items, TEXT("Shift + ]"), TEXT("Increase Rotation Snap"), TEXT(""), TEXT("8. Grid / Snap"));
            AddShortcut(Items, TEXT("End"), TEXT("Snap to Floor"), TEXT(""), TEXT("8. Grid / Snap"));
            AddShortcut(Items, TEXT("Shift + End"), TEXT("Snap Bounds to Floor"), TEXT(""), TEXT("8. Grid / Snap"));
            AddShortcut(Items, TEXT("Alt + End"), TEXT("Snap Pivot to Floor"), TEXT(""), TEXT("8. Grid / Snap"));
            AddShortcut(Items, TEXT("Ctrl + End"), TEXT("Snap to Grid"), TEXT(""), TEXT("8. Grid / Snap"));

            AddShortcut(Items, TEXT("Alt + MMB Drag"), TEXT("Temporarily Move Pivot"), TEXT(""), TEXT("9. Pivot"));
            AddShortcut(Items, TEXT("V + Drag"), TEXT("Vertex Snap"), TEXT(""), TEXT("9. Pivot"));

            AddShortcut(Items, TEXT("Ctrl + 0~9"), TEXT("Save Camera Bookmark"), TEXT(""), TEXT("10. Camera Bookmark"));
            AddShortcut(Items, TEXT("0~9"), TEXT("Jump to Camera Bookmark"), TEXT(""), TEXT("10. Camera Bookmark"));

            AddShortcut(Items, TEXT("Alt + P"), TEXT("Play"), TEXT(""), TEXT("11. Play"));
            AddShortcut(Items, TEXT("Alt + S"), TEXT("Simulate"), TEXT(""), TEXT("11. Play"));
            AddShortcut(Items, TEXT("Shift + F1"), TEXT("Release Mouse Cursor"), TEXT(""), TEXT("11. Play"));
            AddShortcut(Items, TEXT("F8"), TEXT("Possess / Eject"), TEXT(""), TEXT("11. Play"));
            AddShortcut(Items, TEXT("Esc"), TEXT("Stop PIE"), TEXT("May vary depending on editor settings."), TEXT("11. Play"));

            AddShortcut(Items, TEXT("Ctrl + G"), TEXT("Group Actors"), TEXT(""), TEXT("12. Group"));
            AddShortcut(Items, TEXT("Shift + G"), TEXT("Ungroup Actors"), TEXT(""), TEXT("12. Group"));

            AddShortcut(Items, TEXT("Ctrl + Space"), TEXT("Toggle Content Drawer"), TEXT(""), TEXT("13. Content Browser"));
            AddShortcut(Items, TEXT("Ctrl + B"), TEXT("Browse to Selected Asset"), TEXT(""), TEXT("13. Content Browser"));
            AddShortcut(Items, TEXT("Ctrl + E"), TEXT("Open Selected Asset"), TEXT(""), TEXT("13. Content Browser"));

            AddShortcut(Items, TEXT("Shift + 1"), TEXT("Select / Place Mode"), TEXT(""), TEXT("14. Mode"));
            AddShortcut(Items, TEXT("Shift + 2"), TEXT("Landscape Mode"), TEXT(""), TEXT("14. Mode"));
            AddShortcut(Items, TEXT("Shift + 3"), TEXT("Foliage Mode"), TEXT(""), TEXT("14. Mode"));
            AddShortcut(Items, TEXT("Shift + 4"), TEXT("Mesh Paint Mode"), TEXT(""), TEXT("14. Mode"));
            AddShortcut(Items, TEXT("Shift + 5"), TEXT("Modeling Mode"), TEXT(""), TEXT("14. Mode"));
            AddShortcut(Items, TEXT("Shift + 6"), TEXT("Fracture Mode"), TEXT("Availability depends on installed plugins."), TEXT("14. Mode"));
            AddShortcut(Items, TEXT("Shift + 7"), TEXT("Brush Editing Mode"), TEXT("Availability depends on installed plugins."), TEXT("14. Mode"));
            AddShortcut(Items, TEXT("Shift + 8"), TEXT("Animation Mode"), TEXT("Availability depends on installed plugins."), TEXT("14. Mode"));

            AddShortcut(Items, TEXT("H"), TEXT("Hide Selected Actors"), TEXT(""), TEXT("15. Display"));
            AddShortcut(Items, TEXT("Ctrl + H"), TEXT("Unhide Actors"), TEXT(""), TEXT("15. Display"));
            AddShortcut(Items, TEXT("T"), TEXT("Toggle Translucent Selection"), TEXT(""), TEXT("15. Display"));
            AddShortcut(Items, TEXT("Ctrl + L"), TEXT("Rotate Directional Light"), TEXT(""), TEXT("15. Display"));
            AddShortcut(Items, TEXT("Ctrl + Shift + H"), TEXT("Toggle FPS / Frame Stats"), TEXT(""), TEXT("15. Display"));

            AddShortcut(Items, TEXT("Ctrl + Shift + W"), TEXT("Open Widget Reflector"), TEXT("Also useful for adjusting Editor UI Scale."), TEXT("16. Main Frame"));
            AddShortcut(Items, TEXT("Ctrl + Shift + T"), TEXT("Toggle Viewport Toolbar"), TEXT(""), TEXT("16. Main Frame"));
            AddShortcut(Items, TEXT("F10"), TEXT("Toggle All Sidebars"), TEXT(""), TEXT("16. Main Frame"));
            AddShortcut(Items, TEXT("Shift + F11"), TEXT("Toggle Full Screen"), TEXT(""), TEXT("16. Main Frame"));
            AddShortcut(Items, TEXT("F1"), TEXT("Open Help / Documentation"), TEXT(""), TEXT("16. Main Frame"));
            break;

        case ETMShortcutContext::ContentBrowser:
            AddShortcut(Items, TEXT("Ctrl + B"), TEXT("Browse selected asset / sync to Content Browser"));
            AddShortcut(Items, TEXT("F2"), TEXT("Rename selected asset"));
            AddShortcut(Items, TEXT("Ctrl + D"), TEXT("Duplicate selected asset"));
            AddShortcut(Items, TEXT("Delete"), TEXT("Delete selected asset"));
            AddShortcut(Items, TEXT("Ctrl + C / Ctrl + V"), TEXT("Copy / paste asset reference or asset"));
            AddShortcut(Items, TEXT("Right Click"), TEXT("Open asset context menu"));
            break;

        case ETMShortcutContext::WidgetDesigner:
            AddShortcut(Items, TEXT("Ctrl + Z / Y"), TEXT("Undo / redo widget edits"));
            AddShortcut(Items, TEXT("Ctrl + W"), TEXT("Duplicate selected widget"));
            AddShortcut(Items, TEXT("Delete"), TEXT("Delete selected widget"));
            AddShortcut(Items, TEXT("F2"), TEXT("Rename selected widget"), TEXT("When hierarchy/name field supports rename."));
            AddShortcut(Items, TEXT("Ctrl + F"), TEXT("Find/search in the current editor"));
            AddShortcut(Items, TEXT("Right Click"), TEXT("Open widget context menu"));
            break;

        case ETMShortcutContext::GenericGraph:
            AddShortcut(Items, TEXT("Right Click"), TEXT("Open graph action/context menu"), TEXT("SummonCreateNodeMenu / context menu."), TEXT("1. Navigation / Creation"));
            AddShortcut(Items, TEXT("Space"), TEXT("Open create node menu"), TEXT("When the focused graph supports SummonCreateNodeMenu."), TEXT("1. Navigation / Creation"));
            AddShortcut(Items, TEXT("C"), TEXT("Create comment"), TEXT("GraphEditor CreateComment command."), TEXT("1. Navigation / Creation"));
            AddShortcut(Items, TEXT("Mouse Wheel"), TEXT("Zoom graph"), TEXT("GraphEditor ZoomIn / ZoomOut."), TEXT("1. Navigation / Creation"));
            AddShortcut(Items, TEXT("MMB/RMB Drag"), TEXT("Pan graph view"), TEXT("Common graph panel navigation."), TEXT("1. Navigation / Creation"));

            AddShortcut(Items, TEXT("Ctrl + A"), TEXT("Select all nodes"), TEXT("Common graph selection command."), TEXT("2. Edit / Selection"));
            AddShortcut(Items, TEXT("Ctrl + C"), TEXT("Copy selected nodes"), TEXT("Common graph clipboard command."), TEXT("2. Edit / Selection"));
            AddShortcut(Items, TEXT("Ctrl + V"), TEXT("Paste nodes"), TEXT("Common graph clipboard command."), TEXT("2. Edit / Selection"));
            AddShortcut(Items, TEXT("Ctrl + X"), TEXT("Cut selected nodes"), TEXT("Common graph clipboard command."), TEXT("2. Edit / Selection"));
            AddShortcut(Items, TEXT("Ctrl + W / Ctrl + D"), TEXT("Duplicate selected nodes"), TEXT("Editor duplicate command when mapped by the graph editor."), TEXT("2. Edit / Selection"));
            AddShortcut(Items, TEXT("Delete"), TEXT("Delete selected nodes"), TEXT("Delete selected graph nodes."), TEXT("2. Edit / Selection"));
            AddShortcut(Items, TEXT("Ctrl + Z / Ctrl + Y"), TEXT("Undo / redo"), TEXT("Editor transaction undo/redo."), TEXT("2. Edit / Selection"));

            AddShortcut(Items, TEXT("F"), TEXT("Focus selected nodes"), TEXT("Common graph/viewport focus behavior."), TEXT("3. View / Bookmarks"));
            AddShortcut(Items, TEXT("Home"), TEXT("Frame graph / nodes"), TEXT("Common graph navigation behavior."), TEXT("3. View / Bookmarks"));
            AddShortcut(Items, TEXT("Ctrl + 0~9"), TEXT("Set quick jump bookmark"), TEXT("GraphEditor QuickJump SetQuickJump commands."), TEXT("3. View / Bookmarks"));
            AddShortcut(Items, TEXT("0~9"), TEXT("Jump to quick bookmark"), TEXT("GraphEditor QuickJump commands."), TEXT("3. View / Bookmarks"));
            AddShortcut(Items, TEXT("Shift + 0~9"), TEXT("Clear quick bookmark"), TEXT("GraphEditor ClearQuickJump commands when mapped."), TEXT("3. View / Bookmarks"));

            AddShortcut(Items, TEXT("Alt + Pin Click"), TEXT("Break link(s)"), TEXT("GraphEditor BreakThisLink / BreakPinLinks commands."), TEXT("4. Pin / Wire Actions"));
            AddShortcut(Items, TEXT("Right Click Pin"), TEXT("Pin actions"), TEXT("Promote to Variable, Split/Recombine Struct Pin, Reset Pin to Default, Watch Pin."), TEXT("4. Pin / Wire Actions"));
            AddShortcut(Items, TEXT("Double-click wire"), TEXT("Create reroute node"), TEXT("Supported by many graph panels."), TEXT("4. Pin / Wire Actions"));

            AddShortcut(Items, TEXT("Right Click Node"), TEXT("Node actions"), TEXT("Reconstruct Nodes, Break Node Links, Delete and Reconnect Nodes, Enable/Disable Nodes."), TEXT("5. Node Actions"));
            AddShortcut(Items, TEXT("Right Click Selection"), TEXT("Collapse / expand / promote selection"), TEXT("GraphEditor CollapseNodes, ExpandNodes, Promote Selection to Function/Macro when supported."), TEXT("5. Node Actions"));
            AddShortcut(Items, TEXT("Right Click Node"), TEXT("Find References / Go To Definition"), TEXT("GraphEditor FindReferences, FindAndReplaceReferences, GoToDefinition."), TEXT("5. Node Actions"));
            AddShortcut(Items, TEXT("Right Click Node"), TEXT("Breakpoint actions"), TEXT("Add/Remove/Enable/Disable/Toggle Breakpoint where supported."), TEXT("5. Node Actions"));

            AddShortcut(Items, TEXT("Toolbar / context menu"), TEXT("Align nodes"), TEXT("Align Top/Middle/Bottom/Left/Center/Right."), TEXT("6. Layout"));
            AddShortcut(Items, TEXT("Toolbar / context menu"), TEXT("Straighten connections"), TEXT("GraphEditor StraightenConnections."), TEXT("6. Layout"));
            AddShortcut(Items, TEXT("Toolbar / context menu"), TEXT("Distribute nodes"), TEXT("Distribute Horizontally / Vertically."), TEXT("6. Layout"));
            break;

        default:
            AddShortcut(Items, TEXT("Ctrl + S"), TEXT("Save"));
            AddShortcut(Items, TEXT("Ctrl + Z / Y"), TEXT("Undo / redo"));
            AddShortcut(Items, TEXT("Ctrl + F"), TEXT("Find/search if supported by the active panel"));
            AddShortcut(Items, TEXT("F2"), TEXT("Rename if the focused item supports rename"));
            AddShortcut(Items, TEXT("Delete"), TEXT("Delete selected item if supported"));
            AddShortcut(Items, TEXT("Right Click"), TEXT("Open context menu"));
            break;
        }

        return Items;
    }

    static FString BuildWidgetPathText(const FWidgetPath& Path)
    {
        FString Result;
        if (Path.IsValid())
        {
            const TSharedRef<SWindow> Window = Path.GetWindow();
            const FString WindowTitle = Window->GetTitle().ToString();
            if (!WindowTitle.IsEmpty())
            {
                Result += FString::Printf(TEXT("WindowTitle=%s"), *WindowTitle);
            }
        }

        for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
        {
            const TSharedRef<SWidget> Widget = Path.Widgets[Index].Widget;
            if (!Result.IsEmpty())
            {
                Result += TEXT(" > ");
            }
            Result += Widget->GetTypeAsString();
        }
        return Result;
    }

    static ETMShortcutContext DetectContextFromPathText(const FString& PathText)
    {
        if (ContainsAny(PathText, { TEXT("Material"), TEXT("MaterialEditor") }))
        {
            return ETMShortcutContext::MaterialGraph;
        }
        if (ContainsAny(PathText, { TEXT("Blueprint"), TEXT("Kismet") }))
        {
            return ETMShortcutContext::BlueprintGraph;
        }
        if (ContainsAny(PathText, { TEXT("LevelViewport"), TEXT("Viewport") }))
        {
            return ETMShortcutContext::LevelViewport;
        }
        if (ContainsAny(PathText, { TEXT("ContentBrowser"), TEXT("AssetView"), TEXT("PathView") }))
        {
            return ETMShortcutContext::ContentBrowser;
        }
        if (ContainsAny(PathText, { TEXT("Widget"), TEXT("UMG"), TEXT("Designer") }))
        {
            return ETMShortcutContext::WidgetDesigner;
        }
        if (ContainsAny(PathText, { TEXT("GraphPanel"), TEXT("GraphEditor"), TEXT("SGraph") }))
        {
            return ETMShortcutContext::GenericGraph;
        }
        return ETMShortcutContext::GenericEditor;
    }

    class STMContextShortcutHelperWidget : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(STMContextShortcutHelperWidget) {}
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            CurrentShortcuts = BuildShortcutsForContext(CurrentContext);

            ChildSlot
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
                .Padding(10.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        BuildHeader()
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 8.0f)
                    [
                        SNew(STextBlock)
                        .Text(this, &STMContextShortcutHelperWidget::GetContextSummaryText)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                    [
                        SNew(STextBlock)
                        .Text(this, &STMContextShortcutHelperWidget::GetConfidenceText)
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                        .ColorAndOpacity(FLinearColor(0.72f, 0.74f, 0.78f))
                        .AutoWrapText(true)
                    ]
                    + SVerticalBox::Slot().FillHeight(1.0f)
                    [
                        SAssignNew(ShortcutListBox, SScrollBox)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(
                            TEXT("Click once in an editor area to lock this panel to that context. Limit: project-specific or plugin-specific shortcuts may differ from this guide."),
                            TEXT("에디터 영역을 한 번 클릭하면 이 패널이 해당 컨텍스트로 고정됩니다. 제한: 프로젝트/플러그인 전용 단축키는 이 가이드와 다를 수 있습니다.")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                        .ColorAndOpacity(FLinearColor(0.58f, 0.60f, 0.64f))
                        .AutoWrapText(true)
                    ]
                ]
            ];

            RebuildShortcutList();
            TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &STMContextShortcutHelperWidget::TickRefresh), 0.05f);
        }

        virtual ~STMContextShortcutHelperWidget() override
        {
            if (TickerHandle.IsValid())
            {
                FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
            }
        }

    private:
        TSharedRef<SWidget> BuildHeader()
        {
            return SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(TMLoc::Text(TEXT("Context Shortcuts"), TEXT("현재 컨텍스트 단축키")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .ContentPadding(FMargin(8.0f, 3.0f))
                    .OnClicked(this, &STMContextShortcutHelperWidget::OnCopyClicked)
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("Copy"), TEXT("복사")))
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .ContentPadding(FMargin(8.0f, 3.0f))
                    .OnClicked(this, &STMContextShortcutHelperWidget::OnRefreshClicked)
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("Refresh"), TEXT("새로고침")))
                    ]
                ];
        }

        bool TickRefresh(float DeltaTime)
        {
            if (!FSlateApplication::IsInitialized())
            {
                return true;
            }

            const bool bLeftMouseDown = FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
            if (bLeftMouseDown && !bWasLeftMouseDown)
            {
                if (CaptureContextUnderMouse())
                {
                    RebuildShortcutList();
                }
            }
            bWasLeftMouseDown = bLeftMouseDown;
            return true;
        }

        bool CaptureContextUnderMouse()
        {
            if (!FSlateApplication::IsInitialized())
            {
                return false;
            }

            FSlateApplication& SlateApp = FSlateApplication::Get();
            const FVector2D CursorPos = SlateApp.GetCursorPos();
            const FWidgetPath Path = SlateApp.LocateWindowUnderMouse(CursorPos, SlateApp.GetInteractiveTopLevelWindows(), false, SlateApp.GetUserIndexForMouse());
            const FString NewPathText = Path.IsValid() ? BuildWidgetPathText(Path) : FString();
            TSharedPtr<SWindow> ClickedWindow = Path.IsValid() ? Path.GetWindow().ToSharedPtr() : TSharedPtr<SWindow>();
            ETMShortcutContext NewContext = DetectContextFromPathText(NewPathText);
            if (NewContext == ETMShortcutContext::MaterialGraph && ClickedWindow.IsValid())
            {
                LastMaterialContextWindow = ClickedWindow;
            }
            if (NewContext == ETMShortcutContext::GenericGraph)
            {
                const bool bSameWindowAsKnownMaterial = ClickedWindow.IsValid()
                    && LastMaterialContextWindow.IsValid()
                    && LastMaterialContextWindow.Pin() == ClickedWindow;
                const bool bCurrentWindowAlreadyMaterial = CurrentContext == ETMShortcutContext::MaterialGraph
                    && CurrentCapturedWindow.IsValid()
                    && ClickedWindow.IsValid()
                    && CurrentCapturedWindow.Pin() == ClickedWindow;
                if (bSameWindowAsKnownMaterial || bCurrentWindowAlreadyMaterial)
                {
                    NewContext = ETMShortcutContext::MaterialGraph;
                    if (ClickedWindow.IsValid())
                    {
                        LastMaterialContextWindow = ClickedWindow;
                    }
                }
            }

            if (NewContext != CurrentContext || NewPathText != CurrentWidgetPathText || !bHasCapturedContext)
            {
                CurrentContext = NewContext;
                CurrentWidgetPathText = NewPathText;
                CurrentCapturedWindow = ClickedWindow;
                CurrentShortcuts = BuildShortcutsForContext(CurrentContext);
                bHasCapturedContext = true;
                return true;
            }
            return false;
        }

        TSharedRef<SWidget> BuildShortcutRow(const FTMShortcutItem& Item) const
        {
            return SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
                .Padding(FMargin(8.0f, 6.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBorder)
                        .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
                        .Padding(FMargin(7.0f, 3.0f))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Item.Chord))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .ColorAndOpacity(FLinearColor(0.95f, 0.95f, 0.95f))
                        ]
                    ]
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Item.Action))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Item.Note))
                            .Visibility(Item.Note.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                            .ColorAndOpacity(FLinearColor(0.62f, 0.66f, 0.70f))
                            .AutoWrapText(true)
                        ]
                    ]
                ];
        }

        void RebuildShortcutList()
        {
            if (!ShortcutListBox.IsValid())
            {
                return;
            }

            ShortcutListBox->ClearChildren();

            TArray<FString> CategoryOrder;
            TMap<FString, TArray<FTMShortcutItem>> ItemsByCategory;
            for (const FTMShortcutItem& Item : CurrentShortcuts)
            {
                if (!ItemsByCategory.Contains(Item.Category))
                {
                    CategoryOrder.Add(Item.Category);
                }
                ItemsByCategory.FindOrAdd(Item.Category).Add(Item);
            }

            for (const FString& Category : CategoryOrder)
            {
                const TArray<FTMShortcutItem>* CategoryItems = ItemsByCategory.Find(Category);
                if (!CategoryItems)
                {
                    continue;
                }

                TSharedRef<SVerticalBox> CategoryBody = SNew(SVerticalBox);
                for (const FTMShortcutItem& Item : *CategoryItems)
                {
                    CategoryBody->AddSlot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 6.0f)
                    [
                        BuildShortcutRow(Item)
                    ];
                }

                ShortcutListBox->AddSlot()
                .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                [
                    SNew(SExpandableArea)
                    .InitiallyCollapsed(false)
                    .HeaderContent()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("%s  (%d)"), *Category, CategoryItems->Num())))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                    ]
                    .BodyContent()
                    [
                        CategoryBody
                    ]
                ];
            }
        }

        FText GetContextSummaryText() const
        {
            return FText::Format(
                bHasCapturedContext
                    ? TMLoc::Text(TEXT("Captured: {0}"), TEXT("고정됨: {0}"))
                    : TMLoc::Text(TEXT("Click an editor area to capture shortcuts. Current default: {0}"), TEXT("단축키를 볼 에디터 영역을 한 번 클릭하세요. 현재 기본값: {0}")),
                FText::FromString(ContextToString(CurrentContext)));
        }

        FText GetConfidenceText() const
        {
            const FString PathPreview = CurrentWidgetPathText.Len() > 260
                ? CurrentWidgetPathText.Left(260) + TEXT("...")
                : CurrentWidgetPathText;
            return FText::Format(
                TMLoc::Text(TEXT("Mouse-under-widget path: {0}"), TEXT("마우스 아래 위젯 경로: {0}")),
                FText::FromString(PathPreview.IsEmpty() ? FString(TEXT("<none>")) : PathPreview));
        }

        FString BuildReportText() const
        {
            FString Report = FString::Printf(TEXT("# Context Shortcuts\nContext: %s\nWidgetPath: %s\n\n"),
                *ContextToString(CurrentContext),
                CurrentWidgetPathText.IsEmpty() ? TEXT("<none>") : *CurrentWidgetPathText);
            for (const FTMShortcutItem& Item : CurrentShortcuts)
            {
                Report += FString::Printf(TEXT("- %s: %s%s\n"),
                    *Item.Chord,
                    *Item.Action,
                    Item.Note.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Item.Note));
            }
            return Report;
        }

        FReply OnCopyClicked()
        {
            FPlatformApplicationMisc::ClipboardCopy(*BuildReportText());
            return FReply::Handled();
        }

        FReply OnRefreshClicked()
        {
            CaptureContextUnderMouse();
            RebuildShortcutList();
            return FReply::Handled();
        }

        TSharedPtr<SScrollBox> ShortcutListBox;
        FTSTicker::FDelegateHandle TickerHandle;
        ETMShortcutContext CurrentContext = ETMShortcutContext::GenericEditor;
        FString CurrentWidgetPathText;
        TArray<FTMShortcutItem> CurrentShortcuts;
        TWeakPtr<SWindow> CurrentCapturedWindow;
        TWeakPtr<SWindow> LastMaterialContextWindow;
        bool bWasLeftMouseDown = false;
        bool bHasCapturedContext = false;
    };

    static TSharedRef<SDockTab> SpawnContextShortcutHelperTab(const FSpawnTabArgs& Args)
    {
        TSharedRef<SDockTab> Tab = SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            .Label(TMLoc::Text(TEXT("Context Shortcuts"), TEXT("현재 컨텍스트 단축키")))
            .OnTabClosed_Lambda([](TSharedRef<SDockTab>)
            {
                ExistingContextShortcutTab.Reset();
            })
            [
                SNew(STMContextShortcutHelperWidget)
            ];

        ExistingContextShortcutTab = Tab;
        return Tab;
    }
}

namespace TMContextShortcutHelper
{
    void RegisterMenus()
    {
        if (!bContextShortcutHelperRegistered)
        {
            FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
                ContextShortcutHelperTabId,
                FOnSpawnTab::CreateStatic(&SpawnContextShortcutHelperTab))
                .SetDisplayName(TMLoc::Text(TEXT("Context Shortcuts"), TEXT("현재 컨텍스트 단축키")))
                .SetTooltipText(TMLoc::Text(
                    TEXT("Show useful shortcuts for the editor area currently under the mouse."),
                    TEXT("현재 마우스 아래 에디터 영역에서 유용한 단축키를 표시합니다.")))
                .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), "TraceMotive.ContextShortcutGuide"));

            bContextShortcutHelperRegistered = true;
        }

        if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))
        {
            FToolMenuSection& Section = WindowMenu->FindOrAddSection("TraceMotive");
            Section.AddMenuEntry(
                "TMOpenContextShortcuts",
                TMLoc::Text(TEXT("Context Shortcuts"), TEXT("현재 컨텍스트 단축키")),
                TMLoc::Text(
                    TEXT("Show shortcuts for the editor panel currently under the mouse, such as Material Graph, Blueprint Graph, viewport, or Content Browser."),
                    TEXT("Material Graph, Blueprint Graph, 뷰포트, Content Browser처럼 현재 마우스 아래 에디터 패널의 단축키를 표시합니다.")),
                FSlateIcon(TMStyle::GetStyleSetName(), "TraceMotive.ContextShortcutGuide"),
                FUIAction(FExecuteAction::CreateStatic(&TMContextShortcutHelper::OpenWindow)));
        }
    }

    void UnregisterMenus()
    {
        if (bContextShortcutHelperRegistered)
        {
            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ContextShortcutHelperTabId);
            bContextShortcutHelperRegistered = false;
        }
        ExistingContextShortcutTab.Reset();
    }

    void OpenWindow()
    {
        if (TSharedPtr<SDockTab> ExistingTab = ExistingContextShortcutTab.Pin())
        {
            ExistingTab->ActivateInParent(ETabActivationCause::SetDirectly);
            return;
        }

        ExistingContextShortcutTab = FGlobalTabmanager::Get()->TryInvokeTab(ContextShortcutHelperTabId);
    }
}
