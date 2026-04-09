#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Phase 4: AI System 핸들러.
 * Behavior Tree, Blackboard, EQS, AIPerception, AIController 생성/설정을 처리한다.
 */
class FAIHandler
{
public:
    /** 커맨드 타입에 따라 적절한 핸들 함수로 디스패치 */
    static TSharedPtr<FJsonObject> HandleCommand(
        const FString& CommandType,
        const TSharedPtr<FJsonObject>& Params);

private:
    // ── Behavior Tree ────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> CreateBehaviorTree(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> AddBTNode(const TSharedPtr<FJsonObject>& Params);

    // ── Blackboard ───────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> CreateBlackboard(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> AddBlackboardKey(const TSharedPtr<FJsonObject>& Params);

    // ── EQS ─────────────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> CreateEQSQuery(const TSharedPtr<FJsonObject>& Params);

    // ── AIPerception / AIController ──────────────────────────────────
    static TSharedPtr<FJsonObject> SetupAIPerception(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> CreateAIController(const TSharedPtr<FJsonObject>& Params);

    // ── 내부 헬퍼 ───────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> MakeSuccess(TSharedPtr<FJsonObject> Result);
    static TSharedPtr<FJsonObject> MakeError(const FString& Code, const FString& Message);
};
