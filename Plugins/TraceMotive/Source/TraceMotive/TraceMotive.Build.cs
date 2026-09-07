// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TraceMotive : ModuleRules
{
	public TraceMotive(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"PhysicsCore",
				"EnhancedInput",
				"InputBlueprintNodes",
				"UMG",
				"UMGEditor",
				"EngineSettings",
				"DeveloperSettings",
				"Settings",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"Kismet",
				"KismetCompiler",
				"BlueprintGraph",
				"GraphEditor",
				"EditorStyle",
				"InputCore",
				"ApplicationCore",
				"ToolMenus",
				"LevelEditor",
                "EditorWidgets",
                "ToolWidgets",
                "AssetRegistry",
                "ContentBrowser",
                "DesktopPlatform",
				"Projects",
				"Json",
            }
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}





