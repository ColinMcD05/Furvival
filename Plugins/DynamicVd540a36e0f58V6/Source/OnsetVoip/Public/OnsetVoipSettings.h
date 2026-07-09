// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OnsetVoipTalker.h"
#include "OnsetNameplateComponent.h"
#include "Blueprint/UserWidget.h"
#include "Sound/SoundEffectSource.h"
#include "Sound/SoundAttenuation.h"
#include "OnsetVoipSettings.generated.h"

// From engine VoiceConfig.h
UENUM(BlueprintType)
enum class EOnsetAudioEncodeHint : uint8
{
	/** Best for most VoIP applications where listening quality and intelligibility matter most */
	VoiceEncode_Voice,
	/** Best for broadcast/high-fidelity application where the decoded audio should be as close as possible to the input */
	VoiceEncode_Audio
};

USTRUCT(BlueprintType)
struct ONSETVOIP_API FNameplateAttachParam
{
	GENERATED_USTRUCT_BODY()

	// The socket that the nameplate widget component attaches to.
	UPROPERTY(EditAnywhere, Category = "Nameplate Attachment Parameters")
	FName AttachSocketName = NAME_None;

	// Relative attach location of the component.
	UPROPERTY(EditAnywhere, Category = "Nameplate Attachment Parameters")
	FVector RelativeAttachLocation = FVector::ZeroVector;

	// If true, half of the bounds/size of the pawn/character is added to the relative attach location to estimate that it is above the pawn/character's visible mesh.
	UPROPERTY(EditAnywhere, Category = "Nameplate Attachment Parameters")
	bool bAdjustAttachZAccordingToActorBounds = false;
};

USTRUCT(BlueprintType)
struct ONSETVOIP_API FVoiceEncoderSettings
{
	GENERATED_USTRUCT_BODY()

	// Copied setting from VoiceConfig.h, used to tell encoder what type of audio is being encoded.
	UPROPERTY(EditAnywhere, Category = "Voice Encoder")
	EOnsetAudioEncodeHint EncodingHint = EOnsetAudioEncodeHint::VoiceEncode_Voice;

	// Enables advanced encoder settings.
	UPROPERTY(EditAnywhere, Category = "Voice Encoder")
	bool bAdvancedEncoderSettings = false;

	// Encoder variable bitrate.
	UPROPERTY(EditAnywhere, Category = "Voice Encoder", meta = (EditCondition = "bAdvancedEncoderSettings", EditConditionHides))
	bool bEnableVBR = true;

	// Encoder algorithmic complexity. A higher value will result in better quality, but also requires more CPU usage.
	UPROPERTY(EditAnywhere, Category = "Voice Encoder", meta = (EditCondition = "bAdvancedEncoderSettings", EditConditionHides, ClampMin = 0, ClampMax = 10))
	int32 Complexity = 10;

	// Encoder bitrate. See https://wiki.xiph.org/Opus_Recommended_Settings Higher values may require more bandwidth.
	UPROPERTY(EditAnywhere, Category = "Voice Encoder", meta = (EditCondition = "bAdvancedEncoderSettings", EditConditionHides, ClampMin = 500, ClampMax = 512000))
	int32 BitRate = 24000;
};

/**
 * 
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Onset VoIP Settings"))
class ONSETVOIP_API UOnsetVoipSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UOnsetVoipSettings();

	UPROPERTY(config, EditAnywhere, Category = "Voice", meta = (ToolTip = "Microphone capture sample rate. Possible values are 8000, 12000, 16000, 24000 and 48000. Higher values may require more bandwidth."))
	int32 VoiceCaptureSampleRate;

	UPROPERTY(config, EditAnywhere, Category = "Voice", meta = (ToolTip = "Number of microphone capturing channels. Possible values are 1 (mono) or 2 (stereo), default: 1. Stereo requires more bandwidth.", ClampMin = 1, ClampMax = 2))
	int32 VoiceCaptureChannels;

	UPROPERTY(config, EditAnywhere, Category = "Voice")
	FVoiceEncoderSettings VoiceEncoderSettings;

	// This setting lets you specify a child class of UOnsetVoipTalker. Leave it as is by default.
	UPROPERTY(config, EditAnywhere, Category = "Voice")
	TSubclassOf<UOnsetVoipTalker> DefaultOnsetVoipTalkerClass;

	// Used to override the audio component class for 2D voice.
	UPROPERTY(config, EditAnywhere, Category = "Voice|Playback")
	TSubclassOf<UOnsetVoipAudioComponent2D> DefaultOnsetVoipAudioComponentClass2D;
	
	// Used to override the audio component class for 3D voice.
	UPROPERTY(config, EditAnywhere, Category = "Voice|Playback")
	TSubclassOf<UOnsetVoipAudioComponent> DefaultOnsetVoipAudioComponentClass3D;
	
	UPROPERTY(config, EditAnywhere, Category = "Voice|Playback")
	TSoftObjectPtr<USoundAttenuation> DefaultSoundAttenuation3D;

	UPROPERTY(config, EditAnywhere, Category = "Voice|Playback")
	TSoftObjectPtr<USoundAttenuation> DefaultSoundAttenuation2D;

	// Default USoundEffectSourcePresetChain to use for the voip sound wave when a player talks in 3D space.
	UPROPERTY(config, EditAnywhere, Category = "Voice|Playback")
	TSoftObjectPtr<USoundEffectSourcePresetChain> DefaultSourceEffectPresetChain3D;
	
	// Default USoundEffectSourcePresetChain to use for the voip sound wave when a player talks in 2D space (voice channel).
	UPROPERTY(config, EditAnywhere, Category = "Voice|Playback")
	TSoftObjectPtr<USoundEffectSourcePresetChain> DefaultSourceEffectPresetChain2D;

	// What widget to use for the nameplate widget component on dynamic creation. If you attach a nameplate component manually then it will not use this but rather the widget class already set in the component details.
	UPROPERTY(config, EditAnywhere, Category = "Voice|Nameplate")
	TSoftClassPtr<UUserWidget> DefaultNameplateWidget;

	// Default nameplate component class used for dynamic creation and attachment if no component was added manually by the user on the pawn/character class.
	UPROPERTY(config, EditAnywhere, Category = "Voice|Nameplate")
	TSubclassOf<UOnsetNameplateComponent> DefaultNameplateComponentClass;

	// For dynamic creation and attachment of the nameplate component you can specify some rules here.
	UPROPERTY(config, EditAnywhere, Category = "Voice|Nameplate")
	TArray<FNameplateAttachParam> NameplateAttachParams;
	
	// The default nameplate draw distance, or 0.0 to disable the distance limit.
	UPROPERTY(config, EditAnywhere, Category = "Voice|Nameplate")
	double NameplateMaxDrawDistance;
	
	// Amount of buffered voice audio required before starting playback or resuming after starvation. A small prebuffer helps smooth out jitter at the start of speech.
	UPROPERTY(config, EditAnywhere, Category = "Voice|Advanced", meta = (ClampMin = 0.0, ClampMax = 1.0))
	double InitialPlaybackBufferDelay;

	// Maximum buffered voice duration we keep before dropping the oldest queued audio to avoid excessive latency.
	UPROPERTY(config, EditAnywhere, Category = "Voice|Advanced", meta = (ClampMin = 0.02, ClampMax = 2.0))
	double MaxBufferPlaybackDelay;
	
	// Number of frames to wait to call Stop() on an UOnsetVoipAudioComponent if there is no audio to play. 0 to disable.
	UPROPERTY(config, EditAnywhere, Category = "Voice|Advanced", meta = (ClampMin = 0))
	int32 NumberOfFramesToStopPlaybackIfNoAudio;
	
	// Enable for the server to do additional distance checks between two players for voice packet replication in the world channel (0).
	UPROPERTY(config, EditAnywhere, Category = "Voice|Advanced")
	bool bUseDistanceBasedRelevancy;

	// The distance used to check whether two players are relevant to each other for the world channel (0), in Unreal Units (centimeters).
	UPROPERTY(config, EditAnywhere, Category = "Voice|Advanced", meta = (EditCondition = "bUseDistanceBasedRelevancy", EditConditionHides, ClampMin = 1.0))
	double MaxReplicationDistance;

	// Set the default volume of sound waves that play voice.
	UPROPERTY(config, EditAnywhere, Category = "Voice|Advanced", meta = (ClampMin = 0.0))
	float DefaultSoundWaveVolume;

	// Path to where the ffmpeg executable can be found. Only required for ConvertAudioFile node and audio recordings with codecs like mp3.
	UPROPERTY(config, EditAnywhere, Category = "Voice|Advanced", meta = (RelativeToGameDir))
	FDirectoryPath FFmpegPath;
};
