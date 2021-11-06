// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "HTNStandaloneNode.h"
#include "HTNNode_SubNetworkDynamic.generated.h"

// Like SubNetwork, but the HTN can be changed dynamically for each AI
// using the SetDynamicHTN function of HTNComponent.
UCLASS()
class HTN_API UHTNNode_SubNetworkDynamic : public UHTNStandaloneNode
{
	GENERATED_BODY()

public:
	virtual FString GetStaticDescription() const override;
	virtual void MakePlanExpansions(FHTNPlanningContext& Context) override;
	virtual void GetNextPrimitiveSteps(FHTNGetNextStepsContext& Context, const FHTNPlanStepID& ThisStepID) override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif

	UHTN* GetHTN(UHTNComponent& OwnerComp) const;
	
	// The default subnetwork of this node. 
	// If there is no HTN listed under the InjectTag in the HTNComponent, this HTN will be used.
	// If None, planning will move on as if this node instantly succeeded.
	UPROPERTY(Category = Node, EditAnywhere)
	UHTN* DefaultHTN;

	// The tag by which the node will find the HTN to use.
	// Call SetDynamicHTN on the HTNComponent to set the HTN for nodes with a specific tag. 
	UPROPERTY(Category = Node, EditAnywhere)
	FGameplayTag InjectTag;
};
