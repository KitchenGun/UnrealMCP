using UnrealBuildTool;

public class UnrealMCP : ModuleRules
{
    public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // 에디터 API (GEditor, 트랜잭션, 액터 선택 등)
            "UnrealEd",
            // TCP 소켓
            "Sockets",
            "Networking",
            // JSON 직렬화
            "Json",
            "JsonUtilities",
        });

        // 에디터 전용 빌드만 허용
        if (Target.bBuildEditor == false)
        {
            throw new System.Exception("UnrealMCP 플러그인은 에디터 전용입니다.");
        }
    }
}
