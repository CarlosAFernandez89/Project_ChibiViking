// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HTNTask.h"
#include "HTNTask_Fail.generated.h"

// A utility task that fails during planning.
UCLASS()
class HTN_API UHTNTask_Fail : public UHTNTask
{
	GENERATED_BODY()

public:
	UHTNTask_Fail(const FObjectInitializer& ObjectInitializer);
	virtual void CreatePlanSteps(UHTNComponent& OwnerComp, UAITask_MakeHTNPlan& PlanningTask, const TSharedRef<const FBlackboardWorldState>& WorldState) const override;
	virtual FString GetStaticDescription() const override;
};
