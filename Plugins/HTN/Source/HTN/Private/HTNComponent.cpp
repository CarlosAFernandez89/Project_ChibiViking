// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "HTNComponent.h"
#include "Algo/Transform.h"
#include "VisualLogger/VisualLoggerTypes.h"
#include "VisualLogger/VisualLogger.h"
#include "AIController.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "GameplayTasksComponent.h"
#include "Misc/ScopeExit.h"
#include "Misc/RuntimeErrors.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include "AITask_MakeHTNPlan.h"
#include "HTN.h"
#include "HTNPlan.h"
#include "HTNTask.h"
#include "HTNTypes.h"
#include "HTNDecorator.h"
#include "HTNDelegates.h"
#include "HTNService.h"
#include "Nodes/HTNNode_SubNetwork.h"
#include "Nodes/HTNNode_Parallel.h"
#include "Nodes/HTNNode_SubNetworkDynamic.h"
#include "WorldStateProxy.h"

DEFINE_STAT(STAT_AI_HTN_Tick);
DEFINE_STAT(STAT_AI_HTN_Planning);
DEFINE_STAT(STAT_AI_HTN_Execution);
DEFINE_STAT(STAT_AI_HTN_Cleanup);
DEFINE_STAT(STAT_AI_HTN_StopHTN);
DEFINE_STAT(STAT_AI_HTN_NodeInstantiation);
DEFINE_STAT(STAT_AI_HTN_NumProducedPlans);
DEFINE_STAT(STAT_AI_HTN_NumNodeInstances);

#if USE_HTN_DEBUGGER
TArray<TWeakObjectPtr<UHTNComponent>> UHTNComponent::PlayingComponents;
#endif

FHTNDebugExecutionStep& FHTNDebugSteps::Add_GetRef()
{
	if (Steps.Num() >= 100)
	{
		Steps.RemoveAt(0, 1, false);
	}

	const int32 Index = GetLastIndex() + 1;
	FHTNDebugExecutionStep& DebugStep = Steps.AddDefaulted_GetRef();
	DebugStep.DebugStepIndex = Index;
	return DebugStep;
}

void FHTNDebugSteps::Reset()
{
	Steps.Reset();
}

const FHTNDebugExecutionStep* FHTNDebugSteps::GetByIndex(int32 Index) const
{
	if (Steps.Num())
	{
		const int32 FirstIndex = Steps[0].DebugStepIndex;
		const int32 ArrayIndex = Index - FirstIndex;
		if (Steps.IsValidIndex(ArrayIndex))
		{
			return &Steps[ArrayIndex];
		}
	}
	
	return nullptr;
}

FHTNDebugExecutionStep* FHTNDebugSteps::GetByIndex(int32 Index)
{
	if (Steps.Num())
	{
		const int32 FirstIndex = Steps[0].DebugStepIndex;
		const int32 ArrayIndex = Index - FirstIndex;
		if (Steps.IsValidIndex(ArrayIndex))
		{
			return &Steps[ArrayIndex];
		}
	}

	return nullptr;
}

int32 FHTNDebugSteps::GetLastIndex() const
{
	return Steps.Num() ? Steps.Last().DebugStepIndex : INDEX_NONE;
}

struct FHTNComponentScopedLock : FNoncopyable
{
	FHTNComponentScopedLock(UHTNComponent& HTNComponent, uint8 LockFlag) : HTNComponent(HTNComponent), LockFlag(LockFlag)
	{
		HTNComponent.LockFlags |= LockFlag;
	}
	
	~FHTNComponentScopedLock()
	{
		HTNComponent.LockFlags &= ~LockFlag;
	}

	enum : uint8
	{
		LockTick = 1 << 0,
		LockStopHTN = 1 << 1,
		LockAbortPlan = 1 << 2
	};
	
private:
	UHTNComponent& HTNComponent;
	uint8 LockFlag;
};

UHTNComponent::UHTNComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
	LockFlags(0),
	bIsPaused(false),
	bDeferredAbortPlan(false),
	bDeferredStopHTN(false),
	bAbortingPlan(false),
	bAbortingToStopHTN(false),
	bDeferredStartPlanningTask(false),
	CurrentHTNAsset(nullptr),
	CurrentPlanningTask(nullptr)
{
	bAutoActivate = true;
	bWantsInitializeComponent = true;

	PlanningWorldStateProxy = CreateDefaultSubobject<UWorldStateProxy>(TEXT("WorldStateProxy"));
	PlanningWorldStateProxy->Owner = this;

	BlackboardProxy = CreateDefaultSubobject<UWorldStateProxy>(TEXT("BlackboardProxy"));
	BlackboardProxy->Owner = this;
}

void UHTNComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	SCOPE_CYCLE_COUNTER(STAT_AI_Overall);
	SCOPE_CYCLE_COUNTER(STAT_AI_HTN_Tick);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(HTNTick);
	
	UpdateBlackboardState();
	
	if (bIsPaused)
	{
		return;
	}

	if (!bDeferredStopHTN && !bDeferredAbortPlan && !IsWaitingForAbortingTasks())
	{
		if (PendingHTNStartInfo.IsSet())
		{
			StartPendingHTN();
		}
		else if (PendingPlanExecutionInfo.IsSet())
		{
			StartPendingPlanExecution();
		}
	}

	{
		FHTNComponentScopedLock ScopedLock(*this, FHTNComponentScopedLock::LockTick);
		
		if (HasActivePlan())
		{
			if (!CurrentPlanningTask && !RecheckCurrentPlan())
			{
				UE_VLOG(GetOwner(), LogHTN, Error, TEXT("plan recheck failed -> forcing replan."));
				ForceReplan();
			}
		}

		if (bDeferredStartPlanningTask || (!HasActivePlan() && !CurrentPlanningTask))
		{
			StartPlanningTask();
		}

		if (HasActivePlan())
		{
#if ENABLE_VISUAL_LOG
			VisLogCurrentPlan();
#endif
			TickCurrentPlan(DeltaTime);
		}
	}

	if (bDeferredAbortPlan)
	{
		AbortCurrentPlan();
	}

	if (bDeferredStopHTN)
	{
		StopHTN();
	}
}

void UHTNComponent::OnRegister()
{
	Super::OnRegister();

	REDIRECT_TO_VLOG(GetAIOwner());
}

void UHTNComponent::BeginPlay()
{
	Super::BeginPlay();

#if USE_HTN_DEBUGGER
	PlayingComponents.AddUnique(this);
#endif
}

void UHTNComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{	
	// Cleanup and remove worldstates before the blackboard component they reference gets uninitialized
	Cleanup();

#if USE_HTN_DEBUGGER
	PlayingComponents.Remove(this);
#endif

	Super::EndPlay(EndPlayReason);
}

void UHTNComponent::RestartLogic()
{
	UE_VLOG(GetOwner(), LogHTN, Log, TEXT("UHTNComponent::RestartLogic"));

	CancelActivePlanning();
	if (HasActivePlan()) 
	{
		AbortCurrentPlan();
	}
}

void UHTNComponent::StopLogic(const FString& Reason)
{
	UE_VLOG(GetOwner(), LogHTN, Log, TEXT("Stopping HTN, reason: \'%s\'"), *Reason);
	StopHTN();
}

void UHTNComponent::Cleanup()
{
	SCOPE_CYCLE_COUNTER(STAT_AI_HTN_Cleanup);
	
	// Ensure the worldstates in the plan are deallocated before their linked BlackboardComponent,
	// since they need info from there to properly deallocate their values.
	StopHTN(/*bDisregardLatentAbort*/true);
	ClearCurrentPlan();
	PendingHTNStartInfo = {};
	PendingPlanExecutionInfo = {};

	CancelActivePlanning();
	SetPlanningWorldState(nullptr);
	
	// End gameplay tasks
	if (AIOwner)
	{
		if (UGameplayTasksComponent* const GTComp = AIOwner->GetGameplayTasksComponent())
		{
			GTComp->EndAllResourceConsumingTasksOwnedBy(*this);
		}
	}

#if USE_HTN_DEBUGGER
	DebuggerSteps.Reset();
#endif
}

void UHTNComponent::PauseLogic(const FString& Reason)
{
	UE_VLOG(GetOwner(), LogHTN, Log, TEXT("Execution updates: PAUSED (%s)"), *Reason);
	bIsPaused = true;

	if (BlackboardComp)
	{
		BlackboardComp->PauseObserverNotifications();
	}
}

EAILogicResuming::Type UHTNComponent::ResumeLogic(const FString& Reason)
{
	const EAILogicResuming::Type SuperResumeResult = Super::ResumeLogic(Reason);
	if (bIsPaused)
	{
		bIsPaused = false;

		if (SuperResumeResult == EAILogicResuming::Continue)
		{
			if (BlackboardComp)
			{
				// Resume the blackboard's observer notifications and send any queued notifications
				BlackboardComp->ResumeObserverNotifications(true);
			}
		}
		else if (SuperResumeResult == EAILogicResuming::RestartedInstead)
		{
			if (BlackboardComp)
			{
				// Resume the blackboard's observer notifications but do not send any queued notifications
				BlackboardComp->ResumeObserverNotifications(false);
			}
		}
	}

	return SuperResumeResult;
}

bool UHTNComponent::IsRunning() const
{
	return !bIsPaused && (CurrentPlanningTask || HasActivePlan());
}

bool UHTNComponent::IsPaused() const
{
	return bIsPaused;
}

UGameplayTasksComponent* UHTNComponent::GetGameplayTasksComponent(const UGameplayTask& Task) const
{
	// Helpers for making sure the AIController has a gameplay tasks component.
	// AIControllers possessing a pawn make sure there is one on their own, but standalone ones don't.
	struct Local
	{
		struct AAIControllerHelper : public AAIController
		{
			static void SetCachedGameplayComponent(AAIController& InAIController, UGameplayTasksComponent* TasksComponent)
			{
				StaticCast<AAIControllerHelper&>(InAIController).CachedGameplayTasksComponent = TasksComponent;
			}
		};
		
		static UGameplayTasksComponent* EnsureGameplayTasksComponent(AAIController& InAIController, const UGameplayTask& InTask)
		{
			if (UGameplayTasksComponent* const ExistingTasksComponent = InAIController.GetGameplayTasksComponent(InTask))
			{
				return ExistingTasksComponent;
			}

			if (UGameplayTasksComponent* const FoundTasksComponent = InAIController.FindComponentByClass<UGameplayTasksComponent>())
			{
				AAIControllerHelper::SetCachedGameplayComponent(InAIController, FoundTasksComponent);
				return FoundTasksComponent;
			}

			UGameplayTasksComponent* const NewTasksComponent = NewObject<UGameplayTasksComponent>(&InAIController, TEXT("GameplayTasksComponent"));
			NewTasksComponent->RegisterComponent();
			AAIControllerHelper::SetCachedGameplayComponent(InAIController, NewTasksComponent);
			return NewTasksComponent;
		}
	};
	
	if (const UAITask* const AITask = Cast<UAITask>(&Task))
	{
		if (AAIController* const AIController = AITask->GetAIController())
		{
			return Local::EnsureGameplayTasksComponent(*AIController, Task);
		}
	}

	if (AIOwner)
	{
		return Local::EnsureGameplayTasksComponent(*AIOwner, Task);
	}

	return Task.GetGameplayTasksComponent();
}

AActor* UHTNComponent::GetGameplayTaskOwner(const UGameplayTask* Task) const
{
	if (!Task)
	{
		return AIOwner;
	}

	if (const UAITask* AITask = Cast<const UAITask>(Task))
	{
		return AITask->GetAIController();
	}

	if (const UGameplayTasksComponent* const TasksComponent = Task->GetGameplayTasksComponent())
	{
		return TasksComponent->GetGameplayTaskOwner(Task);
	}

	return nullptr;
}

AActor* UHTNComponent::GetGameplayTaskAvatar(const UGameplayTask* Task) const
{
	if (!Task)
	{
		return AIOwner ? AIOwner->GetPawn() : nullptr;
	}

	if (const UAITask* AITask = Cast<const UAITask>(Task))
	{
		return AITask->GetAIController() ? AITask->GetAIController()->GetPawn() : nullptr;
	}

	if (const UGameplayTasksComponent* const TasksComponent = Task->GetGameplayTasksComponent())
	{
		return TasksComponent->GetGameplayTaskAvatar(Task);
	}

	return nullptr;
}

uint8 UHTNComponent::GetGameplayTaskDefaultPriority() const { return StaticCast<uint8>(EAITaskPriority::AutonomousAI); }

void UHTNComponent::OnGameplayTaskInitialized(UGameplayTask& Task)
{
	if (const UAITask* const AITask = Cast<const UAITask>(&Task))
	{
		if (!AITask->GetAIController())
		{
			// this means that the AI task was either
			// created without specifying UAITask::OwnerController's value (like via BP's Construct Object node)
			// or it was created in C++ with an inappropriate function.
			UE_LOG(LogHTN, Error, TEXT("Missing AIController in AITask %s"), *AITask->GetName());
		}
	}
}

void UHTNComponent::OnGameplayTaskDeactivated(UGameplayTask& Task)
{
	if (&Task == CurrentPlanningTask && Task.GetState() != EGameplayTaskState::Paused)
	{
		OnPlanningTaskFinished();
	}
}

void UHTNComponent::StartHTN(UHTN* Asset)
{
	if (CurrentHTNAsset == Asset)
	{
		UE_VLOG(GetOwner(), LogHTN, Log, TEXT("Skipping HTN start request - it's already running"));
		return;
	}

	StopHTN();

	PendingHTNStartInfo = { Asset };
	if (!HasActivePlan() && !IsWaitingForAbortingTasks())
	{
		StartPendingHTN();
	}
}

void UHTNComponent::StopHTN(bool bDisregardLatentAbort)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_HTN_StopHTN);

	if (LockFlags)
	{
		bDeferredStopHTN = true;
		return;
	}
	FHTNComponentScopedLock ScopedLock(*this, FHTNComponentScopedLock::LockStopHTN);
	ON_SCOPE_EXIT { bDeferredStopHTN = false; };

	CancelActivePlanning();
	PendingPlanExecutionInfo = {};
	PendingHTNStartInfo = {};
	
	if (HasActivePlan())
	{
		bAbortingToStopHTN = true;
		AbortCurrentPlan();
	}
	else
	{
		ClearCurrentPlan();
		
		SetPlanningWorldState(nullptr);
		CurrentHTNAsset = nullptr;
		CooldownOwnerToEndTimeMap.Reset();
	}

	if (IsWaitingForAbortingTasks())
	{
		if (!bDisregardLatentAbort)
		{
			UE_VLOG(GetOwner(), LogHTN, Log, TEXT("StopHTN is waiting for aborting tasks to finish..."));
		}
		else
		{
			UE_VLOG(GetOwner(), LogHTN, Warning, TEXT("StopHTN was forced while waiting for tasks to finish aborting!"));
			for (int32 I = CurrentlyAbortingStepIDs.Num() - 1; I >= 0; I--)
			{
				UHTNTask& Task = GetTaskInCurrentPlan(CurrentlyAbortingStepIDs[I]);
				OnTaskFinished(&Task, EHTNNodeResult::Aborted);
			}
			check(CurrentlyAbortingStepIDs.Num() == 0);
		}
	}
}

void UHTNComponent::CancelActivePlanning()
{
	if (CurrentPlanningTask)
	{
		CurrentPlanningTask->ExternalCancel();
		if (CurrentPlanningTask)
		{
			CurrentPlanningTask->Clear();
			CurrentPlanningTask = nullptr;
		}
	}
}

void UHTNComponent::ForceReplan(bool bForceAbortPlan, bool bForceRestartActivePlanning, bool bForceDeferToNextFrame)
{
	if (!IsValid(this))
	{
		return;
	}
	
	if (bForceAbortPlan && CurrentPlan.IsValid() && !IsWaitingForAbortingTasks())
	{
		AbortCurrentPlan(bForceDeferToNextFrame);
	}

	if (bForceRestartActivePlanning || !CurrentPlanningTask)
	{
		StartPlanningTask(bForceDeferToNextFrame);
	}
}

void UHTNComponent::SetPlanningWorldState(TSharedPtr<FBlackboardWorldState> WorldState, bool bIsEditable)
{
	check(PlanningWorldStateProxy);
	PlanningWorldStateProxy->WorldState = WorldState;
	PlanningWorldStateProxy->bIsEditable = bIsEditable;
}

float UHTNComponent::GetCooldownEndTime(const UObject* CooldownOwner) const
{
	const float* const CooldownEndTime = CooldownOwnerToEndTimeMap.Find(CooldownOwner);
	return CooldownEndTime ? *CooldownEndTime : -FLT_MAX;
}

void UHTNComponent::AddCooldownDuration(const UObject* CooldownOwner, float CooldownDuration, bool bAddToExistingDuration)
{
	if (CooldownOwner)
	{
		float* const CurrentEndTime = CooldownOwnerToEndTimeMap.Find(CooldownOwner);

		// If we are supposed to add to an existing duration, do that, otherwise we set a new value.
		if (bAddToExistingDuration && CurrentEndTime)
		{
			*CurrentEndTime += CooldownDuration;
		}
		else
		{
			CooldownOwnerToEndTimeMap.Add(CooldownOwner, GetWorld()->GetTimeSeconds() + CooldownDuration);
		}
	}
}

bool UHTNComponent::SetDynamicHTN(FGameplayTag InjectTag, UHTN* HTN, bool bForceAbortCurrentPlanIfChanged)
{
	UHTN* PreviousHTN = nullptr;
	if (UHTN** const FoundPrevious = GameplayTagToDynamicHTNMap.Find(InjectTag))
	{
		PreviousHTN = *FoundPrevious;
		if (PreviousHTN == HTN)
		{
			return false;
		}
	}

	if (HTN)
	{
		GameplayTagToDynamicHTNMap.Add(InjectTag, HTN);
	}
	else
	{
		GameplayTagToDynamicHTNMap.Remove(InjectTag);
	}
		
	if (CurrentPlan.IsValid())
	{
		for (const TSharedPtr<FHTNPlanLevel>& Level : CurrentPlan->Levels)
		{
			for (const FHTNPlanStep& Step : Level->Steps)
			{
				if (UHTNNode_SubNetworkDynamic* const DynamicSubNetworkNode = Cast<UHTNNode_SubNetworkDynamic>(Step.Node.Get()))
				{
					// Do an exact check, because tag hierarchy is not supported for this.
					if (DynamicSubNetworkNode->InjectTag.MatchesTagExact(InjectTag))
					{
						const UHTN* const PreviousHTNForNode = PreviousHTN ? PreviousHTN : DynamicSubNetworkNode->DefaultHTN;
						const UHTN* const NewHTNForNode = HTN ? HTN : DynamicSubNetworkNode->DefaultHTN;
						if (PreviousHTNForNode != NewHTNForNode)
						{
							ForceReplan(bForceAbortCurrentPlanIfChanged, /*bForceRestartActivePlanning=*/true);
							return true;
						}
					}
				}
			}
		}
	}

	return true;
}

UHTN* UHTNComponent::GetDynamicHTN(FGameplayTag InjectTag) const
{
	if (UHTN*const*const HTN = GameplayTagToDynamicHTNMap.Find(InjectTag))
	{
		return *HTN;
	}

	return nullptr;
}

#if ENABLE_VISUAL_LOG

void UHTNComponent::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{
	struct Local
	{
		static void DescribePlanLevel(const FHTNPlan& Plan, int32 PlanLevelIndex, FVisualLogStatusCategory& LogCategory)
		{
			const FHTNPlanLevel& Level = *Plan.Levels[PlanLevelIndex];
			for (const FHTNPlanStep& Step : Level.Steps)
			{
				const FString& NodeName = Step.Node->GetNodeName();
				FVisualLogStatusCategory& StepCategory = LogCategory.Children.Emplace_GetRef(NodeName);

				if (Cast<UHTNTask>(Step.Node))
				{
					StepCategory.Add(NodeName, Step.Node->GetStaticDescription());
				}
				else if (Cast<UHTNNode_SubNetwork>(Step.Node))
				{
					if (Step.SubLevelIndex != INDEX_NONE)
					{
						DescribePlanLevel(Plan, Step.SubLevelIndex, StepCategory);
					}
					else
					{
						StepCategory.Add(TEXT("invalid"), TEXT("invalid"));
					}
				}
				else
				{
					const bool bHasPrimarySubLevel = Step.SubLevelIndex != INDEX_NONE;
					const bool bHasSecondarySubLevel = Step.SecondarySubLevelIndex != INDEX_NONE;
					if (bHasPrimarySubLevel || bHasSecondarySubLevel)
					{
						if (bHasPrimarySubLevel)
						{
							DescribePlanLevel(Plan, Step.SubLevelIndex, StepCategory.Children.Emplace_GetRef(TEXT("Primary")));
						}

						if (bHasSecondarySubLevel)
						{
							DescribePlanLevel(Plan, Step.SecondarySubLevelIndex, StepCategory.Children.Emplace_GetRef(TEXT("Secondary")));
						}
					}
					else
					{
						StepCategory.Add(TEXT("invalid"), TEXT("invalid"));
					}
				}
			}
		}
	};
	
	if (!IsValid(this))
	{
		return;
	}

	Super::DescribeSelfToVisLog(Snapshot);

	FVisualLogStatusCategory& StatusCategory = Snapshot->Status.Emplace_GetRef();
	StatusCategory.Category = FString::Printf(TEXT("HTN (asset: %s)"), *GetNameSafe(CurrentHTNAsset));

	if (CurrentPlan.IsValid())
	{
		Local::DescribePlanLevel(*CurrentPlan, 0, StatusCategory);
	}
	else
	{
		StatusCategory.Add(TEXT("invalid"), TEXT("invalid"));
	}
}
#endif

void UHTNComponent::StartPendingHTN()
{
	CooldownOwnerToEndTimeMap.Reset();
	
	CurrentHTNAsset = PendingHTNStartInfo.NewAsset.Get();
	PendingHTNStartInfo = {};

	if (CurrentHTNAsset)
	{
		if (EnsureCompatibleBlackboardAsset(CurrentHTNAsset->BlackboardAsset))
		{
			check(!CurrentPlan.IsValid());
			StartPlanningTask();
		}
		else
		{
			StopHTN();
		}
	}
}

bool UHTNComponent::EnsureCompatibleBlackboardAsset(UBlackboardData* DesiredBlackboardAsset)
{
	if (!DesiredBlackboardAsset)
	{
		UE_VLOG_UELOG(AIOwner, LogHTN, Error, TEXT("HTN trying to assign null blackboard asset."));
		return false;
	}
	
	UBlackboardComponent* BlackboardComponent = AIOwner->GetBlackboardComponent();
	if (!BlackboardComponent || !BlackboardComponent->IsCompatibleWith(DesiredBlackboardAsset))
	{
		// If changing to a different blackboard asset, remove all worldstates first.
		// This is necessary because worldstates rely on the blackboard component to manage their data.
		// If the blackboard component's asset changes, the worldstates wouldn't be able to deallocate correctly.
		// To prevent that, we destroy them first.
		DeleteAllWorldStates();
		if (!AIOwner->UseBlackboard(DesiredBlackboardAsset, BlackboardComponent))
		{
			UE_VLOG_UELOG(AIOwner, LogHTN, Error, TEXT("Could not use blackboard asset %s required by HTN %s. Previous blackboard asset is %s."),
				*GetNameSafe(DesiredBlackboardAsset),
				*GetNameSafe(CurrentHTNAsset),
				*GetNameSafe(BlackboardComponent ? BlackboardComponent->GetBlackboardAsset() : nullptr)
			);
			return false;
		}
	}

	return true;
}

void UHTNComponent::DeleteAllWorldStates()
{
	ClearCurrentPlan();
	PendingHTNStartInfo = {};
	PendingPlanExecutionInfo = {};

	CancelActivePlanning();
	SetPlanningWorldState(nullptr);
	
#if USE_HTN_DEBUGGER
	DebuggerSteps.Reset();
#endif
}

void UHTNComponent::StartPlanningTask(bool bDeferToNextFrame)
{
	if (bDeferToNextFrame)
	{
		bDeferredStartPlanningTask = true;
		return;
	}
	ON_SCOPE_EXIT { bDeferredStartPlanningTask = false; };
	
	CancelActivePlanning();

	if (CurrentHTNAsset && AIOwner)
	{
		UpdateBlackboardState();

		check(AIOwner);
		CurrentPlanningTask = UAITask::NewAITask<UAITask_MakeHTNPlan>(*AIOwner, *this, TEXT("Make HTN Plan"));
		CurrentPlanningTask->SetUp(this, CurrentHTNAsset);

		UE_VLOG(AIOwner, LogHTN, Verbose, TEXT("HTNComponent starting planning task %s"), *CurrentPlanningTask->GetName());
		CurrentPlanningTask->ReadyForActivation();
	}
}

void UHTNComponent::OnPlanningTaskFinished()
{
	check(CurrentPlanningTask);

	if (CurrentPlanningTask->WasCancelled())
	{
		UE_VLOG(GetOwner(), LogHTN, Log, TEXT("planning task was cancelled"));
		CurrentPlanningTask->Clear();
		CurrentPlanningTask = nullptr;
		return;
	}
	
	const TSharedPtr<FHTNPlan> ProducedPlan = CurrentPlanningTask->GetFinishedPlan();
	CurrentPlanningTask->Clear();
	CurrentPlanningTask = nullptr;

	if (CurrentPlan.IsValid())
	{
		AbortCurrentPlan();
	}

	if (ProducedPlan.IsValid())
	{
		INC_DWORD_STAT(STAT_AI_HTN_NumProducedPlans);
		PendingPlanExecutionInfo = {ProducedPlan};
		
		if (!bDeferredAbortPlan && !IsWaitingForAbortingTasks())
		{
			StartPendingPlanExecution();
		}
	}
	else
	{
		UE_VLOG(GetOwner(), LogHTN, Log, TEXT("failed to produce a new plan"));
	}
}

void UHTNComponent::StartPendingPlanExecution()
{
	if (!ensure(PendingPlanExecutionInfo.IsSet()))
	{
		return;
	}

	if (!ensure(CurrentHTNAsset))
	{
		PendingPlanExecutionInfo = {};
		return;
	}

	check(!HasActivePlan());
	check(!bDeferredAbortPlan && !IsWaitingForAbortingTasks());
	
	CurrentPlan = PendingPlanExecutionInfo.NewPlan;
	PendingPlanExecutionInfo = {};

	UE_VLOG(GetOwner(), LogHTN, Log, TEXT("produced new plan with cost %d"), CurrentPlan->Cost);
	CurrentPlan->InitializeForExecution(*this, *CurrentHTNAsset, PlanMemory, InstancedNodes);
	if (!CurrentPlan->GetNextPrimitiveSteps(*this, {0, INDEX_NONE}, /*OutStepIds=*/PendingExecutionStepIDs, /*bIsExecutingPlan=*/true))
	{
		UE_VLOG(GetOwner(), LogHTN, Warning, TEXT("produced plan was degenerate, having no primitive tasks. Check if you have any Compound Tasks with unassigned HTN assets."));
		ClearCurrentPlan();
		return;
	}

	UE_VLOG(GetOwner(), LogHTN, Log, TEXT("started executing plan"));
	FHTNDelegates::OnPlanExecutionStarted.Broadcast(*this, CurrentPlan);
	NotifyOnPlanExecutionStarted();
}

void UHTNComponent::TickCurrentPlan(float DeltaTime)
{
	check(HasActivePlan());
	
	StartTasksPendingExecution();

	// Make a copy since items might get removed from the CurrentlyExecutingStepIDs during this.
	TArray<FHTNPlanStepID, TInlineAllocator<8>> ExecutingStepIDs(CurrentlyExecutingStepIDs);
	ExecutingStepIDs.Append(CurrentlyAbortingStepIDs);
	for (const FHTNPlanStepID& ExecutingStepID : ExecutingStepIDs)
	{
		if (!TickSubNodesOrRecheck(ExecutingStepID, DeltaTime))
		{
			// if wasn't already aborted from within the decorators.
			if (HasActivePlan())
			{
				AbortCurrentPlan();
				break;
			}
		}
		else
		{
			uint8* TaskMemory = nullptr;
			const UHTNTask& ExecutingTask = GetTaskInCurrentPlan(ExecutingStepID, TaskMemory);
			UE_VLOG(GetOwner(), LogHTN, VeryVerbose, TEXT("ticking %s."), *ExecutingTask.GetNodeName());
			ExecutingTask.WrappedTickTask(*this, TaskMemory, DeltaTime);
		}

		if (CurrentlyExecutingStepIDs.Num() == 0)
		{
			break;
		}
	}
}

void UHTNComponent::OnTaskFinished(const UHTNTask* Task, EHTNNodeResult Result)
{
	if (!Task || !HasPlan())
	{
		return;
	}

	if (!ensureMsgf(Result != EHTNNodeResult::InProgress, TEXT("UHTNComponent::OnTaskFinished called with EHTNNodeResult::InProgress. Task %s"), *Task->GetNodeName()))
	{
		return;
	}

	uint8* TaskMemory = nullptr;
	FHTNPlanStepID FinishedStepID = FHTNPlanStepID::None;
	const EHTNTaskStatus TaskStatus = FindStepIDAndMemoryOfTask(Task, FinishedStepID, TaskMemory);
	if (TaskStatus == EHTNTaskStatus::Inactive)
	{
		return;
	}
	
	const UHTNTask* const TaskTemplate = StaticCast<const UHTNTask*>(Task->GetTemplateNode());
	TaskTemplate->WrappedOnTaskFinished(*this, TaskMemory, Result);
	if (Result == EHTNNodeResult::Succeeded)
	{
		NotifyParallelSublevelFinishedIfNeeded(FinishedStepID);
	}
	FinishSubNodesAtPlanStep(FinishedStepID, Result);
	if (Result == EHTNNodeResult::Succeeded)
	{
		AbortSecondaryParallelBranchesIfNeeded(FinishedStepID);
		check(CurrentPlan.IsValid());
	}
	CurrentlyExecutingStepIDs.RemoveSingle(FinishedStepID);

	check(BlackboardComp);
	check(CurrentPlan.IsValid());
	CurrentPlan->CheckIntegrity();
	
	if (Result == EHTNNodeResult::Succeeded)
	{
		UE_VLOG(GetOwner(), LogHTN, Verbose, TEXT("finished %s (plan level %i, step %i)"), 
			*Task->GetNodeName(), 
			FinishedStepID.LevelIndex, 
			FinishedStepID.StepIndex
		);

		// Apply worldstate changes of the finished step
		const FHTNPlanStep& FinishedStep = CurrentPlan->GetStep(FinishedStepID);
		FinishedStep.WorldState->ApplyChangedValues(*BlackboardComp);
		
		// Pick next tasks
		CurrentPlan->GetNextPrimitiveSteps(*this, FinishedStepID, /*OutStepIds=*/PendingExecutionStepIDs, /*bIsExecutingPlan=*/true);
		if (!HasActiveTasks())
		{
			OnPlanExecutionSuccessfullyFinished();
		}
	}
	else if (Result == EHTNNodeResult::Aborted)
	{
		UE_VLOG(GetOwner(), LogHTN, Verbose, TEXT("finished aborting %s (plan level %i, step %i)"),
			*Task->GetNodeName(),
			FinishedStepID.LevelIndex,
			FinishedStepID.StepIndex
		);

		CurrentlyAbortingStepIDs.RemoveSingle(FinishedStepID);
		if (!HasActiveTasks())
		{
			if (bAbortingPlan)
			{
				OnPlanAbortFinished();
			}
			else
			{
				OnPlanExecutionSuccessfullyFinished();
			}
		}
	}
	else
	{
		UE_VLOG(GetOwner(), LogHTN, Verbose, TEXT("failed %s (plan level %i, step %i)"), 
			*Task->GetNodeName(), 
			FinishedStepID.LevelIndex, 
			FinishedStepID.StepIndex
		);
		AbortCurrentPlan();
	}
}

bool UHTNComponent::NotifyEventBasedDecoratorCondition(UHTNDecorator* Decorator, bool bRawConditionValue, bool bCanAbortPlanInstantly)
{
	// In versions prior to 4.26, there was a bug in Blackboard:
	// calling RemoveObserver while inside NotifyObservers would cause a crash (modifying a multimap while iterating over it).
	// Postponing the abort of the current plan and the start of a new planning until next frame prevents this from happening.
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
	const bool bForceDeferAbortToNextFrame = true;
#else
	const bool bForceDeferAbortToNextFrame = false;
#endif
	
	if (IsValid(Decorator) && HasActivePlan() && !bAbortingPlan)
	{
		const bool bConditionValue = Decorator->IsInversed() ? !bRawConditionValue : bRawConditionValue;
		
		const UHTNDecorator* const DecoratorTemplate = StaticCast<const UHTNDecorator*>(Decorator->GetTemplateNode());
		if (IsValid(DecoratorTemplate))
		{
			TArray<FHTNSubNodeGroup> SubNodeGroups;
			const auto TryInArray = [&](const TArray<FHTNPlanStepID>& StepIDs) -> bool
			{
				for (const FHTNPlanStepID& StepID : StepIDs)
				{
					SubNodeGroups.Reset();
					CurrentPlan->GetSubNodesAtPlanStep(StepID, SubNodeGroups);
					for (const FHTNSubNodeGroup& Group : SubNodeGroups)
					{
						const bool bCanReplan = bConditionValue == Group.bIsIfNodeFalseBranch && 
							!(Group.bIsIfNodeFalseBranch && !Group.bCanConditionsInterruptFalseBranch);
						if (bCanReplan)
						{
							for (const THTNNodeInfo<UHTNDecorator>& Info : *Group.Decorators)
							{
								if (Info.TemplateNode == DecoratorTemplate)
								{
									UE_VLOG(GetOwner(), LogHTN, Log, TEXT("Decorator '%s' of task '%s' (plan level %i, step %i) notified the HTNComponent of its condition, which forced a replan."),
										*Decorator->GetNodeName(),
										CurrentPlan->HasStep(Group.PlanStepID) ? *CurrentPlan->GetStep(Group.PlanStepID).Node->GetNodeName() : TEXT("root"),
										StepID.LevelIndex, StepID.StepIndex
									);
									ForceReplan(bCanAbortPlanInstantly, /*bForceRestartActivePlanning=*/true, bForceDeferAbortToNextFrame);
									return true;
								}
							}
						}
					}
				}

				return false;
			};

			return TryInArray(CurrentlyExecutingStepIDs) || TryInArray(PendingExecutionStepIDs);
		}
	}

	return false;
}

bool UHTNComponent::HasActivePlan() const
{
	return HasPlan() && HasActiveTasks();
}

bool UHTNComponent::HasPlan() const
{
	return CurrentPlan.IsValid();
}

bool UHTNComponent::HasActiveTasks() const
{
	return CurrentlyExecutingStepIDs.Num() || PendingExecutionStepIDs.Num() || CurrentlyAbortingStepIDs.Num();
}

bool UHTNComponent::IsWaitingForAbortingTasks() const
{
	return CurrentlyAbortingStepIDs.Num() > 0;
}

bool UHTNComponent::IsPlanning() const
{
	return IsValid(CurrentPlanningTask);
}

EHTNTaskStatus UHTNComponent::GetTaskStatus(const UHTNTask* Task) const
{
	const UHTNTask* const TaskTemplate = StaticCast<const UHTNTask*>(Task->GetTemplateNode());
	
	if (IsValid(TaskTemplate) && HasActivePlan())
	{
		for (const FHTNPlanStepID& AbortingStepID : CurrentlyAbortingStepIDs)
		{
			if (TaskTemplate == &GetTaskInCurrentPlan(AbortingStepID))
			{
				return EHTNTaskStatus::Aborting;
			}
		}
		
		for (const FHTNPlanStepID& ExecutingStepID : CurrentlyExecutingStepIDs)
		{
			if (TaskTemplate == &GetTaskInCurrentPlan(ExecutingStepID))
			{
				return EHTNTaskStatus::Active;
			}
		}
	}

	return EHTNTaskStatus::Inactive;
}

EHTNTaskStatus UHTNComponent::FindStepIDAndMemoryOfTask(const UHTNTask* Task, FHTNPlanStepID& OutPlanStepID, uint8*& OutTaskMemory)
{
	if (HasActivePlan())
	{
		const UHTNTask* const TaskTemplate = StaticCast<const UHTNTask*>(Task->GetTemplateNode());
		if (IsValid(TaskTemplate))
		{
			const auto FindIn = [&](const TArray<FHTNPlanStepID>& StepIDs) -> bool
			{
				for (const FHTNPlanStepID& StepID : StepIDs)
				{
					uint8* TaskMemory = nullptr;
					if (TaskTemplate == &GetTaskInCurrentPlan(StepID, TaskMemory))
					{
						OutTaskMemory = TaskMemory;
						OutPlanStepID = StepID;
						return true;
					}
				}

				return false;
			};

			if (FindIn(CurrentlyExecutingStepIDs))
			{
				return EHTNTaskStatus::Active;
			}
			
			if (FindIn(CurrentlyAbortingStepIDs))
			{
				return EHTNTaskStatus::Aborting;
			}
		}
	}

	OutTaskMemory = nullptr;
	OutPlanStepID = FHTNPlanStepID::None;
	return EHTNTaskStatus::Inactive;
}

uint8* UHTNComponent::GetNodeMemory(const UHTNNode* Node, const FHTNPlanStepID& StepID) const
{
	if (!Node)
	{
		return nullptr;
	}

	if (!HasActivePlan())
	{
		return nullptr;
	}

	if (!CurrentPlan->HasStep(StepID))
	{
		return nullptr;
	}
	
	const FHTNPlanStep& Step = CurrentPlan->GetStep(StepID);
	
	const UHTNNode* const TemplateNode = Node->GetTemplateNode();
	if (Cast<UHTNTask>(TemplateNode))
	{
		if (Step.Node == TemplateNode)
		{
			return GetNodeMemory(Step.NodeMemoryOffset);
		}
	}
	else if (Cast<UHTNDecorator>(TemplateNode))
	{
		for (const THTNNodeInfo<UHTNDecorator>& DecoratorInfo : Step.DecoratorInfos)
		{
			if (DecoratorInfo.TemplateNode == TemplateNode)
			{
				return GetNodeMemory(DecoratorInfo.NodeMemoryOffset);
			}
		}
	}
	else // if is service
	{
		for (const THTNNodeInfo<UHTNService>& ServiceInfo : Step.ServiceInfos)
		{
			if (ServiceInfo.TemplateNode == TemplateNode)
			{
				return GetNodeMemory(ServiceInfo.NodeMemoryOffset);
			}
		}
	}

	return nullptr;
}

void UHTNComponent::StartTasksPendingExecution()
{
	// Note that step ids might get added to PendingExecutionStepIDs during the loop 
	// if a task completes instantly (e.g. Success, SetValue)
	TArray<FHTNPlanStepID, TInlineAllocator<8>> AlreadyStartedSteps;
	while (PendingExecutionStepIDs.Num() && !IsWaitingForAbortingTasks())
	{
		// PendingExecutionPlanStepIDs may be arbitrarily changed within this loop (i.e. in AbortSecondaryParallelBranchesIfNeeded).
		// To avoid errors, items are popped from the beginning of the array during iteration.
		const FHTNPlanStepID AddedStepID = PendingExecutionStepIDs[0];
		PendingExecutionStepIDs.RemoveAt(0, 1, /*bAllowShrinking=*/false);

		// Stop if have to start the same step again
		// (such as when the secondary branch of a Parallel node is looping and completed instantly)
		if (AlreadyStartedSteps.Contains(AddedStepID))
		{
			break;
		}

		TArray<FHTNPlanStepID, TInlineAllocator<8>> EnteringStepIDs { AddedStepID };
		while (EnteringStepIDs.Top().StepIndex == 0 && EnteringStepIDs.Top().LevelIndex > 0)
		{
			EnteringStepIDs.Add(CurrentPlan->Levels[EnteringStepIDs.Top().LevelIndex]->ParentStepID);
		}
		
#if USE_HTN_DEBUGGER
		// Store debug steps of entering the plan steps that contain this level
		for (int32 StepIndex = EnteringStepIDs.Num() - 1; StepIndex >= 0; --StepIndex)
		{
			StoreDebugStep().ActivePlanStepIDs.Add(EnteringStepIDs[StepIndex]);
		}
#endif

		if (!CurrentPlan->IsSecondaryParallelStep(AddedStepID))
		{
			for (int32 StepIndex = EnteringStepIDs.Num() - 1; StepIndex >= 0; --StepIndex)
			{
				const FHTNPlanStep& EnteringStep = CurrentPlan->GetStep(EnteringStepIDs[StepIndex]);
				EnteringStep.WorldStateAfterEnteringDecorators->ApplyChangedValues(*BlackboardComp);
			}
		}

		const EHTNNodeResult Result = StartExecuteTask(AddedStepID);
		if (bDeferredAbortPlan || bDeferredStopHTN)
		{
			break;
		}

		AlreadyStartedSteps.Add(AddedStepID);
	}
}

EHTNNodeResult UHTNComponent::StartExecuteTask(const FHTNPlanStepID& PlanStepID)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_HTN_Execution);
	
	if (!ensure(HasPlan()))
	{
		return EHTNNodeResult::Failed;
	}

	check(!CurrentlyExecutingStepIDs.Contains(PlanStepID));
	CurrentlyExecutingStepIDs.Add(PlanStepID);
#if USE_HTN_DEBUGGER
	StoreDebugStep();
#endif
	
	const FHTNPlanStep& PlanStep = CurrentPlan->GetStep(PlanStepID);
	UHTNTask& Task = *CastChecked<UHTNTask>(PlanStep.Node);
	UE_VLOG(GetOwner(), LogHTN, Verbose, TEXT("starting task %s (plan level %i, step %i)"),
		*Task.GetNodeName(),
		PlanStepID.LevelIndex,
		PlanStepID.StepIndex
	);
	
	StartSubNodesStartingAtPlanStep(PlanStepID);
	const EHTNNodeResult Result = Task.WrappedExecuteTask(*this, GetNodeMemory(PlanStep.NodeMemoryOffset), PlanStepID);
	if (Result != EHTNNodeResult::InProgress)
	{
		OnTaskFinished(&Task, Result);
	}
	return Result;
}

bool UHTNComponent::RecheckCurrentPlan()
{
	if (!ensure(HasActivePlan()))
	{
		return false;
	}

	if (!CurrentlyExecutingStepIDs.Num())
	{
		return true;
	}

	// Ensure that the worldstate proxy is restored to its current state at the end of this.
	FGuardWorldStateProxy GuardProxy(*PlanningWorldStateProxy);
	
	struct FRecheckContext
	{
		TSharedRef<FBlackboardWorldState> WorldState;
		FHTNPlanStepID StepID;
	};

	TArray<FRecheckContext> RecheckStack;
	Algo::Transform(CurrentlyExecutingStepIDs, RecheckStack, [&](const FHTNPlanStepID& StepID) -> FRecheckContext
	{
		return { MakeShared<FBlackboardWorldState>(*BlackboardComp), StepID };
	});
	// Make sure that the step on the most primary branch is first, i.e. on the bottom of the stack.
	Algo::SortBy(RecheckStack, [&](const FRecheckContext& RecheckContext)
	{
		return CurrentPlan->IsSecondaryParallelStep(RecheckContext.StepID) ? 1 : 0;
	});
	
	TArray<FHTNPlanStepID> NextStepsBuffer;
	while (RecheckStack.Num())
	{
		const FRecheckContext CurrentContext = RecheckStack.Pop();
		SetPlanningWorldState(CurrentContext.WorldState, /*bIsEditable=*/false);

		// Apply changes caused by decorators entering
		TArray<FHTNPlanStepID, TInlineAllocator<4>> EnteredSteps { CurrentContext.StepID };
		while (EnteredSteps.Top().StepIndex == 0 && EnteredSteps.Top().LevelIndex > 0)
		{
			EnteredSteps.Add(CurrentPlan->Levels[EnteredSteps.Top().LevelIndex]->ParentStepID);
		}
		for (int32 StepIndex = EnteredSteps.Num() - 1; StepIndex >= 0; --StepIndex)
		{
			const FHTNPlanStep& EnteredStep = CurrentPlan->GetStep(EnteredSteps[StepIndex]);
			EnteredStep.WorldStateAfterEnteringDecorators->ApplyChangedValues(*CurrentContext.WorldState);
		}

		const FHTNPlanStep& CurrentStep = CurrentPlan->GetStep(CurrentContext.StepID);
		UHTNTask* const Task = CastChecked<UHTNTask>(CurrentStep.Node);

		// Recheck the task itself
		if (!Task->WrappedRecheckPlan(*this, GetNodeMemory(CurrentStep.NodeMemoryOffset), *CurrentContext.WorldState, CurrentStep))
		{
			UE_VLOG(GetOwner(), LogHTN, Log, 
				TEXT("plan recheck failed on task %s (plan level %i, step %i)."), 
				*Task->GetNodeName(), 
				CurrentContext.StepID.LevelIndex,
				CurrentContext.StepID.StepIndex
			);
			return false;
		}

		// Apply changes caused by the task and by decorators exiting
		CurrentStep.WorldState->ApplyChangedValues(*CurrentContext.WorldState);
		
		// Recheck decorators of future steps
		if (!CurrentlyExecutingStepIDs.Contains(CurrentContext.StepID))
		{
			if (!TickSubNodesOrRecheck(CurrentContext.StepID))
			{
				UE_VLOG(GetOwner(), LogHTN, Log, 
					TEXT("plan recheck failed because of subnodes active at task %s (plan level %i, step %i)."), 
					*Task->GetNodeName(),
					CurrentContext.StepID.LevelIndex,
					CurrentContext.StepID.StepIndex
				);
				return false;
			}
		}

		NextStepsBuffer.Reset();
		if (CurrentPlan->GetNextPrimitiveSteps(*this, CurrentContext.StepID, NextStepsBuffer, false))
		{
			RecheckStack.Push({ CurrentContext.WorldState, NextStepsBuffer[0] });
			for (int32 I = 1; I < NextStepsBuffer.Num(); ++I)
			{
				RecheckStack.Push({ CurrentContext.WorldState->MakeNext(), NextStepsBuffer[I] });
			}
		}
	}

	return true;
}

bool UHTNComponent::TickSubNodesOrRecheck(const FHTNPlanStepID& PlanStepID, float DeltaTime)
{
	if (!ensure(CurrentPlan.IsValid()))
	{
		return false;
	}

	const bool bIsPlanRecheck = !CurrentlyExecutingStepIDs.Contains(PlanStepID);

	const auto TickSubNode = [&](const auto& SubNodeInfo) -> bool
	{
		if (ensure(SubNodeInfo.TemplateNode))
		{
			check(HasActivePlan());
			SubNodeInfo.TemplateNode->WrappedTickNode(*this, GetNodeMemory(SubNodeInfo.NodeMemoryOffset), DeltaTime);
			const bool bWasAborted = bAbortingPlan || !HasActivePlan();
			if (bWasAborted)
			{
				return false;
			}
		}

		return true;
	};
	
	const auto TestDecoratorsOfGroup = 
	[
		&, CheckType = bIsPlanRecheck ? EHTNDecoratorConditionCheckType::PlanRecheck : EHTNDecoratorConditionCheckType::Execution
	]
	(const FHTNSubNodeGroup& Group) -> bool
	{
		bool bTestedAtLeastOneDecorator = false;
		for (const THTNNodeInfo<UHTNDecorator>& DecoratorInfo : *Group.Decorators)
		{
			UHTNDecorator* const DecoratorTemplate = DecoratorInfo.TemplateNode;
			if (!ensure(DecoratorTemplate))
			{
				continue;
			}
			
			uint8* const NodeMemory = GetNodeMemory(DecoratorInfo.NodeMemoryOffset);
			const EHTNDecoratorTestResult Result = DecoratorTemplate->WrappedTestCondition(*this, NodeMemory, CheckType);
			bTestedAtLeastOneDecorator |= Result != EHTNDecoratorTestResult::NotTested;

			const bool bWasAborted = bAbortingPlan || !HasActivePlan();
			if (bWasAborted)
			{
				return false;
			}

			if (Result == EHTNDecoratorTestResult::Failed)
			{
				if (!Group.bIsIfNodeFalseBranch && Group.bCanConditionsInterruptTrueBranch)
				{
					UE_VLOG(GetOwner(), LogHTN, Log, TEXT("%s of node '%s' (plan level %i, step %i) failed when checking decorator '%s' of node '%s' (plan level %i, step %i)"),
						bIsPlanRecheck ? TEXT("plan recheck") : TEXT("runtime test"),
						*CurrentPlan->GetStep(PlanStepID).Node->GetNodeName(),
						PlanStepID.LevelIndex,
						PlanStepID.StepIndex,
						*DecoratorTemplate->GetNodeName(),
						CurrentPlan->HasStep(Group.PlanStepID) ? *CurrentPlan->GetStep(Group.PlanStepID).Node->GetNodeName() : TEXT("root"),
						Group.PlanStepID.LevelIndex,
						Group.PlanStepID.StepIndex
					);

					return false;
				}
				else if (Group.bIsIfNodeFalseBranch)
				{
					return true;
				}
			}
		}

		if (bTestedAtLeastOneDecorator && Group.bIsIfNodeFalseBranch && Group.bCanConditionsInterruptFalseBranch)
		{
			UE_VLOG(GetOwner(), LogHTN, Log, TEXT("%s of node '%s' (plan level %i, step %i) failed because all of the decorators of node '%s' (plan level %i, step %i) succeeded while at least one of them should've failed."),
				bIsPlanRecheck ? TEXT("plan recheck") : TEXT("runtime test"),
				*CurrentPlan->GetStep(PlanStepID).Node->GetNodeName(),
				PlanStepID.LevelIndex,
				PlanStepID.StepIndex,
				CurrentPlan->HasStep(Group.PlanStepID) ? *CurrentPlan->GetStep(Group.PlanStepID).Node->GetNodeName() : TEXT("root"),
				Group.PlanStepID.LevelIndex,
				Group.PlanStepID.StepIndex
			);
			return false;
		}

		return true;
	};
	
	TArray<FHTNSubNodeGroup> SubNodeGroups;
	CurrentPlan->GetSubNodesAtExecutingPlanStep(*this, PlanStepID, SubNodeGroups);

	// Test the subnode groups in order or execution (outermost first, from root decorators of the top level down to the subnodes of the current task).
	for (int32 GroupIndex = SubNodeGroups.Num() - 1; GroupIndex >= 0; --GroupIndex)
	{
		const FHTNSubNodeGroup& Group = SubNodeGroups[GroupIndex];

		if (!bIsPlanRecheck)
		{
			for (const THTNNodeInfo<UHTNDecorator>& DecoratorInfo : *Group.Decorators)
			{
				if (!TickSubNode(DecoratorInfo))
				{
					return false;
				}
			}
		}

		if (!TestDecoratorsOfGroup(Group))
		{
			return false;
		}

		if (!bIsPlanRecheck)
		{
			for (const THTNNodeInfo<UHTNService>& ServiceInfo : *Group.Services)
			{
				if (!TickSubNode(ServiceInfo))
				{
					return false;
				}
			}
		}
	}

	return true;
}

void UHTNComponent::AbortCurrentPlan(bool bForceDeferToNextFrame)
{	
	if (bForceDeferToNextFrame || (LockFlags && !(LockFlags & FHTNComponentScopedLock::LockStopHTN)))
	{
		bDeferredAbortPlan = true;
		return;
	}
	FHTNComponentScopedLock ScopedLock(*this, FHTNComponentScopedLock::LockAbortPlan);
	ON_SCOPE_EXIT { bDeferredAbortPlan = false; };

	if (CurrentPlan.IsValid())
	{
		bAbortingPlan = true;
		PendingExecutionStepIDs.Reset();
		if (CurrentlyExecutingStepIDs.Num())
		{
			for (int32 I = CurrentlyExecutingStepIDs.Num() - 1; I >= 0; --I)
			{
				AbortExecutingPlanStep(CurrentlyExecutingStepIDs[I]);
				check(CurrentlyExecutingStepIDs.Num() == I);
			}
		}
		else if (!IsWaitingForAbortingTasks())
		{
			OnPlanAbortFinished();
		}
	}
}

void UHTNComponent::AbortExecutingPlanStep(const FHTNPlanStepID& PlanStepID)
{
	check(!CurrentlyAbortingStepIDs.Contains(PlanStepID));

	uint8* TaskMemory = nullptr;
	UHTNTask& Task = GetTaskInCurrentPlan(PlanStepID, TaskMemory);
	
	UE_VLOG(GetOwner(), LogHTN, Verbose, TEXT("aborting task %s (plan level %i, step %i)"), 
		*Task.GetNodeName(),
		PlanStepID.LevelIndex,
		PlanStepID.StepIndex
	);

	const EHTNNodeResult Result = Task.WrappedAbortTask(*this, TaskMemory);

#if DO_CHECK
	static const UEnum* const Enum = StaticEnum<EHTNNodeResult>();
	checkf(Result == EHTNNodeResult::Aborted || Result == EHTNNodeResult::InProgress, 
		TEXT("Unexpected Result returned from AbortTask of %s. Expected Aborted or InProgress, instead got %s"),
		*Task.GetNodeName(), *Enum->GetDisplayNameTextByValue(StaticCast<int64>(Result)).ToString()
	);
#endif
	
	if (Result == EHTNNodeResult::Aborted)
	{
		OnTaskFinished(&Task, Result);
	}
	else
	{
		CurrentlyExecutingStepIDs.RemoveSingle(PlanStepID);
		CurrentlyAbortingStepIDs.Add(PlanStepID);
	}
}

void UHTNComponent::ClearCurrentPlan()
{
	check(!IsWaitingForAbortingTasks());
	
	if (CurrentPlan.IsValid())
	{
		CurrentPlan->CleanupAfterExecution(*this);
		CurrentPlan.Reset();
#if USE_HTN_DEBUGGER
		StoreDebugStep(/*bIsEmpty=*/true);
#endif
	}
	
	CurrentlyExecutingStepIDs.Reset();
	PendingExecutionStepIDs.Reset();
	CurrentlyAbortingStepIDs.Reset();
	InstancedNodes.Reset();
	PlanMemory.Reset();
}

void UHTNComponent::OnPlanAbortFinished()
{
	check(bAbortingPlan);
	check(!IsWaitingForAbortingTasks());

	UE_VLOG(GetOwner(), LogHTN, Log, TEXT("finished aborting plan"));
	NotifyNodesOnPlanExecutionFinished(EHTNPlanExecutionFinishedResult::FailedOrAborted);
	ClearCurrentPlan();

	bAbortingPlan = false;
	if (bAbortingToStopHTN)
	{
		bAbortingToStopHTN = false;

		SetPlanningWorldState(nullptr);
		CurrentHTNAsset = nullptr;
		CooldownOwnerToEndTimeMap.Reset();
	}

	NotifyOnPlanExecutionFinished(EHTNPlanExecutionFinishedResult::FailedOrAborted);
}

void UHTNComponent::OnPlanExecutionSuccessfullyFinished()
{
	check(!HasActiveTasks());

	UE_VLOG(GetOwner(), LogHTN, Log, TEXT("finished executing plan successfully"));
	NotifyNodesOnPlanExecutionFinished(EHTNPlanExecutionFinishedResult::Succeeded);
	ClearCurrentPlan();
	NotifyOnPlanExecutionFinished(EHTNPlanExecutionFinishedResult::Succeeded);
}

void UHTNComponent::StartSubNodesStartingAtPlanStep(const FHTNPlanStepID& PlanStepID)
{
	if (!ensure(HasActivePlan()))
	{
		return;
	}

	TArray<FHTNSubNodeGroup> SubNodeGroups;
	CurrentPlan->GetSubNodesAtExecutingPlanStep(*this, PlanStepID, SubNodeGroups, /*bOnlyStarting=*/true);

	const auto StartExecution = [this](const auto& SubNodeInfo)
	{
		if (ensure(SubNodeInfo.TemplateNode))
		{
			SubNodeInfo.TemplateNode->WrappedExecutionStart(*this, GetNodeMemory(SubNodeInfo.NodeMemoryOffset));
		}
	};

	// Outermost to innermost subnodes.
	for (int32 GroupIndex = SubNodeGroups.Num() - 1; GroupIndex >= 0; --GroupIndex)
	{
		for (const THTNNodeInfo<UHTNDecorator>& DecoratorInfo : *SubNodeGroups[GroupIndex].Decorators)
		{
			StartExecution(DecoratorInfo);
		}

		for (const THTNNodeInfo<UHTNService>& ServiceInfo : *SubNodeGroups[GroupIndex].Services)
		{
			StartExecution(ServiceInfo);
		}
	}
}

void UHTNComponent::FinishSubNodesAtPlanStep(const FHTNPlanStepID& PlanStepID, EHTNNodeResult Result)
{
	if (!ensure(HasActivePlan()))
	{
		return;
	}

	TArray<FHTNSubNodeGroup> SubNodeGroups;
	const bool bFinishAllActive = Result == EHTNNodeResult::Aborted;
	CurrentPlan->GetSubNodesAtExecutingPlanStep(*this, PlanStepID, SubNodeGroups, /*bOnlyStarting=*/false, /*bOnlyEnding=*/!bFinishAllActive);
	
	const auto FinishExecution = [&](const auto& SubNodeInfo)
	{
		if (ensure(SubNodeInfo.TemplateNode))
		{
			SubNodeInfo.TemplateNode->WrappedExecutionFinish(*this, GetNodeMemory(SubNodeInfo.NodeMemoryOffset), Result);
		}
	};

	// innermost to outermost subnodes
	for (const FHTNSubNodeGroup& SubNodeGroup : SubNodeGroups)
	{
		const TArray<THTNNodeInfo<UHTNDecorator>>& DecoratorGroup = *SubNodeGroup.Decorators;
		for (int32 I = DecoratorGroup.Num() - 1; I >= 0; --I)
		{
			FinishExecution(DecoratorGroup[I]);
		}

		const TArray<THTNNodeInfo<UHTNService>>& ServiceGroup = *SubNodeGroup.Services;
		for (int32 I = ServiceGroup.Num() - 1; I >= 0; --I)
		{
			FinishExecution(ServiceGroup[I]);
		}
	}
}

void UHTNComponent::UpdateBlackboardState() const
{
	if (const AAIController* const AIController = GetAIOwner())
	{
		if (const APawn* const Pawn = AIController->GetPawn())
		{
			if (BlackboardComp && BlackboardComp->GetBlackboardAsset())
			{
				const FBlackboard::FKey SelfLocationKey = BlackboardComp->GetBlackboardAsset()->GetKeyID(FBlackboard::KeySelfLocation);
				if (ensureAsRuntimeWarning(SelfLocationKey != FBlackboard::InvalidKey))
				{
					BlackboardComp->SetValue<UBlackboardKeyType_Vector>(SelfLocationKey, Pawn->GetActorLocation());
				}
			}
		}
	}
}

void UHTNComponent::NotifyParallelSublevelFinishedIfNeeded(const FHTNPlanStepID& FinishedStepID)
{
	if (!CurrentlyExecutingStepIDs.Num())
	{
		return;
	}

	const FHTNPlanLevel& FinishedStepLevel = *CurrentPlan->Levels[FinishedStepID.LevelIndex];
	const FHTNPlanStepID ParentStepID = FinishedStepLevel.ParentStepID;
	const bool bFinishedLevel = FinishedStepID.StepIndex == FinishedStepLevel.Steps.Num() - 1;
	if (bFinishedLevel && ParentStepID != FHTNPlanStepID::None)
	{
		const FHTNPlanStep& ParentStep = CurrentPlan->GetStep(ParentStepID);
		if (UHTNNode_Parallel* const ParallelNode = Cast<UHTNNode_Parallel>(ParentStep.Node))
		{
			ParallelNode->OnSubLevelFinished(*this, ParentStepID, FinishedStepID.LevelIndex);
		}

		NotifyParallelSublevelFinishedIfNeeded(ParentStepID);
	}
}

void UHTNComponent::AbortSecondaryParallelBranchesIfNeeded(const FHTNPlanStepID& FinishedStepID)
{
	if (!CurrentlyExecutingStepIDs.Num())
	{
		return;
	}

	const FHTNPlanLevel& FinishedStepLevel = *CurrentPlan->Levels[FinishedStepID.LevelIndex];
	const FHTNPlanStepID ParentStepID = FinishedStepLevel.ParentStepID;
	const bool bFinishedLevel = FinishedStepID.StepIndex == FinishedStepLevel.Steps.Num() - 1;
	if (bFinishedLevel && ParentStepID != FHTNPlanStepID::None)
	{
		const FHTNPlanStep& ParentStep = CurrentPlan->GetStep(ParentStepID);
		if (UHTNNode_Parallel* const ParallelNode = Cast<UHTNNode_Parallel>(ParentStep.Node))
		{
			uint8* const NodeRawMemory = GetNodeMemory(ParentStep.NodeMemoryOffset);
			const UHTNNode_Parallel::FMemory* const NodeMemory = ParallelNode->CastInstanceNodeMemory<UHTNNode_Parallel::FMemory>(NodeRawMemory);
			if (FinishedStepID.LevelIndex == ParentStep.SubLevelIndex && NodeMemory->bIsExecutionComplete)
			{
				const auto IsStepUnderAbortedLevel =
				[
					&, SecondaryLevelIndex = ParentStep.SecondarySubLevelIndex
				]
				(const FHTNPlanStepID& StepID) -> bool
				{
					return CurrentPlan->HasStep(StepID, SecondaryLevelIndex);
				};

				PendingExecutionStepIDs.RemoveAll(IsStepUnderAbortedLevel);
				for (int32 I = CurrentlyExecutingStepIDs.Num() - 1; I >= 0; --I)
				{
					if (IsStepUnderAbortedLevel(CurrentlyExecutingStepIDs[I]))
					{
						AbortExecutingPlanStep(CurrentlyExecutingStepIDs[I]);
					}
				}
			}
		}
		
		AbortSecondaryParallelBranchesIfNeeded(ParentStepID);
	}
}

UHTNTask& UHTNComponent::GetTaskInCurrentPlan(const FHTNPlanStepID& ExecutingStepID) const
{
	check(HasActivePlan());
	
	const FHTNPlanStep& ExecutingStep = CurrentPlan->GetStep(ExecutingStepID);
	UHTNTask* const ExecutingTask = CastChecked<UHTNTask>(ExecutingStep.Node);
	
	check(ExecutingTask);
	return *ExecutingTask;
}

UHTNTask& UHTNComponent::GetTaskInCurrentPlan(const FHTNPlanStepID& ExecutingStepID, uint8*& OutTaskMemory) const
{
	check(HasActivePlan());
	
	const FHTNPlanStep& ExecutingStep = CurrentPlan->GetStep(ExecutingStepID);
	UHTNTask* const ExecutingTask = CastChecked<UHTNTask>(ExecutingStep.Node);
	
	OutTaskMemory = GetNodeMemory(ExecutingStep.NodeMemoryOffset);
	
	check(ExecutingTask);
	return *ExecutingTask;
}

void UHTNComponent::NotifyOnPlanExecutionStarted()
{
	PlanExecutionStartedEvent.Broadcast(this);
	PlanExecutionStartedBPEvent.Broadcast(this);
	NotifyNodesOnPlanExecutionStarted();
}

void UHTNComponent::NotifyOnPlanExecutionFinished(EHTNPlanExecutionFinishedResult Result)
{
	PlanExecutionFinishedEvent.Broadcast(this, Result);
	PlanExecutionFinishedBPEvent.Broadcast(this, Result);
}

void UHTNComponent::NotifyNodesOnPlanExecutionStarted()
{
	NotifyNodesOnPlanExecutionHelper([this](UHTNNode* TemplateNode, uint16 NodeMemoryOffset)
	{
		TemplateNode->WrappedOnPlanExecutionStarted(*this, GetNodeMemory(NodeMemoryOffset));
	});
}

void UHTNComponent::NotifyNodesOnPlanExecutionFinished(EHTNPlanExecutionFinishedResult Result)
{
	NotifyNodesOnPlanExecutionHelper([this, Result](UHTNNode* TemplateNode, uint16 NodeMemoryOffset)
	{
		TemplateNode->WrappedOnPlanExecutionFinished(*this, GetNodeMemory(NodeMemoryOffset), Result);
	});
}

void UHTNComponent::NotifyNodesOnPlanExecutionHelper(TFunctionRef<void(UHTNNode* /*TemplateNode*/, uint16 /*NodeMemoryOffset*/)> Callable)
{
	FGuardWorldStateProxy GuardProxy(*PlanningWorldStateProxy);

	for (const TSharedPtr<FHTNPlanLevel>& Level : CurrentPlan->Levels)
	{
		// Root subnodes
		{
			SetPlanningWorldState(Level->WorldStateAtLevelStart, /*bIsEditable=*/false);

			// Do root decorators
			for (THTNNodeInfo<UHTNDecorator>& DecoratorInfo : Level->RootDecoratorInfos)
			{
				Callable(DecoratorInfo.TemplateNode, DecoratorInfo.NodeMemoryOffset);
			}

			// Do root services
			for (THTNNodeInfo<UHTNService>& ServiceInfo : Level->RootServiceInfos)
			{
				Callable(ServiceInfo.TemplateNode, ServiceInfo.NodeMemoryOffset);
			}
		}

		// Do steps
		for (FHTNPlanStep& Step : Level->Steps)
		{
			SetPlanningWorldState(Step.WorldState, /*bIsEditable=*/false);

			if (Step.Node.IsValid())
			{
				Callable(Step.Node.Get(), Step.NodeMemoryOffset);
			}

			for (THTNNodeInfo<UHTNDecorator>& DecoratorInfo : Step.DecoratorInfos)
			{
				Callable(DecoratorInfo.TemplateNode, DecoratorInfo.NodeMemoryOffset);
			}

			for (THTNNodeInfo<UHTNService>& ServiceInfo : Step.ServiceInfos)
			{
				Callable(ServiceInfo.TemplateNode, ServiceInfo.NodeMemoryOffset);
			}
		}
	}
}

#if USE_HTN_DEBUGGER
FHTNDebugExecutionStep& UHTNComponent::StoreDebugStep(bool bIsEmpty) const
{
	FHTNDebugExecutionStep& Info = DebuggerSteps.Add_GetRef();

	if (!bIsEmpty)
	{
		Info.HTNPlan = CurrentPlan;
		Info.ActivePlanStepIDs = CurrentlyExecutingStepIDs;

		if (BlackboardComp && BlackboardComp->HasValidAsset())
		{
			const int32 NumKeys = BlackboardComp->GetNumKeys();
			Info.BlackboardValues.Empty(NumKeys);
			for (FBlackboard::FKey KeyID = 0; KeyID < NumKeys; KeyID++)
			{
				const FString Value = BlackboardComp->DescribeKeyValue(KeyID, EBlackboardDescription::OnlyValue);
				Info.BlackboardValues.Add(BlackboardComp->GetKeyName(KeyID), Value.Len() ? Value : TEXT("n/a"));
			}
		}
	}

	return Info;
}
#endif

#if ENABLE_VISUAL_LOG
void UHTNComponent::VisLogCurrentPlan()
{
	if (!FVisualLogger::IsRecording())
	{
		return;
	}
	
	if (!HasActivePlan())
	{
		return;
	}

	// Ensure that the worldstate proxy is restored to its current state at the end of this.
	FGuardWorldStateProxy GuardProxy(*PlanningWorldStateProxy);

	check(BlackboardComp);
	FVector Location = BlackboardComp->GetValue<UBlackboardKeyType_Vector>(FBlackboard::KeySelfLocation);
	FString LocationDescription;
	const auto LogLocation = [&]()
	{
		UE_VLOG_LOCATION(GetOwner(), LogHTNCurrentPlan, Verbose, Location, 10.0f, FColor::Blue, TEXT("%s"), *LocationDescription);
		LocationDescription.Reset();
	};

	const FHTNPlanStepID* const PrimaryStepID = CurrentlyExecutingStepIDs.FindByPredicate([this](const FHTNPlanStepID& StepID) -> bool
	{
		return !CurrentPlan->IsSecondaryParallelStep(StepID);
	});
	if (!PrimaryStepID)
	{
		return;
	}

	FHTNPlanStepID PlanStepID = *PrimaryStepID;
	TArray<FHTNPlanStepID> NextStepsBuffer;
	while (true)
	{
		const FHTNPlanStep& PlanStep = CurrentPlan->GetStep(PlanStepID);
		UHTNTask* const Task = Cast<UHTNTask>(PlanStep.Node);
		if (Task)
		{
			SetPlanningWorldState(PlanStep.WorldState, /*bIsEditable=*/false);
			Task->WrappedLogToVisualLog(*this, GetNodeMemory(PlanStep.NodeMemoryOffset), PlanStep);
		}
		const bool bShowTaskName = Task && Task->bShowTaskNameOnCurrentPlanVisualization;
		
		const FVector NextLocation = PlanStep.WorldState->GetValue<UBlackboardKeyType_Vector>(FBlackboard::KeySelfLocation);
		if (FAISystem::IsValidLocation(Location) && FAISystem::IsValidLocation(NextLocation) && !FVector::PointsAreNear(Location, NextLocation, KINDA_SMALL_NUMBER))
		{			
			LogLocation();
			UE_VLOG_SEGMENT_THICK(GetOwner(), LogHTNCurrentPlan, Verbose, Location, NextLocation, FColor::Blue, 5.0f, TEXT("%s"), bShowTaskName ? *PlanStep.Node->GetNodeName() : TEXT(""));
		}
		else if (bShowTaskName)
		{
			LocationDescription += PlanStep.Node->GetNodeName();
			LocationDescription += TEXT("\n");
		}
		Location = NextLocation;

		NextStepsBuffer.Reset();
		if (CurrentPlan->GetNextPrimitiveSteps(*this, PlanStepID, /*bIsExecutingPlan=*/NextStepsBuffer, /*bIsExecutingPlan=*/false))
		{
			// Only logging the primary branch, no secondaries.
			PlanStepID = NextStepsBuffer[0];
		}
		else
		{
			break;
		}
	}

	if (!LocationDescription.IsEmpty())
	{
		LogLocation();
	}
}
#endif
