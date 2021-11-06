// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EQSParametrizedQueryExecutionRequestHTN.generated.h"

// An extension of the FEQSParametrizedQueryExecutionRequest to make it work with BlackboardWorldstate
USTRUCT()
struct FEQSParametrizedQueryExecutionRequestHTN : public FEQSParametrizedQueryExecutionRequest
{
	GENERATED_BODY()

	int32 Execute(AActor& QueryOwner, const class FBlackboardWorldState& WorldState, const FQueryFinishedSignature& QueryFinishedDelegate) const;
	TSharedPtr<FEnvQueryResult> ExecuteInstant(AActor& QueryOwner, const class FBlackboardWorldState& WorldState) const;

private:

	UEnvQuery* GetQueryTemplateFromWorldState(AActor& QueryOwner, const class FBlackboardWorldState& WorldState) const;
	void PopulateDynamicParameters(struct FEnvQueryRequest& QueryRequest, const class FBlackboardWorldState& WorldState) const;
};
