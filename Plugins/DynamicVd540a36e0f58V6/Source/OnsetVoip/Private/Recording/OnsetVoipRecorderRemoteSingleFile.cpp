// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#if ONSETVOIP_ENABLE_VOICE_PROCESSING

#include "Recording/OnsetVoipRecorderRemoteSingleFile.h"
#include "OnsetVoip.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "DSP/AlignedBuffer.h"
#include "DSP/FloatArrayMath.h"

FOnsetVoipRecorderRemoteSingleFile::FOnsetVoipRecorderRemoteSingleFile(UOnsetVoipWorldSubsystem* InSubsystem)
	: Subsystem(InSubsystem)
{
	check(Subsystem.IsValid());
}

FOnsetVoipRecorderRemoteSingleFile::~FOnsetVoipRecorderRemoteSingleFile()
{
	if (Subsystem.IsValid() && RecordCaptureDelegate.IsValid())
	{
		Subsystem->Native_OnVoipAudioDataReceived.Remove(RecordCaptureDelegate);
		RecordCaptureDelegate.Reset();

		OnsetVoip::ConsoleLog(Subsystem.Get(), FString::Printf(TEXT("Remote talker recording stopped.")));
	}

	TimerHandle.Invalidate();
}

bool FOnsetVoipRecorderRemoteSingleFile::Init(EOnsetAudioRecordingFormat InRecordingFormat, const TOptional<FString>& InFilename)
{
	if (IOnsetVoipRecorder::Init(InRecordingFormat, InFilename))
	{
		const FString Filename = InFilename.IsSet() ? *InFilename : FPaths::Combine(GetRecordingDirectory(), FString::Printf(TEXT("Remote_%s.%s"), *FDateTime::Now().ToString(), GetBufferedAudioFileExtension(RecordingFormat)));
		AudioFile = CreateAudioFile(Filename, RecordingFormat);

		if (AudioFile.IsValid())
		{
			float VoipRecorderRenderTime = 0.6f;
			FParse::Value(FCommandLine::Get(), TEXT("VoipRecorderRenderTime="), VoipRecorderRenderTime);

			RecordCaptureDelegate = Subsystem->Native_OnVoipAudioDataReceived.AddSP(this, &FOnsetVoipRecorderRemoteSingleFile::OnVoipAudioDataReceived);

			Subsystem->GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateSP(this, &FOnsetVoipRecorderRemoteSingleFile::OnRenderAudioData), VoipRecorderRenderTime, /*InbLoop=*/true);

			OnsetVoip::ConsoleLog(Subsystem.Get(), FString::Printf(TEXT("Remote talker recording started: %s (VoipRecorderRenderTime: %2f)"), *AudioFile->GetFilename(), VoipRecorderRenderTime));
			return true;
		}
	}

	OnsetVoip::ConsoleLog(Subsystem.Get(), TEXT("Unable to start remote talker recording."));
	return false;
}

void FOnsetVoipRecorderRemoteSingleFile::OnRenderAudioData()
{
	if (!AudioFile.IsValid() || GEngine == nullptr)
	{
		CachedAudioData.Empty();
		return;
	}

	if (CachedAudioData.Num() == 0)
	{
		return;
	}

	// Determine the maximum audio size
	int32 MaxCachedAudioDataLength = 0;
	for (const auto& MapPair : CachedAudioData)
	{
		if (MapPair.Value.Num() > MaxCachedAudioDataLength)
		{
			MaxCachedAudioDataLength = MapPair.Value.Num();
		}
	}
	
	if (MaxCachedAudioDataLength == 0)
	{
		return;
	}

	const int32 FixedBufferLengthInt16 = MaxCachedAudioDataLength * sizeof(uint8) / sizeof(int16);

	// Setup a fixed length buffer that we will mix all audio to. Must be zeroed to be silence by default.
	TArray<float> MixedBufferFloat;
	MixedBufferFloat.AddZeroed(FixedBufferLengthInt16);

	for (auto& MapPair : CachedAudioData)
	{
		// Ensure all audio is the same size
		MapPair.Value.SetNumZeroed(MaxCachedAudioDataLength);

		// Convert to float for ArrayMixIn()
		Audio::FAlignedFloatBuffer TransformationsBuffer;
		TransformationsBuffer.AddUninitialized(FixedBufferLengthInt16);
		Audio::ArrayPcm16ToFloat(MakeArrayView((int16*)MapPair.Value.GetData(), MaxCachedAudioDataLength / sizeof(int16)), TransformationsBuffer);

		// Do the mixing
		Audio::ArrayMixIn(TransformationsBuffer, MixedBufferFloat);
	}

	// Empty cached audio as we no longer need it.
	CachedAudioData.Empty();

	// Back to pcm16
	TArray<uint8> MixedBufferPcm16;
	MixedBufferPcm16.AddUninitialized(MixedBufferFloat.Num() * sizeof(int16));
	Audio::ArrayFloatToPcm16(MixedBufferFloat, MakeArrayView((int16*)MixedBufferPcm16.GetData(), MixedBufferPcm16.Num() / sizeof(int16)));

	AudioFile->Write(MixedBufferPcm16);

	GEngine->AddOnScreenDebugMessage(6142, 0.05f, FColor::Green, FString::Printf(TEXT("Recording remote talkers to %s"), *AudioFile->GetFilename()), false);
}

void FOnsetVoipRecorderRemoteSingleFile::OnVoipAudioDataReceived(UOnsetVoipTalker* VoipTalker, const TArrayView<uint8>& AudioData, EOnsetVoipNetRelevancy Relevancy)
{
	TArray<uint8>& PlayerAudioData = CachedAudioData.FindOrAdd(VoipTalker);
	const int32 ElementsBefore = PlayerAudioData.AddUninitialized(AudioData.Num());
	FMemory::Memcpy(PlayerAudioData.GetData() + ElementsBefore, AudioData.GetData(), AudioData.Num());
}

FString FOnsetVoipRecorderRemoteSingleFile::GetRecordingFile() const
{
	return AudioFile.IsValid() ? AudioFile->GetFilename() : TEXT("");
}

#endif /* ONSETVOIP_ENABLE_VOICE_PROCESSING */
