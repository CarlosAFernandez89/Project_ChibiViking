// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Decorators/HTNDecorator_DistanceCheck.h"
#include "WorldStateProxy.h"

UHTNDecorator_DistanceCheck::UHTNDecorator_DistanceCheck(const FObjectInitializer& Initializer) : Super(Initializer),
	MinDistance(0.f),
	MaxDistance(1000.f)
{
	NodeName = TEXT("Distance Check");

	A.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UHTNDecorator_DistanceCheck, A), AActor::StaticClass());
	A.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UHTNDecorator_DistanceCheck, A));

	B.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UHTNDecorator_DistanceCheck, B), AActor::StaticClass());
	B.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UHTNDecorator_DistanceCheck, B));
}

void UHTNDecorator_DistanceCheck::InitializeFromAsset(UHTN& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* const BBAsset = GetBlackboardAsset())
	{
		A.ResolveSelectedKey(*BBAsset);
		B.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		UE_LOG(LogHTN, Warning, TEXT("Can't initialize %s due to missing blackboard data."), *GetNodeName());
		A.InvalidateResolvedKey();
		B.InvalidateResolvedKey();
	}
}

FString UHTNDecorator_DistanceCheck::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: between\n%s and %s\n%s %.2f-%.2f"), *Super::GetStaticDescription(),
		*A.SelectedKeyName.ToString(), *B.SelectedKeyName.ToString(),
		IsInversed() ? TEXT("is not betweeen") : TEXT("is between"),
		MinDistance, MaxDistance
	);
}

bool UHTNDecorator_DistanceCheck::CalculateRawConditionValue(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNDecoratorConditionCheckType CheckType) const
{
	UWorldStateProxy* const WorldStateProxy = GetWorldStateProxy(OwnerComp, CheckType);
	if (!ensure(WorldStateProxy))
	{
		return false;
	}
	
	const FVector LocationA = WorldStateProxy->GetLocation(A);
	if (!FAISystem::IsValidLocation(LocationA))
	{
		return false;
	}

	const FVector LocationB = WorldStateProxy->GetLocation(B);
	if (!FAISystem::IsValidLocation(LocationB))
	{
		return false;
	}

	const float DistanceSquared = FVector::DistSquared(LocationA, LocationB);
	return MinDistance * MinDistance <= DistanceSquared && DistanceSquared <= MaxDistance * MaxDistance;
}
