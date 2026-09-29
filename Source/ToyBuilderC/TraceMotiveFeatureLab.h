#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "TraceMotiveFeatureLab.generated.h"

class UAudioComponent;
class UBoxComponent;
class UButton;
class UStaticMeshComponent;
class UTextBlock;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ETMFeatureLabScenario : uint8
{
    Hub,
    SearchAndReferences,
    AssetUsage,
    CollisionMover,
    CollisionObstacle,
    ClickTarget,
    AudioSource,
    WidgetLifecycle,
    InstanceTarget,
    InstanceController,
    RuntimeError,
    SpeedTarget,
    ViewportHelper
};

UCLASS()
class TOYBUILDERC_API UTraceMotiveLabWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="TraceMotive Feature Lab")
    void PulseLifecycle();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

private:
    UFUNCTION()
    void HandleDemoButtonClicked();

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UButton> DemoButton;

    int32 ClickCount = 0;
    bool bChildVisible = true;
};

UCLASS(BlueprintType)
class TOYBUILDERC_API ATraceMotiveFeatureLabActor : public AActor
{
    GENERATED_BODY()

public:
    ATraceMotiveFeatureLabActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void NotifyActorOnClicked(FKey ButtonPressed) override;

    UFUNCTION(BlueprintCallable, Category="TraceMotive Feature Lab")
    void OpenSecurityDoor();

    UFUNCTION(BlueprintCallable, Category="TraceMotive Feature Lab")
    void ActivateFromKeypad();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TraceMotive Feature Lab")
    ETMFeatureLabScenario Scenario = ETMFeatureLabScenario::Hub;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TraceMotive Feature Lab|Search")
    FString DoorType = TEXT("Security");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TraceMotive Feature Lab|Search")
    FString DiagnosticState = TEXT("Ready");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TraceMotive Feature Lab|Search")
    TArray<TObjectPtr<AActor>> RelatedActors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TraceMotive Feature Lab|References")
    TObjectPtr<AActor> TargetActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TraceMotive Feature Lab|Assets")
    TObjectPtr<UMaterialInterface> DemoMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TraceMotive Feature Lab|Audio")
    TObjectPtr<USoundBase> DemoSound;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TraceMotive Feature Lab")
    TObjectPtr<UStaticMeshComponent> DemoMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TraceMotive Feature Lab")
    TObjectPtr<UBoxComponent> InteractionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TraceMotive Feature Lab")
    TObjectPtr<UTextRenderComponent> Label;

protected:
    virtual void BeginPlay() override;

private:
    void RunScenarioStep();
    void MoveCollisionDemo();
    void ToggleReferencedTarget();
    void EmitDeliberateBlueprintStyleError();
    void PlayDemoSound();
    void CreateLifecycleWidget();
    void UpdateVisuals();

    UPROPERTY(Transient)
    TObjectPtr<UTraceMotiveLabWidget> DemoWidget;

    FTimerHandle ScenarioTimer;
    FVector InitialLocation = FVector::ZeroVector;
    int32 ScenarioStep = 0;
    float SpeedDemoElapsed = 0.0f;
};

UCLASS()
class TOYBUILDERC_API ATraceMotiveLabPlayerController : public APlayerController
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;
};

UCLASS()
class TOYBUILDERC_API ATraceMotiveLabGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ATraceMotiveLabGameMode();
};
