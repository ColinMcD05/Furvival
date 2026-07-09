// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetVoipSoundWave.h"
#include "OnsetVoip.h"
#include "OnsetVoipSettings.h"

// Number of pre-allocated chunk nodes in the packet buffer pool.
static constexpr int32 PacketBufferPoolSize = 64;

UOnsetVoipSoundWave::UOnsetVoipSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	const UOnsetVoipSettings* Settings = GetDefault<UOnsetVoipSettings>();

	SetSampleRate(Settings->VoiceCaptureSampleRate);
	NumChannels = Settings->VoiceCaptureChannels;
	Volume = FMath::Max(Settings->DefaultSoundWaveVolume, 0.0f);
	Duration = INDEFINITELY_LOOPING_DURATION;
	SoundGroup = ESoundGroup::SOUNDGROUP_Voice;
	bLooping = false;
	bCanProcessAsync = true;
	VirtualizationMode = EVirtualizationMode::PlayWhenSilent;

	FrameByteSize = SampleByteSize * NumChannels;
	InitialPlaybackBufferDelay = FMath::Max(0.0, Settings->InitialPlaybackBufferDelay);
	MaxBufferPlaybackDelay = FMath::Max(InitialPlaybackBufferDelay, Settings->MaxBufferPlaybackDelay);
	InitialBufferToStart = OnsetVoip::AlignToFrameBoundary(FMath::CeilToInt32(SampleRate * InitialPlaybackBufferDelay) * FrameByteSize, FrameByteSize);
	MaxBufferToPlay = OnsetVoip::AlignToFrameBoundary(FMath::CeilToInt32(SampleRate * MaxBufferPlaybackDelay) * FrameByteSize, FrameByteSize);

	if (InitialBufferToStart == 0 && InitialPlaybackBufferDelay > 0.0)
	{
		InitialBufferToStart = FrameByteSize;
	}
	if (MaxBufferToPlay == 0 && MaxBufferPlaybackDelay > 0.0)
	{
		MaxBufferToPlay = FrameByteSize;
	}

	ensureAlwaysMsgf(MaxBufferToPlay % 2 == 0, TEXT("MaxBufferToPlay %i"), MaxBufferToPlay);
}

void UOnsetVoipSoundWave::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		OnSoundWaveProceduralUnderflow.BindUObject(this, &UOnsetVoipSoundWave::OnGenerateProceduralAudio);

		UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("UOnsetVoipSoundWave::PostInitProperties() with SampleRate of %i and NumChannels of %i and a SampleByteSize of %i"), SampleRate, NumChannels, SampleByteSize);
	}
}

void UOnsetVoipSoundWave::OnGenerateProceduralAudio(USoundWaveProcedural* InSoundWaveProcedural, int32 InSamplesToGenerate)
{
	FScopeLock ScopeLock(&AudioQueueLock);

	if (FrameByteSize <= 0)
	{
		return;
	}

	if (VoiceAudioBuffer.Num() == 0)
	{
		bPlaybackPrimed = false;
		TimeFirstAudioBuffered = 0.0;
		return;
	}

	if (MaxBufferToPlay > 0 && VoiceAudioBuffer.Num() > MaxBufferToPlay)
	{
		const int32 BytesToDrop = OnsetVoip::AlignToFrameBoundary(VoiceAudioBuffer.Num() - MaxBufferToPlay, FrameByteSize);
		if (BytesToDrop > 0)
		{
			UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("Dropping %i bytes of buffered voice audio to keep playback latency low."), BytesToDrop);
			VoiceAudioBuffer.RemoveAt(0, BytesToDrop, EAllowShrinking::No);
		}
	}

	const double CurrentTime = FPlatformTime::Seconds();
	const bool bStartWaitExpired =
		TimeFirstAudioBuffered > 0.0 &&
		(CurrentTime - TimeFirstAudioBuffered) >= InitialPlaybackBufferDelay;

	if (!bPlaybackPrimed && InitialBufferToStart > 0 && VoiceAudioBuffer.Num() < InitialBufferToStart && !bStartWaitExpired)
	{
		return;
	}

	bPlaybackPrimed = true;

	const int32 RequiredBytes = OnsetVoip::AlignToFrameBoundary(InSamplesToGenerate * FrameByteSize, FrameByteSize);
	int32 BytesToQueue = FMath::Max(RequiredBytes, InitialBufferToStart);
	BytesToQueue = FMath::Min(BytesToQueue, VoiceAudioBuffer.Num());
	BytesToQueue = OnsetVoip::AlignToFrameBoundary(BytesToQueue, FrameByteSize);

	if (BytesToQueue <= 0)
	{
		return;
	}

	UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("Queueing %i bytes of procedural audio data [InSamplesToGenerate %i] [ThreadId %d]"), BytesToQueue, InSamplesToGenerate, FPlatformTLS::GetCurrentThreadId());

	InSoundWaveProcedural->QueueAudio(VoiceAudioBuffer.GetData(), BytesToQueue);

	if (BytesToQueue == VoiceAudioBuffer.Num())
	{
		VoiceAudioBuffer.Reset();
		TimeFirstAudioBuffered = 0.0;
	}
	else
	{
		VoiceAudioBuffer.RemoveAt(0, BytesToQueue, EAllowShrinking::No);
	}
}

void UOnsetVoipSoundWave::AddAudioData(const uint8* InAudioData, const int32 InAudioDataSize)
{
	UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("Adding %i bytes of raw audio data to %s."), InAudioDataSize, *Describe());

	FScopeLock ScopeLock(&AudioQueueLock);
	if (VoiceAudioBuffer.Num() == 0)
	{
		TimeFirstAudioBuffered = FPlatformTime::Seconds();
	}
	const int32 OldBufferSize = VoiceAudioBuffer.AddUninitialized(InAudioDataSize);
	FMemory::Memcpy(VoiceAudioBuffer.GetData() + OldBufferSize, InAudioData, InAudioDataSize);
}

void UOnsetVoipSoundWave::AddAudioData(const TArray<uint8>& InAudioData)
{
	AddAudioData(InAudioData.GetData(), InAudioData.Num());
}

void UOnsetVoipSoundWave::AddAudioData(const TArrayView<uint8>& InAudioData)
{
	AddAudioData(InAudioData.GetData(), InAudioData.Num());
}

int32 UOnsetVoipSoundWave::GetAudioBufferSize() const
{
	FScopeLock ScopeLock(&AudioQueueLock);
	return VoiceAudioBuffer.Num();
}

int32 UOnsetVoipSoundWave::GetTotalPendingAudioByteCount() const
{
	return const_cast<UOnsetVoipSoundWave*>(this)->GetAvailableAudioByteCount() + GetAudioBufferSize();
}

void UOnsetVoipSoundWave::ResetAudioBuffer()
{
	FScopeLock ScopeLock(&AudioQueueLock);
	VoiceAudioBuffer.Empty();
	TimeFirstAudioBuffered = 0.0;
	bPlaybackPrimed = false;
}

USoundAttenuation* UOnsetVoipSoundWave::GetAttenuation() const
{
	return AttenuationSettings;
}

void UOnsetVoipSoundWave::SetAttenuation(USoundAttenuation* Attenuation)
{
	AttenuationSettings = Attenuation;
}

FString UOnsetVoipSoundWave::Describe() const
{
	return FString::Printf(TEXT("[%s AttenuationSettings: %s, SourceEffectChain: %s]"),
		*GetName(), AttenuationSettings ? *AttenuationSettings->GetPathName() : TEXT(""), SourceEffectChain ? *SourceEffectChain->GetPathName() : TEXT(""));
}
