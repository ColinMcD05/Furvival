// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "BlueprintGameModeEvents.h"

void UBlueprintGameModeEvents::Initialize(FSubsystemCollectionBase& Collection)
{
	FGameModeEvents::OnGameModePreLoginEvent().AddUObject(this, &UBlueprintGameModeEvents::Native_OnGameModePreLoginEvent);
	FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &UBlueprintGameModeEvents::Native_OnGameModePostLoginEvent);
	FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &UBlueprintGameModeEvents::Native_OnGameModeLogoutEvent);
}

void UBlueprintGameModeEvents::OnGameModePreLoginEvent(FBlueprintGameModePreLoginEvent EventHandler)
{
	BlueprintGameModePostLoginEventArray.Add(EventHandler);
}

void UBlueprintGameModeEvents::Native_OnGameModePreLoginEvent(AGameModeBase* GameMode, const FUniqueNetIdRepl& NewPlayer, FString& ErrorMessage)
{
	for (auto It = BlueprintGameModePostLoginEventArray.CreateIterator(); It; ++It)
	{
		if ((*It).IsBound())
		{
			FString Error;
			(*It).Execute(GameMode, NewPlayer, Error);

			if (Error.Len() > 0)
			{
				ErrorMessage = Error;
			}
		}
		else
		{
			It.RemoveCurrent();
		}
	}
}

void UBlueprintGameModeEvents::Native_OnGameModePostLoginEvent(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	OnGameModePostLoginEvent.Broadcast(GameMode, NewPlayer);
}

void UBlueprintGameModeEvents::Native_OnGameModeLogoutEvent(AGameModeBase* GameMode, AController* Exiting)
{
	OnGameModeLogoutEvent.Broadcast(GameMode, Exiting);
}
