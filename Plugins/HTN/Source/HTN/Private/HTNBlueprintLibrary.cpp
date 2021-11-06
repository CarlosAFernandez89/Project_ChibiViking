// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "HTNBlueprintLibrary.h"
#include "AIController.h"
#include "VisualLogger/VisualLogger.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"

#include "HTN.h"
#include "HTNComponent.h"
#include "Misc/RuntimeErrors.h"
#include "WorldStateProxy.h"
#include "Decorators/HTNDecorator_BlueprintBase.h"

bool UHTNBlueprintLibrary::RunHTN(AAIController* AIController, UHTN* HTNAsset)
{
	struct Local
	{
		static UHTNComponent* EnsureHTNComponent(AAIController* InAIController)
		{
			UHTNComponent* HTNComponent = Cast<UHTNComponent>(InAIController->BrainComponent);
			if (!IsValid(HTNComponent))
			{
				if (IsValid(InAIController->BrainComponent))
				{
					UE_VLOG_UELOG(InAIController, LogHTN, Error, TEXT("RunHTN: the given AIController already has a BrainComponent that is not an HTNComponent"));
					return nullptr;
				}

				UE_VLOG_UELOG(InAIController, LogHTN, Log, TEXT("RunHTN: spawning UHTNComponent.."));
				HTNComponent = NewObject<UHTNComponent>(InAIController, TEXT("HTNComponent"));
				HTNComponent->RegisterComponent();
			}
			
			InAIController->BrainComponent = HTNComponent;
			return HTNComponent;
		}
	};
	
	if (!IsValid(AIController))
	{
		UE_VLOG_UELOG(AIController, LogHTN, Warning, TEXT("RunHTN: unable to run on an invalid AIController"));
		return false;
	}

	if (!IsValid(HTNAsset))
	{
		UE_VLOG_UELOG(AIController, LogHTN, Warning, TEXT("RunHTN: unable to run an invalid HTN asset"));
		return false;
	}

	if (!IsValid(HTNAsset->BlackboardAsset))
	{
		UE_VLOG_UELOG(AIController, LogHTN, Warning, TEXT("RunHTN: unable to run an HTN asset with a null Blackboard"));
		return false;
	}

	if (UHTNComponent* const HTNComponent = Local::EnsureHTNComponent(AIController))
	{
		HTNComponent->StartHTN(HTNAsset);
		return true;
	}

	return false;
}

namespace
{
	UHTNComponent* GetOwnerComponent(const UHTNNode* Node)
	{
		if (Node)
		{
			UHTNComponent* const OwnerComponent = Node->GetOwnerComponent();
			if (ensureAsRuntimeWarning(OwnerComponent))
			{
				return OwnerComponent;
			}

			return Node->GetTypedOuter<UHTNComponent>();
		}

		return nullptr;
	}

	template<typename TDataType>
	FORCEINLINE typename TDataType::FDataType GetWorldStateValue(UHTNNode* Node, const FBlackboardKeySelector& KeySelector)
	{
		if (UWorldStateProxy* const WorldStateProxy = UHTNNodeLibrary::GetOwnersWorldState(Node))
		{
			return WorldStateProxy->GetValue<TDataType>(KeySelector.SelectedKeyName);
		}
		
		return TDataType::InvalidValue;
	}

	template<typename TDataType>
	FORCEINLINE bool SetWorldStateValue(UHTNNode* Node, const FBlackboardKeySelector& KeySelector, typename TDataType::FDataType Value)
	{
		if (UWorldStateProxy* const WorldStateProxy = UHTNNodeLibrary::GetOwnersWorldState(Node))
		{
			return WorldStateProxy->SetValue<TDataType>(KeySelector.SelectedKeyName, Value);
		}

		return false;
	}
}

void UHTNNodeLibrary::ForceReplan(UHTNNode* Node, bool bForceAbortPlan, bool bForceRestartActivePlanning)
{
	if (UHTNComponent* const OwnerComp = GetOwnerComponent(Node))
	{
		OwnerComp->ForceReplan(bForceAbortPlan, bForceRestartActivePlanning);
	}
}

UWorldStateProxy* UHTNNodeLibrary::GetOwnersWorldState(const UHTNNode* Node)
{
	ensureAsRuntimeWarning(Node && (Node->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) || Cast<UDynamicClass>(Node->GetClass())));

	UHTNComponent* const OwnerComp = GetOwnerComponent(Node);
	if (ensureAsRuntimeWarning(OwnerComp))
	{
		const bool bIsPlanning = Node->bForceUsingPlanningWorldState || !Node->IsInstance();
		return OwnerComp->GetWorldStateProxy(bIsPlanning);
	}

	return nullptr;
}

bool UHTNNodeLibrary::GetLocationFromWorldState(UHTNNode* Node, const FBlackboardKeySelector& KeySelector, FVector& OutLocation, AActor*& OutActor)
{
	if (UWorldStateProxy* const WorldStateProxy = GetOwnersWorldState(Node))
	{
		return WorldStateProxy->GetLocation(KeySelector, OutLocation, OutActor);
	}

	OutLocation = FAISystem::InvalidLocation;
	OutActor = nullptr;
	return false;
}

FVector UHTNNodeLibrary::GetSelfLocationFromWorldState(UHTNNode* Node)
{
	if (UWorldStateProxy* const WorldStateProxy = GetOwnersWorldState(Node))
	{
		return WorldStateProxy->GetSelfLocation();
	}

	return FAISystem::InvalidLocation;
}

UObject* UHTNNodeLibrary::GetWorldStateValueAsObject(UHTNNode* Node, const FBlackboardKeySelector& Key)
{
	return GetWorldStateValue<UBlackboardKeyType_Object>(Node, Key);
}

AActor* UHTNNodeLibrary::GetWorldStateValueAsActor(UHTNNode* Node, const FBlackboardKeySelector& Key)
{
	return Cast<AActor>(GetWorldStateValueAsObject(Node, Key));
}

UClass* UHTNNodeLibrary::GetWorldStateValueAsClass(UHTNNode* Node, const FBlackboardKeySelector& Key)
{
	return GetWorldStateValue<UBlackboardKeyType_Class>(Node, Key);
}

uint8 UHTNNodeLibrary::GetWorldStateValueAsEnum(UHTNNode* Node, const FBlackboardKeySelector& Key)
{
	return GetWorldStateValue<UBlackboardKeyType_Enum>(Node, Key);
}

int32 UHTNNodeLibrary::GetWorldStateValueAsInt(UHTNNode* Node, const FBlackboardKeySelector& Key)
{
	return GetWorldStateValue<UBlackboardKeyType_Int>(Node, Key);
}

float UHTNNodeLibrary::GetWorldStateValueAsFloat(UHTNNode* Node, const FBlackboardKeySelector& Key)
{
	return GetWorldStateValue<UBlackboardKeyType_Float>(Node, Key);
}

bool UHTNNodeLibrary::GetWorldStateValueAsBool(UHTNNode* Node, const FBlackboardKeySelector& Key)
{
	return GetWorldStateValue<UBlackboardKeyType_Bool>(Node, Key);
}

FString UHTNNodeLibrary::GetWorldStateValueAsString(UHTNNode* Node, const FBlackboardKeySelector& Key)
{
	return GetWorldStateValue<UBlackboardKeyType_String>(Node, Key);
}

FName UHTNNodeLibrary::GetWorldStateValueAsName(UHTNNode* Node, const FBlackboardKeySelector& Key)
{
	return GetWorldStateValue<UBlackboardKeyType_Name>(Node, Key);
}

FVector UHTNNodeLibrary::GetWorldStateValueAsVector(UHTNNode* Node, const FBlackboardKeySelector& Key)
{
	return GetWorldStateValue<UBlackboardKeyType_Vector>(Node, Key);
}

FRotator UHTNNodeLibrary::GetWorldStateValueAsRotator(UHTNNode* Node, const FBlackboardKeySelector& Key)
{
	return GetWorldStateValue<UBlackboardKeyType_Rotator>(Node, Key);
}

void UHTNNodeLibrary::SetWorldStateValueAsObject(UHTNNode* Node, const FBlackboardKeySelector& Key, UObject* Value)
{
	SetWorldStateValue<UBlackboardKeyType_Object>(Node, Key, Value);
}

void UHTNNodeLibrary::SetWorldStateValueAsClass(UHTNNode* Node, const FBlackboardKeySelector& Key, UClass* Value)
{
	SetWorldStateValue<UBlackboardKeyType_Class>(Node, Key, Value);
}

void UHTNNodeLibrary::SetWorldStateValueAsEnum(UHTNNode* Node, const FBlackboardKeySelector& Key, uint8 Value)
{
	SetWorldStateValue<UBlackboardKeyType_Enum>(Node, Key, Value);
}

void UHTNNodeLibrary::SetWorldStateValueAsInt(UHTNNode* Node, const FBlackboardKeySelector& Key, int32 Value)
{
	SetWorldStateValue<UBlackboardKeyType_Int>(Node, Key, Value);
}

void UHTNNodeLibrary::SetWorldStateValueAsFloat(UHTNNode* Node, const FBlackboardKeySelector& Key, float Value)
{
	SetWorldStateValue<UBlackboardKeyType_Float>(Node, Key, Value);
}

void UHTNNodeLibrary::SetWorldStateValueAsBool(UHTNNode* Node, const FBlackboardKeySelector& Key, bool Value)
{
	SetWorldStateValue<UBlackboardKeyType_Bool>(Node, Key, Value);
}

void UHTNNodeLibrary::SetWorldStateValueAsString(UHTNNode* Node, const FBlackboardKeySelector& Key, FString Value)
{
	SetWorldStateValue<UBlackboardKeyType_String>(Node, Key, Value);
}

void UHTNNodeLibrary::SetWorldStateValueAsName(UHTNNode* Node, const FBlackboardKeySelector& Key, FName Value)
{
	SetWorldStateValue<UBlackboardKeyType_Name>(Node, Key, Value);
}

void UHTNNodeLibrary::SetWorldStateValueAsVector(UHTNNode* Node, const FBlackboardKeySelector& Key, FVector Value)
{
	SetWorldStateValue<UBlackboardKeyType_Vector>(Node, Key, Value);
}

void UHTNNodeLibrary::SetWorldStateValueAsRotator(UHTNNode* Node, const FBlackboardKeySelector& Key, FRotator Value)
{
	SetWorldStateValue<UBlackboardKeyType_Rotator>(Node, Key, Value);
}

void UHTNNodeLibrary::ClearWorldStateValue(UHTNNode* Node, const FBlackboardKeySelector& Key)
{
	if (UWorldStateProxy* const WorldStateProxy = GetOwnersWorldState(Node))
	{
		return WorldStateProxy->ClearValue(Key.SelectedKeyName);
	}
}
