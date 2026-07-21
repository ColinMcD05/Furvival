// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define ONSET_VOIP_VERSION "1.8.5"

DECLARE_LOG_CATEGORY_EXTERN(LogOnsetVoip, Log, All);

namespace OnsetVoip
{
	ONSETVOIP_API void ConsoleLog(const UObject* WorldContextObject, const FString& Message, bool bUELOG = true);

	ONSETVOIP_API int32 AlignToFrameBoundary(int32 NumBytes, int32 FrameByteSize);
}

class FOnsetVoipModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface
};
