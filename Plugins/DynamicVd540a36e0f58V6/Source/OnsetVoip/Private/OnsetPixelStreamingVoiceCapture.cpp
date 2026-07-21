// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetPixelStreamingVoiceCapture.h"
#if ONSETVOIP_ENABLE_PIXELSTREAMING
#include "OnsetVoip.h"
#include "AudioResampler.h"

void FOnsetPixelStreamingVoiceCapture::AddAudio(const Audio::TSampleBuffer<int16>& InBuffer)
{
	const int32 NumInBuffer = InBuffer.GetNumSamples();

	FScopeLock Lock(&AudioBufferLock);
	const int32 NumElementsBefore = AudioBuffer.AddUninitialized(NumInBuffer * sizeof(int16));
	FMemory::Memmove(AudioBuffer.GetData() + NumElementsBefore, InBuffer.GetData(), NumInBuffer * sizeof(int16));
}

void FOnsetPixelStreamingVoiceCapture::AddAudio(TArrayView<const float> InBuffer)
{
	TArray<uint8> Pcm16;
	Pcm16.AddUninitialized(InBuffer.Num() * sizeof(int16));
	Audio::ArrayFloatToPcm16(InBuffer, MakeArrayView((int16*)Pcm16.GetData(), Pcm16.Num() / sizeof(int16)));

	AddAudio(MoveTemp(Pcm16));
}

void FOnsetPixelStreamingVoiceCapture::AddAudio(TArray<uint8>&& InBuffer)
{
	FScopeLock Lock(&AudioBufferLock);
	AudioBuffer.Append(MoveTemp(InBuffer));
}

void FOnsetPixelStreamingVoiceCapture::ConsumeRawPCM(const int16_t* AudioData, int InSampleRate, size_t NChannels, size_t NFrames)
{
	//UE_LOG(LogOnsetVoip, Log, TEXT("ConsumeRawPCM(%i, %i, %i) TID %i"), InSampleRate, NChannels, NFrames, FPlatformTLS::GetCurrentThreadId());

	if (!bCaptureAudio)
	{
		//UE_LOG(LogOnsetVoip, Warning, TEXT("Dropping PS audio as bCaptureAudio == false"));
		return;
	}

	const int32 NSamples = NFrames * NChannels;
	
	if (TargetSampleRate != InSampleRate)
	{
		// Convert samples to float as required by the resampling library.
		Audio::FAlignedFloatBuffer AudioDataFloat;
		AudioDataFloat.AddUninitialized(NSamples);
		Audio::ArrayPcm16ToFloat(MakeArrayView(AudioData, NSamples), AudioDataFloat);

		const Audio::FResamplingParameters ResamplerParams = {
			Audio::EResamplingMethod::Linear,
			NChannels,
			InSampleRate,
			TargetSampleRate,
			AudioDataFloat
		};

		Audio::FAlignedFloatBuffer ResamplerOutputData;
		ResamplerOutputData.AddUninitialized(Audio::GetOutputBufferSize(ResamplerParams));

		Audio::FResamplerResults ResamplerResults;
		ResamplerResults.OutBuffer = &ResamplerOutputData;

		if (!Audio::Resample(ResamplerParams, ResamplerResults))
		{
			UE_LOG(LogOnsetVoip, Error, TEXT("Failed to resample from %i to %i. AudioDataFloat: %i, ResamplerOutputData: %i"), InSampleRate, TargetSampleRate, AudioDataFloat.Num(), ResamplerOutputData.Num());
			return;
		}

		const int32 NumSamplesGenerated = ResamplerResults.OutputFramesGenerated * NChannels;
		
		//UE_LOG(LogOnsetVoip, Log, TEXT("Resampled from %i to %i. New sample count: %i, old sample count: %i"), InSampleRate, TargetSampleRate, NumSamplesGenerated, NSamples);

		if (TargetNumChannels != NChannels)
		{
			Audio::TSampleBuffer<float> Buffer(ResamplerOutputData.GetData(), NumSamplesGenerated, NChannels, TargetSampleRate);
			Buffer.MixBufferToChannels(TargetNumChannels);

			AddAudio(Buffer.GetArrayView());
		}
		else
		{
			AddAudio(MakeArrayView(ResamplerOutputData.GetData(), NumSamplesGenerated));
		}
	}
	else
	{
		Audio::TSampleBuffer<int16> Buffer(AudioData, NSamples, NChannels, TargetSampleRate);

		if (TargetNumChannels != NChannels)
		{
			Buffer.MixBufferToChannels(TargetNumChannels);
		}

		AddAudio(Buffer);
	}
}

void FOnsetPixelStreamingVoiceCapture::OnConsumerAdded()
{
	UE_LOG(LogOnsetVoip, Verbose, TEXT("FOnsetPixelStreamingVoiceCapture::OnConsumerAdded()"));
}

void FOnsetPixelStreamingVoiceCapture::OnConsumerRemoved()
{
	UE_LOG(LogOnsetVoip, Verbose, TEXT("FOnsetPixelStreamingVoiceCapture::OnConsumerRemoved()"));

	bCaptureAudio = false;
	bLookForConnection = true;
	AudioSink = nullptr;
}

bool FOnsetPixelStreamingVoiceCapture::Tick(float DeltaTime)
{
	if (IsRunningCommandlet())
	{
		return true;
	}

	if (IsListeningToPlayer())
	{
		return true;
	}

	if (!bLookForConnection)
	{
		return true;
	}

	if (StreamerListenTo(StreamerToHear, PlayerToHear))
	{
		UE_LOG(LogOnsetVoip, Log, TEXT("FOnsetPixelStreamingVoiceCapture found a WebRTC peer to listen to."));
	}

	return true;
}

bool FOnsetPixelStreamingVoiceCapture::Init(const FString& DeviceName, int32 SampleRate, int32 NumChannels)
{
	UE_LOG(LogOnsetVoip, Verbose, TEXT("FOnsetPixelStreamingVoiceCapture::Init(%s, %i, %i)"), *DeviceName, SampleRate, NumChannels);

	if (!IPixelStreamingModule::IsAvailable())
	{
		UE_LOG(LogOnsetVoip, Error, TEXT("PixelStreaming voice capture failed as PixelStreaming plugin is not available/enabled."));
		return false;
	}

	TargetSampleRate = SampleRate;
	TargetNumChannels = NumChannels;

	IPixelStreamingModule& PixelStreamingModule = IPixelStreamingModule::Get();
	if (PixelStreamingModule.IsReady())
	{
		UE_LOG(LogOnsetVoip, Verbose, TEXT("PixelStreaming is ready. Will now look for audio connection."));

		for (const auto& Streamer : PixelStreamingModule.GetStreamerIds())
		{
			UE_LOG(LogOnsetVoip, Verbose, TEXT("Streamer: %s"), *Streamer);
		}

		bLookForConnection = true;
	}
	else
	{
		UE_LOG(LogOnsetVoip, Verbose, TEXT("PixelStreaming is not yet ready. Postponing until module is ready."));

		PixelStreamingModule.OnReady().AddSP(this, &FOnsetPixelStreamingVoiceCapture::OnPixelStreamingModuleReady);
	}

	return true;
}

void FOnsetPixelStreamingVoiceCapture::Shutdown()
{
	if (AudioSink)
	{
		AudioSink->RemoveAudioConsumer(this);
	}
	AudioSink = nullptr;

	bCaptureAudio = false;
	bLookForConnection = false;
	PlayerToHear.Empty();
	StreamerToHear.Empty();
}

bool FOnsetPixelStreamingVoiceCapture::Start()
{
	bCaptureAudio = true;
	return AudioSink != nullptr;
}

void FOnsetPixelStreamingVoiceCapture::Stop()
{
	bCaptureAudio = false;

	FScopeLock Lock(&AudioBufferLock);
	AudioBuffer.Empty();
}

bool FOnsetPixelStreamingVoiceCapture::ChangeDevice(const FString& DeviceName, int32 SampleRate, int32 NumChannels)
{
	ensureMsgf(false, TEXT("ChangeDevice called but no input devices to change while running PixelStreaming."));
	return false;
}

bool FOnsetPixelStreamingVoiceCapture::IsCapturing()
{
	return bCaptureAudio;
}

EVoiceCaptureState::Type FOnsetPixelStreamingVoiceCapture::GetCaptureState(uint32& OutAvailableVoiceData) const
{
	OutAvailableVoiceData = 0u;

	if (AudioSink == nullptr)
	{
		return EVoiceCaptureState::UnInitialized;
	}

	if (!bCaptureAudio)
	{
		return EVoiceCaptureState::NotCapturing;
	}

	{
		FScopeLock Lock(&AudioBufferLock);
		OutAvailableVoiceData = static_cast<uint32>(AudioBuffer.Num());
	}

	return OutAvailableVoiceData > 0 ? EVoiceCaptureState::Ok : EVoiceCaptureState::NoData;
}

EVoiceCaptureState::Type FOnsetPixelStreamingVoiceCapture::GetVoiceData(uint8* OutVoiceBuffer, uint32 InVoiceBufferSize, uint32& OutAvailableVoiceData)
{
	if (InVoiceBufferSize == 0)
	{
		return EVoiceCaptureState::BufferTooSmall;
	}

	{
		FScopeLock Lock(&AudioBufferLock);

		const uint32 BufferSize = static_cast<uint32>(AudioBuffer.Num());

		if (BufferSize == 0)
		{
			return EVoiceCaptureState::NoData;
		}

		const uint32 NumToCopy = FMath::Min(BufferSize, InVoiceBufferSize);
		FMemory::Memmove(OutVoiceBuffer, AudioBuffer.GetData(), NumToCopy);

		if (InVoiceBufferSize >= BufferSize)
		{
			// If the out buffer is larger than AudioBuffer then everything was copied and it can be emptied
			AudioBuffer.Empty();
		}
		else
		{
			check(NumToCopy <= BufferSize);
			AudioBuffer.RemoveAt(0, NumToCopy, EAllowShrinking::No);
		}

		OutAvailableVoiceData = NumToCopy;
	}

	return EVoiceCaptureState::Ok;
}

EVoiceCaptureState::Type FOnsetPixelStreamingVoiceCapture::GetVoiceData(uint8* OutVoiceBuffer, uint32 InVoiceBufferSize, uint32& OutAvailableVoiceData, uint64& OutSampleCounter)
{
	OutSampleCounter = 0;
	return GetVoiceData(OutVoiceBuffer, InVoiceBufferSize, OutAvailableVoiceData);
}

int32 FOnsetPixelStreamingVoiceCapture::GetBufferSize() const
{
	FScopeLock Lock(&AudioBufferLock);
	return AudioBuffer.Num();
}

void FOnsetPixelStreamingVoiceCapture::DumpState() const
{
	UE_LOG(LogOnsetVoip, Display, TEXT("Onset PixelStreaming Voice Capture Details"));
	UE_LOG(LogOnsetVoip, Display, TEXT("- bCaptureAudio: %i"), (int)bCaptureAudio);
	UE_LOG(LogOnsetVoip, Display, TEXT("- bLookForConnection: %i"), bLookForConnection);
	UE_LOG(LogOnsetVoip, Display, TEXT("- PlayerToHear: %s"), *PlayerToHear);
	UE_LOG(LogOnsetVoip, Display, TEXT("- StreamerToHear: %s"), *StreamerToHear);
	UE_LOG(LogOnsetVoip, Display, TEXT("- AudioSink: %p"), AudioSink);
	UE_LOG(LogOnsetVoip, Display, TEXT("- TargetSampleRate: %i"), TargetSampleRate);
	UE_LOG(LogOnsetVoip, Display, TEXT("- TargetNumChannels: %i"), TargetNumChannels);
	UE_LOG(LogOnsetVoip, Display, TEXT("- AudioBufferSize: %i"), GetBufferSize());
}

float FOnsetPixelStreamingVoiceCapture::GetCurrentAmplitude() const
{
	return -1.0f;
}

void FOnsetPixelStreamingVoiceCapture::OnPixelStreamingModuleReady(IPixelStreamingModule& Module)
{
	UE_LOG(LogOnsetVoip, Verbose, TEXT("PixelStreaming is now ready. Will now look for audio connection."));

	for (const auto& Streamer : Module.GetStreamerIds())
	{
		UE_LOG(LogOnsetVoip, Verbose, TEXT("Streamer: %s"), *Streamer);
	}

	bLookForConnection = true;
}

bool FOnsetPixelStreamingVoiceCapture::StreamerListenTo(FString StreamerId, FString PlayerToListenTo)
{
	IPixelStreamingModule& PixelStreamingModule = IPixelStreamingModule::Get();
	checkf(PixelStreamingModule.IsReady(), TEXT("StreamerListenTo called but PixelStreaming not ready."));

	PlayerToHear = PlayerToListenTo;

	if (StreamerId.IsEmpty())
	{
		const TArray<FString> StreamerIds = PixelStreamingModule.GetStreamerIds();
		if (StreamerIds.Num() > 0)
		{
			StreamerToHear = StreamerIds[0];
		}
		else
		{
			StreamerToHear = PixelStreamingModule.GetDefaultStreamerID();
		}
	}
	else
	{
		StreamerToHear = StreamerId;
	}

	TSharedPtr<IPixelStreamingStreamer> Streamer = PixelStreamingModule.FindStreamer(StreamerToHear);
	if (!Streamer)
	{
		return false;
	}

	UE_LOG(LogOnsetVoip, Verbose, TEXT("Trying to listen to streamer %s"), *Streamer->GetId());

	for (const auto& Player : Streamer->GetConnectedPlayers())
	{
		UE_LOG(LogOnsetVoip, Verbose, TEXT(" Player: %s"), *Player);
	}

	AudioSink = WillListenToAnyPlayer() ? Streamer->GetUnlistenedAudioSink() : Streamer->GetPeerAudioSink(FPixelStreamingPlayerId(PlayerToHear));

	if (AudioSink == nullptr)
	{
		return false;
	}

	AudioSink->AddAudioConsumer(this);
	return true;
}

#endif /* ONSETVOIP_ENABLE_PIXELSTREAMING */
