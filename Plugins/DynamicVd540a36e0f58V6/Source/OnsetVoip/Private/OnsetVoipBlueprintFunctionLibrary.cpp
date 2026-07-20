// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetVoipBlueprintFunctionLibrary.h"
#include "OnsetVoipWorldSubsystem.h"
#include "OnsetVoip.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"

void UOnsetVoipBlueprintLibrary::SetTalkerVoiceChannel(UOnsetVoipTalker* VoipTalker, int32 ChannelId, bool bAdd)
{
	if (VoipTalker)
	{
		VoipTalker->SetVoiceChannel(ChannelId, bAdd);
	}
}

bool UOnsetVoipBlueprintLibrary::IsTalkerInVoiceChannel(UOnsetVoipTalker* VoipTalker, int32 ChannelId)
{
	return VoipTalker && VoipTalker->IsInVoiceChannel(ChannelId);
}

UOnsetVoipTalker* UOnsetVoipBlueprintLibrary::GetTalkerByPlayerState(APlayerState* PlayerState, const UObject* WorldContextObject)
{
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (const UOnsetVoipWorldSubsystem* Subsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>())
		{
			return Subsystem->GetVoipTalker(PlayerState);
		}
	}

	return nullptr;
}

TArray<UOnsetVoipTalker*> UOnsetVoipBlueprintLibrary::GetAllTalkers(const UObject* WorldContextObject)
{
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (const UOnsetVoipWorldSubsystem* Subsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>())
		{
			return Subsystem->GetAllTalkers();
		}
	}

	TArray<UOnsetVoipTalker*> Empty;
	return Empty;
}

TArray<UOnsetVoipTalker*> UOnsetVoipBlueprintLibrary::GetAllTalkersInVoiceChannel(int32 ChannelId, const UObject* WorldContextObject)
{
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (const UOnsetVoipWorldSubsystem* Subsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>())
		{
			return Subsystem->GetAllTalkersInVoiceChannel(ChannelId);
		}
	}

	TArray<UOnsetVoipTalker*> Empty;
	return Empty;
}

void UOnsetVoipBlueprintLibrary::SetTalkerMutedForPlayerState(UOnsetVoipTalker* VoipTalker, bool bMute, APlayerState* PlayerState)
{
	if (VoipTalker)
	{
		VoipTalker->SetMutedForPlayerState(bMute, PlayerState);
	}
}

void UOnsetVoipBlueprintLibrary::SetTalkerMutedForVoipTalker(UOnsetVoipTalker* VoipTalker, bool bMute, UOnsetVoipTalker* OtherVoipTalker)
{
	if (VoipTalker)
	{
		VoipTalker->SetMutedForVoipTalker(bMute, OtherVoipTalker);
	}
}

bool UOnsetVoipBlueprintLibrary::IsTalkerMutedForVoipTalker(UOnsetVoipTalker* VoipTalker, UOnsetVoipTalker* OtherVoipTalker)
{
	return VoipTalker && VoipTalker->IsMutedForVoipTalker(OtherVoipTalker);
}

bool UOnsetVoipBlueprintLibrary::IsTalkerMutedForPlayerState(UOnsetVoipTalker* VoipTalker, APlayerState* PlayerState)
{
	return VoipTalker && VoipTalker->IsMutedForPlayerState(PlayerState);
}

void UOnsetVoipBlueprintLibrary::SetTalkerMutedOnVoiceChannel(UOnsetVoipTalker* VoipTalker, bool bMute, int32 ChannelId)
{
	if (VoipTalker)
	{
		VoipTalker->SetMutedOnVoiceChannel(bMute, ChannelId);
	}
}

bool UOnsetVoipBlueprintLibrary::IsTalkerMutedOnVoiceChannel(UOnsetVoipTalker* VoipTalker, int32 ChannelId)
{
	return VoipTalker && VoipTalker->IsMutedOnVoiceChannel(ChannelId);
}

void UOnsetVoipBlueprintLibrary::PrintToConsole(const FString& Message, const UObject* WorldContextObject)
{
	OnsetVoip::ConsoleLog(WorldContextObject, Message, false);
}

void UOnsetVoipBlueprintLibrary::SetMicNoiseGateThreshold(float InThreshold)
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.MicNoiseGateThreshold"));
	check(CVar);
	CVar->Set(InThreshold, ECVF_SetByGameOverride);
}

void UOnsetVoipBlueprintLibrary::SetSilenceDetectionThreshold(float InThreshold)
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.SilenceDetectionThreshold"));
	check(CVar);
	CVar->Set(InThreshold, ECVF_SetByGameOverride);
}

float UOnsetVoipBlueprintLibrary::GetMicNoiseGateThreshold()
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.MicNoiseGateThreshold"));
	check(CVar);
	return CVar->GetFloat();
}

float UOnsetVoipBlueprintLibrary::GetSilenceDetectionThreshold()
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.SilenceDetectionThreshold"));
	check(CVar);
	return CVar->GetFloat();
}

FString UOnsetVoipBlueprintLibrary::GetOnsetVoipVersion()
{
	return ONSET_VOIP_VERSION;
}

void UOnsetVoipBlueprintLibrary::SetVoipAudioComponentsVolume_2DVoice(float InVolume)
{
	const float Volume = FMath::Max(InVolume, 0.0f);

	for (UOnsetVoipAudioComponent2D* VoipAudioComponent : TObjectRange<UOnsetVoipAudioComponent2D>())
	{
		VoipAudioComponent->SetVolumeMultiplier(Volume);
	}
}

void UOnsetVoipBlueprintLibrary::SetVoipAudioComponentsVolume_3DVoice(float InVolume)
{
	const float Volume = FMath::Max(InVolume, 0.0f);

	for (UOnsetVoipAudioComponent* VoipAudioComponent : TObjectRange<UOnsetVoipAudioComponent>())
	{
		if (VoipAudioComponent->ComponentHasTag(TEXT("3DVoice")))
		{
			VoipAudioComponent->SetVolumeMultiplier(Volume);
		}
	}
}

void UOnsetVoipBlueprintLibrary::SetVoipAudioComponentsVolume(float InVolume)
{
	const float Volume = FMath::Max(InVolume, 0.0f);

	for (UOnsetVoipAudioComponent* VoipAudioComponent : TObjectRange<UOnsetVoipAudioComponent>())
	{
		VoipAudioComponent->SetVolumeMultiplier(Volume);
	}
}

int32 UOnsetVoipBlueprintLibrary::GetPIEInstanceID(const UObject* WorldContextObject)
{
	int32 PIEInstanceID = -1;

	const UWorld* OwningWorld = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	if (FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(OwningWorld))
	{
		PIEInstanceID = WorldContext->PIEInstance;
	}
	else if (OwningWorld && OwningWorld->GetPackage())
	{
		PIEInstanceID = OwningWorld->GetPackage()->GetPIEInstanceID();
	}

	return PIEInstanceID;
}
