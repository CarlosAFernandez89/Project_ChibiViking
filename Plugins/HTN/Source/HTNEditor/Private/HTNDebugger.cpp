// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "HTNDebugger.h"

#include "Algo/Transform.h"
#include "Algo/RemoveIf.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Controller.h"
#include "WorldStateProxy.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

#include "HTNDelegates.h"
#include "HTNEditor.h"
#include "HTNGraph.h"
#include "HTNTypes.h"
#include "HTNPlan.h"

#define LOCTEXT_NAMESPACE "HTNDebugger"

namespace
{
	UHTNComponent* FindHTNComponentInActor(AActor* Actor)
	{
		if (Actor)
		{
			if (APawn* const Pawn = Cast<APawn>(Actor))
			{
				if (AController* const Controller = Pawn->GetController())
				{
					if (UHTNComponent* const Component = Controller->FindComponentByClass<UHTNComponent>())
					{
						return Component;
					}
				}
			}

			return Actor->FindComponentByClass<UHTNComponent>();
		}

		return nullptr;
	}

	UWorld* GetPIEWorld()
	{
		UWorld* PIEWorld = nullptr;

		for (const FWorldContext& PieContext : GEditor->GetWorldContexts())
		{
			if (PieContext.WorldType == EWorldType::PIE && PieContext.World())
			{
				if (PieContext.RunAsDedicated)
				{
					PIEWorld = PieContext.World();
					break;
				}

				if (!PIEWorld)
				{
					PIEWorld = PieContext.World();
				}
			}
		}

		return PIEWorld;
	}

	template<typename FuncType>
	void ForEachGameWorld(FuncType&& Func)
	{
		if (GUnrealEd)
		{
			for (const FWorldContext& Context : GUnrealEd->GetWorldContexts())
			{
				UWorld* const PlayWorld = Context.World();
				if (PlayWorld && PlayWorld->IsGameWorld())
				{
					Invoke(Forward<FuncType>(Func), PlayWorld);
				}
			}
		}
	}

	bool AreAllGameWorldsPaused()
	{
		bool bPaused = true;
		ForEachGameWorld([&](UWorld* World) {
			bPaused = bPaused && World->bDebugPauseExecution;
		});
		return bPaused;
	}
}

FHTNDebugger::FHTNDebugger() :
	ActiveDebugStepIndex(INDEX_NONE),
	bIsPIEActive(false)
{
	FEditorDelegates::BeginPIE.AddRaw(this, &FHTNDebugger::OnBeginPIE);
	FEditorDelegates::EndPIE.AddRaw(this, &FHTNDebugger::OnEndPIE);
	FEditorDelegates::PausePIE.AddRaw(this, &FHTNDebugger::OnPausePIE);
}

FHTNDebugger::~FHTNDebugger()
{
	FEditorDelegates::BeginPIE.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);
	FEditorDelegates::PausePIE.RemoveAll(this);
	
	USelection::SelectObjectEvent.RemoveAll(this);
	FHTNDelegates::OnPlanExecutionStarted.RemoveAll(this);
}

void FHTNDebugger::Tick(float DeltaTime)
{
	if (!HTNAsset.IsValid())
	{
		return;
	}

	const bool bLostCurrentDebuggedComponent = DebuggedHTNComponent.IsStale() || (DebuggedHTNComponent.IsValid() && (DebuggedHTNComponent->IsBeingDestroyed() || !DebuggedHTNComponent->IsActive()));
	if (bLostCurrentDebuggedComponent)
	{
		DebuggedHTNComponent.Reset();
		ClearDebuggerState();
	}

	if (!DebuggedHTNComponent.IsValid())
	{
		FindMatchingRunningHTNComponent();
	}

	if (DebuggedHTNComponent.IsValid() && !IsPlaySessionPaused())
	{
		// Catch up and trigger breakpoints
		const FHTNDebugSteps& DebugSteps = DebuggedHTNComponent->DebuggerSteps;
		while (const FHTNDebugExecutionStep* const NextDebugStep = DebugSteps.GetByIndex(ActiveDebugStepIndex + 1))
		{
			const FHTNDebugExecutionStep* const ActiveDebugStep = DebugSteps.GetByIndex(ActiveDebugStepIndex);
			const bool bPlanChanged = !ActiveDebugStep || ActiveDebugStep->HTNPlan != NextDebugStep->HTNPlan;
			
			for (const FHTNPlanStepID& NewStepID : NextDebugStep->ActivePlanStepIDs)
			{
				const bool bStepJustBeganExecuting = bPlanChanged || !ActiveDebugStep->ActivePlanStepIDs.Contains(NewStepID);
				if (bStepJustBeganExecuting)
				{
					UHTNGraphNode* const GraphNode = GetGraphNode(*NextDebugStep->HTNPlan, NewStepID);
					if (GraphNode && GraphNode->bHasBreakpoint && GraphNode->bIsBreakpointEnabled)
					{
						if (EditorOwner.IsValid())
						{
							EditorOwner.Pin()->FocusWindow(HTNAsset.Get());
						}
						PausePlaySession();
						break;
					}
				}
			}

			ActiveDebugStepIndex += 1;
			if (IsPlaySessionPaused())
			{
				break;
			}
		}
	}

	UpdateDebugFlags();
}

bool FHTNDebugger::IsTickable() const
{
	return IsDebuggerReady();
}

bool FHTNDebugger::IsPlaySessionPaused()
{
	return AreAllGameWorldsPaused();
}

bool FHTNDebugger::IsPlaySessionRunning()
{
	return !AreAllGameWorldsPaused();
}

void FHTNDebugger::PausePlaySession()
{
	bool bPaused = false;
	ForEachGameWorld([&](UWorld* World) 
	{
		if (!World->bDebugPauseExecution)
		{
			World->bDebugPauseExecution = true;
			bPaused = true;
		}
	});
	
	if (bPaused)
	{
		GUnrealEd->PlaySessionPaused();
	}
}

void FHTNDebugger::ResumePlaySession()
{
	bool bResumed = false;
	ForEachGameWorld([&](UWorld* World) 
	{
		if (World->bDebugPauseExecution)
		{
			World->bDebugPauseExecution = false;
			bResumed = true;
		}
	});
	
	if (bResumed)
	{
		// @TODO: we need a unified flow to leave debugging mode from the different debuggers to prevent strong coupling between modules.
		// Each debugger (Blueprint & BehaviorTree for now) could then take the appropriate actions to resume the session.
		if (FSlateApplication::Get().InKismetDebuggingMode())
		{
			FSlateApplication::Get().LeaveDebuggingMode();
		}

		GUnrealEd->PlaySessionResumed();
	}
}

void FHTNDebugger::StopPlaySession()
{
	if (GUnrealEd->PlayWorld)
	{
		GEditor->RequestEndPlayMap();

		// @TODO: we need a unified flow to leave debugging mode from the different debuggers to prevent strong coupling between modules.
		// Each debugger (Blueprint & BehaviorTree for now) could then take the appropriate actions to resume the session.
		if (FSlateApplication::Get().InKismetDebuggingMode())
		{
			FSlateApplication::Get().LeaveDebuggingMode();
		}
	}
}

bool FHTNDebugger::IsDebuggerReady() const
{
	return bIsPIEActive;
}

bool FHTNDebugger::IsDebuggerRunning() const
{
	return DebuggedHTNComponent.IsValid();
}

void FHTNDebugger::Setup(UHTN* InHTNAsset, TSharedRef<FHTNEditor> InEditorOwner)
{
	HTNAsset = InHTNAsset;
	EditorOwner = InEditorOwner;
	CurrentlySelectedGraphNode.Reset();
	SetDebuggedComponent(nullptr);

	AssetRootNode = nullptr;
	CacheRootNode();

#if USE_HTN_DEBUGGER
	if (FHTNEditor::IsPIESimulating())
	{
		OnBeginPIE(GEditor->bIsSimulatingInEditor);
		Refresh();
	}
#endif
}

void FHTNDebugger::Refresh()
{
	CacheRootNode();
	UpdateDebugFlags();
}

void FHTNDebugger::ClearDebuggerState()
{
	ActiveDebugStepIndex = INDEX_NONE;
	UpdateDebugFlags();
}

void FHTNDebugger::OnSelectedNodesChanged(const TSet<UObject*>& NewSelection)
{
	TArray<UHTNGraphNode*> SelectedNodes;
	static const auto ToHTNGraphNode = [](UObject* Obj) { return Cast<UHTNGraphNode>(Obj); };
	Algo::TransformIf(EditorOwner.Pin()->GetSelectedNodes(), SelectedNodes, ToHTNGraphNode, ToHTNGraphNode);

	if (SelectedNodes.Num() == 1)
	{
		CurrentlySelectedGraphNode = SelectedNodes[0];
	}
	else
	{
		CurrentlySelectedGraphNode.Reset();
	}
}

void FHTNDebugger::OnBreakpointAdded(UHTNGraphNode* GraphNode)
{
}

void FHTNDebugger::OnBreakpointRemoved(UHTNGraphNode* GraphNode)
{
}

void FHTNDebugger::OnObjectSelected(UObject* Object)
{
	if (Object && Object->IsSelected())
	{
		if (UHTNComponent* const HTNComponent = FindHTNComponentInActor(Cast<AActor>(Object)))
		{
			SetDebuggedComponent(HTNComponent);
		}
	}
}

void FHTNDebugger::OnInstanceSelectedInDropdown(TWeakObjectPtr<UHTNComponent> SelectedComponent)
{
	if (!SelectedComponent.IsValid() || SelectedComponent == DebuggedHTNComponent)
	{
		return;
	}
		
	USelection* const Selection = GEditor ? GEditor->GetSelectedActors() : nullptr;
	if (Selection)
	{
		Selection->DeselectAll();
	}

	SetDebuggedComponent(SelectedComponent);
	if (Selection)
	{
		AController* const Controller = Cast<AController>(SelectedComponent->GetOwner());
		if (APawn* const Pawn = Controller ? Controller->GetPawn() : nullptr)
		{
			Selection->Select(Pawn);
		}
	}

	Refresh();
}

void FHTNDebugger::OnPlanExecutionStarted(const UHTNComponent& OwnerComp, const TSharedPtr<FHTNPlan>& Plan)
{
	if (!DebuggedHTNComponent.IsValid())
	{
		if (IsCompatible(OwnerComp))
		{
			SetDebuggedComponent(const_cast<UHTNComponent*>(&OwnerComp));
		}
	}
}

void FHTNDebugger::OnBeginPIE(bool bIsSimulating)
{
	bIsPIEActive = true;
	if (EditorOwner.IsValid())
	{
		EditorOwner.Pin()->RegenerateMenusAndToolbars();
	}

	FindMatchingRunningHTNComponent();

	// remove these delegates first as we can get multiple calls to OnBeginPIE()
	USelection::SelectObjectEvent.RemoveAll(this);
	FHTNDelegates::OnPlanExecutionStarted.RemoveAll(this);

	USelection::SelectObjectEvent.AddRaw(this, &FHTNDebugger::OnObjectSelected);
	FHTNDelegates::OnPlanExecutionStarted.AddRaw(this, &FHTNDebugger::OnPlanExecutionStarted);
}

void FHTNDebugger::OnEndPIE(bool bIsSimulating)
{
	bIsPIEActive = false;
	if (EditorOwner.IsValid())
	{
		EditorOwner.Pin()->RegenerateMenusAndToolbars();
	}

	USelection::SelectObjectEvent.RemoveAll(this);
	FHTNDelegates::OnPlanExecutionStarted.RemoveAll(this);

	ClearDebuggerState();
}

void FHTNDebugger::OnPausePIE(bool bIsSimulating)
{
	// TODO If paused while executing subnetwork, open asset for said network.
}

void FHTNDebugger::FindMatchingRunningHTNComponent()
{
	UWorld* const PIEWorld = GetPIEWorld();
	if (!PIEWorld)
	{
		return;
	}

	UHTNComponent* MatchingHTNComponent = nullptr;

#if USE_HTN_DEBUGGER
	for (const TWeakObjectPtr<UHTNComponent>& HTNComponent : UHTNComponent::PlayingComponents)
	{
		if (HTNComponent.IsValid() && HTNComponent->GetWorld() == PIEWorld && IsCompatible(*HTNComponent))
		{
			MatchingHTNComponent = HTNComponent.Get();
			if (HTNComponent->IsSelected())
			{
				SetDebuggedComponent(HTNComponent);
				return;
			}
		}
	}
#endif
	
	if (MatchingHTNComponent != DebuggedHTNComponent)
	{
		SetDebuggedComponent(MatchingHTNComponent);
	}
}

bool FHTNDebugger::IsCompatible(const UHTNComponent& HTNComponent)
{
	if (HTNComponent.CurrentPlan.IsValid())
	{
		const bool bResult = HTNComponent.CurrentPlan->Levels.ContainsByPredicate([&](const TSharedPtr<FHTNPlanLevel>& Level) -> bool 
		{
			return Level.IsValid() && Level->HTNAsset == HTNAsset;
		});

		return bResult;
	}

	return false;
}

TSharedRef<SWidget> FHTNDebugger::GetActorsMenu() const
{
	FMenuBuilder MenuBuilder(true, nullptr);

	bool bAnyMenuEntriesAdded = false;
	FHTNDebugger* const MutableThis = const_cast<FHTNDebugger*>(this);

#if USE_HTN_DEBUGGER
	for (const TWeakObjectPtr<UHTNComponent>& HTNComponent : UHTNComponent::PlayingComponents)
	{
		if (HTNComponent.IsValid())
		{
			const FText ActorDescription = GetActorDescription(*HTNComponent);
			const FUIAction ItemAction(FExecuteAction::CreateSP(MutableThis, &FHTNDebugger::OnInstanceSelectedInDropdown, HTNComponent));
			MenuBuilder.AddMenuEntry(ActorDescription, TAttribute<FText>(), FSlateIcon(), ItemAction);
			bAnyMenuEntriesAdded = true;
		}
	}
#endif

	if (!bAnyMenuEntriesAdded)
	{
		const FText ActorDesc = LOCTEXT("NoMatchForDebug", "Can't find matching actors");
		const TWeakObjectPtr<UHTNComponent> HTNComponent;
		const FUIAction ItemAction(FExecuteAction::CreateSP(MutableThis, &FHTNDebugger::OnInstanceSelectedInDropdown, HTNComponent));
		MenuBuilder.AddMenuEntry(ActorDesc, TAttribute<FText>(), FSlateIcon(), ItemAction);
	}

	return MenuBuilder.MakeWidget();
}

FText FHTNDebugger::GetCurrentActorDescription() const
{
	return DebuggedHTNComponent.IsValid() ? GetActorDescription(*DebuggedHTNComponent) : LOCTEXT("NoDebugActorsSelected", "No debug actor selected.");
}

FText FHTNDebugger::GetActorDescription(const UHTNComponent& HTNComponent) const
{
	if (AActor* const Owner = HTNComponent.GetOwner())
	{
		if (AController* const Controller = Cast<AController>(Owner))
		{
			if (APawn* const Pawn = Controller->GetPawn())
			{
				return FText::FromString(Pawn->GetName());
			}
			else
			{
				return FText::FromString(Controller->GetName());
			}
		}
		else
		{
			return FText::FromString(Owner->GetActorLabel());
		}
	}

	return FText::Format(LOCTEXT("HTNComponentWithNoOwnerActor", "HTNComponent with no owner: {0}"), FText::FromName(HTNComponent.GetFName()));
}

FText FHTNDebugger::HandleGetDebugKeyValue(const FName& InKeyName, bool bUseCurrentState) const
{
	if (!IsDebuggerReady() || !IsDebuggerRunning() || !DebuggedHTNComponent.IsValid())
	{
		return FText();
	}
	
#if USE_HTN_DEBUGGER

	const FHTNDebugExecutionStep* const DebugStep = DebuggedHTNComponent->DebuggerSteps.GetByIndex(ActiveDebugStepIndex);
	const auto FindCurrentlySelectedStep = [&]() -> const FHTNPlanStep*
	{
		if (!CurrentlySelectedGraphNode.IsValid())
		{
			return nullptr;
		}

		const TSharedPtr<FHTNPlan>& CurrentPlan = DebugStep ? DebugStep->HTNPlan : DebuggedHTNComponent->CurrentPlan;
		if (!CurrentPlan.IsValid())
		{
			return nullptr;
		}
		
		for (int32 LevelIndex = 0; LevelIndex < CurrentPlan->Levels.Num(); ++LevelIndex)
		{
			const FHTNPlanLevel& Level = *CurrentPlan->Levels[LevelIndex];
			if (Level.HTNAsset == HTNAsset)
			{
				for (int32 StepIndex = 0; StepIndex < Level.Steps.Num(); ++StepIndex)
				{
					if (const UHTNGraphNode* const GraphNode = GetGraphNode(*CurrentPlan, {LevelIndex, StepIndex}))
					{
						if (GraphNode == CurrentlySelectedGraphNode)
						{
							return &Level.Steps[StepIndex];
						}
					}
				}
			}
		}

		return nullptr;
	};
	
	const auto GetKeyDescriptionFrom = [&](const auto& Source) -> FText
	{
		return FText::FromString(Source.DescribeKeyValue(InKeyName, EBlackboardDescription::OnlyValue));
	};

	if (bUseCurrentState || (!DebugStep && DebuggedHTNComponent->CurrentPlan.IsValid()))
	{
		if (const UBlackboardComponent* const Blackboard = DebuggedHTNComponent->GetBlackboardComponent())
		{
			return GetKeyDescriptionFrom(*Blackboard);
		}
	}
	else
	{
		if (const FHTNPlanStep* const Step = FindCurrentlySelectedStep())
		{
			if (const FBlackboardWorldState* const WorldState = Step->WorldState.Get())
			{
				return GetKeyDescriptionFrom(*WorldState);
			}
		}
	}

#endif

	return FText();
}

bool FHTNDebugger::IsShowingCurrentState() const
{
	return !CurrentlySelectedGraphNode.IsValid();
}

void FHTNDebugger::SetDebuggedComponent(TWeakObjectPtr<UHTNComponent> NewDebuggedComponent)
{
	if (NewDebuggedComponent == DebuggedHTNComponent)
	{
		return;
	}

	ClearDebuggerState();
	DebuggedHTNComponent = NewDebuggedComponent;
	if (DebuggedHTNComponent.IsValid())
	{
		ActiveDebugStepIndex = DebuggedHTNComponent->DebuggerSteps.GetLastIndex();
	}
	else
	{
		ActiveDebugStepIndex = INDEX_NONE;
	}

	//UpdateDebuggerViewOnInstanceChange();
}

void FHTNDebugger::UpdateDebugFlags()
{
	if (!HTNAsset.IsValid() || !HTNAsset->HTNGraph)
	{
		return;
	}
	
	for (UEdGraphNode* const GraphNode : HTNAsset->HTNGraph->Nodes)
	{
		if (UHTNGraphNode* const HTNGraphNode = Cast<UHTNGraphNode>(GraphNode))
		{
			HTNGraphNode->ClearDebugFlags();
		}
	}
	
#if USE_HTN_DEBUGGER

	const FHTNDebugExecutionStep* const DebugStep = DebuggedHTNComponent.IsValid() ?
		DebuggedHTNComponent->DebuggerSteps.GetByIndex(ActiveDebugStepIndex) : nullptr;
	if (!DebugStep || !DebugStep->HTNPlan.IsValid())
	{
		return;
	}
	
	const FHTNPlan& Plan = *DebugStep->HTNPlan;
	
	// Mark nodes that are part of the current plan
	{
		struct FTraversalStep
		{
			FHTNPlanStepID StepID;
			int32 Depth = 0;
			bool bIsInFutureOfPlan = false;
		};

		int32 NextExecutionIndex = 0;
		TArray<FTraversalStep> Stack = {{{0, 0}}};
		while (Stack.Num())
		{
			const FTraversalStep Current = Stack.Pop();
			const FHTNPlanStepID StepID = Current.StepID;
			const int32 Depth = Current.Depth;
			const bool bIsInFutureOfPlan = Current.bIsInFutureOfPlan || 
				DebugStep->ActivePlanStepIDs.Num() == 0 || 
				DebugStep->ActivePlanStepIDs.Contains(StepID);
			const bool bIsStepExecuting = bIsInFutureOfPlan && DebugStep->ActivePlanStepIDs.Contains(StepID);
			const auto PushToStack = [&](int32 LevelIndex, int32 StepIndex = 0)
			{
				Stack.Push({ {LevelIndex, StepIndex}, Depth + 1, bIsInFutureOfPlan});
			};
			
			const FHTNPlanLevel& Level = *Plan.Levels[StepID.LevelIndex];
			const FHTNPlanStep& Step = Level.Steps[StepID.StepIndex];
			if (StepID.StepIndex == 0 && !Level.IsInlineLevel())
			{
				if (Level.HTNAsset == HTNAsset)
				{
					CacheRootNode();
					AssetRootNode->DebuggerPlanEntries.Add({ nullptr, NextExecutionIndex, Depth, bIsInFutureOfPlan, bIsStepExecuting });
				}
				
				NextExecutionIndex += 1;
			}
			
			if (UHTNGraphNode* const GraphNode = GetGraphNode(Plan, StepID))
			{
				UHTNGraphNode* const PrevGraphNode =
					StepID.StepIndex > 0 ? GetGraphNode(Plan, {StepID.LevelIndex, StepID.StepIndex - 1}) :
					Level.IsInlineLevel() ? GetGraphNode(Plan, Level.ParentStepID) :
					AssetRootNode.Get();

				GraphNode->DebuggerPlanEntries.Add({ PrevGraphNode, NextExecutionIndex, Depth, bIsInFutureOfPlan, bIsStepExecuting });
			}
			NextExecutionIndex += 1;

			if (StepID.StepIndex < Level.Steps.Num() - 1)
			{
				PushToStack(StepID.LevelIndex, StepID.StepIndex + 1);
			}
			if (Step.SubLevelIndex != INDEX_NONE)
			{
				PushToStack(Step.SubLevelIndex);
			}
			if (Step.SecondarySubLevelIndex != INDEX_NONE)
			{
				PushToStack(Step.SecondarySubLevelIndex);
			}
		}
	}

	// Mark currently executing nodes
	for (const FHTNPlanStepID& StepID : DebugStep->ActivePlanStepIDs)
	{
		check(Plan.HasStep(StepID));

		FHTNPlanStepID CurrentStepID = StepID;
		while (CurrentStepID != FHTNPlanStepID::None)
		{
			if (UHTNGraphNode* const GraphNode = GetGraphNode(Plan, CurrentStepID))
			{
				GraphNode->bDebuggerMarkCurrentlyActive = true;
				GraphNode->bDebuggerMarkCurrentlyExecuting = CurrentStepID == StepID;
			}

			const FHTNPlanLevel& CurrentLevel = *Plan.Levels[CurrentStepID.LevelIndex];
			CurrentStepID = CurrentLevel.ParentStepID;
		}
	}
#endif
}

void FHTNDebugger::CacheRootNode()
{
	if (!AssetRootNode.IsValid() && HTNAsset.IsValid())
	{
		if (UHTNGraph* const Graph = Cast<UHTNGraph>(HTNAsset->HTNGraph))
		{
			AssetRootNode = Graph->FindRootNode();
		}
	}
}

UHTNGraphNode* FHTNDebugger::GetGraphNode(const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const
{
	if (!HTNAsset.IsValid() || !HTNAsset->HTNGraph)
	{
		return nullptr;
	}

	if (!Plan.HasStep(StepID))
	{
		return nullptr;
	}

	const FHTNPlanLevel& Level = *Plan.Levels[StepID.LevelIndex];
	if (Level.HTNAsset != HTNAsset)
	{
		return nullptr;
	}
	
	const FHTNPlanStep& Step = Level.Steps[StepID.StepIndex];
	if (!Step.Node.IsValid()) // Node instances might become stale in the debugger steps backlog.
	{
		return nullptr;
	}

	const int32 GraphNodeIndex = Step.Node->NodeIndexInGraph;
	const TArray<UEdGraphNode*>& GraphNodes = HTNAsset->HTNGraph->Nodes;
	if (GraphNodes.IsValidIndex(GraphNodeIndex))
	{
		if (UHTNGraphNode* const HTNGraphNode = Cast<UHTNGraphNode>(GraphNodes[GraphNodeIndex]))
		{
			return HTNGraphNode;
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
