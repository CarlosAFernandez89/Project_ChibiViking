// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Tasks/HTNTask_Wait.h"

UHTNTask_Wait::UHTNTask_Wait(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
	WaitTime(5.0f),
	RandomDeviation(0.0f),
	Cost(100)
{
	NodeName = "Wait";
	bNotifyTick = true;
}

void UHTNTask_Wait::CreatePlanSteps(UHTNComponent& OwnerComp, UAITask_MakeHTNPlan& PlanningTask, const TSharedRef<const FBlackboardWorldState>& WorldState) const
{
	PlanningTask.SubmitPlanStep(this, WorldState->MakeNext(), FMath::Max(0, Cost));
}

EHTNNodeResult UHTNTask_Wait::ExecuteTask(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlanStepID& PlanStepID)
{
	FNodeMemory* const Memory = CastInstanceNodeMemory<FNodeMemory>(NodeMemory);
	Memory->RemainingWaitTime = FMath::FRandRange(FMath::Max(0.0f, WaitTime - RandomDeviation), WaitTime + RandomDeviation);
	
	return EHTNNodeResult::InProgress;
}

void UHTNTask_Wait::TickTask(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FNodeMemory* const Memory = CastInstanceNodeMemory<FNodeMemory>(NodeMemory);
	
	Memory->RemainingWaitTime -= DeltaSeconds;
	if (Memory->RemainingWaitTime <= 0.0f)
	{
		FinishLatentTask(OwnerComp, EHTNNodeResult::Succeeded);
	}
}

FString UHTNTask_Wait::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s:%s%s"), *Super::GetStaticDescription(),
		FMath::IsNearlyZero(RandomDeviation) ? *FString::Printf(TEXT(" %.1fs"), WaitTime) : *FString::Printf(TEXT(" %.1f+-%.1fs"), WaitTime, RandomDeviation),
		Cost == 0 ? TEXT("") : *FString::Printf(TEXT("\nCost: %i"), Cost)
	);
}

uint16 UHTNTask_Wait::GetInstanceMemorySize() const
{
	return sizeof(FNodeMemory);
}

#if WITH_EDITOR
FName UHTNTask_Wait::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.Wait.Icon");
}
#endif
