// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "UObject/ObjectMacros.h"
#include "Misc/TVariant.h"
#include "HTNDecorator.h"
#include "HTNDecorator_FocusScope.generated.h"

// On execution start, optionally sets the focus of the AIController to the value of a blackboard key.
// On execution finish, restores the focus back to its original value.
UCLASS()
class HTN_API UHTNDecorator_FocusScope : public UHTNDecorator
{
	GENERATED_BODY()

public:
	UHTNDecorator_FocusScope(const FObjectInitializer& Initializer);

	virtual FString GetStaticDescription() const override;
	
	virtual void InitializeFromAsset(UHTN& Asset) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void InitializeMemory(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const override;
	
protected:
	virtual void OnExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult Result) override;

	// If true, will set the focus of the AIController to the value of the FocusTarget blackboard key.
	// Upon execution finish, the focus will be restored to the value it had on execution start regardless.
	UPROPERTY(EditAnywhere, Category = Focus)
	uint8 bSetNewFocus : 1;
	
	// The blackboard key on which to focus. Actor and Vector keys are supported.
	UPROPERTY(EditAnywhere, Category = Focus, Meta = (EditCondition = "bSetNewFocus"))
	FBlackboardKeySelector FocusTarget;

	// AIControllers allow multiple focuses to be active at the same time.
	// The Blueprint functions SetFocus and SetFocalPoint use priority 2 (the highest - Gameplay).
	// See EAIFocusPriority.
	UPROPERTY(EditAnywhere, Category = Focus, AdvancedDisplay)
	uint8 FocusPriority;

	struct FNodeMemory
	{
		TVariant<TWeakObjectPtr<AActor>, FVector> OldFocus;
	};
};