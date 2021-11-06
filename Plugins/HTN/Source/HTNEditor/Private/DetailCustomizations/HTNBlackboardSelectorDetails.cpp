// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "DetailCustomizations/HTNBlackboardSelectorDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Engine/GameViewportClient.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BehaviorTree/BlackboardData.h"
#include "IPropertyUtilities.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "HTNEditor.h"
#include "HTNNode.h"

#define LOCTEXT_NAMESPACE "HTNBlackboardSelectorDetails"

TSharedRef<IPropertyTypeCustomization> FHTNBlackboardSelectorDetails::MakeInstance() { return MakeShared<FHTNBlackboardSelectorDetails>(); }

void FHTNBlackboardSelectorDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	MyStructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();

	CacheBlackboardData();
	
	HeaderRow.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FHTNBlackboardSelectorDetails::IsEditingEnabled)))
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &FHTNBlackboardSelectorDetails::OnGetKeyContent)
 			.ContentPadding(FMargin( 2.0f, 2.0f ))
			.IsEnabled(this, &FHTNBlackboardSelectorDetails::IsEditingEnabled)
			.ButtonContent()
			[
				SNew(STextBlock) 
				.Text(this, &FHTNBlackboardSelectorDetails::GetCurrentKeyDesc)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];

	InitKeyFromProperty();
}

void FHTNBlackboardSelectorDetails::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
}

const UBlackboardData* FHTNBlackboardSelectorDetails::FindBlackboardAsset(UObject* InObj)
{
	for (UObject* Object = InObj; Object; Object = Object->GetOuter())
	{
		if (UHTNNode* const Node = Cast<UHTNNode>(Object))
		{
			return Node->GetBlackboardAsset();
		}
	}

	return nullptr;
}

void FHTNBlackboardSelectorDetails::CacheBlackboardData()
{
	const TSharedPtr<IPropertyHandleArray> MyFilterProperty = MyStructProperty->GetChildHandle(TEXT("AllowedTypes"))->AsArray();
	MyKeyNameProperty = MyStructProperty->GetChildHandle(TEXT("SelectedKeyName"));
	MyKeyIDProperty = MyStructProperty->GetChildHandle(TEXT("SelectedKeyID"));
	MyKeyClassProperty = MyStructProperty->GetChildHandle(TEXT("SelectedKeyType"));

	const TSharedPtr<IPropertyHandle> NonesAllowed = MyStructProperty->GetChildHandle(TEXT("bNoneIsAllowedValue"));
	NonesAllowed->GetValue(bNoneIsAllowedValue);
	
	KeyValues.Reset();

	TArray<UBlackboardKeyType*> FilterObjects;
	
	uint32 NumElements = 0;
	FPropertyAccess::Result Result = MyFilterProperty->GetNumElements(NumElements);
	if (Result == FPropertyAccess::Success)
	{
		for (uint32 Idx = 0; Idx < NumElements; Idx++)
		{
			UObject* FilterOb;
			Result = MyFilterProperty->GetElement(Idx)->GetValue(FilterOb);
			if (Result == FPropertyAccess::Success)
			{
				UBlackboardKeyType* KeyFilterOb = Cast<UBlackboardKeyType>(FilterOb);
				if (KeyFilterOb)
				{
					FilterObjects.Add(KeyFilterOb);
				}
			}
		}
	}

	TArray<UObject*> MyObjects;
	MyStructProperty->GetOuterObjects(MyObjects);
	for (int32 ObjectIdx = 0; ObjectIdx < MyObjects.Num(); ObjectIdx++)
	{
		if (UBlackboardData* BlackboardAsset = const_cast<UBlackboardData*>(FindBlackboardAsset(MyObjects[ObjectIdx])))
		{
			CachedBlackboardAsset = BlackboardAsset;

			TArray<FName> ProcessedNames;
			for (UBlackboardData* It = BlackboardAsset; It; It = It->Parent)
			{
				for (int32 KeyIdx = 0; KeyIdx < It->Keys.Num(); KeyIdx++)
				{
					const FBlackboardEntry& EntryInfo = It->Keys[KeyIdx];
					const bool bIsKeyOverridden = ProcessedNames.Contains(EntryInfo.EntryName);
					bool bIsEntryAllowed = !bIsKeyOverridden && EntryInfo.KeyType;

					ProcessedNames.Add(EntryInfo.EntryName);

					if (bIsEntryAllowed && FilterObjects.Num())
					{
						bool bFilterPassed = false;
						for (int32 FilterIdx = 0; FilterIdx < FilterObjects.Num(); FilterIdx++)
						{
							if (EntryInfo.KeyType->IsAllowedByFilter(FilterObjects[FilterIdx]))
							{
								bFilterPassed = true;
								break;
							}
						}

						bIsEntryAllowed = bFilterPassed;
					}

					if (bIsEntryAllowed)
					{
						KeyValues.AddUnique(EntryInfo.EntryName);
					}
				}
			}

			break;
		}
	}

	if (GetDefault<UEditorPerProjectUserSettings>()->bDisplayBlackboardKeysInAlphabeticalOrder)
	{
		KeyValues.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	}	
}

void FHTNBlackboardSelectorDetails::InitKeyFromProperty()
{
	FName KeyNameValue;
	FPropertyAccess::Result Result = MyKeyNameProperty->GetValue(KeyNameValue);
	if (Result == FPropertyAccess::Success)
	{
		const int32 KeyIdx = KeyValues.IndexOfByKey(KeyNameValue);
		if (KeyIdx == INDEX_NONE)
		{
			if (bNoneIsAllowedValue == false)
			{
				const FName PropName = MyStructProperty->GetProperty() ? MyStructProperty->GetProperty()->GetFName() : NAME_None;
				const int32 KeyNameIdx = KeyValues.IndexOfByKey(PropName);

				OnKeyComboChange(KeyNameIdx != INDEX_NONE ? KeyNameIdx : 0);
			}
			else
			{
				MyKeyClassProperty->SetValue(StaticCast<UObject*>(nullptr));
				MyKeyIDProperty->SetValue(FBlackboard::InvalidKey);
				MyKeyNameProperty->SetValue(TEXT("None"));
			}
		}
	}
}

TSharedRef<SWidget> FHTNBlackboardSelectorDetails::OnGetKeyContent() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	for (int32 Idx = 0; Idx < KeyValues.Num(); Idx++)
	{
		FUIAction ItemAction( FExecuteAction::CreateSP( const_cast<FHTNBlackboardSelectorDetails*>(this), &FHTNBlackboardSelectorDetails::OnKeyComboChange, Idx) );
		MenuBuilder.AddMenuEntry( FText::FromName( KeyValues[Idx] ), TAttribute<FText>(), FSlateIcon(), ItemAction);
	}

	return MenuBuilder.MakeWidget();
}

FText FHTNBlackboardSelectorDetails::GetCurrentKeyDesc() const
{
	FName NameValue;
	MyKeyNameProperty->GetValue(NameValue);

	const int32 KeyIdx = KeyValues.IndexOfByKey(NameValue);
	return KeyValues.IsValidIndex(KeyIdx) ? FText::FromName(KeyValues[KeyIdx]) : FText::FromName(NameValue);
}

void FHTNBlackboardSelectorDetails::OnKeyComboChange(int32 Index)
{
	if (KeyValues.IsValidIndex(Index))
	{
		UBlackboardData* BlackboardAsset = CachedBlackboardAsset.Get();
		if (BlackboardAsset)
		{
			const uint8 KeyID = BlackboardAsset->GetKeyID(KeyValues[Index]);
			const UObject* KeyClass = BlackboardAsset->GetKeyType(KeyID);

			MyKeyClassProperty->SetValue(KeyClass);
			MyKeyIDProperty->SetValue(KeyID);

			MyKeyNameProperty->SetValue(KeyValues[Index]);
		}
	}
}

bool FHTNBlackboardSelectorDetails::IsEditingEnabled() const
{
	return FHTNEditor::IsPIENotSimulating() && PropUtils->IsPropertyEditingEnabled();
}

#undef LOCTEXT_NAMESPACE
