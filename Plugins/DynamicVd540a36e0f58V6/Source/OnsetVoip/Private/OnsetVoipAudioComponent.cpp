// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetVoipAudioComponent.h"
#include "OnsetVoip.h"
#include "OnsetVoipSettings.h"
#include "OnsetVoipBlueprintFunctionLibrary.h"
#include "UObject/UObjectIterator.h" // For TObjectRange

#if !UE_SERVER
FAutoConsoleCommandWithWorldAndArgs _CmdNumberOfFramesToStopPlaybackIfNoAudio(TEXT("voice.NumberOfFramesToStopPlaybackIfNoAudio"),
	TEXT("Number of frames to wait to call Stop() on an UOnsetVoipAudioComponent if there is no audio to play. 0 to disable.\n")
	TEXT("0: Disable"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() == 0)
	{
		OnsetVoip::ConsoleLog(World, TEXT("voice.NumberOfFramesToStopPlaybackIfNoAudio <frames>"));
		return;
	}

	const int32 Frames = FCString::Atoi(*Args[0]);
	int32 NumberOfComponentsUpdated = 0;
	for (UOnsetVoipAudioComponent* VoipAudioComponent : TObjectRange<UOnsetVoipAudioComponent>())
	{
		VoipAudioComponent->SetNumberOfFramesToStopPlaybackIfNoAudio(Frames);
		NumberOfComponentsUpdated++;
	}

	OnsetVoip::ConsoleLog(World, FString::Printf(TEXT("%i components were changed to to %i frames"), NumberOfComponentsUpdated, Frames));
}));
#endif

static float VoiceVolume2DCVar = 1.0f;
FAutoConsoleVariableRef CVarVoiceVolume2D(
	TEXT("voice.volume2d"),
	VoiceVolume2DCVar,
	TEXT("Sets volume multiplier on all 2D voip audio components and all future 2D componenets.\n")
	TEXT("Value: Volume multiplier."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
		{
			const float NewVolume = Variable->GetFloat();
			UOnsetVoipBlueprintLibrary::SetVoipAudioComponentsVolume_2DVoice(NewVolume);
		}),
	ECVF_Default);

static float VoiceVolume3DCVar = 1.0f;
FAutoConsoleVariableRef CVarVoiceVolume3D(
	TEXT("voice.volume3d"),
	VoiceVolume3DCVar,
	TEXT("Sets volume multiplier on all 3D voip audio components and all future 3D componenets.\n")
	TEXT("Value: Volume multiplier."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
		{
			const float NewVolume = Variable->GetFloat();
			UOnsetVoipBlueprintLibrary::SetVoipAudioComponentsVolume_3DVoice(NewVolume);
		}),
	ECVF_Default);

UOnsetVoipAudioComponent::UOnsetVoipAudioComponent()
{
	// Allow ticks but don't start with ticking. Component tick start managed by the voip talker.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Let the engine stop it when the owner is destroyed. This the case when a pawn owns this component.
	bStopWhenOwnerDestroyed = true;

	// Don't automatically have the engine destroy this component when there's no audio.
	bAutoDestroy = false;
	
	// Don't auto play. Playback managed by voip talker.
	bAutoActivate = false;

	// Prioritize voice audio components.
	bAlwaysPlay = true;
	bIsVirtualized = false;

	DefaultOnsetVoipSoundWaveClass = UOnsetVoipSoundWave::StaticClass();
}

#if !UE_SERVER
void UOnsetVoipAudioComponent::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		VoipSoundWave = NewObject<UOnsetVoipSoundWave>(this, DefaultOnsetVoipSoundWaveClass);
		VoipSoundWave->AttenuationSettings = GetDefault<UOnsetVoipSettings>()->DefaultSoundAttenuation3D.LoadSynchronous();
		VoipSoundWave->SourceEffectChain = GetDefault<UOnsetVoipSettings>()->DefaultSourceEffectPresetChain3D.LoadSynchronous();

		SetSound(VoipSoundWave);

		NumberOfFramesToStopPlaybackIfNoAudio = GetDefault<UOnsetVoipSettings>()->NumberOfFramesToStopPlaybackIfNoAudio;

		ComponentTags.AddUnique(TEXT("3DVoice"));
	}
}

void UOnsetVoipAudioComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (NumberOfFramesToStopPlaybackIfNoAudio > 0)
	{
		if (VoipSoundWave == nullptr || VoipSoundWave->GetTotalPendingAudioByteCount() == 0)
		{
			StarvedDataCount++;
		}
		else
		{
			StarvedDataCount = 0;
		}

		if (StarvedDataCount > NumberOfFramesToStopPlaybackIfNoAudio)
		{
			UE_LOG(LogOnsetVoip, Verbose, TEXT("Stopped playing %s (Is2D %s)."), *GetName(), ComponentHasTag(TEXT("2DVoice")) ? TEXT("true") : TEXT("false"));

			Stop();
			SetComponentTickEnabled(false);

			StarvedDataCount = 0;
		}
	}
}

void UOnsetVoipAudioComponent::BeginPlay()
{
	UE_LOG(LogOnsetVoip, Verbose, TEXT("%s::BeginPlay()"), *GetName());

	Super::BeginPlay();

	const float AudioVolumeToUse = ComponentHasTag(TEXT("3DVoice")) ? VoiceVolume3DCVar : VoiceVolume2DCVar;
	SetVolumeMultiplier(AudioVolumeToUse);
}

void UOnsetVoipAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogOnsetVoip, Verbose, TEXT("%s::EndPlay(%i)"), *GetName(), int(EndPlayReason));

	Super::EndPlay(EndPlayReason);
}
#endif /* !UE_SERVER */

FString UOnsetVoipAudioComponent::Describe() const
{
	return FString::Printf(TEXT("%s: Owner: %s, AttachParentActor: %s, %s [SoundWave: %s]"),
		*GetPathName(), *GetPathNameSafe(GetOwner()), *GetPathNameSafe(GetAttachParentActor()), ComponentHasTag(TEXT("2DVoice")) ? TEXT("2DVoice") : TEXT("3DVoice"), VoipSoundWave ? *VoipSoundWave->Describe() : TEXT("null"));
}

#if !UE_SERVER
void UOnsetVoipAudioComponent2D::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		if (VoipSoundWave)
		{
			VoipSoundWave->AttenuationSettings = GetDefault<UOnsetVoipSettings>()->DefaultSoundAttenuation2D.LoadSynchronous();
			VoipSoundWave->SourceEffectChain = GetDefault<UOnsetVoipSettings>()->DefaultSourceEffectPresetChain2D.LoadSynchronous();
		}

		ComponentTags.AddUnique(TEXT("2DVoice"));
		ComponentTags.Remove(TEXT("3DVoice"));
	}
}
#endif
