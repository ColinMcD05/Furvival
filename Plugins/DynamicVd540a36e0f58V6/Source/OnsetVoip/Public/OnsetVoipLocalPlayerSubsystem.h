// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#if !UE_SERVER
#include "VoiceModule.h"
#include "Containers/Ticker.h"
#include "Recording/OnsetVoipRecorder.h"
#endif
#include "OnsetVoipTalker.h"
#include "OnsetVoipLocalPlayerSubsystem.generated.h"

namespace OnsetVoip
{
	// Time after the last detected voice from microphone.
	constexpr double TimeLocalTalkerConsideredStoppedTalking = 0.3;

	// The minimum and maximum values for cvar voice.MicInputGain2
	constexpr float MinMicInputGain = 0.0f;
	constexpr float MaxMicInputGain = 10.0f;

	// The minimum and maximum values for cvar voice.MicInputAGC
	constexpr float MinMicInputAGC = 100.0f;
	constexpr float MaxMicInputAGC = 10000.0f;
}

/**
 * Subsystem based on a local player. Handles our own local voice capturing and processing.
 */
UCLASS()
class ONSETVOIP_API UOnsetVoipLocalPlayerSubsystem
	: public ULocalPlayerSubsystem
#if !UE_SERVER
	, public FTSTickerObjectBase
#endif
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoipTalkingStateChange, bool, bIsTalking);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnVoipMicrophoneAudioCaptured, const TArrayView<uint8>&/* AudioData*/);

	//~ Begin ULocalPlayerSubsystem Interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
#if !UE_SERVER
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
#endif
	//~ End ULocalPlayerSubsystem Interface
	
	// Change the input device used for capturing our voice.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Client")
	bool ChangeVoiceCaptureDevice(const FString& DeviceName);

	// Starts or stops voice capture on our microphone. This will make us speak on all channels we are part of on the server.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Client")
	bool ToggleVoiceCapture(bool bCapture);

	/*
	* Same as ToggleVoiceCapture but takes a voice channel id which hints the server that we would like to talk on a specific channel.
	* The talker/player must be set in the channel with UOnsetVoipTalker::SetVoiceChannel on the server first.
	* Each call to ToggleVoiceCapture* will reset the capture channels set in a previous call.
	*/
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Client")
	bool ToggleVoiceCaptureWithChannel(bool bCapture, int32 VoiceChannelId);

	// Please refer to ToggleVoiceCaptureWithChannel for an explanation. This function takes multiple voice channel ids.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Client")
	bool ToggleVoiceCaptureWithChannels(bool bCapture, const TArray<int32>& VoiceChannelIds);

	// Returns true if our voice capture is enabled. To check if we actually talk use IsTalking().
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Client")
	bool IsCapturingVoice() const;

	// Returns true if our voice capture is enabled and the microphone detected voice recently.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Client")
	bool IsTalking() const;

	// The current loudness of the microphone between 0.0 and 1.0, returns -1.0 if this function is not supported by the current platform. Currently this is only supported on Windows. Not affected by cvar voice.MicInputGain2.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Client")
	float GetCurrentAmplitude() const;

	// Called when we start or stop talking.
	UPROPERTY(BlueprintAssignable, Category = "OnsetVoip|Client")
	FOnVoipTalkingStateChange OnVoipTalkingStateChange;

	// Called when audio was captured from the microphone.
	FOnVoipMicrophoneAudioCaptured Native_OnVoipMicrophoneAudioCaptured;

	// Plays our own microphone audio. Voice capture must be enabled (ToggleVoiceCapture).
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Client")
	void ToggleLoopback(bool bLoopback);
	
	// See ToggleLoopback()
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Client")
	bool IsLoopbackEnabled() const;

#if !UE_SERVER
	//~ Begin FTSTickerObjectBase Interface
	virtual bool Tick(float DeltaTime) override;
	//~ End FTSTickerObjectBase Interface
#endif

private:
#if !UE_SERVER
	TSharedPtr<IVoiceCapture> VoiceCapture;
	TSharedPtr<IVoiceEncoder> VoiceEncoder;
	
	TArray<uint8> VoiceRemainder;
	TArray<uint8> CompressedVoiceData;

	static TSharedPtr<IVoiceCapture> CreateVoiceCaptureInterface(const FString& DeviceName, int32 SampleRate, int32 NumChannels);

	bool TryInitializeVoiceCapture();
	bool InitializeVoiceEncoder();

	static bool IsVoiceCapturePermissionGranted();
#endif

	// Voice channel ids we want to speak on, will be sent to the server.
	TArray<int32> HintVoiceChannels;

	// Tracks whether we are currently talking.
	bool bCachedTalking = false;

	// Time when the last voice data was captured from microphone.
	double TimeLastVoiceCaptured = 0.0;

	UPROPERTY()
	TObjectPtr<UAudioComponent> LoopbackAudioComponent;

	UPROPERTY()
	TObjectPtr<UOnsetVoipSoundWave> LoopbackSoundWave;

#if !UE_SERVER
	void OnMicrophoneAudioCapturedForLoopback(const TArrayView<uint8>& AudioData);
#endif

public:
	/* Start recording the local microphone.
	 * Filename: If empty, a file will be generated.
	 * Returns false if recording is already running or failed to start.
	 */
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Client")
	bool StartRecording(const FString& Filename, EOnsetAudioRecordingFormat RecordingFormat);

	// Stop recording. Returns false if not recording.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Client")
	bool StopRecording();

	// Returns true if currently recording.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Client")
	bool IsRecording() const;

	// Returns the directory + filename being recorded to.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Client")
	FString GetRecordingFile() const;

#if !UE_SERVER
private:
	TSharedPtr<IOnsetVoipRecorder> VoipRecorder;

	enum class EVoiceCaptureStateAsync
	{
		None,
		WantsToStart
	};

	EVoiceCaptureStateAsync VoiceCaptureAsyncState = EVoiceCaptureStateAsync::None;

	int32 FrameByteSize = 0;

public:
	void PrintDebugInformation() const;
	static void PrintVoiceRelatedCVarsToLog();
#endif
};
