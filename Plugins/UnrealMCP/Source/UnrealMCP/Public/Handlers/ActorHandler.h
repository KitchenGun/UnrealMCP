#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Phase 1 Actor & Scene 명령 처리기.
 *
 * create_actor, delete_actor, set_actor_transform, get_actors_in_level,
 * find_actors_by_name, duplicate_actor, get_actor_properties, set_actor_property
 * 명령을 담당한다.
 *
 * 주의: 모든 public 메서드는 반드시 GameThread에서 호출해야 한다.
 */
class FActorHandler
{
public:
    /** create_actor 처리 */
    static TSharedPtr<FJsonObject> HandleCreateActor(const TSharedPtr<FJsonObject>& Params);
    /** delete_actor 처리 */
    static TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Params);
    /** set_actor_transform 처리 */
    static TSharedPtr<FJsonObject> HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);
    /** get_actors_in_level 처리 */
    static TSharedPtr<FJsonObject> HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params);
    /** find_actors_by_name 처리 */
    static TSharedPtr<FJsonObject> HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
    /** duplicate_actor 처리 */
    static TSharedPtr<FJsonObject> HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params);
    /** get_actor_properties 처리 */
    static TSharedPtr<FJsonObject> HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params);
    /** set_actor_property 처리 */
    static TSharedPtr<FJsonObject> HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params);

private:
    /** 에디터 월드 반환. PIE 중에도 에디터 월드를 반환한다. */
    static UWorld* GetEditorWorld();

    /**
     * 이름(ActorLabel 또는 Name)으로 액터를 검색한다.
     * ActorLabel 우선, 없으면 Name으로 재검색한다.
     */
    static AActor* FindActorByName(UWorld* World, const FString& Name);

    /**
     * 액터 클래스 이름 문자열에서 UClass*를 반환한다.
     * 내장 맵 우선 조회 후, StaticLoadClass로 폴백한다.
     */
    static UClass* FindActorClass(const FString& ClassName);

    /** JSON 배열 [X, Y, Z]를 FVector로 변환 */
    static FVector JsonArrayToVector(const TArray<TSharedPtr<FJsonValue>>& Arr,
                                     const FVector& Default = FVector::ZeroVector);

    /** FVector를 {"x":..., "y":..., "z":...} JSON 오브젝트로 변환 */
    static TSharedPtr<FJsonObject> VectorToJson(const FVector& V);

    /** FRotator를 {"pitch":..., "yaw":..., "roll":...} JSON 오브젝트로 변환 */
    static TSharedPtr<FJsonObject> RotatorToJson(const FRotator& R);

    /** 액터 정보를 JSON 오브젝트로 변환 */
    static TSharedPtr<FJsonObject> ActorToJson(AActor* Actor);

    /** 성공 응답 JSON 생성 */
    static TSharedPtr<FJsonObject> MakeSuccess(TSharedPtr<FJsonObject> Result);

    /** 에러 응답 JSON 생성 */
    static TSharedPtr<FJsonObject> MakeError(const FString& Code, const FString& Message);
};
