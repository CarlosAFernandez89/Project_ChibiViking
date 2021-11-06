// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "HTNDecorator.h"
#include "Misc/RuntimeErrors.h"
#include "VisualLogger/VisualLogger.h"
#include "WorldStateProxy.h"

UHTNDecorator::UHTNDecorator(const FObjectInitializer& Initializer) : Super(Initializer),
	bNotifyOnEnterPlan(false),
	bModifyStepCost(false),
	bNotifyOnExitPlan(false),
	bNotifyExecutionStart(false),
	bNotifyTick(false),
	bNotifyExecutionFinish(false),
	bInverseCondition(false),
	bCheckConditionOnPlanEnter(true),
	bCheckConditionOnPlanExit(false),
	bCheckConditionOnPlanRecheck(true),
	bCheckConditionOnTick(true)
{}

FString UHTNDecorator::GetStaticDescription() const
{
	TArray<FString, TInlineAllocator<4>> CheckDescriptions;
	if (bCheckConditionOnPlanEnter)
	{
		CheckDescriptions.Add(TEXT("plan enter"));
	}
	
	if (bCheckConditionOnPlanExit)
	{
		CheckDescriptions.Add(TEXT("plan exit"));
	}
	
	if (bCheckConditionOnPlanRecheck)
	{
		CheckDescriptions.Add(TEXT("plan recheck"));
	}

	if (bCheckConditionOnTick)
	{
		CheckDescriptions.Add(TEXT("tick"));
	}

	const FString InversedDesc = IsInversed() ? TEXT("(inversed)\n") : TEXT("");
	const FString ChecksDesc = CheckDescriptions.Num() ? 
		FString::Printf(TEXT("(checks on: %s)\n"), *FString::Join(CheckDescriptions, TEXT(", "))) :
		TEXT("");

	return FString::Printf(TEXT("%s%s%s"), *InversedDesc, *ChecksDesc, *Super::GetStaticDescription());
}

bool UHTNDecorator::WrappedEnterPlan(UHTNComponent& OwnerComp, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const
{
	check(OwnerComp.GetPlanningWorldStateProxy()->IsWorldState());
	check(OwnerComp.GetPlanningWorldStateProxy()->IsEditable());
	
	const EHTNDecoratorTestResult Result = TestCondition(OwnerComp, nullptr, EHTNDecoratorConditionCheckType::PlanEnter);
	const bool bPassedCondition = Result != EHTNDecoratorTestResult::Failed;
	if (bPassedCondition && bNotifyOnEnterPlan)
	{
		OnEnterPlan(OwnerComp, Plan, StepID);
	}
	
	return bPassedCondition;
}

void UHTNDecorator::WrappedModifyStepCost(UHTNComponent& OwnerComp, FHTNPlanStep& Step) const
{
	check(OwnerComp.GetPlanningWorldStateProxy()->IsWorldState());
	
	if (bModifyStepCost)
	{
		const int32 CostBefore = Step.Cost;
		ModifyStepCost(OwnerComp, Step);
		const int32 CostAfter = Step.Cost;

		if (CostAfter < 0)
		{
			UE_VLOG_UELOG(OwnerComp.GetOwner(), LogHTN, Error, TEXT("UHTNDecorator: Plan step cost after ModifyStepCost was negative, which is not allowed. Resetting step cost to 0. When modifying node %s by decorator %s"), *Step.Node->GetNodeName(), *GetNodeName());
			Step.Cost = 0;
		}
	}
}

bool UHTNDecorator::WrappedExitPlan(UHTNComponent& OwnerComp, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const
{
	check(OwnerComp.GetPlanningWorldStateProxy()->IsWorldState());
	check(OwnerComp.GetPlanningWorldStateProxy()->IsEditable());
	
	const EHTNDecoratorTestResult Result = TestCondition(OwnerComp, nullptr, EHTNDecoratorConditionCheckType::PlanExit);
	const bool bPassedCondition = Result != EHTNDecoratorTestResult::Failed;
	if (bPassedCondition && bNotifyOnExitPlan)
	{
		OnExitPlan(OwnerComp, Plan, StepID);
	}
	
	return bPassedCondition;
}

bool UHTNDecorator::WrappedRecheckPlan(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlanStep& SubmittedPlanStep) const
{
	check(!IsInstance());
	const EHTNDecoratorTestResult Result = WrappedTestCondition(OwnerComp, NodeMemory, EHTNDecoratorConditionCheckType::PlanRecheck);
	const bool bPassedCondition = Result != EHTNDecoratorTestResult::Failed;
	return bPassedCondition;
}

void UHTNDecorator::WrappedExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory) const
{
	check(!IsInstance());
	UHTNDecorator* const Decorator = StaticCast<UHTNDecorator*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (!ensure(Decorator))
	{
		return;
	}

	if (Decorator->bNotifyExecutionStart)
	{
		Decorator->OnExecutionStart(OwnerComp, NodeMemory);
	}
}

void UHTNDecorator::WrappedTickNode(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaTime) const
{
	check(!IsInstance());
	UHTNDecorator* const Decorator = StaticCast<UHTNDecorator*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (!ensure(Decorator))
	{
		return;
	}
	
	if (Decorator->bNotifyTick)
	{
		Decorator->TickNode(OwnerComp, NodeMemory, DeltaTime);
	}
}

void UHTNDecorator::WrappedExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult NodeResult) const
{
	check(!IsInstance());
	UHTNDecorator* const Decorator = StaticCast<UHTNDecorator*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (!ensure(Decorator))
	{
		return;
	}
	
	if (bNotifyExecutionFinish)
	{
		Decorator->OnExecutionFinish(OwnerComp, NodeMemory, NodeResult);
	}
}

EHTNDecoratorTestResult UHTNDecorator::WrappedTestCondition(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNDecoratorConditionCheckType CheckType) const
{
	check(!IsInstance());
	UHTNDecorator* const Decorator = StaticCast<UHTNDecorator*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (!ensure(Decorator))
	{
		return EHTNDecoratorTestResult::Failed;
	}

	return Decorator->TestCondition(OwnerComp, NodeMemory, CheckType);
}

EHTNDecoratorTestResult UHTNDecorator::TestCondition(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNDecoratorConditionCheckType CheckType) const
{
	if (!ShouldCheckCondition(OwnerComp, NodeMemory, CheckType))
	{
		return EHTNDecoratorTestResult::NotTested;
	}
	
	const bool bRawValue = CalculateRawConditionValue(OwnerComp, NodeMemory, CheckType);
	const bool bEffectiveValue = bInverseCondition ? !bRawValue : bRawValue;
	return bEffectiveValue ? EHTNDecoratorTestResult::Passed : EHTNDecoratorTestResult::Failed;
}

bool UHTNDecorator::ShouldCheckCondition(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNDecoratorConditionCheckType CheckType) const
{
	switch (CheckType)
	{
		case EHTNDecoratorConditionCheckType::PlanEnter: return bCheckConditionOnPlanEnter;
		case EHTNDecoratorConditionCheckType::PlanExit: return bCheckConditionOnPlanExit;
		case EHTNDecoratorConditionCheckType::PlanRecheck: return bCheckConditionOnPlanRecheck;
		case EHTNDecoratorConditionCheckType::Execution: return bCheckConditionOnTick;
		default: return false;
	}
}