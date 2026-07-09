// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "OnsetStatVoipSubsystem.generated.h"

/**
 * Used for registering 'stat voip'
 */
UCLASS(NotBlueprintType)
class UOnsetStatVoipSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UEngineSubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection);
	virtual void Deinitialize();
	//~ End UEngineSubsystem Interface
};
