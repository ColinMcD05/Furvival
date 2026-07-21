// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetStatVoipSubsystem.h"
#include "Runtime/Launch/Resources/Version.h"
#include "OnsetVoip.h"
#if !UE_SERVER
#include "OnsetVoipDataChannel.h"
#include "OnsetVoipLocalPlayerSubsystem.h"
#include "OnsetVoipWorldSubsystem.h"
#include "Engine/NetDriver.h"
#include "Engine/Font.h"
#include "UnrealClient.h"
#include "CanvasTypes.h"
#endif

#if !UE_SERVER
int32 RenderStatOnsetVoip(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	if (World &&
		(World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game))
	{
		const UFont* Font = FPlatformProperties::SupportsWindowedMode() ? GEngine->GetSmallFont() : GEngine->GetMediumFont();
		const int32 RowHeight = FMath::TruncToInt(Font->GetMaxCharHeight() * 1.1f);

		Canvas->DrawShadowedString(
			X,
			Y,
			*FString::Printf(TEXT("OnsetVoip\n=========\nNetMode: %s\nWorld: %s"), *ToString(World->GetNetMode()), *World->GetPathName()),
			Font,
			FColor::White
		);
		Y += RowHeight * 4;

		if (const UNetDriver* NetDriver = World->GetNetDriver())
		{
			int64 TotalAverageBytesSentReceived[2] = { 0 };
			if (NetDriver->ServerConnection)
			{
				// On a client show data for our connection to the server.
				if (UOnsetVoipDataChannel* VoipChannel = UOnsetVoipDataChannel::GetOnsetVoipChannel(NetDriver, NetDriver->ServerConnection))
				{
					VoipChannel->CalculateAverageThroughput(TotalAverageBytesSentReceived[0], TotalAverageBytesSentReceived[1]);
				}
			}
			else
			{
				// On a listen server show data for all connections (all connected clients).
				for (const UNetConnection* Connection : NetDriver->ClientConnections)
				{
					if (UOnsetVoipDataChannel* VoipChannel = UOnsetVoipDataChannel::GetOnsetVoipChannel(NetDriver, Connection))
					{
						int64 AverageBytesSentReceived[2] = { 0 };
						VoipChannel->CalculateAverageThroughput(AverageBytesSentReceived[0], AverageBytesSentReceived[1]);
						TotalAverageBytesSentReceived[0] += AverageBytesSentReceived[0];
						TotalAverageBytesSentReceived[1] += AverageBytesSentReceived[1];
					}
				}
			}

			Canvas->DrawShadowedString(
				X,
				Y,
				*FString::Printf(TEXT("Outgoing: %.2f KB/s\nIncoming: %.2f KB/s"), (TotalAverageBytesSentReceived[0] / 1000.0), (TotalAverageBytesSentReceived[1] / 1000.0)),
				Font,
				FColor::White
			);
			Y += RowHeight * 2;
		}

		if (const ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController())
		{
			/*Canvas->DrawShadowedString(
				X,
				Y,
				*FString::Printf(TEXT("Our PlayerController: %s"), *GetPathNameSafe(LocalPlayer->PlayerController)),
				Font,
				FColor::White
			);
			Y += RowHeight;
			Canvas->DrawShadowedString(
				X,
				Y,
				*FString::Printf(TEXT("Our PlayerState: [%i]%s"),
					(LocalPlayer->PlayerController && LocalPlayer->PlayerController->PlayerState) ? LocalPlayer->PlayerController->PlayerState->GetPlayerId() : 0,
					LocalPlayer->PlayerController ? *GetPathNameSafe(LocalPlayer->PlayerController->PlayerState) : TEXT("None")
					),
				Font,
				FColor::White
			);
			Y += RowHeight;
			Canvas->DrawShadowedString(
				X,
				Y,
				*FString::Printf(TEXT("Our PawnOrSpectator: %s"), LocalPlayer->PlayerController ? *GetPathNameSafe(LocalPlayer->PlayerController->GetPawnOrSpectator()) : TEXT("None")),
				Font,
				FColor::White
			);
			Y += RowHeight;*/

			if (const UOnsetVoipLocalPlayerSubsystem* OnsetLocalPlayerSubsystem = LocalPlayer->GetSubsystem<UOnsetVoipLocalPlayerSubsystem>())
			{
				Canvas->DrawShadowedString(
					X,
					Y,
					*FString::Printf(TEXT("Microphone capture: %s"), OnsetLocalPlayerSubsystem->IsCapturingVoice() ? TEXT("enabled") : TEXT("disabled")),
					Font,
					OnsetLocalPlayerSubsystem->IsCapturingVoice() ? FColor::Green : FColor::White
				);
				Y += RowHeight;
				Canvas->DrawShadowedString(
					X,
					Y,
					*FString::Printf(TEXT("Microphone voice detected: %s"), OnsetLocalPlayerSubsystem->IsTalking() ? TEXT("yes") : TEXT("no")),
					Font,
					OnsetLocalPlayerSubsystem->IsTalking() ? FColor::Green : FColor::White
				);
				Y += RowHeight;
			}
		}

		if (const UOnsetVoipWorldSubsystem* WorldSubsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>())
		{
			for (const auto& Entry : *WorldSubsystem->GetVoipTalkerMap())
			{
				const APlayerState* PlayerState = Entry.Key;
				const UOnsetVoipTalker* Talker = Entry.Value;

				if (Talker->IsTalking())
				{
					Canvas->DrawShadowedString(
						X,
						Y,
						*FString::Printf(TEXT("[%i]%s is speaking"), PlayerState->GetPlayerId(), *PlayerState->GetPlayerName()),
						Font,
						FColor::Silver
					);
					Y += RowHeight;
				}

#if 0
				// This is for debugging nameplate visibility. See CachedComponentToTestRenderTime for more information.
				if (APawn* Pawn = PlayerState->GetPawn())
				{
					if (UOnsetNameplateComponent* Nameplate = Pawn->FindComponentByClass<UOnsetNameplateComponent>())
					{
						if (UPrimitiveComponent* PrimComp = Nameplate->GetCachedComponentToTestRenderTime())
						{
							Canvas->DrawShadowedString(
								X,
								Y,
								*FString::Printf(TEXT("[%i](%s): %f"), PlayerState->GetPlayerId()/*, *Pawn->GetPathName()*/, *PrimComp->GetPathName(), PrimComp->GetLastRenderTimeOnScreen()),
								Font,
								FColor::Silver
							);
							Y += RowHeight;
						}
					}
				}
#endif
			}
		}
	}

	return Y;
}

bool ToggleStatOnsetVoip(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	if (ViewportClient && World && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game))
	{
		return true;
	}

	return false;
}

#endif /* #if !UE_SERVER */

void UOnsetStatVoipSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
#if !UE_SERVER
	if (GEngine)
	{
		GEngine->AddEngineStat(TEXT("STAT_VOIP"), TEXT("STATCAT_Engine"), FText::FromString(TEXT("Displays network statistics for OnsetVoip.")),
			UEngine::FEngineStatRender::CreateStatic(&RenderStatOnsetVoip), UEngine::FEngineStatToggle::CreateStatic(&ToggleStatOnsetVoip), false);
	}
#endif
}

void UOnsetStatVoipSubsystem::Deinitialize()
{
#if !UE_SERVER
	if (GEngine)
	{
		GEngine->RemoveEngineStat(TEXT("STAT_VOIP"));
	}
#endif
}
