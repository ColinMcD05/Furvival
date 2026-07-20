// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if ONSETVOIP_ENABLE_VOICE_PROCESSING

#include "Recording/OnsetVoipRecorder.h"
#include "OnsetVoipWorldSubsystem.h"
#include "OnsetVoipRelevancyDefinitions.h"

class ONSETVOIP_API FOnsetVoipRecorderRemote : public IOnsetVoipRecorder
{
public:
	typedef TMap<TWeakObjectPtr<UOnsetVoipTalker>, TSharedPtr<IOnsetAudioFile>, FDefaultSetAllocator, TWeakObjectPtrMapKeyFuncs<TWeakObjectPtr<UOnsetVoipTalker>, TSharedPtr<IOnsetAudioFile>>> TTalkerToAudioFileMap;

	FOnsetVoipRecorderRemote(UOnsetVoipWorldSubsystem* InSubsystem);
	~FOnsetVoipRecorderRemote();

	virtual bool Init(EOnsetAudioRecordingFormat InRecordingFormat, const TOptional<FString>& InFilename = TOptional<FString>()) override;

	void OnVoipAudioDataReceived(UOnsetVoipTalker* VoipTalker, const TArrayView<uint8>& AudioData, EOnsetVoipNetRelevancy Relevancy);

	void OnVoipTalkerDestroyed(UOnsetVoipTalker* VoipTalker);

	virtual FString GetRecordingFile() const override;

private:
	FString RecordingDirectory;
	TWeakObjectPtr<UOnsetVoipWorldSubsystem> Subsystem;
	FDelegateHandle VoipTalkerDestroyedDelegate;
	TTalkerToAudioFileMap TalkerToAudioFileMap;
};

#endif
