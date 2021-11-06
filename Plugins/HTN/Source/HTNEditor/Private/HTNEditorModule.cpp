// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "HTNEditorModule.h"

#include "AssetToolsModule.h"
#include "EdGraphUtilities.h"

#include "AssetTypeActions_HTN.h"
#include "HTNEditor.h"
#include "HTNGraphNode.h"
#include "SGraphNode_HTN.h"
#include "HTNNode.h"
#include "DetailCustomizations/HTNBlackboardSelectorDetails.h"
#include "DetailCustomizations/HTNBlackboardDecoratorDetails.h"
#include "DetailCustomizations/WorldstateSetValueContainerDetails.h"

#define LOCTEXT_NAMESPACE "HTNEditorModule"

const FName HTNEditorAppIdentifier = FName(TEXT("HTNEditorApp"));

namespace
{
	class FGraphPanelNodeFactory_HTN : public FGraphPanelNodeFactory
	{
		virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override
		{
			if (UHTNGraphNode* const HTNNode = Cast<UHTNGraphNode>(Node))
			{
				return SNew(SGraphNode_HTN, HTNNode);
			}

			return nullptr;
		}
	};

	// Makes sure that FBlackboardKeySelector|s only get customized with the HTN module's property customizer if they are on an htn node.
	class PropertyTypeIdentifier_HTNBlackboardKeySelectors : public IPropertyTypeIdentifier
	{
		virtual bool IsPropertyTypeCustomized(const IPropertyHandle& PropertyHandle) const override
		{
			TArray<UObject*> Objects;
			PropertyHandle.GetOuterObjects(Objects);
			return Algo::FindByPredicate(Objects, [](UObject* Object) -> bool {
				return Object && (Object->IsA<UHTNNode>() || Object->GetTypedOuter<UHTNNode>());
			}) != nullptr;
		}
	};
}

// Editor module implementation
class FHTNEditorModule : public IHTNEditorModule
{
public:
	FHTNEditorModule() :
		BlackboardKeySelectorPropertyTypeIdentifier(MakeShared<PropertyTypeIdentifier_HTNBlackboardKeySelectors>())
	{}

	// IHasMenuExtensibility interface
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }

	// IHasToolBarExtensibility interface
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

	// Begin IModuleInterface
	virtual void StartupModule() override
	{
		MenuExtensibilityManager = MakeShared<FExtensibilityManager>();
		ToolBarExtensibilityManager = MakeShared<FExtensibilityManager>();

		FEdGraphUtilities::RegisterVisualNodeFactory(GraphNodeWidgetFactory = MakeShared<FGraphPanelNodeFactory_HTN>());

		// Register asset type actions
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		const auto RegisterAssetTypeAction = [&](TSharedRef<IAssetTypeActions> Action)
		{
			AssetTools.RegisterAssetTypeActions(Action);
			CreatedAssetTypeActions.Add(Action);
		};
		RegisterAssetTypeAction(MakeShared<FAssetTypeActions_HTN>());

		// Register the details customizer
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout("BlackboardKeySelector", 
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FHTNBlackboardSelectorDetails::MakeInstance), 
			BlackboardKeySelectorPropertyTypeIdentifier
		);
		PropertyModule.RegisterCustomPropertyTypeLayout("WorldstateSetValueContainer", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FWorldstateSetValueContainerDetails::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("HTNDecorator_Blackboard", FOnGetDetailCustomizationInstance::CreateStatic(&FHTNBlackboardDecoratorDetails::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	virtual void ShutdownModule() override
	{
		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();

		if (GraphNodeWidgetFactory.IsValid())
		{
			FEdGraphUtilities::UnregisterVisualNodeFactory(GraphNodeWidgetFactory);
			GraphNodeWidgetFactory.Reset();
		}	

		// Unregister asset type actions
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
			for (TSharedPtr<IAssetTypeActions>& Action : CreatedAssetTypeActions)
			{
				if (Action.IsValid())
				{
					AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
				}
			}
		}
		CreatedAssetTypeActions.Empty();

		// Unregister the details customizations
		if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomPropertyTypeLayout("BlackboardKeySelector", BlackboardKeySelectorPropertyTypeIdentifier);
			PropertyModule.UnregisterCustomPropertyTypeLayout("WorldstateSetValueContainer");
			PropertyModule.UnregisterCustomPropertyTypeLayout("HTNDecorator_Blackboard");
			PropertyModule.NotifyCustomizationModuleChanged();
		}
	}
	// End IModuleInterface

	
	// Begin IHTNEditorModule interface
	virtual TSharedRef<IHTNEditor> CreateHTNEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UHTN* HTN) override
	{
		if (!ClassCache.IsValid())
		{
			ClassCache = MakeShareable(new FGraphNodeClassHelper(UHTNNode::StaticClass()));
		}
		
		TSharedRef<FHTNEditor> NewHTNEditor = MakeShared<FHTNEditor>();
		NewHTNEditor->InitHTNEditor(Mode, InitToolkitHost, HTN);
		return NewHTNEditor;
	}

	virtual TSharedPtr<FGraphNodeClassHelper> GetClassCache() override { return ClassCache; }
	// End IHTNEditorModule interface

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	TArray<TSharedPtr<IAssetTypeActions>> CreatedAssetTypeActions;
	TSharedPtr<IPropertyTypeIdentifier> BlackboardKeySelectorPropertyTypeIdentifier;
	
	TSharedPtr<FGraphNodeClassHelper> ClassCache;

	static TSharedPtr<FGraphPanelNodeFactory_HTN> GraphNodeWidgetFactory;
};

TSharedPtr<FGraphPanelNodeFactory_HTN> FHTNEditorModule::GraphNodeWidgetFactory;

IMPLEMENT_GAME_MODULE(FHTNEditorModule, HTNEditor);

#undef LOCTEXT_NAMESPACE