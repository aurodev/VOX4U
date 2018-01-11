// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VOX4UEditor : ModuleRules
{
    public VOX4UEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
                "VOX4UEditor/Public"
            }
        );


        PrivateIncludePaths.AddRange(
            new string[] {
                "VOX4UEditor/Private",
            }
        );


        PublicDependencyModuleNames.AddRange(
            new string[] {
                "UnrealEd",
                "CoreUObject",
                "Engine",
                "Core",
                "InputCore",
                "Slate",
                "SlateCore",
                "UnrealEd",
                "RawMesh",
                "MainFrame",
                "PropertyEditor",
            }
        );


        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "VOX4U",
                "ApexDestruction",
            }
        );


        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
            }
        );
    }
}
