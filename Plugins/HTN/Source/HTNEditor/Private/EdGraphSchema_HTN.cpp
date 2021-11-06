// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "EdGraphSchema_HTN.h"

#include "HTNGraphNode_Root.h"
#include "EdGraph/EdGraph.h"
#include "Modules/ModuleManager.h"

#include "HTNEditorModule.h"
#include "HTNEditor.h"
#include "HTNConnectionDrawingPolicy.h"
#include "HTNNode.h"
#include "HTNTask.h"
#include "HTNDecorator.h"
#include "HTNGraphNode_Decorator.h"
#include "HTNGraphNode_TwoBranches.h"
#include "HTNGraphNode_Service.h"
#include "HTNService.h"
#include "Nodes/HTNNode_SubNetwork.h"
#include "Nodes/HTNNode_TwoBranches.h"

#define LOCTEXT_NAMESPACE "HTNEditorGraphSchema"

int32 UEdGraphSchema_HTN::CurrentCacheRefreshID = 0;

namespace
{
	template<typename T>
	T* MakeNode(UEdGraph& Graph)
	{
		FGraphNodeCreator<T> Creator(Graph);
		T* const Node = Creator.CreateNode();
		Creator.Finalize();

		return Node;
	}
}

void UEdGraphSchema_HTN::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	UHTNGraphNode_Root* const Root = MakeNode<UHTNGraphNode_Root>(Graph);
	SetNodeMetaData(Root, FNodeMetadata::DefaultGraphNode);
}

const FPinConnectionResponse UEdGraphSchema_HTN::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorSameNode", "Both are on the same node"));
	}

	if (PinA->Direction == EGPD_Input && PinB->Direction == EGPD_Input)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorInputToInput", "Can't connect input node to input node"));
	}
	else if (PinB->Direction == EGPD_Output && PinA->Direction == EGPD_Output)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorOutputToOutput", "Can't connect output node to output node"));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("PinConnect", "Connect nodes"));
}

void UEdGraphSchema_HTN::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const auto MakeCreateNodeAction = [OwnerOfTemporaries = ContextMenuBuilder.OwnerOfTemporaries]
	(
		FGraphActionListBuilderBase& Builder, 
		const FGraphNodeClassData& RuntimeNodeClassData, 
		const TSubclassOf<UHTNGraphNode> GraphNodeClass = UHTNGraphNode::StaticClass()
	)
	{
		const FText NodeTypeName = FText::FromString(FName::NameToDisplayString(RuntimeNodeClassData.ToString(), false));
		const TSharedPtr<FAISchemaAction_NewNode> AddOpAction = AddNewNodeAction(Builder, RuntimeNodeClassData.GetCategory(), NodeTypeName, FText::GetEmpty());

		UHTNGraphNode* const TemplateNode = NewObject<UHTNGraphNode>(OwnerOfTemporaries, GraphNodeClass);
		TemplateNode->ClassData = RuntimeNodeClassData;
		AddOpAction->NodeTemplate = TemplateNode;

		return AddOpAction;
	};

	FCategorizedGraphActionListBuilder TasksBuilder(TEXT("Tasks"));
	
	TArray<FGraphNodeClassData> NodeClasses;
	IHTNEditorModule::Get().GetClassCache()->GatherClasses(UHTNStandaloneNode::StaticClass(), NodeClasses);
	for (FGraphNodeClassData& NodeClassData : NodeClasses)
	{
		const UClass& Class = *NodeClassData.GetClass();
		if (Class.IsChildOf(UHTNTask::StaticClass()))
		{
			MakeCreateNodeAction(TasksBuilder, NodeClassData, UHTNGraphNode::StaticClass());
		}
		else if (Class.IsChildOf(UHTNNode_TwoBranches::StaticClass()))
		{
			MakeCreateNodeAction(ContextMenuBuilder, NodeClassData, UHTNGraphNode_TwoBranches::StaticClass());
		}
		else
		{
			MakeCreateNodeAction(ContextMenuBuilder, NodeClassData, UHTNGraphNode::StaticClass());
		}
	}
	
	ContextMenuBuilder.Append(TasksBuilder);
}

const FPinConnectionResponse UEdGraphSchema_HTN::CanMergeNodes(const UEdGraphNode* A, const UEdGraphNode* B) const
{
	if (A == B)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Both are the same node"));
	}

	if (FHTNEditor::IsPIESimulating())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Can't edit during a Play in Editor session."));
	}
	
	static const auto CanMergeSubNodeIntoNode = [](TSubclassOf<UHTNGraphNode> SubNodeType, const UEdGraphNode* Target) -> bool
	{
		if (const UHTNGraphNode* const GraphNode = Cast<UHTNGraphNode>(Target))
		{
			if (GraphNode->IsA(SubNodeType))
			{
				return true;
			}

			if (GraphNode->IsA(UHTNGraphNode_Root::StaticClass()))
			{
				return true;
			}

			return GraphNode->NodeInstance && GraphNode->NodeInstance->IsA(UHTNStandaloneNode::StaticClass());
		}

		return false;
	};
	
	if (
		(Cast<UHTNGraphNode_Decorator>(A) && CanMergeSubNodeIntoNode(UHTNGraphNode_Decorator::StaticClass(), B)) ||
		(Cast<UHTNGraphNode_Service>(A) && CanMergeSubNodeIntoNode(UHTNGraphNode_Service::StaticClass(), B))
	)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
}

FConnectionDrawingPolicy* UEdGraphSchema_HTN::CreateConnectionDrawingPolicy(
	int32 InBackLayerID, int32 InFrontLayerID,
	float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj
) const
{
	return new FHTNConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

bool UEdGraphSchema_HTN::IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const { return InVisualizationCacheID != CurrentCacheRefreshID; }
int32 UEdGraphSchema_HTN::GetCurrentVisualizationCacheID() const { return CurrentCacheRefreshID; }
void UEdGraphSchema_HTN::ForceVisualizationCacheClear() const { ++CurrentCacheRefreshID; }

void UEdGraphSchema_HTN::GetSubNodeClasses(int32 SubNodeFlags, TArray<FGraphNodeClassData>& ClassData, UClass*& GraphNodeClass) const
{
	const TSharedPtr<FGraphNodeClassHelper> ClassCache = IHTNEditorModule::Get().GetClassCache();
	check(ClassCache);

	switch (StaticCast<EHTNSubNodeType>(SubNodeFlags))
	{
	case EHTNSubNodeType::Decorator:
		ClassCache->GatherClasses(UHTNDecorator::StaticClass(), ClassData);
		GraphNodeClass = UHTNGraphNode_Decorator::StaticClass();
		break;
	case EHTNSubNodeType::Service:
		ClassCache->GatherClasses(UHTNService::StaticClass(), ClassData);
		GraphNodeClass = UHTNGraphNode_Service::StaticClass();
		break;
	default:
		checkNoEntry();
	}
}

#undef LOCTEXT_NAMESPACE
