// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class ToyBuilderCEditorTarget : TargetRules
{
	public ToyBuilderCEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		if (Target.Version.MajorVersion > 5 || (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 7))
		{
			bOverrideBuildEnvironment = true;
		}

		ExtraModuleNames.AddRange( new string[] { "ToyBuilderC" } );
	}
}
