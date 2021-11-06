// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "HTNStandaloneNode.h"

#include "HTNDecorator.h"
#include "HTNService.h"
#include "HTNPlan.h"

UHTNStandaloneNode::UHTNStandaloneNode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
	MaxRecursionLimit(0)
{}

void UHTNStandaloneNode::InitializeFromAsset(UHTN& Asset)
{
	Super::InitializeFromAsset(Asset);

	for (UHTNDecorator* const Decorator : Decorators)
	{
		if (Decorator)
		{
			Decorator->InitializeFromAsset(Asset);
		}
	}

	for (UHTNService* const Service : Services)
	{
		if (Service)
		{
			Service->InitializeFromAsset(Asset);
		}
	}
}

FString UHTNStandaloneNode::GetStaticDescription() const
{
	if (MaxRecursionLimit > 0)
	{
		return FString::Printf(TEXT("(Recursion limit: max %i per plan)\n%s"), MaxRecursionLimit, *Super::GetStaticDescription());
	}

	return Super::GetStaticDescription();
}

bool UHTNStandaloneNode::OnSubLevelFinishedPlanning(FHTNPlan& Plan, const FHTNPlanStepID& ThisStepID, int32 SubLevelIndex, TSharedPtr<FBlackboardWorldState> WorldState)
{
	return true;
}

void UHTNStandaloneNode::GetNextPrimitiveSteps(FHTNGetNextStepsContext& Context, const FHTNPlanStepID& ThisStepID)
{
	Context.SubmitPlanStep(ThisStepID);
}

void UHTNStandaloneNode::GetNextPrimitiveSteps(FHTNGetNextStepsContext& Context, const FHTNPlanStepID& ThisStepID, int32 FinishedSubLevelIndex)
{
	Context.AddNextPrimitiveStepsAfter(ThisStepID);
}

bool UHTNStandaloneNode::CanIncludeSubnodesInSubnodeQuery(const UHTNComponent& OwnerComp, const FHTNPlanStepID& ThisStepID, int32 SubLevelIndex, bool bOnlyStarting, bool bOnlyEnding) const
{
	return true;
}
