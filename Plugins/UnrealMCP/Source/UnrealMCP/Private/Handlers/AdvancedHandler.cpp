#include "Handlers/AdvancedHandler.h"

// UE5 에디터 헤더
#include "Editor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "PackageTools.h"

// Blueprint
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Factories/AnimBlueprintFactory.h"

// Widget Blueprint
#include "Blueprint/UserWidget.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

// DataTable
#include "Engine/DataTable.h"
#include "Factories/DataTableFactory.h"

// DataAsset
#include "Engine/DataAsset.h"
#include "Factories/DataAssetFactory.h"

// Niagara
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
// UNiagaraSystemFactoryNew은 API 매크로 없음 → StaticLoadClass로 런타임 로드
#include "Factories/Factory.h"

// UObject 리플렉션
#include "UObject/UnrealType.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

// JSON
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

// ─────────────────────────────────────────────────────────────────────────────
// 공개 진입점
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAdvancedHandler::HandleCommand(
    const FString& CommandType,
    const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_niagara_system"))       return CreateNiagaraSystem(Params);
    if (CommandType == TEXT("create_animation_blueprint"))  return CreateAnimationBlueprint(Params);
    if (CommandType == TEXT("create_widget_blueprint"))     return CreateWidgetBlueprint(Params);
    if (CommandType == TEXT("create_data_table"))           return CreateDataTable(Params);
    if (CommandType == TEXT("create_data_asset"))           return CreateDataAsset(Params);
    if (CommandType == TEXT("inspect_uobject"))             return InspectUObject(Params);

    return MakeError(TEXT("INVALID_PARAMS"),
        FString::Printf(TEXT("AdvancedHandler: 알 수 없는 커맨드 '%s'"), *CommandType));
}

// ─────────────────────────────────────────────────────────────────────────────
// 헬퍼
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAdvancedHandler::MakeSuccess(TSharedPtr<FJsonObject> Result)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result.IsValid() ? Result : MakeShared<FJsonObject>());
    Response->SetField(TEXT("error"), MakeShared<FJsonValueNull>());
    return Response;
}

TSharedPtr<FJsonObject> FAdvancedHandler::MakeError(const FString& Code, const FString& Message)
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
// create_niagara_system
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAdvancedHandler::CreateNiagaraSystem(const TSharedPtr<FJsonObject>& Params)
{
    FString Name     = Params->GetStringField(TEXT("name")).TrimStartAndEnd();
    FString SavePath = Params->GetStringField(TEXT("save_path")).TrimStartAndEnd();
    FString Template = Params->GetStringField(TEXT("template")).TrimStartAndEnd();

    if (Name.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("Niagara System 이름이 비어있습니다."));
    }
    if (SavePath.IsEmpty()) SavePath = TEXT("/Game/VFX");

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
    FString PackageName = UPackageTools::SanitizePackageName(PackagePath);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("패키지 생성 실패."));
    }

    // UE5.5: UNiagaraSystemFactoryNew는 API 매크로 없어 직접 링크 불가 → 런타임 로드
    UClass* FactoryClass = StaticLoadClass(
        UFactory::StaticClass(), nullptr,
        TEXT("/Script/NiagaraEditor.NiagaraSystemFactoryNew"));
    if (!FactoryClass)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("NiagaraEditor 팩토리 클래스를 찾을 수 없습니다."));
    }
    UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
    UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(
        Factory->FactoryCreateNew(UNiagaraSystem::StaticClass(), Package, FName(*Name),
                                   RF_Public | RF_Standalone, nullptr, GWarn));

    if (!IsValid(NiagaraSystem))
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("Niagara System 생성 실패."));
    }

    FAssetRegistryModule::AssetCreated(NiagaraSystem);
    NiagaraSystem->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"),         Name);
    Result->SetStringField(TEXT("package_path"), SavePath);
    Result->SetStringField(TEXT("object_path"),  PackageName + TEXT(".") + Name);
    Result->SetStringField(TEXT("template"),     Template.IsEmpty() ? TEXT("Empty") : Template);
    Result->SetStringField(TEXT("note"),
        TEXT("이미터와 모듈은 UE5 Niagara 에디터에서 직접 추가해야 합니다."));
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// create_animation_blueprint
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAdvancedHandler::CreateAnimationBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString Name         = Params->GetStringField(TEXT("name")).TrimStartAndEnd();
    FString SavePath     = Params->GetStringField(TEXT("save_path")).TrimStartAndEnd();
    FString SkeletonPath = Params->GetStringField(TEXT("skeleton_path")).TrimStartAndEnd();
    FString ParentClass  = Params->GetStringField(TEXT("parent_class")).TrimStartAndEnd();

    if (Name.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("Animation Blueprint 이름이 비어있습니다."));
    }
    if (SavePath.IsEmpty()) SavePath = TEXT("/Game/Animations");
    if (ParentClass.IsEmpty()) ParentClass = TEXT("AnimInstance");

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
    FString PackageName = UPackageTools::SanitizePackageName(PackagePath);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("패키지 생성 실패."));
    }

    // Skeleton 로드 (옵션)
    USkeleton* Skeleton = nullptr;
    if (!SkeletonPath.IsEmpty())
    {
        Skeleton = Cast<USkeleton>(
            StaticLoadObject(USkeleton::StaticClass(), nullptr, *SkeletonPath));
    }

    // AnimBlueprintFactory로 생성
    UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
    Factory->TargetSkeleton = Skeleton;
    Factory->ParentClass = UAnimInstance::StaticClass();

    UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(
        Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(), Package, FName(*Name),
                                   RF_Public | RF_Standalone, nullptr, GWarn));

    if (!IsValid(AnimBP))
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("Animation Blueprint 생성 실패."));
    }

    FKismetEditorUtilities::CompileBlueprint(AnimBP);
    FAssetRegistryModule::AssetCreated(AnimBP);
    AnimBP->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"),         Name);
    Result->SetStringField(TEXT("package_path"), SavePath);
    Result->SetStringField(TEXT("object_path"),  PackageName + TEXT(".") + Name);
    Result->SetStringField(TEXT("skeleton"),     SkeletonPath);
    Result->SetStringField(TEXT("parent_class"), ParentClass);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// create_widget_blueprint
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAdvancedHandler::CreateWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString Name        = Params->GetStringField(TEXT("name")).TrimStartAndEnd();
    FString SavePath    = Params->GetStringField(TEXT("save_path")).TrimStartAndEnd();
    FString ParentClass = Params->GetStringField(TEXT("parent_class")).TrimStartAndEnd();

    if (Name.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("Widget Blueprint 이름이 비어있습니다."));
    }
    if (SavePath.IsEmpty()) SavePath = TEXT("/Game/UI");
    if (ParentClass.IsEmpty()) ParentClass = TEXT("UserWidget");

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
    FString PackageName = UPackageTools::SanitizePackageName(PackagePath);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("패키지 생성 실패."));
    }

    // 부모 클래스 결정
    UClass* WidgetParentClass = UUserWidget::StaticClass();

    // CommonUI 클래스 동적 조회
    if (!ParentClass.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase))
    {
        UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *ParentClass);
        if (!FoundClass)
        {
            FString FullPath = FString::Printf(TEXT("/Script/CommonUI.%s"), *ParentClass);
            FoundClass = LoadClass<UUserWidget>(nullptr, *FullPath);
        }
        if (FoundClass && FoundClass->IsChildOf(UUserWidget::StaticClass()))
        {
            WidgetParentClass = FoundClass;
        }
    }

    // WidgetBlueprintFactory로 생성
    UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
    Factory->ParentClass = WidgetParentClass;

    UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(
        Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(), Package, FName(*Name),
                                   RF_Public | RF_Standalone, nullptr, GWarn));

    if (!IsValid(WidgetBP))
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("Widget Blueprint 생성 실패."));
    }

    FKismetEditorUtilities::CompileBlueprint(WidgetBP);
    FAssetRegistryModule::AssetCreated(WidgetBP);
    WidgetBP->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"),         Name);
    Result->SetStringField(TEXT("package_path"), SavePath);
    Result->SetStringField(TEXT("object_path"),  PackageName + TEXT(".") + Name);
    Result->SetStringField(TEXT("parent_class"), ParentClass);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// create_data_table
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAdvancedHandler::CreateDataTable(const TSharedPtr<FJsonObject>& Params)
{
    FString Name       = Params->GetStringField(TEXT("name")).TrimStartAndEnd();
    FString RowStruct  = Params->GetStringField(TEXT("row_struct")).TrimStartAndEnd();
    FString SavePath   = Params->GetStringField(TEXT("save_path")).TrimStartAndEnd();

    if (Name.IsEmpty() || RowStruct.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("name 또는 row_struct가 비어있습니다."));
    }
    if (SavePath.IsEmpty()) SavePath = TEXT("/Game/Data");

    // Row 구조체 클래스 조회
    UScriptStruct* RowStructClass = FindObject<UScriptStruct>(ANY_PACKAGE, *RowStruct);
    if (!RowStructClass)
    {
        // 패키지 경로 시도
        FString FullPath = RowStruct.Contains(TEXT("/Script/"))
            ? RowStruct
            : FString::Printf(TEXT("/Script/Engine.%s"), *RowStruct);
        RowStructClass = LoadObject<UScriptStruct>(nullptr, *FullPath);
    }

    if (!RowStructClass)
    {
        // FTableRowBase 폴백
        RowStructClass = FTableRowBase::StaticStruct();
        UE_LOG(LogTemp, Warning, TEXT("[UnrealMCP] Row 구조체 '%s'를 찾지 못해 FTableRowBase를 사용합니다."), *RowStruct);
    }

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
    FString PackageName = UPackageTools::SanitizePackageName(PackagePath);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("패키지 생성 실패."));
    }

    UDataTableFactory* Factory = NewObject<UDataTableFactory>();
    Factory->Struct = RowStructClass;

    UDataTable* DataTable = Cast<UDataTable>(
        Factory->FactoryCreateNew(UDataTable::StaticClass(), Package, FName(*Name),
                                   RF_Public | RF_Standalone, nullptr, GWarn));

    if (!IsValid(DataTable))
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("DataTable 생성 실패."));
    }

    FAssetRegistryModule::AssetCreated(DataTable);
    DataTable->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"),         Name);
    Result->SetStringField(TEXT("package_path"), SavePath);
    Result->SetStringField(TEXT("object_path"),  PackageName + TEXT(".") + Name);
    Result->SetStringField(TEXT("row_struct"),   RowStructClass->GetName());
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// create_data_asset
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAdvancedHandler::CreateDataAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString Name        = Params->GetStringField(TEXT("name")).TrimStartAndEnd();
    FString AssetClass  = Params->GetStringField(TEXT("asset_class")).TrimStartAndEnd();
    FString SavePath    = Params->GetStringField(TEXT("save_path")).TrimStartAndEnd();

    if (Name.IsEmpty() || AssetClass.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("name 또는 asset_class가 비어있습니다."));
    }
    if (SavePath.IsEmpty()) SavePath = TEXT("/Game/Data");

    // DataAsset 서브클래스 조회
    UClass* DataAssetClass = FindObject<UClass>(ANY_PACKAGE, *AssetClass);
    if (!DataAssetClass)
    {
        FString FullPath = AssetClass.Contains(TEXT("/Script/"))
            ? AssetClass
            : FString::Printf(TEXT("/Script/Engine.%s"), *AssetClass);
        DataAssetClass = LoadClass<UDataAsset>(nullptr, *FullPath);
    }
    if (!DataAssetClass || !DataAssetClass->IsChildOf(UDataAsset::StaticClass()))
    {
        DataAssetClass = UPrimaryDataAsset::StaticClass();
        UE_LOG(LogTemp, Warning, TEXT("[UnrealMCP] DataAsset 클래스 '%s'를 찾지 못해 UPrimaryDataAsset을 사용합니다."), *AssetClass);
    }

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
    FString PackageName = UPackageTools::SanitizePackageName(PackagePath);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("패키지 생성 실패."));
    }

    UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
    Factory->DataAssetClass = DataAssetClass;

    UDataAsset* DataAsset = Cast<UDataAsset>(
        Factory->FactoryCreateNew(DataAssetClass, Package, FName(*Name),
                                   RF_Public | RF_Standalone, nullptr, GWarn));

    if (!IsValid(DataAsset))
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("DataAsset 생성 실패."));
    }

    FAssetRegistryModule::AssetCreated(DataAsset);
    DataAsset->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"),         Name);
    Result->SetStringField(TEXT("package_path"), SavePath);
    Result->SetStringField(TEXT("object_path"),  PackageName + TEXT(".") + Name);
    Result->SetStringField(TEXT("asset_class"),  DataAssetClass->GetName());
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// inspect_uobject
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAdvancedHandler::InspectUObject(const TSharedPtr<FJsonObject>& Params)
{
    FString ObjectPath        = Params->GetStringField(TEXT("object_path")).TrimStartAndEnd();
    bool    bIncludeProps     = true;
    bool    bIncludeFunctions = false;
    FString PropFilter;

    Params->TryGetBoolField(TEXT("include_properties"), bIncludeProps);
    Params->TryGetBoolField(TEXT("include_functions"),  bIncludeFunctions);
    Params->TryGetStringField(TEXT("property_filter"),  PropFilter);

    // Phase A C++ filters — limit serialization volume early
    int32 MaxProps = 0;
    int32 MaxFuncs = 0;
    {
        double V = 0.0;
        if (Params->TryGetNumberField(TEXT("max_properties"), V) && V > 0.0)
        {
            MaxProps = static_cast<int32>(V);
        }
        if (Params->TryGetNumberField(TEXT("max_functions"), V) && V > 0.0)
        {
            MaxFuncs = static_cast<int32>(V);
        }
    }

    // 클래스 또는 에셋에서 UClass 결정
    UClass* TargetClass = nullptr;
    UObject* LoadedObj = nullptr;

    // 1. 클래스 이름으로 직접 조회 시도
    TargetClass = FindObject<UClass>(ANY_PACKAGE, *ObjectPath);
    if (!TargetClass)
    {
        FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *ObjectPath);
        TargetClass = LoadClass<UObject>(nullptr, *FullPath);
    }

    // 2. 에셋 경로로 로드 시도
    if (!TargetClass)
    {
        LoadedObj = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
        if (IsValid(LoadedObj))
        {
            TargetClass = LoadedObj->GetClass();
        }
    }

    if (!TargetClass)
    {
        return MakeError(TEXT("ASSET_NOT_FOUND"),
            FString::Printf(TEXT("UObject/클래스 '%s'를 찾을 수 없습니다."), *ObjectPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("class_name"),     TargetClass->GetName());
    Result->SetStringField(TEXT("package"),        TargetClass->GetPackage()->GetName());

    // 클래스 계층
    TArray<TSharedPtr<FJsonValue>> Hierarchy;
    UClass* Current = TargetClass;
    while (Current)
    {
        Hierarchy.Add(MakeShared<FJsonValueString>(Current->GetName()));
        Current = Current->GetSuperClass();
    }
    Result->SetArrayField(TEXT("class_hierarchy"), Hierarchy);

    // UPROPERTY 목록
    if (bIncludeProps)
    {
        TArray<TSharedPtr<FJsonValue>> Properties;
        int32 MatchedProps = 0;
        for (TFieldIterator<FProperty> It(TargetClass); It; ++It)
        {
            FProperty* Prop = *It;
            FString PropName = Prop->GetName();

            if (!PropFilter.IsEmpty() &&
                !PropName.Contains(PropFilter, ESearchCase::IgnoreCase))
            {
                continue;
            }

            MatchedProps++;
            if (MaxProps > 0 && Properties.Num() >= MaxProps)
            {
                continue;  // keep counting matched, stop appending
            }

            TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
            PropObj->SetStringField(TEXT("name"),       PropName);
            PropObj->SetStringField(TEXT("type"),       Prop->GetClass()->GetName());
            PropObj->SetStringField(TEXT("cpp_type"),   Prop->GetCPPType());

            // BlueprintReadWrite / BlueprintReadOnly 플래그
            const bool bBPReadWrite = Prop->HasAnyPropertyFlags(CPF_BlueprintVisible);
            const bool bBPReadOnly  = Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly);
            PropObj->SetStringField(TEXT("blueprint_access"),
                bBPReadOnly  ? TEXT("ReadOnly") :
                bBPReadWrite ? TEXT("ReadWrite") : TEXT("None"));

            // EditAnywhere / VisibleAnywhere 플래그
            const bool bEditAnywhere = Prop->HasAnyPropertyFlags(CPF_Edit);
            PropObj->SetBoolField(TEXT("editable"), bEditAnywhere);

            Properties.Add(MakeShared<FJsonValueObject>(PropObj));
        }
        Result->SetArrayField(TEXT("properties"), Properties);
        Result->SetNumberField(TEXT("property_count"), Properties.Num());
        Result->SetNumberField(TEXT("total_property_count"), MatchedProps);
        if (MaxProps > 0 && MatchedProps > MaxProps)
        {
            Result->SetNumberField(TEXT("truncated_properties_at"), MaxProps);
        }
    }

    // UFUNCTION 목록
    if (bIncludeFunctions)
    {
        TArray<TSharedPtr<FJsonValue>> Functions;
        int32 MatchedFuncs = 0;
        for (TFieldIterator<UFunction> It(TargetClass); It; ++It)
        {
            UFunction* Func = *It;

            MatchedFuncs++;
            if (MaxFuncs > 0 && Functions.Num() >= MaxFuncs)
            {
                continue;
            }

            TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
            FuncObj->SetStringField(TEXT("name"), Func->GetName());

            // BlueprintCallable / BlueprintPure 여부
            const bool bCallable = Func->HasAnyFunctionFlags(FUNC_BlueprintCallable);
            const bool bPure     = Func->HasAnyFunctionFlags(FUNC_BlueprintPure);
            const bool bEvent    = Func->HasAnyFunctionFlags(FUNC_BlueprintEvent);

            FuncObj->SetBoolField(TEXT("callable"), bCallable);
            FuncObj->SetBoolField(TEXT("pure"),     bPure);
            FuncObj->SetBoolField(TEXT("event"),    bEvent);

            Functions.Add(MakeShared<FJsonValueObject>(FuncObj));
        }
        Result->SetArrayField(TEXT("functions"), Functions);
        Result->SetNumberField(TEXT("function_count"), Functions.Num());
        Result->SetNumberField(TEXT("total_function_count"), MatchedFuncs);
        if (MaxFuncs > 0 && MatchedFuncs > MaxFuncs)
        {
            Result->SetNumberField(TEXT("truncated_functions_at"), MaxFuncs);
        }
    }

    return MakeSuccess(Result);
}
