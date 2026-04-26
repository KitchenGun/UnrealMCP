#include "Handlers/BlueprintHandler.h"

// UE5 에디터 헤더
#include "Editor.h"
#include "Engine/World.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_SpawnActorFromClass.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

// JSON
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

// ─────────────────────────────────────────────────────────────────────────────
// 공개 진입점
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FBlueprintHandler::HandleCommand(
    const FString& CommandType,
    const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_blueprint"))         return CreateBlueprint(Params);
    if (CommandType == TEXT("add_blueprint_node"))       return AddBlueprintNode(Params);
    if (CommandType == TEXT("connect_blueprint_pins"))   return ConnectBlueprintPins(Params);
    if (CommandType == TEXT("remove_blueprint_node"))    return RemoveBlueprintNode(Params);
    if (CommandType == TEXT("add_blueprint_variable"))   return AddBlueprintVariable(Params);
    if (CommandType == TEXT("compile_blueprint"))        return CompileBlueprint(Params);
    if (CommandType == TEXT("get_blueprint_graph"))      return GetBlueprintGraph(Params);
    if (CommandType == TEXT("add_blueprint_component"))  return AddBlueprintComponent(Params);
    if (CommandType == TEXT("spawn_blueprint_actor"))    return SpawnBlueprintActor(Params);

    return MakeError(TEXT("UNKNOWN_COMMAND"),
        FString::Printf(TEXT("알 수 없는 Blueprint 커맨드: %s"), *CommandType));
}

// ─────────────────────────────────────────────────────────────────────────────
// 내부 헬퍼
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FBlueprintHandler::MakeSuccess(TSharedPtr<FJsonObject> Data)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    if (Data.IsValid())
    {
        Result->SetObjectField(TEXT("data"), Data);
    }
    return Result;
}

TSharedPtr<FJsonObject> FBlueprintHandler::MakeError(const FString& Code, const FString& Message)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), false);
    auto ErrorObj = MakeShared<FJsonObject>();
    ErrorObj->SetStringField(TEXT("code"), Code);
    ErrorObj->SetStringField(TEXT("message"), Message);
    Result->SetObjectField(TEXT("error"), ErrorObj);
    return Result;
}

UBlueprint* FBlueprintHandler::FindBlueprintByName(const FString& Name)
{
    FAssetRegistryModule& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    TArray<FAssetData> AssetList;
    AssetRegistry.Get().GetAssetsByClass(
        UBlueprint::StaticClass()->GetClassPathName(), AssetList);

    for (const FAssetData& Asset : AssetList)
    {
        if (Asset.AssetName.ToString().Equals(Name, ESearchCase::IgnoreCase))
        {
            return Cast<UBlueprint>(Asset.GetAsset());
        }
    }
    return nullptr;
}

UClass* FBlueprintHandler::FindParentClass(const FString& ClassName)
{
    static const TMap<FString, UClass*> ClassMap = {
        { TEXT("Actor"),            AActor::StaticClass()     },
        { TEXT("Character"),        ACharacter::StaticClass() },
        { TEXT("Pawn"),             APawn::StaticClass()      },
        { TEXT("ActorComponent"),   UActorComponent::StaticClass() },
        { TEXT("SceneComponent"),   USceneComponent::StaticClass() },
    };

    if (const UClass* const* Found = ClassMap.Find(ClassName))
    {
        return const_cast<UClass*>(*Found);
    }

    // Reflection으로 폴백
    UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName);
    return Class;
}

UK2Node* FBlueprintHandler::FindNodeById(UEdGraph* Graph, const FString& NodeId)
{
    if (!Graph) return nullptr;

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node && Node->NodeGuid.ToString() == NodeId)
        {
            return Cast<UK2Node>(Node);
        }
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// create_blueprint
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FBlueprintHandler::CreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName, ParentClassName, SavePath;
    Params->TryGetStringField(TEXT("name"), BPName);
    Params->TryGetStringField(TEXT("parent_class"), ParentClassName);
    Params->TryGetStringField(TEXT("save_path"), SavePath);

    if (BPName.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("Blueprint 이름이 비어있습니다."));
    }

    UClass* ParentClass = FindParentClass(ParentClassName.IsEmpty() ? TEXT("Actor") : ParentClassName);
    if (!ParentClass)
    {
        return MakeError(TEXT("CLASS_NOT_FOUND"),
            FString::Printf(TEXT("부모 클래스를 찾을 수 없습니다: %s"), *ParentClassName));
    }

    // 저장 경로 패키지 이름 구성
    FString PackageName = SavePath / BPName;
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return MakeError(TEXT("PACKAGE_ERROR"), TEXT("패키지를 생성할 수 없습니다."));
    }

    // Blueprint 생성
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ParentClass;

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    UObject* NewAsset = AssetTools.CreateAsset(BPName, SavePath, UBlueprint::StaticClass(), Factory);

    UBlueprint* NewBP = Cast<UBlueprint>(NewAsset);
    if (!NewBP)
    {
        return MakeError(TEXT("CREATE_FAILED"), TEXT("Blueprint 생성에 실패했습니다."));
    }

    // 에셋 저장
    NewBP->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewBP);

    auto Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("name"), BPName);
    Data->SetStringField(TEXT("path"), PackageName);
    Data->SetStringField(TEXT("parent_class"), ParentClass->GetName());
    return MakeSuccess(Data);
}

// ─────────────────────────────────────────────────────────────────────────────
// add_blueprint_node
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FBlueprintHandler::AddBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName, NodeType, GraphName;
    double PosX = 0.0, PosY = 0.0;
    Params->TryGetStringField(TEXT("blueprint_name"), BPName);
    Params->TryGetStringField(TEXT("node_type"), NodeType);
    Params->TryGetStringField(TEXT("graph_name"), GraphName);
    Params->TryGetNumberField(TEXT("position_x"), PosX);
    Params->TryGetNumberField(TEXT("position_y"), PosY);

    TSharedPtr<FJsonObject> NodeParams;
    if (Params->HasField(TEXT("node_params")))
    {
        NodeParams = Params->GetObjectField(TEXT("node_params"));
    }

    UBlueprint* BP = FindBlueprintByName(BPName);
    if (!BP)
    {
        return MakeError(TEXT("BP_NOT_FOUND"),
            FString::Printf(TEXT("Blueprint를 찾을 수 없습니다: %s"), *BPName));
    }

    // 대상 그래프 검색
    UEdGraph* TargetGraph = nullptr;
    for (UEdGraph* Graph : BP->UbergraphPages)
    {
        if (Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
        {
            TargetGraph = Graph;
            break;
        }
    }
    if (!TargetGraph && !BP->UbergraphPages.IsEmpty())
    {
        TargetGraph = BP->UbergraphPages[0];
    }
    if (!TargetGraph)
    {
        return MakeError(TEXT("GRAPH_NOT_FOUND"),
            FString::Printf(TEXT("그래프를 찾을 수 없습니다: %s"), *GraphName));
    }

    UK2Node* NewNode = nullptr;
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

    // ── 이벤트 노드 ─────────────────────────────────────────────────
    if (NodeType == TEXT("Event_BeginPlay") || NodeType == TEXT("Event_Tick") ||
        NodeType == TEXT("Event_EndPlay")   || NodeType.StartsWith(TEXT("Event_")))
    {
        UK2Node_Event* EventNode = NewObject<UK2Node_Event>(TargetGraph);

        FString EventName = NodeType;
        EventName.RemoveFromStart(TEXT("Event_"));

        if (EventName == TEXT("BeginPlay"))
        {
            EventNode->EventReference.SetExternalDelegateMember(
                FName(TEXT("ReceiveBeginPlay")));
            EventNode->bOverrideFunction = true;
        }
        else if (EventName == TEXT("Tick"))
        {
            EventNode->EventReference.SetExternalDelegateMember(
                FName(TEXT("ReceiveTick")));
            EventNode->bOverrideFunction = true;
        }
        else if (EventName == TEXT("EndPlay"))
        {
            EventNode->EventReference.SetExternalDelegateMember(
                FName(TEXT("ReceiveEndPlay")));
            EventNode->bOverrideFunction = true;
        }

        NewNode = EventNode;
    }
    // ── PrintString 노드 ─────────────────────────────────────────────
    else if (NodeType == TEXT("PrintString"))
    {
        UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(TargetGraph);
        UFunction* PrintFunc = UKismetSystemLibrary::StaticClass()
            ->FindFunctionByName(TEXT("PrintString"));
        if (PrintFunc)
        {
            CallNode->SetFromFunction(PrintFunc);
        }
        NewNode = CallNode;
    }
    // ── Branch (if/else) 노드 ────────────────────────────────────────
    else if (NodeType == TEXT("Branch"))
    {
        UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(TargetGraph);
        NewNode = BranchNode;
    }
    // ── Sequence 노드 ────────────────────────────────────────────────
    else if (NodeType == TEXT("Sequence"))
    {
        UK2Node_ExecutionSequence* SeqNode = NewObject<UK2Node_ExecutionSequence>(TargetGraph);
        NewNode = SeqNode;
    }
    // ── SpawnActor 노드 ──────────────────────────────────────────────
    else if (NodeType == TEXT("SpawnActor"))
    {
        UK2Node_SpawnActorFromClass* SpawnNode = NewObject<UK2Node_SpawnActorFromClass>(TargetGraph);
        NewNode = SpawnNode;
    }
    // ── 일반 함수 호출 노드 ──────────────────────────────────────────
    else if (NodeType == TEXT("CallFunction"))
    {
        FString FuncName;
        if (NodeParams.IsValid())
        {
            NodeParams->TryGetStringField(TEXT("function_name"), FuncName);
        }
        UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(TargetGraph);
        if (!FuncName.IsEmpty())
        {
            UFunction* Func = nullptr;
            // BP 자체 함수 먼저 검색
            if (BP->GeneratedClass)
            {
                Func = BP->GeneratedClass->FindFunctionByName(*FuncName);
            }
            if (Func)
            {
                CallNode->SetFromFunction(Func);
            }
        }
        NewNode = CallNode;
    }
    // ── 변수 Get/Set 노드 ────────────────────────────────────────────
    else if (NodeType == TEXT("VariableGet") || NodeType == TEXT("VariableSet"))
    {
        FString VarName;
        if (NodeParams.IsValid())
        {
            NodeParams->TryGetStringField(TEXT("variable_name"), VarName);
        }

        if (NodeType == TEXT("VariableGet"))
        {
            UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(TargetGraph);
            if (!VarName.IsEmpty() && BP->GeneratedClass)
            {
                FProperty* Prop = BP->GeneratedClass->FindPropertyByName(*VarName);
                if (Prop)
                {
                    GetNode->VariableReference.SetFromField<FProperty>(Prop, false);
                }
            }
            NewNode = GetNode;
        }
        else
        {
            UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(TargetGraph);
            if (!VarName.IsEmpty() && BP->GeneratedClass)
            {
                FProperty* Prop = BP->GeneratedClass->FindPropertyByName(*VarName);
                if (Prop)
                {
                    SetNode->VariableReference.SetFromField<FProperty>(Prop, false);
                }
            }
            NewNode = SetNode;
        }
    }
    else
    {
        return MakeError(TEXT("UNKNOWN_NODE_TYPE"),
            FString::Printf(TEXT("지원하지 않는 노드 타입: %s"), *NodeType));
    }

    // 그래프에 노드 추가 및 위치 설정
    TargetGraph->AddNode(NewNode, false, false);
    NewNode->NodePosX = (int32)PosX;
    NewNode->NodePosY = (int32)PosY;
    NewNode->CreateNewGuid();
    NewNode->PostPlacedNewNode();
    NewNode->AllocateDefaultPins();

    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

    auto Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString());
    Data->SetStringField(TEXT("node_type"), NodeType);
    Data->SetNumberField(TEXT("position_x"), PosX);
    Data->SetNumberField(TEXT("position_y"), PosY);
    return MakeSuccess(Data);
}

// ─────────────────────────────────────────────────────────────────────────────
// connect_blueprint_pins
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FBlueprintHandler::ConnectBlueprintPins(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName, SrcNodeId, SrcPinName, TgtNodeId, TgtPinName, GraphName;
    Params->TryGetStringField(TEXT("blueprint_name"), BPName);
    Params->TryGetStringField(TEXT("source_node_id"), SrcNodeId);
    Params->TryGetStringField(TEXT("source_pin_name"), SrcPinName);
    Params->TryGetStringField(TEXT("target_node_id"), TgtNodeId);
    Params->TryGetStringField(TEXT("target_pin_name"), TgtPinName);
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    UBlueprint* BP = FindBlueprintByName(BPName);
    if (!BP)
    {
        return MakeError(TEXT("BP_NOT_FOUND"),
            FString::Printf(TEXT("Blueprint를 찾을 수 없습니다: %s"), *BPName));
    }

    // 그래프 검색
    UEdGraph* TargetGraph = nullptr;
    for (UEdGraph* G : BP->UbergraphPages)
    {
        if (G->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
        {
            TargetGraph = G;
            break;
        }
    }
    if (!TargetGraph && !BP->UbergraphPages.IsEmpty())
    {
        TargetGraph = BP->UbergraphPages[0];
    }
    if (!TargetGraph)
    {
        return MakeError(TEXT("GRAPH_NOT_FOUND"),
            FString::Printf(TEXT("그래프를 찾을 수 없습니다: %s"), *GraphName));
    }

    UK2Node* SrcNode = FindNodeById(TargetGraph, SrcNodeId);
    UK2Node* TgtNode = FindNodeById(TargetGraph, TgtNodeId);

    if (!SrcNode)
    {
        return MakeError(TEXT("NODE_NOT_FOUND"),
            FString::Printf(TEXT("소스 노드를 찾을 수 없습니다: %s"), *SrcNodeId));
    }
    if (!TgtNode)
    {
        return MakeError(TEXT("NODE_NOT_FOUND"),
            FString::Printf(TEXT("타겟 노드를 찾을 수 없습니다: %s"), *TgtNodeId));
    }

    UEdGraphPin* SrcPin = SrcNode->FindPin(*SrcPinName, EGPD_Output);
    UEdGraphPin* TgtPin = TgtNode->FindPin(*TgtPinName, EGPD_Input);

    if (!SrcPin)
    {
        return MakeError(TEXT("PIN_NOT_FOUND"),
            FString::Printf(TEXT("소스 핀을 찾을 수 없습니다: %s"), *SrcPinName));
    }
    if (!TgtPin)
    {
        return MakeError(TEXT("PIN_NOT_FOUND"),
            FString::Printf(TEXT("타겟 핀을 찾을 수 없습니다: %s"), *TgtPinName));
    }

    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    FPinConnectionResponse Response = Schema->CanCreateConnection(SrcPin, TgtPin);

    if (Response.Response == CONNECT_RESPONSE_DISALLOW)
    {
        return MakeError(TEXT("CONNECTION_DENIED"), Response.Message.ToString());
    }

    SrcPin->MakeLinkTo(TgtPin);
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

    auto Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("source_node_id"), SrcNodeId);
    Data->SetStringField(TEXT("source_pin"), SrcPinName);
    Data->SetStringField(TEXT("target_node_id"), TgtNodeId);
    Data->SetStringField(TEXT("target_pin"), TgtPinName);
    return MakeSuccess(Data);
}

// ─────────────────────────────────────────────────────────────────────────────
// remove_blueprint_node
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FBlueprintHandler::RemoveBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName, NodeId, GraphName;
    Params->TryGetStringField(TEXT("blueprint_name"), BPName);
    Params->TryGetStringField(TEXT("node_id"), NodeId);
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    UBlueprint* BP = FindBlueprintByName(BPName);
    if (!BP)
    {
        return MakeError(TEXT("BP_NOT_FOUND"),
            FString::Printf(TEXT("Blueprint를 찾을 수 없습니다: %s"), *BPName));
    }

    UEdGraph* TargetGraph = nullptr;
    for (UEdGraph* G : BP->UbergraphPages)
    {
        if (G->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
        {
            TargetGraph = G;
            break;
        }
    }
    if (!TargetGraph && !BP->UbergraphPages.IsEmpty())
    {
        TargetGraph = BP->UbergraphPages[0];
    }
    if (!TargetGraph)
    {
        return MakeError(TEXT("GRAPH_NOT_FOUND"),
            FString::Printf(TEXT("그래프를 찾을 수 없습니다: %s"), *GraphName));
    }

    UK2Node* Node = FindNodeById(TargetGraph, NodeId);
    if (!Node)
    {
        return MakeError(TEXT("NODE_NOT_FOUND"),
            FString::Printf(TEXT("노드를 찾을 수 없습니다: %s"), *NodeId));
    }

    FBlueprintEditorUtils::RemoveNode(BP, Node, true);

    auto Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("node_id"), NodeId);
    Data->SetBoolField(TEXT("removed"), true);
    return MakeSuccess(Data);
}

// ─────────────────────────────────────────────────────────────────────────────
// add_blueprint_variable
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FBlueprintHandler::AddBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName, VarName, VarType;
    bool bExposed = false;
    Params->TryGetStringField(TEXT("blueprint_name"), BPName);
    Params->TryGetStringField(TEXT("variable_name"), VarName);
    Params->TryGetStringField(TEXT("variable_type"), VarType);
    Params->TryGetBoolField(TEXT("is_exposed"), bExposed);

    if (BPName.IsEmpty() || VarName.IsEmpty() || VarType.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("blueprint_name, variable_name, variable_type는 필수입니다."));
    }

    UBlueprint* BP = FindBlueprintByName(BPName);
    if (!BP)
    {
        return MakeError(TEXT("BP_NOT_FOUND"),
            FString::Printf(TEXT("Blueprint를 찾을 수 없습니다: %s"), *BPName));
    }

    // 타입 핀 카테고리 매핑
    static const TMap<FString, FName> TypeCategoryMap = {
        { TEXT("Boolean"),   UEdGraphSchema_K2::PC_Boolean   },
        { TEXT("Integer"),   UEdGraphSchema_K2::PC_Int        },
        { TEXT("Float"),     UEdGraphSchema_K2::PC_Real       },
        { TEXT("String"),    UEdGraphSchema_K2::PC_String     },
        { TEXT("Name"),      UEdGraphSchema_K2::PC_Name       },
        { TEXT("Text"),      UEdGraphSchema_K2::PC_Text       },
        { TEXT("Vector"),    UEdGraphSchema_K2::PC_Struct     },
        { TEXT("Rotator"),   UEdGraphSchema_K2::PC_Struct     },
        { TEXT("Transform"), UEdGraphSchema_K2::PC_Struct     },
        { TEXT("Object"),    UEdGraphSchema_K2::PC_Object     },
    };

    FEdGraphPinType PinType;
    if (const FName* Category = TypeCategoryMap.Find(VarType))
    {
        PinType.PinCategory = *Category;

        // 구조체 타입은 SubCategoryObject 설정 필요
        if (*Category == UEdGraphSchema_K2::PC_Struct)
        {
            if (VarType == TEXT("Vector"))
            {
                PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
            }
            else if (VarType == TEXT("Rotator"))
            {
                PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
            }
            else if (VarType == TEXT("Transform"))
            {
                PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
            }
        }
    }
    else
    {
        // 클래스 타입으로 폴백
        UClass* VarClass = FindObject<UClass>(ANY_PACKAGE, *VarType);
        if (VarClass)
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
            PinType.PinSubCategoryObject = VarClass;
        }
        else
        {
            return MakeError(TEXT("UNKNOWN_TYPE"),
                FString::Printf(TEXT("지원하지 않는 변수 타입: %s"), *VarType));
        }
    }

    FBlueprintEditorUtils::AddMemberVariable(BP, FName(*VarName), PinType);

    // Exposed 설정
    if (bExposed)
    {
        int32 VarIdx = FBlueprintEditorUtils::FindNewVariableIndex(BP, FName(*VarName));
        if (VarIdx != INDEX_NONE)
        {
            BP->NewVariables[VarIdx].PropertyFlags |= CPF_Edit | CPF_BlueprintVisible;
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

    auto Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("variable_name"), VarName);
    Data->SetStringField(TEXT("variable_type"), VarType);
    Data->SetBoolField(TEXT("is_exposed"), bExposed);
    return MakeSuccess(Data);
}

// ─────────────────────────────────────────────────────────────────────────────
// compile_blueprint
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FBlueprintHandler::CompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName;
    Params->TryGetStringField(TEXT("blueprint_name"), BPName);

    UBlueprint* BP = FindBlueprintByName(BPName);
    if (!BP)
    {
        return MakeError(TEXT("BP_NOT_FOUND"),
            FString::Printf(TEXT("Blueprint를 찾을 수 없습니다: %s"), *BPName));
    }

    FKismetEditorUtilities::CompileBlueprint(BP,
        EBlueprintCompileOptions::SkipGarbageCollection);

    bool bHasErrors = BP->Status == BS_Error;

    auto Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("blueprint_name"), BPName);
    Data->SetBoolField(TEXT("compiled"), !bHasErrors);
    Data->SetStringField(TEXT("status"), bHasErrors ? TEXT("error") : TEXT("success"));
    return MakeSuccess(Data);
}

// ─────────────────────────────────────────────────────────────────────────────
// get_blueprint_graph
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FBlueprintHandler::GetBlueprintGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName, GraphName;
    Params->TryGetStringField(TEXT("blueprint_name"), BPName);
    Params->TryGetStringField(TEXT("graph_name"), GraphName);

    // Phase A C++ filters — defaults preserve original behavior for any
    // direct C++ caller; Python tool always forwards explicit values.
    bool bIncludePositions = true;
    bool bIncludePinLinks = true;
    bool bDropReroute = false;
    int32 NodeLimit = 0;
    FString NodeClassFilter;
    Params->TryGetBoolField(TEXT("include_positions"), bIncludePositions);
    Params->TryGetBoolField(TEXT("include_pin_links"), bIncludePinLinks);
    Params->TryGetBoolField(TEXT("drop_reroute"), bDropReroute);
    {
        double LimitD = 0.0;
        if (Params->TryGetNumberField(TEXT("limit"), LimitD) && LimitD > 0.0)
        {
            NodeLimit = static_cast<int32>(LimitD);
        }
    }
    Params->TryGetStringField(TEXT("node_class_filter"), NodeClassFilter);

    TSet<FString> ClassWhitelist;
    if (!NodeClassFilter.IsEmpty())
    {
        TArray<FString> Tokens;
        NodeClassFilter.ParseIntoArray(Tokens, TEXT(","), true);
        for (const FString& T : Tokens)
        {
            const FString Trimmed = T.TrimStartAndEnd();
            if (!Trimmed.IsEmpty())
            {
                ClassWhitelist.Add(Trimmed);
            }
        }
    }

    UBlueprint* BP = FindBlueprintByName(BPName);
    if (!BP)
    {
        return MakeError(TEXT("BP_NOT_FOUND"),
            FString::Printf(TEXT("Blueprint를 찾을 수 없습니다: %s"), *BPName));
    }

    UEdGraph* TargetGraph = nullptr;
    for (UEdGraph* G : BP->UbergraphPages)
    {
        if (G->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
        {
            TargetGraph = G;
            break;
        }
    }
    if (!TargetGraph && !BP->UbergraphPages.IsEmpty())
    {
        TargetGraph = BP->UbergraphPages[0];
    }
    if (!TargetGraph)
    {
        return MakeError(TEXT("GRAPH_NOT_FOUND"),
            FString::Printf(TEXT("그래프를 찾을 수 없습니다: %s"), *GraphName));
    }

    const int32 OriginalCount = TargetGraph->Nodes.Num();
    int32 IncludedCount = 0;
    TArray<TSharedPtr<FJsonValue>> NodeArray;
    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        if (!Node) continue;

        const FString NodeClass = Node->GetClass()->GetName();

        if (bDropReroute && NodeClass == TEXT("K2Node_Knot")) continue;
        if (ClassWhitelist.Num() > 0 && !ClassWhitelist.Contains(NodeClass)) continue;
        if (NodeLimit > 0 && IncludedCount >= NodeLimit) break;

        auto NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
        NodeObj->SetStringField(TEXT("node_class"), NodeClass);
        NodeObj->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
        if (bIncludePositions)
        {
            NodeObj->SetNumberField(TEXT("position_x"), Node->NodePosX);
            NodeObj->SetNumberField(TEXT("position_y"), Node->NodePosY);
        }

        // 핀 목록
        TArray<TSharedPtr<FJsonValue>> PinArray;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin) continue;
            auto PinObj = MakeShared<FJsonObject>();
            PinObj->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
            PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
            PinObj->SetStringField(TEXT("pin_type"), Pin->PinType.PinCategory.ToString());

            if (bIncludePinLinks)
            {
                TArray<TSharedPtr<FJsonValue>> Links;
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (!LinkedPin || !LinkedPin->GetOwningNode()) continue;
                    auto LinkObj = MakeShared<FJsonObject>();
                    LinkObj->SetStringField(TEXT("node_id"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
                    LinkObj->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
                    Links.Add(MakeShared<FJsonValueObject>(LinkObj));
                }
                PinObj->SetArrayField(TEXT("links"), Links);
            }
            else
            {
                PinObj->SetArrayField(TEXT("links"), TArray<TSharedPtr<FJsonValue>>());
            }
            PinArray.Add(MakeShared<FJsonValueObject>(PinObj));
        }
        NodeObj->SetArrayField(TEXT("pins"), PinArray);
        NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
        IncludedCount++;
    }

    auto Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("blueprint_name"), BPName);
    Data->SetStringField(TEXT("graph_name"), TargetGraph->GetName());
    Data->SetArrayField(TEXT("nodes"), NodeArray);
    Data->SetNumberField(TEXT("node_count"), NodeArray.Num());
    Data->SetNumberField(TEXT("total_count"), OriginalCount);
    if (NodeLimit > 0 && IncludedCount >= NodeLimit && OriginalCount > NodeLimit)
    {
        Data->SetNumberField(TEXT("truncated_at"), NodeLimit);
    }
    return MakeSuccess(Data);
}

// ─────────────────────────────────────────────────────────────────────────────
// add_blueprint_component
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FBlueprintHandler::AddBlueprintComponent(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName, CompClass, CompName, AttachTo;
    Params->TryGetStringField(TEXT("blueprint_name"), BPName);
    Params->TryGetStringField(TEXT("component_class"), CompClass);
    Params->TryGetStringField(TEXT("component_name"), CompName);
    Params->TryGetStringField(TEXT("attach_to"), AttachTo);

    if (BPName.IsEmpty() || CompClass.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("blueprint_name, component_class는 필수입니다."));
    }

    UBlueprint* BP = FindBlueprintByName(BPName);
    if (!BP)
    {
        return MakeError(TEXT("BP_NOT_FOUND"),
            FString::Printf(TEXT("Blueprint를 찾을 수 없습니다: %s"), *BPName));
    }

    // 컴포넌트 클래스 조회
    static const TMap<FString, UClass*> CompClassMap = {
        { TEXT("StaticMeshComponent"),    UStaticMeshComponent::StaticClass()  },
        { TEXT("BoxComponent"),           UBoxComponent::StaticClass()         },
        { TEXT("CapsuleComponent"),       UCapsuleComponent::StaticClass()     },
        { TEXT("PointLightComponent"),    UPointLightComponent::StaticClass()  },
        { TEXT("AudioComponent"),         UAudioComponent::StaticClass()       },
        { TEXT("SceneComponent"),         USceneComponent::StaticClass()       },
    };

    UClass* ComponentClass = nullptr;
    if (const UClass* const* Found = CompClassMap.Find(CompClass))
    {
        ComponentClass = const_cast<UClass*>(*Found);
    }
    else
    {
        ComponentClass = FindObject<UClass>(ANY_PACKAGE, *CompClass);
    }

    if (!ComponentClass)
    {
        return MakeError(TEXT("CLASS_NOT_FOUND"),
            FString::Printf(TEXT("컴포넌트 클래스를 찾을 수 없습니다: %s"), *CompClass));
    }

    USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
    if (!SCS)
    {
        return MakeError(TEXT("SCS_NOT_FOUND"), TEXT("SimpleConstructionScript가 없습니다."));
    }

    FName NewCompName = CompName.IsEmpty()
        ? FName(*FString::Printf(TEXT("%s_0"), *CompClass))
        : FName(*CompName);

    USCS_Node* NewSCSNode = SCS->CreateNode(ComponentClass, NewCompName);
    if (!NewSCSNode)
    {
        return MakeError(TEXT("COMPONENT_CREATE_FAILED"), TEXT("컴포넌트 생성에 실패했습니다."));
    }

    // 부모 노드 결정
    if (!AttachTo.IsEmpty())
    {
        USCS_Node* ParentNode = nullptr;
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            if (Node->GetVariableName().ToString().Equals(AttachTo, ESearchCase::IgnoreCase))
            {
                ParentNode = Node;
                break;
            }
        }
        if (ParentNode)
        {
            ParentNode->AddChildNode(NewSCSNode);
        }
        else
        {
            SCS->AddNode(NewSCSNode);
        }
    }
    else
    {
        SCS->AddNode(NewSCSNode);
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

    auto Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("component_name"), NewSCSNode->GetVariableName().ToString());
    Data->SetStringField(TEXT("component_class"), CompClass);
    Data->SetStringField(TEXT("attach_to"), AttachTo);
    return MakeSuccess(Data);
}

// ─────────────────────────────────────────────────────────────────────────────
// spawn_blueprint_actor
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FBlueprintHandler::SpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName, ActorName;
    Params->TryGetStringField(TEXT("blueprint_name"), BPName);
    Params->TryGetStringField(TEXT("actor_name"), ActorName);

    // Transform 파싱
    auto ParseVec = [&](const FString& FieldName, FVector& Out) {
        if (const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
            Params->TryGetArrayField(FieldName, Arr) && Arr && Arr->Num() >= 3)
        {
            Out.X = (float)(*Arr)[0]->AsNumber();
            Out.Y = (float)(*Arr)[1]->AsNumber();
            Out.Z = (float)(*Arr)[2]->AsNumber();
        }
    };
    FVector Location(0, 0, 0), ScaleVec(1, 1, 1);
    FVector RotVec(0, 0, 0);
    ParseVec(TEXT("location"), Location);
    ParseVec(TEXT("rotation"), RotVec);
    ParseVec(TEXT("scale"), ScaleVec);

    UBlueprint* BP = FindBlueprintByName(BPName);
    if (!BP || !BP->GeneratedClass)
    {
        return MakeError(TEXT("BP_NOT_FOUND"),
            FString::Printf(TEXT("Blueprint(또는 GeneratedClass)를 찾을 수 없습니다: %s"), *BPName));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return MakeError(TEXT("NO_WORLD"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    FTransform SpawnTransform(
        FRotator(RotVec.X, RotVec.Y, RotVec.Z),
        Location,
        ScaleVec);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AActor* Spawned = World->SpawnActor<AActor>(BP->GeneratedClass, SpawnTransform, SpawnParams);
    if (!Spawned)
    {
        return MakeError(TEXT("SPAWN_FAILED"), TEXT("액터 스폰에 실패했습니다."));
    }

    if (!ActorName.IsEmpty())
    {
        Spawned->SetActorLabel(*ActorName);
    }

    auto Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("actor_name"), Spawned->GetActorLabel());
    Data->SetStringField(TEXT("blueprint_name"), BPName);
    auto LocObj = MakeShared<FJsonObject>();
    LocObj->SetNumberField(TEXT("X"), Spawned->GetActorLocation().X);
    LocObj->SetNumberField(TEXT("Y"), Spawned->GetActorLocation().Y);
    LocObj->SetNumberField(TEXT("Z"), Spawned->GetActorLocation().Z);
    Data->SetObjectField(TEXT("location"), LocObj);
    return MakeSuccess(Data);
}
