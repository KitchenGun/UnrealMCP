#include "Handlers/LightHandler.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Light.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ─────────────────────────────────────────────────
// 내부 헬퍼
// ─────────────────────────────────────────────────

UWorld* FLightHandler::GetEditorWorld()
{
    if (GEditor)
    {
        return GEditor->GetEditorWorldContext().World();
    }
    return nullptr;
}

AActor* FLightHandler::FindActorByName(UWorld* World, const FString& Name)
{
    if (!World || Name.IsEmpty())
    {
        return nullptr;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase))
        {
            return *It;
        }
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName().Equals(Name, ESearchCase::IgnoreCase))
        {
            return *It;
        }
    }
    return nullptr;
}

TSharedPtr<FJsonObject> FLightHandler::MakeSuccess(TSharedPtr<FJsonObject> Result)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result.IsValid() ? Result : MakeShared<FJsonObject>());
    Response->SetField(TEXT("error"), MakeShared<FJsonValueNull>());
    return Response;
}

TSharedPtr<FJsonObject> FLightHandler::MakeError(const FString& Code, const FString& Message)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), false);
    Response->SetField(TEXT("result"), MakeShared<FJsonValueNull>());
    TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
    Err->SetStringField(TEXT("code"), Code);
    Err->SetStringField(TEXT("message"), Message);
    Response->SetObjectField(TEXT("error"), Err);
    return Response;
}

// ─────────────────────────────────────────────────
// 라이트 속성 결과 JSON 구성
// ─────────────────────────────────────────────────

static TSharedPtr<FJsonObject> LightComponentToJson(AActor* Actor, ULightComponent* Light)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("name"), Actor->GetActorLabel());
    Obj->SetStringField(TEXT("actor_class"), Actor->GetClass()->GetName());
    Obj->SetNumberField(TEXT("intensity"), Light->Intensity);
    Obj->SetBoolField(TEXT("cast_shadows"), Light->CastShadows);
    Obj->SetBoolField(TEXT("use_temperature"), Light->bUseTemperature);
    Obj->SetNumberField(TEXT("temperature"), Light->Temperature);

    FLinearColor C = Light->GetLightColor();
    TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
    ColorObj->SetNumberField(TEXT("r"), C.R);
    ColorObj->SetNumberField(TEXT("g"), C.G);
    ColorObj->SetNumberField(TEXT("b"), C.B);
    Obj->SetObjectField(TEXT("light_color"), ColorObj);

    if (UPointLightComponent* PL = Cast<UPointLightComponent>(Light))
    {
        Obj->SetNumberField(TEXT("attenuation_radius"), PL->AttenuationRadius);
    }
    if (USpotLightComponent* SL = Cast<USpotLightComponent>(Light))
    {
        Obj->SetNumberField(TEXT("inner_cone_angle"), SL->InnerConeAngle);
        Obj->SetNumberField(TEXT("outer_cone_angle"), SL->OuterConeAngle);
    }
    return Obj;
}

// ─────────────────────────────────────────────────
// set_light_property
// ─────────────────────────────────────────────────

// 지원 property_name 목록:
//   intensity              float   (모든 라이트)
//   light_color            [r,g,b] 배열, 0~1 범위 (모든 라이트)
//   cast_shadows           bool    (모든 라이트)
//   temperature            float   (모든 라이트, 1700~12000 K)
//   use_temperature        bool    (모든 라이트)
//   attenuation_radius     float   (PointLight / SpotLight)
//   inner_cone_angle       float   (SpotLight)
//   outer_cone_angle       float   (SpotLight)

TSharedPtr<FJsonObject> FLightHandler::HandleSetLightProperty(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    FString ActorName    = Params->GetStringField(TEXT("name")).TrimStartAndEnd();
    FString PropertyName = Params->GetStringField(TEXT("property_name")).TrimStartAndEnd().ToLower();

    AActor* Actor = FindActorByName(World, ActorName);
    if (!IsValid(Actor))
    {
        return MakeError(
            TEXT("ACTOR_NOT_FOUND"),
            FString::Printf(TEXT("액터 '%s'를 찾을 수 없습니다."), *ActorName)
        );
    }

    ULightComponent* Light = Actor->FindComponentByClass<ULightComponent>();
    if (!Light)
    {
        return MakeError(
            TEXT("INVALID_ACTOR"),
            FString::Printf(TEXT("'%s'는 라이트 컴포넌트가 없습니다. DirectionalLight / PointLight / SpotLight / RectLight를 대상으로 하세요."), *ActorName)
        );
    }

    GEditor->BeginTransaction(FText::FromString(
        FString::Printf(TEXT("MCP: Set Light Property '%s' on '%s'"), *PropertyName, *ActorName)
    ));
    Light->Modify();

    bool bHandled = false;

    if (PropertyName == TEXT("intensity"))
    {
        double Val = 0.0;
        Params->TryGetNumberField(TEXT("value"), Val);
        Light->SetIntensity(static_cast<float>(Val));
        bHandled = true;
    }
    else if (PropertyName == TEXT("light_color"))
    {
        const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
        if (Params->TryGetArrayField(TEXT("value"), Arr) && Arr && Arr->Num() >= 3)
        {
            FLinearColor NewColor(
                static_cast<float>((*Arr)[0]->AsNumber()),
                static_cast<float>((*Arr)[1]->AsNumber()),
                static_cast<float>((*Arr)[2]->AsNumber())
            );
            Light->SetLightColor(NewColor);
            bHandled = true;
        }
        else
        {
            GEditor->EndTransaction();
            return MakeError(TEXT("INVALID_PARAMS"),
                TEXT("light_color는 [r, g, b] 배열이어야 합니다. 범위: 0.0 ~ 1.0"));
        }
    }
    else if (PropertyName == TEXT("cast_shadows"))
    {
        bool bVal = false;
        Params->TryGetBoolField(TEXT("value"), bVal);
        Light->SetCastShadows(bVal);
        bHandled = true;
    }
    else if (PropertyName == TEXT("temperature"))
    {
        double Val = 6500.0;
        Params->TryGetNumberField(TEXT("value"), Val);
        Light->SetTemperature(static_cast<float>(Val));
        bHandled = true;
    }
    else if (PropertyName == TEXT("use_temperature"))
    {
        bool bVal = false;
        Params->TryGetBoolField(TEXT("value"), bVal);
        Light->bUseTemperature = bVal;
        Light->MarkRenderStateDirty();
        bHandled = true;
    }
    else if (PropertyName == TEXT("attenuation_radius"))
    {
        UPointLightComponent* PL = Cast<UPointLightComponent>(Light);
        if (!PL)
        {
            GEditor->EndTransaction();
            return MakeError(TEXT("INVALID_PARAMS"),
                TEXT("attenuation_radius는 PointLight / SpotLight에서만 사용 가능합니다."));
        }
        double Val = 1000.0;
        Params->TryGetNumberField(TEXT("value"), Val);
        PL->SetAttenuationRadius(static_cast<float>(Val));
        bHandled = true;
    }
    else if (PropertyName == TEXT("inner_cone_angle"))
    {
        USpotLightComponent* SL = Cast<USpotLightComponent>(Light);
        if (!SL)
        {
            GEditor->EndTransaction();
            return MakeError(TEXT("INVALID_PARAMS"),
                TEXT("inner_cone_angle은 SpotLight에서만 사용 가능합니다."));
        }
        double Val = 0.0;
        Params->TryGetNumberField(TEXT("value"), Val);
        SL->SetInnerConeAngle(static_cast<float>(Val));
        bHandled = true;
    }
    else if (PropertyName == TEXT("outer_cone_angle"))
    {
        USpotLightComponent* SL = Cast<USpotLightComponent>(Light);
        if (!SL)
        {
            GEditor->EndTransaction();
            return MakeError(TEXT("INVALID_PARAMS"),
                TEXT("outer_cone_angle은 SpotLight에서만 사용 가능합니다."));
        }
        double Val = 44.0;
        Params->TryGetNumberField(TEXT("value"), Val);
        SL->SetOuterConeAngle(static_cast<float>(Val));
        bHandled = true;
    }

    if (!bHandled)
    {
        GEditor->EndTransaction();
        return MakeError(
            TEXT("INVALID_PARAMS"),
            FString::Printf(
                TEXT("지원하지 않는 라이트 속성: '%s'. 지원 목록: intensity, light_color, cast_shadows, temperature, use_temperature, attenuation_radius, inner_cone_angle, outer_cone_angle"),
                *PropertyName
            )
        );
    }

    GEditor->EndTransaction();
    return MakeSuccess(LightComponentToJson(Actor, Light));
}

// ─────────────────────────────────────────────────
// get_light_property
// ─────────────────────────────────────────────────

TSharedPtr<FJsonObject> FLightHandler::HandleGetLightProperty(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    FString ActorName = Params->GetStringField(TEXT("name")).TrimStartAndEnd();
    AActor* Actor = FindActorByName(World, ActorName);
    if (!IsValid(Actor))
    {
        return MakeError(
            TEXT("ACTOR_NOT_FOUND"),
            FString::Printf(TEXT("액터 '%s'를 찾을 수 없습니다."), *ActorName)
        );
    }

    ULightComponent* Light = Actor->FindComponentByClass<ULightComponent>();
    if (!Light)
    {
        return MakeError(
            TEXT("INVALID_ACTOR"),
            FString::Printf(TEXT("'%s'는 라이트 컴포넌트가 없습니다."), *ActorName)
        );
    }

    return MakeSuccess(LightComponentToJson(Actor, Light));
}
