using UnrealBuildTool;

public class KemSUinVR : ModuleRules
{
    public KemSUinVR(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "UMG",
                "Slate",
                "SlateCore",
                "NetCore",
                "Networking",
                "Sockets",
                "OnlineSubsystem",
                "OnlineSubsystemUtils",
                "AudioMixer",
                "GameplayTags",
                "DeveloperSettings",
                "WebBrowserWidget",
                "HeadMountedDisplay",
                "EnhancedInput"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "Projects",
                "ApplicationCore",
                "RenderCore",
                "RHI",
                "HTTP"
            });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
