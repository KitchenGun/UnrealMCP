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
            // Phase 2: Blueprint 편집 API
            "BlueprintGraph",           // UK2Node_*, UEdGraph, FBlueprintEditorUtils
            "KismetCompiler",           // FKismetEditorUtilities::CompileBlueprint
            "AssetTools",               // IAssetTools::CreateAsset
            // Phase 3: Material & Asset
            "AssetRegistry",            // FAssetRegistryModule
            "MaterialEditor",           // UMaterialEditingLibrary
            // Phase 4: AI System
            "AIModule",                 // UAIController, UBlackboardData, UBehaviorTree
            "GameplayTasks",            // AI 태스크 기반 시스템
            "EnvironmentQuery",         // UEnvQuery
            // Phase 5: Editor Automation
            "LevelEditor",              // ULevelEditorSubsystem, FLevelEditorViewportClient
            // Phase 6: Advanced Systems
            "Niagara",                  // UNiagaraSystem, UNiagaraSystemFactoryNew
            "NiagaraEditor",            // Niagara 에디터 지원
            "AnimGraph",                // UAnimBlueprint
            "UMG",                      // UUserWidget, UWidgetBlueprint
            "UMGEditor",                // UWidgetBlueprintFactory
        });

        // 에디터 전용 빌드만 허용
        if (Target.bBuildEditor == false)
        {
            throw new System.Exception("UnrealMCP 플러그인은 에디터 전용입니다.");
        }
    }
}
