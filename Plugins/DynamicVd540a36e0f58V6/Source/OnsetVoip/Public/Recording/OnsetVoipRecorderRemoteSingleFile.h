// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

#if ONSETVOIP_ENABLE_VOICE_PROCESSING

#if ENGINE_MINOR_VERSION >= 2
#include "Engine/TimerHandle.h"
#endif
#include "Recording/OnsetVoipRecorder.h"
#include "OnsetVoipWorldSubsystem.h"
#include "OnsetVoipRelevancyDefinitions.h"

class ONSETVOIP_API FOnsetVoipRecorderRemoteSingleFile : public IOnsetVoipRecorder
{
public:
	typedef TMap<TWeakObjectPtr<UOnsetVoipTalker>, TArray<float>, FDefaultSetAllocator, TWeakObjectPtrMapKeyFuncs<TWeakObjectPtr<UOnsetVoipTalker>, TArray<float>>> TTalkerToAudioFileMap;

	FOnsetVoipRecorderRemoteSingleFile(UOnsetVoipWorldSubsystem* InSubsystem);
	~FOnsetVoipRecorderRemoteSingleFile();

	virtual bool Init(EOnsetAudioRecordingFormat InRecordingFormat, const TOptional<FString>& InFilename = TOptional<FString>()) override;

	void OnRenderAudioData();

	void OnVoipAudioDataReceived(UOnsetVoipTalker* VoipTalker, const TArrayView<uint8>& AudioData, EOnsetVoipNetRelevancy Relevancy);

	virtual FString GetRecordingFile() const override;

private:
	TMap<UOnsetVoipTalker*, TArray<uint8>> CachedAudioData;
	FTimerHandle TimerHandle;
	TWeakObjectPtr<UOnsetVoipWorldSubsystem> Subsystem;
	TSharedPtr<IOnsetAudioFile> AudioFile;
};

#endif
