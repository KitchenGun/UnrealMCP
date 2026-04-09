#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

class FSocket;

/**
 * MCP TCP 서버.
 * 포트 13377에서 Python MCP 서버의 연결을 수락하고,
 * newline-delimited JSON 명령을 수신하여 처리한 뒤 응답을 반환한다.
 *
 * 수신 루프는 전용 백그라운드 스레드에서 실행되며,
 * UE API 호출은 반드시 GameThread로 위임한다.
 */
class FMCPTcpServer : public FRunnable
{
public:
    FMCPTcpServer();
    virtual ~FMCPTcpServer();

    /** 서버 소켓 바인딩 및 스레드 시작 */
    bool Start();
    /** 서버 종료 및 소켓 정리 */
    void Stop();

    // FRunnable 인터페이스
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Exit() override;

private:
    /**
     * 클라이언트 소켓에서 '\n'까지 한 줄을 읽는다.
     * @return 성공 여부. false면 연결 끊김 또는 오류.
     */
    bool ReadLine(FSocket* InSocket, FString& OutLine);

    /**
     * 클라이언트 소켓에 UTF-8 문자열을 전송한다.
     * @return 성공 여부.
     */
    bool SendString(FSocket* InSocket, const FString& Message);

    /**
     * JSON 명령 문자열을 파싱하고 적합한 핸들러를 호출한 뒤
     * JSON 응답 문자열을 반환한다.
     * GameThread에서 호출된다.
     */
    FString ProcessCommand(const FString& JsonString);

    /** 에러 응답 JSON 문자열 생성 헬퍼 */
    FString MakeErrorResponse(const FString& Id, const FString& Code, const FString& Message);

private:
    /** 리스닝 소켓 */
    FSocket* ListenSocket = nullptr;
    /** 백그라운드 수신 스레드 */
    FRunnableThread* Thread = nullptr;
    /** 서버 중지 플래그 */
    TAtomic<bool> bStopping{false};

    /** MCP 리스닝 포트 */
    static constexpr int32 MCP_PORT = 13377;
    /** 수신 버퍼 크기 (64KB) */
    static constexpr int32 RECV_BUFFER_SIZE = 65536;
};
