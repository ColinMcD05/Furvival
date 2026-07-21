// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "OnsetVoipSoundWave.generated.h"

/**
 * A procedural sound wave which is used to play voice data for a UOnsetVoipAudioComponent.
 */
UCLASS(Blueprintable)
class ONSETVOIP_API UOnsetVoipSoundWave : public USoundWaveProcedural
{
	GENERATED_BODY()
	
public:
	UOnsetVoipSoundWave(const FObjectInitializer& ObjectInitializer);

	//~ Begin UObject Interface 
	virtual void PostInitProperties() override;
	//~ End UObject Interface 

protected:
	// Called by the sound wave when it needs more audio samples. Drains VoiceAudioBuffer.
	void OnGenerateProceduralAudio(USoundWaveProcedural* InSoundWaveProcedural, int32 InSamplesToGenerate);

public:
	// Adds new audio data to this procedural audio wave.
	virtual void AddAudioData(const uint8* InAudioData, const int32 InAudioDataSize);
	virtual void AddAudioData(const TArrayView<uint8>& InAudioData);

	// Blueprint version for adding new audio data to this procedural audio wave.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip")
	virtual void AddAudioData(const TArray<uint8>& InAudioData);

	// Current size of the audio buffer pending to be queued.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip")
	virtual int32 GetAudioBufferSize() const;

	// Total number of bytes pending for playback, including procedural queued bytes and jitter-buffered bytes.
	virtual int32 GetTotalPendingAudioByteCount() const;

	// Resets audio data that is queued to be sent to the procedural sound wave.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip")
	virtual void ResetAudioBuffer();

	// Get sound attenuation.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip")
	virtual USoundAttenuation* GetAttenuation() const;

	// Set sound attenuation.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip")
	virtual void SetAttenuation(USoundAttenuation* Attenuation);

	virtual FString Describe() const;

protected:
	// Mutex for accessing VoiceAudioBuffer in OnGenerateProceduralAudio from audio threads.
	mutable FCriticalSection AudioQueueLock;
	TArray<uint8> VoiceAudioBuffer;
	// Cached frame byte size (SampleByteSize * NumChannels), set once in constructor.
	int32 FrameByteSize = 0;

	double TimeFirstAudioBuffered = 0.0;
	double InitialPlaybackBufferDelay;
	double MaxBufferPlaybackDelay;
	int32 InitialBufferToStart;
	int32 MaxBufferToPlay;
	bool bPlaybackPrimed = false;
};
