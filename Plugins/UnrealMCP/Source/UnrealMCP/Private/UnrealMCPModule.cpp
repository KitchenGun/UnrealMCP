#include "UnrealMCPModule.h"
#include "MCPTcpServer.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FUnrealMCPModule"

void FUnrealMCPModule::StartupModule()
{
    // TCP 서버 인스턴스 생성 및 시작
    TcpServer = MakeUnique<FMCPTcpServer>();
    if (!TcpServer->Start())
    {
        UE_LOG(LogTemp, Error, TEXT("[UnrealMCP] TCP 서버 시작 실패. 포트 13377을 사용 중인 프로세스가 있는지 확인하세요."));
        TcpServer.Reset();
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[UnrealMCP] 플러그인 시작됨. TCP 포트 13377 수신 대기 중."));
    }
}

void FUnrealMCPModule::ShutdownModule()
{
    if (TcpServer.IsValid())
    {
        TcpServer->Stop();
        TcpServer.Reset();
        UE_LOG(LogTemp, Log, TEXT("[UnrealMCP] 플러그인 종료됨. TCP 서버 중지."));
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealMCPModule, UnrealMCP)
