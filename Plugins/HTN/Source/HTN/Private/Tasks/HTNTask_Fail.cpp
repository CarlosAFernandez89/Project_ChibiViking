// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Tasks/HTNTask_Fail.h"

UHTNTask_Fail::UHTNTask_Fail(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bShowTaskNameOnCurrentPlanVisualization = false;
}

void UHTNTask_Fail::CreatePlanSteps(UHTNComponent& OwnerComp, UAITask_MakeHTNPlan& PlanningTask, const TSharedRef<const FBlackboardWorldState>& WorldState) const 
{
	// Not submitting any plan steps means failure.
}

FString UHTNTask_Fail::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: Always fails"), *Super::GetStaticDescription());
}
