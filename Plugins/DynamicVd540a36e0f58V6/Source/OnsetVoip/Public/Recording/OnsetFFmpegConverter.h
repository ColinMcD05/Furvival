// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "OnsetFFmpegConverter.generated.h"

DECLARE_DELEGATE_OneParam(FOnsetOnExitProcess, int32)

/**
* Implements an external process that can be interacted.
*/
class ONSETVOIP_API FOnsetGenericProcess : public FRunnable, public FNoncopyable
{
public:
	FOnsetGenericProcess();
	~FOnsetGenericProcess();

	// Whether this process is currently running.
	bool IsRunning() const;

	void Cancel();

	// Start a process
	bool Launch(const FString& InURL, const FString& InWorkingDirectory, const FString& InParams);

	// Called when the program ends.
	FOnsetOnExitProcess& OnProcessExit()
	{
		return TerminatedDelegate;
	}

	//~ Begin FRunnable Interface 
	virtual bool Init() override
	{
		return true;
	}
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override { }
	//~ End FRunnable Interface 
	
protected:
	// Pass the log file back to us.
	void ProcessOutput(const FString& Output);

private:
	// Whether this thread is running
	FThreadSafeBool bThreadRunning;

	// Whether the process is being canceled.
	FThreadSafeBool bWantsCancel;

	// If true the exit delegate will be posted to the game thread.
	bool bStartedInGameThread : 1;

	// Whether the window of the process should be hidden.
	bool bHidden : 1;

	// Holds the read pipe.
	void* ReadPipe;

	// Holds the write pipe.
	void* WritePipe;

	// Holds the monitoring thread object.
	FRunnableThread* Thread;

	// Holds the handle to the process.
	FProcHandle ProcessHandle;

	// Holds a delegate that is executed when the process has been canceled.
	FOnsetOnExitProcess TerminatedDelegate;

	TArray<FString> SavedOutput;
	FString SavedParams;
};

DECLARE_DELEGATE_OneParam(FOnOnsetFFmpegCompleteCpp, int32)

class ONSETVOIP_API IOnsetFFmpegConverter : public TSharedFromThis<IOnsetFFmpegConverter>
{
public:
	virtual ~IOnsetFFmpegConverter() { }

	virtual bool Start() = 0;

	FOnOnsetFFmpegCompleteCpp OnComplete;

protected:
	bool LaunchFFmpeg(const FString& Params);

	static FString GetFFmpegExecutableAnyThread();

	TSharedPtr<FOnsetGenericProcess> Process;

	virtual void OnProcessExit(int32 ReturnCode);
};

class ONSETVOIP_API FOnsetFFmpegAudioFileConverter : public IOnsetFFmpegConverter
{
public:
	FOnsetFFmpegAudioFileConverter(const FString& InInputFile, const FString& InOutputFile, int32 InSampleRate, int32 InAudioChannels, int32 InBitRate)
		: InputFile(InInputFile)
		, OutputFile(InOutputFile)
		, SampleRate(InSampleRate)
		, AudioChannels(InAudioChannels)
		, BitRate(InBitRate)
	{

	}

	virtual bool Start() override;

private:
	const FString InputFile;
	const FString OutputFile;
	const int32 SampleRate;
	const int32 AudioChannels;
	const int32 BitRate;

};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOnsetFFmpegComplete);

UCLASS()
class ONSETVOIP_API UOnsetFFmpegAudioConverter : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UOnsetFFmpegAudioConverter(const FObjectInitializer& ObjectInitializer);

	/*
	* Converts an audio file to a different audio format. Requires FFmpeg, see OnsetVoip projects settings.
	*
	* InputFile: The full path to source file. (Example: C:/MyWavFile.wav)
	* OutputFile: The new file to save to. (Example: C:/MyConvertedFile.mp3)
	* SampleRate: New sample rate to convert to. (Example: 44100)
	* AudioChannels: Number of audio channels to convert to. (Example: 1 (mono), 2 (stereo))
	* BitRate: Bitrate to convert to. (Example: 128000 for 128k)
	* 
	* FFmpeg cmdline parameters: -y -i \"%s\" -vn -ar %d -ac %d -b:a %d \"%s\"
	* Please refer to the FFmpeg documentation for valid parameters and formats.
	* 
	* On error the output of ffmpeg will be printed to the game log.
	*/
	UFUNCTION(BlueprintCallable, Category = "Utility|FFmpeg", meta = (KeyWords = "ffmpeg,mp3,wav", BlueprintInternalUseOnly = "true"))
	static UOnsetFFmpegAudioConverter* ConvertAudioFile(const FString& InputFile, const FString& OutputFile, int32 SampleRate, int32 AudioChannels, int32 BitRate);

	UPROPERTY(BlueprintAssignable)
	FOnOnsetFFmpegComplete OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FOnOnsetFFmpegComplete OnFail;

protected:
	void Start(const TSharedRef<IOnsetFFmpegConverter>& InConverter);

	void OnConverterComplete(int32 ReturnCode);

	TSharedPtr<IOnsetFFmpegConverter> Converter;
};
