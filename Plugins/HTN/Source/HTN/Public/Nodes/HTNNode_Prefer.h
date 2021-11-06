// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HTNNode_TwoBranches.h"
#include "HTNNode_Prefer.generated.h"

// Plans one of the branches, but such that the bottom branch is taken only if the top branch can't produce a plan.
UCLASS()
class HTN_API UHTNNode_Prefer : public UHTNNode_TwoBranches
{
	GENERATED_BODY()
	
public:
	virtual void MakePlanExpansions(FHTNPlanningContext& Context) override;
	virtual void GetNextPrimitiveSteps(FHTNGetNextStepsContext& Context, const FHTNPlanStepID& ThisStepID) override;
};