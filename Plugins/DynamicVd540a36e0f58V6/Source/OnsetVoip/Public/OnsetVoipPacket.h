// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnsetVoipRelevancyDefinitions.h"
#include "Engine/Channel.h"
#include "UObject/CoreNet.h"
#include "GameFramework/PlayerState.h"

namespace OnsetVoip
{
	// Maximum size a compressed voice packet buffer may have.
	constexpr uint16 MaxCompressedBufferSize = 4 * 1024;

	// Maximum voice channel hints in ToggleVoiceCaptureWithChannels()
	constexpr int32 MaxVoiceChannelHints = 32;

	// Do a checksum test on voice data sent over the network
	constexpr bool bEnableVoiceDataChecksum = true;
}

class ONSETVOIP_API FOnsetVoipPacket : public FNoncopyable
{
public:
	FOnsetVoipPacket() { }
	explicit FOnsetVoipPacket(uint8* InData, uint32 InDataSize)
	{
		Buffer.AddUninitialized(InDataSize);
		FMemory::Memcpy(Buffer.GetData(), InData, InDataSize);
	}
	explicit FOnsetVoipPacket(uint8* InData, uint32 InDataSize, const TArray<int32>& InHintVoiceChannels, float InAmplitude)
		: FOnsetVoipPacket(InData, InDataSize)
	{
		ClientHintVoiceChannels = InHintVoiceChannels;
		bClientHintVoiceChannelId = ClientHintVoiceChannels.Num() > 0;
		Amplitude = InAmplitude;
	}

	// The compressed voice data.
	TArray<uint8> Buffer;

	// Owner of this voice data.
	TWeakObjectPtr<APlayerState> OwnerPlayerState;

	// The current amplitude.
	float Amplitude = 0.0f;

	// Voice channels a client hints (i.e. wants to talk on)
	bool bClientHintVoiceChannelId = false;
	TArray<int32> ClientHintVoiceChannels;

	bool Serialize(FArchive& Ar, UPackageMap* PackageMap);

	FString ToDebugString() const;
};

typedef TSharedPtr<FOnsetVoipPacket> FOnsetVoipPacketPtr;
typedef TSharedRef<FOnsetVoipPacket> FOnsetVoipPacketRef;

class ONSETVOIP_API FOnsetVoicePacketWrapper : public FNoncopyable
{
public:
	FOnsetVoicePacketWrapper(FOnsetVoicePacketWrapper&& Other) noexcept
		: VoipPacket(MoveTemp(Other.VoipPacket))
		, Relevancy(Other.Relevancy)
	{

	}

	explicit FOnsetVoicePacketWrapper()
		: VoipPacket(nullptr)
		, Relevancy(EOnsetVoipNetRelevancy::RELEVANT_NONE)
	{

	}

	explicit FOnsetVoicePacketWrapper(const FOnsetVoipPacketPtr& VoipPacket, EOnsetVoipNetRelevancy InRelevancy)
		: VoipPacket(VoipPacket)
		, Relevancy(InRelevancy)
	{

	}

	FOnsetVoipPacketPtr VoipPacket;
	EOnsetVoipNetRelevancy Relevancy;

	bool Serialize(FArchive& Ar, UPackageMap* PackageMap);
};
