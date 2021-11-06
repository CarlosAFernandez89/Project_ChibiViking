// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HTNNode_TwoBranches.h"
#include "HTNNode_If.generated.h"

// The top branch will be taken if all the decorators on this node pass, otherwise the bottom branch.
UCLASS()
class HTN_API UHTNNode_If : public UHTNNode_TwoBranches
{
	GENERATED_BODY()

public:
	UHTNNode_If(const FObjectInitializer& ObjectInitializer);
	virtual void MakePlanExpansions(FHTNPlanningContext& Context) override;
	virtual void GetNextPrimitiveSteps(FHTNGetNextStepsContext& Context, const FHTNPlanStepID& ThisStepID) override;

	virtual FString GetStaticDescription() const override;

	// If unticked, the true branch will not be aborted by this node's decorators during execution and plan recheck.
	UPROPERTY(EditAnywhere, Category = Node)
	uint8 bCanConditionsInterruptTrueBranch : 1;

	// If unticked, the false branch will not be aborted by this node's decorators during execution and plan recheck.
	UPROPERTY(EditAnywhere, Category = Node)
	uint8 bCanConditionsInterruptFalseBranch : 1;
};