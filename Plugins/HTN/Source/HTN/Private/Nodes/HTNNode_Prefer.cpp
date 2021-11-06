// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Nodes/HTNNode_Prefer.h"
#include "AITask_MakeHTNPlan.h"

void UHTNNode_Prefer::MakePlanExpansions(FHTNPlanningContext& Context)
{
	const int32 PriorityMarker = Context.PlanningTask->MakePriorityMarker();
	const auto MakeNewPlan = [&](bool bTopBranch)
	{
		FHTNPlanStep* AddedStep = nullptr;
		FHTNPlanStepID AddedStepID;
		const TSharedRef<FHTNPlan> NewPlan = Context.MakePlanCopyWithAddedStep(AddedStep, AddedStepID);
		
		if (bTopBranch)
		{
			AddedStep->SubLevelIndex = AddInlinePrimaryLevel(Context, *NewPlan, AddedStepID);
		}
		else
		{
			AddedStep->SecondarySubLevelIndex = AddInlineSecondaryLevel(Context, *NewPlan, AddedStepID);
		}

		NewPlan->PriorityMarkers.Add(bTopBranch ? PriorityMarker : -PriorityMarker);
		Context.SubmitCandidatePlan(NewPlan, bTopBranch ? TEXT("top branch") : TEXT("bottom branch"));
	};

	MakeNewPlan(true);
	MakeNewPlan(false);
}

void UHTNNode_Prefer::GetNextPrimitiveSteps(FHTNGetNextStepsContext& Context, const FHTNPlanStepID& ThisStepID)
{
	Context.AddFirstPrimitiveStepsInAnySublevelOf(ThisStepID);
}