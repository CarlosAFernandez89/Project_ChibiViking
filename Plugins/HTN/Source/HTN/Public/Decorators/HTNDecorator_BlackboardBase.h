// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "HTNDecorator.h"
#include "HTNDecorator_BlackboardBase.generated.h"

// A base class for decorators parametrized with a single blackboard key.
UCLASS(Abstract)
class HTN_API UHTNDecorator_BlackboardBase : public UHTNDecorator
{
	GENERATED_BODY()

public:
	UHTNDecorator_BlackboardBase(const FObjectInitializer& Initializer);
	virtual void InitializeFromAsset(UHTN& Asset) override;
	FORCEINLINE FName GetSelectedBlackboardKey() const { return BlackboardKey.SelectedKeyName; }

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif
	
protected:
	virtual void OnExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory) override; 
	virtual void OnExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult Result) override;
	virtual EBlackboardNotificationResult OnBlackboardKeyValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID);
	
	// Blackboard key selector
	UPROPERTY(EditAnywhere, Category = Blackboard)
	FBlackboardKeySelector BlackboardKey;
};