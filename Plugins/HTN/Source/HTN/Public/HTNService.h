// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HTNNode.h"
#include "HTNService.generated.h"

struct FHTNServiceSpecialMemory : public FHTNNodeSpecialMemory
{
	FIntervalCountdown TickCountdown;
};

// A task subnode used for updating values and generally running code per tick
UCLASS(Abstract)
class HTN_API UHTNService : public UHTNNode
{
	GENERATED_BODY()

public:
	UHTNService(const FObjectInitializer& Initializer);
	virtual FString GetStaticDescription() const override;
	virtual uint16 GetSpecialMemorySize() const override;
	virtual void InitializeMemory(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const override;

	void WrappedExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory) const;
	void WrappedTickNode(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) const;
	void WrappedExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult NodeResult) const;

protected:
	virtual FString GetStaticServiceDescription() const;
	FString GetStaticTickIntervalDescription() const;
	
	virtual void OnExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory) {}
	virtual void TickNode(UHTNComponent& OwnerComp, uint8* NodeMemory, float DeltaTime) {}
	virtual void OnExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult Result) {}

	float GetInterval() const;
	
	UPROPERTY(EditAnywhere, Category = Service, Meta = (ClampMin = "0.001"))
	float TickInterval;
	
	UPROPERTY(EditAnywhere, Category = Service, Meta = (ClampMin = "0.0"))
	float TickIntervalRandomDeviation;
	
	bool bNotifyExecutionStart : 1;
	bool bNotifyTick : 1;
	bool bNotifyExecutionFinish : 1;
};