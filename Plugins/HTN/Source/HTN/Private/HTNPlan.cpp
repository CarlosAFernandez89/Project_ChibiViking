// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "HTNPlan.h"
#include "Algo/AnyOf.h"
#include "Algo/Transform.h"

#include "HTNDecorator.h"
#include "HTNService.h"
#include "HTNTask.h"
#include "Nodes/HTNNode_SubNetwork.h"
#include "Nodes/HTNNode_Parallel.h"
#include "Nodes/HTNNode_If.h"

const FHTNPlanStepID FHTNPlanStepID::None = { INDEX_NONE, INDEX_NONE };

FHTNPlan::FHTNPlan(UHTN* HTNAsset, TSharedRef<FBlackboardWorldState> WorldStateAtPlanStart) :
	Levels { MakeShared<FHTNPlanLevel>(HTNAsset, WorldStateAtPlanStart) },
	Cost(0)
{}

TSharedRef<FHTNPlan> FHTNPlan::MakeCopy(int32 IndexOfLevelToCopy, bool bAlsoCopyParentLevel) const
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FHTNPlan::MakeCopy"), STAT_AI_HTN_PlanMakeCopy, STATGROUP_AI_HTN);
	
	struct Local
	{
		static bool CopyLevel(FHTNPlan& Plan, int32 LevelIndex)
		{
			if (ensure(Plan.HasLevel(LevelIndex)))
			{
				Plan.Levels[LevelIndex] = MakeShared<FHTNPlanLevel>(*Plan.Levels[LevelIndex]);
				return true;
			}
			
			return false;
		}
	};
	
	const TSharedRef<FHTNPlan> NewPlan = MakeShared<FHTNPlan>(*this);
	if (Local::CopyLevel(*NewPlan, IndexOfLevelToCopy))
	{
		if (bAlsoCopyParentLevel && IndexOfLevelToCopy > 0)
		{
			const int32 ParentLevelIndex = NewPlan->Levels[IndexOfLevelToCopy]->ParentStepID.LevelIndex;
			Local::CopyLevel(*NewPlan, ParentLevelIndex);
		}
	}

	return NewPlan;
}

bool FHTNPlan::HasLevel(int32 LevelIndex) const
{
	return Levels.IsValidIndex(LevelIndex) && Levels[LevelIndex].IsValid();
}

bool FHTNPlan::IsComplete() const
{
	for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); ++LevelIndex)
	{
		if (!IsLevelComplete(LevelIndex))
		{
			return false;
		}
	}

	return true;
}


bool FHTNPlan::IsLevelComplete(int32 LevelIndex) const
{
	if (!ensure(HasLevel(LevelIndex)))
	{
		return false;
	}

	const FHTNPlanLevel& Level = *Levels[LevelIndex];

	if (!Level.Steps.Num())
	{
		return false;
	}

	const FHTNPlanStep& LastStepInLevel = Level.Steps.Last();

	const bool bHasInlinePrimarySubLevel = LastStepInLevel.SubLevelIndex != INDEX_NONE && Levels[LastStepInLevel.SubLevelIndex]->IsInlineLevel();
	const bool bHasInlineSecondarySubLevel = LastStepInLevel.SecondarySubLevelIndex != INDEX_NONE && Levels[LastStepInLevel.SecondarySubLevelIndex]->IsInlineLevel();
	if (bHasInlinePrimarySubLevel || bHasInlineSecondarySubLevel || Cast<UHTNNode_TwoBranches>(LastStepInLevel.Node))
	{
		if (bHasInlinePrimarySubLevel && !IsLevelComplete(LastStepInLevel.SubLevelIndex))
		{
			return false;
		}

		if (bHasInlineSecondarySubLevel && !IsLevelComplete(LastStepInLevel.SecondarySubLevelIndex))
		{
			return false;
		}

		return true;
	}

	return LastStepInLevel.Node->NextNodes.Num() == 0;
}

bool FHTNPlan::FindStepToAddAfter(FHTNPlanStepID& OutPlanStepID) const
{
	bool bIncompleteLevelSkippedBecauseWorldStateNotSet = false;

	// Find an incomplete level, starting with the deepest ones.
	for (int32 LevelIndex = Levels.Num() - 1; LevelIndex >= 0; --LevelIndex)
	{
		if (!IsLevelComplete(LevelIndex))
		{
			const FHTNPlanLevel& Level = *Levels[LevelIndex];
			if (Level.WorldStateAtLevelStart.IsValid())
			{
				OutPlanStepID = { LevelIndex, Level.Steps.Num() ? Level.Steps.Num() - 1 : INDEX_NONE };
				return true;
			}
			else
			{
				bIncompleteLevelSkippedBecauseWorldStateNotSet = true;
			}
		}
	}

	ensureMsgf(!bIncompleteLevelSkippedBecauseWorldStateNotSet, TEXT("The only remaining incomplete plan levels don't have a worldstate set!"));
	OutPlanStepID = FHTNPlanStepID::None;
	return false;
}

void FHTNPlan::GetWorldStateAndNextNodes(const FHTNPlanStepID& StepID, TSharedPtr<FBlackboardWorldState>& OutWorldState, TArrayView<UHTNStandaloneNode*>& OutNextNodes) const
{
	const FHTNPlanLevel& Level = *Levels[StepID.LevelIndex];

	// The beginning of a level
	if (StepID.StepIndex == INDEX_NONE)
	{
		check(Level.WorldStateAtLevelStart.IsValid());
		check(Level.HTNAsset.IsValid());

		OutWorldState = Level.WorldStateAtLevelStart;

		if (!Level.IsInlineLevel())
		{
			OutNextNodes = Level.HTNAsset->StartNodes;
		}
		else
		{
			const FHTNPlanStep& ParentPlanStep = GetStep(Level.ParentStepID);
			if (UHTNNode_TwoBranches* const TwoBranchesNode = Cast<UHTNNode_TwoBranches>(ParentPlanStep.Node))
			{
				const bool bIsPrimaryBranch = StepID.LevelIndex == ParentPlanStep.SubLevelIndex;
				const bool bEffectivePrimaryBranch = ParentPlanStep.bAnyOrderInversed ? !bIsPrimaryBranch : bIsPrimaryBranch;
				OutNextNodes = bEffectivePrimaryBranch ? TwoBranchesNode->GetPrimaryNextNodes() : TwoBranchesNode->GetSecondaryNextNodes();
			}
			else if (UHTNStandaloneNode* const StandaloneNode = Cast<UHTNStandaloneNode>(ParentPlanStep.Node))
			{
				OutNextNodes = StandaloneNode->NextNodes;
			}
			else
			{
				checkNoEntry();
			}
		}
	}
	// If given a valid step, just return the WS and NextNodes of that step
	else
	{
		check(Level.Steps.IsValidIndex(StepID.StepIndex));
		const FHTNPlanStep& StepToAddAfter = Level.Steps[StepID.StepIndex];
		check(StepToAddAfter.Node.IsValid());
		check(StepToAddAfter.WorldState.IsValid());

		OutWorldState = StepToAddAfter.WorldState;
		OutNextNodes = StepToAddAfter.Node->NextNodes;
	}
}

void FHTNPlan::CheckIntegrity() const
{
#if DO_CHECK
	
	check(Levels.Num());
	check(Cost == Levels[0]->Cost);
	for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); ++LevelIndex)
	{	
		check(Levels[LevelIndex].IsValid());
		const FHTNPlanLevel& Level = *Levels[LevelIndex];

		if (LevelIndex == 0)
		{
			check(Level.ParentStepID == FHTNPlanStepID::None);
			check(!Level.IsInlineLevel());
		}
		else
		{
			check(Level.ParentStepID.LevelIndex != INDEX_NONE);
			check(Level.ParentStepID.StepIndex != INDEX_NONE);
		}
		
		check(Level.Steps.Num());
		for (int32 StepIndex = 0; StepIndex < Level.Steps.Num(); ++StepIndex)
		{
			const FHTNPlanStep& Step = Level.Steps[StepIndex];
			
			if (Cast<UHTNTask>(Step.Node))
			{
				check(Step.SubLevelIndex == INDEX_NONE);
				check(Step.SecondarySubLevelIndex == INDEX_NONE);
				check(Step.WorldState.IsValid());
			}
			else if (Cast<UHTNNode_SubNetwork>(Step.Node))
			{
				check(Step.SecondarySubLevelIndex == INDEX_NONE);
			}
			else
			{
				if (Step.SubLevelIndex != INDEX_NONE)
				{
					check(HasLevel(Step.SubLevelIndex));
					
					const FHTNPlanLevel& SubLevel = *Levels[Step.SubLevelIndex];
					check(SubLevel.ParentStepID.LevelIndex == LevelIndex);
					check(SubLevel.ParentStepID.StepIndex == StepIndex);
				}

				if (Step.SecondarySubLevelIndex != INDEX_NONE)
				{
					check(HasLevel(Step.SecondarySubLevelIndex));
					
					const FHTNPlanLevel& SecondarySubLevel = *Levels[Step.SecondarySubLevelIndex];
					check(SecondarySubLevel.ParentStepID.LevelIndex == LevelIndex);
					check(SecondarySubLevel.ParentStepID.StepIndex == StepIndex);
				}
			}

			check(Step.Cost >= 0);
		}
	}
#endif
}

void FHTNPlan::InitializeForExecution(UHTNComponent& OwnerComponent, UHTN& HTNAsset, TArray<uint8>& OutPlanMemory, TArray<UHTNNode*>& OutNodeInstances)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FHTNPlan::InitializeForExecution"), STAT_AI_HTN_PlanInitializeForExecution, STATGROUP_AI_HTN);

	check(OutPlanMemory.Num() == 0);
	check(OutNodeInstances.Num() == 0);
	CheckIntegrity();
	
	struct Local
	{
		// Round to 4 bytes
		static int32 GetAlignedDataSize(int32 Size) { return (Size + 3) & ~3; };
	};

	struct FNodeInitInfo
	{
		UHTNNode* NodeTemplate;
		uint16 MemoryOffset;
		FHTNPlanStepID StepID;
	};
	
	uint16 TotalNumBytesNeeded = 0;
	TArray<FNodeInitInfo> InitList;
	const auto RecordNode = [&](UHTNNode& NodeTemplate, uint16& OutMemoryOffset, const FHTNPlanStepID& StepID)
	{
		const uint16 SpecialDataSize = Local::GetAlignedDataSize(NodeTemplate.GetSpecialMemorySize());
		OutMemoryOffset = TotalNumBytesNeeded + SpecialDataSize;

		const uint16 TotalDataSize = Local::GetAlignedDataSize(SpecialDataSize + NodeTemplate.GetInstanceMemorySize());
		TotalNumBytesNeeded += TotalDataSize;

		InitList.Add({ &NodeTemplate, OutMemoryOffset, StepID });
	};

	for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); ++LevelIndex)
	{
		FHTNPlanLevel& Level = *Levels[LevelIndex];
		const FHTNPlanStepID RootStepID = Level.ParentStepID;
		
		// Do root decorators
		const TArrayView<UHTNDecorator*> DecoratorTemplates = Level.GetRootDecoratorTemplates();
		check(Level.RootDecoratorInfos.Num() == 0);
		Level.RootDecoratorInfos.Reserve(DecoratorTemplates.Num());
		for (UHTNDecorator* const Decorator : DecoratorTemplates)
		{
			if (Decorator)
			{
				RecordNode(*Decorator, Level.RootDecoratorInfos.Add_GetRef({ Decorator, 0 }).NodeMemoryOffset, RootStepID);
			}
		}

		// Do root services
		const TArrayView<UHTNService*> ServiceTemplates = Level.GetRootServiceTemplates();
		check(Level.RootServiceInfos.Num() == 0);
		Level.RootServiceInfos.Reserve(ServiceTemplates.Num());
		for (UHTNService* const Service : ServiceTemplates)
		{
			if (Service)
			{
				RecordNode(*Service, Level.RootServiceInfos.Add_GetRef({ Service, 0 }).NodeMemoryOffset, RootStepID);
			}
		}

		// Do steps
		for (int32 StepIndex = 0; StepIndex < Level.Steps.Num(); ++StepIndex)
		{
			const FHTNPlanStepID StepID { LevelIndex, StepIndex };
			FHTNPlanStep& Step = Level.Steps[StepIndex];
			
			check(Step.NodeMemoryOffset == 0);
			if (Step.Node.IsValid())
			{
				RecordNode(*Step.Node, Step.NodeMemoryOffset, StepID);
			}

			check(Step.DecoratorInfos.Num() == 0);
			Step.DecoratorInfos.Reserve(Step.Node->Decorators.Num());
			for (UHTNDecorator* const Decorator : Step.Node->Decorators)
			{
				RecordNode(*Decorator, Step.DecoratorInfos.Add_GetRef({Decorator, 0}).NodeMemoryOffset, StepID);
			}

			check(Step.ServiceInfos.Num() == 0);
			Step.ServiceInfos.Reserve(Step.Node->Services.Num());
			for (UHTNService* const Service : Step.Node->Services)
			{
				RecordNode(*Service, Step.ServiceInfos.Add_GetRef({Service, 0}).NodeMemoryOffset, StepID);
			}
		}
	}

	OutPlanMemory.SetNumZeroed(TotalNumBytesNeeded);
	for (const FNodeInitInfo& NodeInitInfo : InitList)
	{
		uint8* const RawNodeMemory = OutPlanMemory.GetData() + NodeInitInfo.MemoryOffset;
		NodeInitInfo.NodeTemplate->InitializeFromAsset(HTNAsset);
		NodeInitInfo.NodeTemplate->InitializeInPlan(OwnerComponent, RawNodeMemory, *this, NodeInitInfo.StepID, OutNodeInstances);
	}
}

void FHTNPlan::CleanupAfterExecution(UHTNComponent& OwnerComponent)
{
	const auto CleanupNode = [&](UHTNNode* NodeTemplate, uint16 MemoryOffset)
	{
		if (!ensure(NodeTemplate))
		{
			return;
		}

		NodeTemplate->CleanupInPlan(OwnerComponent, OwnerComponent.GetNodeMemory(MemoryOffset));
	};
	
	for (const TSharedPtr<FHTNPlanLevel>& Level : Levels)
	{
		// Do root decorators
		for (THTNNodeInfo<UHTNDecorator>& DecoratorInfo : Level->RootDecoratorInfos)
		{
			CleanupNode(DecoratorInfo.TemplateNode, DecoratorInfo.NodeMemoryOffset);
		}

		// Do root services
		for (THTNNodeInfo<UHTNService>& ServiceInfo : Level->RootServiceInfos)
		{
			CleanupNode(ServiceInfo.TemplateNode, ServiceInfo.NodeMemoryOffset);
		}

		// Do steps
		for (FHTNPlanStep& Step : Level->Steps)
		{
			if (Step.Node.IsValid())
			{
				CleanupNode(Step.Node.Get(), Step.NodeMemoryOffset);
			}

			for (THTNNodeInfo<UHTNDecorator>& DecoratorInfo : Step.DecoratorInfos)
			{
				CleanupNode(DecoratorInfo.TemplateNode, DecoratorInfo.NodeMemoryOffset);
			}

			for (THTNNodeInfo<UHTNService>& ServiceInfo : Step.ServiceInfos)
			{
				CleanupNode(ServiceInfo.TemplateNode, ServiceInfo.NodeMemoryOffset);
			}
		}
	}
}

const FHTNPlanStep& FHTNPlan::GetStep(const FHTNPlanStepID& PlanStepID) const
{
	return const_cast<FHTNPlan*>(this)->GetStep(PlanStepID);
}

FHTNPlanStep& FHTNPlan::GetStep(const FHTNPlanStepID& PlanStepID)
{
	check(HasLevel(PlanStepID.LevelIndex));
	FHTNPlanLevel& Level = *Levels[PlanStepID.LevelIndex];
	check(Level.Steps.IsValidIndex(PlanStepID.StepIndex));
	return Level.Steps[PlanStepID.StepIndex];
}

const FHTNPlanStep* FHTNPlan::FindStep(const FHTNPlanStepID& PlanStepID) const
{
	return const_cast<FHTNPlan*>(this)->FindStep(PlanStepID);
}

FHTNPlanStep* FHTNPlan::FindStep(const FHTNPlanStepID& PlanStepID)
{
	if (HasLevel(PlanStepID.LevelIndex))
	{
		FHTNPlanLevel& Level = *Levels[PlanStepID.LevelIndex];
		if (Level.Steps.IsValidIndex(PlanStepID.StepIndex))
		{
			return &Level.Steps[PlanStepID.StepIndex];
		}
	}
	
	return nullptr;
}

bool FHTNPlan::HasStep(const FHTNPlanStepID& StepID, int32 LevelIndex) const
{
	if (!HasLevel(StepID.LevelIndex) || !HasLevel(LevelIndex))
	{
		return false;
	}

	if (StepID.LevelIndex == LevelIndex)
	{
		return true;
	}

	if (StepID.LevelIndex == 0)
	{
		return false;
	}
	
	const FHTNPlanStepID& ParentStepID = Levels[StepID.LevelIndex]->ParentStepID;
	return HasStep(ParentStepID, LevelIndex);
}

bool FHTNPlan::IsSecondaryParallelStep(const FHTNPlanStepID& StepID) const
{
	if (!ensure(HasLevel(StepID.LevelIndex)))
	{
		return false;
	}

	FHTNPlanStepID CurrentStepID = StepID;
	while (CurrentStepID != FHTNPlanStepID::None)
	{
		const FHTNPlanStepID ParentStepID = Levels[CurrentStepID.LevelIndex]->ParentStepID;
		if (ParentStepID == FHTNPlanStepID::None)
		{
			break;
		}

		const FHTNPlanStep& ParentStep = GetStep(ParentStepID);
		if (Cast<UHTNNode_Parallel>(ParentStep.Node))
		{
			return CurrentStepID.LevelIndex == ParentStep.SecondarySubLevelIndex;
		}
		
		CurrentStepID = ParentStepID;
	}

	return false;
}

int32 FHTNPlan::GetNextPrimitiveSteps(const UHTNComponent& OwnerComp, const FHTNPlanStepID& InStepID, TArray<FHTNPlanStepID>& OutStepIds, bool bIsExecutingPlan) const
{
	FHTNGetNextStepsContext Context(OwnerComp, *this, bIsExecutingPlan, OutStepIds);
	Context.AddNextPrimitiveStepsAfter(InStepID);
	return Context.GetNumSubmittedSteps();
}

void FHTNPlan::GetSubNodesAtPlanStep(const FHTNPlanStepID& StepID, TArray<FHTNSubNodeGroup>& OutSubNodeGroups, bool bOnlyStarting, bool bOnlyEnding) const
{
	if (!ensure(HasStep(StepID)))
	{
		return;
	}
	
	FHTNPlanStepID CurrentStepID = StepID;
	while (true)
	{
		const FHTNPlanLevel& Level = *Levels[CurrentStepID.LevelIndex];
		const FHTNPlanStep& Step = Level.Steps[CurrentStepID.StepIndex];
		OutSubNodeGroups.Emplace(&Step.DecoratorInfos, &Step.ServiceInfos, CurrentStepID, 
			Step.bIsIfNodeFalseBranch, Step.bCanConditionsInterruptTrueBranch, Step.bCanConditionsInterruptFalseBranch
		);

		if ((!bOnlyStarting || CurrentStepID.StepIndex == 0) && (!bOnlyEnding || CurrentStepID.StepIndex == Level.Steps.Num() - 1))
		{
			OutSubNodeGroups.Emplace(&Level.RootDecoratorInfos, &Level.RootServiceInfos, Level.ParentStepID);

			if (CurrentStepID.LevelIndex > 0)
			{
				CurrentStepID = Level.ParentStepID;
				continue;
			}
		}

		break;
	}
}

void FHTNPlan::GetSubNodesAtExecutingPlanStep(const UHTNComponent& OwnerComp, 
	const FHTNPlanStepID& StepID, TArray<FHTNSubNodeGroup>& OutSubNodeGroups, bool bOnlyStarting, bool bOnlyEnding
) const
{
	if (!ensure(HasStep(StepID)))
	{
		return;
	}

	FHTNPlanStepID CurrentStepID = StepID;
	while (true)
	{
		const FHTNPlanLevel& Level = *Levels[CurrentStepID.LevelIndex];
		const FHTNPlanStep& Step = Level.Steps[CurrentStepID.StepIndex];
		OutSubNodeGroups.Emplace(&Step.DecoratorInfos, &Step.ServiceInfos, CurrentStepID,
			Step.bIsIfNodeFalseBranch, Step.bCanConditionsInterruptTrueBranch, Step.bCanConditionsInterruptFalseBranch
		);

		if ((!bOnlyStarting || CurrentStepID.StepIndex == 0) && (!bOnlyEnding || CurrentStepID.StepIndex == Level.Steps.Num() - 1))
		{
			OutSubNodeGroups.Emplace(&Level.RootDecoratorInfos, &Level.RootServiceInfos, Level.ParentStepID);

			if (CurrentStepID.LevelIndex > 0)
			{
				const FHTNPlanStep& ParentStep = GetStep(Level.ParentStepID);
				if (ParentStep.Node->CanIncludeSubnodesInSubnodeQuery(OwnerComp, Level.ParentStepID, CurrentStepID.LevelIndex, bOnlyStarting, bOnlyEnding))
				{
					CurrentStepID = Level.ParentStepID;
					continue;
				}
			}
		}

		break;
	}
}

TSharedPtr<const FBlackboardWorldState> FHTNPlan::GetWorldstateBeforeDecoratorPlanEnter(const UHTNDecorator& Decorator, const FHTNPlanStepID& ActiveStepID) const
{
	const FHTNPlanStepID StartStepID = FindDecoratorStartStepID(Decorator, ActiveStepID);
	if (Levels.IsValidIndex(StartStepID.LevelIndex))
	{
		const FHTNPlanLevel& Level = *Levels[StartStepID.LevelIndex];
		return StartStepID.StepIndex > 0 ?
			Level.Steps[StartStepID.StepIndex - 1].WorldState :
			Level.WorldStateAtLevelStart;
	}

	return nullptr;
}

FHTNPlanStepID FHTNPlan::FindDecoratorStartStepID(const UHTNDecorator& Decorator, const FHTNPlanStepID& ActiveStepID) const
{
	if (!Levels.IsValidIndex(ActiveStepID.LevelIndex))
	{
		return FHTNPlanStepID::None;
	}

	const UHTNNode* const TemplateDecorator = Decorator.GetTemplateNode();

	if (ActiveStepID.StepIndex == INDEX_NONE)
	{
		if (Levels[ActiveStepID.LevelIndex]->GetRootDecoratorTemplates().Contains(TemplateDecorator))
		{
			return ActiveStepID;
		}
	}
	else if (const FHTNPlanStep* const Step = FindStep(ActiveStepID))
	{
		if (const UHTNStandaloneNode* const Node = Step->Node.Get())
		{
			if (Node->Decorators.Contains(TemplateDecorator))
			{
				return ActiveStepID;
			}
		}
	}

	TArray<FHTNSubNodeGroup> SubNodeGroups;
	GetSubNodesAtPlanStep(ActiveStepID, SubNodeGroups);
	for (const FHTNSubNodeGroup& Group : SubNodeGroups)
	{
		if (Group.Decorators->ContainsByPredicate([&](const THTNNodeInfo<UHTNDecorator>& Info) { return Info.TemplateNode == TemplateDecorator; }))
		{
			return Group.PlanStepID;
		}
	}

	return FHTNPlanStepID::None;
}

int32 FHTNPlan::GetRecursionCount(UHTNNode* Node) const
{
	if (RecursionCounts.IsValid())
	{
		if (const int32* const FoundCount = RecursionCounts->Find(Node))
		{
			return *FoundCount;
		}
	}

	return 0;
}

void FHTNPlan::IncrementRecursionCount(UHTNNode* Node)
{
	if (!RecursionCounts.IsValid())
	{
		RecursionCounts = MakeShared<TMap<TWeakObjectPtr<UHTNNode>, int32>>();
		RecursionCounts->Add(Node, 1);
	}
	else
	{
		RecursionCounts = MakeShared<TMap<TWeakObjectPtr<UHTNNode>, int32>>(*RecursionCounts);
		int32& Count = RecursionCounts->FindOrAdd(Node, 0);
		Count += 1;
	}
}

TArrayView<UHTNDecorator*> FHTNPlanLevel::GetRootDecoratorTemplates() const
{
	if (!IsInlineLevel() && HTNAsset.IsValid())
	{
		return HTNAsset->RootDecorators;
	}

	return {};
}

TArrayView<UHTNService*> FHTNPlanLevel::GetRootServiceTemplates() const
{
	if (!IsInlineLevel() && HTNAsset.IsValid())
	{
		return HTNAsset->RootServices;
	}

	return {};
}

FHTNGetNextStepsContext::FHTNGetNextStepsContext(const UHTNComponent& OwnerComp,
	const FHTNPlan& Plan, bool bIsExecutingPlan,
	TArray<FHTNPlanStepID>& OutStepIds
) : OwnerComp(OwnerComp),
	Plan(Plan),
	bIsExecutingPlan(bIsExecutingPlan),
	OutStepIds(OutStepIds),
	NumSubmittedSteps(0)
{}

void FHTNGetNextStepsContext::SubmitPlanStep(const FHTNPlanStepID& PlanStepID)
{
	OutStepIds.Add(PlanStepID);
	++NumSubmittedSteps;
}

int32 FHTNGetNextStepsContext::AddNextPrimitiveStepsAfter(const FHTNPlanStepID& InStepID)
{
	if (!Plan.HasLevel(InStepID.LevelIndex))
	{
		return 0;
	}

	const FHTNPlanLevel& Level = *Plan.Levels[InStepID.LevelIndex];

	const auto GetNumStepsSubmittedNow = [OldNumSubmittedSteps = NumSubmittedSteps, this]()
	{
		return NumSubmittedSteps - OldNumSubmittedSteps;
	};

	// Check steps of this plan and steps in their sublevels, recursively
	for (int32 StepIndex = InStepID.StepIndex + 1; StepIndex < Level.Steps.Num(); ++StepIndex)
	{
		const FHTNPlanStep& CandidateStep = Level.Steps[StepIndex];
		CandidateStep.Node->GetNextPrimitiveSteps(*this, {InStepID.LevelIndex, StepIndex});
		if (const int32 NumSubmittedNow = GetNumStepsSubmittedNow())
		{
			return NumSubmittedNow;
		}
	}

	// Level finished, check steps of the parent level, recursively.
	if (Level.ParentStepID != FHTNPlanStepID::None)
	{
		const FHTNPlanStep& ParentStep = Plan.GetStep(Level.ParentStepID);
		ParentStep.Node->GetNextPrimitiveSteps(*this, Level.ParentStepID, InStepID.LevelIndex);
	}

	const int32 NumSubmittedNow = GetNumStepsSubmittedNow();
	return NumSubmittedNow;
}

int32 FHTNGetNextStepsContext::AddFirstPrimitiveStepsInLevel(int32 LevelIndex)
{
	return AddNextPrimitiveStepsAfter({ LevelIndex, INDEX_NONE });
}

int32 FHTNGetNextStepsContext::AddFirstPrimitiveStepsInAnySublevelOf(const FHTNPlanStepID& StepID)
{
	if (const FHTNPlanStep* const Step = Plan.FindStep(StepID))
	{
		if (const int32 AddedFromTopBranch = AddFirstPrimitiveStepsInLevel(Step->SubLevelIndex))
		{
			return AddedFromTopBranch;
		}

		return AddFirstPrimitiveStepsInLevel(Step->SecondarySubLevelIndex);
	}

	return 0;
}

