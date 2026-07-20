// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "OnsetVoipSoundWave.h"
#include "OnsetVoipAudioComponent.generated.h"

/**
 * The Onset VoIP audio component. Can be added manually to a pawn/character to prevent spawning and attaching one dynamically.
 */
UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class ONSETVOIP_API UOnsetVoipAudioComponent : public UAudioComponent
{
	GENERATED_BODY()
	
public:
	UOnsetVoipAudioComponent();

#if !UE_SERVER
	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface

	//~ Begin UActorComponent Interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface
#endif

	// Procedural sound wave object used for voice playback.
	UPROPERTY(BlueprintReadOnly, Category = "OnsetVoip")
	TObjectPtr<UOnsetVoipSoundWave> VoipSoundWave;

	// Procedural sound wave class to use for voice playback.
	UPROPERTY(EditDefaultsOnly, Category = "OnsetVoip")
	TSubclassOf<UOnsetVoipSoundWave> DefaultOnsetVoipSoundWaveClass;

	// Describe this component and properties.
	virtual FString Describe() const;

protected:
	// Used for tracking when no this audio component should stop playing due to no more voice data being available.
	int32 StarvedDataCount = 0;

	int32 NumberOfFramesToStopPlaybackIfNoAudio = 0;

public:
	void SetNumberOfFramesToStopPlaybackIfNoAudio(int32 InNumberOfFramesToStopPlaybackIfNoAudio)
	{
		NumberOfFramesToStopPlaybackIfNoAudio = FMath::Max(0, InNumberOfFramesToStopPlaybackIfNoAudio);
	}
};

/**
 * Subclass for 2D audio components
 */
UCLASS(Blueprintable, BlueprintType)
class ONSETVOIP_API UOnsetVoipAudioComponent2D : public UOnsetVoipAudioComponent
{
	GENERATED_BODY()
	
public:
#if !UE_SERVER
	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface
#endif
};
