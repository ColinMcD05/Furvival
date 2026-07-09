// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "Recording/OnsetVoipRecorder.h"
#include "OnsetVoipSettings.h"
#include "OnsetVoip.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

const TCHAR* LexToString(EOnsetAudioRecordingFormat V)
{
	switch (V)
	{
	case EOnsetAudioRecordingFormat::FORMAT_PCM:
		return TEXT("FORMAT_PCM");
	case EOnsetAudioRecordingFormat::FORMAT_WAV:
		return TEXT("FORMAT_WAV");
	case EOnsetAudioRecordingFormat::FORMAT_MP3:
		return TEXT("FORMAT_MP3");
	default:
		return TEXT("Unknown");
	}
}

static const TMap<FString, EOnsetAudioRecordingFormat> ExtensionFormats =
{
	{ "pcm", EOnsetAudioRecordingFormat::FORMAT_PCM },
	{ "wav", EOnsetAudioRecordingFormat::FORMAT_WAV },
	{ "mp3", EOnsetAudioRecordingFormat::FORMAT_MP3 }
};

EOnsetAudioRecordingFormat RecordingFormatFromExtension(const FString& InFileExtension)
{
	if (const EOnsetAudioRecordingFormat* Format = ExtensionFormats.Find(InFileExtension.ToLower()))
	{
		return *Format;
	}

	return EOnsetAudioRecordingFormat::FORMAT_PCM;
}

bool IOnsetVoipRecorder::Init(EOnsetAudioRecordingFormat InRecordingFormat, const TOptional<FString>& InFilename)
{
	RecordingFormat = InRecordingFormat;
	return true;
}

const TCHAR* IOnsetVoipRecorder::GetBufferedAudioFileExtension(EOnsetAudioRecordingFormat InRecordingFormat)
{
	if (const FString* Extension = ExtensionFormats.FindKey(InRecordingFormat))
	{
		return **Extension;
	}

	checkNoEntry();
	return TEXT("");
}

FString IOnsetVoipRecorder::GetRecordingDirectory()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VoipRecording"));
}

TSharedPtr<IOnsetAudioFile> IOnsetVoipRecorder::CreateAudioFile(const FString& InFilename, EOnsetAudioRecordingFormat InRecordingFormat)
{
	UE_LOG(LogOnsetVoip, Verbose, TEXT("CreateAudioFile(%s, %s)"), *InFilename, LexToString(InRecordingFormat));

	constexpr uint32 WriteFlags = FILEWRITE_Silent | FILEWRITE_AllowRead;
	const TSharedPtr<FArchive> Ar = MakeShareable<FArchive>(IFileManager::Get().CreateFileWriter(*InFilename, WriteFlags));

	if (Ar.IsValid())
	{
		if (InRecordingFormat == EOnsetAudioRecordingFormat::FORMAT_WAV)
		{
			const int32 SampleRate = GetDefault<UOnsetVoipSettings>()->VoiceCaptureSampleRate;
			const int32 NumChannels = GetDefault<UOnsetVoipSettings>()->VoiceCaptureChannels;

			return MakeShared<FOnsetAudioWavFile>(InFilename, Ar.ToSharedRef(), SampleRate, NumChannels);
		}
		else if (InRecordingFormat == EOnsetAudioRecordingFormat::FORMAT_MP3)
		{
			const int32 SampleRate = GetDefault<UOnsetVoipSettings>()->VoiceCaptureSampleRate;
			const int32 NumChannels = GetDefault<UOnsetVoipSettings>()->VoiceCaptureChannels;

			return MakeShared<FOnsetAudioMp3File>(InFilename, Ar.ToSharedRef(), SampleRate, NumChannels);
		}
		else if (InRecordingFormat == EOnsetAudioRecordingFormat::FORMAT_PCM)
		{
			return MakeShared<FOnsetAudioPcmFile>(InFilename, Ar.ToSharedRef());
		}
	}

	UE_LOG(LogOnsetVoip, Error, TEXT("Failed to create %s recording file %s."), LexToString(InRecordingFormat), *InFilename);
	return nullptr;
}
