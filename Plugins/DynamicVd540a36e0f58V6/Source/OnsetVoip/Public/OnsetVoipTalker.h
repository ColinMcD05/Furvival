// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnsetVoipSoundWave.h"
#include "OnsetVoipAudioComponent.h"
#include "OnsetVoipRelevancyDefinitions.h"
#if ONSETVOIP_ENABLE_VOICE_PROCESSING
#include "VoiceModule.h"
#include "OnsetVoipPacket.h"
#endif
#include "OnsetNameplateComponent.h"
#include "OnsetVoipTalker.generated.h"

/**
 * A UOnsetVoipTalker is automatically created and destroyed for each APlayerState.
 * It handles the voice for that specific player.
 * There might exist a UOnsetVoipTalker for your local player but UOnsetVoipTalker will only ever be remote players.
 */
UCLASS(Blueprintable)
class ONSETVOIP_API UOnsetVoipTalker : public UObject
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnVoipTalkingStateChange, UOnsetVoipTalker*, VoipTalker, bool, bIsTalking, EOnsetVoipNetRelevancy, Relevancy);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnVoipAudioDataReceived, UOnsetVoipTalker*, VoipTalker, const TArray<uint8>&, AudioData, EOnsetVoipNetRelevancy, Relevancy);
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FNativeOnVoipAudioDataReceived, UOnsetVoipTalker*/* VoipTalker*/, const TArrayView<uint8>&/* AudioData*/, EOnsetVoipNetRelevancy /*Relevancy*/);

	// Returns true if currently speaking.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared")
	virtual bool IsTalking() const;

	// Returns the relevancy state of the last processed/received voip packet.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared")
	virtual EOnsetVoipNetRelevancy GetLastVoipRelevancy() const;

	// Returns the amplitude like UOnsetVoipLocalPlayerSubsystem::GetCurrentAmplitude().
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared")
	virtual float GetCurrentAmplitude() const;

	// Gets the owning player state of this talker.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared")
	virtual APlayerState* GetPlayerState() const;

	// Does the same as GetPlayerState()->GetPawn().
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared")
	virtual APawn* GetPlayerPawn() const;

	// Called when this voip talker starts or stops talking.
	UPROPERTY(BlueprintAssignable, Category = "OnsetVoip|Shared")
	FOnVoipTalkingStateChange OnVoipTalkingStateChange;

	/*
	* Called when this voip talker received new audio data. Format is raw signed 16-bit PCM.
	* Number of channels and sample rate are defined in UOnsetVoipSettings::VoiceCaptureChannels and VoiceCaptureSampleRate.
	* To cancel the audio from being played use CancelReceivedAudioData() inside this delegate.
	*/
	UPROPERTY(BlueprintAssignable, Category = "OnsetVoip|Shared")
	FOnVoipAudioDataReceived OnVoipAudioDataReceived;

	// Same as above but optimized without a data copy using TArrayView for C++. To cancel the audio from being played use CancelReceivedAudioData() inside this delegate.
	FNativeOnVoipAudioDataReceived Native_OnVoipAudioDataReceived;

	// Client/listen server only: Call this function inside any OnVoipAudioDataReceived delegate to stop the audio from being played by the plugin.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Shared")
	virtual void CancelReceivedAudioData();

	// Mute this talker audio. Must be called on the client of the player who wants to mute this talker.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Client")
	virtual void SetMuted(bool bMute);
	
	// Returns true if this talker is muted.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Client")
	virtual bool IsMuted() const;

	// Server only: Same as SetMutedForVoipTalker but with a player state.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Server")
	virtual void SetMutedForPlayerState(bool bMute, APlayerState* PlayerState);

	// Server only: You can mute this talker for another voip talker.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Server")
	virtual void SetMutedForVoipTalker(bool bMute, UOnsetVoipTalker* OtherVoipTalker);

	// Server only: Returns true if this talker is muted for another talker.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Server")
	virtual bool IsMutedForVoipTalker(UOnsetVoipTalker* OtherVoipTalker) const;

	// Server only: Same as IsMutedForVoipTalker but with a player state.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Server")
	virtual bool IsMutedForPlayerState(APlayerState* PlayerState) const;
	
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UOnsetVoipTalker>> MutedVoipTalkers;

	/*
	* This function can enable or disable the operation of a voip talker.
	* Calling this on a **server** will stop this talker from sending voice data to anyone.
	* Calling this on a **client** will stop this talker from playing/processing any of their audio data.
	*
	* This function is not the same as SetMuted. You probably just want to use SetMuted for muting audio. Especially on a listen server.
	*/
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Shared")
	virtual void SetVoipEnabled(bool bEnable);
	
	// Returns true if talker is enabled. See SetVoipEnabled for more information.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared")
	virtual bool IsVoipEnabled() const;

	/*
	* **SERVER ONLY, do not call on client**
	* 
	* This function adds a player/talker to a voice channel or removes them. Voice channels are a server-side concept, meaning only the server knows which channels a talker is part of.
	* 
	* ChannelId is an integer used to determine which players share the same channel. This can be any positive integer you like and is used purely to manage the channel IDs.
	* If two talkers are in the same channel they are able to hear each other as a 2D voice.
	* ChannelId 0 is the world/3D/proximity channel. All talkers are automatically in channel 0. If you remove someone from channel 0, they will no longer participate in world/3D/proximity voice.
	* 
	* Set bAdd to false to remove them from a channel. No further cleanup is required.
	*/
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Server")
	virtual bool SetVoiceChannel(int32 ChannelId, bool bAdd);

	/*
	* **SERVER ONLY, do not call on client**
	* 
	* Tests whether a player is in a specific voice channel. This only works when called on the server. Voice channel 0 is the world/3D channel.
	* See SetVoiceChannel for more information.
	*/
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Server")
	virtual bool IsInVoiceChannel(int32 ChannelId) const;

	// Server only: Voice channel ids of this talker. Only used on the server not on the client. Read only, use SetVoiceChannel() to add/remove voice channels.
	UPROPERTY(BlueprintReadOnly, Category = "OnsetVoip|Server")
	TArray<int32> VoiceChannelIds;

	// Server only: Mute this talker on a specific voice channel id. This only works when called on the server.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Server")
	virtual void SetMutedOnVoiceChannel(bool bMute, int32 ChannelId);
	
	// Server only: Check if this talker is muted on a voice channel id. This only works when called on the server.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Server")
	virtual bool IsMutedOnVoiceChannel(int32 ChannelId) const;

	// Server only: Voice channel IDs that this talker is muted on and cannot speak on, but can still listen on. Only used on the server, not on the client. Read only; use SetMutedOnVoiceChannel().
	UPROPERTY(BlueprintReadOnly, Category = "OnsetVoip|Server")
	TArray<int32> MutedVoiceChannelIds;

	/* 
	* Voice channels > 0 will be played in 2D and have a higher priority than proximity (3D).
	* So if two players share the same 2D voice channel it will always be played in 2D, even if the other player is nearby.
	* Some people have requested to be able to hear voice in 2D and 3D at the same time. For that purpose, enable this setting.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OnsetVoip|Shared")
	bool bEnable2DVoicePlaybackOnBoth2DAnd3D = false;

	/*
	* Voice channels > 0 will be played in 2D and have a higher priority than proximity (3D).
	* So if two players share the same 2D voice channel it will always be played in 2D, even if the other player is nearby.
	* Some people have requested the ability to hear 2D voice in 3D if the other player is nearby. For example, when players are in a 2D channel for a walkie-talkie.
	* Enabling this setting will play voice in proximity (3D) if the player is within PlaybackDistance2DVoicePlaybackOn3DIfNear; otherwise, it will play in 2D.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OnsetVoip|Shared")
	bool bEnablePrefer2DVoicePlaybackOn3DIfNear = false;

	/*
	* Distance threshold for playing 2D voice in proximity if the player is nearby. See bEnablePrefer2DVoicePlaybackOn3DIfNear.
	* This setting should roughly represent the attenuation distance of your 3D voice audio component.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OnsetVoip|Shared", meta = (EditCondition = "bEnablePrefer2DVoicePlaybackOn3DIfNear", EditConditionHides))
	double PlaybackDistance2DVoicePlaybackOn3DIfNear = 2500.0;

	/*
	* Gets the audio component used for talking, either 2D or 3D. If this talker has not received any voice data yet, a component will be created and saved internally.
	* If you want to modify the audio component or sound wave to make some initial settings, you should instead:
	* - subclass UOnsetVoipAudioComponent and override the default class in UOnsetVoipTalker or UOnsetVoipSettings. (DefaultOnsetVoipAudioComponentClass2D / DefaultOnsetVoipAudioComponentClass3D)
	* - or use IOnsetVoipPawnInterface::OnVoipAudioComponentCreated to get notified when a voice audio component is created.
	*/
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Client")
	virtual UOnsetVoipAudioComponent* GetVoiceAudioComponent(EOnsetVoipNetRelevancy Relevancy);

	// Non-creating accessors used for debug and diagnostics.
	virtual UOnsetVoipAudioComponent* GetCachedAudioComponent2D() const { return AudioComponent2D; }
	virtual UOnsetVoipAudioComponent* GetCachedAudioComponent3D() const { return CachedAudioComponent3D; }

	// Used to override the audio component class for 2D voice. This will override the same setting in UOnsetVoipSettings for this UOnsetVoipTalker.
	UPROPERTY(EditDefaultsOnly, Category = "OnsetVoip|Client")
	TSubclassOf<UOnsetVoipAudioComponent2D> DefaultOnsetVoipAudioComponentClass2D = UOnsetVoipAudioComponent2D::StaticClass();
	
	// Used to override the audio component class for 3D voice. This will override the same setting in UOnsetVoipSettings for this UOnsetVoipTalker.
	UPROPERTY(EditDefaultsOnly, Category = "OnsetVoip|Client")
	TSubclassOf<UOnsetVoipAudioComponent> DefaultOnsetVoipAudioComponentClass3D = UOnsetVoipAudioComponent::StaticClass();

	// Called by the engine when the player state pawn changes.
	UFUNCTION()
	void OnPlayerStatePawnSet(APlayerState* ChangedPlayerState, APawn* NewPlayerPawn, APawn* OldPlayerPawn);

	// Server only: Can be overridden to do custom checks for whether this talker should send voice to OtherVoipTalker.
	virtual bool ShouldSendVoiceTo(const UOnsetVoipTalker* OtherVoipTalker) const;

	
#if ONSETVOIP_ENABLE_VOICE_PROCESSING // See OnsetVoip.Build.cs for this definition.

	// Creates a voice decoder on demand.
	virtual TSharedPtr<IVoiceDecoder> GetOrCreateVoiceDecoder();

	// Called to process and decompress new audio data and update the talking state.
	virtual void ProcessVoiceData(const uint8* InAudioData, const int32 InAudioDataSize, EOnsetVoipNetRelevancy Relevancy, float InCurrentAmplitude, class UOnsetVoipWorldSubsystem* OnsetVoipWorldSubsystem);
	virtual void ProcessVoiceData(const FOnsetVoicePacketWrapper& PacketWrapper, class UOnsetVoipWorldSubsystem* OnsetVoipWorldSubsystem);

	// Chance to override relevancy before playback.
	virtual void ProcessOverrideVoiceRelevancy(EOnsetVoipNetRelevancy& Relevancy);

	// Called by the voip world subsystem to update talking state.
	virtual void UpdateTalkingState(const double& CurrentTime);

protected:
	// Used internally to set cached talking state and fire delegates.
	virtual void InternalSetTalkingState(bool bInNewTalking);

#endif

	// See SetVoipEnabled.
	bool bVoipEnabled = true;

	// See SetMuted.
	bool bMuted = false;

	// Cached value indicating whether the talker can speak in the 3D world. (Voice channel 0)
	bool bVoiceWorldEnabled = true;

	// Tracks whether this talker currently speaks.
	bool bCachedTalking = false;

	// Time when the last voip data was received for this talker.
	double LastReceivedVoipData = 0.0;

	// Whether the last received VoIP packet was in 2D space from voice channels.
	EOnsetVoipNetRelevancy LastReceivedRelevancy = EOnsetVoipNetRelevancy::RELEVANT_NONE;

	// See CancelReceivedAudioData()
	bool bCancelReceivedAudioData = false;

	// See GetCurrentAmplitude()
	float CachedAmplitude = 0.0f;

	// Gets or creates the audio component used to play voice data when receiving audio.
	virtual UOnsetVoipAudioComponent* InternalGetOrCreateAudioComponent(EOnsetVoipNetRelevancy Relevancy);

	// Reference to the nameplate component.
	// Attached and owned by the pawn, therefore the "Cached" prefix.
	UPROPERTY(Transient)
	TObjectPtr<UOnsetNameplateComponent> CachedNameplateComponent = nullptr;

	// Reference to the audio component of this player's pawn. Used for 3D playback, created and attached to the pawn on demand.
	// Attached and owned by the pawn, therefore the "Cached" prefix.
	UPROPERTY(Transient)
	TObjectPtr<UOnsetVoipAudioComponent> CachedAudioComponent3D = nullptr;

	// Reference to the audio component used for 2D playback in voice channels.
	// Managed and owned by this VoIP talker object.
	UPROPERTY(Transient)
	TObjectPtr<UOnsetVoipAudioComponent> AudioComponent2D = nullptr;

#if ONSETVOIP_ENABLE_VOICE_PROCESSING
private:
	TSharedPtr<IVoiceDecoder> VoiceDecoder;
	int32 FrameByteSize = 0;
#endif
};
