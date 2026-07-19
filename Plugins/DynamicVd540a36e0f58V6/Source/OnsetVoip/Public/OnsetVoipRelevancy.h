// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IOnsetVoipRelevancy.h"

/**
 * Default relevancy implementation.
 */
class ONSETVOIP_API FOnsetVoipNetRelevancy : public IOnsetVoipNetRelevancy
{
public:
	FOnsetVoipNetRelevancy();

	// Returns true if both talkers share any voice channel.
	virtual bool DoTalkersShareAnyVoiceChannel(const UOnsetVoipTalker* FromVoipTalker, const UOnsetVoipTalker* ToVoipTalker) const;

	// Returns true if both talkers share a specific channel.
	virtual bool DoTalkersShareVoiceChannel(const UOnsetVoipTalker* FromVoipTalker, const UOnsetVoipTalker* ToVoipTalker, const int32 VoiceChannelId);

	// Returns true if both talkers are relevant in the world. ToConnection is used to check whether the receiver has an open actor channel to FromVoipTalker. If not, they are probably not relevant due to net cull distance.
	virtual bool IsVoiceWorldRelevant(const UOnsetVoipTalker* FromVoipTalker, const UOnsetVoipTalker* ToVoipTalker, UNetConnection* ToConnection);

	// Determines the voice relevancy between two talkers.
	virtual EOnsetVoipNetRelevancy GetVoiceNetRelevancy(const UOnsetVoipTalker* FromVoipTalker, const UOnsetVoipTalker* ToVoipTalker, UNetConnection* ToConnection, const FOnsetVoipPacketPtr& VoipPacket) override;

protected:
	// See UOnsetVoipSettings::bUseDistanceBasedRelevancy
	bool bUseDistanceBasedRelevancy;
	double ReplicationDistanceSquared;
};
