// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GMTK_2026 : ModuleRules
{
	public GMTK_2026(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Niagara" });

		PublicIncludePaths.AddRange(new string[] {
			"GMTK_2026",
			"GMTK_2026/Variant_Platforming",
			"GMTK_2026/Variant_Platforming/Animation",
			"GMTK_2026/Variant_Combat",
			"GMTK_2026/Variant_Combat/AI",
			"GMTK_2026/Variant_Combat/Animation",
			"GMTK_2026/Variant_Combat/Gameplay",
			"GMTK_2026/Variant_Combat/Interfaces",
			"GMTK_2026/Variant_Combat/UI",
			"GMTK_2026/Variant_SideScrolling",
			"GMTK_2026/Variant_SideScrolling/AI",
			"GMTK_2026/Variant_SideScrolling/Gameplay",
			"GMTK_2026/Variant_SideScrolling/Interfaces",
			"GMTK_2026/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
