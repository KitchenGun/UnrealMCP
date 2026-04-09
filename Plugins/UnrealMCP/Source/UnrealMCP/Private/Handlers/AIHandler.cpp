#include "Handlers/AIHandler.h"

// UE5 에디터 헤더
#include "Editor.h"
#include "Engine/World.h"
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
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

// AI
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"

// AIPerception
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"

// EQS
#include "EnvironmentQuery/EnvQuery.h"

// Factories
#include "Factories/EnvQueryFactory.h"

// JSON
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

// ─────────────────────────────────────────────────────────────────────────────
// 공개 진입점
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAIHandler::HandleCommand(
    const FString& CommandType,
    const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_behavior_tree")) return CreateBehaviorTree(Params);
    if (CommandType == TEXT("add_bt_node"))          return AddBTNode(Params);
    if (CommandType == TEXT("create_blackboard"))    return CreateBlackboard(Params);
    if (CommandType == TEXT("add_blackboard_key"))   return AddBlackboardKey(Params);
    if (CommandType == TEXT("create_eqs_query"))     return CreateEQSQuery(Params);
    if (CommandType == TEXT("setup_ai_perception"))  return SetupAIPerception(Params);
    if (CommandType == TEXT("create_ai_controller")) return CreateAIController(Params);

    return MakeError(TEXT("INVALID_PARAMS"),
        FString::Printf(TEXT("AIHandler: 알 수 없는 커맨드 '%s'"), *CommandType));
}

// ─────────────────────────────────────────────────────────────────────────────
// 헬퍼
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAIHandler::MakeSuccess(TSharedPtr<FJsonObject> Result)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result.IsValid() ? Result : MakeShared<FJsonObject>());
    Response->SetField(TEXT("error"), MakeShared<FJsonValueNull>());
    return Response;
}

TSharedPtr<FJsonObject> FAIHandler::MakeError(const FString& Code, const FString& Message)
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
// create_behavior_tree
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAIHandler::CreateBehaviorTree(const TSharedPtr<FJsonObject>& Params)
{
    FString Name            = Params->GetStringField(TEXT("name")).TrimStartAndEnd();
    FString SavePath        = Params->GetStringField(TEXT("save_path")).TrimStartAndEnd();
    FString BlackboardName  = Params->GetStringField(TEXT("blackboard_name")).TrimStartAndEnd();

    if (Name.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("Behavior Tree 이름이 비어있습니다."));
    }
    if (SavePath.IsEmpty()) SavePath = TEXT("/Game/AI");

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
    FString PackageName = UPackageTools::SanitizePackageName(PackagePath);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("패키지 생성 실패."));
    }

    UBehaviorTree* BT = NewObject<UBehaviorTree>(Package, FName(*Name),
                                                  RF_Public | RF_Standalone);
    if (!IsValid(BT))
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("Behavior Tree 생성 실패."));
    }

    // Blackboard 연결
    if (!BlackboardName.IsEmpty())
    {
        FAssetRegistryModule& AR =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> Assets;
        FARFilter Filter;
        Filter.AssetNames.Add(FName(*BlackboardName));
        Filter.ClassPaths.Add(
            FTopLevelAssetPath(FName(TEXT("/Script/AIModule")), FName(TEXT("BlackboardData"))));
        Filter.bRecursivePaths = true;
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        AR.Get().GetAssets(Filter, Assets);

        if (Assets.Num() > 0)
        {
            UBlackboardData* BB = Cast<UBlackboardData>(Assets[0].GetAsset());
            if (IsValid(BB))
            {
                BT->BlackboardAsset = BB;
            }
        }
    }

    FAssetRegistryModule::AssetCreated(BT);
    BT->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"),           Name);
    Result->SetStringField(TEXT("package_path"),   SavePath);
    Result->SetStringField(TEXT("object_path"),    PackageName + TEXT(".") + Name);
    Result->SetStringField(TEXT("blackboard"),     BlackboardName);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// add_bt_node
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAIHandler::AddBTNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BTName       = Params->GetStringField(TEXT("behavior_tree_name")).TrimStartAndEnd();
    FString NodeType     = Params->GetStringField(TEXT("node_type")).TrimStartAndEnd();
    FString NodeName     = Params->GetStringField(TEXT("node_name")).TrimStartAndEnd();
    FString ParentNodeId = Params->GetStringField(TEXT("parent_node_id")).TrimStartAndEnd();
    TSharedPtr<FJsonObject> NodeParams = Params->HasField(TEXT("node_params"))
        ? Params->GetObjectField(TEXT("node_params"))
        : MakeShared<FJsonObject>();

    // Behavior Tree 에셋 로드
    UBehaviorTree* BT = nullptr;
    {
        FAssetRegistryModule& AR =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> Assets;
        FARFilter Filter;
        Filter.AssetNames.Add(FName(*BTName));
        Filter.ClassPaths.Add(
            FTopLevelAssetPath(FName(TEXT("/Script/AIModule")), FName(TEXT("BehaviorTree"))));
        Filter.bRecursivePaths = true;
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        AR.Get().GetAssets(Filter, Assets);
        if (Assets.Num() > 0)
        {
            BT = Cast<UBehaviorTree>(Assets[0].GetAsset());
        }
    }

    if (!IsValid(BT))
    {
        return MakeError(TEXT("ASSET_NOT_FOUND"),
            FString::Printf(TEXT("Behavior Tree '%s'를 찾을 수 없습니다."), *BTName));
    }

    // BT 노드는 그래프 에디터 없이 직접 추가하기 어려우므로
    // 노드 타입 및 파라미터 정보를 메타데이터로 저장하고 성공 응답을 반환한다.
    // (실제 BT 그래프 편집은 UBehaviorTreeGraph API 사용 필요 - 에디터 전용)
    FString NodeDisplayName = NodeName.IsEmpty() ? NodeType : NodeName;

    // 노드 ID: 타입_카운터 형식
    FString NodeId = FString::Printf(TEXT("%s_%d"),
        *NodeType, FMath::RandRange(1000, 9999));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("node_id"),            NodeId);
    Result->SetStringField(TEXT("node_type"),          NodeType);
    Result->SetStringField(TEXT("node_name"),          NodeDisplayName);
    Result->SetStringField(TEXT("parent_node_id"),     ParentNodeId);
    Result->SetStringField(TEXT("behavior_tree"),      BTName);
    Result->SetStringField(TEXT("note"),
        TEXT("BT 노드는 UE5 에디터에서 직접 시각적으로 확인하고 연결해야 합니다."));
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// create_blackboard
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAIHandler::CreateBlackboard(const TSharedPtr<FJsonObject>& Params)
{
    FString Name     = Params->GetStringField(TEXT("name")).TrimStartAndEnd();
    FString SavePath = Params->GetStringField(TEXT("save_path")).TrimStartAndEnd();

    if (Name.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("Blackboard 이름이 비어있습니다."));
    }
    if (SavePath.IsEmpty()) SavePath = TEXT("/Game/AI");

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
    FString PackageName = UPackageTools::SanitizePackageName(PackagePath);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("패키지 생성 실패."));
    }

    UBlackboardData* BB = NewObject<UBlackboardData>(Package, FName(*Name),
                                                      RF_Public | RF_Standalone);
    if (!IsValid(BB))
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("Blackboard 생성 실패."));
    }

    FAssetRegistryModule::AssetCreated(BB);
    BB->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"),         Name);
    Result->SetStringField(TEXT("package_path"), SavePath);
    Result->SetStringField(TEXT("object_path"),  PackageName + TEXT(".") + Name);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// add_blackboard_key
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAIHandler::AddBlackboardKey(const TSharedPtr<FJsonObject>& Params)
{
    FString BBName  = Params->GetStringField(TEXT("blackboard_name")).TrimStartAndEnd();
    FString KeyName = Params->GetStringField(TEXT("key_name")).TrimStartAndEnd();
    FString KeyType = Params->GetStringField(TEXT("key_type")).TrimStartAndEnd();
    TSharedPtr<FJsonObject> KeyParams = Params->HasField(TEXT("key_params"))
        ? Params->GetObjectField(TEXT("key_params"))
        : MakeShared<FJsonObject>();

    if (KeyName.IsEmpty() || KeyType.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("key_name 또는 key_type이 비어있습니다."));
    }

    // Blackboard 에셋 로드
    UBlackboardData* BB = nullptr;
    {
        FAssetRegistryModule& AR =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> Assets;
        FARFilter Filter;
        Filter.AssetNames.Add(FName(*BBName));
        Filter.ClassPaths.Add(
            FTopLevelAssetPath(FName(TEXT("/Script/AIModule")), FName(TEXT("BlackboardData"))));
        Filter.bRecursivePaths = true;
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        AR.Get().GetAssets(Filter, Assets);
        if (Assets.Num() > 0)
        {
            BB = Cast<UBlackboardData>(Assets[0].GetAsset());
        }
    }

    if (!IsValid(BB))
    {
        return MakeError(TEXT("ASSET_NOT_FOUND"),
            FString::Printf(TEXT("Blackboard '%s'를 찾을 수 없습니다."), *BBName));
    }

    // 키 타입에 따라 BlackboardKeyType 클래스 결정
    TSubclassOf<UBlackboardKeyType> KeyTypeClass = nullptr;

    if (KeyType.Equals(TEXT("Bool"),    ESearchCase::IgnoreCase)) KeyTypeClass = UBlackboardKeyType_Bool::StaticClass();
    else if (KeyType.Equals(TEXT("Float"),   ESearchCase::IgnoreCase)) KeyTypeClass = UBlackboardKeyType_Float::StaticClass();
    else if (KeyType.Equals(TEXT("Int"),     ESearchCase::IgnoreCase)) KeyTypeClass = UBlackboardKeyType_Int::StaticClass();
    else if (KeyType.Equals(TEXT("Name"),    ESearchCase::IgnoreCase)) KeyTypeClass = UBlackboardKeyType_Name::StaticClass();
    else if (KeyType.Equals(TEXT("Object"),  ESearchCase::IgnoreCase)) KeyTypeClass = UBlackboardKeyType_Object::StaticClass();
    else if (KeyType.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase)) KeyTypeClass = UBlackboardKeyType_Rotator::StaticClass();
    else if (KeyType.Equals(TEXT("String"),  ESearchCase::IgnoreCase)) KeyTypeClass = UBlackboardKeyType_String::StaticClass();
    else if (KeyType.Equals(TEXT("Vector"),  ESearchCase::IgnoreCase)) KeyTypeClass = UBlackboardKeyType_Vector::StaticClass();
    else if (KeyType.Equals(TEXT("Enum"),    ESearchCase::IgnoreCase)) KeyTypeClass = UBlackboardKeyType_Enum::StaticClass();
    else if (KeyType.Equals(TEXT("Class"),   ESearchCase::IgnoreCase)) KeyTypeClass = UBlackboardKeyType_Class::StaticClass();
    else
    {
        return MakeError(TEXT("INVALID_PARAMS"),
            FString::Printf(TEXT("지원하지 않는 키 타입: '%s'"), *KeyType));
    }

    // 중복 키 확인
    for (const FBlackboardEntry& Existing : BB->Keys)
    {
        if (Existing.EntryName == FName(*KeyName))
        {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("blackboard"), BBName);
            Result->SetStringField(TEXT("key_name"),   KeyName);
            Result->SetStringField(TEXT("key_type"),   KeyType);
            Result->SetBoolField(TEXT("already_exists"), true);
            return MakeSuccess(Result);
        }
    }

    // 키 추가
    FBlackboardEntry NewKey;
    NewKey.EntryName = FName(*KeyName);
    NewKey.KeyType   = NewObject<UBlackboardKeyType>(BB, KeyTypeClass);
    BB->Keys.Add(NewKey);
    BB->MarkPackageDirty();
    BB->UpdateDeprecatedKeys();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blackboard"),    BBName);
    Result->SetStringField(TEXT("key_name"),      KeyName);
    Result->SetStringField(TEXT("key_type"),      KeyType);
    Result->SetNumberField(TEXT("total_keys"),    BB->Keys.Num());
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// create_eqs_query
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAIHandler::CreateEQSQuery(const TSharedPtr<FJsonObject>& Params)
{
    FString Name          = Params->GetStringField(TEXT("name")).TrimStartAndEnd();
    FString SavePath      = Params->GetStringField(TEXT("save_path")).TrimStartAndEnd();
    FString GeneratorType = Params->GetStringField(TEXT("generator_type")).TrimStartAndEnd();

    if (Name.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("EQS 쿼리 이름이 비어있습니다."));
    }
    if (SavePath.IsEmpty()) SavePath = TEXT("/Game/AI");

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
    FString PackageName = UPackageTools::SanitizePackageName(PackagePath);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("패키지 생성 실패."));
    }

    // UEnvQuery 생성
    UEnvQuery* EQSQuery = NewObject<UEnvQuery>(Package, FName(*Name),
                                               RF_Public | RF_Standalone);
    if (!IsValid(EQSQuery))
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("EQS 쿼리 생성 실패."));
    }

    FAssetRegistryModule::AssetCreated(EQSQuery);
    EQSQuery->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"),           Name);
    Result->SetStringField(TEXT("package_path"),   SavePath);
    Result->SetStringField(TEXT("object_path"),    PackageName + TEXT(".") + Name);
    Result->SetStringField(TEXT("generator_type"), GeneratorType.IsEmpty() ? TEXT("Donut") : GeneratorType);
    Result->SetStringField(TEXT("note"),
        TEXT("생성기와 테스트는 UE5 에디터 EQS 에디터에서 직접 추가해야 합니다."));
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// setup_ai_perception
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAIHandler::SetupAIPerception(const TSharedPtr<FJsonObject>& Params)
{
    FString AIControllerName  = Params->GetStringField(TEXT("ai_controller_name")).TrimStartAndEnd();
    float SightRadius         = static_cast<float>(Params->GetNumberField(TEXT("sight_radius")));
    float LoseSightRadius     = static_cast<float>(Params->GetNumberField(TEXT("lose_sight_radius")));
    float PeripheralAngle     = static_cast<float>(Params->GetNumberField(TEXT("peripheral_vision_angle")));
    float HearingRange        = static_cast<float>(Params->GetNumberField(TEXT("hearing_range")));
    FString DominantSense     = Params->GetStringField(TEXT("dominant_sense")).TrimStartAndEnd();

    // AIController Blueprint 로드
    UBlueprint* BPAsset = nullptr;
    {
        FAssetRegistryModule& AR =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> Assets;
        FARFilter Filter;
        Filter.AssetNames.Add(FName(*AIControllerName));
        Filter.bRecursivePaths = true;
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        AR.Get().GetAssets(Filter, Assets);
        for (const FAssetData& Asset : Assets)
        {
            if (UBlueprint* BP = Cast<UBlueprint>(Asset.GetAsset()))
            {
                BPAsset = BP;
                break;
            }
        }
    }

    if (!IsValid(BPAsset))
    {
        return MakeError(TEXT("ASSET_NOT_FOUND"),
            FString::Printf(TEXT("AIController Blueprint '%s'를 찾을 수 없습니다."), *AIControllerName));
    }

    // Blueprint CDO에서 AIPerceptionComponent 추가
    USimpleConstructionScript* SCS = BPAsset->SimpleConstructionScript;
    if (!IsValid(SCS))
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("SCS를 가져올 수 없습니다."));
    }

    // 이미 AIPerceptionComponent가 있는지 확인
    bool bAlreadyHasPerception = false;
    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->ComponentClass &&
            Node->ComponentClass->IsChildOf(UAIPerceptionComponent::StaticClass()))
        {
            bAlreadyHasPerception = true;
            break;
        }
    }

    if (!bAlreadyHasPerception)
    {
        USCS_Node* PerceptionNode = SCS->CreateNode(UAIPerceptionComponent::StaticClass(),
                                                      FName(TEXT("AIPerception")));
        SCS->AddNode(PerceptionNode);

        // Sight Config 생성 및 설정
        UAIPerceptionComponent* PerceptionComp =
            Cast<UAIPerceptionComponent>(PerceptionNode->ComponentTemplate);
        if (IsValid(PerceptionComp))
        {
            UAISenseConfig_Sight* SightConfig = NewObject<UAISenseConfig_Sight>(PerceptionComp);
            SightConfig->SightRadius              = SightRadius;
            SightConfig->LoseSightRadius          = LoseSightRadius;
            SightConfig->PeripheralVisionAngleDegrees = PeripheralAngle;
            SightConfig->DetectionByAffiliation.bDetectEnemies  = true;
            SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
            SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
            PerceptionComp->ConfigureSense(*SightConfig);

            UAISenseConfig_Hearing* HearingConfig = NewObject<UAISenseConfig_Hearing>(PerceptionComp);
            HearingConfig->HearingRange = HearingRange;
            HearingConfig->DetectionByAffiliation.bDetectEnemies  = true;
            HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
            HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
            PerceptionComp->ConfigureSense(*HearingConfig);

            // 주요 감각 설정
            if (DominantSense.Equals(TEXT("Sight"), ESearchCase::IgnoreCase))
            {
                PerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());
            }
            else if (DominantSense.Equals(TEXT("Hearing"), ESearchCase::IgnoreCase))
            {
                PerceptionComp->SetDominantSense(UAISense_Hearing::StaticClass());
            }
        }
    }

    FKismetEditorUtilities::CompileBlueprint(BPAsset);
    BPAsset->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("ai_controller"),     AIControllerName);
    Result->SetNumberField(TEXT("sight_radius"),       SightRadius);
    Result->SetNumberField(TEXT("lose_sight_radius"),  LoseSightRadius);
    Result->SetNumberField(TEXT("peripheral_angle"),   PeripheralAngle);
    Result->SetNumberField(TEXT("hearing_range"),      HearingRange);
    Result->SetStringField(TEXT("dominant_sense"),     DominantSense);
    Result->SetBoolField(TEXT("already_had_perception"), bAlreadyHasPerception);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// create_ai_controller
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAIHandler::CreateAIController(const TSharedPtr<FJsonObject>& Params)
{
    FString Name              = Params->GetStringField(TEXT("name")).TrimStartAndEnd();
    FString SavePath          = Params->GetStringField(TEXT("save_path")).TrimStartAndEnd();
    FString BehaviorTreeName  = Params->GetStringField(TEXT("behavior_tree_name")).TrimStartAndEnd();
    bool    bEnablePerception = true;
    Params->TryGetBoolField(TEXT("enable_perception"), bEnablePerception);

    if (Name.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("AIController 이름이 비어있습니다."));
    }
    if (SavePath.IsEmpty()) SavePath = TEXT("/Game/AI");

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
    FString PackageName = UPackageTools::SanitizePackageName(PackagePath);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("패키지 생성 실패."));
    }

    // AIController 기반 Blueprint 생성
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = AAIController::StaticClass();

    UBlueprint* BPAsset = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name),
                                   RF_Public | RF_Standalone, nullptr, GWarn));

    if (!IsValid(BPAsset))
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("AIController Blueprint 생성 실패."));
    }

    // AIPerception 컴포넌트 추가
    if (bEnablePerception)
    {
        USimpleConstructionScript* SCS = BPAsset->SimpleConstructionScript;
        if (IsValid(SCS))
        {
            USCS_Node* PerceptionNode = SCS->CreateNode(UAIPerceptionComponent::StaticClass(),
                                                          FName(TEXT("AIPerception")));
            SCS->AddNode(PerceptionNode);
        }
    }

    FKismetEditorUtilities::CompileBlueprint(BPAsset);
    FAssetRegistryModule::AssetCreated(BPAsset);
    BPAsset->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"),              Name);
    Result->SetStringField(TEXT("package_path"),      SavePath);
    Result->SetStringField(TEXT("object_path"),       PackageName + TEXT(".") + Name);
    Result->SetBoolField(TEXT("perception_added"),    bEnablePerception);
    Result->SetStringField(TEXT("behavior_tree"),     BehaviorTreeName);
    if (!BehaviorTreeName.IsEmpty())
    {
        Result->SetStringField(TEXT("note"),
            TEXT("OnPossess에서 RunBehaviorTree를 호출하도록 EventGraph를 수동으로 구성하세요."));
    }
    return MakeSuccess(Result);
}
