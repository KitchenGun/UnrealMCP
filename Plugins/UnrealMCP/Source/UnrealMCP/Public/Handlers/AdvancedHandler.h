#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Phase 6: Advanced Systems 핸들러.
 * Niagara, Animation Blueprint, Widget Blueprint,
 * DataTable, DataAsset, UObject 검사를 처리한다.
 */
class FAdvancedHandler
{
public:
    /** 커맨드 타입에 따라 적절한 핸들 함수로 디스패치 */
    static TSharedPtr<FJsonObject> HandleCommand(
        const FString& CommandType,
        const TSharedPtr<FJsonObject>& Params);

private:
    // ── Niagara ─────────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> CreateNiagaraSystem(const TSharedPtr<FJsonObject>& Params);

    // ── Animation Blueprint ──────────────────────────────────────────
    static TSharedPtr<FJsonObject> CreateAnimationBlueprint(const TSharedPtr<FJsonObject>& Params);

    // ── Widget Blueprint ─────────────────────────────────────────────
    static TSharedPtr<FJsonObject> CreateWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);

    // ── DataTable / DataAsset ────────────────────────────────────────
    static TSharedPtr<FJsonObject> CreateDataTable(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> CreateDataAsset(const TSharedPtr<FJsonObject>& Params);

    // ── UObject 검사 ─────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> InspectUObject(const TSharedPtr<FJsonObject>& Params);

    // ── 내부 헬퍼 ───────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> MakeSuccess(TSharedPtr<FJsonObject> Result);
    static TSharedPtr<FJsonObject> MakeError(const FString& Code, const FString& Message);
};
