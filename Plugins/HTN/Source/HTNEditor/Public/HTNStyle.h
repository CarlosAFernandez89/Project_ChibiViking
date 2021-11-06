// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FHTNStyle
{
public:

	static void Initialize();

	static void Shutdown();

	// reloads textures used by slate renderer
	static void ReloadTextures();

	static const ISlateStyle& Get();

	static FName GetStyleSetName();

private:

	static TSharedRef<class FSlateStyleSet> Create();

private:

	static TSharedPtr<class FSlateStyleSet> StyleInstance;
};