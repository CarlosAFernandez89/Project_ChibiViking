// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HTN.generated.h"

// A Hierarchical Task Network asset
UCLASS(BlueprintType)
class HTN_API UHTN : public UObject
{
	GENERATED_BODY()

public:
	// The nodes that begin from the root.
	UPROPERTY()
	TArray<class UHTNStandaloneNode*> StartNodes;

	UPROPERTY()
	TArray<class UHTNDecorator*> RootDecorators;

	UPROPERTY()
	TArray<class UHTNService*> RootServices; 

#if WITH_EDITORONLY_DATA
	// The editor graph associated with this HTN.
	UPROPERTY()
	class UEdGraph* HTNGraph;

	// Contextual info stored on editor close. Viewport location, zoom level etc.
	UPROPERTY()
	TArray<FEditedDocumentInfo> LastEditedDocuments;
#endif

	// Blackboard asset for this HTH.
	UPROPERTY()
	class UBlackboardData* BlackboardAsset;
};