// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFramework/PlayerState.h"
#include "OnsetVoipNameWidgetInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UOnsetVoipNameWidgetInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ONSETVOIP_API IOnsetVoipNameWidgetInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.

public:
	/*
	* This will be called with a player state once this nameplate becomes relevant(possessed). Null will be passed when the owning pawn is no longer possessed.
	* To know whether to show a talking icon you can use UOnsetVoipWorldSubsystem to get the UOnsetVoipTalker from the PlayerState and call the function IsTalking() on it.
	*/
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "OnsetVoip")
	void SetOwningPlayerState(APlayerState* PlayerState);

	// Implement this to return the saved player state from SetOwningPlayerState
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "OnsetVoip")
	APlayerState* GetOwningPlayerState();
};
