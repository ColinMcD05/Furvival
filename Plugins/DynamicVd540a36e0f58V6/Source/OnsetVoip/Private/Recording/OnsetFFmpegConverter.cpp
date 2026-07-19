// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "Recording/OnsetFFmpegConverter.h"
#include "OnsetVoipSettings.h"
#include "Misc/Paths.h"
#include "Async/Async.h"

DECLARE_LOG_CATEGORY_CLASS(LogOnsetFFmpegConverter, Warning, All)

bool IOnsetFFmpegConverter::LaunchFFmpeg(const FString& Params)
{
	const FString FFmpegPath = GetFFmpegExecutableAnyThread();
	if (!FPaths::FileExists(FFmpegPath))
	{
		UE_LOG(LogOnsetFFmpegConverter, Error, TEXT("FFmpeg executable not found. (Got %s)"), *FFmpegPath);
		return false;
	}
	else
	{
		UE_LOG(LogOnsetFFmpegConverter, Log, TEXT("Using FFmpeg at %s"), *FFmpegPath);
	}

	Process = MakeShared<FOnsetGenericProcess>();
	Process->OnProcessExit().BindSP(this, &IOnsetFFmpegConverter::OnProcessExit);
	return Process->Launch(FFmpegPath, FPaths::GetPath(FFmpegPath), Params);
}

namespace AsyncHelpers
{
	template<typename ResultType>
	static ResultType GetValueOnGameThread(const TFunction<ResultType()>& Function)
	{
		if (!IsInGameThread())
		{
			const TSharedRef<TPromise<ResultType>, ESPMode::ThreadSafe> Promise = MakeShared<TPromise<ResultType>>();
			AsyncTask(ENamedThreads::GameThread, [Promise, Function]()
				{
					Promise->SetValue(Function());
				});
			return Promise->GetFuture().Get();
		}
		return Function();
	}
}

FString IOnsetFFmpegConverter::GetFFmpegExecutableAnyThread()
{
	FString FFmpegPath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("FFmpegPath="), FFmpegPath))
	{
		FFmpegPath = FPlatformMisc::GetEnvironmentVariable(TEXT("FFMPEG"));
		if (FFmpegPath.Len() == 0)
		{
			FFmpegPath = FPlatformMisc::GetEnvironmentVariable(TEXT("FFMPEG_PATH"));
			if (FFmpegPath.Len() == 0)
			{
				FFmpegPath = AsyncHelpers::GetValueOnGameThread<decltype(FFmpegPath)>([]()
					{
						return GetDefault<UOnsetVoipSettings>()->FFmpegPath.Path;
					});
			}
		}
	}
	
	if (FPaths::GetPath(FFmpegPath).Len() > 0 && FPaths::IsRelative(FFmpegPath))
	{
		const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FFmpegPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectDir, FFmpegPath));
	}

	if (FPaths::DirectoryExists(FPaths::GetPath(FFmpegPath)) && !FPaths::FileExists(FFmpegPath))
	{
#if PLATFORM_WINDOWS
		const FString FFmpegExecutable("ffmpeg.exe");
#else
		const FString FFmpegExecutable("ffmpeg");
#endif

		FFmpegPath = FPaths::Combine(FFmpegPath, FFmpegExecutable);
	}

	return FFmpegPath;
}

void IOnsetFFmpegConverter::OnProcessExit(int32 ReturnCode)
{
	OnComplete.ExecuteIfBound(ReturnCode);
}

bool FOnsetFFmpegAudioFileConverter::Start()
{
	if (!FPaths::FileExists(InputFile))
	{
		UE_LOG(LogOnsetFFmpegConverter, Error, TEXT("InputFile %s does not exist."), *InputFile);
		return false;
	}

	FString NewOutputFile = OutputFile;
	if (OutputFile.Len() == 0)
	{
		UE_LOG(LogOnsetFFmpegConverter, Error, TEXT("OutputFile name is empty."));
		return false;
	}
	else if (OutputFile.Len() == 4 && OutputFile[0] == '.')
	{
		NewOutputFile = FPaths::ChangeExtension(InputFile, OutputFile);
		UE_LOG(LogOnsetFFmpegConverter, Log, TEXT("OutputFile changed to %s"), *NewOutputFile);
	}

	const FString Params = FString::Printf(TEXT("-y -i \"%s\" -vn -ar %d -ac %d -b:a %d \"%s\""),
		*InputFile,
		SampleRate,
		AudioChannels,
		BitRate,
		*NewOutputFile);

	return LaunchFFmpeg(Params);
}

FOnsetGenericProcess::FOnsetGenericProcess()
	: bThreadRunning(false)
	, bWantsCancel(false)
	, bStartedInGameThread(false)
	, bHidden(true)
	, ReadPipe(nullptr)
	, WritePipe(nullptr)
	, Thread(nullptr)
{

}

FOnsetGenericProcess::~FOnsetGenericProcess()
{
	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

bool FOnsetGenericProcess::IsRunning() const
{
	return bThreadRunning;
}

void FOnsetGenericProcess::Cancel()
{
	bWantsCancel = true;
}

bool FOnsetGenericProcess::Launch(const FString& InURL, const FString& InWorkingDirectory, const FString& InParams)
{
	check(!IsRunning());

	if (!FPlatformProcess::CreatePipe(ReadPipe, WritePipe))
	{
		UE_LOG(LogOnsetFFmpegConverter, Error, TEXT("Failed to create process pipes."));
		return false;
	}

	UE_LOG(LogOnsetFFmpegConverter, Verbose, TEXT("Running: \"%s %s\" [WorkingDirectory: %s]"), *InURL, *InParams, *InWorkingDirectory);

	ProcessHandle = FPlatformProcess::CreateProc(*InURL, *InParams, false, bHidden, bHidden, nullptr, 0, *InWorkingDirectory, WritePipe, ReadPipe);

	if (!ProcessHandle.IsValid())
	{
		UE_LOG(LogOnsetFFmpegConverter, Error, TEXT("Failed to create process."));

		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = WritePipe = nullptr;
		return false;
	}

	// Creating a name for the thread
	static uint32 TempInteractiveProcessIndex = 0;
	const FString ThreadName = FString::Printf(TEXT("FOnsetGenericProcess%d"), TempInteractiveProcessIndex);
	TempInteractiveProcessIndex++;

	bStartedInGameThread = IsInGameThread();
	SavedParams = InParams;

	Thread = FRunnableThread::Create(this, *ThreadName);
	ensure(Thread);

	return Thread != nullptr;
}

uint32 FOnsetGenericProcess::Run()
{
	bThreadRunning = true;

	bool bProcessRunning = false;
	do
	{
		// A 1 millisecond sleep is a problem on Windows, so don't change this.
		FPlatformProcess::Sleep(0.0f);

		// Read pipe
		ProcessOutput(FPlatformProcess::ReadPipe(ReadPipe));

		bProcessRunning = ProcessHandle.IsValid() && FPlatformProcess::IsProcRunning(ProcessHandle);
	}
	while (!bWantsCancel && bProcessRunning);

	int32 ReturnCode = 0;
	FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);

	if (bWantsCancel == false)
	{
		if (ReturnCode != 0)
		{
			UE_LOG(LogOnsetFFmpegConverter, Error, TEXT("Params: %s"), *SavedParams);
			UE_LOG(LogOnsetFFmpegConverter, Error, TEXT("FFmpeg exited with a non-zero value: %i"), ReturnCode);

			for (FString& Line : SavedOutput)
			{
				if (Line.Len() > 0)
				{
					//Line.RemoveFromEnd(TEXT("\r\n"));
					//Line.RemoveFromEnd(TEXT("\n"));
					Line.RemoveFromEnd(TEXT("\r"));

					UE_LOG(LogOnsetFFmpegConverter, Warning, TEXT("%s"), *Line);
				}
			}
		}
	}

	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
	ReadPipe = WritePipe = nullptr;

	// Ensure the process is killed when this thread exits.
	FPlatformProcess::CloseProc(ProcessHandle);

	if (bWantsCancel == false)
	{
		// Sleep to ensure process is closed. Do not sleep if canceled.
		FPlatformProcess::Sleep(0.05f);

		if (bStartedInGameThread)
		{
			AsyncTask(ENamedThreads::GameThread, [ReturnCode, Delegate = TerminatedDelegate]()
				{
					Delegate.ExecuteIfBound(ReturnCode);
				});
		}
		else
		{
			TerminatedDelegate.ExecuteIfBound(ReturnCode);
		}
	}

	bThreadRunning = false;

	return 0;
}

void FOnsetGenericProcess::Stop()
{
	bWantsCancel = true;
}

void FOnsetGenericProcess::ProcessOutput(const FString& Output)
{
	TArray<FString> LogLines;

	if (Output.Len() > 0 && Output.ParseIntoArray(LogLines, TEXT("\n"), true) > 0)
	{
		SavedOutput.Append(MoveTemp(LogLines));
	}
}

UOnsetFFmpegAudioConverter::UOnsetFFmpegAudioConverter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AddToRoot();
	}
}

UOnsetFFmpegAudioConverter* UOnsetFFmpegAudioConverter::ConvertAudioFile(const FString& InputFile, const FString& OutputFile, int32 SampleRate, int32 AudioChannels, int32 BitRate)
{
	UOnsetFFmpegAudioConverter* Task = NewObject<UOnsetFFmpegAudioConverter>();
	Task->Start(MakeShared<FOnsetFFmpegAudioFileConverter>(InputFile, OutputFile, SampleRate, AudioChannels, BitRate));
	return Task;
}

void UOnsetFFmpegAudioConverter::Start(const TSharedRef<IOnsetFFmpegConverter>& InConverter)
{
	Converter = InConverter;
	Converter->OnComplete.BindUObject(this, &UOnsetFFmpegAudioConverter::OnConverterComplete);
	if (!Converter->Start())
	{
		// Hack: Delay the error to have the delegate called.
		AsyncTask(ENamedThreads::GameThread, [this]()
			{
				OnConverterComplete(-1);
			});
	}
}

void UOnsetFFmpegAudioConverter::OnConverterComplete(int32 ReturnCode)
{
	Converter = nullptr;

	RemoveFromRoot();

	if (ReturnCode == 0)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFail.Broadcast();
	}
}
