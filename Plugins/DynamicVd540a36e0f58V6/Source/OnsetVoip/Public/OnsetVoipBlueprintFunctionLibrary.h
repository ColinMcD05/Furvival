// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/PlayerState.h"
#include "OnsetVoipTalker.h"
#include "OnsetVoipBlueprintFunctionLibrary.generated.h"

/**
 * This is a wrapper/helper around the most common plugin functions.
 * Users requested this to have a faster or more direct access to the functions.
 */
UCLASS(meta=(ScriptName="OnsetVoipLibrary"))
class ONSETVOIP_API UOnsetVoipBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// See UOnsetVoipTalker::SetVoiceChannel
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Server")
	static void SetTalkerVoiceChannel(UOnsetVoipTalker* VoipTalker, int32 ChannelId, bool bAdd);

	// See UOnsetVoipTalker::IsInVoiceChannel
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Server")
	static bool IsTalkerInVoiceChannel(UOnsetVoipTalker* VoipTalker, int32 ChannelId);

	// See UOnsetVoipWorldSubsystem::GetVoipTalker
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared", meta = (WorldContext = "WorldContextObject"))
	static UOnsetVoipTalker* GetTalkerByPlayerState(APlayerState* PlayerState, const UObject* WorldContextObject);

	// See UOnsetVoipWorldSubsystem::GetAllTalkers
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared", meta = (WorldContext = "WorldContextObject"))
	static TArray<UOnsetVoipTalker*> GetAllTalkers(const UObject* WorldContextObject);

	// See UOnsetVoipWorldSubsystem::GetAllTalkersInVoiceChannel
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Server", meta = (WorldContext = "WorldContextObject"))
	static TArray<UOnsetVoipTalker*> GetAllTalkersInVoiceChannel(int32 ChannelId, const UObject* WorldContextObject);

	// See UOnsetVoipTalker::SetMutedForPlayerState
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Server")
	static void SetTalkerMutedForPlayerState(UOnsetVoipTalker* VoipTalker, bool bMute, APlayerState* PlayerState);

	// See UOnsetVoipTalker::SetMutedForVoipTalker
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Server")
	static void SetTalkerMutedForVoipTalker(UOnsetVoipTalker* VoipTalker, bool bMute, UOnsetVoipTalker* OtherVoipTalker);

	// See UOnsetVoipTalker::IsMutedForVoipTalker
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Server")
	static bool IsTalkerMutedForVoipTalker(UOnsetVoipTalker* VoipTalker, UOnsetVoipTalker* OtherVoipTalker);

	// See UOnsetVoipTalker::IsMutedForPlayerState
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Server")
	static bool IsTalkerMutedForPlayerState(UOnsetVoipTalker* VoipTalker, APlayerState* PlayerState);

	// See UOnsetVoipTalker::SetMutedOnVoiceChannel
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Server")
	static void SetTalkerMutedOnVoiceChannel(UOnsetVoipTalker* VoipTalker, bool bMute, int32 ChannelId);

	// See UOnsetVoipTalker::IsMutedOnVoiceChannel
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Server")
	static bool IsTalkerMutedOnVoiceChannel(UOnsetVoipTalker* VoipTalker, int32 ChannelId);
	
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip", meta = (WorldContext = "WorldContextObject"))
	static void PrintToConsole(const FString& Message, const UObject* WorldContextObject);

	// Blueprint setter helper for cvar "voice.MicNoiseGateThreshold"
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Mic")
	static void SetMicNoiseGateThreshold(float InThreshold);

	// Blueprint setter helper for cvar "voice.SilenceDetectionThreshold"
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Mic")
	static void SetSilenceDetectionThreshold(float InThreshold);

	// Blueprint getter helper for cvar "voice.MicNoiseGateThreshold"
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Mic")
	static float GetMicNoiseGateThreshold();

	// Blueprint getter helper for cvar "voice.SilenceDetectionThreshold"
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Mic")
	static float GetSilenceDetectionThreshold();

	// Get plugin version as string.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip")
	static FString GetOnsetVoipVersion();

	// This will call SetVolumeMultiplier() on all existing UOnsetVoipAudioComponent that are used for 2D audio/voice playback. (i.e. voice channels)
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip")
	static void SetVoipAudioComponentsVolume_2DVoice(float InVolume);

	// This will call SetVolumeMultiplier() on all existing UOnsetVoipAudioComponent that are used for 3D audio/voice playback. (i.e. channel 0 / world / proximity)
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip")
	static void SetVoipAudioComponentsVolume_3DVoice(float InVolume);

	// This will call SetVolumeMultiplier() on all existing UOnsetVoipAudioComponent for both 2D and 3D audio/voice playback.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip")
	static void SetVoipAudioComponentsVolume(float InVolume);

	// In editor gets the current Play In Editor instance id.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared", meta = (WorldContext = "WorldContextObject"))
	static int32 GetPIEInstanceID(const UObject* WorldContextObject);
};
