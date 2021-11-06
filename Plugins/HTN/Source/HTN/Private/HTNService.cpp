// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "HTNService.h"

UHTNService::UHTNService(const FObjectInitializer& Initializer) : Super(Initializer),
	TickInterval(0.5f),
	TickIntervalRandomDeviation(0.1f),
	bNotifyExecutionStart(false),
	bNotifyTick(false),
	bNotifyExecutionFinish(false)
{}

FString UHTNService::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s:\n%s"), *Super::GetStaticDescription(), *GetStaticServiceDescription());
}

uint16 UHTNService::GetSpecialMemorySize() const
{
	return sizeof(FHTNServiceSpecialMemory);
}

void UHTNService::InitializeMemory(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const
{
	GetSpecialNodeMemory<FHTNServiceSpecialMemory>(NodeMemory)->TickCountdown = FIntervalCountdown(GetInterval());
}

void UHTNService::WrappedExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory) const
{
	UHTNService* const Service = StaticCast<UHTNService*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (!ensure(Service))
	{
		return;
	}
	
	if (bNotifyExecutionStart)
	{
		Service->OnExecutionStart(OwnerComp, NodeMemory);
	}
}

void UHTNService::WrappedTickNode(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) const
{
	UHTNService* const Service = StaticCast<UHTNService*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (!ensure(Service))
	{
		return;
	}
	
	if (Service->bNotifyTick)
	{
		FHTNServiceSpecialMemory* const SpecialMemory = GetSpecialNodeMemory<FHTNServiceSpecialMemory>(NodeMemory);
		FIntervalCountdown& TickCountdown = SpecialMemory->TickCountdown;
		if (TickCountdown.Tick(DeltaSeconds))
		{
			DeltaSeconds = TickCountdown.GetElapsedTimeWithFallback(DeltaSeconds);

			Service->TickNode(OwnerComp, NodeMemory, DeltaSeconds);

			TickCountdown.Interval = GetInterval();
			TickCountdown.Reset();
		}
	}
}

void UHTNService::WrappedExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult NodeResult) const
{
	UHTNService* const Service = StaticCast<UHTNService*>(GetNodeFromMemory(OwnerComp, NodeMemory));
	if (!ensure(Service))
	{
		return;
	}
	
	if (Service->bNotifyExecutionFinish)
	{
		Service->OnExecutionFinish(OwnerComp, NodeMemory, NodeResult);
	}
}

FString UHTNService::GetStaticServiceDescription() const
{
	return GetStaticTickIntervalDescription();
}

FString UHTNService::GetStaticTickIntervalDescription() const
{
	const FString IntervalDesc = (TickIntervalRandomDeviation > 0.0f) ?
		FString::Printf(TEXT("%.2fs..%.2fs"), FMath::Max(0.0f, TickInterval - TickIntervalRandomDeviation), (TickInterval + TickIntervalRandomDeviation)) :
		FString::Printf(TEXT("%.2fs"), TickInterval);

	return FString::Printf(TEXT("tick every %s"), *IntervalDesc);
}

float UHTNService::GetInterval() const
{
	return FMath::FRandRange(
		FMath::Max(0.0f, TickInterval - TickIntervalRandomDeviation),
		TickInterval + TickIntervalRandomDeviation
	);
}
