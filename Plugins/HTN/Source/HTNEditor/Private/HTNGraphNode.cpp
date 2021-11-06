// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "HTNGraphNode.h"

#include "ToolMenuDelegates.h"
#include "ToolMenu.h"
#include "SGraphEditorActionMenuAI.h"

#include "EdGraphSchema_HTN.h"
#include "HTNGraph.h"
#include "HTNGraphPinCategories.h"
#include "HTNNode.h"
#include "HTNTypes.h"
#include "HTNGraphNode_Decorator.h"
#include "HTNGraphNode_Service.h"
#include "HTNDecorator.h"
#include "HTNService.h"

#define LOCTEXT_NAMESPACE "HTNGraph"

UHTNGraphNode::UHTNGraphNode(const FObjectInitializer& Initializer) : Super(Initializer),
	bDebuggerMarkCurrentlyActive(false),
	bHasBreakpoint(false),
	bIsBreakpointEnabled(false)
{}

void UHTNGraphNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, FHTNGraphPinCategories::MultipleNodesAllowed, TEXT("In"));
	CreatePin(EGPD_Output, FHTNGraphPinCategories::MultipleNodesAllowed, TEXT("Out"));
}

FText UHTNGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (UHTNNode* const Node = Cast<UHTNNode>(NodeInstance))
	{
		return FText::FromString(Node->GetNodeName());
	}
	
	if (!ClassData.GetClassName().IsEmpty())
	{
		FString StoredClassName = ClassData.GetClassName();
		StoredClassName.RemoveFromEnd(TEXT("_C"));
		return FText::Format(NSLOCTEXT("AIGraph", "NodeClassError", "Class {0} not found, make sure it's saved!"), FText::FromString(StoredClassName));
	}

	return Super::GetNodeTitle(TitleType);
}

bool UHTNGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const
{
	return Cast<UEdGraphSchema_HTN>(DesiredSchema) != nullptr;
}

void UHTNGraphNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	// TODO move this to subclasses for standalone nodes
	AddContextMenuActionsForAddingDecorators(Menu, TEXT("HTNGraphNode"), Context);
	AddContextMenuActionsForAddingServices(Menu, TEXT("HTNGraphNode"), Context);
}

FText UHTNGraphNode::GetDescription() const
{
	if (const UHTNNode* const HTNNode = Cast<const UHTNNode>(NodeInstance))
	{
		return FText::FromString(HTNNode->GetStaticDescription());
	}

	return Super::GetDescription();
}

void UHTNGraphNode::InitializeInstance()
{
	if (UHTNNode* const HTNNode = Cast<UHTNNode>(NodeInstance))
	{
		if (UHTN* const HTNAsset = HTNNode->GetTypedOuter<UHTN>())
		{
			HTNNode->InitializeFromAsset(*HTNAsset);
			//HTNNode->OnNodeCreated();
		}
	}
}

void UHTNGraphNode::OnSubNodeAdded(UAIGraphNode* SubNode)
{
	if (UHTNGraphNode_Decorator* const DecoratorNode = Cast<UHTNGraphNode_Decorator>(SubNode))
	{
		Decorators.Add(DecoratorNode);
	}
	else if (UHTNGraphNode_Service* const ServiceNode = Cast<UHTNGraphNode_Service>(SubNode))
	{
		Services.Add(ServiceNode);
	}
}

void UHTNGraphNode::OnSubNodeRemoved(UAIGraphNode* SubNode)
{
	if (UHTNGraphNode_Decorator* const DecoratorNode = Cast<UHTNGraphNode_Decorator>(SubNode))
	{
		Decorators.Remove(DecoratorNode);
	}
	else if (UHTNGraphNode_Service* const ServiceNode = Cast<UHTNGraphNode_Service>(SubNode))
	{
		Services.Remove(ServiceNode);
	}
}

void UHTNGraphNode::RemoveAllSubNodes()
{
	Super::RemoveAllSubNodes();
	Decorators.Reset();
	Services.Reset();
}

int32 UHTNGraphNode::FindSubNodeDropIndex(UAIGraphNode* SubNode) const
{
	const int32 SubIndex = SubNodes.IndexOfByKey(SubNode) + 1;

	UHTNGraphNode_Decorator* const DecoratorNode = Cast<UHTNGraphNode_Decorator>(SubNode);
	const int32 DecoratorIndex = (DecoratorNode ? Decorators.IndexOfByKey(DecoratorNode) : -1) + 1;

	UHTNGraphNode_Service* const ServiceNode = Cast<UHTNGraphNode_Service>(SubNode);
	const int32 ServiceIndex = (ServiceNode ? Services.IndexOfByKey(ServiceNode) : -1) + 1;

	const int32 CombinedIdx = (SubIndex & 0xff) | ((DecoratorIndex & 0xff) << 8) | ((ServiceIndex & 0xff) << 16);
	return CombinedIdx;
}

void UHTNGraphNode::InsertSubNodeAt(UAIGraphNode* SubNode, int32 DropIndex)
{
	const int32 SubNodeIndex = (DropIndex & 0xff) - 1;
	const int32 DecoratorIndex = ((DropIndex >> 8) & 0xff) - 1;
	const int32 ServiceIndex = ((DropIndex >> 16) & 0xff) - 1;

	if (SubNodeIndex >= 0)
	{
		SubNodes.Insert(SubNode, SubNodeIndex);
	}
	else
	{
		SubNodes.Add(SubNode);
	}

	if (UHTNGraphNode_Decorator* const DecoratorNode = Cast<UHTNGraphNode_Decorator>(SubNode))
	{
		if (DecoratorIndex >= 0)
		{
			Decorators.Insert(DecoratorNode, DecoratorIndex);
		}
		else
		{
			Decorators.Add(DecoratorNode);
		}
	}
	else if (UHTNGraphNode_Service* const ServiceNode = Cast<UHTNGraphNode_Service>(SubNode))
	{
		if (ServiceIndex >= 0)
		{
			Services.Insert(ServiceNode, ServiceIndex);
		}
		else
		{
			Services.Add(ServiceNode);
		}
	}
}

#if WITH_EDITOR
void UHTNGraphNode::PostEditUndo()
{
	Super::PostEditUndo();

	if (UHTNGraphNode* const MyParentNode = Cast<UHTNGraphNode>(ParentNode))
	{
		if (UHTNDecorator* const Decorator = Cast<UHTNDecorator>(NodeInstance))
		{
			UHTNGraphNode_Decorator* const DecoratorGraphNode = Cast<UHTNGraphNode_Decorator>(this);
			check(DecoratorGraphNode);
			MyParentNode->Decorators.AddUnique(DecoratorGraphNode);
		}
		else if (UHTNService* const Service = Cast<UHTNService>(NodeInstance))
		{
			UHTNGraphNode_Service* const ServiceGraphNode = Cast<UHTNGraphNode_Service>(this);
			check(ServiceGraphNode);
			MyParentNode->Services.AddUnique(ServiceGraphNode);
		}
	}
}
#endif

UHTNGraph* UHTNGraphNode::GetHTNGraph() { return CastChecked<UHTNGraph>(GetGraph()); }

FName UHTNGraphNode::GetIconName() const
{
	UHTNNode* const Node = Cast<UHTNNode>(NodeInstance);
	return Node ? Node->GetNodeIconName() : FName("BTEditor.Graph.BTNode.Icon");
}

void UHTNGraphNode::ClearBreakpoints()
{
	bHasBreakpoint = false;
	bIsBreakpointEnabled = false;
}

void UHTNGraphNode::ClearDebugFlags()
{
	DebuggerPlanEntries.Reset();
	bDebuggerMarkCurrentlyActive = false;
	bDebuggerMarkCurrentlyExecuting = false;
}

bool UHTNGraphNode::IsInFutureOfDebuggedPlan() const
{
	return DebuggerPlanEntries.ContainsByPredicate([&](const FDebuggerPlanEntry& ToEntry) -> bool {
		return ToEntry.bIsInFutureOfPlan;
	});
}

void UHTNGraphNode::AddContextMenuActionsForAddingDecorators(UToolMenu* Menu, const FName SectionName, UGraphNodeContextMenuContext* Context) const
{
	FToolMenuSection& Section = Menu->FindOrAddSection(SectionName);
	Section.AddSubMenu(
		TEXT("AddDecorator"),
		LOCTEXT("AddDecorator", "Add Decorator..."),
		LOCTEXT("AddDecoratorTooltip", "Adds new decorator as a subnode"),
		FNewToolMenuDelegate::CreateUObject(this, &UHTNGraphNode::CreateAddDecoratorSubMenu, 
			const_cast<UEdGraph*>(UNWRAP_TOBJECT_PTR(Context->Graph))
		)
	);
}

void UHTNGraphNode::CreateAddDecoratorSubMenu(UToolMenu* Menu, UEdGraph* Graph) const
{
	const TSharedRef<SGraphEditorActionMenuAI> Widget =
		SNew(SGraphEditorActionMenuAI)
		.GraphObj(Graph)
		.GraphNode(const_cast<UHTNGraphNode*>(this))
		.SubNodeFlags(StaticCast<int32>(EHTNSubNodeType::Decorator))
		.AutoExpandActionMenu(true);

	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("Section"));
	Section.AddEntry(FToolMenuEntry::InitWidget(TEXT("DecoratorWidget"), Widget, FText(), true));
}

void UHTNGraphNode::AddContextMenuActionsForAddingServices(UToolMenu* Menu, const FName SectionName, UGraphNodeContextMenuContext* Context) const
{
	FToolMenuSection& Section = Menu->FindOrAddSection(SectionName);
	Section.AddSubMenu(
		TEXT("AddService"),
		LOCTEXT("AddService", "Add Service..."),
		LOCTEXT("AddServiceTooltip", "Adds new service as a subnode"),
		FNewToolMenuDelegate::CreateUObject(this, &UHTNGraphNode::CreateAddServiceSubMenu, 
			const_cast<UEdGraph*>(UNWRAP_TOBJECT_PTR(Context->Graph))
		)
	);
}

void UHTNGraphNode::CreateAddServiceSubMenu(UToolMenu* Menu, UEdGraph* Graph) const
{
	const TSharedRef<SGraphEditorActionMenuAI> Widget =
		SNew(SGraphEditorActionMenuAI)
		.GraphObj(Graph)
		.GraphNode(const_cast<UHTNGraphNode*>(this))
		.SubNodeFlags(StaticCast<int32>(EHTNSubNodeType::Service))
		.AutoExpandActionMenu(true);

	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("Section"));
	Section.AddEntry(FToolMenuEntry::InitWidget(TEXT("ServiceWidget"), Widget, FText(), true));
}

#undef LOCTEXT_NAMESPACE
