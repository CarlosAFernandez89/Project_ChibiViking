// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IHTNModule : public IModuleInterface
{
	static inline IHTNModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IHTNModule>(TEXT("HTN"));
	}
};
