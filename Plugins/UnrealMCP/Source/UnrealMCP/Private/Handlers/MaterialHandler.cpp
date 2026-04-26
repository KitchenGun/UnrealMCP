#include "Handlers/MaterialHandler.h"

// UE5 에디터 헤더
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

// 머티리얼
#include "MaterialShared.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialInstanceConstant.h"

// 팩토리 / 임포트
#include "Factories/MaterialFactoryNew.h"
#include "AssetImportTask.h"
#include "Factories/Factory.h"

// 에셋 도구
#include "ObjectTools.h"
#include "PackageTools.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

// 액터 / 컴포넌트
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"

// JSON
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

// ─────────────────────────────────────────────────────────────────────────────
// 공개 진입점
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMaterialHandler::HandleCommand(
    const FString& CommandType,
    const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("search_assets"))            return SearchAssets(Params);
    if (CommandType == TEXT("get_asset_details"))        return GetAssetDetails(Params);
    if (CommandType == TEXT("create_material"))          return CreateMaterial(Params);
    if (CommandType == TEXT("add_material_expression"))  return AddMaterialExpression(Params);
    if (CommandType == TEXT("connect_material_nodes"))   return ConnectMaterialNodes(Params);
    if (CommandType == TEXT("apply_material_to_actor"))  return ApplyMaterialToActor(Params);
    if (CommandType == TEXT("set_material_parameter"))   return SetMaterialParameter(Params);
    if (CommandType == TEXT("import_asset"))             return ImportAsset(Params);
    if (CommandType == TEXT("duplicate_asset"))          return DuplicateAsset(Params);
    if (CommandType == TEXT("delete_asset"))             return DeleteAsset(Params);

    return MakeError(TEXT("INVALID_PARAMS"),
        FString::Printf(TEXT("MaterialHandler: 알 수 없는 커맨드 '%s'"), *CommandType));
}

// ─────────────────────────────────────────────────────────────────────────────
// 헬퍼
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMaterialHandler::MakeSuccess(TSharedPtr<FJsonObject> Result)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result.IsValid() ? Result : MakeShared<FJsonObject>());
    Response->SetField(TEXT("error"), MakeShared<FJsonValueNull>());
    return Response;
}

TSharedPtr<FJsonObject> FMaterialHandler::MakeError(const FString& Code, const FString& Message)
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

// ─────────────────────────────────────────────────────────────────────────────
// search_assets
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMaterialHandler::SearchAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString Query       = Params->GetStringField(TEXT("query")).TrimStartAndEnd();
    FString ClassFilter = Params->GetStringField(TEXT("asset_class_filter")).TrimStartAndEnd();
    FString SearchPath  = Params->GetStringField(TEXT("search_path")).TrimStartAndEnd();

    if (SearchPath.IsEmpty())
    {
        SearchPath = TEXT("/Game");
    }

    // Phase A C++ pagination
    int32 PageLimit = 0;
    int32 PageOffset = 0;
    {
        double V = 0.0;
        if (Params->TryGetNumberField(TEXT("limit"), V) && V > 0.0)
        {
            PageLimit = static_cast<int32>(V);
        }
        if (Params->TryGetNumberField(TEXT("offset"), V) && V >= 0.0)
        {
            PageOffset = static_cast<int32>(V);
        }
    }

    FAssetRegistryModule& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    FARFilter Filter;
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add(FName(*SearchPath));

    if (!ClassFilter.IsEmpty())
    {
        Filter.ClassPaths.Add(FTopLevelAssetPath(FName(TEXT("/Script/Engine")), FName(*ClassFilter)));
    }

    TArray<FAssetData> AssetDataList;
    AssetRegistry.Get().GetAssets(Filter, AssetDataList);

    // Two-phase: count matches first, then slice for pagination.
    int32 MatchedTotal = 0;
    TArray<TSharedPtr<FJsonValue>> ResultArray;
    for (const FAssetData& Asset : AssetDataList)
    {
        FString AssetName = Asset.AssetName.ToString();
        // 이름 쿼리 필터링 (대소문자 무시)
        if (!Query.IsEmpty() && !AssetName.Contains(Query, ESearchCase::IgnoreCase))
        {
            continue;
        }

        const int32 MatchIndex = MatchedTotal;
        MatchedTotal++;

        // pagination window check
        if (MatchIndex < PageOffset)
        {
            continue;
        }
        if (PageLimit > 0 && ResultArray.Num() >= PageLimit)
        {
            continue;  // keep counting matched, stop appending
        }

        TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
        AssetObj->SetStringField(TEXT("name"),        AssetName);
        AssetObj->SetStringField(TEXT("asset_class"), Asset.AssetClassPath.GetAssetName().ToString());
        AssetObj->SetStringField(TEXT("package_path"), Asset.PackagePath.ToString());
        AssetObj->SetStringField(TEXT("object_path"),  Asset.GetSoftObjectPath().ToString());
        ResultArray.Add(MakeShared<FJsonValueObject>(AssetObj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("assets"), ResultArray);
    Result->SetNumberField(TEXT("count"), ResultArray.Num());
    Result->SetNumberField(TEXT("total_count"), MatchedTotal);
    Result->SetNumberField(TEXT("returned"), ResultArray.Num());
    const int32 NextOffset = PageOffset + ResultArray.Num();
    Result->SetNumberField(TEXT("next_offset"), NextOffset < MatchedTotal ? NextOffset : -1);
    Result->SetBoolField(TEXT("has_more"), NextOffset < MatchedTotal);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// get_asset_details
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMaterialHandler::GetAssetDetails(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath = Params->GetStringField(TEXT("asset_path")).TrimStartAndEnd();

    FAssetRegistryModule& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    // 경로로 에셋 데이터 조회
    FAssetData AssetData = AssetRegistry.Get().GetAssetByObjectPath(
        FSoftObjectPath(AssetPath));

    if (!AssetData.IsValid())
    {
        return MakeError(TEXT("ASSET_NOT_FOUND"),
            FString::Printf(TEXT("에셋을 찾을 수 없습니다: '%s'"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"),         AssetData.AssetName.ToString());
    Result->SetStringField(TEXT("asset_class"),  AssetData.AssetClassPath.GetAssetName().ToString());
    Result->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
    Result->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());
    Result->SetStringField(TEXT("object_path"),  AssetData.GetSoftObjectPath().ToString());

    // 태그/메타데이터 수집
    TSharedPtr<FJsonObject> TagsObj = MakeShared<FJsonObject>();
    TArray<UObject::FAssetRegistryTag> Tags;
    UObject* LoadedAsset = AssetData.GetAsset();
    if (LoadedAsset)
    {
        LoadedAsset->GetAssetRegistryTags(Tags);
        for (const UObject::FAssetRegistryTag& Tag : Tags)
        {
            TagsObj->SetStringField(Tag.Name.ToString(), Tag.Value);
        }
    }
    Result->SetObjectField(TEXT("tags"), TagsObj);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// create_material
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMaterialHandler::CreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString Name           = Params->GetStringField(TEXT("name")).TrimStartAndEnd();
    FString SavePath       = Params->GetStringField(TEXT("save_path")).TrimStartAndEnd();
    FString DomainStr      = Params->GetStringField(TEXT("material_domain")).TrimStartAndEnd();
    FString BlendModeStr   = Params->GetStringField(TEXT("blend_mode")).TrimStartAndEnd();

    if (Name.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("머티리얼 이름이 비어있습니다."));
    }
    if (SavePath.IsEmpty())
    {
        SavePath = TEXT("/Game/Materials");
    }

    // 패키지 경로 생성
    FString PackagePath = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
    FString PackageName = UPackageTools::SanitizePackageName(PackagePath);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("패키지 생성 실패."));
    }

    // 머티리얼 팩토리로 생성
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* NewMaterial = Cast<UMaterial>(
        Factory->FactoryCreateNew(UMaterial::StaticClass(), Package, FName(*Name),
                                   RF_Public | RF_Standalone, nullptr, GWarn));

    if (!IsValid(NewMaterial))
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("머티리얼 생성 실패."));
    }

    // 머티리얼 도메인 설정
    static const TMap<FString, EMaterialDomain> DomainMap = {
        { TEXT("Surface"),         MD_Surface },
        { TEXT("Deferred Decal"),  MD_DeferredDecal },
        { TEXT("Light Function"),  MD_LightFunction },
        { TEXT("PostProcess"),     MD_PostProcess },
        { TEXT("UI"),              MD_UI },
        { TEXT("Volume"),          MD_Volume },
    };
    if (const EMaterialDomain* Domain = DomainMap.Find(DomainStr))
    {
        NewMaterial->MaterialDomain = *Domain;
    }

    // 블렌드 모드 설정
    static const TMap<FString, EBlendMode> BlendMap = {
        { TEXT("Opaque"),       BLEND_Opaque },
        { TEXT("Masked"),       BLEND_Masked },
        { TEXT("Translucent"),  BLEND_Translucent },
        { TEXT("Additive"),     BLEND_Additive },
        { TEXT("Modulate"),     BLEND_Modulate },
    };
    if (const EBlendMode* Blend = BlendMap.Find(BlendModeStr))
    {
        NewMaterial->BlendMode = *Blend;
    }

    // 에셋 저장 및 레지스트리 등록
    FAssetRegistryModule::AssetCreated(NewMaterial);
    NewMaterial->MarkPackageDirty();
    Package->SetDirtyFlag(true);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"),        Name);
    Result->SetStringField(TEXT("package_path"), SavePath);
    Result->SetStringField(TEXT("object_path"),  PackageName + TEXT(".") + Name);
    Result->SetStringField(TEXT("material_domain"), DomainStr.IsEmpty() ? TEXT("Surface") : DomainStr);
    Result->SetStringField(TEXT("blend_mode"),   BlendModeStr.IsEmpty() ? TEXT("Opaque") : BlendModeStr);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// add_material_expression
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMaterialHandler::AddMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName   = Params->GetStringField(TEXT("material_name")).TrimStartAndEnd();
    FString ExpressionType = Params->GetStringField(TEXT("expression_type")).TrimStartAndEnd();
    float PosX = static_cast<float>(Params->GetNumberField(TEXT("position_x")));
    float PosY = static_cast<float>(Params->GetNumberField(TEXT("position_y")));
    TSharedPtr<FJsonObject> ExprParams = Params->HasField(TEXT("expression_params"))
        ? Params->GetObjectField(TEXT("expression_params"))
        : MakeShared<FJsonObject>();

    // 에셋 로드
    UMaterial* Material = Cast<UMaterial>(
        StaticLoadObject(UMaterial::StaticClass(), nullptr, *MaterialName));
    if (!IsValid(Material))
    {
        // 에셋 레지스트리에서 경로 조회
        FAssetRegistryModule& AssetRegistry =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> Assets;
        FARFilter Filter;
        Filter.ClassPaths.Add(FTopLevelAssetPath(FName(TEXT("/Script/Engine")), FName(TEXT("Material"))));
        Filter.bRecursivePaths = true;
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        AssetRegistry.Get().GetAssets(Filter, Assets);
        Assets = Assets.FilterByPredicate([&MaterialName](const FAssetData& AD)
        {
            return AD.AssetName == FName(*MaterialName);
        });
        if (Assets.Num() > 0)
        {
            Material = Cast<UMaterial>(Assets[0].GetAsset());
        }
    }

    if (!IsValid(Material))
    {
        return MakeError(TEXT("ASSET_NOT_FOUND"),
            FString::Printf(TEXT("머티리얼 '%s'를 찾을 수 없습니다."), *MaterialName));
    }

    // 표현식 타입에 따라 노드 생성
    UMaterialExpression* NewExpression = nullptr;

    if (ExpressionType == TEXT("Constant"))
    {
        UMaterialExpressionConstant* Expr =
            NewObject<UMaterialExpressionConstant>(Material);
        double Val = 0.0;
        ExprParams->TryGetNumberField(TEXT("value"), Val);
        Expr->R = static_cast<float>(Val);
        NewExpression = Expr;
    }
    else if (ExpressionType == TEXT("Constant3Vector"))
    {
        UMaterialExpressionConstant3Vector* Expr =
            NewObject<UMaterialExpressionConstant3Vector>(Material);
        double R = 0.0, G = 0.0, B = 0.0;
        ExprParams->TryGetNumberField(TEXT("r"), R);
        ExprParams->TryGetNumberField(TEXT("g"), G);
        ExprParams->TryGetNumberField(TEXT("b"), B);
        Expr->Constant = FLinearColor(
            static_cast<float>(R), static_cast<float>(G), static_cast<float>(B));
        NewExpression = Expr;
    }
    else if (ExpressionType == TEXT("Constant4Vector"))
    {
        UMaterialExpressionConstant4Vector* Expr =
            NewObject<UMaterialExpressionConstant4Vector>(Material);
        double R = 0.0, G = 0.0, B = 0.0, A = 1.0;
        ExprParams->TryGetNumberField(TEXT("r"), R);
        ExprParams->TryGetNumberField(TEXT("g"), G);
        ExprParams->TryGetNumberField(TEXT("b"), B);
        ExprParams->TryGetNumberField(TEXT("a"), A);
        Expr->Constant = FLinearColor(
            static_cast<float>(R), static_cast<float>(G),
            static_cast<float>(B), static_cast<float>(A));
        NewExpression = Expr;
    }
    else if (ExpressionType == TEXT("ScalarParameter"))
    {
        UMaterialExpressionScalarParameter* Expr =
            NewObject<UMaterialExpressionScalarParameter>(Material);
        FString ParamName = TEXT("Scalar");
        ExprParams->TryGetStringField(TEXT("parameter_name"), ParamName);
        Expr->ParameterName = FName(*ParamName);
        double DefVal = 0.0;
        ExprParams->TryGetNumberField(TEXT("default_value"), DefVal);
        Expr->DefaultValue = static_cast<float>(DefVal);
        NewExpression = Expr;
    }
    else if (ExpressionType == TEXT("VectorParameter"))
    {
        UMaterialExpressionVectorParameter* Expr =
            NewObject<UMaterialExpressionVectorParameter>(Material);
        FString ParamName = TEXT("Vector");
        ExprParams->TryGetStringField(TEXT("parameter_name"), ParamName);
        Expr->ParameterName = FName(*ParamName);
        double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
        ExprParams->TryGetNumberField(TEXT("r"), R);
        ExprParams->TryGetNumberField(TEXT("g"), G);
        ExprParams->TryGetNumberField(TEXT("b"), B);
        ExprParams->TryGetNumberField(TEXT("a"), A);
        Expr->DefaultValue = FLinearColor(
            static_cast<float>(R), static_cast<float>(G),
            static_cast<float>(B), static_cast<float>(A));
        NewExpression = Expr;
    }
    else if (ExpressionType == TEXT("TextureSample"))
    {
        UMaterialExpressionTextureSample* Expr =
            NewObject<UMaterialExpressionTextureSample>(Material);
        NewExpression = Expr;
    }
    else if (ExpressionType == TEXT("TextureSampleParameter2D"))
    {
        UMaterialExpressionTextureSampleParameter2D* Expr =
            NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
        FString ParamName = TEXT("Texture");
        ExprParams->TryGetStringField(TEXT("parameter_name"), ParamName);
        Expr->ParameterName = FName(*ParamName);
        NewExpression = Expr;
    }
    else if (ExpressionType == TEXT("Multiply"))
    {
        NewExpression = NewObject<UMaterialExpressionMultiply>(Material);
    }
    else if (ExpressionType == TEXT("Add"))
    {
        NewExpression = NewObject<UMaterialExpressionAdd>(Material);
    }
    else if (ExpressionType == TEXT("Lerp"))
    {
        NewExpression = NewObject<UMaterialExpressionLinearInterpolate>(Material);
    }
    else if (ExpressionType == TEXT("Fresnel"))
    {
        NewExpression = NewObject<UMaterialExpressionFresnel>(Material);
    }
    else
    {
        return MakeError(TEXT("INVALID_PARAMS"),
            FString::Printf(TEXT("지원하지 않는 표현식 타입: '%s'"), *ExpressionType));
    }

    // 위치 설정 및 머티리얼에 등록
    NewExpression->MaterialExpressionEditorX = static_cast<int32>(PosX);
    NewExpression->MaterialExpressionEditorY = static_cast<int32>(PosY);
    Material->GetExpressionCollection().AddExpression(NewExpression);
    Material->MarkPackageDirty();

    // 표현식 ID = 인덱스 기반 문자열
    int32 ExprIdx = Material->GetExpressionCollection().Expressions.Num() - 1;
    FString ExpressionId = FString::Printf(TEXT("%d"), ExprIdx);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("expression_id"),   ExpressionId);
    Result->SetStringField(TEXT("expression_type"), ExpressionType);
    Result->SetNumberField(TEXT("position_x"),      PosX);
    Result->SetNumberField(TEXT("position_y"),      PosY);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// connect_material_nodes
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMaterialHandler::ConnectMaterialNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName     = Params->GetStringField(TEXT("material_name")).TrimStartAndEnd();
    FString SourceExprId     = Params->GetStringField(TEXT("source_expression_id")).TrimStartAndEnd();
    FString SourceOutputName = Params->GetStringField(TEXT("source_output_name")).TrimStartAndEnd();
    FString TargetExprId     = Params->GetStringField(TEXT("target_expression_id")).TrimStartAndEnd();
    FString TargetInputName  = Params->GetStringField(TEXT("target_input_name")).TrimStartAndEnd();

    // 머티리얼 로드
    UMaterial* Material = nullptr;
    {
        FAssetRegistryModule& AR =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> Assets;
        FARFilter Filter;
        Filter.ClassPaths.Add(FTopLevelAssetPath(FName(TEXT("/Script/Engine")), FName(TEXT("Material"))));
        Filter.bRecursivePaths = true;
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        AR.Get().GetAssets(Filter, Assets);
        Assets = Assets.FilterByPredicate([&MaterialName](const FAssetData& AD)
        {
            return AD.AssetName == FName(*MaterialName);
        });
        if (Assets.Num() > 0)
        {
            Material = Cast<UMaterial>(Assets[0].GetAsset());
        }
    }

    if (!IsValid(Material))
    {
        return MakeError(TEXT("ASSET_NOT_FOUND"),
            FString::Printf(TEXT("머티리얼 '%s'를 찾을 수 없습니다."), *MaterialName));
    }

    const TArray<TObjectPtr<UMaterialExpression>>& Expressions =
        Material->GetExpressionCollection().Expressions;

    // 소스 표현식 조회
    UMaterialExpression* SourceExpr = nullptr;
    if (SourceExprId != TEXT("__result__"))
    {
        int32 Idx = FCString::Atoi(*SourceExprId);
        if (Expressions.IsValidIndex(Idx))
        {
            SourceExpr = Expressions[Idx];
        }
    }

    if (!SourceExpr && SourceExprId != TEXT("__result__"))
    {
        return MakeError(TEXT("INVALID_PARAMS"),
            FString::Printf(TEXT("소스 표현식 ID '%s'가 유효하지 않습니다."), *SourceExprId));
    }

    // 타깃이 머티리얼 결과 노드인 경우 → UMaterialEditingLibrary 활용
    if (TargetExprId == TEXT("__result__"))
    {
        bool bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(
            SourceExpr,
            SourceOutputName,
            static_cast<EMaterialProperty>(
                // 주요 핀 이름 → EMaterialProperty 매핑
                TargetInputName == TEXT("BaseColor")      ? MP_BaseColor :
                TargetInputName == TEXT("Metallic")       ? MP_Metallic :
                TargetInputName == TEXT("Roughness")      ? MP_Roughness :
                TargetInputName == TEXT("Normal")         ? MP_Normal :
                TargetInputName == TEXT("EmissiveColor")  ? MP_EmissiveColor :
                TargetInputName == TEXT("Opacity")        ? MP_Opacity :
                TargetInputName == TEXT("AmbientOcclusion") ? MP_AmbientOcclusion :
                MP_BaseColor  // 기본값
            )
        );

        if (!bConnected)
        {
            return MakeError(TEXT("INTERNAL_ERROR"),
                FString::Printf(TEXT("머티리얼 결과 노드 연결 실패: '%s'"), *TargetInputName));
        }
    }
    else
    {
        // 표현식 간 연결
        int32 TargetIdx = FCString::Atoi(*TargetExprId);
        if (!Expressions.IsValidIndex(TargetIdx))
        {
            return MakeError(TEXT("INVALID_PARAMS"),
                FString::Printf(TEXT("타깃 표현식 ID '%s'가 유효하지 않습니다."), *TargetExprId));
        }

        UMaterialExpression* TargetExpr = Expressions[TargetIdx];
        bool bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(
            SourceExpr, SourceOutputName,
            TargetExpr, TargetInputName);

        if (!bConnected)
        {
            return MakeError(TEXT("INTERNAL_ERROR"),
                FString::Printf(TEXT("표현식 연결 실패: %s -> %s"),
                    *SourceExprId, *TargetExprId));
        }
    }

    Material->MarkPackageDirty();
    UMaterialEditingLibrary::RecompileMaterial(Material);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("source_expression_id"), SourceExprId);
    Result->SetStringField(TEXT("target_expression_id"), TargetExprId);
    Result->SetStringField(TEXT("target_input"),         TargetInputName);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// apply_material_to_actor
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMaterialHandler::ApplyMaterialToActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName    = Params->GetStringField(TEXT("actor_name")).TrimStartAndEnd();
    FString MaterialPath = Params->GetStringField(TEXT("material_path")).TrimStartAndEnd();
    int32   SlotIndex    = 0;
    Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

    // 에디터 월드에서 액터 탐색
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    AActor* TargetActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
            It->GetName().Equals(ActorName, ESearchCase::IgnoreCase))
        {
            TargetActor = *It;
            break;
        }
    }

    if (!IsValid(TargetActor))
    {
        return MakeError(TEXT("ACTOR_NOT_FOUND"),
            FString::Printf(TEXT("액터 '%s'를 찾을 수 없습니다."), *ActorName));
    }

    // 머티리얼 로드
    UMaterialInterface* Material = Cast<UMaterialInterface>(
        StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialPath));
    if (!IsValid(Material))
    {
        return MakeError(TEXT("ASSET_NOT_FOUND"),
            FString::Printf(TEXT("머티리얼 '%s'를 찾을 수 없습니다."), *MaterialPath));
    }

    // StaticMeshComponent 탐색 및 적용
    UStaticMeshComponent* SMC = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!IsValid(SMC))
    {
        return MakeError(TEXT("INVALID_PARAMS"),
            FString::Printf(TEXT("액터 '%s'에 StaticMeshComponent가 없습니다."), *ActorName));
    }

    GEditor->BeginTransaction(FText::FromString(
        FString::Printf(TEXT("MCP: 머티리얼 적용 '%s'"), *ActorName)));
    SMC->Modify();
    SMC->SetMaterial(SlotIndex, Material);
    GEditor->EndTransaction();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actor"),    ActorName);
    Result->SetStringField(TEXT("material"), MaterialPath);
    Result->SetNumberField(TEXT("slot"),     SlotIndex);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// set_material_parameter
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMaterialHandler::SetMaterialParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName  = Params->GetStringField(TEXT("material_name")).TrimStartAndEnd();
    FString ParamName     = Params->GetStringField(TEXT("parameter_name")).TrimStartAndEnd();
    FString ParamType     = Params->GetStringField(TEXT("parameter_type")).TrimStartAndEnd();
    TSharedPtr<FJsonValue> ValueField = Params->TryGetField(TEXT("value"));

    // 머티리얼 로드
    UMaterial* Material = nullptr;
    {
        FAssetRegistryModule& AR =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> Assets;
        FARFilter Filter;
        Filter.ClassPaths.Add(FTopLevelAssetPath(FName(TEXT("/Script/Engine")), FName(TEXT("Material"))));
        Filter.bRecursivePaths = true;
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        AR.Get().GetAssets(Filter, Assets);
        Assets = Assets.FilterByPredicate([&MaterialName](const FAssetData& AD)
        {
            return AD.AssetName == FName(*MaterialName);
        });
        if (Assets.Num() > 0)
        {
            Material = Cast<UMaterial>(Assets[0].GetAsset());
        }
    }

    if (!IsValid(Material))
    {
        return MakeError(TEXT("ASSET_NOT_FOUND"),
            FString::Printf(TEXT("머티리얼 '%s'를 찾을 수 없습니다."), *MaterialName));
    }

    bool bSet = false;

    if (ParamType.Equals(TEXT("Scalar"), ESearchCase::IgnoreCase))
    {
        float Value = ValueField.IsValid() ? static_cast<float>(ValueField->AsNumber()) : 0.f;
        bSet = UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(
            Cast<UMaterialInstanceConstant>(Material), FName(*ParamName), Value);

        // UMaterial 자체의 ScalarParameter 기본값 변경
        if (!bSet)
        {
            for (UMaterialExpression* Expr : Material->GetExpressionCollection().Expressions)
            {
                UMaterialExpressionScalarParameter* SP =
                    Cast<UMaterialExpressionScalarParameter>(Expr);
                if (SP && SP->ParameterName == FName(*ParamName))
                {
                    SP->DefaultValue = Value;
                    bSet = true;
                    break;
                }
            }
        }
    }
    else if (ParamType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
    {
        FLinearColor Color = FLinearColor::White;
        if (ValueField.IsValid() && ValueField->Type == EJson::Object)
        {
            TSharedPtr<FJsonObject> ColorObj = ValueField->AsObject();
            double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
            ColorObj->TryGetNumberField(TEXT("r"), R);
            ColorObj->TryGetNumberField(TEXT("g"), G);
            ColorObj->TryGetNumberField(TEXT("b"), B);
            ColorObj->TryGetNumberField(TEXT("a"), A);
            Color = FLinearColor(
                static_cast<float>(R), static_cast<float>(G),
                static_cast<float>(B), static_cast<float>(A));
        }

        bSet = UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(
            Cast<UMaterialInstanceConstant>(Material), FName(*ParamName), Color);

        if (!bSet)
        {
            for (UMaterialExpression* Expr : Material->GetExpressionCollection().Expressions)
            {
                UMaterialExpressionVectorParameter* VP =
                    Cast<UMaterialExpressionVectorParameter>(Expr);
                if (VP && VP->ParameterName == FName(*ParamName))
                {
                    VP->DefaultValue = Color;
                    bSet = true;
                    break;
                }
            }
        }
    }
    else
    {
        return MakeError(TEXT("INVALID_PARAMS"),
            FString::Printf(TEXT("지원하지 않는 파라미터 타입: '%s'. 'Scalar' 또는 'Vector' 사용."), *ParamType));
    }

    if (bSet)
    {
        Material->MarkPackageDirty();
        UMaterialEditingLibrary::RecompileMaterial(Material);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("material"),        MaterialName);
    Result->SetStringField(TEXT("parameter_name"),  ParamName);
    Result->SetStringField(TEXT("parameter_type"),  ParamType);
    Result->SetBoolField(TEXT("updated"),           bSet);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// import_asset
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMaterialHandler::ImportAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourceFilePath  = Params->GetStringField(TEXT("source_file_path")).TrimStartAndEnd();
    FString DestinationPath = Params->GetStringField(TEXT("destination_path")).TrimStartAndEnd();
    FString AssetName       = Params->GetStringField(TEXT("asset_name")).TrimStartAndEnd();

    if (SourceFilePath.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("source_file_path가 비어있습니다."));
    }
    if (DestinationPath.IsEmpty())
    {
        DestinationPath = TEXT("/Game/Imports");
    }

    // 에셋 이름 미지정 시 파일명 사용
    if (AssetName.IsEmpty())
    {
        AssetName = FPaths::GetBaseFilename(SourceFilePath);
    }

    FAssetToolsModule& AssetToolsModule =
        FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    Task->Filename        = SourceFilePath;
    Task->DestinationPath = DestinationPath;
    Task->DestinationName = AssetName;
    Task->bSave           = true;
    Task->bAutomated      = true;
    Task->bReplaceExisting = false;

    TArray<UAssetImportTask*> Tasks = { Task };
    AssetToolsModule.Get().ImportAssetTasks(Tasks);

    TArray<UObject*> ImportedAssets = Task->GetObjects();
    if (ImportedAssets.IsEmpty())
    {
        return MakeError(TEXT("IMPORT_FAILED"),
            FString::Printf(TEXT("파일 '%s' 임포트에 실패했습니다."), *SourceFilePath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_name"),      AssetName);
    Result->SetStringField(TEXT("destination_path"), DestinationPath);
    Result->SetStringField(TEXT("source_file"),     SourceFilePath);
    Result->SetNumberField(TEXT("imported_count"),  ImportedAssets.Num());
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// duplicate_asset
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMaterialHandler::DuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath      = Params->GetStringField(TEXT("source_asset_path")).TrimStartAndEnd();
    FString NewName         = Params->GetStringField(TEXT("new_name")).TrimStartAndEnd();
    FString DestinationPath = Params->GetStringField(TEXT("destination_path")).TrimStartAndEnd();

    if (SourcePath.IsEmpty() || NewName.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"),
            TEXT("source_asset_path 또는 new_name이 비어있습니다."));
    }

    // 원본 에셋 로드
    UObject* SourceAsset = StaticLoadObject(UObject::StaticClass(), nullptr, *SourcePath);
    if (!IsValid(SourceAsset))
    {
        return MakeError(TEXT("ASSET_NOT_FOUND"),
            FString::Printf(TEXT("원본 에셋 '%s'를 찾을 수 없습니다."), *SourcePath));
    }

    // 복제 대상 경로
    FString TargetPath = DestinationPath.IsEmpty()
        ? FPackageName::GetLongPackagePath(SourcePath)
        : DestinationPath;

    FAssetToolsModule& AssetToolsModule =
        FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

    UObject* DupAsset = AssetToolsModule.Get().DuplicateAsset(NewName, TargetPath, SourceAsset);
    if (!IsValid(DupAsset))
    {
        return MakeError(TEXT("INTERNAL_ERROR"),
            FString::Printf(TEXT("에셋 복제 실패: '%s'"), *SourcePath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("source"),           SourcePath);
    Result->SetStringField(TEXT("new_name"),          NewName);
    Result->SetStringField(TEXT("destination_path"),  TargetPath);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// delete_asset
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMaterialHandler::DeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath  = Params->GetStringField(TEXT("asset_path")).TrimStartAndEnd();
    bool bForceDelete  = false;
    Params->TryGetBoolField(TEXT("force_delete"), bForceDelete);

    if (AssetPath.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("asset_path가 비어있습니다."));
    }

    UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
    if (!IsValid(Asset))
    {
        return MakeError(TEXT("ASSET_NOT_FOUND"),
            FString::Printf(TEXT("에셋 '%s'를 찾을 수 없습니다."), *AssetPath));
    }

    TArray<UObject*> AssetsToDelete = { Asset };
    int32 DeletedCount = ObjectTools::DeleteObjects(AssetsToDelete, !bForceDelete);

    if (DeletedCount == 0)
    {
        return MakeError(TEXT("DELETE_FAILED"),
            FString::Printf(TEXT("에셋 삭제 실패 (참조 중일 수 있습니다): '%s'"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("deleted"),       AssetPath);
    Result->SetNumberField(TEXT("deleted_count"),  DeletedCount);
    return MakeSuccess(Result);
}
