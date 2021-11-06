// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Nodes/HTNNode_Scope.h"
#include "AITask_MakeHTNPlan.h"

FString UHTNNode_Scope::GetStaticDescription() const
{
	return TEXT("Scope for decorators and services.");
}

void UHTNNode_Scope::MakePlanExpansions(FHTNPlanningContext& Context)
{
	FHTNPlanStep* AddedStep = nullptr;
	FHTNPlanStepID AddedStepID;
	const TSharedRef<FHTNPlan> NewPlan = Context.MakePlanCopyWithAddedStep(AddedStep, AddedStepID);
	AddedStep->SubLevelIndex = NextNodes.Num() ? Context.AddInlineLevel(*NewPlan, AddedStepID) : INDEX_NONE;
	Context.SubmitCandidatePlan(NewPlan);
}

void UHTNNode_Scope::GetNextPrimitiveSteps(FHTNGetNextStepsContext& Context, const FHTNPlanStepID& ThisStepID)
{
	const FHTNPlanStep& Step = Context.Plan.GetStep(ThisStepID);
	if (Step.SubLevelIndex != INDEX_NONE)
	{
		Context.AddFirstPrimitiveStepsInLevel(Step.SubLevelIndex);
	}
}