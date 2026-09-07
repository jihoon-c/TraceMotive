#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/CPBTypes.h"
#include "CPBUserWidgetBase.generated.h"

class UMaterialInstanceDynamic;
class UCPBPlatformSubsystem;
class UCPBUISubsystem;
class UWidgetComponent;

UCLASS(Abstract, Blueprintable)
class CPPBUILDERC_API UCPBUserWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	void AddToViewport(int32 ZOrder = 0);
	bool AddToPlayerScreen(int32 ZOrder = 0);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void AddToCPBViewport(int32 ZOrder = 0);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "CPB|UI")
	void SetupWidgetRenderMode();
	virtual void SetupWidgetRenderMode_Implementation();

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void SetWorldWidgetComponent(UWidgetComponent* InWidgetComponent);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void SetTargetRenderMaterial(UMaterialInstanceDynamic* InTargetRenderMaterial);

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	ECPBWidgetRenderMode GetWidgetRenderMode() const { return WidgetRenderMode; }

protected:
	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	bool IsViewportPresentationAllowed() const;

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	bool IsVRPlatformActive() const;

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	UCPBUISubsystem* GetCPBUISubsystem() const;

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	UCPBPlatformSubsystem* GetCPBPlatformSubsystem() const;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "CPB|UI")
	ECPBWidgetRenderMode WidgetRenderMode = ECPBWidgetRenderMode::Viewport;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "CPB|UI")
	TObjectPtr<UWidgetComponent> WorldWidgetComponent;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "CPB|UI")
	TObjectPtr<UMaterialInstanceDynamic> TargetRenderMaterial;
};
