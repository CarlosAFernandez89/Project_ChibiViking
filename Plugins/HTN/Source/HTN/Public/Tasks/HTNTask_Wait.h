// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HTNTask.h"
#include "HTNTask_Wait.generated.h"

// During execution, waits for a configurable, randomizable duration and then succeeds.
UCLASS()
class HTN_API UHTNTask_Wait : public UHTNTask
{
	GENERATED_BODY()

public:
	UPROPERTY(Category = Wait, EditAnywhere, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WaitTime;

	UPROPERTY(Category = Wait, EditAnywhere, meta = (UIMin = 0, ClampMin = 0))
	float RandomDeviation;
	
	// The planning cost of this task.
	UPROPERTY(EditAnywhere, Category = Planning, Meta = (ClampMin = "0"))
	int32 Cost;

	UHTNTask_Wait(const FObjectInitializer& Initializer);
	virtual void CreatePlanSteps(UHTNComponent& OwnerComp, UAITask_MakeHTNPlan& PlanningTask, const TSharedRef<const FBlackboardWorldState>& WorldState) const override;
	virtual EHTNNodeResult ExecuteTask(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlanStepID& PlanStepID) override;
	virtual void TickTask(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;
	virtual uint16 GetInstanceMemorySize() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif

	struct FNodeMemory
	{
		float RemainingWaitTime;
	};
};
