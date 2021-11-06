// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "SHTNBlackboardEditor.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BehaviorTree/BlackboardData.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "ScopedTransaction.h"
#include "SGraphActionMenu.h"
#include "Runtime/Launch/Resources/Version.h"

#include "HTNCommands.h"

#define LOCTEXT_NAMESPACE "SHTNBlackboardEditor"

DEFINE_LOG_CATEGORY(LogBlackboardEditor);

void SHTNBlackboardEditor::Construct(const FArguments& InArgs, TSharedRef<FUICommandList> InCommandList, UBlackboardData* InBlackboardData)
{
	OnEntrySelected = InArgs._OnEntrySelected;
	OnGetDebugKeyValue = InArgs._OnGetDebugKeyValue;
	OnIsDebuggerReady = InArgs._OnIsDebuggerReady;
	OnIsDebuggerPaused = InArgs._OnIsDebuggerPaused;
	OnGetDebugTimeStamp = InArgs._OnGetDebugTimeStamp;
	OnGetDisplayCurrentState = InArgs._OnGetDisplayCurrentState;
	OnIsBlackboardModeActive = InArgs._OnIsBlackboardModeActive;

	TSharedRef<FUICommandList> CommandList = MakeShareable(new FUICommandList);

	CommandList->MapAction(
		FHTNBlackboardCommands::Get().DeleteEntry,
		FExecuteAction::CreateSP(this, &SHTNBlackboardEditor::HandleDeleteEntry),
		FCanExecuteAction::CreateSP(this, &SHTNBlackboardEditor::CanDeleteEntry)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &SHTNBlackboardEditor::HandleRenameEntry),
		FCanExecuteAction::CreateSP(this, &SHTNBlackboardEditor::CanRenameEntry)
	);

	InCommandList->Append(CommandList);

	SHTNBlackboardView::Construct(
		SHTNBlackboardView::FArguments()
		.OnEntrySelected(InArgs._OnEntrySelected)
		.OnGetDebugKeyValue(InArgs._OnGetDebugKeyValue)
		.OnGetDisplayCurrentState(InArgs._OnGetDisplayCurrentState)
		.OnIsDebuggerReady(InArgs._OnIsDebuggerReady)
		.OnIsDebuggerPaused(InArgs._OnIsDebuggerPaused)
		.OnGetDebugTimeStamp(InArgs._OnGetDebugTimeStamp)
		.OnBlackboardKeyChanged(InArgs._OnBlackboardKeyChanged)
		.IsReadOnly(false),
		CommandList,
		InBlackboardData
	);
}

void SHTNBlackboardEditor::FillContextMenu(FMenuBuilder& MenuBuilder) const
{
	if(!IsDebuggerActive() && HasSelectedItems())
	{
		MenuBuilder.AddMenuEntry(FHTNBlackboardCommands::Get().DeleteEntry);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename, NAME_None, LOCTEXT("Rename", "Rename"), LOCTEXT("Rename_Tooltip", "Renames this blackboard entry.") );
	}
}

void SHTNBlackboardEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder) const
{
	ToolbarBuilder.AddComboButton(
		FUIAction(
			FExecuteAction(),
			FCanExecuteAction::CreateSP(this, &SHTNBlackboardEditor::CanCreateNewEntry)
		), 
		FOnGetContent::CreateSP(this, &SHTNBlackboardEditor::HandleCreateNewEntryMenu),
		LOCTEXT( "New_Label", "New Key" ),
		LOCTEXT( "New_ToolTip", "Create a new blackboard entry" ),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "BTEditor.Blackboard.NewEntry")
	);			
}

TSharedPtr<FExtender> SHTNBlackboardEditor::GetToolbarExtender(TSharedRef<FUICommandList> ToolkitCommands) const
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtender->AddToolBarExtension("Debugging", EExtensionHook::Before, ToolkitCommands, FToolBarExtensionDelegate::CreateSP( this, &SHTNBlackboardEditor::FillToolbar ));

	return ToolbarExtender;
}

void SHTNBlackboardEditor::HandleDeleteEntry()
{
	if (BlackboardData == nullptr)
	{
		UE_LOG(LogBlackboardEditor, Error, TEXT("Trying to delete an entry from a blackboard while no Blackboard Asset is set!"));
		return;
	}

	if(!IsDebuggerActive())
	{
		bool bIsInherited = false;
		FBlackboardEntry* BlackboardEntry = GetSelectedEntry(bIsInherited);
		if(BlackboardEntry != nullptr && !bIsInherited)
		{
			const FScopedTransaction Transaction(LOCTEXT("BlackboardEntryDeleteTransaction", "Delete Blackboard Entry"));
			BlackboardData->SetFlags(RF_Transactional);
			BlackboardData->Modify();
		
			for(int32 ItemIndex = 0; ItemIndex < BlackboardData->Keys.Num(); ItemIndex++)
			{
				if(BlackboardEntry == &BlackboardData->Keys[ItemIndex])
				{
					BlackboardData->Keys.RemoveAt(ItemIndex);
					break;
				}
			}

			GraphActionMenu->RefreshAllActions(true);
			OnBlackboardKeyChanged.ExecuteIfBound(BlackboardData, nullptr);

			// signal de-selection
			if(OnEntrySelected.IsBound())
			{
				OnEntrySelected.Execute(nullptr, false);
			}
		}
	}
}

void SHTNBlackboardEditor::HandleRenameEntry()
{
	if(!IsDebuggerActive())
	{
		GraphActionMenu->OnRequestRenameOnActionNode();
	}
}

bool SHTNBlackboardEditor::CanDeleteEntry() const
{
	const bool bModeActive = OnIsBlackboardModeActive.IsBound() && OnIsBlackboardModeActive.Execute();

	if(!IsDebuggerActive() && bModeActive)
	{
		bool bIsInherited = false;
		FBlackboardEntry* BlackboardEntry = GetSelectedEntry(bIsInherited);
		if(BlackboardEntry != nullptr)
		{
			return !bIsInherited;
		}
	}

	return false;
}

bool SHTNBlackboardEditor::CanRenameEntry() const
{
	const bool bModeActive = OnIsBlackboardModeActive.IsBound() && OnIsBlackboardModeActive.Execute();

	if(!IsDebuggerActive() && bModeActive)
	{
		bool bIsInherited = false;
		FBlackboardEntry* BlackboardEntry = GetSelectedEntry(bIsInherited);
		if(BlackboardEntry != nullptr)
		{
			return !bIsInherited;
		}
	}

	return false;
}

class FBlackboardEntryClassFilter : public IClassViewerFilter
{
public:

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		if(InClass != nullptr)
		{
			return !InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_HideDropDown) &&
				InClass->HasAnyClassFlags(CLASS_EditInlineNew) &&
				InClass->IsChildOf(UBlackboardKeyType::StaticClass());
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return InUnloadedClassData->IsChildOf(UBlackboardKeyType::StaticClass());
	}
};


TSharedRef<SWidget> SHTNBlackboardEditor::HandleCreateNewEntryMenu() const
{
	FClassViewerInitializationOptions Options;
	Options.bShowUnloadedBlueprints = true;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	Options.ClassFilter = MakeShared<FBlackboardEntryClassFilter>();

	const FOnClassPicked OnPicked(FOnClassPicked::CreateRaw(const_cast<SHTNBlackboardEditor*>(this), &SHTNBlackboardEditor::HandleKeyClassPicked));

	// clear the search box, just in case there's something typed in there 
	// We need to do that since key adding code takes advantage of selection mechanics
	TSharedRef<SEditableTextBox> FilterTextBox = GraphActionMenu->GetFilterTextBox();
	FilterTextBox->SetText(FText());

	return 
		SNew(SBox)
		.HeightOverride(240.0f)
		.WidthOverride(200.0f)
		[
			FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked)
		];
}

void SHTNBlackboardEditor::HandleKeyClassPicked(UClass* InClass)
{
	if (BlackboardData == nullptr)
	{
		UE_LOG(LogBlackboardEditor, Error, TEXT("Trying to delete an entry from a blackboard while no Blackboard Asset is set!"));
		return;
	}

	FSlateApplication::Get().DismissAllMenus();

	check(InClass);
	check(InClass->IsChildOf(UBlackboardKeyType::StaticClass()));

	const FScopedTransaction Transaction(LOCTEXT("BlackboardEntryAddTransaction", "Add Blackboard Entry"));
	BlackboardData->SetFlags(RF_Transactional);
	BlackboardData->Modify();

	// create a name for this new key
	FString NewKeyName = InClass->GetDisplayNameText().ToString();
	NewKeyName = NewKeyName.Replace(TEXT(" "), TEXT(""));
	NewKeyName += TEXT("Key");

	int32 IndexSuffix = -1;
	auto DuplicateFunction = [&](const FBlackboardEntry& Key)
	{		
		if(Key.EntryName.ToString() == NewKeyName)
		{
			IndexSuffix = FMath::Max(0, IndexSuffix);
		}
		if(Key.EntryName.ToString().StartsWith(NewKeyName))
		{
			const FString ExistingSuffix = Key.EntryName.ToString().RightChop(NewKeyName.Len());
			if(ExistingSuffix.IsNumeric())
			{
				IndexSuffix = FMath::Max(FCString::Atoi(*ExistingSuffix) + 1, IndexSuffix);
			}
		}
	};

	// check for existing keys of the same name
	for(const auto& Key : BlackboardData->Keys) { DuplicateFunction(Key); };
	for(const auto& Key : BlackboardData->ParentKeys) { DuplicateFunction(Key); };

	if(IndexSuffix != -1)
	{
		NewKeyName += FString::Printf(TEXT("%d"), IndexSuffix);
	}

	FBlackboardEntry Entry;
	Entry.EntryName = FName(*NewKeyName);
	Entry.KeyType = NewObject<UBlackboardKeyType>(BlackboardData, InClass);

	BlackboardData->Keys.Add(Entry);

	GraphActionMenu->RefreshAllActions(true);
	OnBlackboardKeyChanged.ExecuteIfBound(BlackboardData, &BlackboardData->Keys.Last());

	GraphActionMenu->SelectItemByName(Entry.EntryName, ESelectInfo::OnMouseClick);

	// Mark newly created entry as 'new'
	TArray< TSharedPtr<FEdGraphSchemaAction> > SelectedActions;
	GraphActionMenu->GetSelectedActions(SelectedActions);
	check(SelectedActions.Num() == 1);
	check(SelectedActions[0]->GetTypeId() == FEdGraphSchemaAction_BlackboardEntry::StaticGetTypeId());
	TSharedPtr<FEdGraphSchemaAction_BlackboardEntry> BlackboardEntryAction = StaticCastSharedPtr<FEdGraphSchemaAction_BlackboardEntry>(SelectedActions[0]);
	BlackboardEntryAction->bIsNew = true;

	GraphActionMenu->OnRequestRenameOnActionNode();
}

bool SHTNBlackboardEditor::CanCreateNewEntry() const
{
	if(OnIsDebuggerReady.IsBound())
	{
		return !OnIsDebuggerReady.Execute();
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
