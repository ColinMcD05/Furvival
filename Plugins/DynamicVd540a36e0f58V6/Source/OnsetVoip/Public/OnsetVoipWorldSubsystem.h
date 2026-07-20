// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "OnsetVoipTalker.h"
#include "Recording/OnsetVoipRecorder.h"
#include "OnsetVoipWorldSubsystem.generated.h"

namespace OnsetVoip
{
	// Maximum size of the buffer that we use to write decompressed voice data into.
	constexpr uint32 MaxDecompressedBufferSize = 2U * 48000U * sizeof(int16);

	// Time at which all talkers are updated. Used for detecting when a player stops speaking to call corresponding delegates.
	constexpr float VoipTalkerUpdateRate = 0.25f;

	// Time to pass after receiving the last voip data to assume the player has stopped speaking.
	constexpr double TimeRemoteTalkerConsideredStoppedTalking = 0.3;
}

/**
 * Subsystem managing UOnsetVoipTalker (other/remote players).
 */
UCLASS()
class ONSETVOIP_API UOnsetVoipWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem Interface

	// Delegates bound to world callbacks.
	FDelegateHandle ActorSpawnedDelegateHandle;
	FDelegateHandle ActorDestroyedDelegateHandle;

	// Functions called upon world delegates.
	void OnActorSpawned(AActor* InActor);
	void OnActorDestroyed(AActor* InActor);

private:
	// Internally create and destroy a VoIP talker for a player state.
	UOnsetVoipTalker* InternalGetOrCreateVoipTalker(APlayerState* PlayerState);
	void InternalDestroyVoipTalker(APlayerState* PlayerState);

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoipTalkerCreatedOrDestroyed, UOnsetVoipTalker*, VoipTalker);
	DECLARE_MULTICAST_DELEGATE_OneParam(FNativeOnVoipTalkerCreatedOrDestroyed, UOnsetVoipTalker*/*, VoipTalker*/);

	// Gets the voip talker for a specific player state.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared")
	UOnsetVoipTalker* GetVoipTalker(APlayerState* PlayerState) const;

	// Gets an array of all voip talkers.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared")
	TArray<UOnsetVoipTalker*> GetAllTalkers() const;

	// Gets an array of all voip talkers in a specific voice channel.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Server")
	TArray<UOnsetVoipTalker*> GetAllTalkersInVoiceChannel(int32 ChannelId) const;

	// Called when any voip talker starts or stops talking.
	UPROPERTY(BlueprintAssignable, Category = "OnsetVoip|Client")
	UOnsetVoipTalker::FOnVoipTalkingStateChange OnVoipTalkingStateChange;

	// Called when a new instance of OnsetVoipTalker is created. (Player joined)
	UPROPERTY(BlueprintAssignable, Category = "OnsetVoip|Shared")
	FOnVoipTalkerCreatedOrDestroyed OnVoipTalkerCreated;

	// Called when an instance of OnsetVoipTalker is destroyed. (Player left)
	UPROPERTY(BlueprintAssignable, Category = "OnsetVoip|Shared")
	FOnVoipTalkerCreatedOrDestroyed OnVoipTalkerDestroyed;

	// C++ versions of the above delegates.
	FNativeOnVoipTalkerCreatedOrDestroyed Native_OnVoipTalkerCreated;
	FNativeOnVoipTalkerCreatedOrDestroyed Native_OnVoipTalkerDestroyed;

	// See UOnsetVoipTalker::OnVoipAudioDataReceived.
	UPROPERTY(BlueprintAssignable, Category = "OnsetVoip|Client")
	UOnsetVoipTalker::FOnVoipAudioDataReceived OnVoipAudioDataReceived;

	// See UOnsetVoipTalker::Native_OnVoipAudioDataReceived.
	UOnsetVoipTalker::FNativeOnVoipAudioDataReceived Native_OnVoipAudioDataReceived;

	// Called by the timer to update all voip talkers.
	UFUNCTION()
	void UpdateVoipTalkers();

#if ONSETVOIP_ENABLE_VOICE_PROCESSING
	TArray<uint8>& GetTemporaryDecompressionBuffer()
	{
		return DecompressedVoiceData;
	}
#endif

	void PrintTalkers() const;

private:
	// Timer handle for UpdateVoipTalkers().
	FTimerHandle VoipTalkerUpdateTimerHandle;

	// Map holding the voip talker for each corresponding player state. Player state is a strong reference here.
	// Upon actor destruction of the player state the voip talker will be destroyed as well.
	UPROPERTY(Transient)
	TMap<APlayerState*, UOnsetVoipTalker*> VoipTalkers;

public:
	// For stat voip
	const auto* const GetVoipTalkerMap() const
	{
		return &VoipTalkers;
	}

private:
#if ONSETVOIP_ENABLE_VOICE_PROCESSING
	// Buffer used to decompress voice data.
	TArray<uint8> DecompressedVoiceData;
#endif

	// See UOnsetVoipSettings::DefaultOnsetVoipTalkerClass
	UPROPERTY()
	TObjectPtr<UClass> OnsetVoipTalkerClass = nullptr;

public:
	/* Start recording remote talkers to an audio file.
	 * Filename: If empty, a file will be generated.
	 * bMixToSingleFile: If true, all audio data will be mixed into a single file instead of recording each remote talker to a separate file.
	 * Returns false if recording is already running or failed to start.
	 * To enable this on a dedicated server see OnsetVoip.Build.cs file.
	 */
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Shared")
	bool StartRecording(const FString& Filename, EOnsetAudioRecordingFormat RecordingFormat, bool bMixToSingleFile);

	// Stop recording. Returns false if not recording.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Shared")
	bool StopRecording();

	// Returns true if currently recording.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared")
	bool IsRecording();

	// Returns the recording directory. If this is a single-file recording, it returns the directory + filename.
	UFUNCTION(BlueprintPure, Category = "OnsetVoip|Shared")
	FString GetRecordingFile() const;

#if ONSETVOIP_ENABLE_VOICE_PROCESSING
private:
	TSharedPtr<IOnsetVoipRecorder> VoipRecorder;
#endif
};
