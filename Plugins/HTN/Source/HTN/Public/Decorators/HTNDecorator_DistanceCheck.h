// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "HTNDecorator.h"
#include "HTNDecorator_DistanceCheck.generated.h"

// Checks if the distance between two worldstate keys is smaller than a specified distance.
UCLASS()
class HTN_API UHTNDecorator_DistanceCheck : public UHTNDecorator
{
	GENERATED_BODY()

public:
	UHTNDecorator_DistanceCheck(const FObjectInitializer& Initializer);
	virtual void InitializeFromAsset(UHTN& Asset) override;
	virtual FString GetStaticDescription() const override;

	UPROPERTY(EditAnywhere, Category = Node)
	FBlackboardKeySelector A;

	UPROPERTY(EditAnywhere, Category = Node)
	FBlackboardKeySelector B;
	
	UPROPERTY(EditAnywhere, Category = Node)
	float MinDistance;

	UPROPERTY(EditAnywhere, Category = Node)
	float MaxDistance;

protected:
	virtual bool CalculateRawConditionValue(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNDecoratorConditionCheckType CheckType) const override;
};
