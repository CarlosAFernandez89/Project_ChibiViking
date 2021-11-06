// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HTNPlanStep.h"

struct HTN_API FHTNSubNodeGroup
{
	const TArray<THTNNodeInfo<class UHTNDecorator>>* Decorators;
	const TArray<THTNNodeInfo<class UHTNService>>* Services;

	FHTNPlanStepID PlanStepID;
	bool bIsIfNodeFalseBranch : 1;
	bool bCanConditionsInterruptTrueBranch : 1;
	bool bCanConditionsInterruptFalseBranch : 1;

	FHTNSubNodeGroup
	(
		const TArray<THTNNodeInfo<class UHTNDecorator>>* Decorators, 
		const TArray<THTNNodeInfo<class UHTNService>>* Services, 
		FHTNPlanStepID PlanStepID,
		bool bIsIfNodeFalseBranch = false,
		bool bCanConditionsInterruptTrueBranch = true,
		bool bCanConditionsInterruptFalseBranch = true
	) :
		Decorators(Decorators),
		Services(Services),
		PlanStepID(PlanStepID),
		bIsIfNodeFalseBranch(bIsIfNodeFalseBranch),
		bCanConditionsInterruptTrueBranch(bCanConditionsInterruptTrueBranch),
		bCanConditionsInterruptFalseBranch(bCanConditionsInterruptFalseBranch)
	{}
};

struct HTN_API FHTNPlan
{
	// Each plan level corresponds to a compound task.
	// If you have an plan with a single compound task which only has primitive tasks, there will be two levels.
	TArray<TSharedPtr<struct FHTNPlanLevel>> Levels;

	// The sum of the costs of the Levels.
	int32 Cost;

	// For tasks with a recursion limit, stores how many times each task is present in this plan.
	// Since most plan expansions don't change this, the map is shared between most plans and only copied when a task with a recursion limit is added.
	TSharedPtr<TMap<TWeakObjectPtr<UHTNNode>, int32>> RecursionCounts;

	// Allows for some plans to be prioritized over others regardless of cost.
	// Positive markers block the corresponding negative markers.
	// For example, a plan with marker -5 will not be considered for as long as there are any possible plans with marker 5.
	// This way, it's possible to make the planner consider all plans in the top branch of a Prefer node 
	// before the ones in the bottom branch.
	TArray<FHTNPriorityMarker, TInlineAllocator<8>> PriorityMarkers;

	FHTNPlan(UHTN* HTNAsset, TSharedRef<FBlackboardWorldState> WorldStateAtPlanStart);
	TSharedRef<FHTNPlan> MakeCopy(int32 IndexOfLevelToCopy, bool bAlsoCopyParentLevel = false) const;
	bool HasLevel(int32 LevelIndex) const;
	bool IsComplete() const;
	bool IsLevelComplete(int32 LevelIndex) const;
	
	bool FindStepToAddAfter(FHTNPlanStepID& OutPlanStepID) const;
	void GetWorldStateAndNextNodes(const FHTNPlanStepID& StepID, TSharedPtr<class FBlackboardWorldState>& OutWorldState, TArrayView<UHTNStandaloneNode*>& OutNextNodes) const;
	
	// Performs a number of checks to verify that the plan is valid and all cross-links via array indices are valid.
	void CheckIntegrity() const;
	
	const FHTNPlanStep& GetStep(const FHTNPlanStepID& PlanStepID) const;
	FHTNPlanStep& GetStep(const FHTNPlanStepID& PlanStepID);
	const FHTNPlanStep* FindStep(const FHTNPlanStepID& PlanStepID) const;
	FHTNPlanStep* FindStep(const FHTNPlanStepID& PlanStepID);
	bool HasStep(const FHTNPlanStepID& StepID, int32 LevelIndex = 0) const;
	bool IsSecondaryParallelStep(const FHTNPlanStepID& StepID) const;
	
	// Outputs the IDs of primitive steps after the given one. Returns the number of step IDs added.
	// If bIsExecutingPlan is true, takes into account parallel branches waiting for secondary branch to complete.
	int32 GetNextPrimitiveSteps(const UHTNComponent& OwnerComp, const FHTNPlanStepID& StepID, TArray<FHTNPlanStepID>& OutStepIds, bool bIsExecutingPlan) const;

	// Fills OutSubNodeGroups with groups of subnodes (decorators and services) that are active at plan step specified by StepID.
	// The innermost group (the one that starts last) is added first, the outermost group is added last.
	// Optionally only includes the subnodes starting or ending at this plan step.
	void GetSubNodesAtPlanStep(const FHTNPlanStepID& StepID, TArray<FHTNSubNodeGroup>& OutSubNodeGroups, bool bOnlyStarting = false, bool bOnlyEnding = false) const;

	// Similar to GetSubNodesAtPlanStep, but for during plan execution. 
	// Correctly filters out some subnode groups per the subnode scoping rules to make sure 
	// the right subnodes are started/ticked/finished at the right time during execution.
	void GetSubNodesAtExecutingPlanStep(const UHTNComponent& OwnerComp, 
		const FHTNPlanStepID& StepID, TArray<FHTNSubNodeGroup>& OutSubNodeGroups, bool bOnlyStarting = false, bool bOnlyEnding = false
	) const;

	// Given a decorator and a step ID during which it is active, finds the worldstate that was before the decorator became active.
	// This can be used for restoring worldstate/blackboard values to values they had before a decorator, which is useful for scope guards.
	TSharedPtr<const FBlackboardWorldState> GetWorldstateBeforeDecoratorPlanEnter(const class UHTNDecorator& Decorator, const FHTNPlanStepID& ActiveStepID) const;
	
	// Given a decorator and a step ID during which it is active, finds the first step ID at which this decorator became active.
	FHTNPlanStepID FindDecoratorStartStepID(const class UHTNDecorator& Decorator, const FHTNPlanStepID& ActiveStepID) const;
	
	int32 GetRecursionCount(UHTNNode* Node) const;
	void IncrementRecursionCount(UHTNNode* Node);

	// Allocates memory for nodes, instances them if required, initializes them and prepares for execution.
	void InitializeForExecution(class UHTNComponent& OwnerComponent, UHTN& HTNAsset, TArray<uint8>& OutPlanMemory, TArray<UHTNNode*>& OutNodeInstances);
	// Cleans up node memory after plan execution.
	void CleanupAfterExecution(class UHTNComponent& OwnerComponent);
};

// A sequence of plan steps
struct HTN_API FHTNPlanLevel
{
	TWeakObjectPtr<UHTN> HTNAsset;
	TSharedPtr<class FBlackboardWorldState> WorldStateAtLevelStart;

	TArray<FHTNPlanStep> Steps;

	// Step ID of the step containing this level
	FHTNPlanStepID ParentStepID;
	
	// The sum of the costs of the Steps.
	int32 Cost;

	bool bIsInline;
	
	TArray<THTNNodeInfo<class UHTNDecorator>> RootDecoratorInfos;
	TArray<THTNNodeInfo<class UHTNService>> RootServiceInfos;

	FHTNPlanLevel(UHTN* HTNAsset, TSharedPtr<class FBlackboardWorldState> WorldStateAtLevelStart, const FHTNPlanStepID& ParentStepID = FHTNPlanStepID::None, bool bIsInline = false) :
		HTNAsset(HTNAsset),
		WorldStateAtLevelStart(WorldStateAtLevelStart),
		ParentStepID(ParentStepID),
		Cost(0),
		bIsInline(bIsInline)
	{}

	FORCEINLINE bool IsInlineLevel() const { return bIsInline; }
	TArrayView<class UHTNDecorator*> GetRootDecoratorTemplates() const;
	TArrayView<class UHTNService*> GetRootServiceTemplates() const;
};

struct HTN_API FHTNGetNextStepsContext
{
	const UHTNComponent& OwnerComp;
	const FHTNPlan& Plan;
	bool bIsExecutingPlan;

	FHTNGetNextStepsContext(const UHTNComponent& OwnerComp, const FHTNPlan& Plan, bool bIsExecutingPlan, TArray<FHTNPlanStepID>& OutStepIds);

	void SubmitPlanStep(const FHTNPlanStepID& PlanStepID);
	FORCEINLINE int32 GetNumSubmittedSteps() const { return NumSubmittedSteps; }

	int32 AddNextPrimitiveStepsAfter(const FHTNPlanStepID& InStepID);
	int32 AddFirstPrimitiveStepsInLevel(int32 LevelIndex);
	int32 AddFirstPrimitiveStepsInAnySublevelOf(const FHTNPlanStepID& StepID);

private:
	TArray<FHTNPlanStepID>& OutStepIds;
	int32 NumSubmittedSteps;
};