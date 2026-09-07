#include "TMClassFavorites.h"
#include "TraceMotiveLog.h"
#include "TMStyle.h"



#include "TMLocalization.h"



#include "AssetRegistry/AssetData.h"

#include "AssetRegistry/AssetRegistryModule.h"

#include "ActorFactories/ActorFactory.h"

#include "AssetSelection.h"

#include "Blueprint/BlueprintSupport.h"

#include "ContentBrowserMenuContexts.h"

#include "ContentBrowserModule.h"

#include "IContentBrowserSingleton.h"

#include "Containers/Ticker.h"

#include "DragAndDrop/AssetDragDropOp.h"

#include "Editor.h"

#include "Editor/EditorEngine.h"

#include "Engine/Blueprint.h"

#include "FileHelpers.h"

#include "Framework/Application/SlateApplication.h"

#include "Framework/Docking/TabManager.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "GameFramework/Actor.h"

#include "HAL/IConsoleManager.h"

#include "Kismet2/BlueprintEditorUtils.h"

#include "LevelEditorViewport.h"

#include "Misc/ConfigCacheIni.h"

#include "Misc/PackageName.h"

#include "ScopedTransaction.h"

#include "Styling/AppStyle.h"

#include "Styling/SlateIconFinder.h"

#include "Subsystems/AssetEditorSubsystem.h"

#include "ToolMenus.h"

#include "Modules/ModuleManager.h"

#include "UObject/SoftObjectPath.h"

#include "Widgets/Input/SButton.h"

#include "Widgets/Input/SSearchBox.h"

#include "Widgets/Images/SImage.h"

#include "Widgets/Layout/SBorder.h"

#include "Widgets/Layout/SBox.h"

#include "Widgets/Layout/SScrollBox.h"

#include "Widgets/Layout/SSeparator.h"

#include "Widgets/Layout/SWidgetSwitcher.h"

#include "Widgets/Docking/SDockTab.h"

#include "Widgets/SBoxPanel.h"

#include "Widgets/SOverlay.h"

#include "Widgets/Text/STextBlock.h"

#include "Widgets/Views/SListView.h"

#include "Widgets/Views/STableRow.h"

#include "Widgets/Views/STableViewBase.h"



#define LOCTEXT_NAMESPACE "TMClassFavorites"



namespace

{

    const TCHAR* FavoriteConfigSection = TEXT("TraceMotive.ClassFavorites");

    const TCHAR* FavoriteConfigKey = TEXT("Asset");

    const FName ClassFavoritesTabId(TEXT("TraceMotive.ClassFavorites"));



    FString GetGeneratedClassObjectPath(const FAssetData& AssetData)

    {

        FString GeneratedClassExportPath;

        if (AssetData.GetTagValue(FBlueprintTags::GeneratedClassPath, GeneratedClassExportPath)

            && !GeneratedClassExportPath.IsEmpty())

        {

            return FPackageName::ExportTextPathToObjectPath(GeneratedClassExportPath);

        }



        return FString();

    }



    UClass* ResolveActorClassFromAssetData(const FAssetData& AssetData)

    {

        if (!AssetData.IsValid())

        {

            return nullptr;

        }



        if (UObject* LoadedObject = AssetData.FastGetAsset(false))

        {

            if (const UBlueprint* Blueprint = Cast<UBlueprint>(LoadedObject))

            {

                return Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass())

                    ? Blueprint->GeneratedClass

                    : nullptr;

            }



            if (UClass* ClassAsset = Cast<UClass>(LoadedObject))

            {

                return ClassAsset->IsChildOf(AActor::StaticClass()) ? ClassAsset : nullptr;

            }

        }



        const FString GeneratedClassObjectPath = GetGeneratedClassObjectPath(AssetData);

        if (!GeneratedClassObjectPath.IsEmpty())

        {

            if (UClass* GeneratedClass = FSoftClassPath(GeneratedClassObjectPath).TryLoadClass<AActor>())

            {

                return GeneratedClass->IsChildOf(AActor::StaticClass()) ? GeneratedClass : nullptr;

            }

        }



        UObject* LoadedObject = AssetData.GetAsset();

        if (const UBlueprint* Blueprint = Cast<UBlueprint>(LoadedObject))

        {

            return Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass())

                ? Blueprint->GeneratedClass

                : nullptr;

        }



        if (UClass* ClassAsset = Cast<UClass>(LoadedObject))

        {

            return ClassAsset->IsChildOf(AActor::StaticClass()) ? ClassAsset : nullptr;

        }



        return nullptr;

    }



    bool IsLevelAsset(const FAssetData& AssetData)

    {

        return AssetData.IsValid()

            && AssetData.AssetClassPath == UWorld::StaticClass()->GetClassPathName();

    }



    UActorFactory* ResolveActorFactoryFromAssetData(const FAssetData& AssetData)

    {

        if (!AssetData.IsValid() || !GEditor || IsLevelAsset(AssetData))

        {

            return nullptr;

        }



        for (UActorFactory* Factory : GEditor->ActorFactories)

        {

            FText ErrorMessage;

            if (Factory && Factory->CanCreateActorFrom(AssetData, ErrorMessage))

            {

                return Factory;

            }

        }



        return nullptr;

    }



    FAssetData ResolveAssetDataFromSavedPath(const FString& Path)

    {

        if (Path.IsEmpty())

        {

            return FAssetData();

        }



        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

        FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(Path));

        if (AssetData.IsValid())

        {

            return AssetData;

        }



        UObject* LoadedObject = FSoftObjectPath(Path).TryLoad();

        return LoadedObject ? FAssetData(LoadedObject) : FAssetData();

    }



    struct FTMFavoriteClassItem

    {

        FAssetData AssetData;

        FString DisplayName;

        FString ClassName;

        FString Path;

        FString FolderPath;



        explicit FTMFavoriteClassItem(const FAssetData& InAssetData)

            : AssetData(InAssetData)

            , DisplayName(InAssetData.AssetName.ToString())

            , Path(InAssetData.GetObjectPathString())

            , FolderPath(InAssetData.PackagePath.ToString())

        {

            if (UClass* ActorClass = ResolveActorClass(InAssetData))

            {

                ClassName = ActorClass->GetName();

                ClassName.RemoveFromEnd(TEXT("_C"));

            }

            else

            {

                ClassName = InAssetData.AssetClassPath.GetAssetName().ToString();

            }

        }



        static UClass* ResolveActorClass(const FAssetData& AssetData)

        {

            return ResolveActorClassFromAssetData(AssetData);

        }



        bool CanPlaceActor() const

        {

            UClass* ActorClass = ResolveActorClass(AssetData);

            const bool bCanPlaceActorClass = ActorClass

                && !ActorClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)

                && ActorClass->IsChildOf(AActor::StaticClass());

            return bCanPlaceActorClass || ResolveActorFactoryFromAssetData(AssetData) != nullptr;

        }

    };



    using FFavoriteItemPtr = TSharedPtr<FTMFavoriteClassItem>;



    TArray<FString> LoadFavoritePaths()

    {

        TArray<FString> Paths;

        if (GConfig)

        {

            GConfig->GetArray(FavoriteConfigSection, FavoriteConfigKey, Paths, GEditorPerProjectIni);

        }

        return Paths;

    }



    void SaveFavoritePaths(const TArray<FString>& Paths)

    {

        if (!GConfig)

        {

            return;

        }



        GConfig->SetArray(FavoriteConfigSection, FavoriteConfigKey, Paths, GEditorPerProjectIni);

        GConfig->Flush(false, GEditorPerProjectIni);

    }



    bool IsSupportedFavoriteAsset(const FAssetData& AssetData)

    {

        return ResolveActorClassFromAssetData(AssetData) != nullptr

            || IsLevelAsset(AssetData)

            || ResolveActorFactoryFromAssetData(AssetData) != nullptr;

    }



    bool AddFavoriteAsset(const FAssetData& AssetData)

    {

        if (!IsSupportedFavoriteAsset(AssetData))

        {

            UE_LOG(LogTraceMotive, Warning, TEXT("Asset & Level Favorites: %s is not a placeable asset, Actor class, or Level."),

                *AssetData.GetObjectPathString());

            return false;

        }



        TArray<FString> Paths = LoadFavoritePaths();

        const FString AssetPath = AssetData.GetSoftObjectPath().ToString();

        if (!Paths.Contains(AssetPath))

        {

            Paths.Add(AssetPath);

            SaveFavoritePaths(Paths);

        }



        return true;

    }



    TArray<FAssetData> GetSelectedContentBrowserAssets(const FToolMenuContext& Context)

    {

        if (const UContentBrowserAssetContextMenuContext* AssetContext =

            Context.FindContext<UContentBrowserAssetContextMenuContext>())

        {

            if (!AssetContext->SelectedAssets.IsEmpty())

            {

                return AssetContext->SelectedAssets;

            }

        }



        TArray<FAssetData> SelectedAssets;

        if (FModuleManager::Get().IsModuleLoaded("ContentBrowser"))

        {

            FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

            ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

        }

        return SelectedAssets;

    }



    void OpenFavoritesWindowDeferred()

    {

        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)

        {

            TMClassFavorites::OpenFavoritesWindow();

            return false;

        }));

    }



    void RemoveFavoriteAsset(const FString& AssetPath)

    {

        TArray<FString> Paths = LoadFavoritePaths();

        Paths.Remove(AssetPath);

        SaveFavoritePaths(Paths);

    }



    void SaveFavoriteItemsInOrder(const TArray<FFavoriteItemPtr>& Items)

    {

        TArray<FString> Paths;

        for (const FFavoriteItemPtr& Item : Items)

        {

            if (Item.IsValid() && !Item->Path.IsEmpty())

            {

                Paths.Add(Item->Path);

            }

        }



        SaveFavoritePaths(Paths);

    }



    TArray<FFavoriteItemPtr> LoadFavoriteItems()

    {

        TArray<FFavoriteItemPtr> Items;

        TArray<FString> ValidPaths;

        for (const FString& Path : LoadFavoritePaths())

        {

            const FAssetData AssetData = ResolveAssetDataFromSavedPath(Path);

            if (IsSupportedFavoriteAsset(AssetData))

            {

                Items.Add(MakeShared<FTMFavoriteClassItem>(AssetData));

                ValidPaths.Add(AssetData.GetSoftObjectPath().ToString());

            }

            else

            {

                UE_LOG(LogTraceMotive, Warning, TEXT("Class Favorites: skipped invalid favorite path: %s"), *Path);

            }

        }



        SaveFavoritePaths(ValidPaths);

        return Items;

    }



    FVector GetPlacementLocation()

    {

        return FVector::ZeroVector;

    }



    bool GetCurrentViewPlacementLocation(FVector& OutLocation)

    {

        if (!GCurrentLevelEditingViewportClient)

        {

            UE_LOG(LogTraceMotive, Warning, TEXT("Class Favorites: no active level viewport location is available."));

            return false;

        }



        OutLocation = GCurrentLevelEditingViewportClient->GetViewLocation();

        return true;

    }



    FVector GetFavoriteActorWholePlacementAnchor(const AActor* Actor)

    {

        if (!Actor)

        {

            return FVector::ZeroVector;

        }



        FVector Origin = Actor->GetActorLocation();

        FVector Extent = FVector::ZeroVector;

        Actor->GetActorBounds(false, Origin, Extent, true);

        return Extent.IsNearlyZero() ? Actor->GetActorLocation() : Origin;

    }



    void AlignFavoriteActorWholeToLocation(AActor* Actor, const FVector& TargetLocation)

    {

        if (!Actor)

        {

            return;

        }



        const FVector Anchor = GetFavoriteActorWholePlacementAnchor(Actor);

        const FVector Delta = TargetLocation - Anchor;

        Actor->SetActorLocation(Actor->GetActorLocation() + Delta, false, nullptr, ETeleportType::TeleportPhysics);

        Actor->PostEditMove(true);

    }



    bool PlaceFavoriteActorAtLocation(const FFavoriteItemPtr& Item, const FVector& Location, const FText& TransactionText, bool bAlignWholeActorToLocation = false)

    {

        if (!Item.IsValid() || !GEditor)

        {

            return false;

        }



        UClass* ActorClass = FTMFavoriteClassItem::ResolveActorClass(Item->AssetData);

        UActorFactory* ActorFactory = ActorClass ? nullptr : ResolveActorFactoryFromAssetData(Item->AssetData);

        UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

        if ((!ActorClass && !ActorFactory) || !EditorWorld || EditorWorld->IsPlayInEditor())

        {

            return false;

        }



        const FScopedTransaction Transaction(TransactionText);

        const FTransform SpawnTransform(FRotator::ZeroRotator, Location);

        AActor* SpawnedActor = ActorClass

            ? GEditor->AddActor(EditorWorld->GetCurrentLevel(), ActorClass, SpawnTransform, false, RF_Transactional, true)

            : GEditor->UseActorFactory(ActorFactory, Item->AssetData, &SpawnTransform, RF_Transactional);

        if (SpawnedActor)

        {

            SpawnedActor->SetActorLabel(Item->DisplayName);

            if (bAlignWholeActorToLocation)

            {

                SpawnedActor->Modify();

                AlignFavoriteActorWholeToLocation(SpawnedActor, Location);

            }



            GEditor->SelectNone(false, true);

            GEditor->SelectActor(SpawnedActor, true, true);

            GEditor->NoteSelectionChange();

            GEditor->RedrawLevelEditingViewports();

            return true;

        }



        return false;

    }



    bool PlaceFavoriteActor(const FFavoriteItemPtr& Item)

    {

        return PlaceFavoriteActorAtLocation(

            Item,

            GetPlacementLocation(),

            LOCTEXT("PlaceFavoriteActorTransaction", "Place Favorite Actor"));

    }



    bool PlaceFavoriteActorAtCurrentView(const FFavoriteItemPtr& Item)

    {

        FVector ViewLocation = FVector::ZeroVector;

        if (!GetCurrentViewPlacementLocation(ViewLocation))

        {

            return false;

        }



        return PlaceFavoriteActorAtLocation(

            Item,

            ViewLocation,

            LOCTEXT("PlaceFavoriteActorAtCurrentViewTransaction", "Place Favorite Actor At Current View"),

            true);

    }



    bool OpenFavoriteAsset(const FFavoriteItemPtr& Item)

    {

        if (!Item.IsValid() || !Item->AssetData.IsValid() || !GEditor)

        {

            return false;

        }



        if (IsLevelAsset(Item->AssetData))

        {

            // Favorite-level navigation calls LoadMap directly, which does not prompt to
            // save the active map. Save first and keep the current level open if saving
            // was cancelled or failed.
            if (!FEditorFileUtils::SaveCurrentLevel())

            {

                UE_LOG(LogTraceMotive, Warning, TEXT("Class Favorites: cancelled level switch because the current level was not saved."));

                return false;

            }

            return FEditorFileUtils::LoadMap(Item->AssetData.PackageName.ToString(), false, true);

        }



        UObject* AssetObject = Item->AssetData.FastGetAsset(false);

        if (!AssetObject)

        {

            AssetObject = Item->AssetData.GetAsset();

        }



        if (!AssetObject)

        {

            return false;

        }



        if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())

        {

            return AssetEditorSubsystem->OpenEditorForAsset(AssetObject);

        }



        return false;

    }



    bool SyncFavoriteAssetToContentBrowser(const FFavoriteItemPtr& Item)

    {

        if (!Item.IsValid())

        {

            return false;

        }



        FAssetData AssetData = Item->AssetData;

        if (!AssetData.IsValid())

        {

            AssetData = ResolveAssetDataFromSavedPath(Item->Path);

        }



        if (!AssetData.IsValid())

        {

            return false;

        }



        FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

        TArray<FAssetData> AssetsToSync;

        AssetsToSync.Add(AssetData);

        ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync, false, true);

        return true;

    }



    class STMFavoriteClassRow : public STableRow<FFavoriteItemPtr>

    {

    public:

        using FOnCanAcceptDrop = STableRow<FFavoriteItemPtr>::FOnCanAcceptDrop;

        using FOnAcceptDrop = STableRow<FFavoriteItemPtr>::FOnAcceptDrop;



        SLATE_BEGIN_ARGS(STMFavoriteClassRow) {}

            SLATE_ARGUMENT(FFavoriteItemPtr, Item)

            SLATE_EVENT(FSimpleDelegate, OnRemoved)

            SLATE_EVENT(FOnCanAcceptDrop, OnCanAcceptDrop)

            SLATE_EVENT(FOnAcceptDrop, OnAcceptDrop)

            SLATE_ATTRIBUTE(bool, IsEditMode)

        SLATE_END_ARGS()



        void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView)

        {

            Item = InArgs._Item;

            OnRemoved = InArgs._OnRemoved;

            IsEditMode = InArgs._IsEditMode;



            STableRow<FFavoriteItemPtr>::Construct(

                STableRow<FFavoriteItemPtr>::FArguments()

                .Padding(FMargin(2.0f, 1.0f))

                .OnCanAcceptDrop(InArgs._OnCanAcceptDrop)

                .OnAcceptDrop(InArgs._OnAcceptDrop)

                [

                    BuildRowContent()

                ],

                OwnerTableView

            );

        }



        virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override

        {

            STableRow<FFavoriteItemPtr>::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

            CachedRowWidth = AllottedGeometry.GetLocalSize().X;

        }



        TSharedRef<SWidget> BuildRowContent()

        {

            return SNew(SWidgetSwitcher)

                .WidgetIndex(this, &STMFavoriteClassRow::GetLayoutIndex)

                + SWidgetSwitcher::Slot()

                [

                    BuildNormalRowContent()

                ]

                + SWidgetSwitcher::Slot()

                [

                    BuildCompactRowContent()

                ]

                + SWidgetSwitcher::Slot()

                [

                    BuildMicroRowContent()

                ];

        }



        TSharedRef<SWidget> BuildNormalRowContent()

        {

            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

                .Padding(FMargin(8.0f, 6.0f))

                [

                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)

                    [

                        BuildClassIcon(18.0f)

                    ]

                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                    [

                        SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight()

                        [

                            SNew(STextBlock)

                            .Text(this, &STMFavoriteClassRow::GetDisplayNameText)

                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)

                        [

                            SNew(STextBlock)

                            .Text(this, &STMFavoriteClassRow::GetClassPathText)

                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))

                            .ColorAndOpacity(FLinearColor(0.62f, 0.66f, 0.70f))

                        ]

                    ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)

                    [

                        SNew(SButton)

                        .ContentPadding(FMargin(8.0f, 3.0f))

                        .ToolTipText(TMLoc::Text(TEXT("Place this asset at world origin (0,0,0)."), TEXT("이 에셋을 월드 원점(0,0,0)에 배치합니다.")))

                        .IsEnabled(this, &STMFavoriteClassRow::CanPlaceFavorite)

                        .OnClicked(this, &STMFavoriteClassRow::OnPlaceClicked)

                        [

                            SNew(STextBlock)

                            .Text(TMLoc::Text(TEXT("Place"), TEXT("배치")))

                        ]

                    ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)

                    [

                        SNew(SButton)

                        .ContentPadding(FMargin(8.0f, 3.0f))

                        .ToolTipText(TMLoc::Text(TEXT("Place this asset at the current level viewport camera location."), TEXT("이 에셋을 현재 레벨 뷰포트 카메라 위치에 배치합니다.")))

                        .IsEnabled(this, &STMFavoriteClassRow::CanPlaceFavorite)

                        .OnClicked(this, &STMFavoriteClassRow::OnPlaceHereClicked)

                        [

                            SNew(STextBlock)

                            .Text(TMLoc::Text(TEXT("Here"), TEXT("현재 위치")))

                        ]

                    ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)

                    [

                        SNew(SBox)
                        .Visibility(this, &STMFavoriteClassRow::GetEditOnlyVisibility)
                        [
                            BuildUnfavoriteXButton(false)
                        ]

                    ]

                ];

        }



        TSharedRef<SWidget> BuildCompactRowContent()

        {

            return SNew(SOverlay)

                + SOverlay::Slot()

                [

                    SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

                    .Padding(FMargin(4.0f, 8.0f, 4.0f, 6.0f))

                    .ToolTipText(this, &STMFavoriteClassRow::GetCompactTooltipText)

                    [

                        SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)

                        [

                            BuildClassIcon(22.0f)

                        ]

                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(1.0f, 4.0f, 1.0f, 0.0f)

                        [

                            SNew(STextBlock)

                            .Text(this, &STMFavoriteClassRow::GetDisplayNameText)

                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))

                            .Justification(ETextJustify::Center)

                            .AutoWrapText(true)

                            .ColorAndOpacity(FLinearColor(0.86f, 0.88f, 0.90f))

                        ]

                    ]

                ]

                + SOverlay::Slot()

                .HAlign(HAlign_Right)

                .VAlign(VAlign_Top)

                .Padding(FMargin(0.0f, 1.0f, 1.0f, 0.0f))

                [

                    SNew(SBox)
                    .Visibility(this, &STMFavoriteClassRow::GetEditOnlyVisibility)
                    [
                        BuildUnfavoriteXButton(true)
                    ]

                ];

        }


        TSharedRef<SWidget> BuildMicroRowContent()

        {

            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

                .Padding(FMargin(1.0f, 6.0f))

                .ToolTipText(this, &STMFavoriteClassRow::GetCompactTooltipText)

                [

                    SNew(SBox)

                    .MinDesiredWidth(24.0f)

                    .MinDesiredHeight(28.0f)

                    .HAlign(HAlign_Center)

                    .VAlign(VAlign_Center)

                    [

                        BuildClassIcon(20.0f)

                    ]

                ];

        }



        virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override

        {

            if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)

            {

                return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);

            }



            if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)

            {

                ShowContextMenu(MouseEvent.GetScreenSpacePosition());

                return FReply::Handled();

            }



            return STableRow<FFavoriteItemPtr>::OnMouseButtonDown(MyGeometry, MouseEvent);

        }



        virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override

        {

            if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && OpenFavoriteAsset(Item))

            {

                return FReply::Handled();

            }

            return STableRow<FFavoriteItemPtr>::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);

        }



        virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override

        {

            if (Item.IsValid() && Item->AssetData.IsValid())

            {

                return FReply::Handled().BeginDragDrop(FAssetDragDropOp::New(Item->AssetData));

            }

            return FReply::Unhandled();

        }



    private:

        int32 GetLayoutIndex() const

        {

            if (CachedRowWidth > 0.0f && CachedRowWidth < 72.0f)

            {

                return 2;

            }

            return CachedRowWidth > 0.0f && CachedRowWidth < 240.0f ? 1 : 0;

        }



        TSharedRef<SWidget> BuildClassIcon(float IconSize) const

        {

            return SNew(SBox)

                .WidthOverride(IconSize)

                .HeightOverride(IconSize)

                [

                    SNew(SImage)

                    .Image(this, &STMFavoriteClassRow::GetClassIconBrush)

                ];

        }



        TSharedRef<SWidget> BuildUnfavoriteXButton(bool bCompact)

        {

            return SNew(SButton)

                .ButtonStyle(FAppStyle::Get(), "SimpleButton")

                .ContentPadding(bCompact ? FMargin(3.0f, 0.0f) : FMargin(5.0f, 1.0f))

                .ToolTipText(TMLoc::Text(TEXT("Remove this entry from the favorites list. This does not delete actors or assets."), TEXT("이 항목을 즐겨찾기 목록에서 제거합니다. 액터나 에셋은 삭제하지 않습니다.")))

                .OnClicked(this, &STMFavoriteClassRow::OnRemoveClicked)

                [

                    SNew(STextBlock)

                    .Text(LOCTEXT("RemoveFavoriteButton", "X"))

                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", bCompact ? 13 : 11))

                    .ColorAndOpacity(FLinearColor(0.78f, 0.80f, 0.84f))

                ];

        }



        EVisibility GetEditOnlyVisibility() const
        {
            return IsEditMode.Get(false) ? EVisibility::Visible : EVisibility::Collapsed;
        }

        const FSlateBrush* GetClassIconBrush() const

        {

            if (Item.IsValid())

            {

                if (IsLevelAsset(Item->AssetData))

                {

                    if (const FSlateBrush* LevelIcon = FSlateIconFinder::FindIconBrushForClass(UWorld::StaticClass(), FName(TEXT("ClassIcon.World"))))

                    {

                        return LevelIcon;

                    }

                }



                if (UClass* ActorClass = FTMFavoriteClassItem::ResolveActorClass(Item->AssetData))

                {

                    if (const FSlateBrush* ClassIcon = FSlateIconFinder::FindIconBrushForClass(ActorClass, FName(TEXT("ClassIcon.Actor"))))

                    {

                        return ClassIcon;

                    }

                }



                if (UClass* AssetClass = Item->AssetData.GetClass())

                {

                    if (const FSlateBrush* AssetIcon = FSlateIconFinder::FindIconBrushForClass(AssetClass))

                    {

                        return AssetIcon;

                    }

                }

            }



            return FAppStyle::GetBrush("ClassIcon.Actor");

        }



        bool CanPlaceFavorite() const

        {

            return Item.IsValid() && Item->CanPlaceActor();

        }



        bool CanSyncFavoriteToContentBrowser() const

        {

            return Item.IsValid() && (Item->AssetData.IsValid() || !Item->Path.IsEmpty());

        }



        void ShowContextMenu(const FVector2D& ScreenSpacePosition)

        {

            FMenuBuilder MenuBuilder(true, nullptr);

            MenuBuilder.AddMenuEntry(

                TMLoc::Text(TEXT("Browse To Asset"), TEXT("에셋으로 이동")),

                TMLoc::Text(TEXT("Sync the Content Browser to this asset, matching the editor Ctrl+B behavior."), TEXT("콘텐츠 브라우저를 이 에셋 위치로 이동합니다. 에디터의 Ctrl+B 동작과 같습니다.")),

                FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"),

                FUIAction(

                    FExecuteAction::CreateSP(this, &STMFavoriteClassRow::OnSyncFavoriteToContentBrowser),

                    FCanExecuteAction::CreateSP(this, &STMFavoriteClassRow::CanSyncFavoriteToContentBrowser)));



            FSlateApplication::Get().PushMenu(

                SharedThis(this),

                FWidgetPath(),

                MenuBuilder.MakeWidget(),

                ScreenSpacePosition,

                FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

        }



        void OnSyncFavoriteToContentBrowser()

        {

            SyncFavoriteAssetToContentBrowser(Item);

        }



        FText GetCompactTooltipText() const

        {

            if (!Item.IsValid())

            {

                return FText::GetEmpty();

            }



            return FText::FromString(FString::Printf(TEXT("%s\n%s"), *Item->DisplayName, *Item->FolderPath));

        }



        FText GetDisplayNameText() const

        {

            return Item.IsValid() ? FText::FromString(Item->DisplayName) : FText::GetEmpty();

        }



        FText GetClassPathText() const

        {

            if (!Item.IsValid())

            {

                return FText::GetEmpty();

            }



            return FText::FromString(Item->FolderPath.IsEmpty() ? Item->Path : Item->FolderPath);

        }



        FReply OnPlaceClicked()

        {

            PlaceFavoriteActor(Item);

            return FReply::Handled();

        }



        FReply OnPlaceHereClicked()

        {

            PlaceFavoriteActorAtCurrentView(Item);

            return FReply::Handled();

        }



        FReply OnRemoveClicked()

        {

            if (Item.IsValid())

            {

                RemoveFavoriteAsset(Item->Path);

                OnRemoved.ExecuteIfBound();

            }

            return FReply::Handled();

        }



        FFavoriteItemPtr Item;

        FSimpleDelegate OnRemoved;
        TAttribute<bool> IsEditMode;

        float CachedRowWidth = 0.0f;

    };



    class STMClassFavoritesWidget : public SCompoundWidget

    {

    public:

        SLATE_BEGIN_ARGS(STMClassFavoritesWidget) {}

        SLATE_END_ARGS()



        void Construct(const FArguments& InArgs)

        {

            RefreshItems();



            ChildSlot

            [

                SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

                .Padding(10.0f)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                        [

                            SNew(STextBlock)

                            .Text(TMLoc::Text(TEXT("Asset & Level Favorites"), TEXT("에셋 및 레벨 즐겨찾기")))

                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                        [

                            SNew(SButton)

                            .ContentPadding(FMargin(8.0f, 3.0f))

                            .OnClicked(this, &STMClassFavoritesWidget::OnRefreshClicked)

                            [

                                SNew(STextBlock).Text(TMLoc::Text(TEXT("Refresh"), TEXT("새로고침")))

                            ]

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(SButton)
                            .ContentPadding(FMargin(8.0f, 3.0f))
                            .ToolTipText(TMLoc::Text(TEXT("Enable editing to remove favorites or reorder the list."), TEXT("편집을 켜면 즐겨찾기 삭제와 목록 순서를 변경할 수 있습니다.")))
                            .OnClicked(this, &STMClassFavoritesWidget::OnEditToggleClicked)
                            [
                                SNew(STextBlock)
                                .Text_Lambda([this]() { return TMLoc::Text(bEditMode ? TEXT("Done") : TEXT("Edit"), bEditMode ? TEXT("완료") : TEXT("편집")); })
                                .ColorAndOpacity_Lambda([this]() { return bEditMode ? FLinearColor(0.25f, 0.85f, 0.35f) : FLinearColor::White; })
                            ]
                        ]

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)

                    [

                        SNew(SSeparator)

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)

                    [

                        SNew(SSearchBox)

                        .HintText(TMLoc::Text(TEXT("Filter favorite assets and levels"), TEXT("즐겨찾기 에셋 및 레벨 필터")))

                        .OnTextChanged(this, &STMClassFavoritesWidget::OnFilterTextChanged)

                    ]

                    + SVerticalBox::Slot().FillHeight(1.0f)

                    [

                        SAssignNew(ListView, SListView<FFavoriteItemPtr>)

                        .ListItemsSource(&FilteredItems)

                        .SelectionMode(ESelectionMode::Single)

                        .OnGenerateRow(this, &STMClassFavoritesWidget::GenerateRow)

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)

                    [

                        SNew(STextBlock)

                        .Text(TMLoc::Text(TEXT("Double-click to open. Place adds at origin, Here adds at the current view, or drag into the level."), TEXT("더블클릭하면 엽니다. Place는 원점에, Here는 현재 뷰 위치에 추가하며, 드래그로 레벨에 배치할 수도 있습니다.")))

                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))

                        .ColorAndOpacity(FLinearColor(0.62f, 0.66f, 0.70f))

                    ]

                ]

            ];

        }



        void Refresh()

        {

            RefreshItems();

            ApplyFilter();

        }



    private:

        void RefreshItems()

        {

            AllItems = LoadFavoriteItems();

            ApplyFilter();

        }



        void ApplyFilter()

        {

            FilteredItems.Empty();

            const FString Filter = FilterText.ToString();



            for (const FFavoriteItemPtr& Item : AllItems)

            {

                if (!Item.IsValid())

                {

                    continue;

                }



                if (Filter.IsEmpty()

                    || Item->DisplayName.Contains(Filter)

                    || Item->ClassName.Contains(Filter)

                    || Item->Path.Contains(Filter))

                {

                    FilteredItems.Add(Item);

                }

            }



            if (ListView.IsValid())

            {

                ListView->RequestListRefresh();

            }

        }



        void OnFilterTextChanged(const FText& InFilterText)

        {

            FilterText = InFilterText;

            ApplyFilter();

        }



        FReply OnRefreshClicked()

        {

            Refresh();

            return FReply::Handled();

        }

        FReply OnEditToggleClicked()
        {
            bEditMode = !bEditMode;
            if (ListView.IsValid())
            {
                ListView->RequestListRefresh();
            }
            return FReply::Handled();
        }



        TSharedRef<ITableRow> GenerateRow(FFavoriteItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)

        {

            return SNew(STMFavoriteClassRow, OwnerTable)

                .Item(Item)
                .IsEditMode(TAttribute<bool>::CreateLambda([this]() { return bEditMode; }))

                .OnRemoved(FSimpleDelegate::CreateSP(this, &STMClassFavoritesWidget::Refresh))

                .OnCanAcceptDrop(STMFavoriteClassRow::FOnCanAcceptDrop::CreateSP(this, &STMClassFavoritesWidget::HandleCanAcceptFavoriteDrop))

                .OnAcceptDrop(STMFavoriteClassRow::FOnAcceptDrop::CreateSP(this, &STMClassFavoritesWidget::HandleAcceptFavoriteDrop));

        }



        TOptional<EItemDropZone> HandleCanAcceptFavoriteDrop(

            const FDragDropEvent& DragDropEvent,

            EItemDropZone DropZone,

            FFavoriteItemPtr TargetItem) const

        {

            const FString DraggedPath = GetDraggedFavoritePath(DragDropEvent);

            const int32 SourceIndex = FindAllItemIndexByPath(DraggedPath);

            const int32 TargetIndex = FindAllItemIndex(TargetItem);



            if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE || SourceIndex == TargetIndex)

            {

                return TOptional<EItemDropZone>();

            }



            return DropZone == EItemDropZone::OntoItem ? EItemDropZone::BelowItem : DropZone;

        }



        FReply HandleAcceptFavoriteDrop(

            const FDragDropEvent& DragDropEvent,

            EItemDropZone DropZone,

            FFavoriteItemPtr TargetItem)

        {

            const FString DraggedPath = GetDraggedFavoritePath(DragDropEvent);

            const int32 SourceIndex = FindAllItemIndexByPath(DraggedPath);

            int32 TargetIndex = FindAllItemIndex(TargetItem);



            if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE || SourceIndex == TargetIndex)

            {

                return FReply::Unhandled();

            }



            const EItemDropZone EffectiveDropZone = DropZone == EItemDropZone::OntoItem

                ? EItemDropZone::BelowItem

                : DropZone;



            FFavoriteItemPtr DraggedItem = AllItems[SourceIndex];

            AllItems.RemoveAt(SourceIndex);



            if (SourceIndex < TargetIndex)

            {

                --TargetIndex;

            }



            const int32 InsertIndex = EffectiveDropZone == EItemDropZone::AboveItem

                ? TargetIndex

                : TargetIndex + 1;



            AllItems.Insert(DraggedItem, FMath::Clamp(InsertIndex, 0, AllItems.Num()));

            SaveFavoriteItemsInOrder(AllItems);

            ApplyFilter();



            if (ListView.IsValid())

            {

                ListView->SetSelection(DraggedItem);

                ListView->RequestScrollIntoView(DraggedItem);

            }



            return FReply::Handled();

        }



        FString GetDraggedFavoritePath(const FDragDropEvent& DragDropEvent) const

        {

            TSharedPtr<FAssetDragDropOp> DragOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();

            if (!DragOperation.IsValid())

            {

                return FString();

            }



            const TArray<FAssetData>& DraggedAssets = DragOperation->GetAssets();

            if (!DraggedAssets.IsEmpty() && DraggedAssets[0].IsValid())

            {

                return DraggedAssets[0].GetSoftObjectPath().ToString();

            }



            const TArray<FString>& DraggedPaths = DragOperation->GetAssetPaths();

            return DraggedPaths.IsEmpty() ? FString() : DraggedPaths[0];

        }



        int32 FindAllItemIndex(FFavoriteItemPtr Item) const

        {

            return AllItems.IndexOfByPredicate([Item](const FFavoriteItemPtr& Candidate)

            {

                return Candidate == Item;

            });

        }



        int32 FindAllItemIndexByPath(const FString& AssetPath) const

        {

            if (AssetPath.IsEmpty())

            {

                return INDEX_NONE;

            }



            return AllItems.IndexOfByPredicate([&AssetPath](const FFavoriteItemPtr& Candidate)

            {

                return Candidate.IsValid()

                    && (Candidate->Path == AssetPath

                        || Candidate->AssetData.GetSoftObjectPath().ToString() == AssetPath

                        || Candidate->AssetData.GetObjectPathString() == AssetPath);

            });

        }



        TArray<FFavoriteItemPtr> AllItems;

        TArray<FFavoriteItemPtr> FilteredItems;

        TSharedPtr<SListView<FFavoriteItemPtr>> ListView;

        FText FilterText;
        bool bEditMode = false;

    };



    TWeakPtr<SDockTab> ExistingFavoritesTab;

    bool bClassFavoritesTabSpawnerRegistered = false;



    TSharedRef<SDockTab> SpawnClassFavoritesTab(const FSpawnTabArgs& Args)

    {

        TSharedRef<SDockTab> Tab = SNew(SDockTab)

            .TabRole(ETabRole::NomadTab)

            .Label(TMLoc::Text(TEXT("Asset & Level Favorites"), TEXT("에셋 및 레벨 즐겨찾기")))

            .OnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>)

            {

                ExistingFavoritesTab.Reset();

            }))

            [

                SNew(STMClassFavoritesWidget)

            ];



        ExistingFavoritesTab = Tab;

        return Tab;

    }



    void RegisterClassFavoritesTabSpawner()

    {

        if (bClassFavoritesTabSpawnerRegistered)

        {

            return;

        }



        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(

            ClassFavoritesTabId,

            FOnSpawnTab::CreateStatic(&SpawnClassFavoritesTab))

            .SetDisplayName(TMLoc::Text(TEXT("Asset & Level Favorites"), TEXT("에셋 및 레벨 즐겨찾기")))

            .SetTooltipText(TMLoc::Text(TEXT("Open placeable asset and level favorites."), TEXT("배치 가능한 에셋 및 레벨 즐겨찾기를 엽니다.")))

            .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), "TraceMotive.ClassFavorites"));



        bClassFavoritesTabSpawnerRegistered = true;

    }

}



namespace TMClassFavorites

{

    void RegisterMenus()

    {

        RegisterClassFavoritesTabSpawner();



        if (UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu"))

        {

            FToolMenuSection& Section = AssetMenu->FindOrAddSection("TraceMotive");

            Section.AddMenuEntry(

                "TMAddClassFavorite",

                TMLoc::Text(TEXT("Add to Asset & Level Favorites"), TEXT("에셋 및 레벨 즐겨찾기에 추가")),

                TMLoc::Text(TEXT("Add selected placeable assets, Actor classes, or Levels to favorites."), TEXT("선택한 배치 가능 에셋, 액터 클래스 또는 레벨을 즐겨찾기에 추가합니다.")),

                FSlateIcon(TMStyle::GetStyleSetName(), "TraceMotive.ClassFavorites"),

                FToolMenuExecuteAction::CreateStatic(&TMClassFavorites::AddSelectedAssetsFromContext)

            );



            Section.AddMenuEntry(

                "TMOpenClassFavorites",

                TMLoc::Text(TEXT("Open Asset & Level Favorites"), TEXT("에셋 및 레벨 즐겨찾기 열기")),

                TMLoc::Text(TEXT("Open the placeable asset and level favorites window."), TEXT("배치 가능한 에셋 및 레벨 즐겨찾기 창을 엽니다.")),

                FSlateIcon(TMStyle::GetStyleSetName(), "TraceMotive.ClassFavorites"),

                FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&)

                {

                    TMClassFavorites::OpenFavoritesWindow();

                })

            );

        }



        if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))

        {

            FToolMenuSection& Section = WindowMenu->FindOrAddSection("TraceMotive");

            Section.AddMenuEntry(

                "TMOpenClassFavoritesWindow",

                TMLoc::Text(TEXT("Asset & Level Favorites"), TEXT("에셋 및 레벨 즐겨찾기")),

                TMLoc::Text(TEXT("Open the asset and level favorites window."), TEXT("에셋 및 레벨 즐겨찾기 창을 엽니다.")),

                FSlateIcon(TMStyle::GetStyleSetName(), "TraceMotive.ClassFavorites"),

                FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&)

                {

                    TMClassFavorites::OpenFavoritesWindow();

                })

            );

        }

    }



    void UnregisterMenus()

    {

        if (bClassFavoritesTabSpawnerRegistered)

        {

            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ClassFavoritesTabId);

            bClassFavoritesTabSpawnerRegistered = false;

        }



        ExistingFavoritesTab.Reset();

    }



    void AddSelectedAssetsFromContext(const FToolMenuContext& Context)

    {

        const TArray<FAssetData> SelectedAssets = GetSelectedContentBrowserAssets(Context);



        int32 AddedCount = 0;

        for (const FAssetData& AssetData : SelectedAssets)

        {

            if (AddFavoriteAsset(AssetData))

            {

                AddedCount++;

            }

        }



        if (AddedCount == 0)

        {

            UE_LOG(LogTraceMotive, Warning, TEXT("Asset & Level Favorites: no selected placeable assets, Actor classes, or Levels were added."));

        }



        OpenFavoritesWindowDeferred();

    }



    void OpenFavoritesWindow()

    {

        RegisterClassFavoritesTabSpawner();



        if (TSharedPtr<SDockTab> ExistingTab = ExistingFavoritesTab.Pin())

        {

            FGlobalTabmanager::Get()->TryInvokeTab(ClassFavoritesTabId);

            return;

        }



        ExistingFavoritesTab = FGlobalTabmanager::Get()->TryInvokeTab(ClassFavoritesTabId);

    }

}



#undef LOCTEXT_NAMESPACE
