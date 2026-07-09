// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetVoip.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/Console.h"

DEFINE_LOG_CATEGORY(LogOnsetVoip);

void OnsetVoip::ConsoleLog(const UObject* WorldContextObject, const FString& Message, bool bUELOG)
{
	if (GEngine)
	{
		if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
		{
			if (const UGameViewportClient* GameViewport = World->GetGameViewport())
			{
				if (UConsole* Console = GameViewport->ViewportConsole)
				{
					Console->OutputText(Message);
				}
			}
		}
	}

	UE_CLOG(bUELOG, LogOnsetVoip, Log, TEXT("%s"), *Message);
}

int32 OnsetVoip::AlignToFrameBoundary(int32 NumBytes, int32 FrameByteSize)
{
	return (FrameByteSize > 0) ? (NumBytes - (NumBytes % FrameByteSize)) : NumBytes;
}

#define LOCTEXT_NAMESPACE "FOnsetVoipModule"

void FOnsetVoipModule::StartupModule()
{
	UE_LOG(LogOnsetVoip, Log, TEXT("FOnsetVoipModule::StartupModule()"));
	UE_LOG(LogOnsetVoip, Log, TEXT("Version %hs"), ONSET_VOIP_VERSION);
}

void FOnsetVoipModule::ShutdownModule()
{
	UE_LOG(LogOnsetVoip, Log, TEXT("FOnsetVoipModule::ShutdownModule()"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FOnsetVoipModule, OnsetVoip)
