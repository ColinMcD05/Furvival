// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetVoipLocalPlayerSubsystem.h"
#include "OnsetVoipWorldSubsystem.h"
#include "Recording/OnsetVoipRecorderLocal.h"
#include "Recording/OnsetVoipRecorderRemote.h"
#include "Recording/OnsetVoipRecorderRemoteSingleFile.h"

#if !UE_SERVER && ONSETVOIP_ENABLE_VOICE_PROCESSING

FAutoConsoleCommandWithWorldAndArgs _CmdVoiceRecordLocal(TEXT("voice.record.local"),
	TEXT("Records local microphone input and saves it to a file on disk.\n")
	TEXT("Optional parameter \"<format>\": pcm, wav, mp3 (mp3 requires FFmpeg) (Default: \"wav\")\n")
	TEXT("PCM format is signed 16 bit, channels and sample rate according to your project settings. Raw PCM files can be opened with programs like Audacity."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
{
	// Example #1

	/*static TSharedPtr<IOnsetVoipRecorder> AudioRecording;

	if (AudioRecording.IsValid())
	{
		AudioRecording.Reset();
	}
	else
	{
		if (const ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController())
		{
			if (UOnsetVoipLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UOnsetVoipLocalPlayerSubsystem>())
			{
				AudioRecording = CreateAudioRecorder<FOnsetVoipRecorderLocal, UOnsetVoipLocalPlayerSubsystem>(Subsystem, Args.Num() > 0 ? Args[0] : "");
			}
		}
	}*/

	// Example #2

	if (const ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController())
	{
		if (UOnsetVoipLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UOnsetVoipLocalPlayerSubsystem>())
		{
			if (Subsystem->IsRecording())
			{
				Subsystem->StopRecording();
			}
			else
			{
				Subsystem->StartRecording("", RecordingFormatFromExtension(Args.Num() > 0 ? Args[0] : ""));
			}
		}
	}
}));

#endif /* !UE_SERVER */

#if ONSETVOIP_ENABLE_VOICE_PROCESSING

FAutoConsoleCommandWithWorldAndArgs _CmdVoiceRecordRemote(TEXT("voice.record.remote"),
	TEXT("Records remote VoIP talkers and saves them to a file on disk.\n")
	TEXT("Optional parameter \"<format>\": pcm, wav, mp3 (mp3 requires FFmpeg) (Default: \"wav\")\n")
	TEXT("PCM format is signed 16 bit, channels and sample rate according to your project settings. Raw PCM files can be opened with programs like Audacity.\n")
	TEXT("Optional parameter \"<single_file>\". \"True\": Combines all voice data from remote talkers into a single file. \"False\": Saves each remote talker into a single audio file. Default: \"False\".\n"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
{
	// Example #1

	/*static TSharedPtr<IOnsetVoipRecorder> AudioRecording;

	if (AudioRecording.IsValid())
	{
		AudioRecording.Reset();
	}
	else
	{
		if (UOnsetVoipWorldSubsystem* Subsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>())
		{
			const bool bSingleFile = Args.Num() > 1 ? Args[1].ToBool() : false;
			if (bSingleFile)
			{
				AudioRecording = CreateAudioRecorder<FOnsetVoipRecorderRemoteSingleFile, UOnsetVoipWorldSubsystem>(Subsystem, Args.Num() > 0 ? Args[0] : "");
			}
			else
			{
				AudioRecording = CreateAudioRecorder<FOnsetVoipRecorderRemote, UOnsetVoipWorldSubsystem>(Subsystem, Args.Num() > 0 ? Args[0] : "");
			}
		}
	}*/

	// Example #2

	if (UOnsetVoipWorldSubsystem* Subsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>())
	{
		if (Subsystem->IsRecording())
		{
			Subsystem->StopRecording();
		}
		else
		{
			const bool bSingleFile = Args.Num() > 1 ? Args[1].ToBool() : false;
			Subsystem->StartRecording("", RecordingFormatFromExtension(Args.Num() > 0 ? Args[0] : ""), bSingleFile);
		}
	}
}));

#endif
