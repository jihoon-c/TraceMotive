#include "TMInstanceTraceGlobalManager.h"
#include "TMEngineCompatibility.h"



#include "SInstanceReferenceTracker.h"



#include "Components/ActorComponent.h"

#include "Editor.h"

#include "UObject/Script.h"

#include "UObject/UObjectGlobals.h"

#include "UObject/UnrealType.h"



FInstanceTraceGlobalManager& FInstanceTraceGlobalManager::Get()

{

    static FInstanceTraceGlobalManager Instance;

    return Instance;

}



FInstanceTraceGlobalManager::~FInstanceTraceGlobalManager()

{

    UnregisterGlobalHooks();

}



void FInstanceTraceGlobalManager::AddSubscriber(const TSharedRef<SInstanceReferenceTracker>& Subscriber)

{

    PruneSubscribers();



    for (const TWeakPtr<SInstanceReferenceTracker>& WeakSubscriber : Subscribers)

    {

        const TSharedPtr<SInstanceReferenceTracker> ExistingSubscriber = WeakSubscriber.Pin();

        if (ExistingSubscriber.Get() == &Subscriber.Get())

        {

            RegisterGlobalHooksIfNeeded();

            return;

        }

    }



    Subscribers.Add(Subscriber);

    RegisterGlobalHooksIfNeeded();

}



void FInstanceTraceGlobalManager::RemoveSubscriber(const SInstanceReferenceTracker* Subscriber)

{

    Subscribers.RemoveAll([Subscriber](const TWeakPtr<SInstanceReferenceTracker>& WeakSubscriber)

    {

        const TSharedPtr<SInstanceReferenceTracker> ExistingSubscriber = WeakSubscriber.Pin();

        return !ExistingSubscriber.IsValid() || ExistingSubscriber.Get() == Subscriber;

    });



    if (Subscribers.Num() == 0)

    {

        UnregisterGlobalHooks();

    }

}



void FInstanceTraceGlobalManager::RegisterGlobalHooksIfNeeded()

{

    if (bHooksRegistered)

    {

        return;

    }



    PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(this, &FInstanceTraceGlobalManager::HandlePreBeginPIE);

    PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &FInstanceTraceGlobalManager::HandlePostPIEStarted);

    EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FInstanceTraceGlobalManager::HandleEndPIE);

    MarkRenderStateDirtyHandle = UActorComponent::MarkRenderStateDirtyEvent.AddRaw(this, &FInstanceTraceGlobalManager::HandleRenderStateDirty);

    ObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FInstanceTraceGlobalManager::HandleObjectPropertyChanged);

#if DO_BLUEPRINT_GUARD

    BlueprintScriptEnterHandle = FBlueprintContextTracker::OnEnterScriptContext.AddRaw(this, &FInstanceTraceGlobalManager::HandleBlueprintScriptEnter);

    BlueprintScriptExitHandle = FBlueprintContextTracker::OnExitScriptContext.AddRaw(this, &FInstanceTraceGlobalManager::HandleBlueprintScriptExit);

#endif

    CoreTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FInstanceTraceGlobalManager::TickSubscribers), 0.05f);

    bHooksRegistered = true;

}



void FInstanceTraceGlobalManager::UnregisterGlobalHooks()

{

    if (!bHooksRegistered)

    {

        return;

    }



    if (PreBeginPIEHandle.IsValid())

    {

        FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);

        PreBeginPIEHandle.Reset();

    }



    if (PostPIEStartedHandle.IsValid())

    {

        FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);

        PostPIEStartedHandle.Reset();

    }



    if (EndPIEHandle.IsValid())

    {

        FEditorDelegates::EndPIE.Remove(EndPIEHandle);

        EndPIEHandle.Reset();

    }



    if (MarkRenderStateDirtyHandle.IsValid())

    {

        UActorComponent::MarkRenderStateDirtyEvent.Remove(MarkRenderStateDirtyHandle);

        MarkRenderStateDirtyHandle.Reset();

    }



    if (ObjectPropertyChangedHandle.IsValid())

    {

        FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectPropertyChangedHandle);

        ObjectPropertyChangedHandle.Reset();

    }



#if DO_BLUEPRINT_GUARD

    if (BlueprintScriptEnterHandle.IsValid())

    {

        FBlueprintContextTracker::OnEnterScriptContext.Remove(BlueprintScriptEnterHandle);

        BlueprintScriptEnterHandle.Reset();

    }



    if (BlueprintScriptExitHandle.IsValid())

    {

        FBlueprintContextTracker::OnExitScriptContext.Remove(BlueprintScriptExitHandle);

        BlueprintScriptExitHandle.Reset();

    }

#endif



    if (CoreTickerHandle.IsValid())

    {

        FTSTicker::GetCoreTicker().RemoveTicker(CoreTickerHandle);

        CoreTickerHandle.Reset();

    }



    bHooksRegistered = false;

}



void FInstanceTraceGlobalManager::PruneSubscribers()

{

    Subscribers.RemoveAll([](const TWeakPtr<SInstanceReferenceTracker>& WeakSubscriber)

    {

        return !WeakSubscriber.IsValid();

    });

}



void FInstanceTraceGlobalManager::ForEachSubscriber(TFunctionRef<void(SInstanceReferenceTracker&)> Callback)

{

    TArray<TSharedRef<SInstanceReferenceTracker>> LiveSubscribers;

    for (int32 Index = Subscribers.Num() - 1; Index >= 0; --Index)

    {

        const TSharedPtr<SInstanceReferenceTracker> Subscriber = Subscribers[Index].Pin();

        if (!Subscriber.IsValid())

        {

            TMEngineCompatibility::RemoveAtSwapNoShrink(Subscribers, Index);

            continue;

        }



        LiveSubscribers.Add(Subscriber.ToSharedRef());

    }



    for (const TSharedRef<SInstanceReferenceTracker>& Subscriber : LiveSubscribers)

    {

        Callback(Subscriber.Get());

    }



    if (Subscribers.Num() == 0)

    {

        UnregisterGlobalHooks();

    }

}



bool FInstanceTraceGlobalManager::TickSubscribers(float InDeltaTime)

{

    ForEachSubscriber([InDeltaTime](SInstanceReferenceTracker& Subscriber)

    {

        Subscriber.RunTrackerTick(InDeltaTime);

    });



    return Subscribers.Num() > 0;

}



void FInstanceTraceGlobalManager::HandlePreBeginPIE(bool bIsSimulating)

{

    ForEachSubscriber([bIsSimulating](SInstanceReferenceTracker& Subscriber)

    {

        Subscriber.HandlePreBeginPIE(bIsSimulating);

    });

}



void FInstanceTraceGlobalManager::HandlePostPIEStarted(bool bIsSimulating)

{

    ForEachSubscriber([bIsSimulating](SInstanceReferenceTracker& Subscriber)

    {

        Subscriber.HandlePostPIEStarted(bIsSimulating);

    });

}



void FInstanceTraceGlobalManager::HandleEndPIE(bool bIsSimulating)

{

    ForEachSubscriber([bIsSimulating](SInstanceReferenceTracker& Subscriber)

    {

        Subscriber.HandleEndPIE(bIsSimulating);

    });

}



void FInstanceTraceGlobalManager::HandleRenderStateDirty(UActorComponent& Component)

{

    ForEachSubscriber([&Component](SInstanceReferenceTracker& Subscriber)

    {

        Subscriber.HandleRenderStateDirty(Component);

    });

}



void FInstanceTraceGlobalManager::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)

{

    ForEachSubscriber([Object, &PropertyChangedEvent](SInstanceReferenceTracker& Subscriber)

    {

        Subscriber.HandleObjectPropertyChanged(Object, PropertyChangedEvent);

    });

}



#if DO_BLUEPRINT_GUARD

void FInstanceTraceGlobalManager::HandleBlueprintScriptEnter(const FBlueprintContextTracker& ContextTracker, const UObject* ContextObject, const UFunction* ContextFunction)

{

    ForEachSubscriber([&ContextTracker, ContextObject, ContextFunction](SInstanceReferenceTracker& Subscriber)

    {

        Subscriber.HandleBlueprintScriptEnter(ContextTracker, ContextObject, ContextFunction);

    });

}



void FInstanceTraceGlobalManager::HandleBlueprintScriptExit(const FBlueprintContextTracker& ContextTracker)

{

    ForEachSubscriber([&ContextTracker](SInstanceReferenceTracker& Subscriber)

    {

        Subscriber.HandleBlueprintScriptExit(ContextTracker);

    });

}

#endif

