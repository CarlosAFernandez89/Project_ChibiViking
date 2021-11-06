// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Utility/HTNHelpers.h"

#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "AIController.h"
#include "HTNComponent.h"

UHTNComponent* FHTNHelpers::GetHTNComponent(AActor* Target)
{
	if (AAIController* const Controller = UAIBlueprintHelperLibrary::GetAIController(Target))
	{
		if (UHTNComponent* const HTNComponent = Controller->FindComponentByClass<UHTNComponent>())
		{
			return HTNComponent;
		}
	}

	if (UHTNComponent* const HTNComponent = Target->FindComponentByClass<UHTNComponent>())
	{
		return HTNComponent;
	}

	return nullptr;
}

UWorldStateProxy* FHTNHelpers::GetWorldStateProxy(AActor* Target, bool bIsPlanning)
{
	if (UHTNComponent* const HTNComponent = GetHTNComponent(Target))
	{
		return bIsPlanning ? HTNComponent->GetPlanningWorldStateProxy() : HTNComponent->GetBlackboardProxy();
	}

	return nullptr;
}