// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if !UE_SERVER

#include "Recording/OnsetVoipRecorder.h"
#include "OnsetVoipLocalPlayerSubsystem.h"

class ONSETVOIP_API FOnsetVoipRecorderLocal : public IOnsetVoipRecorder
{
public:
	FOnsetVoipRecorderLocal(class UOnsetVoipLocalPlayerSubsystem* InSubsystem);
	~FOnsetVoipRecorderLocal();

	virtual bool Init(EOnsetAudioRecordingFormat InRecordingFormat, const TOptional<FString>& InFilename = TOptional<FString>()) override;

	void OnVoipMicrophoneAudioCaptured(const TArrayView<uint8>& AudioData);

	virtual FString GetRecordingFile() const override;

private:
	TWeakObjectPtr<UOnsetVoipLocalPlayerSubsystem> Subsystem;
	TSharedPtr<IOnsetAudioFile> AudioFile;
};

#endif
