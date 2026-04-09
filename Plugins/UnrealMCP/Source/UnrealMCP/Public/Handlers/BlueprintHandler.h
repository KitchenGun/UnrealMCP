#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * FBlueprintHandler
 *
 * Phase 2 Blueprint 편집 커맨드를 처리한다.
 * MCPTcpServer에서 수신한 JSON 커맨드를 UE5 Editor Blueprint API 호출로 변환.
 */
class FBlueprintHandler
{
public:
    /**
     * 커맨드 타입에 따라 적절한 핸들러를 호출한다.
     *
     * @param CommandType  커맨드 타입 문자열 (예: "create_blueprint")
     * @param Params       커맨드 파라미터 JSON 객체
     * @return             결과 JSON 객체 (success, data/error 포함)
     */
    static TSharedPtr<FJsonObject> HandleCommand(
        const FString& CommandType,
        const TSharedPtr<FJsonObject>& Params);

private:
    // ── Blueprint 에셋 ────────────────────────────────────────────
    static TSharedPtr<FJsonObject> CreateBlueprint(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> CompileBlueprint(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> GetBlueprintGraph(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> SpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);

    // ── 그래프 노드 ───────────────────────────────────────────────
    static TSharedPtr<FJsonObject> AddBlueprintNode(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> ConnectBlueprintPins(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> RemoveBlueprintNode(const TSharedPtr<FJsonObject>& Params);

    // ── 변수 & 컴포넌트 ──────────────────────────────────────────
    static TSharedPtr<FJsonObject> AddBlueprintVariable(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> AddBlueprintComponent(const TSharedPtr<FJsonObject>& Params);

    // ── 내부 헬퍼 ────────────────────────────────────────────────

    /** 에셋 이름으로 Blueprint 오브젝트를 검색한다. */
    static class UBlueprint* FindBlueprintByName(const FString& Name);

    /** 부모 클래스 이름으로 UClass를 검색한다. */
    static UClass* FindParentClass(const FString& ClassName);

    /** 에디터 그래프에서 노드 ID(GUID 문자열)로 노드를 찾는다. */
    static class UK2Node* FindNodeById(class UEdGraph* Graph, const FString& NodeId);

    /** 성공 응답 JSON을 생성한다. */
    static TSharedPtr<FJsonObject> MakeSuccess(TSharedPtr<FJsonObject> Data = nullptr);

    /** 실패 응답 JSON을 생성한다. */
    static TSharedPtr<FJsonObject> MakeError(const FString& Code, const FString& Message);
};
