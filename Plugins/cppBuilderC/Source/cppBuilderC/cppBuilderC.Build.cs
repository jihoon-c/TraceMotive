// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class cppBuilderC : ModuleRules
{
	public cppBuilderC(ReadOnlyTargetRules Target) : base(Target)
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

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UMG",
				"EnhancedInput",
				"InputCore",
				"MediaAssets",
				"MediaUtils",
				"Slate",
				"SlateCore"
			}
			);

		bool bEnableVRSupport = true;
		if (bEnableVRSupport)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"HeadMountedDisplay",
					"XRBase"
				}
				);

			PublicDefinitions.Add("CPB_VR_ENABLED=1");
		}
		else
		{
			PublicDefinitions.Add("CPB_VR_ENABLED=0");
		}
	}
}
