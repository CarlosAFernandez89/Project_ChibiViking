// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Tasks/HTNTask_Success.h"

UHTNTask_Success::UHTNTask_Success(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
	Cost(100)
{
	bShowTaskNameOnCurrentPlanVisualization = false;
}

void UHTNTask_Success::CreatePlanSteps(UHTNComponent& OwnerComp, UAITask_MakeHTNPlan& PlanningTask, const TSharedRef<const FBlackboardWorldState>& WorldState) const
{
	PlanningTask.SubmitPlanStep(this, WorldState->MakeNext(), FMath::Max(0, Cost));
}

FString UHTNTask_Success::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: Cost: %i"), *Super::GetStaticDescription(), Cost);
}
