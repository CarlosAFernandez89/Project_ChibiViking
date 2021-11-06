// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Decorators/HTNDecorator_BlueprintBase.h"
#include "VisualLogger/VisualLogger.h"
#include "BlueprintNodeHelpers.h"
#include "AIController.h"
#include "WorldStateProxy.h"
#include "HTNBlueprintLibrary.h"

UHTNDecorator_BlueprintBase::UHTNDecorator_BlueprintBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
	bShowPropertyDetails(true)
{
#define IS_IMPLEMENTED(FunctionName) \
	(BlueprintNodeHelpers::HasBlueprintFunction(GET_FUNCTION_NAME_CHECKED(UHTNDecorator_BlueprintBase, FunctionName), *this, *StaticClass()))
	bImplementsPerformConditionCheck = IS_IMPLEMENTED(PerformConditionCheck);
	bImplementsModifyStepCost = IS_IMPLEMENTED(ReceiveModifyStepCost);
	bImplementsOnPlanEnter = IS_IMPLEMENTED(ReceiveOnPlanEnter);
	bImplementsOnPlanExit = IS_IMPLEMENTED(ReceiveOnPlanExit);
	bImplementsOnExecutionStart = IS_IMPLEMENTED(ReceiveExecutionStart);
	bImplementsTick = IS_IMPLEMENTED(ReceiveTick);
	bImplementsOnExecutionFinish = IS_IMPLEMENTED(ReceiveExecutionFinish);
	bImplementsOnPlanExecutionStarted = IS_IMPLEMENTED(ReceiveOnPlanExecutionStarted);
	bImplementsOnPlanExecutionFinished = IS_IMPLEMENTED(ReceiveOnPlanExecutionFinished);
#undef IS_IMPLEMENTED
	
	bNotifyOnEnterPlan = bImplementsOnPlanEnter;
	bNotifyOnExitPlan = bImplementsOnPlanExit;
	bNotifyExecutionStart = bImplementsOnExecutionStart;
	bNotifyTick = bImplementsTick;
	bNotifyExecutionFinish = bImplementsOnExecutionFinish;
	bNotifyOnPlanExecutionStarted = bImplementsOnPlanExecutionStarted;
	bNotifyOnPlanExecutionFinished = bImplementsOnPlanExecutionFinished;
	bModifyStepCost = bImplementsModifyStepCost;

	// All blueprint-based nodes must create instances
	bCreateNodeInstance = true;
	// Since tasks created from blueprints won't set this to true on their own.
	bOwnsGameplayTasks = true;
	
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		BlueprintNodeHelpers::CollectPropertyData(this, StaticClass(), PropertyData);
	}
}

void UHTNDecorator_BlueprintBase::InitializeFromAsset(UHTN& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (Asset.BlackboardAsset)
	{
		BlueprintNodeHelpers::ResolveBlackboardSelectors(*this, *StaticClass(), *Asset.BlackboardAsset);
	}
}

FString UHTNDecorator_BlueprintBase::GetStaticDescription() const
{
	FString Description = Super::GetStaticDescription();

	if (bShowPropertyDetails)
	{
		if (UHTNDecorator_BlueprintBase* const CDO = GetClass()->GetDefaultObject<UHTNDecorator_BlueprintBase>())
		{
			const FString PropertyDescription = BlueprintNodeHelpers::CollectPropertyDescription(this, StaticClass(), CDO->PropertyData);
			if (PropertyDescription.Len())
			{
				Description += TEXT(":\n\n");
				Description += PropertyDescription;
			}
		}
	}

	return Description;
}

bool UHTNDecorator_BlueprintBase::CalculateRawConditionValue(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNDecoratorConditionCheckType CheckType) const
{
	if (!bImplementsPerformConditionCheck)
	{
		return Super::CalculateRawConditionValue(OwnerComp, NodeMemory, CheckType);
	}

	FGuardValue_Bitfield(bForceUsingPlanningWorldState, CheckType == EHTNDecoratorConditionCheckType::PlanRecheck ? true : bForceUsingPlanningWorldState);
	SetOwnerComponent(&OwnerComp);
	check(UHTNNodeLibrary::GetOwnersWorldState(this) == GetWorldStateProxy(OwnerComp, CheckType));
	check(GetWorldStateProxy(OwnerComp, CheckType)->IsBlackboard() == (CheckType == EHTNDecoratorConditionCheckType::Execution));
	return PerformConditionCheck(
		OwnerComp.GetOwner(),
		OwnerComp.GetAIOwner(), 
		OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr,
		CheckType
	);
}

void UHTNDecorator_BlueprintBase::ModifyStepCost(UHTNComponent& OwnerComp, FHTNPlanStep& Step) const
{
	if (!bImplementsModifyStepCost)
	{
		Super::ModifyStepCost(OwnerComp, Step);
		return;
	}

	SetOwnerComponent(&OwnerComp);
	check(UHTNNodeLibrary::GetOwnersWorldState(this) == OwnerComp.GetPlanningWorldStateProxy());
	Step.Cost = ReceiveModifyStepCost(Step.Cost,
		OwnerComp.GetOwner(), 
		OwnerComp.GetAIOwner(), 
		OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr
	);

	if (Step.Cost < 0)
	{
		UE_VLOG_UELOG(OwnerComp.GetOwner(), LogHTN, Warning, TEXT("Plan step modified by %s is %i. Negative costs aren't allowed, resetting to zero."), *GetNodeName(), Step.Cost);
		Step.Cost = 0;
	}
}

void UHTNDecorator_BlueprintBase::OnEnterPlan(UHTNComponent& OwnerComp, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const
{
	if (!bImplementsOnPlanEnter)
	{
		return;
	}
	
	SetOwnerComponent(&OwnerComp);
	check(UHTNNodeLibrary::GetOwnersWorldState(this) == OwnerComp.GetPlanningWorldStateProxy());
	check(UHTNNodeLibrary::GetOwnersWorldState(this)->IsEditable());
	ReceiveOnPlanEnter(OwnerComp.GetOwner(), OwnerComp.GetAIOwner(), OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr);
}

void UHTNDecorator_BlueprintBase::OnExitPlan(UHTNComponent& OwnerComp, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const
{
	if (!bImplementsOnPlanExit)
	{
		return;
	}

	SetOwnerComponent(&OwnerComp);
	check(UHTNNodeLibrary::GetOwnersWorldState(this) == OwnerComp.GetPlanningWorldStateProxy());
	check(UHTNNodeLibrary::GetOwnersWorldState(this)->IsEditable());
	ReceiveOnPlanExit(OwnerComp.GetOwner(), OwnerComp.GetAIOwner(), OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr);
}

void UHTNDecorator_BlueprintBase::OnExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory)
{	
	if (!bImplementsOnExecutionStart)
	{
		return;
	}

	check(GetOwnerComponent() == &OwnerComp);
	check(UHTNNodeLibrary::GetOwnersWorldState(this) == OwnerComp.GetBlackboardProxy());
	check(UHTNNodeLibrary::GetOwnersWorldState(this)->IsBlackboard());
	check(UHTNNodeLibrary::GetOwnersWorldState(this)->IsEditable());
	ReceiveExecutionStart(OwnerComp.GetOwner(), OwnerComp.GetAIOwner(), OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr);
}

void UHTNDecorator_BlueprintBase::TickNode(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaTime)
{
	if (!bImplementsTick)
	{
		return;
	}

	check(GetOwnerComponent() == &OwnerComp);
	check(UHTNNodeLibrary::GetOwnersWorldState(this) == OwnerComp.GetBlackboardProxy());
	check(UHTNNodeLibrary::GetOwnersWorldState(this)->IsBlackboard());
	check(UHTNNodeLibrary::GetOwnersWorldState(this)->IsEditable());
	ReceiveTick(OwnerComp.GetOwner(), OwnerComp.GetAIOwner(), OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr, DeltaTime);
}

void UHTNDecorator_BlueprintBase::OnExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult Result)
{
	BlueprintNodeHelpers::AbortLatentActions(OwnerComp, *this);
	
	if (!bImplementsOnExecutionFinish)
	{
		return;
	}
	
	check(GetOwnerComponent() == &OwnerComp);
	check(UHTNNodeLibrary::GetOwnersWorldState(this) == OwnerComp.GetBlackboardProxy());
	check(UHTNNodeLibrary::GetOwnersWorldState(this)->IsBlackboard());
	check(UHTNNodeLibrary::GetOwnersWorldState(this)->IsEditable());
	ReceiveExecutionFinish(OwnerComp.GetOwner(), OwnerComp.GetAIOwner(), OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr, Result);
}

void UHTNDecorator_BlueprintBase::OnPlanExecutionStarted(UHTNComponent& OwnerComp, uint8* NodeMemory)
{
	if (!bImplementsOnPlanExecutionStarted)
	{
		return;
	}

	FGuardValue_Bitfield(bForceUsingPlanningWorldState, true);
	check(GetOwnerComponent() == &OwnerComp);
	check(UHTNNodeLibrary::GetOwnersWorldState(this) == OwnerComp.GetPlanningWorldStateProxy());
	check(!UHTNNodeLibrary::GetOwnersWorldState(this)->IsBlackboard());
	check(!UHTNNodeLibrary::GetOwnersWorldState(this)->IsEditable());
	ReceiveOnPlanExecutionStarted(OwnerComp.GetOwner(), OwnerComp.GetAIOwner(), OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr);
}

void UHTNDecorator_BlueprintBase::OnPlanExecutionFinished(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNPlanExecutionFinishedResult Result)
{
	if (!bImplementsOnPlanExecutionFinished)
	{
		return;
	}

	FGuardValue_Bitfield(bForceUsingPlanningWorldState, true);
	check(GetOwnerComponent() == &OwnerComp);
	check(UHTNNodeLibrary::GetOwnersWorldState(this) == OwnerComp.GetPlanningWorldStateProxy());
	check(!UHTNNodeLibrary::GetOwnersWorldState(this)->IsBlackboard());
	check(!UHTNNodeLibrary::GetOwnersWorldState(this)->IsEditable());
	ReceiveOnPlanExecutionFinished(
		OwnerComp.GetOwner(), OwnerComp.GetAIOwner(), OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr,
		Result
	);
}
