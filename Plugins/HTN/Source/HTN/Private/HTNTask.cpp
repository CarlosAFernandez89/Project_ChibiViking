// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "HTNTask.h"
#include "AIController.h"
#include "GameplayTasksComponent.h"
#include "WorldStateProxy.h"
#include "HTNPlan.h"

UHTNTask::UHTNTask(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
	bShowTaskNameOnCurrentPlanVisualization(true),
	bNotifyTick(false),
	bNotifyTaskFinished(false)
{}

void UHTNTask::CreatePlanSteps(UHTNComponent& OwnerComp, UAITask_MakeHTNPlan& PlanningTask, const TSharedRef<const FBlackboardWorldState>& WorldState) const
{ 
	// Dummy plan step
	PlanningTask.SubmitPlanStep(this, WorldState->MakeNext(), 100);
}

bool UHTNTask::WrappedRecheckPlan(UHTNComponent& OwnerComp, uint8* NodeMemory, 
	const FBlackboardWorldState& WorldState, const FHTNPlanStep& SubmittedPlanStep) const
{
	check(!IsInstance());
	check(SubmittedPlanStep.Node == this);

	UHTNTask* const Task = StaticCast<UHTNTask*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (!ensure(Task))
	{
		return false;
	}

	return Task->RecheckPlan(OwnerComp, NodeMemory, WorldState, SubmittedPlanStep);
}

EHTNNodeResult UHTNTask::WrappedExecuteTask(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlanStepID& PlanStepID) const
{
	check(!IsInstance());
	UHTNTask* const Task = StaticCast<UHTNTask*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (!ensure(Task))
	{
		return EHTNNodeResult::Failed;
	}

	const EHTNNodeResult Result = Task->ExecuteTask(OwnerComp, NodeMemory, PlanStepID);
	return Result;
}

EHTNNodeResult UHTNTask::WrappedAbortTask(UHTNComponent& OwnerComp, uint8* NodeMemory) const
{
	check(!IsInstance());
	UHTNTask* const Task = StaticCast<UHTNTask*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (!ensure(Task))
	{
		return EHTNNodeResult::Aborted;
	}

	const EHTNNodeResult Result = Task->AbortTask(OwnerComp, NodeMemory);
	return Result;
}

void UHTNTask::WrappedTickTask(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaTime) const
{
	check(!IsInstance());
	UHTNTask* const Task = StaticCast<UHTNTask*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (ensure(Task) && Task->bNotifyTick)
	{
		Task->TickTask(OwnerComp, NodeMemory, DeltaTime);
	}
}

void UHTNTask::WrappedOnTaskFinished(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult Result) const
{
	check(!IsInstance());
	UHTNTask* const Task = StaticCast<UHTNTask*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (!ensure(Task))
	{
		return;
	}

	if (Task->bNotifyTaskFinished)
	{
		Task->OnTaskFinished(OwnerComp, NodeMemory, Result);
	}
		
	// End gameplay tasks owned by the task.
	if (Task->bOwnsGameplayTasks)
	{
		if (const AAIController* const AIController = OwnerComp.GetAIOwner())
		{
			if (UGameplayTasksComponent* const GTComp = AIController->GetGameplayTasksComponent())
			{
				GTComp->EndAllResourceConsumingTasksOwnedBy(*Task);
			}
		}
	}
}

void UHTNTask::WrappedLogToVisualLog(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlanStep& SubmittedPlanStep) const
{
	check(!IsInstance());
	UHTNTask* const Task = StaticCast<UHTNTask*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (ensure(Task))
	{
		Task->LogToVisualLog(OwnerComp, NodeMemory, SubmittedPlanStep);
	}
}

void UHTNTask::FinishLatentTask(UHTNComponent& OwnerComp, EHTNNodeResult TaskResult) const
{
	OwnerComp.OnTaskFinished(this, TaskResult);
}
