#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Light component control commands handler.
 *
 * set_light_property — DirectionalLight / PointLight / SpotLight / RectLight / SkyLight
 * get_light_property — 라이트 컴포넌트 속성 읽기
 */
class FLightHandler
{
public:
    static TSharedPtr<FJsonObject> HandleSetLightProperty(const TSharedPtr<FJsonObject>& Params);
    static TSharedPtr<FJsonObject> HandleGetLightProperty(const TSharedPtr<FJsonObject>& Params);

private:
    static UWorld* GetEditorWorld();
    static AActor* FindActorByName(UWorld* World, const FString& Name);
    static TSharedPtr<FJsonObject> MakeSuccess(TSharedPtr<FJsonObject> Result);
    static TSharedPtr<FJsonObject> MakeError(const FString& Code, const FString& Message);
};
