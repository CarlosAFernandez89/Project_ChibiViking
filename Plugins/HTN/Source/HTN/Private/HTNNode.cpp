// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "HTNNode.h"

#include "Tasks/AITask.h"
#include "AIController.h"
#include "GameplayTasksComponent.h"

UHTNNode::UHTNNode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
#if WITH_EDITORONLY_DATA
	NodeIndexInGraph(INDEX_NONE),
#endif
	bCreateNodeInstance(false),
	bOwnsGameplayTasks(false),
	bNotifyOnPlanExecutionStarted(false),
	bNotifyOnPlanExecutionFinished(false),
	bForceUsingPlanningWorldState(false),
	HTNAsset(nullptr),
	OwnerComponent(nullptr)
{}

UWorld* UHTNNode::GetWorld() const
{
	if (OwnerComponent)
	{
		return OwnerComponent->GetWorld();
	}
	
	if (GetOuter())
	{
		// Special case for htn nodes in the editor
		if (Cast<UPackage>(GetOuter()))
		{
			// GetOuter should return a UPackage and its Outer is a UWorld
			return Cast<UWorld>(GetOuter()->GetOuter());
		}

		return GetOuter()->GetWorld();
	}

	return nullptr;
}

void UHTNNode::InitializeFromAsset(UHTN& Asset)
{
	HTNAsset = &Asset;
}

void UHTNNode::InitializeInPlan(UHTNComponent& OwnerComp, uint8* NodeMemory, 
	const FHTNPlan& Plan, const FHTNPlanStepID& StepID, 
	TArray<UHTNNode*>& OutNodeInstances) const
{
	FHTNNodeSpecialMemory* const SpecialMemory = GetSpecialNodeMemory<FHTNNodeSpecialMemory>(NodeMemory);
	if (SpecialMemory)
	{
		SpecialMemory->NodeInstanceIndex = INDEX_NONE;
	}

	if (!bCreateNodeInstance)
	{
		InitializeMemory(OwnerComp, NodeMemory, Plan, StepID);
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_AI_HTN_NodeInstantiation);
		UHTNNode* const NodeInstance = DuplicateObject(this, &OwnerComp);
		INC_DWORD_STAT(STAT_AI_HTN_NumNodeInstances);

		check(HTNAsset);
		check(SpecialMemory);
		
		NodeInstance->TemplateNode = const_cast<UHTNNode*>(this);
		NodeInstance->InitializeFromAsset(*HTNAsset);
		NodeInstance->SetOwnerComponent(&OwnerComp);
		NodeInstance->InitializeMemory(OwnerComp, NodeMemory, Plan, StepID);

		SpecialMemory->NodeInstanceIndex = OutNodeInstances.Add(NodeInstance);
	}
}

void UHTNNode::CleanupInPlan(UHTNComponent& OwnerComp, uint8* NodeMemory) const
{
	UHTNNode* const Node = GetNodeFromMemory(OwnerComp, NodeMemory);
	if (!ensure(Node))
	{
		return;
	}

	Node->CleanupMemory(OwnerComp, NodeMemory);
}

UHTNNode* UHTNNode::GetNodeFromMemory(const UHTNComponent& OwnerComp, uint8* NodeMemory) const
{
	if (!bCreateNodeInstance)
	{
		return const_cast<UHTNNode*>(this);
	}

	FHTNNodeSpecialMemory* const SpecialMemory = GetSpecialNodeMemory<FHTNNodeSpecialMemory>(NodeMemory);
	return SpecialMemory && OwnerComp.InstancedNodes.IsValidIndex(SpecialMemory->NodeInstanceIndex) ?
		OwnerComp.InstancedNodes[SpecialMemory->NodeInstanceIndex] :
		nullptr;
}

void UHTNNode::WrappedOnPlanExecutionStarted(UHTNComponent& OwnerComp, uint8* NodeMemory) const
{
	check(!IsInstance());
	if (bNotifyOnPlanExecutionStarted)
	{
		UHTNNode* const Node = GetNodeFromMemory(OwnerComp, NodeMemory);
		if (ensure(Node))
		{
			Node->OnPlanExecutionStarted(OwnerComp, NodeMemory);
		}
	}
}

void UHTNNode::WrappedOnPlanExecutionFinished(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNPlanExecutionFinishedResult Result) const
{
	check(!IsInstance());
	if (bNotifyOnPlanExecutionFinished)
	{
		UHTNNode* const Node = GetNodeFromMemory(OwnerComp, NodeMemory);
		if (ensure(Node))
		{
			Node->OnPlanExecutionFinished(OwnerComp, NodeMemory, Result);
		}
	}
}

FString UHTNNode::GetStaticDescription() const
{
	if (GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		return GetClass()->GetName().LeftChop(2);
	}

	return GetSubStringAfterUnderscore(GetClass()->GetName());
}

FString UHTNNode::GetNodeName() const
{
	if (NodeName.Len())
	{
		return NodeName;
	}
	
	const FString ClassName = GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) ? 
		GetClass()->GetName().LeftChop(2) :
		GetClass()->GetName();

	return GetSubStringAfterUnderscore(ClassName);
}

UGameplayTasksComponent* UHTNNode::GetGameplayTasksComponent(const UGameplayTask& Task) const
{
	if (const UAITask* const AITask = Cast<UAITask>(&Task))
	{
		if (AAIController* const AIController = AITask->GetAIController())
		{
			return AIController->GetGameplayTasksComponent(Task);
		}
	}

	if (OwnerComponent)
	{
		return OwnerComponent->GetGameplayTasksComponent(Task);
	}

	return Task.GetGameplayTasksComponent();
}

AActor* UHTNNode::GetGameplayTaskOwner(const UGameplayTask* Task) const
{
	if (!Task)
	{
		if (/*IsInstanced()*/true)
		{
			const UHTNComponent* const HTNComponent = Cast<const UHTNComponent>(GetOuter());
			check(HTNComponent);
			return HTNComponent->GetAIOwner();
		}
		else
		{
			UE_LOG(LogHTN, Warning, TEXT("%s: Unable to determine default GameplayTaskOwner!"), *GetName());
			return nullptr;
		}
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

AActor* UHTNNode::GetGameplayTaskAvatar(const UGameplayTask* Task) const
{
	if (!Task)
	{
		if (/*IsInstanced()*/true)
		{
			const UHTNComponent* const HTNComponent = Cast<const UHTNComponent>(GetOuter());
			check(HTNComponent);
			return HTNComponent->GetAIOwner();
		}
		else
		{
			UE_LOG(LogHTN, Warning, TEXT("%s: Unable to determine default GameplayTaskAvatar!"), *GetName());
			return nullptr;
		}
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

uint8 UHTNNode::GetGameplayTaskDefaultPriority() const { return StaticCast<uint8>(EAITaskPriority::AutonomousAI); }

void UHTNNode::OnGameplayTaskInitialized(UGameplayTask& Task)
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

UHTNComponent* UHTNNode::GetHTNComponentByTask(UGameplayTask& Task) const
{
	if (UAITask* const AITask = Cast<UAITask>(&Task))
	{
		if (AAIController* const AIController = AITask->GetAIController())
		{
			return Cast<UHTNComponent>(AIController->BrainComponent);
		}
	}

	return nullptr;
}

FString UHTNNode::GetSubStringAfterUnderscore(const FString& Input)
{
	const int32 Index = Input.Find(TEXT("_"));
	if (Index != INDEX_NONE)
	{
		return Input.Mid(Index + 1);
	}

	return Input;
}