// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "HTNTypes.h"
#include "HTNDecorator.h"
#include "HTNDecorator_Cooldown.generated.h"

// Cooldown decorator node.
// A decorator node that bases its condition on whether a cooldown timer has expired.
UCLASS()
class HTN_API UHTNDecorator_Cooldown : public UHTNDecorator
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Decorator")
	float CooldownDuration;
	
	UHTNDecorator_Cooldown(const FObjectInitializer& Initializer);

	virtual FString GetStaticDescription() const override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void InitializeMemory(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const override;
	
protected:
	virtual bool CalculateRawConditionValue(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNDecoratorConditionCheckType CheckType) const override;
	virtual void OnExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult Result) override;

	struct FNodeMemory
	{
		bool bIsIfNodeFalseBranch = false;
	};
};
