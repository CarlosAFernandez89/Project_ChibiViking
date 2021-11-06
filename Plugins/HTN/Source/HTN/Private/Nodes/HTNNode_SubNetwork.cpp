// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Nodes/HTNNode_SubNetwork.h"
#include "AITask_MakeHTNPlan.h"

FString UHTNNode_SubNetwork::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s:\n%s"), *Super::GetStaticDescription(), *GetNameSafe(HTN));
}

void UHTNNode_SubNetwork::MakePlanExpansions(FHTNPlanningContext& Context)
{	
	FHTNPlanStep* AddedStep = nullptr;
	FHTNPlanStepID AddedStepID;
	const TSharedRef<FHTNPlan> NewPlan = Context.MakePlanCopyWithAddedStep(AddedStep, AddedStepID);

	const auto IsSubHTNValid = [&]() -> bool
	{
		if (HTN && HTN->StartNodes.Num())
		{
			if (const UBlackboardComponent* const BlackboardComponent = Context.PlanningTask->GetOwnerComponent()->GetBlackboardComponent())
			{
				return HTN->BlackboardAsset && BlackboardComponent->IsCompatibleWith(HTN->BlackboardAsset);
			}
		}

		return false;
	};
	
	// Valid subnetwork, add a sublevel.
	if (IsSubHTNValid())
	{
		AddedStep->SubLevelIndex = Context.AddLevel(*NewPlan, HTN, AddedStepID);
	}
	
	Context.SubmitCandidatePlan(NewPlan);
}

void UHTNNode_SubNetwork::GetNextPrimitiveSteps(FHTNGetNextStepsContext& Context, const FHTNPlanStepID& ThisStepID)
{
	const FHTNPlanStep& Step = Context.Plan.GetStep(ThisStepID);
	Context.AddFirstPrimitiveStepsInLevel(Step.SubLevelIndex);
}

FString UHTNNode_SubNetwork::GetNodeName() const
{
	if (!HTN || NodeName.Len())
	{
		return Super::GetNodeName();
	}

	return GetSubStringAfterUnderscore(HTN->GetName());
}

#if WITH_EDITOR
FName UHTNNode_SubNetwork::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.RunBehavior.Icon");
}
#endif