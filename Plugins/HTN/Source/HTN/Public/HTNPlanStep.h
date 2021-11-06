// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlackboardWorldstate.h"
#include "HTNStandaloneNode.h"

template<typename SubNodeType = UHTNNode>
struct THTNNodeInfo
{
	SubNodeType* TemplateNode;
	uint16 NodeMemoryOffset;
};

// A step in an plan. Each standalone node contributes one step.
struct HTN_API FHTNPlanStep
{
	// The standalone node of this plan step.
	// Can be a Task, a SubNetwork, or a structural node like If, Parallel, Prefer etc.
	// This is the template node owned by the HTN asset. 
	// If the node is supposed to be instanced, use the NodeMemoryOffset to find the node instance in the HTNComponent.
	TWeakObjectPtr<UHTNStandaloneNode> Node;

	// The worldstate the task returned during planning and then possibly modified by decorators OnPlanExit.
	// Also stores info on which blackboard keys were changed by this plan step.
	// If the plan step execution succeeds, those keys will be copied to the blackboard.
	// Can be null during planning for nodes with incomplete sublevels (e.g. SubNetwork, If, Prefer, Scope, Sequence etc.).
	TSharedPtr<FBlackboardWorldState> WorldState;

	// The cost of this step, as decided by the Node during planning.
	int32 Cost;

	// Non-task standalone nodes (SubNetworks, structural nodes like Prefer etc.) can have one or two sublevels contained within them.
	// This is the index of the primary sublevel into the FHTNPlan::Levels array of the plan this step is in.
	int32 SubLevelIndex;

	// Some structural nodes (Parallel, If, AnyOrder etc.) produce a plan step with two sublevels, one for each of the branches. 
	// This stores the index of the secondary sublevel.
	int32 SecondarySubLevelIndex;

	// Extra flags used by specific structural nodes.
	bool bAnyOrderInversed : 1;
	bool bIsIfNodeFalseBranch : 1;
	bool bCanConditionsInterruptTrueBranch : 1;
	bool bCanConditionsInterruptFalseBranch : 1;

	// Set by the planner. 
	// If the Node has decorators which made changes to the worldstate OnPlanEnter, this will contain the worldstate modified by them. 
	// Changes in this will be applied before executing the task itself.
	TSharedPtr<FBlackboardWorldState> WorldStateAfterEnteringDecorators;

	// Set when initializing for execution.
	// Offsets into the PlanMemory array of HTNComponent for the standalone Node and its Decorators and Services,
	// so it's possible to find the plan-specific memory that was allocated for each node.
	uint16 NodeMemoryOffset;
	TArray<THTNNodeInfo<UHTNDecorator>> DecoratorInfos;
	TArray<THTNNodeInfo<UHTNService>> ServiceInfos;
	
	explicit FHTNPlanStep(UHTNStandaloneNode* Node = nullptr, TSharedPtr<FBlackboardWorldState> WorldState = nullptr, int32 Cost = 0, int32 SubLevelIndex = INDEX_NONE, int32 ParallelSubLevelIndex = INDEX_NONE) :
		Node(Node),
		WorldState(WorldState),
		Cost(Cost),
		SubLevelIndex(SubLevelIndex),
		SecondarySubLevelIndex(ParallelSubLevelIndex),
		bAnyOrderInversed(false),
		bIsIfNodeFalseBranch(false),
		bCanConditionsInterruptTrueBranch(true),
		bCanConditionsInterruptFalseBranch(true),
		NodeMemoryOffset(0)
	{}

	int32 GetFirstSubLevelIndex() const;
	int32 GetLastSubLevelIndex() const;
};