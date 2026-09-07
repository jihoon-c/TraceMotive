#pragma once



#include "Containers/Ticker.h"

#include "CoreMinimal.h"



class SInstanceReferenceTracker;

class UActorComponent;

class UObject;

class UFunction;

struct FPropertyChangedEvent;



#if DO_BLUEPRINT_GUARD

struct FBlueprintContextTracker;

#endif



class FInstanceTraceGlobalManager

{

public:

    static FInstanceTraceGlobalManager& Get();



    ~FInstanceTraceGlobalManager();



    FInstanceTraceGlobalManager(const FInstanceTraceGlobalManager&) = delete;

    FInstanceTraceGlobalManager& operator=(const FInstanceTraceGlobalManager&) = delete;



    void AddSubscriber(const TSharedRef<SInstanceReferenceTracker>& Subscriber);

    void RemoveSubscriber(const SInstanceReferenceTracker* Subscriber);



private:

    FInstanceTraceGlobalManager() = default;



    void RegisterGlobalHooksIfNeeded();

    void UnregisterGlobalHooks();

    void PruneSubscribers();

    void ForEachSubscriber(TFunctionRef<void(SInstanceReferenceTracker&)> Callback);

    bool TickSubscribers(float InDeltaTime);



    void HandlePreBeginPIE(bool bIsSimulating);

    void HandlePostPIEStarted(bool bIsSimulating);

    void HandleEndPIE(bool bIsSimulating);

    void HandleRenderStateDirty(UActorComponent& Component);

    void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);



#if DO_BLUEPRINT_GUARD

    void HandleBlueprintScriptEnter(const FBlueprintContextTracker& ContextTracker, const UObject* ContextObject, const UFunction* ContextFunction);

    void HandleBlueprintScriptExit(const FBlueprintContextTracker& ContextTracker);

#endif



    TArray<TWeakPtr<SInstanceReferenceTracker>> Subscribers;

    FDelegateHandle PreBeginPIEHandle;

    FDelegateHandle PostPIEStartedHandle;

    FDelegateHandle EndPIEHandle;

    FDelegateHandle MarkRenderStateDirtyHandle;

    FDelegateHandle ObjectPropertyChangedHandle;

#if DO_BLUEPRINT_GUARD

    FDelegateHandle BlueprintScriptEnterHandle;

    FDelegateHandle BlueprintScriptExitHandle;

#endif

    FTSTicker::FDelegateHandle CoreTickerHandle;

    bool bHooksRegistered = false;

};

