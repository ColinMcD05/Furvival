// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#if ONSETVOIP_ENABLE_PIXELSTREAMING
#include "VoiceModule.h"
#include "Containers/Ticker.h"
#include "IPixelStreamingModule.h"
#include "IPixelStreamingStreamer.h"
#include "PixelStreamingAudioComponent.h"
#include "IPixelStreamingAudioConsumer.h"
#include "SampleBuffer.h"

class FOnsetPixelStreamingVoiceCapture : public IVoiceCapture, public IPixelStreamingAudioConsumer, public FTSTickerObjectBase
{
protected:
	void AddAudio(const Audio::TSampleBuffer<int16>& InBuffer);
	void AddAudio(TArrayView<const float> InBuffer);
	void AddAudio(TArray<uint8>&& InBuffer);

public:
	//~ Begin IPixelStreamingAudioConsumer interface
	virtual void ConsumeRawPCM(const int16_t* AudioData, int InSampleRate, size_t NChannels, size_t NFrames) override;
	virtual void OnConsumerAdded() override;
	virtual void OnConsumerRemoved() override;
	//~ End IPixelStreamingAudioConsumer interface

	//~ Begin FTSTickerObjectBase Interface
	virtual bool Tick(float DeltaTime) override;
	//~ End FTSTickerObjectBase Interface

	//~ Begin IVoiceCapture Interface
	virtual bool Init(const FString& DeviceName, int32 SampleRate, int32 NumChannels) override;
	virtual void Shutdown() override;
	virtual bool Start() override;
	virtual void Stop() override;
	virtual bool ChangeDevice(const FString& DeviceName, int32 SampleRate, int32 NumChannels) override;
	virtual bool IsCapturing() override;
	virtual EVoiceCaptureState::Type GetCaptureState(uint32& OutAvailableVoiceData) const override;
	virtual EVoiceCaptureState::Type GetVoiceData(uint8* OutVoiceBuffer, uint32 InVoiceBufferSize, uint32& OutAvailableVoiceData) override;
	virtual EVoiceCaptureState::Type GetVoiceData(uint8* OutVoiceBuffer, uint32 InVoiceBufferSize, uint32& OutAvailableVoiceData, uint64& OutSampleCounter) override;
	virtual int32 GetBufferSize() const;
	virtual void DumpState() const override;
	virtual float GetCurrentAmplitude() const;
	//~ End IVoiceCapture Interface

private:
	void OnPixelStreamingModuleReady(IPixelStreamingModule& Module);

	bool StreamerListenTo(FString StreamerId, FString PlayerToListenTo);

	bool WillListenToAnyPlayer() const
	{
		return PlayerToHear.IsEmpty();
	}

	bool IsListeningToPlayer() const
	{
		return AudioSink != nullptr;
	}

	bool bLookForConnection = false;
	FThreadSafeBool bCaptureAudio = false;

	FString StreamerToHear;
	FString PlayerToHear;
	IPixelStreamingAudioSink* AudioSink = nullptr;

	int32 TargetSampleRate = 0;
	int32 TargetNumChannels = 0;

	TArray<uint8> AudioBuffer;
	mutable FCriticalSection AudioBufferLock;
};

#endif /* ONSETVOIP_ENABLE_PIXELSTREAMING */
