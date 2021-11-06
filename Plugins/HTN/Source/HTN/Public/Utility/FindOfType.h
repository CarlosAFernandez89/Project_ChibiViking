// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Utility
{
	template<typename ItemType, typename RangeType>
	ItemType* FindOfType(RangeType&& Range)
	{
		for (auto&& Elem : Forward<RangeType>(Range))
		{
			if (ItemType* const Item = Cast<ItemType>(Elem))
			{
				return Item;
			}
		}

		return nullptr;
	}
}