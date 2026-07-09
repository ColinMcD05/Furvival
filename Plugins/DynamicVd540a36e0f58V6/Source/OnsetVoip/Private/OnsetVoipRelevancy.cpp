// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetVoipRelevancy.h"
#include "OnsetVoipSettings.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpectatorPawn.h"

static APlayerController* GetOwningPlayerController(const APlayerState* PlayerState)
{
	if (PlayerState == nullptr)
	{
		return nullptr;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(PlayerState->GetOwningController()))
	{
		return PlayerController;
	}

	return Cast<APlayerController>(PlayerState->GetOwner());
}

static bool TryGetTalkerWorldLocation(const UOnsetVoipTalker* Talker, const APlayerController* PreferredPlayerController, FVector& OutWorldLocation)
{
	if (PreferredPlayerController != nullptr)
	{
		OutWorldLocation = PreferredPlayerController->GetFocalLocation();
		return true;
	}

	if (const AActor* VoiceActor = Talker->GetPlayerPawn())
	{
		OutWorldLocation = VoiceActor->GetActorLocation();
		return true;
	}

	if (const APlayerController* PlayerController = GetOwningPlayerController(Talker->GetPlayerState()))
	{
		OutWorldLocation = PlayerController->GetFocalLocation();
		return true;
	}

	return false;
}

const FString LexToString(EOnsetVoipNetRelevancy Relevancy)
{
	if (Relevancy == EOnsetVoipNetRelevancy::RELEVANT_NONE)
	{
		return "None";
	}

	FString Result;
#define RELEVANCY_STATE(StatusFlag, DisplayName) if (EnumHasAnyFlags(Relevancy, StatusFlag)) Result += (FString(DisplayName) + TEXT("|"));
	RELEVANCY_STATE(EOnsetVoipNetRelevancy::RELEVANT_2D, TEXT("2D"));
	RELEVANCY_STATE(EOnsetVoipNetRelevancy::RELEVANT_3D, TEXT("3D"));
#undef RELEVANCY_STATE

	Result.RemoveFromEnd(TEXT("|"));
	return Result;
}

FOnsetVoipNetRelevancy::FOnsetVoipNetRelevancy()
{
	bUseDistanceBasedRelevancy = GetDefault<UOnsetVoipSettings>()->bUseDistanceBasedRelevancy;
	ReplicationDistanceSquared = GetDefault<UOnsetVoipSettings>()->MaxReplicationDistance;
	ReplicationDistanceSquared *= ReplicationDistanceSquared;
}

bool FOnsetVoipNetRelevancy::DoTalkersShareAnyVoiceChannel(const UOnsetVoipTalker* FromVoipTalker, const UOnsetVoipTalker* ToVoipTalker) const
{
	for (const int32 ChannelId : ToVoipTalker->VoiceChannelIds)
	{
		if (FromVoipTalker->VoiceChannelIds.Contains(ChannelId))
		{
			if (!FromVoipTalker->IsMutedOnVoiceChannel(ChannelId))
			{
				return true;
			}
		}
	}
	return false;
}

bool FOnsetVoipNetRelevancy::DoTalkersShareVoiceChannel(const UOnsetVoipTalker* FromVoipTalker, const UOnsetVoipTalker* ToVoipTalker, const int32 VoiceChannelId)
{
	return FromVoipTalker->IsInVoiceChannel(VoiceChannelId) && ToVoipTalker->IsInVoiceChannel(VoiceChannelId) && !FromVoipTalker->IsMutedOnVoiceChannel(VoiceChannelId);
}

bool FOnsetVoipNetRelevancy::IsVoiceWorldRelevant(const UOnsetVoipTalker* FromVoipTalker, const UOnsetVoipTalker* ToVoipTalker, UNetConnection* ToConnection)
{
	if (ToConnection != nullptr)
	{
		// See if receiving connection is aware of the sending player state.
		const TWeakObjectPtr<AActor> PlayerStateActor(FromVoipTalker->GetPlayerState());
		if (!ToConnection->ContainsActorChannel(PlayerStateActor))
		{
			return false;
		}

		// If sender has a replicated world actor, ensure the receiver currently has a channel for it.
		if (AActor* VoiceActor = FromVoipTalker->GetPlayerPawn())
		{
			const bool bShouldCheckActorChannelForVoiceActor = (
				VoiceActor->IsA(ASpectatorPawn::StaticClass()) == false &&
				VoiceActor->GetIsReplicated() &&
				VoiceActor->bOnlyRelevantToOwner == false
				);

			if (bShouldCheckActorChannelForVoiceActor)
			{
				const TWeakObjectPtr<AActor> VoiceActorWeak(VoiceActor);
				if (!ToConnection->ContainsActorChannel(VoiceActorWeak))
				{
					return false;
				}
			}
		}
	}

	// See if sender is muted on channel 0
	if (FromVoipTalker->IsMutedOnVoiceChannel(0))
	{
		return false;
	}

	// See if both are in channel 0
	if (!FromVoipTalker->IsInVoiceChannel(0) || !ToVoipTalker->IsInVoiceChannel(0))
	{
		return false;
	}

	// Check distances between both if enabled
	if (bUseDistanceBasedRelevancy)
	{
		FVector FromWorldLocation;
		if (!TryGetTalkerWorldLocation(FromVoipTalker, nullptr, FromWorldLocation))
		{
			return false;
		}

		const APlayerController* ListeningPlayerController = ToConnection ? ToConnection->PlayerController : nullptr;
		FVector ToWorldLocation;
		if (!TryGetTalkerWorldLocation(ToVoipTalker, ListeningPlayerController, ToWorldLocation))
		{
			return false;
		}

		if ((FromWorldLocation - ToWorldLocation).SizeSquared() > ReplicationDistanceSquared)
		{
			return false;
		}
	}

	return true;
}

EOnsetVoipNetRelevancy FOnsetVoipNetRelevancy::GetVoiceNetRelevancy(const UOnsetVoipTalker* FromVoipTalker, const UOnsetVoipTalker* ToVoipTalker, UNetConnection* ToConnection, const FOnsetVoipPacketPtr& VoipPacket)
{
	// Early out if required.
	if (!FromVoipTalker->ShouldSendVoiceTo(ToVoipTalker))
	{
		return EOnsetVoipNetRelevancy::RELEVANT_NONE;
	}

	// Check if the client wants to speak on specific channels only.
	if (VoipPacket->bClientHintVoiceChannelId)
	{
		bool bHintWorldChannel = false;

		for (const int32 ChannelId : VoipPacket->ClientHintVoiceChannels)
		{
			if (ChannelId <= 0)
			{
				// Remember that the client hinted at the world channel (0). Continue checking for 2D voice channels, as they have a higher priority.
				bHintWorldChannel = true;
				continue;
			}
			else
			{
				// If we match a channel, we are relevant in 2D.
				if (DoTalkersShareVoiceChannel(FromVoipTalker, ToVoipTalker, ChannelId))
				{
					return EOnsetVoipNetRelevancy::RELEVANT_2D;
				}
			}
		}

		// If it gets here, we did not find a 2D voice channel. See whether the world channel is relevant; otherwise, exit with no relevancy.
		return (bHintWorldChannel && IsVoiceWorldRelevant(FromVoipTalker, ToVoipTalker, ToConnection)) ? EOnsetVoipNetRelevancy::RELEVANT_3D : EOnsetVoipNetRelevancy::RELEVANT_NONE;
	}

	return DoTalkersShareAnyVoiceChannel(FromVoipTalker, ToVoipTalker) ? EOnsetVoipNetRelevancy::RELEVANT_2D : (IsVoiceWorldRelevant(FromVoipTalker, ToVoipTalker, ToConnection) ? EOnsetVoipNetRelevancy::RELEVANT_3D : EOnsetVoipNetRelevancy::RELEVANT_NONE);
}
