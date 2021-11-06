// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "AITask_MakeHTNPlan.h"
#include "Algo/Accumulate.h"
#include "Algo/AnyOf.h"
#include "Algo/MinElement.h"
#include "Algo/Partition.h"
#include "Misc/RuntimeErrors.h"
#include "Misc/ScopeExit.h"
#include "VisualLogger/VisualLogger.h"

#include "HTN.h"
#include "HTNPlan.h"
#include "HTNDecorator.h"
#include "HTNTask.h"
#include "Nodes/HTNNode_If.h"
#include "WorldStateProxy.h"

#if HTN_DEBUG_PLANNING && ENABLE_VISUAL_LOG

#define SAVE_PLANNING_STEP_SUCCESS(AddedNode, NewPlan, StepDescription) if (FVisualLogger::IsRecording()) DebugInfo.AddNode(CurrentPlanToExpand, AddedNode, NewPlan, TEXT(""), StepDescription);
#define SAVE_PLANNING_STEP_FAILURE(AddedNode, FailureMessage) if (FVisualLogger::IsRecording()) DebugInfo.AddNode(CurrentPlanToExpand, AddedNode, nullptr, FailureMessage);
#define SET_NODE_FAILURE_REASON(FailureMessage) if (FVisualLogger::IsRecording()) NodePlanningFailureReason = FailureMessage;

#else

#define SAVE_PLANNING_STEP_SUCCESS(AddedNode, NewPlan, StepDescription)
#define SAVE_PLANNING_STEP_FAILURE(AddedNode, FailureMessage)
#define SET_NODE_FAILURE_REASON(FailureMessage)

#endif

namespace
{
	struct FCompareHTNPlanCosts
	{
		FORCEINLINE bool operator()(const TSharedPtr<FHTNPlan>& A, const TSharedPtr<FHTNPlan>& B) const
		{
			return A.IsValid() && B.IsValid() ?
				A->Cost < B->Cost :
				B.IsValid();
		}
	};

	int32 GetTotalNumSteps(const FHTNPlan& Plan)
	{
		const int32 NumSteps = Algo::Accumulate(Plan.Levels, 0, [](int32 Sum, const TSharedPtr<FHTNPlanLevel>& Level) -> int32 { return Sum + Level->Steps.Num(); });
		return NumSteps;
	}
}

FHTNPlanningContext::FHTNPlanningContext(UAITask_MakeHTNPlan* PlanningTask, UHTNStandaloneNode* AddingNode,
	TSharedPtr<FHTNPlan> PlanToExpand, const FHTNPlanStepID& PlanStepID,
	TSharedPtr<FBlackboardWorldState> WorldStateAfterEnteringDecorators, bool bDecoratorsPassed
) :
	PlanningTask(PlanningTask),
	AddingNode(AddingNode),
	PlanToExpand(PlanToExpand),
	CurrentPlanStepID(PlanStepID),
	WorldStateAfterEnteringDecorators(WorldStateAfterEnteringDecorators),
	bDecoratorsPassed(bDecoratorsPassed)
{}

TSharedRef<FHTNPlan> FHTNPlanningContext::MakePlanCopyWithAddedStep(FHTNPlanStep*& OutAddedStep, FHTNPlanStepID& OutAddedStepID) const
{
	const TSharedRef<FHTNPlan> PlanCopy = PlanToExpand->MakeCopy(CurrentPlanStepID.LevelIndex);

	FHTNPlanLevel& Level = *PlanCopy->Levels[CurrentPlanStepID.LevelIndex];
	OutAddedStep = &Level.Steps.Emplace_GetRef(AddingNode.Get());
	OutAddedStep->WorldStateAfterEnteringDecorators = WorldStateAfterEnteringDecorators;

	OutAddedStepID = { CurrentPlanStepID.LevelIndex, Level.Steps.Num() - 1 };

	return PlanCopy;
}

int32 FHTNPlanningContext::AddLevel(FHTNPlan& NewPlan, UHTN* HTN, const FHTNPlanStepID& ParentStepID) const
{
	return NewPlan.Levels.Add(MakeShared<FHTNPlanLevel>(HTN, WorldStateAfterEnteringDecorators, ParentStepID));
}

int32 FHTNPlanningContext::AddInlineLevel(FHTNPlan& NewPlan, const FHTNPlanStepID& ParentStepID) const
{
	const FHTNPlanStepID StepID = ParentStepID != FHTNPlanStepID::None ? ParentStepID : CurrentPlanStepID;
	UHTN* const HTN = NewPlan.Levels[StepID.LevelIndex]->HTNAsset.Get();
	return NewPlan.Levels.Add(MakeShared<FHTNPlanLevel>(HTN, WorldStateAfterEnteringDecorators, ParentStepID, /*bIsInline=*/true));
}

void FHTNPlanningContext::SubmitCandidatePlan(const TSharedRef<FHTNPlan>& CandidatePlan, const FString& AddedStepDescription) const
{
	if (AddingNode->MaxRecursionLimit > 0)
	{
		CandidatePlan->IncrementRecursionCount(AddingNode.Get());
	}

	const FHTNPlanStepID AddedStepID = { CurrentPlanStepID.LevelIndex, CurrentPlanStepID.StepIndex + 1 };

#if DO_CHECK
	const FHTNPlanLevel& Level = *CandidatePlan->Levels[AddedStepID.LevelIndex];
	check(Level.Steps.IsValidIndex(AddedStepID.StepIndex));
	check(Level.Steps.Num() - 1 == AddedStepID.StepIndex);
#endif
	
	FHTNPlanStep& AddedStep = CandidatePlan->GetStep(AddedStepID);
	check(AddedStep.Node == AddingNode);
	
	// If the node has no sublevels and no worldstate, set the worldstate to what it was after entering decorators.
	if (AddedStep.SubLevelIndex == INDEX_NONE && AddedStep.SecondarySubLevelIndex == INDEX_NONE && !AddedStep.WorldState.IsValid())
	{
		AddedStep.WorldState = AddedStep.WorldStateAfterEnteringDecorators;
	}

	if (!AddedStep.WorldState.IsValid() || PlanningTask->ExitDecoratorsAndPropagateWorldState(*CandidatePlan, AddedStepID))
	{
		PlanningTask->SubmitCandidatePlan(CandidatePlan, AddingNode.Get(), AddedStepDescription);
	}
}

UAITask_MakeHTNPlan::UAITask_MakeHTNPlan(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
	OwnerComponent(nullptr),
	TopLevelHTN(nullptr),
	BlackboardComponent(nullptr),
	NextPriorityMarker(1),
	CurrentPlanStepID(FHTNPlanStepID::None),
	NextNodesIndex(0),
	CurrentTask(nullptr),
	bIsWaitingForTaskToProducePlanSteps(false),
	bWasCancelled(false)
{
	bIsPausable = false;
}

void UAITask_MakeHTNPlan::SetUp(UHTNComponent* InOwnerComponent, UHTN* InTopLevelHTN)
{
	OwnerComponent = InOwnerComponent;
	TopLevelHTN = InTopLevelHTN;
	BlackboardComponent = OwnerComponent->GetBlackboardComponent();
	
	check(IsValid(OwnerComponent));
	check(IsValid(TopLevelHTN));
	check(IsValid(BlackboardComponent));
}

void UAITask_MakeHTNPlan::ExternalCancel()
{
	bWasCancelled = true;
	Super::ExternalCancel();
}

void UAITask_MakeHTNPlan::Clear()
{
	ClearIntermediateState();
	Frontier.Reset();
	BlockedPlans.Reset();
	FinishedPlan = nullptr;
	NextPriorityMarker = 1;

#if HTN_DEBUG_PLANNING
	DebugInfo.Reset();
	NodePlanningFailureReason.Reset();
#endif
}

void UAITask_MakeHTNPlan::SubmitPlanStep(const UHTNTask* Task, TSharedPtr<FBlackboardWorldState> WorldState, int32 Cost, const FString& Description)
{
	if (ensure(CurrentTask && Task == CurrentTask))
	{
		PossibleStepsBuffer.Emplace(FHTNPlanStep(CurrentTask, WorldState, Cost), Description);
	}
}

void UAITask_MakeHTNPlan::WaitForLatentCreatePlanSteps(const UHTNTask* Task)
{
	if (ensure(CurrentTask && Task == CurrentTask))
	{
		bIsWaitingForTaskToProducePlanSteps = true;
	}
}

void UAITask_MakeHTNPlan::FinishLatentCreatePlanSteps(const UHTNTask* Task)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_HTN_Planning);
	
	if (!WasCancelled() && ensure(CurrentTask && Task == CurrentTask))
	{
		if (ensureMsgf(bIsWaitingForTaskToProducePlanSteps, TEXT("FinishLatentCreatePlanSteps called with task %s even though the planner is not waiting for latent CreatePlanSteps. Did you not call WaitForLatentCreatePlanSteps or called FinishLatentCreatePlanSteps twice?"), *Task->GetNodeName()))
		{
			bIsWaitingForTaskToProducePlanSteps = false;
#if HTN_DEBUG_PLANNING
			const auto DidProduceAnyPlans = [&, NumPlansBefore = GetNumCandidatePlans()]() -> bool { return GetNumCandidatePlans() >= NumPlansBefore; };
#endif
			OnTaskFinishedProducingCandidateSteps(CurrentTask);
#if HTN_DEBUG_PLANNING
			if (!DidProduceAnyPlans())
			{
				SAVE_PLANNING_STEP_FAILURE(CurrentTask, NodePlanningFailureReason.IsEmpty() ? TEXT("Failed to produce any plan steps") : NodePlanningFailureReason);
			}
#endif
			// Move to the next possible node
			NextNodesIndex += 1;
			DoPlanning();
		}
	}
}

void UAITask_MakeHTNPlan::Activate()
{
	SCOPE_CYCLE_COUNTER(STAT_AI_HTN_Planning);
	
	check(IsValid(OwnerComponent));
	check(IsValid(TopLevelHTN));
	check(IsValid(BlackboardComponent));

	Clear();

	const TSharedRef<FBlackboardWorldState> WorldStateAtPlanStart = MakeShared<FBlackboardWorldState>(*BlackboardComponent);
	Frontier.HeapPush(MakeShared<FHTNPlan>(TopLevelHTN, WorldStateAtPlanStart), FCompareHTNPlanCosts());
	
	DoPlanning();
}

void UAITask_MakeHTNPlan::OnDestroy(bool bInOwnerFinished)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_HTN_Planning);
	
	ClearIntermediateState();
	Frontier.Reset();
	BlockedPlans.Reset();

#if HTN_DEBUG_PLANNING
	if (FoundPlan())
	{
		DebugInfo.MarkAsFinishedPlan(FinishedPlan);
	}
	
	UE_VLOG_UELOG(OwnerComponent->GetOwner(), LogHTN, Log, TEXT("Planning task %s %s. Recorded planspace traversal:\n(Note that results may be misleading if the visual logger wasn't recording for the entire duration of planning)\n%s"),
		*GetName(),
		WasCancelled() ? TEXT("was cancelled") : FoundPlan() ? TEXT("succeeded") : TEXT("failed"),
		*DebugInfo.ToString()
	);
	DebugInfo.Reset();
#endif
	
	Super::OnDestroy(bInOwnerFinished);
}

void UAITask_MakeHTNPlan::DoPlanning()
{
	SCOPE_CYCLE_COUNTER(STAT_AI_HTN_Planning);
	
	check(!FinishedPlan.IsValid());

	while (!bIsWaitingForTaskToProducePlanSteps)
	{
		if (!CurrentPlanToExpand.IsValid())
		{
			CurrentPlanToExpand = DequeueCurrentBestPlan();
			if (!CurrentPlanToExpand.IsValid())
			{
				// Planning failed
				ensureAsRuntimeWarning(BlockedPlans.Num() == 0);
				EndTask();
				return;
			}
			
			if (CurrentPlanToExpand->IsComplete())
			{
				// Planning succeeded
				FinishedPlan = CurrentPlanToExpand;
				EndTask();
				return;
			}
		}

		MakeExpansionsOfCurrentPlan();
	}
}

TSharedPtr<FHTNPlan> UAITask_MakeHTNPlan::DequeueCurrentBestPlan()
{
	AddUnblockedPlansToFrontier();
	if (Frontier.Num())
	{
		TSharedPtr<FHTNPlan> Plan;
		Frontier.HeapPop(Plan, FCompareHTNPlanCosts());
		check(Plan.IsValid());

		RemoveBlockingPriorityMarkersOf(*Plan);

		// TODO make the MaxPlanLength a config var on the htn component.
		if (GetTotalNumSteps(*Plan) > 100)
		{
			UE_VLOG(OwnerComponent->GetOwner(), LogHTN, Error, TEXT("Max plan length exceeded, planning failed"));
			return nullptr;
		}
		return Plan;
	}

	return nullptr;
}

void UAITask_MakeHTNPlan::MakeExpansionsOfCurrentPlan()
{
	check(CurrentPlanToExpand.IsValid());

	if (CurrentPlanStepID == FHTNPlanStepID::None)
	{
		const bool bSuccess = CurrentPlanToExpand->FindStepToAddAfter(CurrentPlanStepID);
		check(bSuccess);
		check(NextNodesIndex == 0);
	}
	check(CurrentPlanToExpand->HasLevel(CurrentPlanStepID.LevelIndex));

	TSharedPtr<FBlackboardWorldState> WorldState;
	TArrayView<UHTNStandaloneNode*> NextNodes;
	CurrentPlanToExpand->GetWorldStateAndNextNodes(CurrentPlanStepID, WorldState, NextNodes);
	check(WorldState.IsValid());
	
	check(NextNodes.IsValidIndex(NextNodesIndex) || NextNodesIndex == NextNodes.Num());
	for (; NextNodesIndex < NextNodes.Num(); ++NextNodesIndex)
	{
		UHTNStandaloneNode* const Node = NextNodes[NextNodesIndex];
		if (Node->MaxRecursionLimit > 0 && CurrentPlanToExpand->GetRecursionCount(Node) >= Node->MaxRecursionLimit)
		{
			continue;
		}
		
		MakeExpansionsOfCurrentPlan(WorldState, Node);
		if (bIsWaitingForTaskToProducePlanSteps || FinishedPlan.IsValid())
		{
			break;
		}
	}

	if (!bIsWaitingForTaskToProducePlanSteps)
	{
		ClearIntermediateState();
	}
	else
	{
		UE_VLOG_UELOG(OwnerComponent->GetOwner(), LogHTN, VeryVerbose, TEXT("Planning task %s is waiting for task \"%s\" to produce plan steps.\nRecorded planspace traversal so far:\n(Note that results may be misleading if the visual logger wasn't recording for the entire duration of planning)\n%s"),
			*GetName(),
			CurrentTask ? *CurrentTask->GetNodeName() : TEXT("[missing task]"),
			*DebugInfo.ToString()
		);
	}
}

void UAITask_MakeHTNPlan::MakeExpansionsOfCurrentPlan(const TSharedPtr<FBlackboardWorldState>& WorldState, UHTNStandaloneNode* Node)
{
	check(CurrentPlanToExpand.IsValid());
	check(Node);
	check(OwnerComponent);
	// Initialize the node with asset if hasn't been initialized with an asset already.
	// This is to make sure that blackboard keys are resolved etc before planning reaches the node.
	Node->InitializeFromAsset(*TopLevelHTN);

	SET_NODE_FAILURE_REASON(TEXT(""));
	
	const bool bDecoratorsPassed = EnterDecorators(*CurrentPlanToExpand, CurrentPlanStepID, *WorldState, Node, WorldStateAfterEnteredDecorators);
	if (!WorldStateAfterEnteredDecorators.IsValid() || (!bDecoratorsPassed && !Node->IsA(UHTNNode_If::StaticClass())))
	{
		SAVE_PLANNING_STEP_FAILURE(Node, NodePlanningFailureReason);
		return;
	}

#if HTN_DEBUG_PLANNING
	const auto DidProduceAnyPlans = [&, NumPlansBefore = GetNumCandidatePlans()]() { return GetNumCandidatePlans() > NumPlansBefore; };
#endif

	CurrentTask = Cast<UHTNTask>(Node);
	// Adding primitive task. Make as many new plans as there are possible ways to perform the task.
	if (CurrentTask)
	{
		PossibleStepsBuffer.Reset();
		check(!bIsWaitingForTaskToProducePlanSteps);
		CurrentTask->CreatePlanSteps(*OwnerComponent, *this, WorldStateAfterEnteredDecorators.ToSharedRef());
		if (!bIsWaitingForTaskToProducePlanSteps)
		{
			OnTaskFinishedProducingCandidateSteps(CurrentTask);
		}
	}
	else
	{
		FHTNPlanningContext PlanningContext(this, Node,
			CurrentPlanToExpand, CurrentPlanStepID,
			WorldStateAfterEnteredDecorators, bDecoratorsPassed
		);
		
		Node->MakePlanExpansions(PlanningContext);
	}

#if HTN_DEBUG_PLANNING
	if (!bIsWaitingForTaskToProducePlanSteps && !DidProduceAnyPlans())
	{
		SAVE_PLANNING_STEP_FAILURE(Node, NodePlanningFailureReason.IsEmpty() ? TEXT("Failed to produce any plan steps") : NodePlanningFailureReason);
	}
#endif
}

void UAITask_MakeHTNPlan::OnTaskFinishedProducingCandidateSteps(UHTNTask* Task)
{
	if (!ensure(Task == CurrentTask))
	{
		return;
	}
	
	for (TPair<FHTNPlanStep, FString>& Pair : PossibleStepsBuffer)
	{
		FHTNPlanStep& Step = Pair.Key;
		const FString& StepDescription = Pair.Value;

		check(Step.Node == CurrentTask);
		check(Step.WorldState.IsValid());
		if (Step.Cost < 0)
		{
			UE_VLOG_UELOG(OwnerComponent->GetOwner(), LogHTN, Warning, TEXT("Plan step given by %s is %i. Negative costs aren't allowed, resetting to zero."), 
				*Step.Node->GetNodeName(), Step.Cost
			);
			Step.Cost = 0;
		}
		
		Step.WorldStateAfterEnteringDecorators = WorldStateAfterEnteredDecorators;
		ModifyStepCost(Step, Task->Decorators);

		// Make a new plan with this step added in the appropriate level.
		const TSharedRef<FHTNPlan> NewPlan = CurrentPlanToExpand->MakeCopy(CurrentPlanStepID.LevelIndex);
		FHTNPlanLevel& LevelInNewPlan = *NewPlan->Levels[CurrentPlanStepID.LevelIndex];
		LevelInNewPlan.Steps.Add(Step);
		LevelInNewPlan.Cost += Step.Cost;
		NewPlan->Cost += Step.Cost;
		if (Task->MaxRecursionLimit > 0)
		{
			NewPlan->IncrementRecursionCount(Task);
		}

		if (ExitDecoratorsAndPropagateWorldState(*NewPlan, {CurrentPlanStepID.LevelIndex, CurrentPlanStepID.StepIndex + 1}))
		{
			SubmitCandidatePlan(NewPlan, Task, StepDescription);
		}
	}

	PossibleStepsBuffer.Reset();
	CurrentTask = nullptr;
}

bool UAITask_MakeHTNPlan::EnterDecorators(const FHTNPlan& Plan, const FHTNPlanStepID& StepID, const FBlackboardWorldState& WorldState, UHTNStandaloneNode* Node, TSharedPtr<FBlackboardWorldState>& OutNewWorldState) const
{	
	OutNewWorldState = WorldState.MakeNext();
	check(OwnerComponent);
	OwnerComponent->SetPlanningWorldState(OutNewWorldState);

	SET_NODE_FAILURE_REASON(TEXT(""));

	// If starting a plan level, enter root decorators of this level.
	if (StepID.StepIndex == INDEX_NONE)
	{
		const FHTNPlanLevel& Level = *Plan.Levels[StepID.LevelIndex];
		if (!EnterDecorators(Level.GetRootDecoratorTemplates(), Plan, StepID))
		{
			OutNewWorldState.Reset();
			return false;
		}
	}

	// Enter decorators of the node
	if (!EnterDecorators(Node->Decorators, Plan, StepID))
	{
		return false;
	}

	return true;
}

bool UAITask_MakeHTNPlan::EnterDecorators(const TArrayView<UHTNDecorator*>& Decorators, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const
{
	for (UHTNDecorator* const Decorator : Decorators)
	{
		if (ensure(Decorator))
		{
			const bool bPassedDecorator = Decorator->WrappedEnterPlan(*OwnerComponent, Plan, StepID);
			if (!bPassedDecorator)
			{
				SET_NODE_FAILURE_REASON(FString::Printf(TEXT("Failed decorator %s"), *Decorator->GetNodeName()));
				return false;
			}
		}
	}

	return true;
}

// Exits the decorators ending on the specified step.
// If the added task is the last one in a sublevel, assigns its worldstate to the compound step containing that sublevel. Recursively.
bool UAITask_MakeHTNPlan::ExitDecoratorsAndPropagateWorldState(FHTNPlan& Plan, const FHTNPlanStepID& StepID) const
{	
	const FHTNPlanLevel& Level = *Plan.Levels[StepID.LevelIndex];
	const FHTNPlanStep& Step = Level.Steps[StepID.StepIndex];

	const TSharedPtr<FBlackboardWorldState> WorldState = Step.WorldState;
	check(WorldState.IsValid());
	FGuardWorldStateProxy GuardProxy(*OwnerComponent->GetPlanningWorldStateProxy(), WorldState);
	
	SET_NODE_FAILURE_REASON(TEXT(""));

	if (!ExitDecorators(Step.Node->Decorators, Plan, StepID))
	{
		return false;
	}

	if (Plan.IsLevelComplete(StepID.LevelIndex))
	{
		// Exit root decorators of level
		if (!ExitDecorators(Level.GetRootDecoratorTemplates(), Plan, { StepID.LevelIndex, -1 }))
		{
			return false;
		}
		
		// Exit decorators of the compound task containing the level of the added task
		if (Level.ParentStepID != FHTNPlanStepID::None)
		{
			// Duplicate the parent level in this plan, since we'll be making changes to it.
			TSharedPtr<FHTNPlanLevel>& ParentLevel = Plan.Levels[Level.ParentStepID.LevelIndex];
			ParentLevel = MakeShared<FHTNPlanLevel>(*ParentLevel);
			
			FHTNPlanStep& ParentStep = ParentLevel->Steps[Level.ParentStepID.StepIndex];
			check(!ParentStep.WorldState.IsValid());

			const bool bIsParentStepFinished = ParentStep.Node->OnSubLevelFinishedPlanning(Plan, Level.ParentStepID, StepID.LevelIndex, WorldState);
			ParentStep.Cost += Level.Cost;
			ParentLevel->Cost += Level.Cost;
			if (bIsParentStepFinished)
			{
				ParentStep.WorldState = WorldState;
				
				// Allow decorators on the parent node to modify cost of the sublevels.
				const int32 OldCost = ParentStep.Cost;
				ModifyStepCost(ParentStep, ParentStep.Node->Decorators);
				const int32 CostChange = ParentStep.Cost - OldCost;
				if (CostChange < 0)
				{
					UE_VLOG_UELOG(OwnerComponent->GetOwner(), LogHTN, Error, TEXT("When modifying the cost of node %s with a decorator, cost was decreased. This is only allowed for primitive tasks. Otherwise the planner cannot guarantee finding the lowest-cost plan."), *ParentStep.Node->GetNodeName());
					ParentStep.Cost = OldCost;
				}
				else
				{
					ParentLevel->Cost += CostChange;
					Plan.Cost += CostChange;
				}
					
				return ExitDecoratorsAndPropagateWorldState(Plan, Level.ParentStepID);
			}
		}
	}

	return true;
}

bool UAITask_MakeHTNPlan::ExitDecorators(const TArrayView<UHTNDecorator*>& Decorators, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const
{
	for (int32 I = Decorators.Num() - 1; I >= 0; --I)
	{
		UHTNDecorator* const Decorator = Decorators[I];
		if (ensure(Decorator))
		{
			const bool bPassedDecorator = Decorator->WrappedExitPlan(*OwnerComponent, Plan, StepID);
			if (!bPassedDecorator)
			{
				SET_NODE_FAILURE_REASON(FString::Printf(TEXT("Failed to exit decorator %s"), *Decorator->GetNodeName()));
				return false;
			}
		}
	}

	return true;
}

void UAITask_MakeHTNPlan::ModifyStepCost(FHTNPlanStep& Step, const TArray<UHTNDecorator*>& Decorators) const
{
	FGuardWorldStateProxy GuardProxy(*OwnerComponent->GetPlanningWorldStateProxy(), Step.WorldState);
	for (int32 I = Decorators.Num() - 1; I >= 0; --I)
	{
		UHTNDecorator* const Decorator = Decorators[I];
		if (ensure(Decorator))
		{
			Decorator->WrappedModifyStepCost(*OwnerComponent, Step);
		}
	}
}

void UAITask_MakeHTNPlan::SubmitCandidatePlan(const TSharedRef<FHTNPlan>& NewPlan, UHTNStandaloneNode* AddedNode, const FString& AddedStepDescription)
{
	if (WasCancelled())
	{
		return;
	}

	AddBlockingPriorityMarkersOf(*NewPlan);
	if (!IsBlockedByPriorityMarkers(*NewPlan))
	{
		Frontier.HeapPush(NewPlan, FCompareHTNPlanCosts());
	}
	else
	{
		BlockedPlans.Add(NewPlan);
	}

	SAVE_PLANNING_STEP_SUCCESS(AddedNode, NewPlan, AddedStepDescription);
}

void UAITask_MakeHTNPlan::ClearIntermediateState()
{
	CurrentPlanToExpand = nullptr;
	CurrentPlanStepID = FHTNPlanStepID::None;
	NextNodesIndex = 0;
	WorldStateAfterEnteredDecorators = nullptr;
	CurrentTask = nullptr;
}

void UAITask_MakeHTNPlan::AddBlockingPriorityMarkersOf(const FHTNPlan& Plan)
{
	for (const FHTNPriorityMarker PriorityMarker : Plan.PriorityMarkers)
	{
		if (PriorityMarker > 0)
		{
			PriorityMarkerCounts.FindOrAdd(PriorityMarker) += 1;
		}
	}
}

void UAITask_MakeHTNPlan::RemoveBlockingPriorityMarkersOf(const FHTNPlan& Plan)
{
	for (const FHTNPriorityMarker PriorityMarker : Plan.PriorityMarkers)
	{
		if (PriorityMarker > 0)
		{
			PriorityMarkerCounts[PriorityMarker] -= 1;
		}
	}
}

bool UAITask_MakeHTNPlan::IsBlockedByPriorityMarkers(const FHTNPlan& Plan) const
{
	const auto IsBlocked = [&](int32 Marker) -> bool { return Marker < 0 && PriorityMarkerCounts.FindRef(-Marker) > 0; };
	return Algo::AnyOf(Plan.PriorityMarkers, IsBlocked);
}

void UAITask_MakeHTNPlan::AddUnblockedPlansToFrontier()
{
	bool bRemovedAny = false;
	for (auto It = PriorityMarkerCounts.CreateIterator(); It; ++It)
	{
		check(It->Value >= 0);
		if (It->Value == 0)
		{
			bRemovedAny = true;
			It.RemoveCurrent();
		}
	}
	
	if (bRemovedAny)
	{
		const int32 Index = Algo::Partition(BlockedPlans.GetData(), BlockedPlans.Num(), [this](const TSharedPtr<FHTNPlan>& Plan) 
		{ 
			return IsBlockedByPriorityMarkers(*Plan); 
		});
		for (int32 I = Index; I < BlockedPlans.Num(); ++I)
		{
			Frontier.HeapPush(BlockedPlans[I], FCompareHTNPlanCosts());
		}
		BlockedPlans.SetNum(Index);
	}
}

#undef SAVE_PLANNING_STEP_SUCCESS
#undef SAVE_PLANNING_STEP_FAILURE
#undef SET_NODE_FAILURE_REASON
