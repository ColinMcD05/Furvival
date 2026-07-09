// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetVoipWorldSubsystem.h"
#include "OnsetVoip.h"
#include "OnsetVoipTalker.h"
#include "OnsetVoipPawnInterface.h"
#include "OnsetVoipAudioComponent.h"
#include "OnsetVoipDataChannel.h"
#include "OnsetVoipSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/FileHelper.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Engine/GameInstance.h"
#include "Runtime/Launch/Resources/Version.h"

#if ONSETVOIP_ENABLE_VOICE_PROCESSING
#include "Recording/OnsetVoipRecorderRemote.h"
#include "Recording/OnsetVoipRecorderRemoteSingleFile.h"
#endif

FAutoConsoleCommandWithWorld _CmdDebugTalkers(TEXT("debug.talkers"), TEXT("Alias for \"voice.talkers\""),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
	if (World)
	{
		if (const UOnsetVoipWorldSubsystem* Subsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>())
		{
			Subsystem->PrintTalkers();
		}
	}
}));

FAutoConsoleCommandWithWorld _CmdVoiceTalkers(TEXT("voice.talkers"), TEXT("Prints all UOnsetVoipTalker"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
	if (World)
	{
		if (const UOnsetVoipWorldSubsystem* Subsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>())
		{
			Subsystem->PrintTalkers();
		}
	}
}));

void UOnsetVoipWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if ONSETVOIP_ENABLE_VOICE_PROCESSING
	static_assert(OnsetVoip::MaxDecompressedBufferSize > 0);
	DecompressedVoiceData.AddUninitialized(OnsetVoip::MaxDecompressedBufferSize);
#endif

	OnsetVoipTalkerClass = GetDefault<UOnsetVoipSettings>()->DefaultOnsetVoipTalkerClass.Get();
	if (!IsValid(OnsetVoipTalkerClass) || !OnsetVoipTalkerClass->IsChildOf(UOnsetVoipTalker::StaticClass()))
	{
		OnsetVoipTalkerClass = UOnsetVoipTalker::StaticClass();
	}
}

void UOnsetVoipWorldSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (ActorSpawnedDelegateHandle.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(ActorSpawnedDelegateHandle);
			ActorSpawnedDelegateHandle.Reset();
		}

		if (ActorDestroyedDelegateHandle.IsValid())
		{
#if ENGINE_MINOR_VERSION >= 5
			World->RemoveOnActorDestroyedHandler(ActorDestroyedDelegateHandle);
#else
			World->RemoveOnActorDestroyededHandler(ActorDestroyedDelegateHandle);
#endif
			ActorDestroyedDelegateHandle.Reset();
		}

#if !UE_SERVER
		World->GetTimerManager().ClearTimer(VoipTalkerUpdateTimerHandle);
#endif
	}
	else
	{
		ActorSpawnedDelegateHandle.Reset();
		ActorDestroyedDelegateHandle.Reset();
	}

	VoipTalkerUpdateTimerHandle.Invalidate();

#if ONSETVOIP_ENABLE_VOICE_PROCESSING
	VoipRecorder = nullptr;
#endif

	VoipTalkers.Empty();

	Super::Deinitialize();
}

void UOnsetVoipWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	UE_LOG(LogOnsetVoip, Verbose, TEXT("%s::OnWorldBeginPlay(%s)"), *GetName(), *InWorld.GetName());
	UE_LOG(LogOnsetVoip, Verbose, TEXT("World UNetDriver: %s"), *GetPathNameSafe(InWorld.NetDriver));

	if (const UNetDriver* NetDriver = InWorld.NetDriver)
	{
		for (const auto& ChannelDefinition : NetDriver->ChannelDefinitionMap)
		{
			const FString& ChannelClassName = ChannelDefinition.Value.ChannelClass ? ChannelDefinition.Value.ChannelClass->GetPathName() : TEXT("(null)");

			UE_LOG(LogOnsetVoip, Verbose, TEXT(" Channel[%i] %s %s"), ChannelDefinition.Value.StaticChannelIndex, *ChannelDefinition.Key.ToString(), *ChannelClassName);
		}

		/*

		Check for the existence of an UOnsetVoiceDataChannel. Force an assert if not found.
		If this is triggered then you did not properly configure UOnsetVoiceDataChannel in your DefaultEngine.ini config file.

		Unreal Engine >= 5.3:
		[/Script/Engine.NetDriver]
		+ChannelDefinitions=(ChannelName=OnsetVoip, ClassName=/Script/OnsetVoip.OnsetVoipDataChannel, StaticChannelIndex=3, bTickOnCreate=true, bServerOpen=true, bClientOpen=true, bInitialServer=true, bInitialClient=true)

		Unreal Engine <= 5.2: (Notice the StaticChannelIndex value)
		[/Script/Engine.NetDriver]
		+ChannelDefinitions=(ChannelName=OnsetVoip, ClassName=/Script/OnsetVoip.OnsetVoipDataChannel, StaticChannelIndex=2, bTickOnCreate=true, bServerOpen=true, bClientOpen=true, bInitialServer=true, bInitialClient=true)

		Please make sure that you have exactly this line in your config file WITHOUT any line breaks. It must be a single line like above.

		*/

		checkf(NetDriver->IsKnownChannelName(OnsetVoipChannelName), TEXT("This is a forced crash because the OnsetVoip channel was not configured in the DefaultEngine.ini. Please read the documentation on how to set it up."));

		UE_LOG(LogOnsetVoip, Verbose, TEXT("Channel %s(StaticChannelIndex: %i)"), *OnsetVoipChannelName.ToString(), NetDriver->ChannelDefinitionMap.Find(OnsetVoipChannelName)->StaticChannelIndex);

		checkf(NetDriver->ChannelDefinitionMap.Find(OnsetVoipChannelName)->StaticChannelIndex != INDEX_NONE, TEXT("The OnsetVoip channel was not properly configured in the DefaultEngine.ini. Please read the documentation on how to set it up. Ensure it looks exactly like in the documentation and the line +ChannelDefinitions has no line breaks. It must be a single line."));
	}

	// Play in editor the listen server player state was created before OnWorldBeginPlay is called.
	for (TActorIterator<APlayerState> ActorItr(&InWorld); ActorItr; ++ActorItr)
	{
		InternalGetOrCreateVoipTalker(*ActorItr);
	}

	// Listen to spawn and destroy events to automatically create voip talkers for player states.
	ActorSpawnedDelegateHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UOnsetVoipWorldSubsystem::OnActorSpawned));
	ActorDestroyedDelegateHandle = InWorld.AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateUObject(this, &UOnsetVoipWorldSubsystem::OnActorDestroyed));

#if !UE_SERVER
	InWorld.GetTimerManager().SetTimer(VoipTalkerUpdateTimerHandle, this, &UOnsetVoipWorldSubsystem::UpdateVoipTalkers, OnsetVoip::VoipTalkerUpdateRate, true, 2.0f);
#endif
}

bool UOnsetVoipWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return Super::DoesSupportWorldType(WorldType) && (WorldType == EWorldType::Game || WorldType == EWorldType::PIE);
}

void UOnsetVoipWorldSubsystem::OnActorSpawned(AActor* InActor)
{
	if (InActor->IsA(APlayerState::StaticClass()))
	{
		InternalGetOrCreateVoipTalker(Cast<APlayerState>(InActor));
	}
}

void UOnsetVoipWorldSubsystem::OnActorDestroyed(AActor* InActor)
{
	if (InActor->IsA(APlayerState::StaticClass()))
	{
		InternalDestroyVoipTalker(Cast<APlayerState>(InActor));
	}
}

UOnsetVoipTalker* UOnsetVoipWorldSubsystem::InternalGetOrCreateVoipTalker(APlayerState* PlayerState)
{
	checkf(IsValid(PlayerState), TEXT("Invalid PlayerState object passed to InternalGetOrCreateVoipTalker"));

	UOnsetVoipTalker* VoipTalker = GetVoipTalker(PlayerState);

	if (VoipTalker == nullptr)
	{
		VoipTalker = NewObject<UOnsetVoipTalker>(PlayerState, OnsetVoipTalkerClass);
		checkf(VoipTalker, TEXT("Did you change the setting DefaultOnsetVoipTalkerClass? Try to reset it or see if it is referring to a correct UOnsetVoipTalker class."));
		VoipTalkers.Add(PlayerState, VoipTalker);

		UE_LOG(LogOnsetVoip, Verbose, TEXT("Created %s for [%i]%s in world %s[%s]."), *VoipTalker->GetName(), PlayerState->GetPlayerId(), *PlayerState->GetPathName(), *GetWorld()->GetName(), *ToString(GetWorld()->GetNetMode()));

		Native_OnVoipTalkerCreated.Broadcast(VoipTalker);
		OnVoipTalkerCreated.Broadcast(VoipTalker);
	}

	return VoipTalker;
}

void UOnsetVoipWorldSubsystem::InternalDestroyVoipTalker(APlayerState* PlayerState)
{
	if (UOnsetVoipTalker* const* Talker = VoipTalkers.Find(PlayerState))
	{
		UE_LOG(LogOnsetVoip, Verbose, TEXT("Removing %s for [%i]%s."), *(*Talker)->GetName(), PlayerState->GetPlayerId(), *PlayerState->GetPathName());

		Native_OnVoipTalkerDestroyed.Broadcast(*Talker);
		OnVoipTalkerDestroyed.Broadcast(*Talker);

		for (const auto& Iter : VoipTalkers)
		{
			Iter.Value->MutedVoipTalkers.Remove(*Talker);
		}

		const int32 RemovedCount = VoipTalkers.Remove(PlayerState);
		ensureMsgf(RemovedCount == 1, TEXT("In InternalDestroyVoipTalker removed %i elements from VoipTalkers map."), RemovedCount);
	}
}

UOnsetVoipTalker* UOnsetVoipWorldSubsystem::GetVoipTalker(APlayerState* PlayerState) const
{
	UOnsetVoipTalker* const* Talker = VoipTalkers.Find(PlayerState);
	if (Talker)
	{
		checkSlow(*Talker); // VoipTalker must always contain valid talkers.
		return *Talker;
	}
	return nullptr;
}

TArray<UOnsetVoipTalker*> UOnsetVoipWorldSubsystem::GetAllTalkers() const
{
	TArray<UOnsetVoipTalker*> Talkers;
	Talkers.Reserve(VoipTalkers.Num());
	for (const auto& Iter : VoipTalkers)
	{
		Talkers.Add(Iter.Value);
	}
	return Talkers;
}

TArray<UOnsetVoipTalker*> UOnsetVoipWorldSubsystem::GetAllTalkersInVoiceChannel(int32 ChannelId) const
{
	TArray<UOnsetVoipTalker*> Talkers;
	for (const auto& Iter : VoipTalkers)
	{
		if (Iter.Value->IsInVoiceChannel(ChannelId))
		{
			Talkers.Add(Iter.Value);
		}
	}
	return Talkers;
}

void UOnsetVoipWorldSubsystem::UpdateVoipTalkers()
{
#if !UE_SERVER
	const double CurrentTime = FPlatformTime::Seconds();

	for (const auto& Iter : VoipTalkers)
	{
		Iter.Value->UpdateTalkingState(CurrentTime);
	}
#endif
}

void UOnsetVoipWorldSubsystem::PrintTalkers() const
{
	if (GetWorld() && GetWorld()->GetGameInstance())
	{
		if (const AController* Controller = GetWorld()->GetGameInstance()->GetFirstLocalPlayerController())
		{
			if (const APlayerState* OurPlayerState = Controller->GetPlayerState<APlayerState>())
			{
				OnsetVoip::ConsoleLog(this, FString::Printf(TEXT("OurPlayerState: [%i]%s(%s)"), OurPlayerState->GetPlayerId(), *OurPlayerState->GetPathName(), *OurPlayerState->GetPlayerName()));
			}
		}
	}

	OnsetVoip::ConsoleLog(this, FString::Printf(TEXT("Total of %i VoipTalkers:"), VoipTalkers.Num()));

	for (const auto& Iter : VoipTalkers)
	{
		const UOnsetVoipTalker* Talker = Iter.Value;
		if (Talker)
		{
			OnsetVoip::ConsoleLog(this, FString::Printf(TEXT(" %s [IsTalking: %i] for [%i]%s(%s)"),
				*Talker->GetName(), Talker->IsTalking(), Talker->GetPlayerState()->GetPlayerId(), *Talker->GetPlayerState()->GetPathName(), *Talker->GetPlayerState()->GetPlayerName()));

			if (const UOnsetVoipAudioComponent* AudioComponent2D = Talker->GetCachedAudioComponent2D())
			{
				OnsetVoip::ConsoleLog(this, FString::Printf(TEXT("  2D: %s"), *AudioComponent2D->Describe()));
			}

			if (const UOnsetVoipAudioComponent* AudioComponent3D = Talker->GetCachedAudioComponent3D())
			{
				OnsetVoip::ConsoleLog(this, FString::Printf(TEXT("  3D: %s"), *AudioComponent3D->Describe()));
			}
		}
	}
}

bool UOnsetVoipWorldSubsystem::StartRecording(const FString& Filename, EOnsetAudioRecordingFormat RecordingFormat, bool bMixToSingleFile)
{
#if ONSETVOIP_ENABLE_VOICE_PROCESSING
	if (VoipRecorder.IsValid())
	{
		return false;
	}

	VoipRecorder = bMixToSingleFile
		? CreateAudioRecorder<FOnsetVoipRecorderRemoteSingleFile, UOnsetVoipWorldSubsystem>(this, RecordingFormat, Filename.Len() > 0 ? Filename : TOptional<FString>())
		: CreateAudioRecorder<FOnsetVoipRecorderRemote, UOnsetVoipWorldSubsystem>(this, RecordingFormat, Filename.Len() > 0 ? Filename : TOptional<FString>());

	return VoipRecorder.IsValid();
#else
	return false;
#endif
}

bool UOnsetVoipWorldSubsystem::StopRecording()
{
#if ONSETVOIP_ENABLE_VOICE_PROCESSING
	if (!VoipRecorder.IsValid())
	{
		return false;
	}

	VoipRecorder = nullptr;
	return true;
#else
	return false;
#endif
}

bool UOnsetVoipWorldSubsystem::IsRecording()
{
#if ONSETVOIP_ENABLE_VOICE_PROCESSING
	return VoipRecorder.IsValid();
#else
	return false;
#endif
}

FString UOnsetVoipWorldSubsystem::GetRecordingFile() const
{
#if ONSETVOIP_ENABLE_VOICE_PROCESSING
	return VoipRecorder.IsValid() ? VoipRecorder->GetRecordingFile() : TEXT("");
#else
	return TEXT("");
#endif
}
