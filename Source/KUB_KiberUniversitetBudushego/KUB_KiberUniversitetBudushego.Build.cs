using UnrealBuildTool;

public class KUB_KiberUniversitetBudushego : ModuleRules
{
    public KUB_KiberUniversitetBudushego(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.Add("Core");
    }
}
