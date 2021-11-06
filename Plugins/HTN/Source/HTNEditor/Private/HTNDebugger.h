// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Tickable.h"
#include "HTN.h"
#include "HTNGraphNode_Root.h"
#include "HTNComponent.h"

class FHTNDebugger : public FTickableGameObject, public TSharedFromThis<FHTNDebugger>
{
public:
	FHTNDebugger();
	~FHTNDebugger();

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FHTNEditorTickHelper, STATGROUP_Tickables); }

	static bool IsPlaySessionPaused();
	static bool IsPlaySessionRunning();
	static void PausePlaySession();
	static void ResumePlaySession();
	static void StopPlaySession();
	
	bool IsDebuggerReady() const;
	bool IsDebuggerRunning() const;
	
	void Setup(UHTN* InHTNAsset, TSharedRef<class FHTNEditor> InEditorOwner);
	void Refresh();
	void ClearDebuggerState();
	void OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection);
	void OnBreakpointAdded(UHTNGraphNode* GraphNode);
	void OnBreakpointRemoved(UHTNGraphNode* GraphNode);
	
	void OnObjectSelected(UObject* Object);
	void OnInstanceSelectedInDropdown(TWeakObjectPtr<UHTNComponent> SelectedComponent);
	void OnPlanExecutionStarted(const UHTNComponent& OwnerComp, const TSharedPtr<FHTNPlan>& Plan);
	void OnBeginPIE(bool bIsSimulating);
	void OnEndPIE(bool bIsSimulating);
	void OnPausePIE(bool bIsSimulating);

	void FindMatchingRunningHTNComponent();
	bool IsCompatible(const class UHTNComponent& HTNComponent);

	TSharedRef<class SWidget> GetActorsMenu() const;
	FText GetCurrentActorDescription() const;
	FText GetActorDescription(const UHTNComponent& HTNComponent) const;
	FText HandleGetDebugKeyValue(const FName& InKeyName, bool bUseCurrentState) const;
	bool IsShowingCurrentState() const;
	FORCEINLINE UHTNComponent* GetDebuggedComponent() const { return DebuggedHTNComponent.Get(); };
	
private:
	void SetDebuggedComponent(TWeakObjectPtr<UHTNComponent> NewDebuggedComponent);
	void UpdateDebugFlags();
	void CacheRootNode();
	UHTNGraphNode* GetGraphNode(const FHTNPlan& Plan, const FHTNPlanStepID& StepID) const;

	TWeakPtr<class FHTNEditor> EditorOwner;
	TWeakObjectPtr<UHTN> HTNAsset;
	TWeakObjectPtr<UHTNGraphNode_Root> AssetRootNode;
	TWeakObjectPtr<UHTNGraphNode> CurrentlySelectedGraphNode;
	
	// The component being debugged
	TWeakObjectPtr<UHTNComponent> DebuggedHTNComponent;
	// an id of the current step id
	int32 ActiveDebugStepIndex;
	
	uint8 bIsPIEActive : 1;
};
