#include "TMCollisionPairAnalyzer.h"
#include "TMInvestigationSession.h"
#include "TMStyle.h"



#include "TMLocalization.h"

#include "TMReportFormatter.h"



#include "Algo/Count.h"

#include "CollisionQueryParams.h"

#include "Components/ActorComponent.h"

#include "Components/PrimitiveComponent.h"

#include "EdGraph/EdGraph.h"

#include "EdGraph/EdGraphNode.h"

#include "EdGraph/EdGraphPin.h"

#include "Editor.h"

#include "Engine/Engine.h"

#include "Engine/CollisionProfile.h"

#include "Engine/HitResult.h"

#include "Engine/Blueprint.h"

#include "Engine/BlueprintGeneratedClass.h"

#include "Engine/Selection.h"

#include "Engine/World.h"

#include "EngineUtils.h"

#include "DragAndDrop/ActorDragDropOp.h"

#include "Framework/Application/SlateApplication.h"

#include "Framework/Docking/TabManager.h"

#include "K2Node_CallFunction.h"

#include "Kismet2/KismetEditorUtilities.h"

#include "GameFramework/Actor.h"

#include "GameFramework/Pawn.h"

#include "HAL/PlatformApplicationMisc.h"

#include "LevelEditorMenuContext.h"

#include "PhysicsEngine/AggregateGeom.h"

#include "PhysicsEngine/BodySetup.h"

#include "SDropTarget.h"

#include "Styling/AppStyle.h"

#include "ToolMenus.h"

#include "Widgets/Docking/SDockTab.h"

#include "Widgets/SCompoundWidget.h"

#include "Widgets/Images/SImage.h"

#include "Widgets/Input/SButton.h"

#include "Widgets/Layout/SBorder.h"

#include "Widgets/Layout/SBox.h"

#include "Widgets/Layout/SExpandableArea.h"

#include "Widgets/Layout/SScrollBox.h"

#include "Widgets/Layout/SSeparator.h"

#include "Widgets/SBoxPanel.h"

#include "Widgets/Text/STextBlock.h"



#define LOCTEXT_NAMESPACE "TMCollisionPairAnalyzer"



namespace

{

    const FName CollisionAnalyzerTabId(TEXT("TraceMotive.CollisionPairAnalyzer"));



    enum class ECollisionFindingSeverity : uint8

    {

        Cause,

        Warning,

        Pass,

        Info

    };



    struct FCollisionFinding

    {

        ECollisionFindingSeverity Severity = ECollisionFindingSeverity::Info;

        FString Title;

        FString Detail;

    };



    struct FCollisionPairReport

    {

        TWeakObjectPtr<UPrimitiveComponent> ComponentA;

        TWeakObjectPtr<UPrimitiveComponent> ComponentB;

        FString ComponentALabel;

        FString ComponentBLabel;

        FString ProfileA;

        FString ProfileB;

        FString ObjectTypeA;

        FString ObjectTypeB;

        FString ResponseAtoB;

        FString ResponseBtoA;

        FString EnabledA;

        FString EnabledB;

        bool bBlockConfigured = false;

        bool bMoveIgnored = false;

        bool bBoundsOverlap = false;

        bool bShapeOverlap = false;

        bool bShapeTestPerformed = false;

        bool bSweepTestPerformed = false;

        bool bSweepBlockingHit = false;

        TArray<FCollisionFinding> Findings;

    };



    struct FCollisionAnalysisResult

    {

        FString ActorAName;

        FString ActorBName;

        FString WorldName;

        FString Summary;

        TArray<FCollisionFinding> ActorFindings;

        TArray<FCollisionPairReport> PairReports;

        int32 PrimitiveCountA = 0;

        int32 PrimitiveCountB = 0;

        int32 BlockConfiguredPairCount = 0;

        int32 ShapeOverlapPairCount = 0;

        int32 SweepBlockingPairCount = 0;

        int32 CauseCount = 0;

    };



    struct FMovementNodeFinding

    {

        TWeakObjectPtr<UBlueprint> Blueprint;

        TWeakObjectPtr<UEdGraphNode> Node;

        FString BlueprintName;

        FString GraphName;

        FString NodeTitle;

        FString FunctionName;

        FString Detail;

        FString Severity;

        int32 NodePosX = 0;

        int32 NodePosY = 0;

    };

    FString GetCollisionActorLabelSafe(const AActor* Actor)

    {

        if (!Actor)

        {

            return TEXT("<None>");

        }

#if WITH_EDITOR

        return Actor->GetActorLabel();

#else

        return Actor->GetName();

#endif

    }



    FString GetWorldTypeText(const UWorld* World)

    {

        if (!World) return TEXT("NoWorld");

        switch (World->WorldType)

        {

        case EWorldType::PIE: return TEXT("PIE");

        case EWorldType::Game: return TEXT("Game");

        case EWorldType::Editor: return TEXT("Editor");

        case EWorldType::EditorPreview: return TEXT("EditorPreview");

        default: return TEXT("Other");

        }

    }



    FString GetCollisionEnabledText(ECollisionEnabled::Type Value)

    {

        switch (Value)

        {

        case ECollisionEnabled::NoCollision: return TEXT("NoCollision");

        case ECollisionEnabled::QueryOnly: return TEXT("QueryOnly");

        case ECollisionEnabled::PhysicsOnly: return TEXT("PhysicsOnly");

        case ECollisionEnabled::QueryAndPhysics: return TEXT("QueryAndPhysics");

        case ECollisionEnabled::ProbeOnly: return TEXT("ProbeOnly");

        case ECollisionEnabled::QueryAndProbe: return TEXT("QueryAndProbe");

        default: return TEXT("Unknown");

        }

    }



    FString GetCollisionResponseText(ECollisionResponse Value)

    {

        switch (Value)

        {

        case ECR_Ignore: return TEXT("Ignore");

        case ECR_Overlap: return TEXT("Overlap");

        case ECR_Block: return TEXT("Block");

        default: return TEXT("Unknown");

        }

    }



    FString GetCollisionChannelText(ECollisionChannel Channel)

    {

        if (const UCollisionProfile* Profile = UCollisionProfile::Get())

        {

            const FName ConfiguredName = Profile->ReturnChannelNameFromContainerIndex(static_cast<int32>(Channel));

            if (!ConfiguredName.IsNone())

            {

                return ConfiguredName.ToString();

            }

        }

        if (const UEnum* Enum = StaticEnum<ECollisionChannel>())

        {

            FString Name = Enum->GetNameStringByValue(static_cast<int64>(Channel));

            Name.RemoveFromStart(TEXT("ECC_"));

            return Name;

        }

        return FString::Printf(TEXT("Channel%d"), static_cast<int32>(Channel));

    }



    FLinearColor GetSeverityColor(ECollisionFindingSeverity Severity)

    {

        switch (Severity)

        {

        case ECollisionFindingSeverity::Cause: return FLinearColor(0.95f, 0.25f, 0.18f);

        case ECollisionFindingSeverity::Warning: return FLinearColor(0.95f, 0.62f, 0.18f);

        case ECollisionFindingSeverity::Pass: return FLinearColor(0.20f, 0.75f, 0.34f);

        default: return FLinearColor(0.35f, 0.62f, 0.95f);

        }

    }



    FString GetSeverityText(ECollisionFindingSeverity Severity)

    {

        switch (Severity)

        {

        case ECollisionFindingSeverity::Cause: return TMLoc::String(TEXT("CAUSE"), TEXT("CAUSE"));

        case ECollisionFindingSeverity::Warning: return TMLoc::String(TEXT("CHECK"), TEXT("CHECK"));

        case ECollisionFindingSeverity::Pass: return TMLoc::String(TEXT("PASS"), TEXT("PASS"));

        default: return TMLoc::String(TEXT("INFO"), TEXT("INFO"));

        }

    }



    void AddFinding(TArray<FCollisionFinding>& Findings, ECollisionFindingSeverity Severity, const FString& Title, const FString& Detail)

    {

        FCollisionFinding& Finding = Findings.AddDefaulted_GetRef();

        Finding.Severity = Severity;

        Finding.Title = Title;

        Finding.Detail = Detail;

    }



    bool IsActorIgnoredWhenMoving(const UPrimitiveComponent* Component, const AActor* OtherActor)

    {

        return Component && OtherActor && Component->GetMoveIgnoreActors().Contains(const_cast<AActor*>(OtherActor));

    }



    bool IsComponentIgnoredWhenMoving(const UPrimitiveComponent* Component, const UPrimitiveComponent* OtherComponent)

    {

        return Component && OtherComponent && Component->GetMoveIgnoreComponents().Contains(const_cast<UPrimitiveComponent*>(OtherComponent));

    }



    bool HasUsableCollisionGeometry(UPrimitiveComponent* Component, FString& OutDetail)

    {

        if (!Component)

        {

            OutDetail = TEXT("Component is invalid.");

            return false;

        }



        UBodySetup* BodySetup = Component->GetBodySetup();

        if (!BodySetup)

        {

            OutDetail = TEXT("GetBodySetup returned null. Some custom primitive components provide collision without a BodySetup, so verify the component implementation.");

            return false;

        }



        const int32 SimpleShapeCount = BodySetup->AggGeom.GetElementCount();

        if (SimpleShapeCount > 0)

        {

            OutDetail = FString::Printf(TEXT("BodySetup contains %d simple collision shape(s)."), SimpleShapeCount);

            return true;

        }



        if (BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple)

        {

            OutDetail = TEXT("No simple shapes, but Collision Complexity is UseComplexAsSimple.");

            return true;

        }



        OutDetail = TEXT("BodySetup has no simple collision shapes and is not configured as UseComplexAsSimple.");

        return false;

    }



    void GatherSelectedActors(TArray<AActor*>& OutActors)

    {

        OutActors.Reset();

        if (!GEditor)

        {

            return;

        }



        auto AddUniqueActor = [&OutActors](AActor* Actor)

        {

            if (Actor && !Actor->IsTemplate())

            {

                OutActors.AddUnique(Actor);

            }

        };



        if (USelection* SelectedActors = GEditor->GetSelectedActors())

        {

            for (FSelectionIterator It(*SelectedActors); It; ++It)

            {

                AddUniqueActor(Cast<AActor>(*It));

            }

        }



        if (USelection* SelectedComponents = GEditor->GetSelectedComponents())

        {

            for (FSelectionIterator It(*SelectedComponents); It; ++It)

            {

                if (const UActorComponent* Component = Cast<UActorComponent>(*It))

                {

                    AddUniqueActor(Component->GetOwner());

                }

            }

        }

    }



    void GatherActorsFromMenuContext(const FToolMenuContext& Context, TArray<AActor*>& OutActors)

    {

        GatherSelectedActors(OutActors);



        auto AddUniqueActor = [&OutActors](AActor* Actor)

        {

            if (IsValid(Actor) && !Actor->IsTemplate())

            {

                OutActors.AddUnique(Actor);

            }

        };



        if (const ULevelEditorContextMenuContext* LevelContext = Context.FindContext<ULevelEditorContextMenuContext>())

        {

            AddUniqueActor(LevelContext->HitProxyActor.Get());

            for (UActorComponent* Component : LevelContext->SelectedComponents)

            {

                if (Component)

                {

                    AddUniqueActor(Component->GetOwner());

                }

            }

        }

    }



    AActor* ResolvePIECounterpart(AActor* SourceActor)

    {

        if (!SourceActor || !GEngine)

        {

            return SourceActor;

        }



        UWorld* SourceWorld = SourceActor->GetWorld();

        if (SourceWorld && (SourceWorld->WorldType == EWorldType::PIE || SourceWorld->WorldType == EWorldType::Game))

        {

            return SourceActor;

        }



        TArray<UWorld*> RuntimeWorlds;

        for (const FWorldContext& Context : GEngine->GetWorldContexts())

        {

            UWorld* World = Context.World();

            if (World && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game))

            {

                RuntimeWorlds.Add(World);

            }

        }



        if (RuntimeWorlds.IsEmpty())

        {

            return SourceActor;

        }



        const FGuid SourceGuid = SourceActor->GetActorGuid();

        const FString SourceLabel = GetCollisionActorLabelSafe(SourceActor);

        for (UWorld* RuntimeWorld : RuntimeWorlds)

        {

            AActor* LabelAndClassMatch = nullptr;

            for (TActorIterator<AActor> It(RuntimeWorld); It; ++It)

            {

                AActor* Candidate = *It;

                if (!Candidate) continue;

                if (SourceGuid.IsValid() && Candidate->GetActorGuid() == SourceGuid)

                {

                    return Candidate;

                }

                if (!LabelAndClassMatch

                    && Candidate->GetClass() == SourceActor->GetClass()

                    && GetCollisionActorLabelSafe(Candidate) == SourceLabel)

                {

                    LabelAndClassMatch = Candidate;

                }

            }

            if (LabelAndClassMatch)

            {

                return LabelAndClassMatch;

            }

        }



        return SourceActor;

    }



    void GatherPrimitiveComponents(AActor* Actor, TArray<UPrimitiveComponent*>& OutComponents)

    {

        OutComponents.Reset();

        if (!Actor) return;

        Actor->GetComponents<UPrimitiveComponent>(OutComponents, true);

        OutComponents.RemoveAll([](const UPrimitiveComponent* Component)

        {

            return !Component || Component->IsTemplate();

        });

    }





    bool ProbeShortSweep(UPrimitiveComponent* MovingComponent, UPrimitiveComponent* TargetComponent, const TCHAR* MovingSide, FString& OutDetail)

    {

        if (!MovingComponent || !TargetComponent || MovingComponent->GetWorld() != TargetComponent->GetWorld())

        {

            OutDetail = TMLoc::String(TEXT("Sweep probe skipped: components are invalid or not in the same world."), TEXT("Sweep probe skipped: components are invalid or not in the same world."));

            return false;

        }



        UWorld* World = MovingComponent->GetWorld();

        if (!World)

        {

            OutDetail = TMLoc::String(TEXT("Sweep probe skipped: component has no world."), TEXT("Sweep probe skipped: component has no world."));

            return false;

        }



        FVector Direction = TargetComponent->Bounds.Origin - MovingComponent->Bounds.Origin;

        if (!Direction.Normalize())

        {

            Direction = TargetComponent->GetComponentLocation() - MovingComponent->GetComponentLocation();

            if (!Direction.Normalize())

            {

                OutDetail = TMLoc::String(TEXT("Sweep probe skipped: the components share the same center, so no reliable sweep direction could be inferred."), TEXT("Sweep probe skipped: the components share the same center, so no reliable sweep direction could be inferred."));

                return false;

            }

        }



        const FVector Start = MovingComponent->GetComponentLocation();

        const float CenterDistance = FVector::Dist(MovingComponent->Bounds.Origin, TargetComponent->Bounds.Origin);

        const float SweepDistance = FMath::Clamp(CenterDistance + 10.0f, 10.0f, 300.0f);

        const FVector End = Start + Direction * SweepDistance;



        FComponentQueryParams Params;

        Params.TraceTag = TEXT("TMCollisionPairAnalyzerSweep");

        Params.bTraceComplex = false;

        Params.AddIgnoredActor(MovingComponent->GetOwner());



        TArray<FHitResult> Hits;

        World->ComponentSweepMulti(Hits, MovingComponent, Start, End, MovingComponent->GetComponentQuat(), Params);



        for (const FHitResult& Hit : Hits)

        {

            if (!Hit.bBlockingHit)

            {

                continue;

            }



            const UPrimitiveComponent* HitComponent = Hit.GetComponent();

            const AActor* HitActor = Hit.GetActor();

            if (HitComponent == TargetComponent || HitActor == TargetComponent->GetOwner())

            {

                OutDetail = FString::Printf(TEXT("%s swept %.1f units toward the other actor and produced a blocking hit on %s at distance %.1f."),

                    MovingSide,

                    SweepDistance,

                    *GetNameSafe(HitComponent),

                    Hit.Distance);

                return true;

            }

        }



        OutDetail = FString::Printf(TEXT("%s short sweep toward the other actor did not hit this component pair. This is not a failure by itself; the real movement direction and distance may be different."), MovingSide);

        return false;

    }

    FCollisionPairReport AnalyzeComponentPair(AActor* ActorA, AActor* ActorB, UPrimitiveComponent* ComponentA, UPrimitiveComponent* ComponentB)

    {

        FCollisionPairReport Report;

        Report.ComponentA = ComponentA;

        Report.ComponentB = ComponentB;

        Report.ComponentALabel = ComponentA

            ? FString::Printf(TEXT("%s.%s"), *GetCollisionActorLabelSafe(ComponentA->GetOwner()), *ComponentA->GetName())

            : TEXT("<Invalid>");

        Report.ComponentBLabel = ComponentB

            ? FString::Printf(TEXT("%s.%s"), *GetCollisionActorLabelSafe(ComponentB->GetOwner()), *ComponentB->GetName())

            : TEXT("<Invalid>");

        if (!ComponentA || !ComponentB)

        {

            AddFinding(Report.Findings, ECollisionFindingSeverity::Cause, TEXT("Invalid component"), TEXT("One side of the component pair is no longer valid."));

            return Report;

        }



        const ECollisionEnabled::Type EnabledA = ComponentA->GetCollisionEnabled();

        const ECollisionEnabled::Type EnabledB = ComponentB->GetCollisionEnabled();

        const ECollisionChannel TypeA = ComponentA->GetCollisionObjectType();

        const ECollisionChannel TypeB = ComponentB->GetCollisionObjectType();

        const ECollisionResponse ResponseAtoB = ComponentA->GetCollisionResponseToChannel(TypeB);

        const ECollisionResponse ResponseBtoA = ComponentB->GetCollisionResponseToChannel(TypeA);



        Report.ProfileA = ComponentA->GetCollisionProfileName().ToString();

        Report.ProfileB = ComponentB->GetCollisionProfileName().ToString();

        Report.ObjectTypeA = GetCollisionChannelText(TypeA);

        Report.ObjectTypeB = GetCollisionChannelText(TypeB);

        Report.ResponseAtoB = GetCollisionResponseText(ResponseAtoB);

        Report.ResponseBtoA = GetCollisionResponseText(ResponseBtoA);

        Report.EnabledA = GetCollisionEnabledText(EnabledA);

        Report.EnabledB = GetCollisionEnabledText(EnabledB);



        if (!ComponentA->IsRegistered() || !ComponentB->IsRegistered())

        {

            AddFinding(Report.Findings, ECollisionFindingSeverity::Cause, TEXT("Component is not registered"),

                FString::Printf(TEXT("Registered: A=%s, B=%s. An unregistered component has no usable runtime collision state."),

                    ComponentA->IsRegistered() ? TEXT("true") : TEXT("false"), ComponentB->IsRegistered() ? TEXT("true") : TEXT("false")));

        }



        if ((EnabledA != ECollisionEnabled::NoCollision && !ComponentA->IsPhysicsStateCreated())

            || (EnabledB != ECollisionEnabled::NoCollision && !ComponentB->IsPhysicsStateCreated()))

        {

            AddFinding(Report.Findings, ECollisionFindingSeverity::Warning, TEXT("Runtime physics state is missing"),

                FString::Printf(TEXT("Physics state created: A=%s, B=%s. Collision settings may have changed without recreating component physics state."),

                    ComponentA->IsPhysicsStateCreated() ? TEXT("true") : TEXT("false"),

                    ComponentB->IsPhysicsStateCreated() ? TEXT("true") : TEXT("false")));

        }



        if (EnabledA == ECollisionEnabled::NoCollision || EnabledB == ECollisionEnabled::NoCollision)

        {

            AddFinding(Report.Findings, ECollisionFindingSeverity::Cause, TEXT("Collision is disabled"),

                FString::Printf(TEXT("Collision Enabled: A=%s, B=%s."), *Report.EnabledA, *Report.EnabledB));

        }



        const bool bQueryCompatible = CollisionEnabledHasQuery(EnabledA) && CollisionEnabledHasQuery(EnabledB);

        const bool bPhysicsCompatible = CollisionEnabledHasPhysics(EnabledA) && CollisionEnabledHasPhysics(EnabledB);

        if (EnabledA != ECollisionEnabled::NoCollision && EnabledB != ECollisionEnabled::NoCollision && !bQueryCompatible && !bPhysicsCompatible)

        {

            AddFinding(Report.Findings, ECollisionFindingSeverity::Cause, TEXT("Collision modes do not share a common path"),

                FString::Printf(TEXT("A=%s and B=%s cannot meet in either query/sweep or physics simulation."), *Report.EnabledA, *Report.EnabledB));

        }



        if (ResponseAtoB == ECR_Ignore || ResponseBtoA == ECR_Ignore)

        {

            AddFinding(Report.Findings, ECollisionFindingSeverity::Cause, TEXT("A collision response is Ignore"),

                FString::Printf(TEXT("A responds %s to B's %s channel; B responds %s to A's %s channel. Ignore wins."),

                    *Report.ResponseAtoB, *Report.ObjectTypeB, *Report.ResponseBtoA, *Report.ObjectTypeA));

        }

        else if (ResponseAtoB == ECR_Overlap || ResponseBtoA == ECR_Overlap)

        {

            AddFinding(Report.Findings, ECollisionFindingSeverity::Cause, TEXT("The pair resolves to Overlap, not Block"),

                FString::Printf(TEXT("A->B=%s and B->A=%s. Both sides must resolve to Block for blocking contact."),

                    *Report.ResponseAtoB, *Report.ResponseBtoA));

            if (!ComponentA->GetGenerateOverlapEvents() || !ComponentB->GetGenerateOverlapEvents())

            {

                AddFinding(Report.Findings, ECollisionFindingSeverity::Warning, TEXT("Overlap events are also disabled"),

                    FString::Printf(TEXT("Generate Overlap Events: A=%s, B=%s. Both must be enabled to receive overlap callbacks."),

                        ComponentA->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"),

                        ComponentB->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false")));

            }

        }



        Report.bMoveIgnored = IsActorIgnoredWhenMoving(ComponentA, ActorB)

            || IsActorIgnoredWhenMoving(ComponentB, ActorA)

            || IsComponentIgnoredWhenMoving(ComponentA, ComponentB)

            || IsComponentIgnoredWhenMoving(ComponentB, ComponentA);

        if (Report.bMoveIgnored)

        {

            AddFinding(Report.Findings, ECollisionFindingSeverity::Cause, TEXT("MoveComponent ignore list excludes this pair"),

                TEXT("IgnoreActorWhenMoving or IgnoreComponentWhenMoving contains the other side. This bypasses swept movement collision, though it does not disable physics contacts."));

        }



        FString GeometryDetailA;

        FString GeometryDetailB;

        const bool bGeometryA = HasUsableCollisionGeometry(ComponentA, GeometryDetailA);

        const bool bGeometryB = HasUsableCollisionGeometry(ComponentB, GeometryDetailB);

        if (!bGeometryA || !bGeometryB)

        {

            AddFinding(Report.Findings, ECollisionFindingSeverity::Warning, TEXT("Collision geometry may be missing"),

                FString::Printf(TEXT("A: %s B: %s"), *GeometryDetailA, *GeometryDetailB));

        }



        Report.bBlockConfigured = ComponentA->IsRegistered()

            && ComponentB->IsRegistered()

            && EnabledA != ECollisionEnabled::NoCollision

            && EnabledB != ECollisionEnabled::NoCollision

            && (bQueryCompatible || bPhysicsCompatible)

            && ResponseAtoB == ECR_Block

            && ResponseBtoA == ECR_Block

            && !Report.bMoveIgnored;



        Report.bBoundsOverlap = ComponentA->Bounds.GetBox().Intersect(ComponentB->Bounds.GetBox());

        if (bQueryCompatible && bGeometryA && bGeometryB && ComponentA->GetWorld() == ComponentB->GetWorld())

        {

            FCollisionQueryParams Params(SCENE_QUERY_STAT(TMCollisionPairAnalyzer), false);

            Report.bShapeTestPerformed = true;

            Report.bShapeOverlap = ComponentA->ComponentOverlapComponent(

                ComponentB,

                ComponentB->GetComponentLocation(),

                ComponentB->GetComponentQuat(),

                Params);

        }



        FString SweepDetailA;

        FString SweepDetailB;

        if (Report.bBlockConfigured && bQueryCompatible && bGeometryA && bGeometryB && ComponentA->GetWorld() == ComponentB->GetWorld())

        {

            Report.bSweepTestPerformed = true;

            const bool bSweepAHitB = ProbeShortSweep(ComponentA, ComponentB, TEXT("A"), SweepDetailA);

            const bool bSweepBHitA = ProbeShortSweep(ComponentB, ComponentA, TEXT("B"), SweepDetailB);

            Report.bSweepBlockingHit = bSweepAHitB || bSweepBHitA;

        }



        if (Report.bBlockConfigured)

        {

            AddFinding(Report.Findings, ECollisionFindingSeverity::Pass, TEXT("This component pair is configured to Block"),

                FString::Printf(TEXT("A[%s/%s] <-> B[%s/%s] both resolve to Block."),

                    *Report.ProfileA, *Report.ObjectTypeA, *Report.ProfileB, *Report.ObjectTypeB));



            if (!Report.bBoundsOverlap)

            {

                AddFinding(Report.Findings, ECollisionFindingSeverity::Info, TEXT("Components are not currently touching"),

                    TEXT("Their world-space bounds do not overlap. This does not mean the pair cannot block during a swept move."));

            }

            else if (!Report.bShapeOverlap && bQueryCompatible)

            {

                AddFinding(Report.Findings, ECollisionFindingSeverity::Info, TEXT("No current overlap contact"),

                    TEXT("Rendering bounds intersect, but the overlap query reports no current penetration. A swept movement block can still be correct because UE often stops the mover before shapes overlap."));

            }

            else if (Report.bShapeOverlap)

            {

                AddFinding(Report.Findings, ECollisionFindingSeverity::Pass, TEXT("Collision shapes currently overlap"),

                    TEXT("The runtime component overlap test found current penetration/contact. If movement still passes through, inspect whether the move used Sweep=false or Teleport."));

            }



            if (Report.bSweepTestPerformed)

            {

                if (Report.bSweepBlockingHit)

                {

                    AddFinding(Report.Findings, ECollisionFindingSeverity::Pass, TEXT("Swept movement can block"),

                        !SweepDetailA.IsEmpty() && SweepDetailA.Contains(TEXT("produced a blocking hit")) ? SweepDetailA : SweepDetailB);

                }

                else

                {

                    AddFinding(Report.Findings, ECollisionFindingSeverity::Info, TEXT("Short sweep probe did not hit this pair"),

                        FString::Printf(TEXT("%s %s"), *SweepDetailA, *SweepDetailB));

                }

            }

        }



        if (!ComponentA->GetGenerateOverlapEvents() || !ComponentB->GetGenerateOverlapEvents())

        {

            AddFinding(Report.Findings, ECollisionFindingSeverity::Info, TEXT("Generate Overlap Events does not control blocking"),

                TEXT("Disabling overlap events suppresses overlap callbacks, but it is not by itself a reason that a Block response fails."));

        }



        return Report;

    }



    FCollisionAnalysisResult AnalyzeActors(AActor* InputA, AActor* InputB)

    {

        FCollisionAnalysisResult Result;

        AActor* ActorA = ResolvePIECounterpart(InputA);

        AActor* ActorB = ResolvePIECounterpart(InputB);

        Result.ActorAName = GetCollisionActorLabelSafe(ActorA);

        Result.ActorBName = GetCollisionActorLabelSafe(ActorB);



        if (!ActorA || !ActorB)

        {

            AddFinding(Result.ActorFindings, ECollisionFindingSeverity::Cause, TEXT("Two actors are required"), TEXT("Capture actor A and actor B, or select exactly two actors."));

            Result.Summary = TMLoc::String(TEXT("Analysis could not start because one or both actors are invalid."), TEXT("Analysis could not start because one or both actors are invalid."));

            Result.CauseCount = 1;

            return Result;

        }

        if (ActorA == ActorB)

        {

            AddFinding(Result.ActorFindings, ECollisionFindingSeverity::Cause, TEXT("A and B are the same actor"), TEXT("Choose two different actor instances."));

            Result.Summary = TMLoc::String(TEXT("Choose two different actors."), TEXT("Choose two different actors."));

            Result.CauseCount = 1;

            return Result;

        }



        UWorld* WorldA = ActorA->GetWorld();

        UWorld* WorldB = ActorB->GetWorld();

        Result.WorldName = FString::Printf(TEXT("A=%s, B=%s"), *GetWorldTypeText(WorldA), *GetWorldTypeText(WorldB));

        if (!WorldA || WorldA != WorldB)

        {

            AddFinding(Result.ActorFindings, ECollisionFindingSeverity::Cause, TEXT("Actors are in different worlds"),

                TEXT("Editor and PIE instances cannot collide with each other. Re-capture both actors from the same PIE world."));

        }



        if (!ActorA->GetActorEnableCollision() || !ActorB->GetActorEnableCollision())

        {

            AddFinding(Result.ActorFindings, ECollisionFindingSeverity::Cause, TEXT("Actor collision is disabled"),

                FString::Printf(TEXT("Actor Enable Collision: A=%s, B=%s."),

                    ActorA->GetActorEnableCollision() ? TEXT("true") : TEXT("false"),

                    ActorB->GetActorEnableCollision() ? TEXT("true") : TEXT("false")));

        }



        if (ActorA->IsAttachedTo(ActorB) || ActorB->IsAttachedTo(ActorA))

        {

            AddFinding(Result.ActorFindings, ECollisionFindingSeverity::Warning, TEXT("Actors share an attachment hierarchy"),

                TEXT("Attached actors commonly move as one hierarchy. If simulated bodies were welded, internal collision may not produce separate blocking contact."));

        }



        TArray<UPrimitiveComponent*> ComponentsA;

        TArray<UPrimitiveComponent*> ComponentsB;

        GatherPrimitiveComponents(ActorA, ComponentsA);

        GatherPrimitiveComponents(ActorB, ComponentsB);

        Result.PrimitiveCountA = ComponentsA.Num();

        Result.PrimitiveCountB = ComponentsB.Num();

        if (ComponentsA.IsEmpty() || ComponentsB.IsEmpty())

        {

            AddFinding(Result.ActorFindings, ECollisionFindingSeverity::Cause, TEXT("No primitive collision component"),

                FString::Printf(TEXT("Primitive components: A=%d, B=%d. SceneComponent-only actors cannot collide."), ComponentsA.Num(), ComponentsB.Num()));

        }



        for (UPrimitiveComponent* ComponentA : ComponentsA)

        {

            for (UPrimitiveComponent* ComponentB : ComponentsB)

            {

                FCollisionPairReport Pair = AnalyzeComponentPair(ActorA, ActorB, ComponentA, ComponentB);

                if (Pair.bBlockConfigured) ++Result.BlockConfiguredPairCount;

                if (Pair.bShapeOverlap && Pair.bBlockConfigured) ++Result.ShapeOverlapPairCount;

                if (Pair.bSweepBlockingHit && Pair.bBlockConfigured) ++Result.SweepBlockingPairCount;

                for (const FCollisionFinding& Finding : Pair.Findings)

                {

                    if (Finding.Severity == ECollisionFindingSeverity::Cause) ++Result.CauseCount;

                }

                Result.PairReports.Add(MoveTemp(Pair));

            }

        }



        Result.PairReports.Sort([](const FCollisionPairReport& Left, const FCollisionPairReport& Right)

        {

            if (Left.bBlockConfigured != Right.bBlockConfigured) return Left.bBlockConfigured;

            if (Left.bShapeOverlap != Right.bShapeOverlap) return Left.bShapeOverlap;

            if (Left.ComponentALabel != Right.ComponentALabel) return Left.ComponentALabel < Right.ComponentALabel;

            return Left.ComponentBLabel < Right.ComponentBLabel;

        });



        for (const FCollisionFinding& Finding : Result.ActorFindings)

        {

            if (Finding.Severity == ECollisionFindingSeverity::Cause) ++Result.CauseCount;

        }



        const int32 ActorCauseCount = Algo::CountIf(Result.ActorFindings, [](const FCollisionFinding& Finding)

        {

            return Finding.Severity == ECollisionFindingSeverity::Cause;

        });



        if (ActorCauseCount > 0)

        {

            Result.Summary = FString::Printf(TEXT("Actor/world state prevents collision. %d actor-level cause(s) were found."), ActorCauseCount);

        }

        else if (Result.CauseCount > 0 && Result.BlockConfiguredPairCount == 0)

        {

            Result.Summary = FString::Printf(TEXT("Block cannot occur with the current settings. %d definite cause(s) were found."), Result.CauseCount);

        }

        else if (Result.BlockConfiguredPairCount == 0)

        {

            Result.Summary = TMLoc::String(TEXT("No component pair resolves to Block. Review the component pair responses below."), TEXT("No component pair resolves to Block. Review the component pair responses below."));

        }

        else if (Result.SweepBlockingPairCount > 0)

        {

            Result.Summary = FString::Printf(TEXT("%d component pair(s) are configured to Block, and %d pair(s) produced a blocking short sweep probe. Treat this as high-confidence evidence for swept query blocking, not proof that every gameplay movement path will stop."), Result.BlockConfiguredPairCount, Result.SweepBlockingPairCount);

        }

        else if (Result.ShapeOverlapPairCount > 0)

        {

            Result.Summary = FString::Printf(TEXT("%d component pair(s) are configured to Block, and %d pair(s) currently report shape overlap/contact. This confirms current query contact, but movement blocking still depends on the actual movement API and updated component."), Result.BlockConfiguredPairCount, Result.ShapeOverlapPairCount);

        }

        else

        {

            Result.Summary = FString::Printf(TEXT("%d component pair(s) are configured to Block. No current shape contact was detected; this is still compatible with valid swept blocking because UE movement often stops before penetration. Confirm the actual movement path with Sweep=true and FHitResult."), Result.BlockConfiguredPairCount);

        }



        AddFinding(Result.ActorFindings, ECollisionFindingSeverity::Warning, TEXT("Movement API cannot be inferred from static state"),

            TEXT("SetActorLocation/SetWorldLocation/MoveComponent with Sweep=false, or Teleport, can pass through otherwise valid blocking collision. Reproduce with Sweep=true and inspect the returned FHitResult."));

        AddFinding(Result.ActorFindings, ECollisionFindingSeverity::Info, TEXT("Hit-event settings are separate"),

            TEXT("Simulation Generates Hit Events controls hit callbacks; it does not decide whether geometry blocks movement."));

        return Result;

    }



    FString LocalizedFindingTitle(const FString& Title)

    {

        if (Title == TEXT("Invalid component")) return TMLoc::String(TEXT("Invalid component"), TEXT("Invalid component"));

        if (Title == TEXT("Component is not registered")) return TMLoc::String(TEXT("Component is not registered"), TEXT("Component is not registered"));

        if (Title == TEXT("Runtime physics state is missing")) return TMLoc::String(TEXT("Runtime physics state is missing"), TEXT("Runtime physics state is missing"));

        if (Title == TEXT("Collision is disabled")) return TMLoc::String(TEXT("Collision is disabled"), TEXT("Collision is disabled"));

        if (Title == TEXT("Collision modes do not share a common path")) return TMLoc::String(TEXT("Collision modes do not share a common path"), TEXT("Collision modes do not share a common path"));

        if (Title == TEXT("A collision response is Ignore")) return TMLoc::String(TEXT("A collision response is Ignore"), TEXT("A collision response is Ignore"));

        if (Title == TEXT("The pair resolves to Overlap, not Block")) return TMLoc::String(TEXT("The pair resolves to Overlap, not Block"), TEXT("The pair resolves to Overlap, not Block"));

        if (Title == TEXT("Overlap events are also disabled")) return TMLoc::String(TEXT("Overlap events are also disabled"), TEXT("Overlap events are also disabled"));

        if (Title == TEXT("MoveComponent ignore list excludes this pair")) return TMLoc::String(TEXT("MoveComponent ignore list excludes this pair"), TEXT("MoveComponent ignore list excludes this pair"));

        if (Title == TEXT("Collision geometry may be missing")) return TMLoc::String(TEXT("Collision geometry may be missing"), TEXT("Collision geometry may be missing"));

        if (Title == TEXT("This component pair is configured to Block")) return TMLoc::String(TEXT("This component pair is configured to Block"), TEXT("This component pair is configured to Block"));

        if (Title == TEXT("Components are not currently touching")) return TMLoc::String(TEXT("Components are not currently touching"), TEXT("Components are not currently touching"));

        if (Title == TEXT("No current overlap contact")) return TMLoc::String(TEXT("No current overlap contact"), TEXT("No current overlap contact"));

        if (Title == TEXT("Collision shapes currently overlap")) return TMLoc::String(TEXT("Collision shapes currently overlap"), TEXT("Collision shapes currently overlap"));

        if (Title == TEXT("Swept movement can block")) return TMLoc::String(TEXT("Swept movement can block"), TEXT("Swept movement can block"));

        if (Title == TEXT("Short sweep probe did not hit this pair")) return TMLoc::String(TEXT("Short sweep probe did not hit this pair"), TEXT("Short sweep probe did not hit this pair"));

        if (Title == TEXT("Generate Overlap Events does not control blocking")) return TMLoc::String(TEXT("Generate Overlap Events does not control blocking"), TEXT("Generate Overlap Events does not control blocking"));

        if (Title == TEXT("Two actors are required")) return TMLoc::String(TEXT("Two actors are required"), TEXT("Two actors are required"));

        if (Title == TEXT("A and B are the same actor")) return TMLoc::String(TEXT("A and B are the same actor"), TEXT("A and B are the same actor"));

        if (Title == TEXT("Actors are in different worlds")) return TMLoc::String(TEXT("Actors are in different worlds"), TEXT("Actors are in different worlds"));

        if (Title == TEXT("Actor collision is disabled")) return TMLoc::String(TEXT("Actor collision is disabled"), TEXT("Actor collision is disabled"));

        if (Title == TEXT("Actors share an attachment hierarchy")) return TMLoc::String(TEXT("Actors share an attachment hierarchy"), TEXT("Actors share an attachment hierarchy"));

        if (Title == TEXT("No primitive collision component")) return TMLoc::String(TEXT("No primitive collision component"), TEXT("No primitive collision component"));

        if (Title == TEXT("Movement API cannot be inferred from static state")) return TMLoc::String(TEXT("Movement API cannot be inferred from static state"), TEXT("Movement API cannot be inferred from static state"));

        if (Title == TEXT("Hit-event settings are separate")) return TMLoc::String(TEXT("Hit-event settings are separate"), TEXT("Hit-event settings are separate"));

        if (Title == TEXT("Selection required")) return TMLoc::String(TEXT("Selection required"), TEXT("Selection required"));

        return Title;

    }

    TSharedRef<SWidget> BuildFindingWidget(const FCollisionFinding& Finding)

    {

        const FLinearColor Color = GetSeverityColor(Finding.Severity);

        return SNew(SBorder)

            .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

            .Padding(7.0f)

            [

                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0, 0, 8, 0)

                [

                    SNew(STextBlock)

                    .Text(FText::FromString(GetSeverityText(Finding.Severity)))

                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                    .ColorAndOpacity(Color)

                ]

                + SHorizontalBox::Slot().FillWidth(1.0f)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        SNew(STextBlock).Text(FText::FromString(LocalizedFindingTitle(Finding.Title))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9)).ColorAndOpacity(Color)

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)

                    [

                        SNew(STextBlock).Text(FText::FromString(Finding.Detail)).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                    ]

                ]

            ];

    }



    class SCollisionPairAnalyzerWidget : public SCompoundWidget

    {

    public:

        SLATE_BEGIN_ARGS(SCollisionPairAnalyzerWidget) {}

        SLATE_END_ARGS()



        void Construct(const FArguments&)

        {

            ChildSlot

            [

                SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                .Padding(10.0f)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        BuildHeader()

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8)

                    [

                        SNew(SSeparator)

                    ]

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        BuildSelectionBar()

                    ]

                    + SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 8, 0, 0)

                    [

                        SAssignNew(ResultScrollBox, SScrollBox)

                    ]

                ]

            ];

            UseTwoSelectedAndAnalyze(false);

            RefreshResults();

        }



        void UseCurrentSelectionAndAnalyze()

        {

            UseTwoSelectedAndAnalyze(true);

        }



        void UseActorsAndAnalyze(const TArray<AActor*>& Actors)

        {

            if (Actors.Num() != 2)

            {

                StatusMessage = FString::Printf(TEXT("Select exactly two actors (currently %d)."), Actors.Num());

                bHasResult = false;

                RefreshResults();

                return;

            }



            ActorA = Actors[0];

            ActorB = Actors[1];

            RunAnalysis();

        }



    private:

        TSharedRef<SWidget> BuildHeader()

        {

            return SNew(SHorizontalBox)

                + SHorizontalBox::Slot().FillWidth(1.0f)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        SNew(STextBlock)

                        .Text(TMLoc::Text(TEXT("Collision Pair Analyzer"), TEXT("Collision Pair Analyzer")))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15))

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)

                    [

                        SNew(STextBlock)

                        .Text(TMLoc::Text(TEXT("Compare two actors at runtime and explain why their primitive components do not Block."), TEXT("Compare two actors at runtime and explain why their primitive components do not Block.")))

                        .AutoWrapText(true)

                        .ColorAndOpacity(FLinearColor(0.65f, 0.69f, 0.75f))

                    ]

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(8, 0, 0, 0)

                [

                    SNew(SButton)

                    .Text(TMLoc::Text(TEXT("Add to Investigation"), TEXT("조사에 추가")))

                    .OnClicked_Lambda([this]()

                    {

                        const FString Target = FString::Printf(TEXT("%s <-> %s"),
                            ActorA.IsValid() ? *ActorA->GetActorLabel() : TEXT("None"),
                            ActorB.IsValid() ? *ActorB->GetActorLabel() : TEXT("None"));

                        TMInvestigationSession::RecordEvidence(TEXT("Collision Pair Analyzer"), StatusMessage.IsEmpty() ? TEXT("Collision analysis captured") : StatusMessage, Target);

                        TMInvestigationSession::OpenWindow();

                        return FReply::Handled();

                    })

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(4, 0, 0, 0)

                [

                    SNew(SButton)

                    .Text(TMLoc::Text(TEXT("Copy Report"), TEXT("리포트 복사")))

                    .OnClicked(this, &SCollisionPairAnalyzerWidget::OnCopyReportClicked)

                ];

        }



        TSharedRef<SWidget> BuildSelectionBar()

        {

            return SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()

                [

                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)

                    [

                        BuildInteractiveActorSlot(true)

                    ]

                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(4, 0, 0, 0)

                    [

                        BuildInteractiveActorSlot(false)

                    ]

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)

                [

                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)

                    [

                        SNew(SButton).Text(TMLoc::Text(TEXT("Use Two Selected"), TEXT("Use Two Selected"))).OnClicked(this, &SCollisionPairAnalyzerWidget::OnUseTwoSelectedClicked)

                    ]

                    + SHorizontalBox::Slot().AutoWidth()

                    [

                        SNew(SButton).Text(TMLoc::Text(TEXT("Analyze"), TEXT("Analyze"))).OnClicked(this, &SCollisionPairAnalyzerWidget::OnAnalyzeClicked)

                    ]

                ];

        }



        TSharedRef<SWidget> BuildInteractiveActorSlot(bool bSlotA)

        {

            return SNew(SDropTarget)

                .OnAllowDrop_Lambda([](TSharedPtr<FDragDropOperation> Operation)

                {

                    return Operation.IsValid() && Operation->IsOfType<FActorDragDropOp>();

                })

                .OnDropped_Lambda([this, bSlotA](const FGeometry&, const FDragDropEvent& Event)

                {

                    const TSharedPtr<FActorDragDropOp> ActorDrop = Event.GetOperationAs<FActorDragDropOp>();

                    if (ActorDrop.IsValid() && ActorDrop->Actors.Num() > 0 && ActorDrop->Actors[0].IsValid())

                    {

                        SetSlotActor(bSlotA, ActorDrop->Actors[0].Get());

                        return FReply::Handled();

                    }

                    return FReply::Unhandled();

                })

                [

                    SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

                    .Padding(7.0f)

                    [

                        SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight()

                        [

                            SNew(STextBlock)

                            .Text_Lambda([this, bSlotA]()

                            {

                                AActor* Actor = bSlotA ? ActorA.Get() : ActorB.Get();

                                return FText::FromString(FString::Printf(TEXT("%s: %s"), bSlotA ? TEXT("A") : TEXT("B"), *GetCollisionActorLabelSafe(Actor)));

                            })

                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 5)

                        [

                            SNew(STextBlock)

                            .Text(TMLoc::Text(TEXT("Drop an actor here, or replace it with the current selection."), TEXT("Drop an actor here, or replace it with the current selection.")))

                            .AutoWrapText(true)

                            .ColorAndOpacity(FLinearColor(0.58f, 0.63f, 0.70f))

                        ]

                        + SVerticalBox::Slot().AutoHeight()

                        [

                            SNew(SHorizontalBox)

                            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)

                            [

                                SNew(SButton)

                                .Text(TMLoc::Text(TEXT("Replace With Selected"), TEXT("Replace With Selected")))

                                .OnClicked_Lambda([this, bSlotA]()

                                {

                                    CaptureSelectedActor(bSlotA);

                                    return FReply::Handled();

                                })

                            ]

                            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)

                            [

                                SNew(SButton)

                                .Text(TMLoc::Text(TEXT("Clear"), TEXT("지우기")))

                                .OnClicked_Lambda([this, bSlotA]()

                                {

                                    SetSlotActor(bSlotA, nullptr);

                                    return FReply::Handled();

                                })

                            ]

                            + SHorizontalBox::Slot().AutoWidth()

                            [

                                SNew(SButton)

                                .Text(TMLoc::Text(TEXT("Swap A / B"), TEXT("Swap A / B")))

                                .OnClicked(this, &SCollisionPairAnalyzerWidget::OnSwapActorsClicked)

                            ]

                        ]

                    ]

                ];

        }



        TSharedRef<SWidget> BuildActorSlot(bool bSlotA)

        {

            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))

                .Padding(7.0f)

                [

                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                    [

                        SNew(STextBlock)

                        .Text_Lambda([this, bSlotA]()

                        {

                            AActor* Actor = bSlotA ? ActorA.Get() : ActorB.Get();

                            return FText::FromString(FString::Printf(TEXT("%s: %s"), bSlotA ? TEXT("A") : TEXT("B"), *GetCollisionActorLabelSafe(Actor)));

                        })

                    ]

                    + SHorizontalBox::Slot().AutoWidth()

                    [

                        SNew(SButton)

                        .Text(TMLoc::Text(TEXT("Capture Selected"), TEXT("Capture Selected")))

                        .OnClicked_Lambda([this, bSlotA]()

                        {

                            CaptureSelectedActor(bSlotA);

                            return FReply::Handled();

                        })

                    ]

                ];

        }



        void SetSlotActor(bool bSlotA, AActor* Actor)

        {

            if (bSlotA)

            {

                ActorA = Actor;

            }

            else

            {

                ActorB = Actor;

            }



            bHasResult = false;

            StatusMessage.Empty();

            if (ActorA.IsValid() && ActorB.IsValid())

            {

                RunAnalysis();

            }

            else

            {

                RefreshResults();

            }

        }



        void CaptureSelectedActor(bool bSlotA)

        {

            TArray<AActor*> Selected;

            GatherSelectedActors(Selected);

            if (Selected.IsEmpty())

            {

                StatusMessage = TEXT("Select an actor first.");

                RefreshResults();

                return;

            }

            SetSlotActor(bSlotA, Selected[0]);

        }



        void UseTwoSelectedAndAnalyze(bool bAnalyze)

        {

            TArray<AActor*> Selected;

            GatherSelectedActors(Selected);

            if (Selected.Num() == 2)

            {

                ActorA = Selected[0];

                ActorB = Selected[1];

                StatusMessage.Empty();

                if (bAnalyze) RunAnalysis();

            }

            else if (bAnalyze)

            {

                StatusMessage = FString::Printf(TEXT("Select exactly two actors (currently %d), or capture A and B separately."), Selected.Num());

                RefreshResults();

            }

        }



        void RunAnalysis()

        {

            LastResult = AnalyzeActors(ActorA.Get(), ActorB.Get());

            bHasResult = true;

            StatusMessage.Empty();

            RefreshResults();

        }



        FReply OnUseTwoSelectedClicked()

        {

            UseTwoSelectedAndAnalyze(false);

            return FReply::Handled();

        }



        FReply OnSwapActorsClicked()

        {

            Swap(ActorA, ActorB);

            bHasResult = false;

            StatusMessage.Empty();

            if (ActorA.IsValid() && ActorB.IsValid())

            {

                RunAnalysis();

            }

            else

            {

                RefreshResults();

            }

            return FReply::Handled();

        }



        FReply OnAnalyzeClicked()

        {

            RunAnalysis();

            return FReply::Handled();

        }



        FReply OnCopyReportClicked() const

        {

            FPlatformApplicationMisc::ClipboardCopy(*BuildReportText());

            return FReply::Handled();

        }



        void RefreshResults()

        {

            if (!ResultScrollBox.IsValid()) return;

            ResultScrollBox->ClearChildren();



            if (!StatusMessage.IsEmpty())

            {

                ResultScrollBox->AddSlot()[BuildFindingWidget({ ECollisionFindingSeverity::Warning, TEXT("Selection required"), StatusMessage })];

                return;

            }

            if (!bHasResult)

            {

                ResultScrollBox->AddSlot()

                [

                    SNew(STextBlock)

                    .Text(TMLoc::Text(TEXT("Select or capture actor A and B, then run Analyze."), TEXT("Select or capture actor A and B, then run Analyze.")))

                    .ColorAndOpacity(FLinearColor(0.6f, 0.64f, 0.70f))

                ];

                return;

            }



            ResultScrollBox->AddSlot()

            [

                BuildDashboardWidget()

            ];

        }



        FString GetPrimaryCauseTitle() const

        {

            TMap<FString, int32> CauseCounts;

            for (const FCollisionFinding& Finding : LastResult.ActorFindings)

            {

                if (Finding.Severity == ECollisionFindingSeverity::Cause)

                {

                    ++CauseCounts.FindOrAdd(Finding.Title);

                }

            }

            for (const FCollisionPairReport& Pair : LastResult.PairReports)

            {

                if (Pair.bBlockConfigured)

                {

                    continue;

                }

                for (const FCollisionFinding& Finding : Pair.Findings)

                {

                    if (Finding.Severity == ECollisionFindingSeverity::Cause)

                    {

                        ++CauseCounts.FindOrAdd(Finding.Title);

                    }

                }

            }



            FString BestTitle = TEXT("-");

            int32 BestCount = 0;

            for (const TPair<FString, int32>& Entry : CauseCounts)

            {

                if (Entry.Value > BestCount)

                {

                    BestTitle = Entry.Key;

                    BestCount = Entry.Value;

                }

            }

            return BestTitle;

        }





        void AddBlueprintFromClass(UClass* Class, TArray<UBlueprint*>& OutBlueprints, TSet<UBlueprint*>& SeenBlueprints) const

        {

            for (UClass* WalkClass = Class; WalkClass; WalkClass = WalkClass->GetSuperClass())

            {

                if (UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(WalkClass))

                {

                    if (UBlueprint* Blueprint = Cast<UBlueprint>(GeneratedClass->ClassGeneratedBy))

                    {

                        if (!SeenBlueprints.Contains(Blueprint))

                        {

                            SeenBlueprints.Add(Blueprint);

                            OutBlueprints.Add(Blueprint);

                        }

                    }

                }

            }

        }



        bool IsMovementFunctionName(const FString& FunctionName) const

        {

            return FunctionName.Contains(TEXT("SetActorLocation"))

                || FunctionName.Contains(TEXT("SetActorTransform"))

                || FunctionName.Contains(TEXT("SetWorldLocation"))

                || FunctionName.Contains(TEXT("SetWorldTransform"))

                || FunctionName.Contains(TEXT("SetRelativeLocation"))

                || FunctionName.Contains(TEXT("SetRelativeTransform"))

                || FunctionName.Contains(TEXT("AddActorWorldOffset"))

                || FunctionName.Contains(TEXT("AddActorLocalOffset"))

                || FunctionName.Contains(TEXT("AddWorldOffset"))

                || FunctionName.Contains(TEXT("AddLocalOffset"))

                || FunctionName.Contains(TEXT("MoveComponent"))

                || FunctionName.Contains(TEXT("Teleport"));

        }



        UEdGraphPin* FindPinContaining(UK2Node_CallFunction* Node, const FString& Token) const

        {

            if (!Node) return nullptr;

            for (UEdGraphPin* Pin : Node->Pins)

            {

                if (Pin && Pin->PinName.ToString().Contains(Token, ESearchCase::IgnoreCase))

                {

                    return Pin;

                }

            }

            return nullptr;

        }



        bool IsFalseLikePinDefault(const UEdGraphPin* Pin) const

        {

            if (!Pin) return false;

            const FString Value = Pin->DefaultValue.TrimStartAndEnd();

            return Value.IsEmpty() || Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Value == TEXT("0");

        }



        bool IsTeleportEnabledPinDefault(const UEdGraphPin* Pin) const

        {

            if (!Pin) return false;

            const FString Value = Pin->DefaultValue.TrimStartAndEnd();

            if (Value.IsEmpty()

                || Value.Equals(TEXT("false"), ESearchCase::IgnoreCase)

                || Value == TEXT("0")

                || Value.Equals(TEXT("None"), ESearchCase::IgnoreCase)

                || Value.Contains(TEXT("None"), ESearchCase::IgnoreCase))

            {

                return false;

            }

            return true;

        }



        void AddMovementFinding(TArray<FMovementNodeFinding>& Findings, UBlueprint* Blueprint, UEdGraphNode* Node, const FString& FunctionName, const FString& Severity, const FString& Detail) const

        {

            if (!Blueprint || !Node || Findings.Num() >= 12)

            {

                return;

            }



            FMovementNodeFinding Finding;

            Finding.Blueprint = Blueprint;

            Finding.Node = Node;

            Finding.BlueprintName = Blueprint->GetName();

            Finding.GraphName = Node->GetGraph() ? Node->GetGraph()->GetName() : TEXT("<No Graph>");

            Finding.NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();

            Finding.FunctionName = FunctionName;

            Finding.Detail = Detail;

            Finding.Severity = Severity;

            Finding.NodePosX = Node->NodePosX;

            Finding.NodePosY = Node->NodePosY;

            Findings.Add(Finding);

        }



        void ScanBlueprintForMovementRisks(UBlueprint* Blueprint, TArray<FMovementNodeFinding>& OutFindings) const

        {

            if (!Blueprint || OutFindings.Num() >= 12)

            {

                return;

            }



            TArray<UEdGraph*> Graphs;

            Blueprint->GetAllGraphs(Graphs);

            for (UEdGraph* Graph : Graphs)

            {

                if (!Graph)

                {

                    continue;

                }



                for (UEdGraphNode* GraphNode : Graph->Nodes)

                {

                    UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(GraphNode);

                    if (!CallNode)

                    {

                        continue;

                    }



                    const FString FunctionName = CallNode->FunctionReference.GetMemberName().ToString();

                    if (!IsMovementFunctionName(FunctionName))

                    {

                        continue;

                    }



                    bool bAdded = false;

                    if (FunctionName.Contains(TEXT("Teleport")))

                    {

                        AddMovementFinding(OutFindings, Blueprint, GraphNode, FunctionName, TEXT("High"), TMLoc::String(TEXT("Teleport-style movement can bypass normal swept blocking. Verify this is intentional for collision-critical movement."), TEXT("Teleport-style movement can bypass normal swept blocking. Verify this is intentional for collision-critical movement.")));

                        bAdded = true;

                    }



                    if (UEdGraphPin* SweepPin = FindPinContaining(CallNode, TEXT("Sweep")))

                    {

                        if (SweepPin->LinkedTo.Num() == 0 && IsFalseLikePinDefault(SweepPin))

                        {

                            AddMovementFinding(OutFindings, Blueprint, GraphNode, FunctionName, TEXT("High"), FString::Printf(TEXT("Sweep pin '%s' is false/unconnected. Set Sweep=true and inspect the returned FHitResult for blocking hits."), *SweepPin->PinName.ToString()));

                            bAdded = true;

                        }

                        else if (SweepPin->LinkedTo.Num() > 0)

                        {

                            AddMovementFinding(OutFindings, Blueprint, GraphNode, FunctionName, TEXT("Check"), FString::Printf(TEXT("Sweep pin '%s' is driven by another node. Confirm the runtime value becomes true when collision should block."), *SweepPin->PinName.ToString()));

                            bAdded = true;

                        }

                    }

                    else if (!bAdded && FunctionName.Contains(TEXT("MoveComponent")))

                    {

                        AddMovementFinding(OutFindings, Blueprint, GraphNode, FunctionName, TEXT("Check"), TMLoc::String(TEXT("MoveComponent-style node found. Confirm that this path uses swept movement and does not ignore the blocking component."), TEXT("MoveComponent-style node found. Confirm that this path uses swept movement and does not ignore the blocking component.")));

                        bAdded = true;

                    }



                    if (UEdGraphPin* TeleportPin = FindPinContaining(CallNode, TEXT("Teleport")))

                    {

                        if (TeleportPin->LinkedTo.Num() == 0 && IsTeleportEnabledPinDefault(TeleportPin))

                        {

                            AddMovementFinding(OutFindings, Blueprint, GraphNode, FunctionName, TEXT("High"), FString::Printf(TEXT("Teleport pin '%s' is set to '%s'. Use None/false for collision-critical swept movement."), *TeleportPin->PinName.ToString(), *TeleportPin->DefaultValue));

                        }

                        else if (TeleportPin->LinkedTo.Num() > 0)

                        {

                            AddMovementFinding(OutFindings, Blueprint, GraphNode, FunctionName, TEXT("Check"), FString::Printf(TEXT("Teleport pin '%s' is driven by another node. Confirm it resolves to None/false during blocking movement."), *TeleportPin->PinName.ToString()));

                        }

                    }

                }

            }

        }



        TArray<FMovementNodeFinding> BuildMovementNodeFindings() const

        {

            TArray<FMovementNodeFinding> Findings;

            TArray<UBlueprint*> Blueprints;

            TSet<UBlueprint*> SeenBlueprints;



            AActor* ResolvedActorA = ResolvePIECounterpart(ActorA.Get());

            AActor* ResolvedActorB = ResolvePIECounterpart(ActorB.Get());

            AddBlueprintFromClass(ResolvedActorA ? ResolvedActorA->GetClass() : nullptr, Blueprints, SeenBlueprints);

            AddBlueprintFromClass(ResolvedActorB ? ResolvedActorB->GetClass() : nullptr, Blueprints, SeenBlueprints);



            if (APawn* PawnA = Cast<APawn>(ResolvedActorA))

            {

                AddBlueprintFromClass(PawnA->GetController() ? PawnA->GetController()->GetClass() : nullptr, Blueprints, SeenBlueprints);

            }

            if (APawn* PawnB = Cast<APawn>(ResolvedActorB))

            {

                AddBlueprintFromClass(PawnB->GetController() ? PawnB->GetController()->GetClass() : nullptr, Blueprints, SeenBlueprints);

            }



            for (UBlueprint* Blueprint : Blueprints)

            {

                ScanBlueprintForMovementRisks(Blueprint, Findings);

                if (Findings.Num() >= 12)

                {

                    break;

                }

            }



            return Findings;

        }



        FLinearColor GetMovementFindingColor(const FString& Severity) const

        {

            return Severity == TEXT("High")

                ? FLinearColor(0.95f, 0.34f, 0.24f)

                : FLinearColor(0.95f, 0.72f, 0.28f);

        }



        TSharedRef<SWidget> BuildMovementNodeRiskWidget() const

        {

            const TArray<FMovementNodeFinding> Findings = BuildMovementNodeFindings();

            TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);



            Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)

            [

                SNew(STextBlock)

                .Text(TMLoc::Text(TEXT("Suspicious movement nodes"), TEXT("Suspicious movement nodes")))

                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))

                .ColorAndOpacity(FLinearColor(0.95f, 0.72f, 0.28f))

            ];



            if (Findings.IsEmpty())

            {

                Box->AddSlot().AutoHeight()

                [

                    SNew(STextBlock)

                    .Text(TMLoc::Text(TEXT("No obvious Sweep=false or Teleport movement node was found in the selected actors' Blueprint class chain."), TEXT("No obvious Sweep=false or Teleport movement node was found in the selected actors' Blueprint class chain.")))

                    .AutoWrapText(true)

                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                    .ColorAndOpacity(FLinearColor(0.70f, 0.73f, 0.78f))

                ];

            }

            else

            {

                for (const FMovementNodeFinding& Finding : Findings)

                {

                    const FLinearColor Color = GetMovementFindingColor(Finding.Severity);

                    Box->AddSlot().AutoHeight().Padding(0, 3)

                    [

                        SNew(SBorder)

                        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                        .BorderBackgroundColor(FLinearColor(Color.R * 0.13f, Color.G * 0.13f, Color.B * 0.13f, 1.0f))

                        .Padding(7.0f)

                        [

                            SNew(SHorizontalBox)

                            + SHorizontalBox::Slot().FillWidth(1.0f)

                            [

                                SNew(SVerticalBox)

                                + SVerticalBox::Slot().AutoHeight()

                                [

                                    SNew(STextBlock)

                                    .Text(FText::FromString(FString::Printf(TEXT("[%s] %s / %s"), *Finding.Severity, *Finding.BlueprintName, *Finding.GraphName)))

                                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                                    .ColorAndOpacity(Color)

                                ]

                                + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)

                                [

                                    SNew(STextBlock)

                                    .Text(FText::FromString(FString::Printf(TEXT("Node: %s  (%s)  Pos: %d, %d"), *Finding.NodeTitle, *Finding.FunctionName, Finding.NodePosX, Finding.NodePosY)))

                                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                                ]

                                + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)

                                [

                                    SNew(STextBlock)

                                    .Text(FText::FromString(Finding.Detail))

                                    .AutoWrapText(true)

                                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                                    .ColorAndOpacity(FLinearColor(0.76f, 0.78f, 0.82f))

                                ]

                            ]

                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)

                            [

                                SNew(SButton)

                                .Text(TMLoc::Text(TEXT("Open Node"), TEXT("Open Node")))

                                .OnClicked_Lambda([Finding]()

                                {

                                    if (UEdGraphNode* Node = Finding.Node.Get())

                                    {

                                        FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node);

                                    }

                                    return FReply::Handled();

                                })

                            ]

                        ]

                    ];

                }

            }



            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))

                .BorderBackgroundColor(FLinearColor(0.11f, 0.085f, 0.045f, 1.0f))

                .Padding(10.0f)

                [

                    Box

                ];

        }

        void AddUniqueTip(TArray<FString>& Tips, const FString& Tip) const

        {

            if (!Tip.IsEmpty() && !Tips.Contains(Tip) && Tips.Num() < 8)

            {

                Tips.Add(Tip);

            }

        }



        TArray<FString> BuildCollisionFixTips() const

        {

            TArray<FString> Tips;



            for (const FCollisionFinding& Finding : LastResult.ActorFindings)

            {

                if (Finding.Title == TEXT("Actor collision is disabled"))

                {

                    AddUniqueTip(Tips, FString::Printf(TEXT("Actor level: enable Actor > Collision > Actor Enable Collision on the disabled side. Detail: %s"), *Finding.Detail));

                }

                else if (Finding.Title == TEXT("Actors are in different worlds"))

                {

                    AddUniqueTip(Tips, TMLoc::String(TEXT("Capture both actors from the same PIE/Game world. Editor-world and PIE-world actors cannot collide with each other."), TEXT("Capture both actors from the same PIE/Game world. Editor-world and PIE-world actors cannot collide with each other.")));

                }

                else if (Finding.Title == TEXT("No primitive collision component"))

                {

                    AddUniqueTip(Tips, TMLoc::String(TEXT("Add or enable a PrimitiveComponent with collision, such as CapsuleComponent, BoxComponent, StaticMeshComponent, or BrushComponent."), TEXT("Add or enable a PrimitiveComponent with collision, such as CapsuleComponent, BoxComponent, StaticMeshComponent, or BrushComponent.")));

                }

            }



            if (LastResult.BlockConfiguredPairCount > 0)

            {

                const FString RuntimeTip = LastResult.SweepBlockingPairCount > 0

                    ? TMLoc::String(TEXT("A BLOCK-capable pair already exists and the sweep probe hit it. Usually no collision preset change is needed; if gameplay still passes through, check Sweep=true, Teleport=false, and custom ignore logic."), TEXT("A BLOCK-capable pair already exists and the sweep probe hit it. Usually no collision preset change is needed; if gameplay still passes through, check Sweep=true, Teleport=false, and custom ignore logic."))

                    : TMLoc::String(TEXT("A BLOCK-capable pair already exists. If gameplay still passes through, move the actor/component with Sweep=true, avoid Teleport, and make sure the movement component uses the blocking primitive as its UpdatedComponent/root collision."), TEXT("A BLOCK-capable pair already exists. If gameplay still passes through, move the actor/component with Sweep=true, avoid Teleport, and make sure the movement component uses the blocking primitive as its UpdatedComponent/root collision."));

                AddUniqueTip(Tips, RuntimeTip);

            }



            for (const FCollisionPairReport& Pair : LastResult.PairReports)

            {

                if (Tips.Num() >= 8)

                {

                    break;

                }



                const FString PairName = FString::Printf(TEXT("%s <-> %s"), *Pair.ComponentALabel, *Pair.ComponentBLabel);



                if (Pair.bBlockConfigured)

                {

                    continue;

                }



                if (Pair.EnabledA == TEXT("NoCollision"))

                {

                    AddUniqueTip(Tips, FString::Printf(TEXT("%s: set A component Collision Enabled to QueryOnly for swept movement, or QueryAndPhysics if it also simulates physics."), *Pair.ComponentALabel));

                }

                if (Pair.EnabledB == TEXT("NoCollision"))

                {

                    AddUniqueTip(Tips, FString::Printf(TEXT("%s: set B component Collision Enabled to QueryOnly for swept movement, or QueryAndPhysics if it also simulates physics."), *Pair.ComponentBLabel));

                }



                if (Pair.ResponseAtoB != TEXT("Block"))

                {

                    AddUniqueTip(Tips, FString::Printf(TEXT("%s: set A response to B's object channel '%s' from %s to Block."), *PairName, *Pair.ObjectTypeB, *Pair.ResponseAtoB));

                }

                if (Pair.ResponseBtoA != TEXT("Block"))

                {

                    AddUniqueTip(Tips, FString::Printf(TEXT("%s: set B response to A's object channel '%s' from %s to Block."), *PairName, *Pair.ObjectTypeA, *Pair.ResponseBtoA));

                }



                for (const FCollisionFinding& Finding : Pair.Findings)

                {

                    if (Finding.Title == TEXT("Collision modes do not share a common path"))

                    {

                        AddUniqueTip(Tips, FString::Printf(TEXT("%s: for movement blocking, both sides need query collision. Use QueryOnly or QueryAndPhysics on both relevant components."), *PairName));

                    }

                    else if (Finding.Title == TEXT("MoveComponent ignore list excludes this pair"))

                    {

                        AddUniqueTip(Tips, FString::Printf(TEXT("%s: remove IgnoreActorWhenMoving / IgnoreComponentWhenMoving entries between these two components or actors."), *PairName));

                    }

                    else if (Finding.Title == TEXT("Collision geometry may be missing"))

                    {

                        AddUniqueTip(Tips, FString::Printf(TEXT("%s: add simple collision to the mesh/body setup, or enable a valid collision complexity mode such as UseComplexAsSimple where appropriate."), *PairName));

                    }

                }

            }



            if (Tips.IsEmpty())

            {

                AddUniqueTip(Tips, TMLoc::String(TEXT("No direct collision-option fix was detected. Prefer testing the actual movement path with Sweep=true and inspect the returned FHitResult."), TEXT("No direct collision-option fix was detected. Prefer testing the actual movement path with Sweep=true and inspect the returned FHitResult.")));

            }



            return Tips;

        }



        TSharedRef<SWidget> BuildCollisionFixTipsWidget() const

        {

            const TArray<FString> Tips = BuildCollisionFixTips();

            TSharedRef<SVerticalBox> TipBox = SNew(SVerticalBox);



            TipBox->AddSlot().AutoHeight().Padding(0, 0, 0, 6)

            [

                SNew(STextBlock)

                .Text(TMLoc::Text(TEXT("Quick fix tips"), TEXT("Quick fix tips")))

                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))

                .ColorAndOpacity(FLinearColor(0.95f, 0.78f, 0.35f))

            ];



            for (int32 Index = 0; Index < Tips.Num(); ++Index)

            {

                TipBox->AddSlot().AutoHeight().Padding(0, 2)

                [

                    SNew(STextBlock)

                    .Text(FText::FromString(FString::Printf(TEXT("%d. %s"), Index + 1, *Tips[Index])))

                    .AutoWrapText(true)

                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                    .ColorAndOpacity(FLinearColor(0.82f, 0.84f, 0.88f))

                ];

            }



            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))

                .BorderBackgroundColor(FLinearColor(0.14f, 0.11f, 0.055f, 1.0f))

                .Padding(10.0f)

                [

                    TipBox

                ];

        }



        TSharedRef<SWidget> BuildMetricCard(const FText& Label, const FText& Value, const FLinearColor& ValueColor) const

        {

            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                .BorderBackgroundColor(FLinearColor(0.10f, 0.105f, 0.115f, 1.0f))

                .Padding(FMargin(10.0f, 8.0f))

                [

                    SNew(SBox)

                    .MinDesiredHeight(46.0f)

                    [

                        SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight()

                        [

                            SNew(STextBlock)

                            .Text(Label)

                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                            .ColorAndOpacity(FLinearColor(0.62f, 0.65f, 0.70f))

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)

                        [

                            SNew(STextBlock)

                            .Text(Value)

                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 12))

                            .ColorAndOpacity(ValueColor)

                        ]

                    ]

                ];

        }



        TSharedRef<SWidget> BuildDashboardFindingRow(const FCollisionFinding& Finding) const

        {

            const FLinearColor Color = GetSeverityColor(Finding.Severity);

            FText BadgeText;

            switch (Finding.Severity)

            {

            case ECollisionFindingSeverity::Cause: BadgeText = TMLoc::Text(TEXT("Cause"), TEXT("Cause")); break;

            case ECollisionFindingSeverity::Warning: BadgeText = TMLoc::Text(TEXT("Check"), TEXT("Check")); break;

            case ECollisionFindingSeverity::Pass: BadgeText = TMLoc::Text(TEXT("Pass"), TEXT("Pass")); break;

            default: BadgeText = TMLoc::Text(TEXT("Info"), TEXT("Info")); break;

            }



            return SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0, 1, 8, 0)

                [

                    SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                    .BorderBackgroundColor(FLinearColor(Color.R * 0.22f, Color.G * 0.22f, Color.B * 0.22f, 1.0f))

                    .Padding(FMargin(5.0f, 2.0f))

                    [

                        SNew(STextBlock)

                        .Text(BadgeText)

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 7))

                        .ColorAndOpacity(Color)

                    ]

                ]

                + SHorizontalBox::Slot().FillWidth(1.0f)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        SNew(STextBlock)

                        .Text(FText::FromString(LocalizedFindingTitle(Finding.Title)))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 1, 0, 0)

                    [

                        SNew(STextBlock)

                        .Text(FText::FromString(Finding.Detail))

                        .AutoWrapText(true)

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                        .ColorAndOpacity(FLinearColor(0.68f, 0.70f, 0.74f))

                    ]

                ];

        }



        FString GetPairShortComponentName(const TWeakObjectPtr<UPrimitiveComponent>& Component, const FString& Fallback) const
        {
            if (const UPrimitiveComponent* Primitive = Component.Get())
            {
                return Primitive->GetName();
            }
            const int32 DotIndex = Fallback.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
            return DotIndex != INDEX_NONE ? Fallback.Mid(DotIndex + 1) : Fallback;
        }

        FString GetPairVerdictText(const FCollisionPairReport& Pair) const
        {
            if (Pair.bBlockConfigured)
            {
                return TMLoc::String(TEXT("Block-capable"), TEXT("Block-capable"));
            }
            if (Pair.ResponseAtoB == TEXT("Ignore") || Pair.ResponseBtoA == TEXT("Ignore"))
            {
                return TEXT("Ignore");
            }
            if (Pair.EnabledA == TEXT("NoCollision") || Pair.EnabledB == TEXT("NoCollision"))
            {
                return TMLoc::String(TEXT("Block impossible"), TEXT("Block impossible"));
            }
            return TMLoc::String(TEXT("Auxiliary"), TEXT("Auxiliary"));
        }

        FLinearColor GetPairVerdictColor(const FCollisionPairReport& Pair) const
        {
            if (Pair.bBlockConfigured)
            {
                return FLinearColor(0.10f, 0.72f, 0.18f, 1.0f);
            }
            if (Pair.ResponseAtoB == TEXT("Ignore") || Pair.ResponseBtoA == TEXT("Ignore"))
            {
                return FLinearColor(0.95f, 0.62f, 0.18f, 1.0f);
            }
            if (Pair.EnabledA == TEXT("NoCollision") || Pair.EnabledB == TEXT("NoCollision"))
            {
                return FLinearColor(0.90f, 0.24f, 0.28f, 1.0f);
            }
            return FLinearColor(0.62f, 0.65f, 0.70f, 1.0f);
        }

        TSharedRef<SWidget> BuildSoftMetricCard(const FText& Label, const FText& Value, const FLinearColor& Accent, const bool bEmphasizeBackground = false) const
        {
            const FLinearColor Background = bEmphasizeBackground
                ? FLinearColor(Accent.R * 0.22f + 0.02f, Accent.G * 0.22f + 0.02f, Accent.B * 0.22f + 0.02f, 1.0f)
                : FLinearColor(0.985f, 0.985f, 0.965f, 1.0f);
            return SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .BorderBackgroundColor(Background)
                .Padding(FMargin(14.0f, 10.0f))
                [
                    SNew(SBox)
                    .MinDesiredHeight(46.0f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(Label)
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
                            .ColorAndOpacity(FLinearColor(0.34f, 0.34f, 0.34f))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                        [
                            SNew(STextBlock)
                            .Text(Value)
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))
                            .ColorAndOpacity(Accent)
                        ]
                    ]
                ];
        }

        TSharedRef<SWidget> BuildActorPairLine() const
        {
            return SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
                    .BorderBackgroundColor(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f))
                    .Padding(FMargin(8.0f, 3.0f))
                    [
                        SNew(STextBlock).Text(TMLoc::Text(TEXT("A"), TEXT("A"))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0)
                [
                    SNew(STextBlock).Text(FText::FromString(LastResult.ActorAName)).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0)
                [
                    SNew(STextBlock).Text(TMLoc::Text(TEXT(">"), TEXT(">"))).ColorAndOpacity(FLinearColor(0.42f, 0.42f, 0.42f))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
                    .BorderBackgroundColor(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f))
                    .Padding(FMargin(8.0f, 3.0f))
                    [
                        SNew(STextBlock).Text(TMLoc::Text(TEXT("B"), TEXT("B"))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0)
                [
                    SNew(STextBlock).Text(FText::FromString(LastResult.ActorBName)).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))
                ];
        }

        TSharedRef<SWidget> BuildPairVerdictPill(const FCollisionPairReport& Pair) const
        {
            const FLinearColor Color = GetPairVerdictColor(Pair);
            return SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
                .BorderBackgroundColor(FLinearColor(Color.R * 0.25f + 0.72f, Color.G * 0.25f + 0.72f, Color.B * 0.25f + 0.72f, 1.0f))
                .Padding(FMargin(8.0f, 2.0f))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(GetPairVerdictText(Pair)))
                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))
                    .ColorAndOpacity(Color)
                ];
        }

        TSharedRef<SWidget> BuildComponentPairTable() const
        {
            TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
            auto AddCell = [](const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color)
            {
                return SNew(STextBlock)
                    .Text(FText::FromString(Text))
                    .Font(Font)
                    .ColorAndOpacity(Color);
            };

            Rows->AddSlot().AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
                .BorderBackgroundColor(FLinearColor(0.985f, 0.985f, 0.975f, 1.0f))
                .Padding(FMargin(12.0f, 8.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(0.38f)[AddCell(TMLoc::String(TEXT("A component"), TEXT("A component")), FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8), FLinearColor(0.28f, 0.28f, 0.28f))]
                    + SHorizontalBox::Slot().FillWidth(0.38f)[AddCell(TMLoc::String(TEXT("B component"), TEXT("B component")), FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8), FLinearColor(0.28f, 0.28f, 0.28f))]
                    + SHorizontalBox::Slot().FillWidth(0.24f)[AddCell(TMLoc::String(TEXT("Verdict"), TEXT("Verdict")), FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8), FLinearColor(0.28f, 0.28f, 0.28f))]
                ]
            ];

            for (const FCollisionPairReport& Pair : LastResult.PairReports)
            {
                const bool bImportant = Pair.bBlockConfigured;
                const FLinearColor RowBg = bImportant ? FLinearColor(0.78f, 0.93f, 0.76f, 1.0f) : FLinearColor(1.0f, 1.0f, 0.985f, 1.0f);
                Rows->AddSlot().AutoHeight()
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
                    .BorderBackgroundColor(RowBg)
                    .Padding(FMargin(12.0f, 7.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(0.38f).VAlign(VAlign_Center)
                        [AddCell(GetPairShortComponentName(Pair.ComponentA, Pair.ComponentALabel), FCoreStyle::GetDefaultFontStyle(bImportant ? TEXT("Bold") : TEXT("Regular"), 8), FLinearColor(0.08f, 0.08f, 0.08f))]
                        + SHorizontalBox::Slot().FillWidth(0.38f).VAlign(VAlign_Center)
                        [AddCell(GetPairShortComponentName(Pair.ComponentB, Pair.ComponentBLabel), FCoreStyle::GetDefaultFontStyle(bImportant ? TEXT("Bold") : TEXT("Regular"), 8), FLinearColor(0.08f, 0.08f, 0.08f))]
                        + SHorizontalBox::Slot().FillWidth(0.24f).VAlign(VAlign_Center)
                        [BuildPairVerdictPill(Pair)]
                    ]
                ];
            }

            return SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .BorderBackgroundColor(FLinearColor::White)
                .Padding(0.0f)
                [Rows];
        }

        TSharedRef<SWidget> BuildActionTipCard(const FString& Tip, const bool bPrimary) const
        {
            const FLinearColor Color = bPrimary ? FLinearColor(0.12f, 0.62f, 0.18f) : FLinearColor(0.78f, 0.52f, 0.10f);
            return SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 0.985f, 1.0f))
                .Padding(FMargin(10.0f, 8.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0, 0, 8, 0)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(bPrimary ? TEXT("?") : TEXT("?")))
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
                        .ColorAndOpacity(Color)
                    ]
                    + SHorizontalBox::Slot().FillWidth(1.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Tip))
                        .AutoWrapText(true)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
                        .ColorAndOpacity(FLinearColor(0.10f, 0.10f, 0.10f))
                    ]
                ];
        }

        TSharedRef<SWidget> BuildSimpleActionTips() const
        {
            const TArray<FString> Tips = BuildCollisionFixTips();
            TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
            Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
            [
                SNew(STextBlock)
                .Text(TMLoc::Text(TEXT("Actions you can try now"), TEXT("Actions you can try now")))
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
                .ColorAndOpacity(FLinearColor(0.12f, 0.12f, 0.12f))
            ];
            for (int32 Index = 0; Index < Tips.Num(); ++Index)
            {
                Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
                [BuildActionTipCard(Tips[Index], Index == 0)];
            }
            return Box;
        }

        TSharedRef<SWidget> BuildDashboardPairWidget(const FCollisionPairReport& Pair) const
        {
            const FLinearColor StateColor = Pair.bBlockConfigured
                ? FLinearColor(0.24f, 0.72f, 0.35f)
                : FLinearColor(0.90f, 0.30f, 0.25f);
            const UPrimitiveComponent* ComponentA = Pair.ComponentA.Get();
            const UPrimitiveComponent* ComponentB = Pair.ComponentB.Get();
            const FString ComponentNameA = ComponentA ? ComponentA->GetName() : Pair.ComponentALabel;
            const FString ComponentNameB = ComponentB ? ComponentB->GetName() : Pair.ComponentBLabel;
            const FString OwnerNameA = ComponentA ? GetCollisionActorLabelSafe(ComponentA->GetOwner()) : LastResult.ActorAName;
            const FString OwnerNameB = ComponentB ? GetCollisionActorLabelSafe(ComponentB->GetOwner()) : LastResult.ActorBName;

            TSharedRef<SVerticalBox> FindingsBox = SNew(SVerticalBox);
            for (const FCollisionFinding& Finding : Pair.Findings)
            {
                FindingsBox->AddSlot().AutoHeight().Padding(0, 2)[BuildDashboardFindingRow(Finding)];
            }

            return SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .BorderBackgroundColor(FLinearColor(0.075f, 0.08f, 0.09f, 1.0f))
                .Padding(10.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f)
                        [SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s  <->  %s"), *ComponentNameA, *ComponentNameB))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))]
                        + SHorizontalBox::Slot().AutoWidth()
                        [BuildPairVerdictPill(Pair)]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 7)
                    [SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("A: %s   |   B: %s"), *OwnerNameA, *OwnerNameB))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8)).ColorAndOpacity(FLinearColor(0.58f, 0.62f, 0.68f))]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
                        [BuildMetricCard(FText::FromString(FString::Printf(TEXT("A Profile: %s"), *Pair.ProfileA)), FText::FromString(FString::Printf(TEXT("%s | %s -> %s"), *Pair.EnabledA, *Pair.ObjectTypeA, *Pair.ResponseAtoB)), FLinearColor(0.82f, 0.84f, 0.88f))]
                        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(4, 0, 0, 0)
                        [BuildMetricCard(FText::FromString(FString::Printf(TEXT("B Profile: %s"), *Pair.ProfileB)), FText::FromString(FString::Printf(TEXT("%s | %s -> %s"), *Pair.EnabledB, *Pair.ObjectTypeB, *Pair.ResponseBtoA)), FLinearColor(0.82f, 0.84f, 0.88f))]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 6)[SNew(SSeparator)]
                    + SVerticalBox::Slot().AutoHeight()[FindingsBox]
                ];
        }

        FString GetCollisionEvidenceLevel() const
        {
            const bool bHasActorCause = Algo::CountIf(LastResult.ActorFindings, [](const FCollisionFinding& Finding)
            {
                return Finding.Severity == ECollisionFindingSeverity::Cause;
            }) > 0;

            if (bHasActorCause || LastResult.BlockConfiguredPairCount == 0)
            {
                return TMLoc::String(TEXT("High confidence: Block is not possible with current inspected settings"), TEXT("High confidence: Block is not possible with current inspected settings"));
            }
            if (LastResult.SweepBlockingPairCount > 0)
            {
                return TMLoc::String(TEXT("High confidence for swept query blocking; gameplay path still must use Sweep/FHitResult"), TEXT("High confidence for swept query blocking; gameplay path still must use Sweep/FHitResult"));
            }
            if (LastResult.ShapeOverlapPairCount > 0)
            {
                return TMLoc::String(TEXT("Medium confidence: current query contact exists, but movement blocking path is unverified"), TEXT("Medium confidence: current query contact exists, but movement blocking path is unverified"));
            }
            return TMLoc::String(TEXT("Medium confidence: settings allow Block, but actual movement blocking is unverified"), TEXT("Medium confidence: settings allow Block, but actual movement blocking is unverified"));
        }

        TSharedRef<SWidget> BuildRiskDisclosureWidget() const
        {
            TArray<FString> Limitations;
            Limitations.Add(TMLoc::String(TEXT("Block possible means both components are configured to block. It does not prove that the real gameplay movement will be stopped."), TEXT("Block possible means both components are configured to block. It does not prove that the real gameplay movement will be stopped.")));
            Limitations.Add(TMLoc::String(TEXT("Sweep=false, Teleport, custom ignore lists, or a MovementComponent using a non-blocking UpdatedComponent can bypass otherwise valid blocking collision."), TEXT("Sweep=false, Teleport, custom ignore lists, or a MovementComponent using a non-blocking UpdatedComponent can bypass otherwise valid blocking collision.")));
            Limitations.Add(TMLoc::String(TEXT("PhysicsOnly contacts, Chaos simulation details, complex BodySetup cases, and custom primitive implementations cannot be fully proven by this panel."), TEXT("PhysicsOnly contacts, Chaos simulation details, complex BodySetup cases, and custom primitive implementations cannot be fully proven by this panel.")));
            Limitations.Add(TMLoc::String(TEXT("Best confirmation: reproduce the move with Sweep=true and inspect the returned FHitResult, hit component, and blocking hit flag."), TEXT("Best confirmation: reproduce the move with Sweep=true and inspect the returned FHitResult, hit component, and blocking hit flag.")));

            TSharedRef<SVerticalBox> LimitationBox = SNew(SVerticalBox);
            for (const FString& Limitation : Limitations)
            {
                LimitationBox->AddSlot().AutoHeight().Padding(0, 2)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("- %s"), *Limitation)))
                    .AutoWrapText(true)
                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
                    .ColorAndOpacity(FLinearColor(0.36f, 0.30f, 0.18f))
                ];
            }

            return SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .BorderBackgroundColor(FLinearColor(1.0f, 0.94f, 0.78f, 1.0f))
                .Padding(FMargin(12.0f, 10.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(GetCollisionEvidenceLevel()))
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
                        .ColorAndOpacity(FLinearColor(0.52f, 0.32f, 0.04f))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("This panel reports evidence and likely causes, not an absolute collision verdict."), TEXT("This panel reports evidence and likely causes, not an absolute collision verdict.")))
                        .AutoWrapText(true)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
                        .ColorAndOpacity(FLinearColor(0.40f, 0.32f, 0.18f))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
                    [
                        LimitationBox
                    ]
                ];
        }

        TSharedRef<SWidget> BuildDashboardWidget() const
        {
            const int32 TotalPairs = LastResult.PairReports.Num();
            const int32 NonBlockingPairs = FMath::Max(0, TotalPairs - LastResult.BlockConfiguredPairCount);
            const int32 ContactOrSweepEvidencePairs = LastResult.ShapeOverlapPairCount + LastResult.SweepBlockingPairCount;
            const bool bHasActorCause = Algo::CountIf(LastResult.ActorFindings, [](const FCollisionFinding& Finding)
            {
                return Finding.Severity == ECollisionFindingSeverity::Cause;
            }) > 0;
            const bool bHasBlockingPath = LastResult.BlockConfiguredPairCount > 0 && !bHasActorCause;
            const bool bSweepConfirmed = LastResult.SweepBlockingPairCount > 0;
            const FLinearColor ResultColor = bHasBlockingPath ? FLinearColor(0.20f, 0.70f, 0.25f) : FLinearColor(0.88f, 0.24f, 0.28f);
            const FString ResultTitle = bHasActorCause
                ? TMLoc::String(TEXT("Diagnosis: actor/world state prevents collision"), TEXT("Diagnosis: actor/world state prevents collision"))
                : (bHasBlockingPath
                    ? (bSweepConfirmed ? TMLoc::String(TEXT("Diagnosis: swept query blocking was observed"), TEXT("Diagnosis: swept query blocking was observed")) : TMLoc::String(TEXT("Diagnosis: Block-capable pair exists; movement path unverified"), TEXT("Diagnosis: Block-capable pair exists; movement path unverified")))
                    : TMLoc::String(TEXT("Diagnosis: no Block-capable pair found"), TEXT("Diagnosis: no Block-capable pair found")));
            const FString SummaryText = bHasBlockingPath
                ? FString::Printf(TEXT("%d component pair(s) inspected: %d are configured to Block, %d cannot Block or are auxiliary. This is configuration/query evidence only; actual movement blocking depends on Sweep, Teleport, UpdatedComponent, ignore lists, and runtime physics state."), TotalPairs, LastResult.BlockConfiguredPairCount, NonBlockingPairs)
                : FString::Printf(TEXT("%d component pair(s) inspected. No Block-capable pair was found in inspected settings, so blocking is unlikely until the listed causes are fixed."), TotalPairs);

            TSharedRef<SVerticalBox> Root = SNew(SVerticalBox);
            Root->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .BorderBackgroundColor(bHasBlockingPath ? FLinearColor(0.80f, 0.95f, 0.79f, 1.0f) : FLinearColor(1.0f, 0.82f, 0.82f, 1.0f))
                .Padding(FMargin(16.0f, 14.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0, 0, 12, 0)
                    [SNew(STextBlock).Text(FText::FromString(bHasBlockingPath ? TEXT("?") : TEXT("!"))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18)).ColorAndOpacity(ResultColor)]
                    + SHorizontalBox::Slot().FillWidth(1.0f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [SNew(STextBlock).Text(FText::FromString(ResultTitle)).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 12)).ColorAndOpacity(FLinearColor(0.05f, 0.35f, 0.08f))]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
                        [SNew(STextBlock).Text(FText::FromString(SummaryText)).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9)).ColorAndOpacity(FLinearColor(0.20f, 0.25f, 0.20f))]
                    ]
                ]
            ];

            Root->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[BuildRiskDisclosureWidget()];

            Root->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 6, 0)[BuildSoftMetricCard(TMLoc::Text(TEXT("Inspected pairs"), TEXT("Inspected pairs")), FText::AsNumber(TotalPairs), FLinearColor(0.05f, 0.05f, 0.05f))]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(6, 0)[BuildSoftMetricCard(TMLoc::Text(TEXT("Block impossible"), TEXT("Block impossible")), FText::AsNumber(NonBlockingPairs), FLinearColor(0.78f, 0.16f, 0.20f), NonBlockingPairs > 0)]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(6, 0)[BuildSoftMetricCard(TMLoc::Text(TEXT("Block-capable settings"), TEXT("Block-capable settings")), FText::AsNumber(LastResult.BlockConfiguredPairCount), FLinearColor(0.05f, 0.55f, 0.10f), LastResult.BlockConfiguredPairCount > 0)]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(6, 0, 0, 0)[BuildSoftMetricCard(TMLoc::Text(TEXT("Contact / sweep evidence"), TEXT("Contact / sweep evidence")), FText::AsNumber(ContactOrSweepEvidencePairs), ContactOrSweepEvidencePairs > 0 ? FLinearColor(0.05f, 0.55f, 0.10f) : FLinearColor(0.10f, 0.10f, 0.10f))]
            ];

            Root->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[BuildActorPairLine()];
            Root->AddSlot().AutoHeight().Padding(0, 0, 0, 14)[BuildComponentPairTable()];
            Root->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[BuildSimpleActionTips()];
            Root->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[BuildMovementNodeRiskWidget()];

            if (!LastResult.ActorFindings.IsEmpty())
            {
                TSharedRef<SVerticalBox> ActorFindingBox = SNew(SVerticalBox);
                for (const FCollisionFinding& Finding : LastResult.ActorFindings)
                {
                    ActorFindingBox->AddSlot().AutoHeight().Padding(0, 2)[BuildDashboardFindingRow(Finding)];
                }
                Root->AddSlot().AutoHeight().Padding(0, 0, 0, 10)
                [
                    SNew(SExpandableArea)
                    .InitiallyCollapsed(true)
                    .HeaderContent()[SNew(STextBlock).Text(TMLoc::Text(TEXT("Actor-level and movement checks"), TEXT("Actor-level and movement checks"))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))]
                    .BodyContent()[ActorFindingBox]
                ];
            }

            TSharedRef<SVerticalBox> DetailedPairsBox = SNew(SVerticalBox);
            for (const FCollisionPairReport& Pair : LastResult.PairReports)
            {
                DetailedPairsBox->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[BuildDashboardPairWidget(Pair)];
            }
            Root->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
            [
                SNew(SExpandableArea)
                .InitiallyCollapsed(true)
                .HeaderContent()[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Details: component pair debug (%d pair(s))"), TotalPairs))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9)).ColorAndOpacity(FLinearColor(0.38f, 0.40f, 0.44f))]
                .BodyContent()[DetailedPairsBox]
            ];
            return Root;
        }

        FString BuildReportText() const

        {

            if (!bHasResult)

            {

                return TMReportFormatter::BuildWrappedLegacyReport(

                    TEXT("Collision Pair Analyzer"),

                    TEXT("- No collision analysis result is currently available."),

                    TEXT("Run an A/B collision analysis first."));

            }



            TArray<FString> Lines;

            Lines.Add(TMLoc::String(TEXT("## Evidence level and limitations"), TEXT("## Evidence level and limitations")));
            Lines.Add(FString::Printf(TEXT("- %s"), *GetCollisionEvidenceLevel()));
            Lines.Add(TMLoc::String(TEXT("- Block possible means settings are compatible with blocking; it is not proof that the real gameplay movement path blocks."), TEXT("- Block possible means settings are compatible with blocking; it is not proof that the real gameplay movement path blocks.")));
            Lines.Add(TMLoc::String(TEXT("- Actual movement can bypass collision through Sweep=false, Teleport, custom ignore lists, or MovementComponent UpdatedComponent/root mismatch."), TEXT("- Actual movement can bypass collision through Sweep=false, Teleport, custom ignore lists, or MovementComponent UpdatedComponent/root mismatch.")));
            Lines.Add(TMLoc::String(TEXT("- PhysicsOnly, Chaos contact resolution, complex BodySetup, and custom primitive collision cannot be fully proven by static/query inspection."), TEXT("- PhysicsOnly, Chaos contact resolution, complex BodySetup, and custom primitive collision cannot be fully proven by static/query inspection.")));
            Lines.Add(TMLoc::String(TEXT("- Best confirmation is a reproduced swept move with Sweep=true and the returned FHitResult."), TEXT("- Best confirmation is a reproduced swept move with Sweep=true and the returned FHitResult.")));
            Lines.Add(TEXT(""));

            Lines.Add(TMLoc::String(TEXT("## Quick fix tips"), TEXT("## Quick fix tips")));

            const TArray<FString> Tips = BuildCollisionFixTips();

            for (int32 TipIndex = 0; TipIndex < Tips.Num(); ++TipIndex)

            {

                Lines.Add(FString::Printf(TEXT("%d. %s"), TipIndex + 1, *Tips[TipIndex]));

            }

            Lines.Add(TEXT(""));

            Lines.Add(TMLoc::String(TEXT("## Suspicious movement nodes"), TEXT("## Suspicious movement nodes")));

            const TArray<FMovementNodeFinding> MovementFindings = BuildMovementNodeFindings();

            if (MovementFindings.IsEmpty())

            {

                Lines.Add(TMLoc::String(TEXT("- No obvious Sweep=false or Teleport movement node was found in the selected actors' Blueprint class chain."), TEXT("- No obvious Sweep=false or Teleport movement node was found in the selected actors' Blueprint class chain.")));

            }

            else

            {

                for (const FMovementNodeFinding& MovementFinding : MovementFindings)

                {

                    Lines.Add(FString::Printf(TEXT("- [%s] %s / %s / %s (%s) Pos=%d,%d: %s"),

                        *MovementFinding.Severity,

                        *MovementFinding.BlueprintName,

                        *MovementFinding.GraphName,

                        *MovementFinding.NodeTitle,

                        *MovementFinding.FunctionName,

                        MovementFinding.NodePosX,

                        MovementFinding.NodePosY,

                        *MovementFinding.Detail));

                }

            }

            Lines.Add(TEXT(""));



            Lines.Add(TMLoc::String(TEXT("## Actor-level findings"), TEXT("## Actor-level findings")));

            for (const FCollisionFinding& Finding : LastResult.ActorFindings)

            {

                Lines.Add(FString::Printf(TEXT("- [%s] %s: %s"), *GetSeverityText(Finding.Severity), *LocalizedFindingTitle(Finding.Title), *Finding.Detail));

            }

            for (const FCollisionPairReport& Pair : LastResult.PairReports)

            {

                Lines.Add(TEXT(""));

                Lines.Add(FString::Printf(TEXT("## %s <-> %s [%s]"), *Pair.ComponentALabel, *Pair.ComponentBLabel, Pair.bBlockConfigured ? TEXT("BLOCK-CAPABLE") : TEXT("NO BLOCK CONFIG")));

                Lines.Add(FString::Printf(TEXT("A=%s/%s/%s->%s | B=%s/%s/%s->%s"),

                    *Pair.ProfileA, *Pair.EnabledA, *Pair.ObjectTypeA, *Pair.ResponseAtoB,

                    *Pair.ProfileB, *Pair.EnabledB, *Pair.ObjectTypeB, *Pair.ResponseBtoA));

                for (const FCollisionFinding& Finding : Pair.Findings)

                {

                    Lines.Add(FString::Printf(TEXT("- [%s] %s: %s"), *GetSeverityText(Finding.Severity), *LocalizedFindingTitle(Finding.Title), *Finding.Detail));

                }

            }



            TArray<TMReportFormatter::FMetadataItem> Metadata;

            Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Actor A"), LastResult.ActorAName));

            Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Actor B"), LastResult.ActorBName));

            Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Primitive Pairs"), FString::FromInt(LastResult.PairReports.Num())));

            Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Block-capable Pairs"), FString::FromInt(LastResult.BlockConfiguredPairCount)));

            Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Shape Contacts"), FString::FromInt(LastResult.ShapeOverlapPairCount)));

            Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Sweep Hits"), FString::FromInt(LastResult.SweepBlockingPairCount)));



            return TMReportFormatter::BuildWrappedLegacyReport(

                TEXT("Collision Pair Analyzer"),

                FString::Printf(TEXT("- A: %s\n- B: %s\n- Summary: %s\n- Primitive pairs: %d; Block-capable: %d; Shape contacts: %d; Sweep hits: %d\n- Evidence level: %s\n- Important: this analysis separates Block-capable settings from verified gameplay movement blocking."),

                    *LastResult.ActorAName,

                    *LastResult.ActorBName,

                    *LastResult.Summary,

                    LastResult.PairReports.Num(),

                    LastResult.BlockConfiguredPairCount,

                    LastResult.ShapeOverlapPairCount,

                    LastResult.SweepBlockingPairCount,

                    *GetCollisionEvidenceLevel()),

                FString::Join(Lines, TEXT("\n")),

                Metadata);

        }



        TWeakObjectPtr<AActor> ActorA;

        TWeakObjectPtr<AActor> ActorB;

        FCollisionAnalysisResult LastResult;

        TSharedPtr<SScrollBox> ResultScrollBox;

        FString StatusMessage;

        bool bHasResult = false;

    };



    TWeakPtr<SDockTab> ExistingCollisionAnalyzerTab;

    TWeakPtr<SCollisionPairAnalyzerWidget> ExistingCollisionAnalyzerWidget;

    bool bCollisionAnalyzerTabRegistered = false;



    TSharedRef<SDockTab> SpawnCollisionAnalyzerTab(const FSpawnTabArgs&)

    {

        TSharedPtr<SCollisionPairAnalyzerWidget> Widget;

        TSharedRef<SDockTab> Tab = SNew(SDockTab)

            .TabRole(ETabRole::NomadTab)

            .Label(TMLoc::Text(TEXT("Collision Pair Analyzer"), TEXT("Collision Pair Analyzer")))

            .OnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>)

            {

                ExistingCollisionAnalyzerTab.Reset();

                ExistingCollisionAnalyzerWidget.Reset();

            }))

            [

                SAssignNew(Widget, SCollisionPairAnalyzerWidget)

            ];

        ExistingCollisionAnalyzerTab = Tab;

        ExistingCollisionAnalyzerWidget = Widget;

        return Tab;

    }



    void RegisterCollisionAnalyzerTab()

    {

        if (bCollisionAnalyzerTabRegistered) return;

        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(CollisionAnalyzerTabId, FOnSpawnTab::CreateStatic(&SpawnCollisionAnalyzerTab))

            .SetDisplayName(TMLoc::Text(TEXT("Collision Pair Analyzer"), TEXT("Collision Pair Analyzer")))

            .SetTooltipText(TMLoc::Text(TEXT("Explain why two selected actors do not block each other."), TEXT("Explain why two selected actors do not block each other.")))

            .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.CollisionPairAnalyzer")));

        bCollisionAnalyzerTabRegistered = true;

    }

}



namespace TMCollisionPairAnalyzer

{

    void RegisterMenus()

    {

        RegisterCollisionAnalyzerTab();



        if (UToolMenu* ActorMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.ActorContextMenu")))

        {

            FToolMenuSection& Section = ActorMenu->FindOrAddSection(TEXT("TraceMotive"));

            Section.AddMenuEntry(

                TEXT("TMAnalyzeCollisionPair"),

                TMLoc::Text(TEXT("Analyze Collision Between Selected"), TEXT("Analyze Collision Between Selected")),

                TMLoc::Text(TEXT("Analyze why the two selected actors do not Block each other in Editor or PIE."), TEXT("Analyze why the two selected actors do not Block each other in Editor or PIE.")),

                FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.CollisionPairAnalyzer")),

                FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& Context)

                {

                    TArray<AActor*> ContextActors;

                    GatherActorsFromMenuContext(Context, ContextActors);

                    RegisterCollisionAnalyzerTab();

                    FGlobalTabmanager::Get()->TryInvokeTab(CollisionAnalyzerTabId);

                    if (TSharedPtr<SCollisionPairAnalyzerWidget> Widget = ExistingCollisionAnalyzerWidget.Pin())

                    {

                        Widget->UseActorsAndAnalyze(ContextActors);

                    }

                }));

        }



        if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")))

        {

            FToolMenuSection& Section = WindowMenu->FindOrAddSection(TEXT("TraceMotive"));

            Section.AddMenuEntry(

                TEXT("TMOpenCollisionPairAnalyzer"),

                TMLoc::Text(TEXT("Collision Pair Analyzer"), TEXT("Collision Pair Analyzer")),

                TMLoc::Text(TEXT("Open the two-actor runtime collision diagnosis window."), TEXT("Open the two-actor runtime collision diagnosis window.")),

                FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.CollisionPairAnalyzer")),

                FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&)

                {

                    RegisterCollisionAnalyzerTab();

                    FGlobalTabmanager::Get()->TryInvokeTab(CollisionAnalyzerTabId);

                }));

        }



        if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")))

        {

            FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("TraceMotive"));

            Section.AddMenuEntry(

                TEXT("TMToolsOpenCollisionPairAnalyzer"),

                TMLoc::Text(TEXT("Collision Pair Analyzer"), TEXT("Collision Pair Analyzer")),

                TMLoc::Text(TEXT("Open the two-actor runtime collision diagnosis window."), TEXT("Open the two-actor runtime collision diagnosis window.")),

                FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.CollisionPairAnalyzer")),

                FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&)

                {

                    RegisterCollisionAnalyzerTab();

                    FGlobalTabmanager::Get()->TryInvokeTab(CollisionAnalyzerTabId);

                }));

        }

    }



    void UnregisterMenus()

    {

        if (bCollisionAnalyzerTabRegistered)

        {

            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(CollisionAnalyzerTabId);

            bCollisionAnalyzerTabRegistered = false;

        }

        ExistingCollisionAnalyzerTab.Reset();

        ExistingCollisionAnalyzerWidget.Reset();

    }



    void OpenWindowAndAnalyzeSelection()

    {

        RegisterCollisionAnalyzerTab();

        FGlobalTabmanager::Get()->TryInvokeTab(CollisionAnalyzerTabId);

        if (TSharedPtr<SCollisionPairAnalyzerWidget> Widget = ExistingCollisionAnalyzerWidget.Pin())

        {

            Widget->UseCurrentSelectionAndAnalyze();

        }

    }

}



#undef LOCTEXT_NAMESPACE







