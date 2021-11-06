// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Decorators/HTNDecorator_FocusScope.h"
#include "AIController.h"

UHTNDecorator_FocusScope::UHTNDecorator_FocusScope(const FObjectInitializer& Initializer) : Super(Initializer),
	bSetNewFocus(true),
	FocusPriority(EAIFocusPriority::Gameplay)
{
	bNotifyExecutionStart = true;
	bNotifyExecutionFinish = true;

	bCheckConditionOnPlanEnter = false;
	bCheckConditionOnPlanExit = false;
	bCheckConditionOnPlanRecheck = false;
	bCheckConditionOnTick = false;

	NodeName = TEXT("Focus Scope");

	FocusTarget.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UHTNDecorator_FocusScope, FocusTarget), AActor::StaticClass());
	FocusTarget.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UHTNDecorator_FocusScope, FocusTarget));
}

FString UHTNDecorator_FocusScope::GetStaticDescription() const
{
	return bSetNewFocus ?
		FString::Printf(TEXT("%s:\nFocus target: %s"), *Super::GetStaticDescription(), *FocusTarget.SelectedKeyName.ToString()) :
		FString::Printf(TEXT("%s:\nRestores focus on execution finish."), *Super::GetStaticDescription());
}

void UHTNDecorator_FocusScope::InitializeFromAsset(UHTN& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* const BBAsset = GetBlackboardAsset())
	{
		FocusTarget.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		UE_LOG(LogHTN, Warning, TEXT("Can't initialize %s due to missing blackboard data."), *GetNodeName());
		FocusTarget.InvalidateResolvedKey();
	}
}

uint16 UHTNDecorator_FocusScope::GetInstanceMemorySize() const { return sizeof(FNodeMemory); }

void UHTNDecorator_FocusScope::InitializeMemory(UHTNComponent& OwnerComp, uint8* NodeMemory, const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const
{
	FNodeMemory* const Memory = CastInstanceNodeMemory<FNodeMemory>(NodeMemory);
	*Memory = {};
}

void UHTNDecorator_FocusScope::OnExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory)
{
	FNodeMemory* const Memory = CastInstanceNodeMemory<FNodeMemory>(NodeMemory);
	if (AAIController* const Controller = OwnerComp.GetAIOwner())
	{
		// Save previous focus
		if (AActor* const PreviousFocusActor = Controller->GetFocusActorForPriority(FocusPriority))
		{
			Memory->OldFocus.Set<TWeakObjectPtr<AActor>>(PreviousFocusActor);
		}
		else
		{
			Memory->OldFocus.Set<FVector>(Controller->GetFocalPointForPriority(FocusPriority));
		}

		// Set new focus
		if (bSetNewFocus)
		{
			if (UBlackboardComponent* const Blackboard = OwnerComp.GetBlackboardComponent())
			{
				if (FocusTarget.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
				{
					AActor* const FocusActor = Cast<AActor>(Blackboard->GetValue<UBlackboardKeyType_Object>(FocusTarget.GetSelectedKeyID()));
					Controller->SetFocus(FocusActor);
				}
				else if (FocusTarget.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
				{
					const FVector FocalPoint = Blackboard->GetValue<UBlackboardKeyType_Vector>(FocusTarget.GetSelectedKeyID());
					Controller->SetFocalPoint(FocalPoint);
				}
			}
		}
	}
}

void UHTNDecorator_FocusScope::OnExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult Result)
{
	FNodeMemory* const Memory = CastInstanceNodeMemory<FNodeMemory>(NodeMemory);
	if (AAIController* const Controller = OwnerComp.GetAIOwner())
	{
		if (Memory->OldFocus.IsType<TWeakObjectPtr<AActor>>())
		{
			const TWeakObjectPtr<AActor> PreviousFocusActor = Memory->OldFocus.Get<TWeakObjectPtr<AActor>>();
			Controller->SetFocus(PreviousFocusActor.Get(), FocusPriority);
		}
		else
		{
			const FVector PreviousFocalPoint = Memory->OldFocus.Get<FVector>();
			Controller->SetFocalPoint(PreviousFocalPoint, FocusPriority);
		}
	}
}
