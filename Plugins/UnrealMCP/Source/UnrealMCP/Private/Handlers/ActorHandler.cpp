#include "Handlers/ActorHandler.h"

#include "Editor.h"
#include "EditorActorFolders.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Misc/Char.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyAccessUtil.h"

// ─────────────────────────────────────────────────
// 내부 헬퍼
// ─────────────────────────────────────────────────

UWorld* FActorHandler::GetEditorWorld()
{
    if (GEditor)
    {
        return GEditor->GetEditorWorldContext().World();
    }
    return nullptr;
}

AActor* FActorHandler::FindActorByName(UWorld* World, const FString& Name)
{
    if (!World || Name.IsEmpty())
    {
        return nullptr;
    }

    // 1순위: ActorLabel 일치 (에디터 표시 이름)
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase))
        {
            return *It;
        }
    }

    // 2순위: 내부 Name 일치
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetName().Equals(Name, ESearchCase::IgnoreCase))
        {
            return *It;
        }
    }

    return nullptr;
}

UClass* FActorHandler::FindActorClass(const FString& ClassName)
{
    // 자주 사용하는 액터 클래스 빠른 조회 테이블
    static const TMap<FString, UClass*> ClassMap = {
        { TEXT("StaticMeshActor"),      AStaticMeshActor::StaticClass() },
        { TEXT("PointLight"),           APointLight::StaticClass()       },
        { TEXT("SpotLight"),            ASpotLight::StaticClass()        },
        { TEXT("DirectionalLight"),     ADirectionalLight::StaticClass() },
        { TEXT("CameraActor"),          ACameraActor::StaticClass()      },
        { TEXT("SkyLight"),             ASkyLight::StaticClass()         },
        { TEXT("RectLight"),            ARectLight::StaticClass()        },
        { TEXT("ExponentialHeightFog"), AExponentialHeightFog::StaticClass() },
    };

    if (const UClass* const* Found = ClassMap.Find(ClassName))
    {
        return const_cast<UClass*>(*Found);
    }

    // 폴백: 엔진/게임 에셋에서 동적 조회
    // 예: "/Script/Engine.StaticMeshActor" 형식 지원
    UClass* DynamicClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
    if (!DynamicClass)
    {
        // 패키지 경로 없이 이름만 주어진 경우 전체 검색
        FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
        DynamicClass = LoadClass<AActor>(nullptr, *FullPath);
    }
    return DynamicClass;
}

FVector FActorHandler::JsonArrayToVector(const TArray<TSharedPtr<FJsonValue>>& Arr,
                                          const FVector& Default)
{
    if (Arr.Num() < 3)
    {
        return Default;
    }
    return FVector(
        static_cast<float>(Arr[0]->AsNumber()),
        static_cast<float>(Arr[1]->AsNumber()),
        static_cast<float>(Arr[2]->AsNumber())
    );
}

TSharedPtr<FJsonObject> FActorHandler::VectorToJson(const FVector& V)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("x"), V.X);
    Obj->SetNumberField(TEXT("y"), V.Y);
    Obj->SetNumberField(TEXT("z"), V.Z);
    return Obj;
}

TSharedPtr<FJsonObject> FActorHandler::RotatorToJson(const FRotator& R)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("pitch"), R.Pitch);
    Obj->SetNumberField(TEXT("yaw"), R.Yaw);
    Obj->SetNumberField(TEXT("roll"), R.Roll);
    return Obj;
}

TSharedPtr<FJsonObject> FActorHandler::ActorToJson(AActor* Actor)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    if (!Actor)
    {
        return Obj;
    }

    Obj->SetStringField(TEXT("name"), Actor->GetActorLabel());
    Obj->SetStringField(TEXT("actor_class"), Actor->GetClass()->GetName());

    // Transform
    Obj->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));
    Obj->SetObjectField(TEXT("rotation"), RotatorToJson(Actor->GetActorRotation()));
    Obj->SetObjectField(TEXT("scale"),    VectorToJson(Actor->GetActorScale3D()));

    return Obj;
}

TSharedPtr<FJsonObject> FActorHandler::MakeSuccess(TSharedPtr<FJsonObject> Result)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result.IsValid() ? Result : MakeShared<FJsonObject>());
    Response->SetField(TEXT("error"), MakeShared<FJsonValueNull>());
    return Response;
}

TSharedPtr<FJsonObject> FActorHandler::MakeError(const FString& Code, const FString& Message)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), false);
    Response->SetField(TEXT("result"), MakeShared<FJsonValueNull>());

    TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
    ErrorObj->SetStringField(TEXT("code"), Code);
    ErrorObj->SetStringField(TEXT("message"), Message);
    Response->SetObjectField(TEXT("error"), ErrorObj);
    return Response;
}

// ─────────────────────────────────────────────────
// create_actor
// ─────────────────────────────────────────────────

TSharedPtr<FJsonObject> FActorHandler::HandleCreateActor(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    // 파라미터 추출
    FString ActorClassName = Params->GetStringField(TEXT("actor_class"));
    FString Name           = Params->GetStringField(TEXT("name"));

    TArray<TSharedPtr<FJsonValue>> LocArr = Params->GetArrayField(TEXT("location"));
    TArray<TSharedPtr<FJsonValue>> RotArr = Params->GetArrayField(TEXT("rotation"));
    TArray<TSharedPtr<FJsonValue>> ScaleArr = Params->GetArrayField(TEXT("scale"));

    FVector  Location = JsonArrayToVector(LocArr);
    FRotator Rotation = FRotator(
        RotArr.Num() >= 3 ? static_cast<float>(RotArr[0]->AsNumber()) : 0.f,
        RotArr.Num() >= 3 ? static_cast<float>(RotArr[1]->AsNumber()) : 0.f,
        RotArr.Num() >= 3 ? static_cast<float>(RotArr[2]->AsNumber()) : 0.f
    );
    FVector Scale = JsonArrayToVector(ScaleArr, FVector::OneVector);

    // 클래스 조회
    UClass* ActorClass = FindActorClass(ActorClassName);
    if (!ActorClass)
    {
        return MakeError(
            TEXT("INVALID_PARAMS"),
            FString::Printf(TEXT("액터 클래스 '%s'를 찾을 수 없습니다."), *ActorClassName)
        );
    }

    // 트랜잭션 시작 (Undo 지원)
    GEditor->BeginTransaction(FText::FromString(FString::Printf(TEXT("MCP: 액터 생성 '%s'"), *Name)));

    // 스폰 파라미터 설정
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (!Name.IsEmpty())
    {
        SpawnParams.Name = FName(*Name);
    }

    AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);

    if (!IsValid(NewActor))
    {
        GEditor->EndTransaction();
        return MakeError(TEXT("INTERNAL_ERROR"),
            FString::Printf(TEXT("액터 스폰 실패. 클래스: '%s'"), *ActorClassName));
    }

    // 라벨 설정 (에디터 표시 이름)
    if (!Name.IsEmpty())
    {
        NewActor->SetActorLabel(Name);
    }

    // 스케일 설정 (SpawnActor는 스케일을 직접 지원하지 않음)
    NewActor->SetActorScale3D(Scale);

    GEditor->EndTransaction();

    // 응답 구성
    TSharedPtr<FJsonObject> Result = ActorToJson(NewActor);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────
// delete_actor
// ─────────────────────────────────────────────────

TSharedPtr<FJsonObject> FActorHandler::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    FString Name = Params->GetStringField(TEXT("name"));
    AActor* Actor = FindActorByName(World, Name);

    if (!IsValid(Actor))
    {
        return MakeError(
            TEXT("ACTOR_NOT_FOUND"),
            FString::Printf(TEXT("액터 '%s'를 찾을 수 없습니다. get_actors_in_level로 현재 레벨의 액터 목록을 확인하세요."), *Name)
        );
    }

    GEditor->BeginTransaction(FText::FromString(FString::Printf(TEXT("MCP: 액터 삭제 '%s'"), *Name)));

    FString DeletedLabel = Actor->GetActorLabel();
    World->DestroyActor(Actor);

    GEditor->EndTransaction();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("deleted"), DeletedLabel);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────
// set_actor_transform
// ─────────────────────────────────────────────────

TSharedPtr<FJsonObject> FActorHandler::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    FString Name = Params->GetStringField(TEXT("name"));
    AActor* Actor = FindActorByName(World, Name);

    if (!IsValid(Actor))
    {
        return MakeError(
            TEXT("ACTOR_NOT_FOUND"),
            FString::Printf(TEXT("액터 '%s'를 찾을 수 없습니다."), *Name)
        );
    }

    GEditor->BeginTransaction(FText::FromString(FString::Printf(TEXT("MCP: 트랜스폼 설정 '%s'"), *Name)));
    Actor->Modify();

    // location 필드가 있는 경우에만 적용
    if (Params->HasField(TEXT("location")))
    {
        TArray<TSharedPtr<FJsonValue>> LocArr = Params->GetArrayField(TEXT("location"));
        Actor->SetActorLocation(JsonArrayToVector(LocArr));
    }

    // rotation 필드가 있는 경우에만 적용
    if (Params->HasField(TEXT("rotation")))
    {
        TArray<TSharedPtr<FJsonValue>> RotArr = Params->GetArrayField(TEXT("rotation"));
        if (RotArr.Num() >= 3)
        {
            FRotator NewRot(
                static_cast<float>(RotArr[0]->AsNumber()),
                static_cast<float>(RotArr[1]->AsNumber()),
                static_cast<float>(RotArr[2]->AsNumber())
            );
            Actor->SetActorRotation(NewRot);
        }
    }

    // scale 필드가 있는 경우에만 적용
    if (Params->HasField(TEXT("scale")))
    {
        TArray<TSharedPtr<FJsonValue>> ScaleArr = Params->GetArrayField(TEXT("scale"));
        Actor->SetActorScale3D(JsonArrayToVector(ScaleArr, FVector::OneVector));
    }

    GEditor->EndTransaction();

    return MakeSuccess(ActorToJson(Actor));
}

// ─────────────────────────────────────────────────
// get_actors_in_level
// ─────────────────────────────────────────────────

TSharedPtr<FJsonObject> FActorHandler::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    FString ClassFilter = Params->GetStringField(TEXT("actor_class_filter")).TrimStartAndEnd();

    TArray<TSharedPtr<FJsonValue>> ActorList;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        // 시스템 내부 액터 제외 (WorldSettings, Brush 등)
        if (Actor->IsHiddenEd() && Actor->IsEditorOnly())
        {
            continue;
        }

        // 클래스 필터 적용
        if (!ClassFilter.IsEmpty() &&
            !Actor->GetClass()->GetName().Contains(ClassFilter, ESearchCase::IgnoreCase))
        {
            continue;
        }

        ActorList.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor)));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("actors"), ActorList);
    Result->SetNumberField(TEXT("count"), ActorList.Num());
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────
// find_actors_by_name
// ─────────────────────────────────────────────────

TSharedPtr<FJsonObject> FActorHandler::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    FString Pattern = Params->GetStringField(TEXT("pattern")).TrimStartAndEnd();
    if (Pattern.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("검색 패턴이 비어있습니다."));
    }

    TArray<TSharedPtr<FJsonValue>> FoundList;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor))
        {
            continue;
        }

        const FString Label = Actor->GetActorLabel();
        const FString InternalName = Actor->GetName();

        // 대소문자 무시 부분 일치
        if (Label.Contains(Pattern, ESearchCase::IgnoreCase) ||
            InternalName.Contains(Pattern, ESearchCase::IgnoreCase))
        {
            FoundList.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor)));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("actors"), FoundList);
    Result->SetNumberField(TEXT("count"), FoundList.Num());
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────
// duplicate_actor
// ─────────────────────────────────────────────────

TSharedPtr<FJsonObject> FActorHandler::HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    FString Name    = Params->GetStringField(TEXT("name"));
    FString NewName = Params->GetStringField(TEXT("new_name"));

    TArray<TSharedPtr<FJsonValue>> OffsetArr = Params->GetArrayField(TEXT("offset"));
    FVector Offset = JsonArrayToVector(OffsetArr, FVector(100.f, 0.f, 0.f));

    AActor* SourceActor = FindActorByName(World, Name);
    if (!IsValid(SourceActor))
    {
        return MakeError(
            TEXT("ACTOR_NOT_FOUND"),
            FString::Printf(TEXT("액터 '%s'를 찾을 수 없습니다."), *Name)
        );
    }

    GEditor->BeginTransaction(FText::FromString(FString::Printf(TEXT("MCP: 액터 복제 '%s'"), *Name)));

    // 현재 선택 저장
    TArray<AActor*> PrevSelection;
    for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
    {
        if (AActor* A = Cast<AActor>(*It))
        {
            PrevSelection.Add(A);
        }
    }

    // 원본 액터 선택 후 복제
    GEditor->SelectNone(false, true, false);
    GEditor->SelectActor(SourceActor, true, false, true);
    GEditor->edactDuplicateSelected(SourceActor->GetLevel(), false, true, true, Offset);

    // 복제된 액터 참조 (복제 후 선택된 액터)
    AActor* DupActor = nullptr;
    for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
    {
        if (AActor* A = Cast<AActor>(*It))
        {
            DupActor = A;
            break;
        }
    }

    // 이름 설정
    if (IsValid(DupActor) && !NewName.IsEmpty())
    {
        DupActor->SetActorLabel(NewName);
    }

    // 이전 선택 복원
    GEditor->SelectNone(false, true, false);
    for (AActor* A : PrevSelection)
    {
        if (IsValid(A))
        {
            GEditor->SelectActor(A, true, false, false);
        }
    }

    GEditor->EndTransaction();

    if (!IsValid(DupActor))
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("액터 복제에 실패했습니다."));
    }

    return MakeSuccess(ActorToJson(DupActor));
}

// ─────────────────────────────────────────────────
// get_actor_properties
// ─────────────────────────────────────────────────

TSharedPtr<FJsonObject> FActorHandler::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    FString Name = Params->GetStringField(TEXT("name"));
    AActor* Actor = FindActorByName(World, Name);

    if (!IsValid(Actor))
    {
        return MakeError(
            TEXT("ACTOR_NOT_FOUND"),
            FString::Printf(TEXT("액터 '%s'를 찾을 수 없습니다."), *Name)
        );
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    // 기본 정보
    Result->SetStringField(TEXT("name"),       Actor->GetActorLabel());
    Result->SetStringField(TEXT("actor_class"), Actor->GetClass()->GetName());
    Result->SetBoolField(TEXT("bHidden"),      Actor->IsHidden());

    // Transform
    Result->SetObjectField(TEXT("location"), VectorToJson(Actor->GetActorLocation()));
    Result->SetObjectField(TEXT("rotation"), RotatorToJson(Actor->GetActorRotation()));
    Result->SetObjectField(TEXT("scale"),    VectorToJson(Actor->GetActorScale3D()));

    // 태그
    TArray<TSharedPtr<FJsonValue>> TagArray;
    for (const FName& Tag : Actor->Tags)
    {
        TagArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
    }
    Result->SetArrayField(TEXT("tags"), TagArray);

    // CustomDepthStencilValue (렌더링 관련)
    Result->SetNumberField(TEXT("custom_depth_stencil_value"), Actor->CustomDepthStencilValue);

    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────
// set_actor_property
// ─────────────────────────────────────────────────

TSharedPtr<FJsonObject> FActorHandler::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorld();
    if (!World)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    FString ActorName    = Params->GetStringField(TEXT("name"));
    FString PropertyName = Params->GetStringField(TEXT("property_name")).TrimStartAndEnd();

    AActor* Actor = FindActorByName(World, ActorName);
    if (!IsValid(Actor))
    {
        return MakeError(
            TEXT("ACTOR_NOT_FOUND"),
            FString::Printf(TEXT("액터 '%s'를 찾을 수 없습니다."), *ActorName)
        );
    }

    GEditor->BeginTransaction(FText::FromString(
        FString::Printf(TEXT("MCP: 액터 속성 설정 '%s.%s'"), *ActorName, *PropertyName)
    ));
    Actor->Modify();

    TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("property_value"));
    bool bHandled = false;

    // ── 자주 사용하는 속성: 직접 처리 ─────────────────────────
    if (PropertyName.Equals(TEXT("bHidden"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("bHiddenInGame"), ESearchCase::IgnoreCase))
    {
        bool bHidden = Value.IsValid() ? Value->AsBool() : false;
        Actor->SetActorHiddenInGame(bHidden);
        Actor->bHidden = bHidden;
        bHandled = true;
    }
    else if (PropertyName.Equals(TEXT("ActorLabel"), ESearchCase::IgnoreCase))
    {
        FString NewLabel = Value.IsValid() ? Value->AsString() : TEXT("");
        if (!NewLabel.IsEmpty())
        {
            Actor->SetActorLabel(NewLabel);
            bHandled = true;
        }
    }
    else if (PropertyName.Equals(TEXT("Tags"), ESearchCase::IgnoreCase))
    {
        // 태그 배열로 설정
        Actor->Tags.Empty();
        if (Value.IsValid() && Value->Type == EJson::Array)
        {
            TArray<TSharedPtr<FJsonValue>> TagArr = Value->AsArray();
            for (const TSharedPtr<FJsonValue>& TagVal : TagArr)
            {
                Actor->Tags.Add(FName(*TagVal->AsString()));
            }
        }
        bHandled = true;
    }
    else if (PropertyName.Equals(TEXT("CustomDepthStencilValue"), ESearchCase::IgnoreCase))
    {
        if (Value.IsValid())
        {
            Actor->CustomDepthStencilValue = static_cast<int32>(Value->AsNumber());
            Actor->MarkPackageDirty();
            bHandled = true;
        }
    }

    // ── 폴백: UE 리플렉션 시스템으로 속성 탐색 ──────────────────
    if (!bHandled)
    {
        FProperty* Prop = FindFProperty<FProperty>(Actor->GetClass(), *PropertyName);
        if (!Prop)
        {
            GEditor->EndTransaction();
            return MakeError(
                TEXT("INVALID_PARAMS"),
                FString::Printf(TEXT("속성 '%s'를 찾을 수 없습니다. 지원 속성: bHidden, ActorLabel, Tags, CustomDepthStencilValue"), *PropertyName)
            );
        }

        // bool 타입
        if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
        {
            BoolProp->SetPropertyValue_InContainer(Actor, Value.IsValid() ? Value->AsBool() : false);
            bHandled = true;
        }
        // int32 타입
        else if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
        {
            IntProp->SetPropertyValue_InContainer(Actor, Value.IsValid() ? static_cast<int32>(Value->AsNumber()) : 0);
            bHandled = true;
        }
        // float 타입
        else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
        {
            FloatProp->SetPropertyValue_InContainer(Actor, Value.IsValid() ? static_cast<float>(Value->AsNumber()) : 0.f);
            bHandled = true;
        }
        // FString 타입
        else if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
        {
            StrProp->SetPropertyValue_InContainer(Actor, Value.IsValid() ? Value->AsString() : TEXT(""));
            bHandled = true;
        }

        if (bHandled)
        {
            Actor->MarkPackageDirty();
        }
    }

    GEditor->EndTransaction();

    if (!bHandled)
    {
        return MakeError(
            TEXT("INVALID_PARAMS"),
            FString::Printf(TEXT("속성 '%s'의 타입이 지원되지 않습니다."), *PropertyName)
        );
    }

    // 변경된 속성 포함하여 전체 속성 반환
    return HandleGetActorProperties(Params);
}
