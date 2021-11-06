// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "AssetTypeActions_HTN.h"
#include "HTN.h"
#include "HTNEditorModule.h"
#include "AIModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_HTN"

FText FAssetTypeActions_HTN::GetName() const
{
	return LOCTEXT("HTN", "Hierarchical Task Network");
}

FColor FAssetTypeActions_HTN::GetTypeColor() const
{
	return FColor::Orange;
}

UClass* FAssetTypeActions_HTN::GetSupportedClass() const
{
	return UHTN::StaticClass();
}

uint32 FAssetTypeActions_HTN::GetCategories()
{
	return IAIModule::Get().GetAIAssetCategoryBit();
}

void FAssetTypeActions_HTN::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	IHTNEditorModule& EditorModule = IHTNEditorModule::Get();
	
	for (UObject* const Obj : InObjects)
	{
		if (UHTN* const HTN = Cast<UHTN>(Obj))
		{
			EditorModule.CreateHTNEditor(Mode, EditWithinLevelEditor, HTN);
		}
	}
}

#undef LOCTEXT_NAMESPACE
