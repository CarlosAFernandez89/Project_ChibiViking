// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Nodes/HTNNode_If.h"
#include "AITask_MakeHTNPlan.h"

UHTNNode_If::UHTNNode_If(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
	bCanConditionsInterruptTrueBranch(true),
	bCanConditionsInterruptFalseBranch(true)
{}

void UHTNNode_If::MakePlanExpansions(FHTNPlanningContext& Context)
{
	FHTNPlanStep* AddedStep = nullptr;
	FHTNPlanStepID AddedStepID;
	const TSharedRef<FHTNPlan> NewPlan = Context.MakePlanCopyWithAddedStep(AddedStep, AddedStepID);

	AddedStep->bIsIfNodeFalseBranch = !Context.bDecoratorsPassed;
	AddedStep->bCanConditionsInterruptTrueBranch = bCanConditionsInterruptTrueBranch;
	AddedStep->bCanConditionsInterruptFalseBranch = bCanConditionsInterruptFalseBranch;

	if (Context.bDecoratorsPassed)
	{
		AddedStep->SubLevelIndex = AddInlinePrimaryLevel(Context, *NewPlan, AddedStepID);
	}
	else
	{
		AddedStep->SecondarySubLevelIndex = AddInlineSecondaryLevel(Context, *NewPlan, AddedStepID);
	}
	
	Context.SubmitCandidatePlan(NewPlan, Context.bDecoratorsPassed ? TEXT("true branch") : TEXT("false branch"));
}

void UHTNNode_If::GetNextPrimitiveSteps(FHTNGetNextStepsContext& Context, const FHTNPlanStepID& ThisStepID)
{
	const FHTNPlanStep& Step = Context.Plan.GetStep(ThisStepID);

	if (Step.SubLevelIndex != INDEX_NONE)
	{
		Context.AddFirstPrimitiveStepsInLevel(Step.SubLevelIndex);
	}
	else if (Step.SecondarySubLevelIndex != INDEX_NONE)
	{
		Context.AddFirstPrimitiveStepsInLevel(Step.SecondarySubLevelIndex);
	}
}

FString UHTNNode_If::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s%s%s"), 
		*Super::GetStaticDescription(),
		bCanConditionsInterruptTrueBranch ? TEXT("") : TEXT("\n(decorators won't interrupt true branch)"),
		bCanConditionsInterruptFalseBranch ? TEXT("") : TEXT("\n(decorators won't interrupt false branch)")
	);
}
