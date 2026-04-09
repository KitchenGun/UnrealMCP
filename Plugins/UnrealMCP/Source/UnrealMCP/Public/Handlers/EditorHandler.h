#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Phase 5: Editor Automation 핸들러.
 * PIE 제어, 뷰포트 카메라, 콘솔 명령, 스크린샷,
 * 액터 선택, 레벨 저장/로드를 처리한다.
 */
class FEditorHandler
{
public:
    /** 커맨드 타입에 따라 적절한 핸들 함수로 디스패치 */
    static TSharedPtr<FJsonObject> HandleCommand(
        const FString& CommandType,
        const TSharedPtr<FJsonObject>& Params);

private:
    // ── PIE ─────────────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> PlayInEditor(const TSharedPtr<FJsonObject>& Params);

    // ── 뷰포트 ──────────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> SetViewportCamera(const TSharedPtr<FJsonObject>& Params);

    // ── 콘솔 명령 ───────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> RunConsoleCommand(const TSharedPtr<FJsonObject>& Params);

    // ── 스크린샷 ────────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> TakeScreenshot(const TSharedPtr<FJsonObject>& Params);

    // ── 액터 선택 ───────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> GetSelectedActors(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> SelectActors(const TSharedPtr<FJsonObject>& Params);

    // ── 레벨 ────────────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> SaveLevel(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> LoadLevel(const TSharedPtr<FJsonObject>& Params);

    // ── 내부 헬퍼 ───────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> MakeSuccess(TSharedPtr<FJsonObject> Result);
    static TSharedPtr<FJsonObject> MakeError(const FString& Code, const FString& Message);
    static TSharedPtr<FJsonObject> ActorToJson(AActor* Actor);
};
