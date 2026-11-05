using UnrealBuildTool;

public class KUB_KiberUniversitetBudushegoTarget : TargetRules
{
    public KUB_KiberUniversitetBudushegoTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("KUB_KiberUniversitetBudushego");
    }
}
