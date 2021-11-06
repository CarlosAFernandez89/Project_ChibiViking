// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UHTNComponent;
class UWorldStateProxy;
class AActor;

namespace FHTNHelpers
{
	UHTNComponent* GetHTNComponent(AActor* Target);
	UWorldStateProxy* GetWorldStateProxy(AActor* Target, bool bIsPlanning);
}
