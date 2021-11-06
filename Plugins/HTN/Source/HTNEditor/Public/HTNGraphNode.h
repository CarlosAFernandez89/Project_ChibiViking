// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AIGraphNode.h"
#include "HTNGraphNode.generated.h"

UCLASS()
class UHTNGraphNode : public UAIGraphNode
{
	GENERATED_BODY()

public:

	UPROPERTY()
	TArray<class UHTNGraphNode_Decorator*> Decorators;

	UPROPERTY()
	TArray<class UHTNGraphNode_Service*> Services;

	UHTNGraphNode(const FObjectInitializer& Initializer);
	
	virtual void AllocateDefaultPins() override;
	//virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanCreateUnderSpecifiedSchema(const class UEdGraphSchema* DesiredSchema) const override;
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	
	virtual FText GetDescription() const override;
	virtual void InitializeInstance() override;
	
	virtual void OnSubNodeAdded(UAIGraphNode* SubNode) override;
	virtual void OnSubNodeRemoved(UAIGraphNode* SubNode) override;
	virtual void RemoveAllSubNodes() override;
	virtual int32 FindSubNodeDropIndex(UAIGraphNode* SubNode) const override;
	virtual void InsertSubNodeAt(UAIGraphNode* SubNode, int32 DropIndex) override;

#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	virtual class UHTNGraph* GetHTNGraph();
	
	virtual FName GetIconName() const;
	
	virtual bool CanPlaceBreakpoints() const { return true; }
	void ClearBreakpoints();
	void ClearDebugFlags();
	bool IsInFutureOfDebuggedPlan() const;

	struct FDebuggerPlanEntry
	{
		UHTNGraphNode* PreviousNode;
		int32 ExecutionIndex;
		int32 DepthInPlan;
		bool bIsInFutureOfPlan : 1;
		bool bIsExecuting : 1;
	};
	
	TArray<FDebuggerPlanEntry> DebuggerPlanEntries;
	// like bDebuggerMarkCurrentlyExecuting, but includes nodes with active sublevels (e.g. subnetwork nodes)
	uint8 bDebuggerMarkCurrentlyActive : 1;
	uint8 bDebuggerMarkCurrentlyExecuting : 1;
	uint8 bHasBreakpoint : 1;
	uint8 bIsBreakpointEnabled : 1;

protected:

	// Adds the "Add Decorator..." option in the context menu of the node
	void AddContextMenuActionsForAddingDecorators(class UToolMenu* Menu, const FName SectionName, class UGraphNodeContextMenuContext* Context) const;
	void CreateAddDecoratorSubMenu(class UToolMenu* Menu, UEdGraph* Graph) const;

	// Adds the "Add Service..." option in the context menu of the node
	void AddContextMenuActionsForAddingServices(class UToolMenu* Menu, const FName SectionName, class UGraphNodeContextMenuContext* Context) const;
	void CreateAddServiceSubMenu(class UToolMenu* Menu, UEdGraph* Graph) const;
};