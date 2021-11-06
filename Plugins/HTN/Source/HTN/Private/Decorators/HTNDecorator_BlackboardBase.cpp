// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "Decorators/HTNDecorator_BlackboardBase.h"

UHTNDecorator_BlackboardBase::UHTNDecorator_BlackboardBase(const FObjectInitializer& Initializer) : Super(Initializer)
{
	bNotifyExecutionStart = true;
	bNotifyExecutionFinish = true;
}

void UHTNDecorator_BlackboardBase::InitializeFromAsset(UHTN& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* const BBAsset = GetBlackboardAsset())
	{
		BlackboardKey.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		UE_LOG(LogHTN, Warning, TEXT("Can't initialize %s due to missing blackboard data."), *GetNodeName());
		BlackboardKey.InvalidateResolvedKey();
	}
}

#if WITH_EDITOR

FName UHTNDecorator_BlackboardBase::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.Blackboard.Icon");
}

#endif

void UHTNDecorator_BlackboardBase::OnExecutionStart(UHTNComponent& OwnerComp, uint8* NodeMemory)
{
	if (UBlackboardComponent* const BlackboardComp = OwnerComp.GetBlackboardComponent())
	{
		const FBlackboard::FKey KeyID = BlackboardKey.GetSelectedKeyID();
		BlackboardComp->RegisterObserver(KeyID, this,
			FOnBlackboardChangeNotification::CreateUObject(this, &UHTNDecorator_BlackboardBase::OnBlackboardKeyValueChange)
		);
	}
}

void UHTNDecorator_BlackboardBase::OnExecutionFinish(UHTNComponent& OwnerComp, uint8* NodeMemory, EHTNNodeResult Result)
{
	if (UBlackboardComponent* const BlackboardComp = OwnerComp.GetBlackboardComponent())
	{
		BlackboardComp->UnregisterObserversFrom(this);
	}
}

EBlackboardNotificationResult UHTNDecorator_BlackboardBase::OnBlackboardKeyValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID)
{
	return Cast<UHTNComponent>(Blackboard.GetBrainComponent()) ? 
		EBlackboardNotificationResult::ContinueObserving :
		EBlackboardNotificationResult::RemoveObserver;
}
