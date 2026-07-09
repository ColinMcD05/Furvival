// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnsetAudioFile.h"
#include "Misc/Optional.h"
#include "OnsetVoipRecorder.generated.h"

UENUM(BlueprintType)
enum class EOnsetAudioRecordingFormat : uint8
{
	// Save audio as raw PCM data.
	FORMAT_PCM,

	// Save audio as a wav file with PCM data.
	FORMAT_WAV,

	// Requires FFmpeg to be configured!
	FORMAT_MP3
};

ONSETVOIP_API const TCHAR* LexToString(EOnsetAudioRecordingFormat V);

class ONSETVOIP_API IOnsetVoipRecorder : public TSharedFromThis<IOnsetVoipRecorder>
{
public:
	virtual ~IOnsetVoipRecorder() = default;

	virtual bool Init(EOnsetAudioRecordingFormat InRecordingFormat, const TOptional<FString>& InFilename = TOptional<FString>());

	static const TCHAR* GetBufferedAudioFileExtension(EOnsetAudioRecordingFormat InRecordingFormat);

	static FString GetRecordingDirectory();

	virtual FString GetRecordingFile() const = 0;

protected:
	static TSharedPtr<IOnsetAudioFile> CreateAudioFile(const FString& InFilename, EOnsetAudioRecordingFormat InRecordingFormat);

	EOnsetAudioRecordingFormat RecordingFormat;
	FDelegateHandle RecordCaptureDelegate;
};

EOnsetAudioRecordingFormat RecordingFormatFromExtension(const FString& InFileExtension);

template<typename TRecorder, typename TSubsystem>
TSharedPtr<IOnsetVoipRecorder> CreateAudioRecorder(TSubsystem* InSubsystem, EOnsetAudioRecordingFormat InRecordingFormat, const TOptional<FString>& InFilename = TOptional<FString>())
{
	TSharedPtr<IOnsetVoipRecorder> Recorder = MakeShared<TRecorder>(InSubsystem);
	if (Recorder->Init(InRecordingFormat, InFilename))
	{
		return Recorder;
	}
	return nullptr;
}

template<typename TRecorder, typename TSubsystem>
TSharedPtr<IOnsetVoipRecorder> CreateAudioRecorder(TSubsystem* InSubsystem, const FString InRecordingFormatStr)
{
	return CreateAudioRecorder<TRecorder, TSubsystem>(InSubsystem, RecordingFormatFromExtension(InRecordingFormatStr));
}
