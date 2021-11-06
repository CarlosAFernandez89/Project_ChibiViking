// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#include "BlackboardDataDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "BehaviorTree/BlackboardData.h"

#define LOCTEXT_NAMESPACE "BlackboardDataDetails"

TSharedRef<IDetailCustomization> FBlackboardDataDetails::MakeInstance(FOnGetSelectedBlackboardItemIndex InOnGetSelectedBlackboardItemIndex)
{
	return MakeShared<FBlackboardDataDetails>(InOnGetSelectedBlackboardItemIndex);
}

void FBlackboardDataDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	// First hide all keys
	DetailLayout.HideProperty(TEXT("Keys"));
	DetailLayout.HideProperty(TEXT("ParentKeys"));

	// Now show only the currently selected key
	bool bIsInherited = false;
	int32 CurrentSelection = INDEX_NONE;
	if (OnGetSelectedBlackboardItemIndex.IsBound())
	{
		CurrentSelection = OnGetSelectedBlackboardItemIndex.Execute(bIsInherited);
	}

	if (CurrentSelection >= 0)
	{
		const TSharedPtr<IPropertyHandle> KeysHandle = bIsInherited ? DetailLayout.GetProperty(TEXT("ParentKeys")) : DetailLayout.GetProperty(TEXT("Keys"));
		check(KeysHandle.IsValid());
		uint32 NumChildKeys = 0;
		KeysHandle->GetNumChildren(NumChildKeys);
		if (StaticCast<uint32>(CurrentSelection) < NumChildKeys)
		{
			const TSharedPtr<IPropertyHandle> KeyHandle = KeysHandle->GetChildHandle(StaticCast<uint32>(CurrentSelection));

			IDetailCategoryBuilder& DetailCategoryBuilder = DetailLayout.EditCategory("Key");
			const TSharedPtr<IPropertyHandle> EntryNameProperty = KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBlackboardEntry, EntryName));
			DetailCategoryBuilder.AddCustomRow(LOCTEXT("EntryNameLabel", "Entry Name"))
			.NameContent()
			[
				EntryNameProperty->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				.IsEnabled(true)
				+SHorizontalBox::Slot()
				[
					EntryNameProperty->CreatePropertyValueWidget()
				]
			];

#if WITH_EDITORONLY_DATA
			const TSharedPtr<IPropertyHandle> EntryDescriptionHandle = KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBlackboardEntry, EntryDescription));
			DetailCategoryBuilder.AddProperty(EntryDescriptionHandle);
#endif

			const TSharedPtr<IPropertyHandle> KeyTypeProperty = KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBlackboardEntry, KeyType));
			DetailCategoryBuilder.AddProperty(KeyTypeProperty);

			const TSharedPtr<IPropertyHandle> bInstanceSyncedProperty = KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBlackboardEntry, bInstanceSynced));
			DetailCategoryBuilder.AddProperty(bInstanceSyncedProperty);
		}	
	}
}

#undef LOCTEXT_NAMESPACE
