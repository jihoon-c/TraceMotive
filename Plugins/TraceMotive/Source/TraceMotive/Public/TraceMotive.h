// Source/TraceMotive/Public/TraceMotive.h

#pragma once

#include "CoreMinimal.h"

#include "Modules/ModuleManager.h"

#include "EdGraphUtilities.h"



class FTraceMotiveModule : public IModuleInterface

{

public:

    virtual void StartupModule() override;

    virtual void ShutdownModule() override;



private:

    void RegisterMenus();



    // 변수 참조 찾기

    void OnVisualFindReferences(const struct FToolMenuContext& Context);



    // 함수 참조 찾기 (새로 추가)

    void OnVisualFindFunctionReferences(const struct FToolMenuContext& Context);



    // 레벨에 배치된 특정 액터 인스턴스의 라이브 참조 추적

    void OnTraceSelectedInstanceReferences(const struct FToolMenuContext& Context);



    // 선택된 액터 인스턴스 전체를 현재 레벨 뷰포트 카메라 위치로 이동

    void OnMoveSelectedActorsToViewLocation(const struct FToolMenuContext& Context);



    // 선택된 하위 씬 컴포넌트만 현재 레벨 뷰포트 카메라 위치로 이동

    void OnMoveSelectedComponentsToViewLocation(const struct FToolMenuContext& Context);



    // 통합 뷰어 (4번째 인자로 함수/변수 구분)

    void OpenVisualReferenceViewer(

        class UBlueprint* InBlueprint,

        FName InName,

        const class UEdGraphNode* InSourceNode = nullptr,

        bool bIsFunction = false,

        class UClass* InTargetOwnerClass = nullptr,

        bool bIsDispatcher = false

    );



    TSharedPtr<FGraphPanelNodeFactory> VisualRefNodeFactory;

};
