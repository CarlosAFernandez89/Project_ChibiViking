// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HTNStandaloneNode.h"
#include "HTNNode_TwoBranches.generated.h"

// The base class for standalone nodes with two branches (e.g., If, AnyOrder)
UCLASS(Abstract)
class HTN_API UHTNNode_TwoBranches : public UHTNStandaloneNode
{
	GENERATED_BODY()

public:
	UHTNNode_TwoBranches(const FObjectInitializer& Initializer);

	virtual bool CanIncludeSubnodesInSubnodeQuery(const UHTNComponent& OwnerComp, const FHTNPlanStepID& ThisStepID, int32 SubLevelIndex, bool bOnlyStarting, bool bOnlyEnding) const override;

	TArrayView<UHTNStandaloneNode*> GetPrimaryNextNodes() const;
	TArrayView<UHTNStandaloneNode*> GetSecondaryNextNodes() const;

protected:
	// Helpers for creating sublevels for the primary/secondary branch during planning.
	int32 AddInlinePrimaryLevel(FHTNPlanningContext& Context, FHTNPlan& Plan, const FHTNPlanStepID& AddedStepID);
	int32 AddInlineSecondaryLevel(FHTNPlanningContext& Context, FHTNPlan& Plan, const FHTNPlanStepID& AddedStepID);

public:
	// Starting at this index, nodes in the NextNodes array are secondary.
	UPROPERTY()
	int32 NumPrimaryNodes;
};