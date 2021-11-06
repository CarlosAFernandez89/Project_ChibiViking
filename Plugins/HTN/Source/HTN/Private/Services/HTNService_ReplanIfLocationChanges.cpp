// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Services/HTNService_ReplanIfLocationChanges.h"
#include "BehaviorTree/BlackboardComponent.h"

UHTNService_ReplanIfLocationChanges::UHTNService_ReplanIfLocationChanges(const FObjectInitializer& Initializer) : Super(Initializer),
	Tolerance(100.0f),
	bForceAbortPlan(false),
	bForceRestartActivePlanning(false)
{
	NodeName = TEXT("Replan If Location Changes");
	
	bNotifyExecutionStart = true;
	bNotifyTick = true;

	TickInterval = 0.2f;
	TickIntervalRandomDeviation = 0.05f;
}

void UHTNService_ReplanIfLocationChanges::InitializeFromAsset(UHTN& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (Asset.BlackboardAsset)
	{
		BlackboardKey.ResolveSelectedKey(*Asset.BlackboardAsset);
	}
}

uint16 UHTNService_ReplanIfLocationChanges::GetInstanceMemorySize() const
{
	return sizeof(FMemory);
}

void UHTNService_ReplanIfLocationChanges::InitializeMemory(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const
{
	*CastInstanceNodeMemory<FMemory>(NodeMemory) = FMemory();
}

FString UHTNService_ReplanIfLocationChanges::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s\n\nBlackboardKey: %s\nTolerance: %.2f"), 
		*Super::GetStaticDescription(), 
		*BlackboardKey.SelectedKeyName.ToString(), 
		Tolerance
	);
}

void UHTNService_ReplanIfLocationChanges::OnExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory)
{
	SetInitialLocation(OwnerComp, NodeMemory);
}

void UHTNService_ReplanIfLocationChanges::TickNode(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaTime)
{
	FMemory& Memory = *CastInstanceNodeMemory<FMemory>(NodeMemory);
	if (Memory.bTriggeredForceReplan)
	{
		return;
	}
	
	if (!Memory.InitialLocation.IsSet())
	{
		SetInitialLocation(OwnerComp, NodeMemory);
		return;
	}
	
	bool bShouldReplan = false;
	
	FVector CurrentLocation;
	if (GetLocation(OwnerComp, CurrentLocation))
	{
		const float DistSquared = FVector::DistSquared(CurrentLocation, Memory.InitialLocation.GetValue());
		if (DistSquared >= Tolerance * Tolerance)
		{
			bShouldReplan = true;
		}
	}
	else
	{
		bShouldReplan = true;
	}

	if (bShouldReplan)
	{
		Memory.bTriggeredForceReplan = true;
		OwnerComp.ForceReplan(bForceAbortPlan, bForceRestartActivePlanning);
	}
}

bool UHTNService_ReplanIfLocationChanges::GetLocation(UHTNComponent& OwnerComp, FVector& OutLocation) const
{
	if (UBlackboardComponent* const BlackboardComponent = OwnerComp.GetBlackboardComponent())
	{
		if (BlackboardComponent->GetLocationFromEntry(BlackboardKey.GetSelectedKeyID(), OutLocation))
		{
			return true;
		}
	}

	return false;
}

bool UHTNService_ReplanIfLocationChanges::SetInitialLocation(UHTNComponent& OwnerComp, uint8* NodeMemory) const
{
	FVector InitialLocation;
	if (GetLocation(OwnerComp, InitialLocation))
	{
		FMemory& Memory = *CastInstanceNodeMemory<FMemory>(NodeMemory);
		Memory.InitialLocation = InitialLocation;
		return true;
	}

	return false;
}
