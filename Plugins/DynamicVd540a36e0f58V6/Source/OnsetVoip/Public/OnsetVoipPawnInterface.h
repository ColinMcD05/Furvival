// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnsetVoipRelevancyDefinitions.h"
#include "UObject/Interface.h"
#include "OnsetVoipPawnInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UOnsetVoipPawnInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ONSETVOIP_API IOnsetVoipPawnInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.

public:
	// Called on a pawn/character if the owning VoIP starts or stops talking.
	UFUNCTION(BlueprintImplementableEvent, Category = "OnsetVoip")
	void OnTalkingStateChange(bool bIsTalking, EOnsetVoipNetRelevancy Relevancy);

	// Called on a pawn/character when another player state is assigned and therefore the voip talker changes. Basically a new possession.
	UFUNCTION(BlueprintImplementableEvent, Category = "OnsetVoip")
	void OnVoipTalkerChange(APlayerState* PlayerState, UOnsetVoipTalker* OnsetVoipTalker);

	// Called on a pawn/character when an audio component was dynamically created for 2D/3D voice. Can be used to customize settings like SourceEffectChain or AttenuationSettings.
	UFUNCTION(BlueprintImplementableEvent, Category = "OnsetVoip")
	void OnVoipAudioComponentCreated(UOnsetVoipAudioComponent* VoipAudioComponent);
};
