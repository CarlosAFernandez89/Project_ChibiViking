// Copyright 2020-2021 Maksym Maisak. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

// Miscellaneous types and definitions used in the Hierarchical Task Networks plugin.

#define USE_HTN_DEBUGGER (1 && WITH_EDITORONLY_DATA)
#define HTN_DEBUG_PLANNING (1 && ENABLE_VISUAL_LOG)

// In 4.25, UProperty etc. were renamed to FProperty etc.
// The code in the plugin uses the new naming, so these preprocessor definitions
// allow the plugin to compile successfully even on versions below 4.25
#if (ENGINE_MAJOR_VERSION == 4) && (ENGINE_MINOR_VERSION < 25)
	#define FField UField
	#define FProperty UProperty
	#define FStructProperty UStructProperty
#endif

#if ENGINE_MAJOR_VERSION < 5
	#define UNWRAP_TOBJECT_PTR(Ptr) Ptr
#else
	#define UNWRAP_TOBJECT_PTR(Ptr) Ptr.Get()
#endif

HTN_API DECLARE_LOG_CATEGORY_EXTERN(LogHTN, Display, All);
HTN_API DECLARE_LOG_CATEGORY_EXTERN(LogHTNCurrentPlan, Display, All);

// Statistics groups for stat tracking and profiling
DECLARE_STATS_GROUP(TEXT("HTN"), STATGROUP_AI_HTN, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Tick"), STAT_AI_HTN_Tick, STATGROUP_AI_HTN, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Planning"), STAT_AI_HTN_Planning, STATGROUP_AI_HTN, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Execution Time"), STAT_AI_HTN_Execution, STATGROUP_AI_HTN, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Cleanup Time"), STAT_AI_HTN_Cleanup, STATGROUP_AI_HTN, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Stop HTN Time"), STAT_AI_HTN_StopHTN, STATGROUP_AI_HTN, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Node Instantiation Time"), STAT_AI_HTN_NodeInstantiation, STATGROUP_AI_HTN, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Produced Plans"), STAT_AI_HTN_NumProducedPlans, STATGROUP_AI_HTN, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Node Instances"), STAT_AI_HTN_NumNodeInstances, STATGROUP_AI_HTN, );

UENUM(BlueprintType)
enum class EHTNNodeResult : uint8
{
	Succeeded,		// Finished as success
	Failed,			// Finished as failure
	Aborted,		// Finished aborting = failure
	InProgress		// Not finished yet
};

UENUM(BlueprintType)
enum class EHTNTaskStatus : uint8
{
	Active,
	Aborting,
	Inactive
};

UENUM(BlueprintType)
enum class EHTNPlanExecutionFinishedResult : uint8
{
	Succeeded,
	FailedOrAborted
};

enum class EHTNSubNodeType : int32
{
	Decorator,
	Service
};

namespace FBlackboard
{
	// The location of the character at a particular point in the plan.
	const FName KeySelfLocation = TEXT("SelfLocation");
}

namespace FHTNNames
{
	// Used by HTNTask_EQSQuery to let EQS contexts know if they're running during planning.
	const FName IsPlanTimeQuery = TEXT("HTNInternal_EQSParamName_IsPlanTimeQuery");
}

struct HTN_API FHTNPlanStepID
{
	static const FHTNPlanStepID None;

	// The index (in the FHTNPlan::Levels array) of the step's plan level.
	int32 LevelIndex = INDEX_NONE;

	// The index (in the FHTNPlanLevel::Steps array) of the step in its plan level.
	int32 StepIndex = INDEX_NONE;

	FORCEINLINE friend bool operator==(const FHTNPlanStepID& A, const FHTNPlanStepID& B)
	{
		return A.LevelIndex == B.LevelIndex && A.StepIndex == B.StepIndex;
	}

	FORCEINLINE friend bool operator!=(const FHTNPlanStepID& A, const FHTNPlanStepID& B)
	{
		return A.LevelIndex != B.LevelIndex || A.StepIndex != B.StepIndex;
	}
};

// Used in FHTNPlan::PriorityMarkers to deprioritize some plans relative to others. 
// This is necessary for HTNNode_Prefer to work.
// See FHTNPlan::PriorityMarkers for more info.
using FHTNPriorityMarker = int16;
