// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetVoipDataChannel.h"
#include "OnsetVoip.h"
#include "OnsetVoipWorldSubsystem.h"
#include "OnsetVoipRelevancy.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/WeakObjectPtrTemplates.h"

FName OnsetVoipChannelName(TEXT("OnsetVoip"));

FORCEINLINE UOnsetVoipDataChannel* StaticGetOnsetVoipChannel(const UNetDriver* NetDriver, const UNetConnection* NetConn)
{
	return Cast<UOnsetVoipDataChannel>(NetConn->Channels[NetDriver->ChannelDefinitionMap[OnsetVoipChannelName].StaticChannelIndex]);
}

UOnsetVoipDataChannel* UOnsetVoipDataChannel::GetOnsetVoipChannel(const UNetDriver* NetDriver, const UNetConnection* NetConn)
{
	return StaticGetOnsetVoipChannel(NetDriver, NetConn);
}

// Function only called on servers to replicate packets to relevant clients.
FORCEINLINE void ServerReplicateVoipPacket(const UNetDriver* NetDriver, const UOnsetVoipWorldSubsystem* VoipWorldSubsystem, const UOnsetVoipTalker* FromVoipTalker, const FOnsetVoipPacketPtr& VoipPacket, const UNetConnection* FromConnection)
{
	for (UNetConnection* ToConnection : NetDriver->ClientConnections)
	{
		if (ToConnection == nullptr ||
			ToConnection->PlayerController == nullptr ||
			ToConnection == FromConnection)
		{
			continue;
		}

		const UOnsetVoipTalker* ToVoipTalker = VoipWorldSubsystem->GetVoipTalker(ToConnection->PlayerController->PlayerState);
		if (UNLIKELY(ToVoipTalker == nullptr)) // A talker object or player state might not yet exist if the connection is new.
		{
			continue;
		}

		UOnsetVoipDataChannel* Ch = StaticGetOnsetVoipChannel(NetDriver, ToConnection);
		if (UNLIKELY(Ch == nullptr))
		{
			continue;
		}

		const EOnsetVoipNetRelevancy VoiceNetRelevancy = Ch->VoipNetRelevancy->GetVoiceNetRelevancy(FromVoipTalker, ToVoipTalker, ToConnection, VoipPacket);
		if (VoiceNetRelevancy == EOnsetVoipNetRelevancy::RELEVANT_NONE)
		{
			continue;
		}

		FOnsetVoicePacketWrapper VoicePacketWrapper(VoipPacket, VoiceNetRelevancy);
		Ch->AddVoipPacket(MoveTemp(VoicePacketWrapper));
	}
}

#if !UE_SERVER
// Function to replicate a locally recorded packet to the server if client or to clients if listen server.
void UOnsetVoipDataChannel::ReplicateLocalVoipPacket(const ULocalPlayer* LocalPlayer, const FOnsetVoipPacketPtr& VoipPacket)
{
	if (const UWorld* World = GEngine->GetWorldFromContextObject(LocalPlayer, EGetWorldErrorMode::ReturnNull))
	{
		if (World->bIsTearingDown == false &&
			World->IsInSeamlessTravel() == false)
		{
			if (const UNetDriver* NetDriver = World->GetNetDriver())
			{
				if (World->GetNetMode() == ENetMode::NM_Client)
				{
					if (NetDriver->ServerConnection)
					{
						// Client: Send to server connection.
						if (UOnsetVoipDataChannel* Ch = StaticGetOnsetVoipChannel(NetDriver, NetDriver->ServerConnection))
						{
							FOnsetVoicePacketWrapper VoicePacketWrapper(VoipPacket, EOnsetVoipNetRelevancy::RELEVANT_NONE); // Relevancy not set as the server will decide.
							Ch->AddVoipPacket(MoveTemp(VoicePacketWrapper));
						}
					}
				}
				else
				{
					if (const UOnsetVoipWorldSubsystem* VoipWorldSubsystem = World->GetSubsystem<UOnsetVoipWorldSubsystem>())
					{
						if (LocalPlayer->PlayerController)
						{
							if (const UOnsetVoipTalker* FromVoipTalker = VoipWorldSubsystem->GetVoipTalker(LocalPlayer->PlayerController->PlayerState))
							{
								VoipPacket->OwnerPlayerState = LocalPlayer->PlayerController->PlayerState;

								// Listen server: Send to all.
								ServerReplicateVoipPacket(NetDriver, VoipWorldSubsystem, FromVoipTalker, VoipPacket, nullptr);
							}
						}
					}
				}
			}
		}
	}
}
#endif

void UOnsetVoipDataChannel::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		VoipNetRelevancy = TUniquePtr<FOnsetVoipNetRelevancy>(new FOnsetVoipNetRelevancy());
	}
}

void UOnsetVoipDataChannel::ReceivedBunch(FInBunch& Bunch)
{
#if !UE_SERVER
	NumTotalBitsReceived += Bunch.GetNumBits();
#endif

	if (Connection->Driver == nullptr ||
		Connection->Driver->World == nullptr ||
		Connection->Driver->World->bIsTearingDown ||
		Connection->Driver->World->IsInSeamlessTravel() ||
		Connection->PackageMap == nullptr ||
		Connection->PlayerController == nullptr)
	{
		return;
	}

	// Get the VoipWorldSubsystem for this connection's world.
	UOnsetVoipWorldSubsystem* VoipWorldSubsystem = Connection->Driver->World->GetSubsystem<UOnsetVoipWorldSubsystem>();
	if (UNLIKELY(VoipWorldSubsystem == nullptr))
	{
		return;
	}

	while (Bunch.AtEnd() == false && Connection && Connection->GetConnectionState() != USOCK_Closed)
	{
		UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("[%s]: Received %i bytes VoIP bunch from %s (PC: %s)"),
			*ToString(Connection->Driver->GetNetMode()), Bunch.GetNumBytes(), *GetNameSafe(Connection->OwningActor), *Connection->PlayerController->GetName());

		FOnsetVoicePacketWrapper PacketWrapper;
		if (PacketWrapper.Serialize(Bunch, Connection->PackageMap) == false || Bunch.IsError() == true)
		{
			UE_LOG(LogOnsetVoip, Warning, TEXT("Failed to deserialize VoIP bunch by %s"),
				*Connection->PlayerId.ToDebugString());

			Bunch.SetError();
			break;
		}

		// Get the owning VoipTalker object.
#if UE_SERVER
		APlayerState* FromPlayerState = Connection->PlayerController->PlayerState;
#else
		const bool bIsListenServer = /*Connection->Driver->ServerConnection == nullptr && */ Connection->Driver->GetNetMode() == ENetMode::NM_ListenServer;
		APlayerState* FromPlayerState = bIsListenServer ? ToRawPtr(Connection->PlayerController->PlayerState) : PacketWrapper.VoipPacket->OwnerPlayerState.Get();
#endif

		UOnsetVoipTalker* FromVoipTalker = VoipWorldSubsystem->GetVoipTalker(FromPlayerState);
		if (FromVoipTalker == nullptr)
		{
			UE_LOG(LogOnsetVoip, Warning, TEXT("Connection %s (PlayerId: %s, PlayerState: %s) has no valid OnsetVoipTalker, discarding packet."),
				*Connection->GetPathName(), *Connection->PlayerId.ToDebugString(), *GetNameSafe(FromPlayerState));
			continue;
		}

		// Check whether VoIP is enabled on this talker.
		// Server: If not enabled, do not send it to any other clients.
		// Client: If not enabled, do not process voice for this talker.
		if (!FromVoipTalker->IsVoipEnabled())
		{
			continue;
		}

#if !UE_SERVER
		if (bIsListenServer)
		{
#endif
			// If we are the server, set the playerstate on this incoming packet.
			PacketWrapper.VoipPacket->OwnerPlayerState = Connection->PlayerController->PlayerState;
#if !UE_SERVER
		}
#endif

#if ONSETVOIP_ENABLE_VOICE_PROCESSING
		bool bProcessVoice = false; // false by default, will be checked below.
#if UE_SERVER
		bProcessVoice = true; // Always process on a dedicated server with voice processing enabled.
		PacketWrapper.Relevancy = EOnsetVoipNetRelevancy::RELEVANT_NONE; // Dedicated servers have no real listener so no relevancy to decide on.
#endif
#endif

#if !UE_SERVER
		if (bIsListenServer)
		{
			// On a listen server we have to decide relevancy for local playback here.
			const auto GetLocalPC = [World = Connection->Driver->World]() -> const APlayerController*
			{
				if (World == nullptr)
				{
					return nullptr;
				}

				const APlayerController* FirstLocalController = nullptr;

				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					const APlayerController* PC = It->Get();
					if (PC == nullptr || !PC->IsLocalController())
					{
						continue;
					}

					if (FirstLocalController == nullptr)
					{
						FirstLocalController = PC;
					}

					if (PC->HasAuthority())
					{
						return PC;
					}
				}

				return FirstLocalController != nullptr ? FirstLocalController : (GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr);
			};

			if (const APlayerController* LocalPlayerController = GetLocalPC())
			{
				if (const UOnsetVoipTalker* ToVoipTalker = VoipWorldSubsystem->GetVoipTalker(LocalPlayerController->PlayerState))
				{
					const EOnsetVoipNetRelevancy VoiceNetRelevancy = VoipNetRelevancy->GetVoiceNetRelevancy(FromVoipTalker, ToVoipTalker, nullptr, PacketWrapper.VoipPacket);
					if (VoiceNetRelevancy != EOnsetVoipNetRelevancy::RELEVANT_NONE)
					{
						bProcessVoice = true;
						PacketWrapper.Relevancy = VoiceNetRelevancy;
					}
				}
			}
		}
		else
		{
			// Always process if we are a client.
			bProcessVoice = true;
		}
#endif

#if ONSETVOIP_ENABLE_VOICE_PROCESSING
		if (bProcessVoice)
		{
			FromVoipTalker->ProcessVoiceData(PacketWrapper, VoipWorldSubsystem);
		}
#endif

#if !UE_SERVER
		if (bIsListenServer)
		{
#endif
			ServerReplicateVoipPacket(Connection->Driver, VoipWorldSubsystem, FromVoipTalker, PacketWrapper.VoipPacket, Connection);
#if !UE_SERVER
		}
#endif
	}
}

FPacketIdRange UOnsetVoipDataChannel::SendBunch(FOutBunch* Bunch, bool Merge)
{
#if !UE_SERVER
	NumTotalBitsSent += Bunch->GetNumBits();
#endif

	UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("Sending VoIP bunch with size of %i bytes."), Bunch->GetNumBytes());

	return Super::SendBunch(Bunch, Merge);
}

void UOnsetVoipDataChannel::Tick()
{
	if (VoipPackets.Num() > 0 && Connection->PlayerController != nullptr)
	{
#if ENGINE_MINOR_VERSION >= 5
		if (Connection->IsClosingOrClosed() == false &&
#else
		if (Connection->GetConnectionState() != USOCK_Closed &&
#endif
			Connection->GetWorld() &&
			Connection->GetWorld()->bIsTearingDown == false &&
			Connection->GetWorld()->IsInSeamlessTravel() == false)
		{
			int32 NumSent = 0;
			for (FOnsetVoicePacketWrapper& Packet : VoipPackets)
			{
#if ENGINE_MINOR_VERSION >= 6
				if (UNLIKELY(IsNetReady() == false))
#else
				if (UNLIKELY(IsNetReady(true) == false))
#endif
				{
					UE_LOG(LogOnsetVoip, Warning, TEXT("Connection not ready or saturated."));
					break;
				}

				FOutBunch Bunch(this, 0);
				Bunch.bReliable = OpenAcked != true;

				if (Packet.Serialize(Bunch, Connection->PackageMap) && !Bunch.IsError())
				{
					SendBunch(&Bunch, true);
					NumSent++;
				}
				else
				{
					UE_LOG(LogOnsetVoip, Error, TEXT("Failed to serialize voice packet %s."), *Packet.VoipPacket->ToDebugString());
					continue;
				}
			}

			if (NumSent != VoipPackets.Num())
			{
				UE_LOG(LogOnsetVoip, Warning, TEXT("Dropped %i voice packets for %s."), VoipPackets.Num() - NumSent, *Connection->Describe());
			}
		}

		VoipPackets.Empty();
	}
}

bool UOnsetVoipDataChannel::CanStopTicking() const
{
	return false;
}

FString UOnsetVoipDataChannel::Describe()
{
	return FString::Printf(TEXT("UOnsetVoipDataChannel: %s"), *Super::Describe());
}

void UOnsetVoipDataChannel::AddVoipPacket(FOnsetVoicePacketWrapper&& VoipPacketWrapper)
{
	UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("AddVoipPacket: %s [%s] to=%s from=%s"),
		*Connection->PlayerId.ToDebugString(),
		*Connection->Driver->GetDescription(),
		*Connection->LowLevelDescribe(),
		*GetPathNameSafe(VoipPacketWrapper.VoipPacket->OwnerPlayerState.Get()));

	VoipPackets.Emplace(MoveTemp(VoipPacketWrapper));
}

#if !UE_SERVER
void UOnsetVoipDataChannel::CalculateAverageThroughput(int64& OutAverageBytesSent, int64& OutAverageBytesReceived)
{
	const double CurrentTime = FPlatformTime::Seconds();
	const double Timespan = CurrentTime - LastTimeAverageCalculated;

	// Time in seconds since the last call until we reset the average value calculation.
	const constexpr double MaxTimespanBeforeReset = 10.0;

	const bool bResetStat = LastTimeAverageCalculated == 0.0 || Timespan >= MaxTimespanBeforeReset;

	if (bResetStat)
	{
		OutAverageBytesSent = 0;
		OutAverageBytesReceived = 0;
	}
	else
	{
		OutAverageBytesSent = (int64(double(NumTotalBitsSent - LastNumTotalBits[0]) * (1.0 / Timespan)) + 7) >> 3;
		OutAverageBytesReceived = (int64(double(NumTotalBitsReceived - LastNumTotalBits[1]) * (1.0 / Timespan)) + 7) >> 3;
	}

	if (bResetStat || Timespan >= 0.5)
	{
		LastNumTotalBits[0] = NumTotalBitsSent;
		LastNumTotalBits[1] = NumTotalBitsReceived;
		LastTimeAverageCalculated = CurrentTime;
	}
}
#endif
