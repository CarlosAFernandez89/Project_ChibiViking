// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "BrainComponent.h"
#include "Delegates/Delegate.h"
#include "GameplayTagContainer.h"
#include "GameplayTaskOwnerInterface.h"
#include "HTN.h"
#include "HTNTypes.h"
#include "HTNComponent.generated.h"

struct FHTNPendingPlanExecutionInfo
{
	TSharedPtr<struct FHTNPlan> NewPlan;

	FORCEINLINE bool IsSet() const { return NewPlan.IsValid(); }
};

struct FHTNPendingHTNStartInfo
{
	TWeakObjectPtr<UHTN> NewAsset;

	FORCEINLINE bool IsSet() const { return NewAsset.IsValid(); }
};

struct FHTNDebugExecutionStep
{
	TSharedPtr<FHTNPlan> HTNPlan;
	
	TArray<FHTNPlanStepID> ActivePlanStepIDs;
	
	// Descriptions of blackboard values at this step
	TMap<FName, FString> BlackboardValues;

	int32 DebugStepIndex = 0;
};

class HTN_API FHTNDebugSteps
{
public:
	FHTNDebugExecutionStep& Add_GetRef();
	void Reset();
	const FHTNDebugExecutionStep* GetByIndex(int32 Index) const;
	FHTNDebugExecutionStep* GetByIndex(int32 Index);
	int32 GetLastIndex() const;
	
private:
	TArray<FHTNDebugExecutionStep> Steps;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHTNPlanExecutionStartedBP, UHTNComponent*, Sender);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHTNPlanExecutionFinishedBP, UHTNComponent*, Sender, EHTNPlanExecutionFinishedResult, Result);

// The HTN counterpart to UBehaviorTreeComponent
UCLASS(ClassGroup = AI, Meta = (BlueprintSpawnableComponent))
class HTN_API UHTNComponent : public UBrainComponent, public IGameplayTaskOwnerInterface
{
	GENERATED_BODY()

public:
	UHTNComponent(const FObjectInitializer& ObjectInitializer);

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void RestartLogic() override;
	virtual void StopLogic(const FString& Reason) override;
	virtual void Cleanup() override;
	virtual void PauseLogic(const FString& Reason) override;
	virtual EAILogicResuming::Type ResumeLogic(const FString& Reason) override;
	virtual bool IsRunning() const override;
	virtual bool IsPaused() const override;

	// Begin IGameplayTaskOwnerInterface
	virtual UGameplayTasksComponent* GetGameplayTasksComponent(const UGameplayTask& Task) const override;
	virtual AActor* GetGameplayTaskOwner(const UGameplayTask* Task) const override;
	virtual AActor* GetGameplayTaskAvatar(const UGameplayTask* Task) const override;
	virtual uint8 GetGameplayTaskDefaultPriority() const override;
	virtual void OnGameplayTaskInitialized(UGameplayTask& Task) override;
	virtual void OnGameplayTaskDeactivated(UGameplayTask& Task) override;
	// End IGameplayTaskOwnerInterface

#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const override;
#endif

	UFUNCTION(BlueprintCallable, Category = "AI|HTN")
	void StartHTN(class UHTN* Asset);

	UFUNCTION(BlueprintCallable, Category = "AI|HTN")
	void StopHTN(bool bDisregardLatentAbort = false);

	UFUNCTION(BlueprintCallable, Category = "AI|HTN")
	void CancelActivePlanning();

	UFUNCTION(BlueprintCallable, Category = "AI|HTN")
	void ForceReplan(bool bForceAbortPlan = false, bool bForceRestartActivePlanning = false, bool bForceDeferToNextFrame = false);

	void OnTaskFinished(const class UHTNTask* Task, EHTNNodeResult Result);

	// Call this from a decorator that checks its condition in an event-based manner when it checks the condition in its event.
	// Takes the raw condition value, as returned from CalculateRawConditionValue.
	// Returns true if this resulted in an replan.
	UFUNCTION(BlueprintCallable, Category = "AI|HTN")
	bool NotifyEventBasedDecoratorCondition(UHTNDecorator* Decorator, bool bRawConditionValue, bool bCanAbortPlanInstantly = true);

	// Equivalent to HasPlan() && HasActiveTasks()
	UFUNCTION(BlueprintPure, Category = "AI|HTN")
	bool HasActivePlan() const;

	UFUNCTION(BlueprintPure, Category = "AI|HTN")
	bool HasPlan() const;

	UFUNCTION(BlueprintPure, Category = "AI|HTN")
	bool HasActiveTasks() const;

	// Are there any task that are currently taking their time to abort? New tasks can't begin execution while some are aborting. 
	// This may be true while bAbortingPlan is not true (but not the other way around) in cases when tasks are aborting but won't cancel the plan 
	// (i.e. when aborting the secondary branch of a Parallel node).
	UFUNCTION(BlueprintPure, Category = "AI|HTN")
	bool IsWaitingForAbortingTasks() const;

	UFUNCTION(BlueprintPure, Category = "AI|HTN")
	bool IsPlanning() const;

	UFUNCTION(BlueprintPure, Category = "AI|HTN")
	FORCEINLINE UHTN* GetCurrentHTN() const { return CurrentHTNAsset; }

	EHTNTaskStatus GetTaskStatus(const class UHTNTask* Task) const;
	EHTNTaskStatus FindStepIDAndMemoryOfTask(const class UHTNTask* Task, FHTNPlanStepID& OutPlanStepID, uint8*& OutTaskMemory);
	uint8* GetNodeMemory(uint16 MemoryOffset) const;
	uint8* GetNodeMemory(const class UHTNNode* Node, const FHTNPlanStepID& StepID) const;

	FORCEINLINE const class UAITask_MakeHTNPlan* GetCurrentPlanningTask() const { return CurrentPlanningTask; }
	FORCEINLINE class UAITask_MakeHTNPlan* GetCurrentPlanningTask() { return CurrentPlanningTask; }
	FORCEINLINE TSharedPtr<const struct FHTNPlan> GetCurrentPlan() const { return CurrentPlan; }
	FORCEINLINE TSharedPtr<struct FHTNPlan> GetCurrentPlan() { return CurrentPlan; }
	FORCEINLINE const TArray<FHTNPlanStepID>& GetPendingExecutingStepIDs() const { return PendingExecutionStepIDs; }
	FORCEINLINE const TArray<FHTNPlanStepID>& GetCurrentlyExecutingStepIDs() const { return CurrentlyExecutingStepIDs; }
	FORCEINLINE const TArray<FHTNPlanStepID>& GetCurrentlyAbortingStepIDs() const { return CurrentlyAbortingStepIDs; }
	
	UFUNCTION(BlueprintPure, Category = "AI|HTN")
	FORCEINLINE class UWorldStateProxy* GetPlanningWorldStateProxy() const { check(PlanningWorldStateProxy); return PlanningWorldStateProxy; }

	UFUNCTION(BlueprintPure, Category = "AI|HTN")
	FORCEINLINE class UWorldStateProxy* GetBlackboardProxy() const { check(BlackboardProxy); return BlackboardProxy; }

	UFUNCTION(BlueprintPure, Category = "AI|HTN")
	FORCEINLINE class UWorldStateProxy* GetWorldStateProxy(bool bForPlanning) const { return bForPlanning ? GetPlanningWorldStateProxy() : GetBlackboardProxy(); }

	// Sets the "current worldstate" in the planning WorldStateProxy. Call this before handing over control to external logic like eqs contexts during planning.
	// When calling GetPlanningWorldStateProxy, they will get a proxy to the given worldstate. If null, the proxy will be pointing to the blackboard.
	void SetPlanningWorldState(TSharedPtr<class FBlackboardWorldState> WorldState, bool bIsEditable = true);

	UFUNCTION(BlueprintCallable, Category = "AI|Logic")
	float GetCooldownEndTime(const UObject* CooldownOwner) const;

	UFUNCTION(BlueprintCallable, Category = "AI|Logic")
	void AddCooldownDuration(const UObject* CooldownOwner, float CooldownDuration, bool bAddToExistingDuration);

	// Assign an HTN asset to SubnetworkDynamic specified by tag.
	// Returns true if the new HTN is different from the old one.
	// If the HTN of any SubnetworkDynamic nodes in the current plan was changed, forces a replan.
	// In that case, bForceAbortCurrentPlanIfChanged determines if the current plan 
	// should be aborted immediately or wait until a new plan is made.
	UFUNCTION(BlueprintCallable, Category = "AI|Logic")
	bool SetDynamicHTN(FGameplayTag InjectTag, UHTN* HTN, bool bForceAbortCurrentPlanIfChanged = false);

	// Returns the dynamic HTN for the given injection tag.
	// Does not return the DefaultHTN of SubNetworkDynamic nodes, only what was set using SetDynamicHTN.
	UFUNCTION(BlueprintCallable, Category = "AI|Logic")
	UHTN* GetDynamicHTN(FGameplayTag InjectTag) const;

	DECLARE_EVENT_OneParam(UHTNComponent, FOnHTNPlanExecutionStarted, UHTNComponent* /*Sender*/);
	FORCEINLINE FOnHTNPlanExecutionStarted& OnPlanExecutionStarted() { return PlanExecutionStartedEvent; }

	DECLARE_EVENT_TwoParams(UHTNComponent, FOnHTNPlanExecutionFinished, UHTNComponent* /*Sender*/, EHTNPlanExecutionFinishedResult /*Result*/);
	FORCEINLINE FOnHTNPlanExecutionFinished& OnPlanExecutionFinished() { return PlanExecutionFinishedEvent; }

protected:
	uint8 LockFlags;
	uint8 bIsPaused : 1;
	uint8 bDeferredAbortPlan : 1;
	uint8 bDeferredStopHTN : 1;
	uint8 bAbortingPlan : 1;
	uint8 bAbortingToStopHTN : 1;
	uint8 bDeferredStartPlanningTask : 1;
	
private:
	void StartPendingHTN();
	bool EnsureCompatibleBlackboardAsset(UBlackboardData* DesiredBlackboardAsset);
	void DeleteAllWorldStates();
	
	void StartPlanningTask(bool bDeferToNextFrame = false);
	void OnPlanningTaskFinished();
	void StartPendingPlanExecution();
	void TickCurrentPlan(float DeltaTime);
	
	void StartTasksPendingExecution();
	EHTNNodeResult StartExecuteTask(const FHTNPlanStepID& PlanStepID);
	bool RecheckCurrentPlan();
	bool TickSubNodesOrRecheck(const FHTNPlanStepID& PlanStepID, float DeltaTime = 0.0f);
	void AbortCurrentPlan(bool bForceDeferToNextFrame = false);
	void AbortExecutingPlanStep(const FHTNPlanStepID& PlanStepID);
	void ClearCurrentPlan();
	void OnPlanAbortFinished();
	void OnPlanExecutionSuccessfullyFinished();

	void StartSubNodesStartingAtPlanStep(const FHTNPlanStepID& PlanStepID);
	void FinishSubNodesAtPlanStep(const FHTNPlanStepID& PlanStepID, EHTNNodeResult Result);
	
	void UpdateBlackboardState() const;
	void NotifyParallelSublevelFinishedIfNeeded(const FHTNPlanStepID& FinishedStepID);
	void AbortSecondaryParallelBranchesIfNeeded(const FHTNPlanStepID& FinishedStepID);

	UHTNTask& GetTaskInCurrentPlan(const FHTNPlanStepID& ExecutingStepID) const;
	UHTNTask& GetTaskInCurrentPlan(const FHTNPlanStepID& ExecutingStepID, uint8*& OutTaskMemory) const;

	void NotifyOnPlanExecutionStarted();
	void NotifyOnPlanExecutionFinished(EHTNPlanExecutionFinishedResult Result);
	void NotifyNodesOnPlanExecutionStarted();
	void NotifyNodesOnPlanExecutionFinished(EHTNPlanExecutionFinishedResult Result);
	void NotifyNodesOnPlanExecutionHelper(TFunctionRef<void(UHTNNode* /*TemplateNode*/, uint16 /*NodeMemoryOffset*/)> Callable);

#if USE_HTN_DEBUGGER
	FHTNDebugExecutionStep& StoreDebugStep(bool bIsEmpty = false) const;
#endif
	
#if ENABLE_VISUAL_LOG
	virtual void VisLogCurrentPlan();
#endif

	UPROPERTY(BlueprintAssignable, Category = "AI|HTN", Meta = (DisplayName = "On Plan Execution Started"))
	FOnHTNPlanExecutionStartedBP PlanExecutionStartedBPEvent;
	FOnHTNPlanExecutionStarted PlanExecutionStartedEvent;

	UPROPERTY(BlueprintAssignable, Category = "AI|HTN", Meta = (DisplayName = "On Plan Execution Finished"))
	FOnHTNPlanExecutionFinishedBP PlanExecutionFinishedBPEvent;
	FOnHTNPlanExecutionFinished PlanExecutionFinishedEvent;

	UPROPERTY()
	class UHTN* CurrentHTNAsset;

	UPROPERTY()
	class UAITask_MakeHTNPlan* CurrentPlanningTask;

	TSharedPtr<struct FHTNPlan> CurrentPlan;
	TArray<FHTNPlanStepID> CurrentlyExecutingStepIDs;
	TArray<FHTNPlanStepID> PendingExecutionStepIDs;
	TArray<FHTNPlanStepID> CurrentlyAbortingStepIDs;

	// Instances of nodes that were created for the current plan.
	UPROPERTY(Transient)
	TArray<class UHTNNode*> InstancedNodes;
	
	// Memory of nodes in the current plan.
	UPROPERTY(Transient)
	TArray<uint8> PlanMemory;

	// The proxy to the "current" worldstate.
	// When planning, proxies to the worldstate currently being processed by the planner.
	// This allows things like EQS Contexts to access future state instead of the current blackboard.
	// UHTNBlueprintLibrary functions (e.g. Get/SetWorldStateValueAsVector) use this proxy during planning.
	UPROPERTY()
	class UWorldStateProxy* PlanningWorldStateProxy;

	// The proxy to the BlackboardComponent of the AIController.
	// UHTNBlueprintLibrary functions(e.g.Get / SetWorldStateValueAsVector) use this proxy during plan execution.
	UPROPERTY()
	class UWorldStateProxy* BlackboardProxy;

	// Maps cooldown owners (usually HTNDecorator_Cooldown nodes to their cooldown values)
	// Deliberately does not retain the objects, so they might become invalid.
	UPROPERTY(Transient, VisibleAnywhere, Category = "AI|HTN")
	TMap<const UObject*, float> CooldownOwnerToEndTimeMap;
	
	// Maps from gameplay tags to HTN assets used by HTNNode_SubnetworkDynamic
	UPROPERTY(Transient, VisibleAnywhere, Category = "AI|HTN")
	TMap<FGameplayTag, UHTN*> GameplayTagToDynamicHTNMap;
	
	FHTNPendingHTNStartInfo PendingHTNStartInfo;
	FHTNPendingPlanExecutionInfo PendingPlanExecutionInfo;

	friend class UHTNNode;
	friend class FHTNDebugger;
	friend struct FHTNComponentScopedLock;

#if USE_HTN_DEBUGGER
	mutable FHTNDebugSteps DebuggerSteps;
	static TArray<TWeakObjectPtr<UHTNComponent>> PlayingComponents;
#endif
};

FORCEINLINE uint8* UHTNComponent::GetNodeMemory(uint16 MemoryOffset) const
{
	// The range intentionally includes PlanMemory.Num() for when the (non-special) memory use of the last node is 0.
	check(MemoryOffset >= 0 && MemoryOffset <= PlanMemory.Num());
	return const_cast<uint8*>(PlanMemory.GetData() + MemoryOffset);
}