#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMCPTcpServer;

/**
 * UnrealMCP 플러그인 모듈.
 * 에디터 시작 시 TCP 서버를 구동하고, 종료 시 정리한다.
 */
class FUnrealMCPModule : public IModuleInterface
{
public:
    // IModuleInterface 구현
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    /** TCP 서버 인스턴스 */
    TUniquePtr<FMCPTcpServer> TcpServer;
};
