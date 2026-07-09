// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "Recording/OnsetVoipRecorderLocal.h"

#if !UE_SERVER

#include "OnsetVoip.h"
#include "Misc/Paths.h"

FOnsetVoipRecorderLocal::FOnsetVoipRecorderLocal(UOnsetVoipLocalPlayerSubsystem* InSubsystem)
	: Subsystem(InSubsystem)
{
	check(Subsystem.IsValid());
}

FOnsetVoipRecorderLocal::~FOnsetVoipRecorderLocal()
{
	if (Subsystem.IsValid() && RecordCaptureDelegate.IsValid())
	{
		Subsystem->Native_OnVoipMicrophoneAudioCaptured.Remove(RecordCaptureDelegate);
		RecordCaptureDelegate.Reset();

		OnsetVoip::ConsoleLog(Subsystem.Get(), FString::Printf(TEXT("Local microphone recording stopped.")));
	}
}

bool FOnsetVoipRecorderLocal::Init(EOnsetAudioRecordingFormat InRecordingFormat, const TOptional<FString>& InFilename)
{
	if (IOnsetVoipRecorder::Init(InRecordingFormat, InFilename))
	{
		const FString Filename = InFilename.IsSet() ? *InFilename : FPaths::Combine(GetRecordingDirectory(), FString::Printf(TEXT("Local_%s.%s"), *FDateTime::Now().ToString(), GetBufferedAudioFileExtension(RecordingFormat)));
		AudioFile = CreateAudioFile(Filename, RecordingFormat);

		if (AudioFile.IsValid())
		{
			RecordCaptureDelegate = Subsystem->Native_OnVoipMicrophoneAudioCaptured.AddSP(this, &FOnsetVoipRecorderLocal::OnVoipMicrophoneAudioCaptured);

			OnsetVoip::ConsoleLog(Subsystem.Get(), FString::Printf(TEXT("Local microphone recording started: %s"), *AudioFile->GetFilename()));
			return true;
		}
	}

	OnsetVoip::ConsoleLog(Subsystem.Get(), TEXT("Unable to start local microphone recording."));
	return false;
}

void FOnsetVoipRecorderLocal::OnVoipMicrophoneAudioCaptured(const TArrayView<uint8>& AudioData)
{
	if (AudioFile.IsValid() && GEngine)
	{
		AudioFile->Write(AudioData);

		GEngine->AddOnScreenDebugMessage(6142, 0.05f, FColor::Green, FString::Printf(TEXT("Recording microphone to %s"), *AudioFile->GetFilename()), false);
	}
}

FString FOnsetVoipRecorderLocal::GetRecordingFile() const
{
	return AudioFile.IsValid() ? AudioFile->GetFilename() : TEXT("");
}

#endif /* !UE_SERVER */
