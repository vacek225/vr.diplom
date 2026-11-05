using UnrealBuildTool;

public class KUB_KiberUniversitetBudushegoEditorTarget : TargetRules
{
    public KUB_KiberUniversitetBudushegoEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("KUB_KiberUniversitetBudushego");
    }
}
