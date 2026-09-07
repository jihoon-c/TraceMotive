#include "TraceMotiveFeatureLab.h"

#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/Border.h"
#include "Components/BoxComponent.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTraceMotiveFeatureLab, Log, All);

TSharedRef<SWidget> UTraceMotiveLabWidget::RebuildWidget()
{
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("FeatureLabWidgetTree"));
    }

    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FeatureLabCanvas"));
    WidgetTree->RootWidget = Canvas;

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FeatureLabPanel"));
    Panel->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.055f, 0.94f));
    UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(0.02f, 0.03f, 0.34f, 0.34f));
    PanelSlot->SetOffsets(FMargin(0.0f));

    UCanvasPanel* PanelCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FeatureLabPanelCanvas"));
    Panel->SetContent(PanelCanvas);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FeatureLabTitle"));
    Title->SetText(FText::FromString(TEXT("TraceMotive Feature Lab")));
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.15f, 0.75f, 1.0f)));
    UCanvasPanelSlot* TitleSlot = PanelCanvas->AddChildToCanvas(Title);
    TitleSlot->SetPosition(FVector2D(18.0f, 14.0f));
    TitleSlot->SetSize(FVector2D(320.0f, 30.0f));

    StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LifecycleStatusText"));
    StatusText->SetText(FText::FromString(TEXT("Widget lifecycle: Visible")));
    UCanvasPanelSlot* StatusSlot = PanelCanvas->AddChildToCanvas(StatusText);
    StatusSlot->SetPosition(FVector2D(18.0f, 54.0f));
    StatusSlot->SetSize(FVector2D(320.0f, 28.0f));

    DemoButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("FeatureLabClickButton"));
    UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FeatureLabButtonText"));
    ButtonText->SetText(FText::FromString(TEXT("Click flow test")));
    DemoButton->SetContent(ButtonText);
    UCanvasPanelSlot* ButtonSlot = PanelCanvas->AddChildToCanvas(DemoButton);
    ButtonSlot->SetPosition(FVector2D(18.0f, 94.0f));
    ButtonSlot->SetSize(FVector2D(210.0f, 42.0f));

    return Super::RebuildWidget();
}

void UTraceMotiveLabWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (DemoButton)
    {
        DemoButton->OnClicked.AddUniqueDynamic(this, &UTraceMotiveLabWidget::HandleDemoButtonClicked);
    }
    UE_LOG(LogTraceMotiveFeatureLab, Display, TEXT("TM_DEMO_WIDGET_CREATED Owner=%s"), *GetName());
}

void UTraceMotiveLabWidget::HandleDemoButtonClicked()
{
    ++ClickCount;
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("OnClicked received: %d"), ClickCount)));
    }
    UE_LOG(LogTraceMotiveFeatureLab, Display, TEXT("TM_DEMO_WIDGET_CLICK OnClicked Count=%d"), ClickCount);
}

void UTraceMotiveLabWidget::PulseLifecycle()
{
    bChildVisible = !bChildVisible;
    if (StatusText)
    {
        StatusText->SetVisibility(bChildVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    UE_LOG(LogTraceMotiveFeatureLab, Display, TEXT("TM_DEMO_WIDGET_VISIBILITY %s"),
        bChildVisible ? TEXT("Collapsed->Visible") : TEXT("Visible->Collapsed"));
}

ATraceMotiveFeatureLabActor::ATraceMotiveFeatureLabActor()
{
    PrimaryActorTick.bCanEverTick = true;

    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    SetRootComponent(InteractionBox);
    InteractionBox->SetBoxExtent(FVector(110.0f, 110.0f, 110.0f));
    InteractionBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    DemoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DemoMesh"));
    DemoMesh->SetupAttachment(InteractionBox);
    DemoMesh->SetRelativeScale3D(FVector(2.0f, 2.0f, 2.0f));
    DemoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FeatureLabel"));
    Label->SetupAttachment(InteractionBox);
    Label->SetRelativeLocation(FVector(0.0f, 0.0f, 155.0f));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(32.0f);
    Label->SetTextRenderColor(FColor(80, 205, 255));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        DemoMesh->SetStaticMesh(CubeMesh.Object);
    }
}

void ATraceMotiveFeatureLabActor::BeginPlay()
{
    Super::BeginPlay();
    InitialLocation = GetActorLocation();
    UpdateVisuals();

    switch (Scenario)
    {
    case ETMFeatureLabScenario::WidgetLifecycle:
        CreateLifecycleWidget();
        GetWorldTimerManager().SetTimer(ScenarioTimer, this, &ATraceMotiveFeatureLabActor::RunScenarioStep, 2.5f, true, 1.0f);
        break;
    case ETMFeatureLabScenario::CollisionMover:
    case ETMFeatureLabScenario::InstanceController:
        GetWorldTimerManager().SetTimer(ScenarioTimer, this, &ATraceMotiveFeatureLabActor::RunScenarioStep, 2.0f, true, 1.0f);
        break;
    case ETMFeatureLabScenario::AudioSource:
        GetWorldTimerManager().SetTimer(ScenarioTimer, this, &ATraceMotiveFeatureLabActor::PlayDemoSound, 3.0f, true, 0.75f);
        break;
    case ETMFeatureLabScenario::RuntimeError:
        GetWorldTimerManager().SetTimer(ScenarioTimer, this, &ATraceMotiveFeatureLabActor::EmitDeliberateBlueprintStyleError, 6.0f, true, 2.0f);
        break;
    default:
        break;
    }
}

void ATraceMotiveFeatureLabActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateVisuals();
}

void ATraceMotiveFeatureLabActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Scenario != ETMFeatureLabScenario::SpeedTarget)
    {
        return;
    }

    AddActorLocalRotation(FRotator(0.0f, 55.0f * DeltaSeconds, 0.0f));
    SpeedDemoElapsed += DeltaSeconds;
    if (SpeedDemoElapsed >= 8.0f)
    {
        SpeedDemoElapsed = -100000.0f;
        UE_LOG(LogTraceMotiveFeatureLab, Display, TEXT("TM_DEMO_SPEED_TARGET_REACHED BossPhase2"));
    }
}

void ATraceMotiveFeatureLabActor::NotifyActorOnClicked(FKey ButtonPressed)
{
    Super::NotifyActorOnClicked(ButtonPressed);
    DiagnosticState = FString::Printf(TEXT("Clicked with %s"), *ButtonPressed.ToString());
    UE_LOG(LogTraceMotiveFeatureLab, Display, TEXT("TM_DEMO_ACTOR_CLICK Actor=%s Key=%s"),
        *GetName(), *ButtonPressed.ToString());
}

void ATraceMotiveFeatureLabActor::OpenSecurityDoor()
{
    DiagnosticState = TEXT("Door opened");
    SetActorLocation(GetActorLocation() + FVector(0.0f, 0.0f, 120.0f));
    UE_LOG(LogTraceMotiveFeatureLab, Display, TEXT("TM_DEMO_CALL_CHAIN OpenSecurityDoor Actor=%s"), *GetName());
}

void ATraceMotiveFeatureLabActor::ActivateFromKeypad()
{
    UE_LOG(LogTraceMotiveFeatureLab, Display, TEXT("TM_DEMO_CALL_CHAIN ActivateFromKeypad -> OpenSecurityDoor"));
    OpenSecurityDoor();
}

void ATraceMotiveFeatureLabActor::RunScenarioStep()
{
    ++ScenarioStep;
    if (Scenario == ETMFeatureLabScenario::CollisionMover)
    {
        MoveCollisionDemo();
    }
    else if (Scenario == ETMFeatureLabScenario::InstanceController)
    {
        ToggleReferencedTarget();
    }
    else if (Scenario == ETMFeatureLabScenario::WidgetLifecycle && DemoWidget)
    {
        DemoWidget->PulseLifecycle();
    }
}

void ATraceMotiveFeatureLabActor::MoveCollisionDemo()
{
    const FVector Destination = InitialLocation + FVector(0.0f, (ScenarioStep % 2 == 0) ? 0.0f : 480.0f, 0.0f);
    FHitResult Hit;
    SetActorLocation(Destination, true, &Hit, ETeleportType::None);
    UE_LOG(LogTraceMotiveFeatureLab, Display, TEXT("TM_DEMO_COLLISION Sweep=1 BlockingHit=%d Other=%s"),
        Hit.bBlockingHit ? 1 : 0, Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("None"));
}

void ATraceMotiveFeatureLabActor::ToggleReferencedTarget()
{
    if (!IsValid(TargetActor))
    {
        for (TActorIterator<ATraceMotiveFeatureLabActor> It(GetWorld()); It; ++It)
        {
            if (It->Scenario == ETMFeatureLabScenario::InstanceTarget)
            {
                TargetActor = *It;
                break;
            }
        }
    }

    ATraceMotiveFeatureLabActor* Target = Cast<ATraceMotiveFeatureLabActor>(TargetActor);
    if (!Target)
    {
        return;
    }

    const bool bEnable = ScenarioStep % 2 == 0;
    Target->InteractionBox->SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Target->DemoMesh->SetVisibility(bEnable, true);
    Target->DiagnosticState = bEnable ? TEXT("Enabled by BP_TM_ReferenceController") : TEXT("Disabled by BP_TM_ReferenceController");
    UE_LOG(LogTraceMotiveFeatureLab, Display, TEXT("TM_DEMO_REFERENCE_CHANGE Target=%s Collision=%s Visibility=%s"),
        *Target->GetName(), bEnable ? TEXT("QueryAndPhysics") : TEXT("NoCollision"), bEnable ? TEXT("Visible") : TEXT("Hidden"));
}

void ATraceMotiveFeatureLabActor::EmitDeliberateBlueprintStyleError()
{
    UE_LOG(LogTraceMotiveFeatureLab, Error,
        TEXT("Blueprint Runtime Error: \"Accessed None trying to read property MissingDemoTarget\". Node: TM_Demo_GetTarget Graph: EventGraph Function: ExecuteUbergraph_BP_TM_DeliberateError Blueprint: BP_TM_DeliberateError"));
}

void ATraceMotiveFeatureLabActor::PlayDemoSound()
{
    if (DemoSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, DemoSound, GetActorLocation());
        UE_LOG(LogTraceMotiveFeatureLab, Display, TEXT("TM_DEMO_AUDIO_PLAY Sound=%s Owner=%s"),
            *DemoSound->GetName(), *GetName());
    }
}

void ATraceMotiveFeatureLabActor::CreateLifecycleWidget()
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        DemoWidget = CreateWidget<UTraceMotiveLabWidget>(PC, UTraceMotiveLabWidget::StaticClass());
        if (DemoWidget)
        {
            DemoWidget->AddToViewport(10);
        }
    }
}

void ATraceMotiveFeatureLabActor::UpdateVisuals()
{
    if (!Label)
    {
        return;
    }

    const UEnum* ScenarioEnum = StaticEnum<ETMFeatureLabScenario>();
    Label->SetText(FText::Format(
        NSLOCTEXT("TraceMotiveFeatureLab", "StationLabel", "{0}  {1}"),
        FText::AsNumber(static_cast<int32>(Scenario)),
        FText::FromName(FName(*ScenarioEnum->GetNameStringByValue(static_cast<int64>(Scenario))))));
    if (DemoMaterial)
    {
        DemoMesh->SetMaterial(0, DemoMaterial);
    }
}

void ATraceMotiveLabPlayerController::BeginPlay()
{
    Super::BeginPlay();
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableTouchEvents = true;
    DefaultMouseCursor = EMouseCursor::Crosshairs;
    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
}

ATraceMotiveLabGameMode::ATraceMotiveLabGameMode()
{
    PlayerControllerClass = ATraceMotiveLabPlayerController::StaticClass();
    DefaultPawnClass = ADefaultPawn::StaticClass();
}
