// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "BlueprintGameModeEvents.generated.h"


DECLARE_DYNAMIC_DELEGATE_ThreeParams(FBlueprintGameModePreLoginEvent, AGameModeBase*, GameMode, const FUniqueNetIdRepl&, NewPlayer, FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBlueprintGameModePostLoginEvent, AGameModeBase*, GameMode, APlayerController*, NewPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBlueprintGameModeLogoutEvent, AGameModeBase*, GameMode, AController*, Exiting);

/**
 * Exposes FGameModeEvents to blueprints.
 */
UCLASS()
class ONSETVOIP_API UBlueprintGameModeEvents : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Gamemode Events")
	void OnGameModePreLoginEvent(FBlueprintGameModePreLoginEvent EventHandler);

	UPROPERTY(BlueprintAssignable, Category = "Gamemode Events")
	FBlueprintGameModePostLoginEvent OnGameModePostLoginEvent;

	UPROPERTY(BlueprintAssignable, Category = "Gamemode Events")
	FBlueprintGameModeLogoutEvent OnGameModeLogoutEvent;

	void Native_OnGameModePreLoginEvent(AGameModeBase* GameMode, const FUniqueNetIdRepl& NewPlayer, FString& ErrorMessage);
	void Native_OnGameModePostLoginEvent(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void Native_OnGameModeLogoutEvent(AGameModeBase* GameMode, AController* Exiting);

protected:
	UPROPERTY(Transient)
	TArray<FBlueprintGameModePreLoginEvent> BlueprintGameModePostLoginEventArray;
};
