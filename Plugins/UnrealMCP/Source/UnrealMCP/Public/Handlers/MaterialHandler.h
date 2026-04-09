#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Phase 3: Material & Asset 핸들러.
 * 에셋 검색, 머티리얼 생성/편집, 에셋 임포트/복제/삭제를 처리한다.
 */
class FMaterialHandler
{
public:
    /** 커맨드 타입에 따라 적절한 핸들 함수로 디스패치 */
    static TSharedPtr<FJsonObject> HandleCommand(
        const FString& CommandType,
        const TSharedPtr<FJsonObject>& Params);

private:
    // ── 에셋 검색 / 정보 ────────────────────────────────────────────
    static TSharedPtr<FJsonObject> SearchAssets(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> GetAssetDetails(const TSharedPtr<FJsonObject>& Params);

    // ── 머티리얼 생성 / 편집 ────────────────────────────────────────
    static TSharedPtr<FJsonObject> CreateMaterial(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> AddMaterialExpression(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> ConnectMaterialNodes(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> SetMaterialParameter(const TSharedPtr<FJsonObject>& Params);

    // ── 액터에 머티리얼 적용 ─────────────────────────────────────────
    static TSharedPtr<FJsonObject> ApplyMaterialToActor(const TSharedPtr<FJsonObject>& Params);

    // ── 에셋 임포트 / 복제 / 삭제 ──────────────────────────────────
    static TSharedPtr<FJsonObject> ImportAsset(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> DuplicateAsset(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> DeleteAsset(const TSharedPtr<FJsonObject>& Params);

    // ── 내부 헬퍼 ───────────────────────────────────────────────────
    static TSharedPtr<FJsonObject> MakeSuccess(TSharedPtr<FJsonObject> Result);
    static TSharedPtr<FJsonObject> MakeError(const FString& Code, const FString& Message);
};
