// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class cppBuilderCEditor : ModuleRules
{
	public cppBuilderCEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
			);

		PrivateDependencyModuleNames.Add("cppBuilderC");

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"EditorScriptingUtilities",
					"DataTableEditor",
					"AssetRegistry",
					"XmlParser",
					"Blutility",
					"UMG",
					"UMGEditor",
					"Slate",
					"SlateCore"
				}
				);

			AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
		}
	}
}
