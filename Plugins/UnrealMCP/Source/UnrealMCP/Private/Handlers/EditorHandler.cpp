#include "Handlers/EditorHandler.h"

// UE5 에디터 헤더
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "LevelEditor.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

// PIE
#include "LevelEditorSubsystem.h"

// 레벨
#include "FileHelpers.h"
#include "EditorLevelUtils.h"

// 스크린샷
#include "HighResScreenshot.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// JSON
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

// ─────────────────────────────────────────────────────────────────────────────
// 공개 진입점
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEditorHandler::HandleCommand(
    const FString& CommandType,
    const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("play_in_editor"))      return PlayInEditor(Params);
    if (CommandType == TEXT("set_viewport_camera")) return SetViewportCamera(Params);
    if (CommandType == TEXT("run_console_command")) return RunConsoleCommand(Params);
    if (CommandType == TEXT("take_screenshot"))     return TakeScreenshot(Params);
    if (CommandType == TEXT("get_selected_actors")) return GetSelectedActors(Params);
    if (CommandType == TEXT("select_actors"))       return SelectActors(Params);
    if (CommandType == TEXT("save_level"))          return SaveLevel(Params);
    if (CommandType == TEXT("load_level"))          return LoadLevel(Params);

    return MakeError(TEXT("INVALID_PARAMS"),
        FString::Printf(TEXT("EditorHandler: 알 수 없는 커맨드 '%s'"), *CommandType));
}

// ─────────────────────────────────────────────────────────────────────────────
// 헬퍼
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEditorHandler::MakeSuccess(TSharedPtr<FJsonObject> Result)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("result"), Result.IsValid() ? Result : MakeShared<FJsonObject>());
    Response->SetField(TEXT("error"), MakeShared<FJsonValueNull>());
    return Response;
}

TSharedPtr<FJsonObject> FEditorHandler::MakeError(const FString& Code, const FString& Message)
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

TSharedPtr<FJsonObject> FEditorHandler::ActorToJson(AActor* Actor)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    if (!IsValid(Actor)) return Obj;

    Obj->SetStringField(TEXT("name"),        Actor->GetActorLabel());
    Obj->SetStringField(TEXT("actor_class"), Actor->GetClass()->GetName());

    const FVector Loc = Actor->GetActorLocation();
    const FRotator Rot = Actor->GetActorRotation();
    const FVector Scl = Actor->GetActorScale3D();

    TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
    LocObj->SetNumberField(TEXT("x"), Loc.X);
    LocObj->SetNumberField(TEXT("y"), Loc.Y);
    LocObj->SetNumberField(TEXT("z"), Loc.Z);
    Obj->SetObjectField(TEXT("location"), LocObj);

    TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
    RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
    RotObj->SetNumberField(TEXT("yaw"),   Rot.Yaw);
    RotObj->SetNumberField(TEXT("roll"),  Rot.Roll);
    Obj->SetObjectField(TEXT("rotation"), RotObj);

    TSharedPtr<FJsonObject> SclObj = MakeShared<FJsonObject>();
    SclObj->SetNumberField(TEXT("x"), Scl.X);
    SclObj->SetNumberField(TEXT("y"), Scl.Y);
    SclObj->SetNumberField(TEXT("z"), Scl.Z);
    Obj->SetObjectField(TEXT("scale"), SclObj);

    return Obj;
}

// ─────────────────────────────────────────────────────────────────────────────
// play_in_editor
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEditorHandler::PlayInEditor(const TSharedPtr<FJsonObject>& Params)
{
    FString Action = Params->GetStringField(TEXT("action")).TrimStartAndEnd().ToLower();
    FString Mode   = Params->GetStringField(TEXT("mode")).TrimStartAndEnd().ToLower();

    ULevelEditorSubsystem* LevelEditorSubsystem =
        GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

    if (!LevelEditorSubsystem)
    {
        return MakeError(TEXT("INTERNAL_ERROR"),
            TEXT("LevelEditorSubsystem을 가져올 수 없습니다."));
    }

    FString ResultMessage;

    if (Action == TEXT("play"))
    {
        // PIE 시작
        FRequestPlaySessionParams PlayParams;

        if (Mode == TEXT("mobile_preview"))
        {
            PlayParams.WorldType = EPlaySessionWorldType::PlayInEditorViewport;
        }
        else if (Mode == TEXT("new_editor_window"))
        {
            PlayParams.WorldType = EPlaySessionWorldType::NewWindow;
        }
        else if (Mode == TEXT("standalone"))
        {
            PlayParams.WorldType = EPlaySessionWorldType::StandaloneProcess;
        }
        else  // viewport (기본)
        {
            PlayParams.WorldType = EPlaySessionWorldType::PlayInEditorViewport;
        }

        GEditor->RequestPlaySession(PlayParams);
        ResultMessage = TEXT("PIE 시작 요청됨");
    }
    else if (Action == TEXT("stop"))
    {
        GEditor->RequestEndPlayMap();
        ResultMessage = TEXT("PIE 중지 요청됨");
    }
    else if (Action == TEXT("pause"))
    {
        if (GEditor->PlayWorld)
        {
            GEditor->SetPlayInEditorWorld(GEditor->PlayWorld);
            GEditor->PlayWorld->bDebugPauseExecution = !GEditor->PlayWorld->bDebugPauseExecution;
            ResultMessage = GEditor->PlayWorld->bDebugPauseExecution
                ? TEXT("PIE 일시정지") : TEXT("PIE 재개");
        }
        else
        {
            return MakeError(TEXT("INVALID_STATE"), TEXT("현재 PIE 세션이 없습니다."));
        }
    }
    else if (Action == TEXT("resume"))
    {
        if (GEditor->PlayWorld)
        {
            GEditor->PlayWorld->bDebugPauseExecution = false;
            ResultMessage = TEXT("PIE 재개됨");
        }
        else
        {
            return MakeError(TEXT("INVALID_STATE"), TEXT("현재 PIE 세션이 없습니다."));
        }
    }
    else
    {
        return MakeError(TEXT("INVALID_PARAMS"),
            FString::Printf(TEXT("알 수 없는 action: '%s'. 'play','stop','pause','resume' 중 하나를 사용하세요."), *Action));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("action"),  Action);
    Result->SetStringField(TEXT("mode"),    Mode);
    Result->SetStringField(TEXT("message"), ResultMessage);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// set_viewport_camera
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEditorHandler::SetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
    TArray<TSharedPtr<FJsonValue>> LocArr = Params->GetArrayField(TEXT("location"));
    TArray<TSharedPtr<FJsonValue>> RotArr = Params->GetArrayField(TEXT("rotation"));
    int32 ViewportIndex = 0;
    Params->TryGetNumberField(TEXT("viewport_index"), ViewportIndex);

    FVector NewLocation = FVector::ZeroVector;
    if (LocArr.Num() >= 3)
    {
        NewLocation = FVector(
            static_cast<float>(LocArr[0]->AsNumber()),
            static_cast<float>(LocArr[1]->AsNumber()),
            static_cast<float>(LocArr[2]->AsNumber()));
    }

    FRotator NewRotation = FRotator::ZeroRotator;
    if (RotArr.Num() >= 3)
    {
        NewRotation = FRotator(
            static_cast<float>(RotArr[0]->AsNumber()),
            static_cast<float>(RotArr[1]->AsNumber()),
            static_cast<float>(RotArr[2]->AsNumber()));
    }

    // 레벨 에디터 뷰포트 클라이언트 접근
    bool bSuccess = false;
    int32 CurrentIndex = 0;

    for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
    {
        if (CurrentIndex == ViewportIndex && ViewportClient)
        {
            ViewportClient->SetViewLocation(NewLocation);
            ViewportClient->SetViewRotation(NewRotation);
            ViewportClient->Invalidate();
            bSuccess = true;
            break;
        }
        ++CurrentIndex;
    }

    if (!bSuccess)
    {
        return MakeError(TEXT("INVALID_PARAMS"),
            FString::Printf(TEXT("뷰포트 인덱스 %d를 찾을 수 없습니다."), ViewportIndex));
    }

    TSharedPtr<FJsonObject> LocResult = MakeShared<FJsonObject>();
    LocResult->SetNumberField(TEXT("x"), NewLocation.X);
    LocResult->SetNumberField(TEXT("y"), NewLocation.Y);
    LocResult->SetNumberField(TEXT("z"), NewLocation.Z);

    TSharedPtr<FJsonObject> RotResult = MakeShared<FJsonObject>();
    RotResult->SetNumberField(TEXT("pitch"), NewRotation.Pitch);
    RotResult->SetNumberField(TEXT("yaw"),   NewRotation.Yaw);
    RotResult->SetNumberField(TEXT("roll"),  NewRotation.Roll);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetObjectField(TEXT("location"),       LocResult);
    Result->SetObjectField(TEXT("rotation"),       RotResult);
    Result->SetNumberField(TEXT("viewport_index"), ViewportIndex);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// run_console_command
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEditorHandler::RunConsoleCommand(const TSharedPtr<FJsonObject>& Params)
{
    FString CommandString = Params->GetStringField(TEXT("command_string")).TrimStartAndEnd();

    if (CommandString.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("콘솔 명령이 비어있습니다."));
    }

    // GEditor를 통해 콘솔 명령 실행
    if (GEditor)
    {
        GEditor->Exec(GEditor->GetEditorWorldContext().World(),
                       *CommandString, *GLog);
    }
    else
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("GEditor를 사용할 수 없습니다."));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("command"), CommandString);
    Result->SetStringField(TEXT("status"),  TEXT("executed"));
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// take_screenshot
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEditorHandler::TakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    FString FileName = Params->GetStringField(TEXT("file_name")).TrimStartAndEnd();
    int32 Width  = 1920;
    int32 Height = 1080;
    bool  bShowUI = false;

    Params->TryGetNumberField(TEXT("width"),  Width);
    Params->TryGetNumberField(TEXT("height"), Height);
    Params->TryGetBoolField(TEXT("show_ui"),  bShowUI);

    // 파일 이름 자동 생성
    if (FileName.IsEmpty())
    {
        FDateTime Now = FDateTime::Now();
        FileName = FString::Printf(TEXT("Screenshot_%04d%02d%02d_%02d%02d%02d"),
            Now.GetYear(), Now.GetMonth(), Now.GetDay(),
            Now.GetHour(), Now.GetMinute(), Now.GetSecond());
    }

    // 저장 경로: Saved/Screenshots/Editor/
    FString SaveDir = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("Editor");
    IFileManager::Get().MakeDirectory(*SaveDir, true);
    FString FullPath = SaveDir / FileName + TEXT(".png");

    // 콘솔 명령으로 스크린샷 캡처
    FString ScreenshotCmd = FString::Printf(
        TEXT("HighResShot %dx%d filename=%s"), Width, Height, *FullPath);

    if (GEditor)
    {
        GEditor->Exec(GEditor->GetEditorWorldContext().World(),
                       *ScreenshotCmd, *GLog);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("file_name"), FileName);
    Result->SetStringField(TEXT("file_path"), FullPath);
    Result->SetNumberField(TEXT("width"),     Width);
    Result->SetNumberField(TEXT("height"),    Height);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// get_selected_actors
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEditorHandler::GetSelectedActors(const TSharedPtr<FJsonObject>& Params)
{
    TArray<TSharedPtr<FJsonValue>> SelectedList;

    for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
    {
        if (AActor* Actor = Cast<AActor>(*It))
        {
            SelectedList.Add(MakeShared<FJsonValueObject>(ActorToJson(Actor)));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("actors"), SelectedList);
    Result->SetNumberField(TEXT("count"), SelectedList.Num());
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// select_actors
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEditorHandler::SelectActors(const TSharedPtr<FJsonObject>& Params)
{
    TArray<TSharedPtr<FJsonValue>> NamesArray = Params->GetArrayField(TEXT("actor_names"));
    bool bAppend = false;
    Params->TryGetBoolField(TEXT("append_to_selection"), bAppend);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
    }

    if (!bAppend)
    {
        GEditor->SelectNone(false, true, false);
    }

    TArray<TSharedPtr<FJsonValue>> SelectedList;
    TArray<FString> NotFoundNames;

    for (const TSharedPtr<FJsonValue>& NameVal : NamesArray)
    {
        FString Name = NameVal->AsString().TrimStartAndEnd();
        bool bFound = false;

        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase) ||
                It->GetName().Equals(Name, ESearchCase::IgnoreCase))
            {
                GEditor->SelectActor(*It, true, false, true);
                SelectedList.Add(MakeShared<FJsonValueObject>(ActorToJson(*It)));
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            NotFoundNames.Add(Name);
        }
    }

    GEditor->NoteSelectionChange();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("selected_actors"), SelectedList);
    Result->SetNumberField(TEXT("selected_count"), SelectedList.Num());

    TArray<TSharedPtr<FJsonValue>> NotFoundArr;
    for (const FString& N : NotFoundNames)
    {
        NotFoundArr.Add(MakeShared<FJsonValueString>(N));
    }
    Result->SetArrayField(TEXT("not_found"), NotFoundArr);

    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// save_level
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEditorHandler::SaveLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath = Params->GetStringField(TEXT("level_path")).TrimStartAndEnd();

    bool bSaved = false;
    FString SavedPath;

    if (LevelPath.IsEmpty())
    {
        // 현재 활성 레벨 저장
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World)
        {
            return MakeError(TEXT("INTERNAL_ERROR"), TEXT("에디터 월드를 가져올 수 없습니다."));
        }

        TArray<UPackage*> PackagesToSave;
        PackagesToSave.Add(World->GetOutermost());

        bSaved = FEditorFileUtils::SaveDirtyPackages(
            /*bPromptUserToSave=*/false,
            /*bSaveMapPackages=*/true,
            /*bSaveContentPackages=*/true,
            /*bFastSave=*/false,
            /*bNotifyNoPackagesSaved=*/false,
            /*bCanBeDeclined=*/false);

        SavedPath = World->GetPathName();
    }
    else
    {
        // 지정 레벨 저장
        UPackage* Package = FindPackage(nullptr, *LevelPath);
        if (!Package)
        {
            Package = LoadPackage(nullptr, *LevelPath, LOAD_None);
        }

        if (!Package)
        {
            return MakeError(TEXT("ASSET_NOT_FOUND"),
                FString::Printf(TEXT("레벨 '%s'를 찾을 수 없습니다."), *LevelPath));
        }

        FString FileName;
        FPackageName::TryConvertLongPackageNameToFilename(
            LevelPath, FileName, FPackageName::GetMapPackageExtension());

        bSaved = UPackage::SavePackage(Package, nullptr,
            *FileName, SAVE_NoError);
        SavedPath = LevelPath;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("saved"),      bSaved);
    Result->SetStringField(TEXT("level"),    SavedPath);
    return MakeSuccess(Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// load_level
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEditorHandler::LoadLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath = Params->GetStringField(TEXT("level_path")).TrimStartAndEnd();

    if (LevelPath.IsEmpty())
    {
        return MakeError(TEXT("INVALID_PARAMS"), TEXT("level_path가 비어있습니다."));
    }

    // 패키지 파일 경로 변환
    FString FileName;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
        LevelPath, FileName, FPackageName::GetMapPackageExtension()))
    {
        return MakeError(TEXT("INVALID_PARAMS"),
            FString::Printf(TEXT("레벨 경로 변환 실패: '%s'"), *LevelPath));
    }

    if (!FPaths::FileExists(FileName))
    {
        return MakeError(TEXT("ASSET_NOT_FOUND"),
            FString::Printf(TEXT("레벨 파일이 존재하지 않습니다: '%s'"), *FileName));
    }

    // 현재 레벨 저장 여부 묻지 않고 로드
    FEditorFileUtils::LoadMap(FileName, /*bLoadAsTemplate=*/false, /*bShowProgress=*/true);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("level_path"), LevelPath);
    Result->SetStringField(TEXT("file_path"),  FileName);
    Result->SetStringField(TEXT("status"),     TEXT("load_requested"));
    return MakeSuccess(Result);
}
