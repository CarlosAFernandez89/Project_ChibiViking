// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Decorators/HTNDecorator_Cooldown.h"
#include "HTNPlan.h"

UHTNDecorator_Cooldown::UHTNDecorator_Cooldown(const FObjectInitializer& Initializer) : Super(Initializer),
	CooldownDuration(5.0f)
{
	bCheckConditionOnPlanEnter = true;
	bCheckConditionOnPlanExit = false;
	bCheckConditionOnPlanRecheck = true;
	bCheckConditionOnTick = true; // TEMP, should have an option to check condition once on execution start.

	bNotifyExecutionFinish = true;
}

FString UHTNDecorator_Cooldown::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: lock for %.1fs after execution"), *Super::GetStaticDescription(), CooldownDuration);
}

uint16 UHTNDecorator_Cooldown::GetInstanceMemorySize() const
{
	return sizeof(FNodeMemory);
}

void UHTNDecorator_Cooldown::InitializeMemory(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const
{
	FNodeMemory* const Memory = CastInstanceNodeMemory<FNodeMemory>(NodeMemory);
	*Memory = {};
	if (const FHTNPlanStep* const Step = Plan.FindStep(StepID))
	{
		Memory->bIsIfNodeFalseBranch = Step->bIsIfNodeFalseBranch;
	}
}

bool UHTNDecorator_Cooldown::CalculateRawConditionValue(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNDecoratorConditionCheckType CheckType) const
{
	const float CooldownEndTime = OwnerComp.GetCooldownEndTime(this);
	const bool bCooldownActive = OwnerComp.GetWorld()->GetTimeSeconds() < CooldownEndTime;
	return !bCooldownActive;
}

void UHTNDecorator_Cooldown::OnExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult Result)
{
	FNodeMemory* const Memory = CastInstanceNodeMemory<FNodeMemory>(NodeMemory);
	// If a Cooldown node is on an If node and the false branch is picked, it should not lock the cooldown upon execution finish.
	if (!Memory->bIsIfNodeFalseBranch)
	{
		OwnerComp.AddCooldownDuration(this, CooldownDuration, /*bAddToExistingDuration=*/false);
	}
}
