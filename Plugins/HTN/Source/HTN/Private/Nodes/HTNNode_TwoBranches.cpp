// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Nodes/HTNNode_TwoBranches.h"
#include "AITask_MakeHTNPlan.h"

UHTNNode_TwoBranches::UHTNNode_TwoBranches(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
	NumPrimaryNodes(INDEX_NONE)
{}

bool UHTNNode_TwoBranches::CanIncludeSubnodesInSubnodeQuery(const UHTNComponent& OwnerComp, 
	const FHTNPlanStepID& ThisStepID, int32 SubLevelIndex, bool bOnlyStarting, bool bOnlyEnding
) const
{
	const FHTNPlanStep& Step = OwnerComp.GetCurrentPlan()->GetStep(ThisStepID);
	// Only start subnodes if the starting sublevel is the first one.
	if (bOnlyStarting)
	{
		const int32 FirstSubLevelIndex = Step.GetFirstSubLevelIndex();
		return SubLevelIndex == FirstSubLevelIndex;
	}
	// Only end subnodes if the ending sublevel is the last one.
	else if (bOnlyEnding)
	{
		const int32 LastSubLevelIndex = Step.GetLastSubLevelIndex();
		return SubLevelIndex == LastSubLevelIndex;
	}
	// Always include all subnodes for ticking and aborting.
	else
	{
		return true;
	}
}

TArrayView<UHTNStandaloneNode*> UHTNNode_TwoBranches::GetPrimaryNextNodes() const
{
	TArray<UHTNStandaloneNode*>& Nodes = const_cast<UHTNNode_TwoBranches*>(this)->NextNodes;

	if (!ensure(NumPrimaryNodes != INDEX_NONE))
	{
		return TArrayView<UHTNStandaloneNode*>(Nodes);
	}
	
	return NumPrimaryNodes ?
		TArrayView<UHTNStandaloneNode*>(Nodes).Slice(0, NumPrimaryNodes) :
		TArrayView<UHTNStandaloneNode*>(Nodes.GetData(), 0);
}

TArrayView<UHTNStandaloneNode*> UHTNNode_TwoBranches::GetSecondaryNextNodes() const
{
	TArray<UHTNStandaloneNode*>& Nodes = const_cast<UHTNNode_TwoBranches*>(this)->NextNodes;
	
	if (!ensure(NumPrimaryNodes != INDEX_NONE))
	{
		return TArrayView<UHTNStandaloneNode*>(Nodes.GetData(), 0);
	}
	
	const int32 NumSecondaryNodes = NextNodes.Num() - NumPrimaryNodes;
	return NumSecondaryNodes ? 
		TArrayView<UHTNStandaloneNode*>(Nodes).Slice(NumPrimaryNodes, NumSecondaryNodes) :
		TArrayView<UHTNStandaloneNode*>(Nodes.GetData(), 0);
}

int32 UHTNNode_TwoBranches::AddInlinePrimaryLevel(FHTNPlanningContext& Context, FHTNPlan& Plan, const FHTNPlanStepID& AddedStepID)
{
	return GetPrimaryNextNodes().Num() > 0 ? Context.AddInlineLevel(Plan, AddedStepID) : INDEX_NONE;
}

int32 UHTNNode_TwoBranches::AddInlineSecondaryLevel(FHTNPlanningContext& Context, FHTNPlan& Plan, const FHTNPlanStepID& AddedStepID)
{
	return GetSecondaryNextNodes().Num() > 0 ? Context.AddInlineLevel(Plan, AddedStepID) : INDEX_NONE;
}
