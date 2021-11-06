// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "AIController.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "Tasks/HTNTask_BlueprintBase.h"
#include "HTNBlueprintLibrary.generated.h"

UCLASS()
class HTN_API UHTNBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "AI|HTN", Meta=(DefaultToSelf="AIController"))
	static bool RunHTN(AAIController* AIController, class UHTN* HTNAsset);
};

UCLASS(Meta = (RestrictedToClasses = "HTNNode"))
class HTN_API UHTNNodeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	// Forces the HTN component running this node to start making a new plan.
	UFUNCTION(BlueprintCallable, Category = "AI|HTN", Meta = (HidePin = "Node", DefaultToSelf = "Node"))
	static void ForceReplan(UHTNNode* Node, bool bForceAbortPlan = false, bool bForceRestartActivePlanning = false);
	
	// Gets the worldstate of the owner of this HTN node.
	// WARNING: for Blueprint nodes only!
	UFUNCTION(BlueprintPure, Category = "AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static class UWorldStateProxy* GetOwnersWorldState(const UHTNNode* Node);

	// If the given key contains a Vector, returns it.
	// If the given key contains an Actor, returns its location and the actor itself.
    // Otherwise returns false.
	UFUNCTION(BlueprintCallable, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static bool GetLocationFromWorldState(UHTNNode* Node, const FBlackboardKeySelector& KeySelector, FVector& OutLocation, AActor*& OutActor);

	// A helper returning the value of SelfLocation key in the worldstate.
	// This key is automatically added to every blackboard asset used for HTN planning and is automatically updated.
	UFUNCTION(BlueprintPure, Category = "AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static FVector GetSelfLocationFromWorldState(UHTNNode* Node);
	
	UFUNCTION(BlueprintPure, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static UObject* GetWorldStateValueAsObject(UHTNNode* Node, const FBlackboardKeySelector& Key);

	UFUNCTION(BlueprintPure, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static AActor* GetWorldStateValueAsActor(UHTNNode* Node, const FBlackboardKeySelector& Key);

	UFUNCTION(BlueprintPure, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static UClass* GetWorldStateValueAsClass(UHTNNode* Node, const FBlackboardKeySelector& Key);

	UFUNCTION(BlueprintPure, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static uint8 GetWorldStateValueAsEnum(UHTNNode* Node, const FBlackboardKeySelector& Key);

	UFUNCTION(BlueprintPure, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static int32 GetWorldStateValueAsInt(UHTNNode* Node, const FBlackboardKeySelector& Key);

	UFUNCTION(BlueprintPure, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static float GetWorldStateValueAsFloat(UHTNNode* Node, const FBlackboardKeySelector& Key);

	UFUNCTION(BlueprintPure, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static bool GetWorldStateValueAsBool(UHTNNode* Node, const FBlackboardKeySelector& Key);

	UFUNCTION(BlueprintPure, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static FString GetWorldStateValueAsString(UHTNNode* Node, const FBlackboardKeySelector& Key);

	UFUNCTION(BlueprintPure, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static FName GetWorldStateValueAsName(UHTNNode* Node, const FBlackboardKeySelector& Key);

	UFUNCTION(BlueprintPure, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static FVector GetWorldStateValueAsVector(UHTNNode* Node, const FBlackboardKeySelector& Key);

	UFUNCTION(BlueprintPure, Category ="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static FRotator GetWorldStateValueAsRotator(UHTNNode* Node, const FBlackboardKeySelector& Key);

	UFUNCTION(BlueprintCallable, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static void SetWorldStateValueAsObject(UHTNNode* Node, const FBlackboardKeySelector& Key, UObject* Value);

	UFUNCTION(BlueprintCallable, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static void SetWorldStateValueAsClass(UHTNNode* Node, const FBlackboardKeySelector& Key, UClass* Value);

	UFUNCTION(BlueprintCallable, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static void SetWorldStateValueAsEnum(UHTNNode* Node, const FBlackboardKeySelector& Key, uint8 Value);

	UFUNCTION(BlueprintCallable, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static void SetWorldStateValueAsInt(UHTNNode* Node, const FBlackboardKeySelector& Key, int32 Value);

	UFUNCTION(BlueprintCallable, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static void SetWorldStateValueAsFloat(UHTNNode* Node, const FBlackboardKeySelector& Key, float Value);

	UFUNCTION(BlueprintCallable, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static void SetWorldStateValueAsBool(UHTNNode* Node, const FBlackboardKeySelector& Key, bool Value);

	UFUNCTION(BlueprintCallable, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static void SetWorldStateValueAsString(UHTNNode* Node, const FBlackboardKeySelector& Key, FString Value);

	UFUNCTION(BlueprintCallable, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static void SetWorldStateValueAsName(UHTNNode* Node, const FBlackboardKeySelector& Key, FName Value);

	UFUNCTION(BlueprintCallable, Category="AI|HTN", Meta=(HidePin="Node", DefaultToSelf="Node"))
	static void SetWorldStateValueAsVector(UHTNNode* Node, const FBlackboardKeySelector& Key, FVector Value);

	UFUNCTION(BlueprintCallable, Category="AI|HTN", Meta = (HidePin = "Node", DefaultToSelf = "Node"))
	static void SetWorldStateValueAsRotator(UHTNNode* Node, const FBlackboardKeySelector& Key, FRotator Value);

	// Resets indicated value to "not set" value, based on the key type
	UFUNCTION(BlueprintCallable, Category="AI|HTN", Meta = (HidePin = "Node", DefaultToSelf = "Node"))
	static void ClearWorldStateValue(UHTNNode* Node, const FBlackboardKeySelector& Key);
};
