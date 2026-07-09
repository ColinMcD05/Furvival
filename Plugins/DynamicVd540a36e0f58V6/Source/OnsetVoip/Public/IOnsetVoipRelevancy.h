// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnsetVoipTalker.h"
#include "OnsetVoipPacket.h"
#include "OnsetVoipRelevancyDefinitions.h"

/**
 * Base interface for determining voice relevancy between two talkers. See FOnsetVoipNetRelevancy for the default implementation.
 */
class ONSETVOIP_API IOnsetVoipNetRelevancy : public FNoncopyable
{
public:
	virtual ~IOnsetVoipNetRelevancy() { }

	// Determines the voice relevancy between two talkers.
	virtual EOnsetVoipNetRelevancy GetVoiceNetRelevancy(const UOnsetVoipTalker* FromVoipTalker, const UOnsetVoipTalker* ToVoipTalker, UNetConnection* ToConnection, const FOnsetVoipPacketPtr& VoipPacket) = 0;
};
