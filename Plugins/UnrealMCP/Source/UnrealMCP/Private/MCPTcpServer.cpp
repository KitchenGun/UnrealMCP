#include "MCPTcpServer.h"
#include "Handlers/ActorHandler.h"
#include "Handlers/BlueprintHandler.h"
#include "Handlers/MaterialHandler.h"
#include "Handlers/AIHandler.h"
#include "Handlers/EditorHandler.h"
#include "Handlers/AdvancedHandler.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Common/TcpSocketBuilder.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Event.h"

// ─────────────────────────────────────────────────
// 생성자 / 소멸자
// ─────────────────────────────────────────────────

FMCPTcpServer::FMCPTcpServer()
{
}

FMCPTcpServer::~FMCPTcpServer()
{
    Stop();
}

// ─────────────────────────────────────────────────
// 서버 시작 / 중지
// ─────────────────────────────────────────────────

bool FMCPTcpServer::Start()
{
    // 리스닝 소켓 생성 (TCP, IPv4, 포트 13377)
    ListenSocket = FTcpSocketBuilder(TEXT("MCPListenSocket"))
        .AsReusable()
        .BoundToPort(MCP_PORT)
        .Listening(1)   // 최대 대기 연결 수: 1 (Python MCP 서버 1개 연결)
        .Build();

    if (!ListenSocket)
    {
        UE_LOG(LogTemp, Error, TEXT("[UnrealMCP] 리스닝 소켓 생성 실패 (포트 %d)."), MCP_PORT);
        return false;
    }

    // 소켓 수신 버퍼 크기 설정
    int32 NewSize = 0;
    ListenSocket->SetReceiveBufferSize(RECV_BUFFER_SIZE, NewSize);

    bStopping = false;

    // 백그라운드 스레드 시작
    Thread = FRunnableThread::Create(this, TEXT("MCPTcpServerThread"),
                                      0, TPri_Normal);
    if (!Thread)
    {
        UE_LOG(LogTemp, Error, TEXT("[UnrealMCP] 서버 스레드 생성 실패."));
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
        return false;
    }

    return true;
}

void FMCPTcpServer::Stop()
{
    bStopping = true;

    // 소켓을 닫아 Run()의 블로킹 호출(Accept/Recv)에서 깨어나도록 함
    if (ListenSocket)
    {
        ListenSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
    }

    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }
}

// ─────────────────────────────────────────────────
// FRunnable 인터페이스
// ─────────────────────────────────────────────────

bool FMCPTcpServer::Init()
{
    return true;
}

uint32 FMCPTcpServer::Run()
{
    UE_LOG(LogTemp, Log, TEXT("[UnrealMCP] 수신 스레드 시작. 포트 %d 대기 중."), MCP_PORT);

    while (!bStopping)
    {
        // 클라이언트 연결 대기 (100ms 폴링)
        bool bHasPending = false;
        if (!ListenSocket || !ListenSocket->HasPendingConnection(bHasPending))
        {
            FPlatformProcess::Sleep(0.1f);
            continue;
        }

        if (!bHasPending)
        {
            FPlatformProcess::Sleep(0.05f);
            continue;
        }

        // 클라이언트 소켓 수락
        FSocket* ClientSocket = ListenSocket->Accept(TEXT("MCPClientSocket"));
        if (!ClientSocket)
        {
            UE_LOG(LogTemp, Warning, TEXT("[UnrealMCP] 클라이언트 소켓 수락 실패."));
            continue;
        }

        UE_LOG(LogTemp, Log, TEXT("[UnrealMCP] Python MCP 서버 연결됨."));

        // 연결된 클라이언트와 명령/응답 루프
        while (!bStopping)
        {
            FString JsonLine;
            if (!ReadLine(ClientSocket, JsonLine))
            {
                // 연결 끊김 또는 오류
                UE_LOG(LogTemp, Log, TEXT("[UnrealMCP] 클라이언트 연결 종료."));
                break;
            }

            if (JsonLine.IsEmpty())
            {
                continue;
            }

            // GameThread에서 명령 처리 (UE API 호출은 GameThread에서만 허용)
            // 백그라운드 스레드는 이벤트로 완료 대기
            FString ResponseJson;
            FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool(false);

            AsyncTask(ENamedThreads::GameThread, [this, JsonLine, &ResponseJson, DoneEvent]()
            {
                ResponseJson = ProcessCommand(JsonLine);
                DoneEvent->Trigger();
            });

            // GameThread 처리 완료 대기 (최대 30초)
            const bool bDone = DoneEvent->Wait(FTimespan::FromSeconds(30.0));
            FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

            if (!bDone)
            {
                UE_LOG(LogTemp, Error, TEXT("[UnrealMCP] 명령 처리 타임아웃 (30초 초과)."));
                // id 없이 타임아웃 오류 전송
                FString Timeout = TEXT("{\"id\":\"\",\"success\":false,\"result\":null,\"error\":{\"code\":\"CONNECTION_TIMEOUT\",\"message\":\"명령 처리가 30초를 초과했습니다.\"}}\n");
                SendString(ClientSocket, Timeout);
                continue;
            }

            // 응답 전송
            if (!SendString(ClientSocket, ResponseJson + TEXT("\n")))
            {
                UE_LOG(LogTemp, Warning, TEXT("[UnrealMCP] 응답 전송 실패. 연결 종료."));
                break;
            }
        }

        // 클라이언트 소켓 정리
        ClientSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
    }

    UE_LOG(LogTemp, Log, TEXT("[UnrealMCP] 수신 스레드 종료."));
    return 0;
}

void FMCPTcpServer::Exit()
{
}

// ─────────────────────────────────────────────────
// 소켓 I/O 헬퍼
// ─────────────────────────────────────────────────

bool FMCPTcpServer::ReadLine(FSocket* InSocket, FString& OutLine)
{
    OutLine.Empty();
    uint8 Byte = 0;
    int32 BytesRead = 0;

    while (true)
    {
        if (bStopping)
        {
            return false;
        }

        // 1바이트 수신 시도 (논블로킹 폴링)
        if (!InSocket->Recv(&Byte, 1, BytesRead))
        {
            return false;
        }

        if (BytesRead == 0)
        {
            // 데이터 없음 -- 잠시 대기
            FPlatformProcess::Sleep(0.005f);
            continue;
        }

        if (Byte == '\n')
        {
            // 줄 끝 -- 파싱 완료
            return true;
        }

        OutLine.AppendChar(static_cast<TCHAR>(Byte));
    }
}

bool FMCPTcpServer::SendString(FSocket* InSocket, const FString& Message)
{
    FTCHARToUTF8 Converter(*Message);
    const uint8* Data = reinterpret_cast<const uint8*>(Converter.Get());
    int32 Size = Converter.Length();
    int32 Sent = 0;

    while (Sent < Size)
    {
        int32 BytesSent = 0;
        if (!InSocket->Send(Data + Sent, Size - Sent, BytesSent))
        {
            return false;
        }
        Sent += BytesSent;
    }
    return true;
}

// ─────────────────────────────────────────────────
// 명령 처리 (GameThread)
// ─────────────────────────────────────────────────

FString FMCPTcpServer::ProcessCommand(const FString& JsonString)
{
    // JSON 파싱
    TSharedPtr<FJsonObject> Command;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Command) || !Command.IsValid())
    {
        return MakeErrorResponse(TEXT(""), TEXT("INVALID_PARAMS"), TEXT("JSON 파싱 실패."));
    }

    // id 추출 (응답에 그대로 포함)
    FString Id = Command->GetStringField(TEXT("id"));
    FString Type = Command->GetStringField(TEXT("type"));

    // params 객체 추출
    TSharedPtr<FJsonObject> Params;
    if (Command->HasField(TEXT("params")))
    {
        Params = Command->GetObjectField(TEXT("params"));
    }
    else
    {
        Params = MakeShared<FJsonObject>();
    }

    // 타입에 따라 핸들러 디스패치
    TSharedPtr<FJsonObject> Result;

    if (Type == TEXT("create_actor"))
    {
        Result = FActorHandler::HandleCreateActor(Params);
    }
    else if (Type == TEXT("delete_actor"))
    {
        Result = FActorHandler::HandleDeleteActor(Params);
    }
    else if (Type == TEXT("set_actor_transform"))
    {
        Result = FActorHandler::HandleSetActorTransform(Params);
    }
    else if (Type == TEXT("get_actors_in_level"))
    {
        Result = FActorHandler::HandleGetActorsInLevel(Params);
    }
    else if (Type == TEXT("find_actors_by_name"))
    {
        Result = FActorHandler::HandleFindActorsByName(Params);
    }
    else if (Type == TEXT("duplicate_actor"))
    {
        Result = FActorHandler::HandleDuplicateActor(Params);
    }
    else if (Type == TEXT("get_actor_properties"))
    {
        Result = FActorHandler::HandleGetActorProperties(Params);
    }
    else if (Type == TEXT("set_actor_property"))
    {
        Result = FActorHandler::HandleSetActorProperty(Params);
    }
    // ── Phase 2: Blueprint 편집 커맨드 ───────────────────────────────
    else if (Type == TEXT("create_blueprint")       ||
             Type == TEXT("add_blueprint_node")     ||
             Type == TEXT("connect_blueprint_pins") ||
             Type == TEXT("remove_blueprint_node")  ||
             Type == TEXT("add_blueprint_variable") ||
             Type == TEXT("compile_blueprint")      ||
             Type == TEXT("get_blueprint_graph")    ||
             Type == TEXT("add_blueprint_component")||
             Type == TEXT("spawn_blueprint_actor"))
    {
        Result = FBlueprintHandler::HandleCommand(Type, Params);
    }
    // ── Phase 3: Material & Asset 커맨드 ─────────────────────────────
    else if (Type == TEXT("search_assets")           ||
             Type == TEXT("get_asset_details")       ||
             Type == TEXT("create_material")         ||
             Type == TEXT("add_material_expression") ||
             Type == TEXT("connect_material_nodes")  ||
             Type == TEXT("apply_material_to_actor") ||
             Type == TEXT("set_material_parameter")  ||
             Type == TEXT("import_asset")            ||
             Type == TEXT("duplicate_asset")         ||
             Type == TEXT("delete_asset"))
    {
        Result = FMaterialHandler::HandleCommand(Type, Params);
    }
    // ── Phase 4: AI System 커맨드 ────────────────────────────────────
    else if (Type == TEXT("create_behavior_tree")  ||
             Type == TEXT("add_bt_node")           ||
             Type == TEXT("create_blackboard")     ||
             Type == TEXT("add_blackboard_key")    ||
             Type == TEXT("create_eqs_query")      ||
             Type == TEXT("setup_ai_perception")   ||
             Type == TEXT("create_ai_controller"))
    {
        Result = FAIHandler::HandleCommand(Type, Params);
    }
    // ── Phase 5: Editor Automation 커맨드 ────────────────────────────
    else if (Type == TEXT("play_in_editor")      ||
             Type == TEXT("set_viewport_camera") ||
             Type == TEXT("run_console_command") ||
             Type == TEXT("take_screenshot")     ||
             Type == TEXT("get_selected_actors") ||
             Type == TEXT("select_actors")       ||
             Type == TEXT("save_level")          ||
             Type == TEXT("load_level"))
    {
        Result = FEditorHandler::HandleCommand(Type, Params);
    }
    // ── Phase 6: Advanced Systems 커맨드 ─────────────────────────────
    else if (Type == TEXT("create_niagara_system")      ||
             Type == TEXT("create_animation_blueprint") ||
             Type == TEXT("create_widget_blueprint")    ||
             Type == TEXT("create_data_table")          ||
             Type == TEXT("create_data_asset")          ||
             Type == TEXT("inspect_uobject"))
    {
        Result = FAdvancedHandler::HandleCommand(Type, Params);
    }
    else
    {
        Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), false);
        TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
        ErrorObj->SetStringField(TEXT("code"), TEXT("INVALID_PARAMS"));
        ErrorObj->SetStringField(TEXT("message"), FString::Printf(TEXT("알 수 없는 명령 타입: '%s'"), *Type));
        Result->SetObjectField(TEXT("error"), ErrorObj);
        Result->SetField(TEXT("result"), MakeShared<FJsonValueNull>());
    }

    // id 필드 주입
    if (Result.IsValid())
    {
        Result->SetStringField(TEXT("id"), Id);
    }

    // JSON 직렬화
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
    return OutputString;
}

FString FMCPTcpServer::MakeErrorResponse(const FString& Id, const FString& Code, const FString& Message)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField(TEXT("id"), Id);
    Response->SetBoolField(TEXT("success"), false);
    Response->SetField(TEXT("result"), MakeShared<FJsonValueNull>());

    TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
    ErrorObj->SetStringField(TEXT("code"), Code);
    ErrorObj->SetStringField(TEXT("message"), Message);
    Response->SetObjectField(TEXT("error"), ErrorObj);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    return OutputString;
}
