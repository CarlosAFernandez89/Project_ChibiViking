// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Nodes/HTNNode_SubNetworkDynamic.h"
#include "AITask_MakeHTNPlan.h"

FString UHTNNode_SubNetworkDynamic::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s:\nDefault: %s\nInjection tag: %s"), *Super::GetStaticDescription(), *GetNameSafe(DefaultHTN), *InjectTag.ToString());
}

void UHTNNode_SubNetworkDynamic::MakePlanExpansions(FHTNPlanningContext& Context)
{	
	FHTNPlanStep* AddedStep = nullptr;
	FHTNPlanStepID AddedStepID;
	const TSharedRef<FHTNPlan> NewPlan = Context.MakePlanCopyWithAddedStep(AddedStep, AddedStepID);

	UHTN* const HTN = GetHTN(*Context.PlanningTask->GetOwnerComponent());
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

void UHTNNode_SubNetworkDynamic::GetNextPrimitiveSteps(FHTNGetNextStepsContext& Context, const FHTNPlanStepID& ThisStepID)
{
	const FHTNPlanStep& Step = Context.Plan.GetStep(ThisStepID);
	Context.AddFirstPrimitiveStepsInLevel(Step.SubLevelIndex);
}

#if WITH_EDITOR
FName UHTNNode_SubNetworkDynamic::GetNodeIconName() const
{
	return FName(TEXT("BTEditor.Graph.BTNode.Task.RunBehavior.Icon"));
}
#endif

UHTN* UHTNNode_SubNetworkDynamic::GetHTN(UHTNComponent& OwnerComp) const
{
	if (UHTN* const InjectedHTN = OwnerComp.GetDynamicHTN(InjectTag))
	{
		return InjectedHTN;
	}

	return DefaultHTN;
}
