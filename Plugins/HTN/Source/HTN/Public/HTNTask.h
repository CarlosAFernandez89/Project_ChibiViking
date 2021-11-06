// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HTNStandaloneNode.h"
#include "HTNTypes.h"
#include "HTNComponent.h"
#include "BlackboardWorldstate.h"
#include "HTNPlan.h"
#include "AITask_MakeHTNPlan.h"

#include "HTNTask.generated.h"

UCLASS(Abstract)
class HTN_API UHTNTask : public UHTNStandaloneNode
{
	GENERATED_BODY()

public:
	UHTNTask(const FObjectInitializer& Initializer);

	// Check preconditions and output one (or more, for branching) plan steps with a link to self and a modified worldstate.
	virtual void CreatePlanSteps(UHTNComponent& OwnerComp, UAITask_MakeHTNPlan& PlanningTask, const TSharedRef<const FBlackboardWorldState>& WorldState) const;

	bool WrappedRecheckPlan(UHTNComponent& OwnerComp, uint8* NodeMemory, const FBlackboardWorldState& WorldState, const FHTNPlanStep& SubmittedPlanStep) const;
	EHTNNodeResult WrappedExecuteTask(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlanStepID& PlanStepID) const;
	EHTNNodeResult WrappedAbortTask(UHTNComponent& OwnerComp, uint8* NodeMemory) const;
	void WrappedTickTask(UHTNComponent& OwnerComp, uint8* NodeMemory , float DeltaTime) const;
	void WrappedOnTaskFinished(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult Result) const;
	void WrappedLogToVisualLog(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlanStep& SubmittedPlanStep) const;
	
	// If you return InProgress from ExecuteTask, call this at some point after that (usually in TickTask) to actually finish executing.
	void FinishLatentTask(UHTNComponent& OwnerComp, EHTNNodeResult TaskResult) const;

	// If false, this task won't be shown in location summaries when visualizing the current plan. LogToVisualLog will still be called.
	UPROPERTY(Category = "Planning", EditAnywhere)
	uint8 bShowTaskNameOnCurrentPlanVisualization : 1;
	
protected:
	uint8 bNotifyTick : 1;
	uint8 bNotifyTaskFinished : 1;

	// Called to recheck if a currently executed plan is still actionable.
	virtual bool RecheckPlan(UHTNComponent& OwnerComp, uint8* NodeMemory, const FBlackboardWorldState& WorldState, const FHTNPlanStep& SubmittedPlanStep) { return true; }
	virtual EHTNNodeResult ExecuteTask(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlanStepID& PlanStepID) { return EHTNNodeResult::Succeeded; }
	virtual EHTNNodeResult AbortTask(UHTNComponent& OwnerComp, uint8* NodeMemory) { return EHTNNodeResult::Aborted; }
	virtual void TickTask(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaTime) {}
	virtual void OnTaskFinished(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult Result) {}

	virtual void LogToVisualLog(UHTNComponent& OwnerComp, const uint8* NodeMemory, const FHTNPlanStep& SubmittedPlanStep) const {}
};