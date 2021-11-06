// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "HTNNode.h"
#include "HTNStandaloneNode.generated.h"

// The base class for standalone nodes (as opposed to subnodes, like decorators or services).
UCLASS(Abstract)
class HTN_API UHTNStandaloneNode : public UHTNNode
{
	GENERATED_BODY()
	
public:
	UHTNStandaloneNode(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeFromAsset(class UHTN& Asset) override;
	virtual FString GetStaticDescription() const override;

	// Called during planning when planning reaches this node. Should create zero or more new plans and submit them via the planning context.
	virtual void MakePlanExpansions(struct FHTNPlanningContext& Context) {}
	// Called during planning when one of the sublevels of this node finished planning. Returns true if this node is finished.
	virtual bool OnSubLevelFinishedPlanning(struct FHTNPlan& Plan, const FHTNPlanStepID& ThisStepID, int32 SubLevelIndex, TSharedPtr<FBlackboardWorldState> WorldState);
	// Called during execution to decide what to execute when execution reaches this node.
	virtual void GetNextPrimitiveSteps(struct FHTNGetNextStepsContext& Context, const FHTNPlanStepID& ThisStepID);
	// Called during execution to decide what to execute when execution finishes in one of the sublevels of this node.
	virtual void GetNextPrimitiveSteps(struct FHTNGetNextStepsContext& Context, const FHTNPlanStepID& ThisStepID, int32 FinishedSubLevelIndex);
	// Called during execution to control the execution scope of subnodes. Decide which to start/tick/stop, depending on the bOnlyStarting and bOnlyEnding parameters.
	virtual bool CanIncludeSubnodesInSubnodeQuery(const UHTNComponent& OwnerComp, const FHTNPlanStepID& ThisStepID, int32 SubLevelIndex, bool bOnlyStarting, bool bOnlyEnding) const;

	// The maximum number of times this task can be present in a single plan. 0 means no limit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Planning, Meta = (ClampMin = "0"))
	int32 MaxRecursionLimit;
	
	// Nodes that this node connects to with outgoing arrows.
	UPROPERTY()
	TArray<UHTNStandaloneNode*> NextNodes;

	UPROPERTY()
	TArray<class UHTNDecorator*> Decorators;

	UPROPERTY()
	TArray<class UHTNService*> Services;
};