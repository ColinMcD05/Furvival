// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnsetVoipRelevancyDefinitions.generated.h"

// Number of required bits to serialize EOnsetVoipNetRelevancy. Don't forget to change if you extend the enum.
#define ONSET_VOICE_NET_RELEVANCY_NUM_BITS 2

// Different possible relevancy states between two talkers/players.
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EOnsetVoipNetRelevancy : uint8
{
	// Not relevant at all.
	RELEVANT_NONE = 0,

	// Relevant in 2D space. (voice channels)
	RELEVANT_2D = 1 << 0,

	// Relevant in 3D space. (world channel 0)
	RELEVANT_3D = 1 << 1,
};

ENUM_CLASS_FLAGS(EOnsetVoipNetRelevancy);

ONSETVOIP_API const FString LexToString(EOnsetVoipNetRelevancy Relevancy);
