// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Tasks/AITask.h"
#include "HTN.h"
#include "HTNPlan.h"
#include "HTNPlanningDebugInfo.h"
#include "HTNStandaloneNode.h"
#include "AITask_MakeHTNPlan.generated.h"

class UHTNComponent;

struct HTN_API FHTNPlanningContext
{
	TWeakObjectPtr<UAITask_MakeHTNPlan> PlanningTask;
	TWeakObjectPtr<UHTNStandaloneNode> AddingNode;
	
	TSharedPtr<FHTNPlan> PlanToExpand;
	FHTNPlanStepID CurrentPlanStepID;
	TSharedPtr<FBlackboardWorldState> WorldStateAfterEnteringDecorators;
	bool bDecoratorsPassed : 1;
	
	FHTNPlanningContext(UAITask_MakeHTNPlan* PlanningTask, UHTNStandaloneNode* AddingNode,
		TSharedPtr<FHTNPlan> PlanToExpand, const FHTNPlanStepID& PlanStepID, 
		TSharedPtr<FBlackboardWorldState> WorldStateAfterEnteringDecorators, bool bDecoratorsPassed
	);

	TSharedRef<FHTNPlan> MakePlanCopyWithAddedStep(FHTNPlanStep*& OutStep, FHTNPlanStepID& OutStepID) const;
	int32 AddLevel(FHTNPlan& NewPlan, UHTN* HTN, const FHTNPlanStepID& ParentStepID = FHTNPlanStepID::None) const;
	int32 AddInlineLevel(FHTNPlan& NewPlan, const FHTNPlanStepID& ParentStepID = FHTNPlanStepID::None) const;
	
	void SubmitCandidatePlan(const TSharedRef<FHTNPlan>& CandidatePlan, const FString& AddedStepDescription = TEXT("")) const;
};

// Can make a plan given a top level htn and a blackboard component.
UCLASS()
class HTN_API UAITask_MakeHTNPlan : public UAITask
{
	GENERATED_BODY()
	
public:
	UAITask_MakeHTNPlan(const FObjectInitializer& ObjectInitializer);
	void SetUp(UHTNComponent* OwnerComponent, UHTN* TopLevelHTN);
	virtual void ExternalCancel() override;

	UHTNComponent* GetOwnerComponent() const;
	bool WasCancelled() const;

	TSharedPtr<const FHTNPlan> GetCurrentPlan() const;
	TSharedPtr<FHTNPlan> GetCurrentPlan();
	FHTNPlanStepID GetExpandingPlanStepID() const;

	bool FoundPlan() const;
	TSharedPtr<struct FHTNPlan> GetFinishedPlan() const;
	void Clear();

	// To be used by tasks when planning
	void SubmitPlanStep(const class UHTNTask* Task, TSharedPtr<class FBlackboardWorldState> WorldState, int32 Cost, const FString& Description = TEXT(""));
	void WaitForLatentCreatePlanSteps(const class UHTNTask* Task);
	void FinishLatentCreatePlanSteps(const class UHTNTask* Task);
	int32 MakePriorityMarker();
	void SetNodePlanningFailureReason(const FString& FailureReason);

protected:
	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;
	
private:
	void DoPlanning();
	TSharedPtr<FHTNPlan> DequeueCurrentBestPlan();
	void MakeExpansionsOfCurrentPlan();
	void MakeExpansionsOfCurrentPlan(const TSharedPtr<class FBlackboardWorldState>& WorldState, UHTNStandaloneNode* NextNode);
	void SubmitCandidatePlan(const TSharedRef<FHTNPlan>& NewPlan, UHTNStandaloneNode* AddedNode, const FString& AddedStepDescription = TEXT(""));

	void OnTaskFinishedProducingCandidateSteps(class UHTNTask* Task);

	bool EnterDecorators(const FHTNPlan& Plan, const FHTNPlanStepID& StepID, const FBlackboardWorldState& WorldState, UHTNStandaloneNode* Node, TSharedPtr<FBlackboardWorldState>& OutNewWorldState) const;
	bool EnterDecorators(const TArrayView<UHTNDecorator*>& Decorators, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const;
	bool ExitDecoratorsAndPropagateWorldState(FHTNPlan& Plan, const FHTNPlanStepID& StepID) const;
	bool ExitDecorators(const TArrayView<UHTNDecorator*>& Decorators, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const;
	void ModifyStepCost(struct FHTNPlanStep& Step, const TArray<UHTNDecorator*>& Decorators) const;

	void ClearIntermediateState();

	int32 GetNumCandidatePlans() const;
	void AddBlockingPriorityMarkersOf(const FHTNPlan& Plan);
	void RemoveBlockingPriorityMarkersOf(const FHTNPlan& Plan);
	bool IsBlockedByPriorityMarkers(const FHTNPlan& Plan) const;
	void AddUnblockedPlansToFrontier();

	UPROPERTY(Transient)
	UHTNComponent* OwnerComponent;

	UPROPERTY(Transient)
	UHTN* TopLevelHTN;
	
	UPROPERTY(Transient)
	UBlackboardComponent* BlackboardComponent;

	TArray<TSharedPtr<FHTNPlan>> Frontier;

	// Contains plans that are currently blocked from consideration by higher-priority plans regardless of cost
	// (e.g. the bottom branch plans of an HTNNode_Prefer).
	TArray<TSharedPtr<FHTNPlan>> BlockedPlans;

	// PriorityMarkerCounts[PriorityMarker] tells how many plans with a specific priority marker are in the priority queue.
	// This is used to determine which plans are to be kept in the BlockedPlans array.
	TMap<FHTNPriorityMarker, int32> PriorityMarkerCounts;

	int32 NextPriorityMarker;

	TSharedPtr<FHTNPlan> CurrentPlanToExpand;
	FHTNPlanStepID CurrentPlanStepID;
	int32 NextNodesIndex;
	TSharedPtr<FBlackboardWorldState> WorldStateAfterEnteredDecorators;
	UPROPERTY()
	class UHTNTask* CurrentTask;
	// The buffer for candidate plan steps (and their descriptions) that are provided by the currently planning task. 
	TArray<TPair<FHTNPlanStep, FString>> PossibleStepsBuffer;
	
	TSharedPtr<FHTNPlan> FinishedPlan;

	UPROPERTY(Transient)
	uint8 bIsWaitingForTaskToProducePlanSteps : 1;

	uint8 bWasCancelled : 1;

#if HTN_DEBUG_PLANNING
	FHTNPlanningDebugInfo DebugInfo;
	mutable FString NodePlanningFailureReason;
#endif

	friend FHTNPlanningContext;
};

FORCEINLINE UHTNComponent* UAITask_MakeHTNPlan::GetOwnerComponent() const { return OwnerComponent; }
FORCEINLINE bool UAITask_MakeHTNPlan::WasCancelled() const { return bWasCancelled; }

FORCEINLINE TSharedPtr<const FHTNPlan> UAITask_MakeHTNPlan::GetCurrentPlan() const { return CurrentPlanToExpand; }
FORCEINLINE TSharedPtr<FHTNPlan> UAITask_MakeHTNPlan::GetCurrentPlan() { return CurrentPlanToExpand; }
FORCEINLINE FHTNPlanStepID UAITask_MakeHTNPlan::GetExpandingPlanStepID() const { return CurrentPlanStepID; }

FORCEINLINE bool UAITask_MakeHTNPlan::FoundPlan() const { return FinishedPlan.IsValid(); }
FORCEINLINE TSharedPtr<struct FHTNPlan> UAITask_MakeHTNPlan::GetFinishedPlan() const { return FinishedPlan; }

FORCEINLINE int32 UAITask_MakeHTNPlan::MakePriorityMarker() { return NextPriorityMarker++; }

#if HTN_DEBUG_PLANNING
FORCEINLINE void UAITask_MakeHTNPlan::SetNodePlanningFailureReason(const FString& FailureReason) { NodePlanningFailureReason = FailureReason; }
#else
FORCEINLINE void UAITask_MakeHTNPlan::SetNodePlanningFailureReason(const FString& FailureReason) {}
#endif

FORCEINLINE int32 UAITask_MakeHTNPlan::GetNumCandidatePlans() const { return Frontier.Num() + BlockedPlans.Num(); }
