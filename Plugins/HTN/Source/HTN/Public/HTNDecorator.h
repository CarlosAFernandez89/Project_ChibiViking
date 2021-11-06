// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HTNNode.h"
#include "HTNPlanStep.h"
#include "HTNDecorator.generated.h"

UENUM(BlueprintType)
enum class EHTNDecoratorConditionCheckType : uint8
{
	// Planner is entering this decorator
	PlanEnter,
	// Planner is exiting this decorator
	PlanExit,
	// Plan is being rechecked during execution
	PlanRecheck,
	// Plan is being executed
	Execution
};

UENUM(BlueprintType)
enum class EHTNDecoratorTestResult : uint8
{
	Failed = 0,
	Passed = 1,
	NotTested = 2
};

// A task subnode used for conditions, plan cost modification, scoping etc.
UCLASS(Abstract)
class HTN_API UHTNDecorator : public UHTNNode
{
	GENERATED_BODY()

public:
	UHTNDecorator(const FObjectInitializer& Initializer);
	virtual FString GetStaticDescription() const override;

	bool WrappedEnterPlan(UHTNComponent& OwnerComp, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const;
	void WrappedModifyStepCost(UHTNComponent& OwnerComp, FHTNPlanStep& Step) const;
	bool WrappedExitPlan(UHTNComponent& OwnerComp, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const;
	
	bool WrappedRecheckPlan(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlanStep& SubmittedPlanStep) const;
	void WrappedExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory) const;
	void WrappedTickNode(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaTime) const;
	void WrappedExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult NodeResult) const;

	EHTNDecoratorTestResult WrappedTestCondition(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNDecoratorConditionCheckType CheckType) const;
	EHTNDecoratorTestResult TestCondition(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNDecoratorConditionCheckType CheckType) const;
	virtual bool ShouldCheckCondition(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNDecoratorConditionCheckType CheckType) const;

	UFUNCTION(BlueprintPure, Category = AI)
	FORCEINLINE bool IsInversed() const { return bInverseCondition; }

protected:
	// Note that NodeMemory will be nullptr during plan-time checks, as memory blocks are only allocated once a plan is selected for execution.
	virtual bool CalculateRawConditionValue(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNDecoratorConditionCheckType CheckType) const { return true; }
	virtual void OnEnterPlan(UHTNComponent& OwnerComp, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const {}
	virtual void ModifyStepCost(UHTNComponent& OwnerComp, FHTNPlanStep& Step) const {}
	virtual void OnExitPlan(UHTNComponent& OwnerComp, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const {}
	virtual void OnExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory) {}
	virtual void TickNode(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaTime) {}
	virtual void OnExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult Result) {}

	static UWorldStateProxy* GetWorldStateProxy(UHTNComponent& OwnerComp, EHTNDecoratorConditionCheckType CheckType);
	
	bool bNotifyOnEnterPlan : 1;
	bool bModifyStepCost : 1;
	bool bNotifyOnExitPlan : 1;
	bool bNotifyExecutionStart : 1;
	bool bNotifyTick : 1;
	bool bNotifyExecutionFinish : 1;
	
	// If set, condition check result will be inversed
	UPROPERTY(Category = Condition, EditAnywhere)
	uint8 bInverseCondition : 1;

	UPROPERTY(Category = Condition, EditAnywhere)
	uint8 bCheckConditionOnPlanEnter : 1;

	UPROPERTY(Category = Condition, EditAnywhere)
	uint8 bCheckConditionOnPlanExit : 1;

	UPROPERTY(Category = Condition, EditAnywhere)
	uint8 bCheckConditionOnPlanRecheck : 1;

	UPROPERTY(Category = Condition, EditAnywhere)
	uint8 bCheckConditionOnTick : 1;
};

FORCEINLINE UWorldStateProxy* UHTNDecorator::GetWorldStateProxy(UHTNComponent& OwnerComp, EHTNDecoratorConditionCheckType CheckType)
{
	return OwnerComp.GetWorldStateProxy(/*bForPlanning=*/CheckType != EHTNDecoratorConditionCheckType::Execution);
}