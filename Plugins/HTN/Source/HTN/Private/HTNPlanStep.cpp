// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "HTNPlanStep.h"

int32 FHTNPlanStep::GetFirstSubLevelIndex() const
{
	return SubLevelIndex != INDEX_NONE ? SubLevelIndex : SecondarySubLevelIndex;
}

int32 FHTNPlanStep::GetLastSubLevelIndex() const
{
	return SecondarySubLevelIndex != INDEX_NONE ? SecondarySubLevelIndex : SubLevelIndex;
}
