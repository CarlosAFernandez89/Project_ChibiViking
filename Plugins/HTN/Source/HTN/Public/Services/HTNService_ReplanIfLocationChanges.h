// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HTNService.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "HTNService_ReplanIfLocationChanges.generated.h"

// If the location of the specified blackboard key changes too much from what it was at the beginning of execution, forces a replan.
UCLASS()
class HTN_API UHTNService_ReplanIfLocationChanges : public UHTNService
{
	GENERATED_BODY()

public:
	// Changes below this value will be ignored.
	UPROPERTY(EditAnywhere, Category = Node, meta = (UIMin = "0.0"))
	float Tolerance;

	UPROPERTY(EditAnywhere, Category = Node)
	FBlackboardKeySelector BlackboardKey;
	
	UPROPERTY(EditAnywhere, Category = Node)
	bool bForceAbortPlan;

	UPROPERTY(EditAnywhere, Category = Node)
	bool bForceRestartActivePlanning;

	UHTNService_ReplanIfLocationChanges(const FObjectInitializer& Initializer);
	
protected:
	void InitializeFromAsset(UHTN& Asset) override;
	uint16 GetInstanceMemorySize() const override;
	void InitializeMemory(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const override;

	FString GetStaticDescription() const override;
	
	void OnExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory) override;
	void TickNode(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaTime) override;

private:
	bool GetLocation(UHTNComponent& OwnerComp, FVector& OutLocation) const;
	bool SetInitialLocation(UHTNComponent& OwnerComp, uint8* NodeMemory) const;
	
	struct FMemory
	{
		TOptional<FVector> InitialLocation;
		bool bTriggeredForceReplan = false;
	};
};