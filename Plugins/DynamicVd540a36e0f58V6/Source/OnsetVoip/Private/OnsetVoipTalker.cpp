// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetVoipTalker.h"
#include "OnsetVoip.h"
#include "OnsetVoipSettings.h"
#include "OnsetVoipWorldSubsystem.h"
#include "OnsetVoipPawnInterface.h"
#include "OnsetVoipNameWidgetInterface.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Runtime/Launch/Resources/Version.h"

static FName Name_DynamicallyCreated(TEXT("DynamicallyCreated"));

void UOnsetVoipTalker::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		GetPlayerState()->OnPawnSet.AddUniqueDynamic(this, &UOnsetVoipTalker::OnPlayerStatePawnSet);
	}
}

bool UOnsetVoipTalker::IsTalking() const
{
	return bCachedTalking;
}

EOnsetVoipNetRelevancy UOnsetVoipTalker::GetLastVoipRelevancy() const
{
	return LastReceivedRelevancy;
}

float UOnsetVoipTalker::GetCurrentAmplitude() const
{
	return CachedAmplitude;
}

APlayerState* UOnsetVoipTalker::GetPlayerState() const
{
	return Cast<APlayerState>(GetOuter());
}

APawn* UOnsetVoipTalker::GetPlayerPawn() const
{
	if (const APlayerState* PlayerState = GetPlayerState())
	{
		if (APawn* Pawn = PlayerState->GetPawn())
		{
			return Pawn;
		}

		if (const APlayerController* PlayerController = Cast<APlayerController>(PlayerState->GetOwningController()))
		{
			return PlayerController->GetPawnOrSpectator();
		}

		if (const APlayerController* PlayerController = Cast<APlayerController>(PlayerState->GetOwner()))
		{
			return PlayerController->GetPawnOrSpectator();
		}
	}

	return nullptr;
}

void UOnsetVoipTalker::CancelReceivedAudioData()
{
	bCancelReceivedAudioData = true;
}

void UOnsetVoipTalker::SetMuted(bool bMute)
{
	bMuted = bMute;
}

bool UOnsetVoipTalker::IsMuted() const
{
	return bMuted;
}

void UOnsetVoipTalker::SetMutedForPlayerState(bool bMute, APlayerState* PlayerState)
{
	ensureMsgf(UKismetSystemLibrary::IsServer(this), TEXT("SetMutedForPlayerState function only has an effect when called on the server."));

	if (const UWorld* World = GEngine->GetWorldFromContextObject(PlayerState, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (const UOnsetVoipWorldSubsystem* Subsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>())
		{
			if (UOnsetVoipTalker* VoipTalker = Subsystem->GetVoipTalker(PlayerState))
			{
				SetMutedForVoipTalker(bMute, VoipTalker);
			}
		}
	}
}

void UOnsetVoipTalker::SetMutedForVoipTalker(bool bMute, UOnsetVoipTalker* OtherVoipTalker)
{
	ensureMsgf(UKismetSystemLibrary::IsServer(this), TEXT("SetMutedForVoipTalker function only has an effect when called on the server."));

	if (OtherVoipTalker)
	{
		if (bMute)
		{
			MutedVoipTalkers.AddUnique(OtherVoipTalker);
		}
		else
		{
			MutedVoipTalkers.Remove(OtherVoipTalker);
		}
	}
}

bool UOnsetVoipTalker::IsMutedForVoipTalker(UOnsetVoipTalker* OtherVoipTalker) const
{
	ensureMsgf(UKismetSystemLibrary::IsServer(this), TEXT("IsMutedForVoipTalker function only has an effect when called on the server."));

	return MutedVoipTalkers.Contains(OtherVoipTalker);
}

bool UOnsetVoipTalker::IsMutedForPlayerState(APlayerState* PlayerState) const
{
	ensureMsgf(UKismetSystemLibrary::IsServer(this), TEXT("IsMutedForPlayerState function only has an effect when called on the server."));

	if (const UWorld* World = GEngine->GetWorldFromContextObject(PlayerState, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (const UOnsetVoipWorldSubsystem* Subsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>())
		{
			if (UOnsetVoipTalker* VoipTalker = Subsystem->GetVoipTalker(PlayerState))
			{
				return IsMutedForVoipTalker(VoipTalker);
			}
		}
	}

	return false;
}

void UOnsetVoipTalker::SetVoipEnabled(bool bEnable)
{
	bVoipEnabled = bEnable;
}

bool UOnsetVoipTalker::IsVoipEnabled() const
{
	return bVoipEnabled;
}

bool UOnsetVoipTalker::SetVoiceChannel(int32 ChannelId, bool bAdd)
{
	ensureMsgf(UKismetSystemLibrary::IsServer(this), TEXT("SetVoiceChannel function only has an effect when called on the server."));

	if (ChannelId <= 0)
	{
		const bool bResult = bAdd ? !bVoiceWorldEnabled : bVoiceWorldEnabled;

		bVoiceWorldEnabled = bAdd;

		return bResult;
	}

	if (bAdd)
	{
		return VoiceChannelIds.AddUnique(ChannelId) != INDEX_NONE;
	}
	else
	{
		return VoiceChannelIds.Remove(ChannelId) != 0;
	}
}

bool UOnsetVoipTalker::IsInVoiceChannel(int32 ChannelId) const
{
	ensureMsgf(UKismetSystemLibrary::IsServer(this), TEXT("IsInVoiceChannel function only has an effect when called on the server."));

	if (ChannelId <= 0)
	{
		return bVoiceWorldEnabled;
	}

	return VoiceChannelIds.Find(ChannelId) != INDEX_NONE;
}

void UOnsetVoipTalker::SetMutedOnVoiceChannel(bool bMute, int32 ChannelId)
{
	ensureMsgf(UKismetSystemLibrary::IsServer(this), TEXT("SetMutedOnChannel function only has an effect when called on the server."));

	if (bMute)
	{
		MutedVoiceChannelIds.AddUnique(ChannelId);
	}
	else
	{
		MutedVoiceChannelIds.Remove(ChannelId);
	}
}

bool UOnsetVoipTalker::IsMutedOnVoiceChannel(int32 ChannelId) const
{
	ensureMsgf(UKismetSystemLibrary::IsServer(this), TEXT("IsMutedOnChannel function only has an effect when called on the server."));

	return MutedVoiceChannelIds.Contains(ChannelId);
}

UOnsetVoipAudioComponent* UOnsetVoipTalker::GetVoiceAudioComponent(EOnsetVoipNetRelevancy Relevancy)
{
	return InternalGetOrCreateAudioComponent(Relevancy);
}

void UOnsetVoipTalker::OnPlayerStatePawnSet(APlayerState* ChangedPlayerState, APawn* NewPlayerPawn, APawn* OldPlayerPawn)
{
	UE_LOG(LogOnsetVoip, Verbose, TEXT("%s::OnPlayerStatePawnSet(%s, %s, %s)"), *GetName(), *GetNameSafe(ChangedPlayerState), *GetNameSafe(NewPlayerPawn), *GetNameSafe(OldPlayerPawn));

#if !UE_SERVER
	checkf(ChangedPlayerState && ChangedPlayerState == GetPlayerState(), TEXT("OnPlayerStatePawnSet called with an invalid or different player state. %s != %s"), *GetNameSafe(GetPlayerState()), *GetNameSafe(ChangedPlayerState));

	if (CachedAudioComponent3D)
	{
		// Clear any remaining 3D voice data.
		CachedAudioComponent3D->VoipSoundWave->ResetAudioBuffer();

		// If this component was dynamically created by this voip talker object we destroy it and let the system create a new one in InternalGetOrCreateAudioComponent.
		// This ensures the OnVoipAudioComponentCreated interface function is called.
		if (CachedAudioComponent3D->ComponentHasTag(Name_DynamicallyCreated))
		{
			CachedAudioComponent3D->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			CachedAudioComponent3D->DestroyComponent();
		}

		// Reset cached audio component.
		CachedAudioComponent3D = nullptr;
	}

	if (NewPlayerPawn)
	{
		// Try to get an existing audio component from the new player pawn.
#if ENGINE_MINOR_VERSION <= 1
		CachedAudioComponent3D = Cast<UOnsetVoipAudioComponent>(NewPlayerPawn->GetComponentByClass(UOnsetVoipAudioComponent::StaticClass()));
#else
		CachedAudioComponent3D = NewPlayerPawn->GetComponentByClass<UOnsetVoipAudioComponent>();
#endif
	}

	if (CachedNameplateComponent)
	{
		// Inform nameplate component
		CachedNameplateComponent->SetPlayerState(nullptr);

		// Reset cached nameplate component.
		CachedNameplateComponent = nullptr;
	}

	// If there is a new player pawn for this player state and is not controlled by us then setup a nameplate component.
	if (NewPlayerPawn && !NewPlayerPawn->IsLocallyControlled())
	{
		// See if there is an existing nameplate component.
#if ENGINE_MINOR_VERSION <= 1
		CachedNameplateComponent = Cast<UOnsetNameplateComponent>(NewPlayerPawn->GetComponentByClass(UOnsetNameplateComponent::StaticClass()));
#else
		CachedNameplateComponent = NewPlayerPawn->GetComponentByClass<UOnsetNameplateComponent>();
#endif

		const UOnsetVoipSettings* VoipSettings = GetDefault<UOnsetVoipSettings>();
		const bool bNameplateRequirementsSet = VoipSettings->DefaultNameplateComponentClass.Get() != nullptr && VoipSettings->DefaultNameplateWidget.IsNull() == false;

		// If there is none, create a new one if it does not have the NoCreateNameplate tag, is not locally controlled, and we have a default nameplate widget set in the Onset VoIP settings.
		if (!CachedNameplateComponent &&
			!NewPlayerPawn->ActorHasTag((TEXT("NoCreateNameplate"))) &&
			bNameplateRequirementsSet)
		{
			CachedNameplateComponent = Cast<UOnsetNameplateComponent>(NewPlayerPawn->AddComponentByClass(VoipSettings->DefaultNameplateComponentClass, true, FTransform::Identity, true));
			checkf(CachedNameplateComponent, TEXT("Did you forget to set DefaultNameplateComponentClass in the project voip settings?"));
			CachedNameplateComponent->ComponentTags.Add(Name_DynamicallyCreated);

			const ACharacter* Character = Cast<ACharacter>(NewPlayerPawn);
			if (Character && Character->GetMesh())
			{
				USkeletalMeshComponent* CharacterMeshComponent = Character->GetMesh();

				static FNameplateAttachParam DefaultAttachParam;
				const FNameplateAttachParam* UseAttachParam = &DefaultAttachParam;
				for (const FNameplateAttachParam& AttachParam : VoipSettings->NameplateAttachParams)
				{
					if (CharacterMeshComponent->DoesSocketExist(AttachParam.AttachSocketName))
					{
						UseAttachParam = &AttachParam;
						break;
					}
				}

				CachedNameplateComponent->SetupAttachment(CharacterMeshComponent, UseAttachParam->AttachSocketName);

				NewPlayerPawn->FinishAddComponent(CachedNameplateComponent, true, FTransform::Identity);

				double AddHeight = 0.0;
				if (UseAttachParam->bAdjustAttachZAccordingToActorBounds)
				{
					AddHeight = CharacterMeshComponent->Bounds.BoxExtent.Z * 1.1;
				}

				if (UseAttachParam->AttachSocketName != NAME_None)
				{
					const FTransform ComponentTransform = CachedNameplateComponent->GetComponentTransform();
					const FTransform SocketTransform = CharacterMeshComponent->GetSocketTransform(UseAttachParam->AttachSocketName, ERelativeTransformSpace::RTS_World);

					const FVector NewRelativeLocation = SocketTransform.InverseTransformPosition(ComponentTransform.GetLocation() + FVector(0.0, 0.0, AddHeight) + UseAttachParam->RelativeAttachLocation);
					CachedNameplateComponent->SetRelativeLocation(NewRelativeLocation);
				}
				else
				{
					CachedNameplateComponent->AddRelativeLocation(FVector(0.0, 0.0, AddHeight));
				}
			}
			else
			{
				USceneComponent* AttachToComponent = nullptr;
#if ENGINE_MINOR_VERSION >= 3
				AttachToComponent = NewPlayerPawn->FindComponentByTag<USceneComponent>(TEXT("NameplateAttachComponent"));
#else
				const auto Components = NewPlayerPawn->GetComponentsByTag(USceneComponent::StaticClass(), TEXT("NameplateAttachComponent"));
				if (Components.Num() > 0)
				{
					AttachToComponent = Cast<USceneComponent>(Components[0]);
				}
#endif
				if (AttachToComponent == nullptr)
				{
					AttachToComponent = NewPlayerPawn->GetRootComponent();
				}
				CachedNameplateComponent->SetupAttachment(AttachToComponent);

				NewPlayerPawn->FinishAddComponent(CachedNameplateComponent, true, FTransform::Identity);

				FVector Origin, BoxExtent;
				NewPlayerPawn->GetActorBounds(false, Origin, BoxExtent, false);

				CachedNameplateComponent->SetRelativeLocation(FVector(0.0, 0.0, BoxExtent.Z * 1.1));
			}

			// SetWidgetClass will create a widget instance for us.
			CachedNameplateComponent->SetWidgetClass(VoipSettings->DefaultNameplateWidget.LoadSynchronous());
		}

		if (CachedNameplateComponent)
		{
			CachedNameplateComponent->SetPlayerState(GetPlayerState());
		}
	}
#endif /* !UE_SERVER */

	if (OldPlayerPawn && OldPlayerPawn->Implements<UOnsetVoipPawnInterface>())
	{
		IOnsetVoipPawnInterface::Execute_OnVoipTalkerChange(OldPlayerPawn, nullptr, nullptr);
	}

	if (NewPlayerPawn && NewPlayerPawn->Implements<UOnsetVoipPawnInterface>())
	{
		IOnsetVoipPawnInterface::Execute_OnVoipTalkerChange(NewPlayerPawn, GetPlayerState(), this);
	}
}

bool UOnsetVoipTalker::ShouldSendVoiceTo(const UOnsetVoipTalker* OtherVoipTalker) const
{
	return MutedVoipTalkers.Contains(OtherVoipTalker) == false;
}

#if ONSETVOIP_ENABLE_VOICE_PROCESSING
TSharedPtr<IVoiceDecoder> UOnsetVoipTalker::GetOrCreateVoiceDecoder()
{
	if (!VoiceDecoder.IsValid())
	{
		const int32 SampleRate = GetDefault<UOnsetVoipSettings>()->VoiceCaptureSampleRate;
		const int32 NumChannels = GetDefault<UOnsetVoipSettings>()->VoiceCaptureChannels;

		FrameByteSize = sizeof(int16) * NumChannels;

		VoiceDecoder = FVoiceModule::Get().CreateVoiceDecoder(SampleRate, NumChannels);
		checkf(VoiceDecoder.IsValid(), TEXT("Failed to create voice decoder (SampleRate: %i, NumChannels: %i). Possibly wrong settings or unsupported platform."), SampleRate, NumChannels);
	}

	return VoiceDecoder;
}

void UOnsetVoipTalker::ProcessVoiceData(const uint8* InAudioData, const int32 InAudioDataSize, EOnsetVoipNetRelevancy Relevancy, float InCurrentAmplitude, UOnsetVoipWorldSubsystem* OnsetVoipWorldSubsystem)
{
	if (IsMuted())
	{
		return;
	}

	if (!OnsetVoipWorldSubsystem)
	{
		OnsetVoipWorldSubsystem = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::Assert)->GetSubsystem<UOnsetVoipWorldSubsystem>();
	}

	TArray<uint8>& DecompressionBuffer = OnsetVoipWorldSubsystem->GetTemporaryDecompressionBuffer();
	uint32 DecompressedDataSize = DecompressionBuffer.Num(); // OnsetVoip::MaxDecompressedBufferSize

	// Decompress the voice data back to raw. CompressedDataSize will be overridden with the new decompressed size.
	GetOrCreateVoiceDecoder()->Decode(InAudioData, InAudioDataSize, DecompressionBuffer.GetData(), DecompressedDataSize);

	UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("Decoded %i bytes of compressed voice data to %u bytes. (%s)"), InAudioDataSize, DecompressedDataSize, *LexToString(Relevancy));

	if (UNLIKELY((!(DecompressedDataSize > 0))))
	{
		UE_LOG(LogOnsetVoip, Verbose, TEXT("The compressed voice data is zero."));
		return;
	}

	if (UNLIKELY(FrameByteSize <= 0 || (DecompressedDataSize % FrameByteSize) != 0))
	{
		UE_LOG(LogOnsetVoip, Verbose, TEXT("Decoded voice data is unaligned"));
		return;
	}

	const decltype(CachedAmplitude) OldAmplitude = CachedAmplitude;
	CachedAmplitude = InCurrentAmplitude;

	bCancelReceivedAudioData = false;

	if (OnVoipAudioDataReceived.IsBound() || OnsetVoipWorldSubsystem->OnVoipAudioDataReceived.IsBound())
	{
		TArray<uint8> RawPCMAudio;
		RawPCMAudio.AddUninitialized(DecompressedDataSize);
		FMemory::Memcpy(RawPCMAudio.GetData(), DecompressionBuffer.GetData(), DecompressedDataSize);

		OnVoipAudioDataReceived.Broadcast(this, RawPCMAudio, Relevancy);
		OnsetVoipWorldSubsystem->OnVoipAudioDataReceived.Broadcast(this, RawPCMAudio, Relevancy);
	}

	if (Native_OnVoipAudioDataReceived.IsBound() || OnsetVoipWorldSubsystem->Native_OnVoipAudioDataReceived.IsBound())
	{
		const TArrayView<uint8> AudioDataVew(DecompressionBuffer.GetData(), DecompressedDataSize);
		Native_OnVoipAudioDataReceived.Broadcast(this, AudioDataVew, Relevancy);
		OnsetVoipWorldSubsystem->Native_OnVoipAudioDataReceived.Broadcast(this, AudioDataVew, Relevancy);
	}

	if (bCancelReceivedAudioData)
	{
		// Cancel: CancelReceivedAudioData() was called
		CachedAmplitude = OldAmplitude;
		return;
	}

	ProcessOverrideVoiceRelevancy(Relevancy);

	TArray<UOnsetVoipAudioComponent*, TInlineAllocator<2>> ComponentsToPlayAudioOn;

	if (EnumHasAnyFlags(Relevancy, EOnsetVoipNetRelevancy::RELEVANT_2D))
	{
		ComponentsToPlayAudioOn.Add(InternalGetOrCreateAudioComponent(EOnsetVoipNetRelevancy::RELEVANT_2D));
	}

	if (EnumHasAnyFlags(Relevancy, EOnsetVoipNetRelevancy::RELEVANT_3D))
	{
		ComponentsToPlayAudioOn.Add(InternalGetOrCreateAudioComponent(EOnsetVoipNetRelevancy::RELEVANT_3D));
	}

	for (UOnsetVoipAudioComponent* AudioComponent : ComponentsToPlayAudioOn)
	{
		if (AudioComponent != nullptr && AudioComponent->VoipSoundWave != nullptr)
		{
			AudioComponent->VoipSoundWave->AddAudioData(DecompressionBuffer.GetData(), DecompressedDataSize);

			if (!AudioComponent->IsPlaying())
			{
				UE_LOG(LogOnsetVoip, Verbose, TEXT("Playing audio component %s."), *AudioComponent->GetName());

				AudioComponent->Play();
				AudioComponent->SetComponentTickEnabled(true);
			}
		}
	}

	LastReceivedRelevancy = Relevancy;
	LastReceivedVoipData = FPlatformTime::Seconds();

	if (!bCachedTalking)
	{
		InternalSetTalkingState(true);
	}
}

void UOnsetVoipTalker::ProcessVoiceData(const FOnsetVoicePacketWrapper& PacketWrapper, UOnsetVoipWorldSubsystem* OnsetVoipWorldSubsystem)
{
	ProcessVoiceData(PacketWrapper.VoipPacket->Buffer.GetData(), PacketWrapper.VoipPacket->Buffer.Num(), PacketWrapper.Relevancy, PacketWrapper.VoipPacket->Amplitude, OnsetVoipWorldSubsystem);
}

void UOnsetVoipTalker::ProcessOverrideVoiceRelevancy(EOnsetVoipNetRelevancy& Relevancy)
{
	if (EnumHasAnyFlags(Relevancy, EOnsetVoipNetRelevancy::RELEVANT_2D))
	{
		if (bEnable2DVoicePlaybackOnBoth2DAnd3D)
		{
			EnumAddFlags(Relevancy, EOnsetVoipNetRelevancy::RELEVANT_3D);
		}

		if (bEnablePrefer2DVoicePlaybackOn3DIfNear)
		{
			if (GEngine)
			{
				if (const APlayerController* LocalPlayerController = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::Assert)->GetFirstPlayerController())
				{
					if (const APawn* RemotePawn = GetPlayerPawn())
					{
						FVector ListenLocation, Unused0, Unused1;
						LocalPlayerController->GetAudioListenerPosition(ListenLocation, Unused0, Unused1);

						const double AudibleDistance = PlaybackDistance2DVoicePlaybackOn3DIfNear * PlaybackDistance2DVoicePlaybackOn3DIfNear;
						if ((ListenLocation - RemotePawn->GetActorLocation()).SizeSquared() < AudibleDistance)
						{
							EnumRemoveFlags(Relevancy, EOnsetVoipNetRelevancy::RELEVANT_2D);
							EnumAddFlags(Relevancy, EOnsetVoipNetRelevancy::RELEVANT_3D);
						}
					}
				}
			}
		}
	}
}

void UOnsetVoipTalker::UpdateTalkingState(const double& CurrentTime)
{
	if (bCachedTalking &&
		((CurrentTime - LastReceivedVoipData) >= OnsetVoip::TimeRemoteTalkerConsideredStoppedTalking))
	{
		InternalSetTalkingState(false);
	}
}

void UOnsetVoipTalker::InternalSetTalkingState(bool bInNewTalking)
{
	bCachedTalking = bInNewTalking;

	OnVoipTalkingStateChange.Broadcast(this, bInNewTalking, LastReceivedRelevancy);

	if (const UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (const UOnsetVoipWorldSubsystem* Subsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>())
		{
			Subsystem->OnVoipTalkingStateChange.Broadcast(this, bInNewTalking, LastReceivedRelevancy);
		}
	}

	if (APawn* PlayerPawn = GetPlayerState()->GetPawn())
	{
		if (PlayerPawn->Implements<UOnsetVoipPawnInterface>())
		{
			IOnsetVoipPawnInterface::Execute_OnTalkingStateChange(PlayerPawn, bInNewTalking, LastReceivedRelevancy);
		}
	}
}
#endif /* ONSETVOIP_ENABLE_VOICE_PROCESSING */

UOnsetVoipAudioComponent* UOnsetVoipTalker::InternalGetOrCreateAudioComponent(EOnsetVoipNetRelevancy Relevancy)
{
#if !UE_SERVER
	if (EnumHasAnyFlags(Relevancy, EOnsetVoipNetRelevancy::RELEVANT_2D))
	{
		if (!AudioComponent2D)
		{
			UClass* AudioComponentClass = nullptr;

			if (DefaultOnsetVoipAudioComponentClass2D.Get())
			{
				AudioComponentClass = DefaultOnsetVoipAudioComponentClass2D.Get();
			}
			else if (GetDefault<UOnsetVoipSettings>()->DefaultOnsetVoipAudioComponentClass2D.Get())
			{
				AudioComponentClass = GetDefault<UOnsetVoipSettings>()->DefaultOnsetVoipAudioComponentClass2D.Get();
			}
			else
			{
				AudioComponentClass = UOnsetVoipAudioComponent2D::StaticClass();
			}

			AudioComponent2D = NewObject<UOnsetVoipAudioComponent2D>(this, AudioComponentClass);
			UE_LOG(LogOnsetVoip, Verbose, TEXT("Created 2D audio component %s for %s."), *AudioComponent2D->GetPathName(), *GetPathName());

			if (APawn* PlayerPawn = GetPlayerState()->GetPawn())
			{
				if (PlayerPawn->Implements<UOnsetVoipPawnInterface>())
				{
					IOnsetVoipPawnInterface::Execute_OnVoipAudioComponentCreated(PlayerPawn, AudioComponent2D);
				}
			}
		}

		return AudioComponent2D;
	}
	else if (EnumHasAnyFlags(Relevancy, EOnsetVoipNetRelevancy::RELEVANT_3D))
	{
		if (!CachedAudioComponent3D)
		{
			if (APawn* PlayerPawn = GetPlayerState()->GetPawn())
			{
				UClass* AudioComponentClass = nullptr;

				if (DefaultOnsetVoipAudioComponentClass3D.Get())
				{
					AudioComponentClass = DefaultOnsetVoipAudioComponentClass3D.Get();
				}
				else if (GetDefault<UOnsetVoipSettings>()->DefaultOnsetVoipAudioComponentClass3D.Get())
				{
					AudioComponentClass = GetDefault<UOnsetVoipSettings>()->DefaultOnsetVoipAudioComponentClass3D.Get();
				}
				else
				{
					AudioComponentClass = UOnsetVoipAudioComponent::StaticClass();
				}

				CachedAudioComponent3D = CastChecked<UOnsetVoipAudioComponent>(PlayerPawn->AddComponentByClass(AudioComponentClass, false, FTransform::Identity, false));
				UE_LOG(LogOnsetVoip, Verbose, TEXT("Created 3D audio component %s for %s."), *CachedAudioComponent3D->GetPathName(), *GetPathName());
				// A tag is added if someone wants to know if this voice component was created dynamically by this talker object.
				// If the pawn/character class already has a voice component added manually it will be looked for and "cached" in OnPlayerStatePawnSet.
				CachedAudioComponent3D->ComponentTags.Add(Name_DynamicallyCreated);

				if (PlayerPawn->Implements<UOnsetVoipPawnInterface>())
				{
					IOnsetVoipPawnInterface::Execute_OnVoipAudioComponentCreated(PlayerPawn, CachedAudioComponent3D);
				}
			}
		}

		return CachedAudioComponent3D;
	}
#endif
	return nullptr;
}
