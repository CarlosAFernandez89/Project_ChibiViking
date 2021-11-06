// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTNEditor : ModuleRules
{
	public HTNEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"UnrealEd",
				"LevelEditor",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "EditorStyle",
                "AIGraph",
                "GraphEditor",
                "Kismet",
                "KismetWidgets",
                "GameplayTasks",
                "AIModule",
                "ToolMenus",
                "PropertyEditor",
                "HTN"
			}
		);
	}
}
