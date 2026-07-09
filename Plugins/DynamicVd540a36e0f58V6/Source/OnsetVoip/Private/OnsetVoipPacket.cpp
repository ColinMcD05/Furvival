// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetVoipPacket.h"

DEFINE_LOG_CATEGORY_STATIC(LogOnsetVoipPacket, Error, All);

static_assert(OnsetVoip::MaxCompressedBufferSize < TNumericLimits<uint16>::Max());
static_assert(OnsetVoip::MaxCompressedBufferSize > 0);

bool FOnsetVoipPacket::Serialize(FArchive& Ar, UPackageMap* PackageMap)
{
	if (Ar.IsLoading())
	{
		bool bHasPlayerState = false;
		Ar << bHasPlayerState;

		if (bHasPlayerState)
		{
			UObject* PlayerStateObject = nullptr;
			if (!PackageMap->SerializeObject(Ar, APlayerState::StaticClass(), PlayerStateObject))
			{
				UE_LOG(LogOnsetVoipPacket, Warning, TEXT("Failed to read PlayerStateObject"));
				return false;
			}

			APlayerState* PlayerState = Cast<APlayerState>(PlayerStateObject);
			if (!IsValid(PlayerState))
			{
				UE_LOG(LogOnsetVoipPacket, Warning, TEXT("PlayerState is not valid"));
				return false;
			}

			OwnerPlayerState = PlayerState;
		}

		Ar << bClientHintVoiceChannelId;
		if (bClientHintVoiceChannelId)
		{
			uint8 NumChannelHints = 0;
			Ar << NumChannelHints;

			if (NumChannelHints == 0 || NumChannelHints > OnsetVoip::MaxVoiceChannelHints)
			{
				UE_LOG(LogOnsetVoipPacket, Warning, TEXT("Invalid NumChannelHints value of %i"), NumChannelHints);
				return false;
			}

			ClientHintVoiceChannels.Reserve(NumChannelHints);
			for (uint8 i = 0; i < NumChannelHints; i++)
			{
				int32 HintChannelId = 0;
				Ar << HintChannelId;
				ClientHintVoiceChannels.Add(HintChannelId);
			}
		}

		bool bReadAmplitude = false;
		Ar << bReadAmplitude;

		if (bReadAmplitude)
		{
			Ar << Amplitude;
			Amplitude = FMath::Clamp(Amplitude, 0.0f, 1.0f);
		}

		uint16 Length = 0;
		Ar << Length;

		if (Length == 0 || Length > OnsetVoip::MaxCompressedBufferSize)
		{
			UE_LOG(LogOnsetVoipPacket, Warning, TEXT("Invalid Length value of %i"), Length);
			return false;
		}

		Buffer.AddUninitialized(Length);
		Ar.Serialize(Buffer.GetData(), Length);

		if constexpr (OnsetVoip::bEnableVoiceDataChecksum)
		{
			uint32 CRC = 0;
			Ar << CRC;

			const uint32 BufferCRC = FCrc::MemCrc32(Buffer.GetData(), Length);

			if (CRC != BufferCRC)
			{
				UE_LOG(LogOnsetVoipPacket, Warning, TEXT("Voice data checksum mismatch, expected %u, got %u"), CRC, BufferCRC);
				return false;
			}
		}
	}
	else
	{
		bool bSendPlayerState = OwnerPlayerState.IsValid();
		Ar << bSendPlayerState;

		if (bSendPlayerState)
		{
			UObject* PlayerStateObject = OwnerPlayerState.Get();
			if (!PackageMap->SerializeObject(Ar, APlayerState::StaticClass(), PlayerStateObject))
			{
				UE_LOG(LogOnsetVoipPacket, Warning, TEXT("Failed to write PlayerStateObject"));
				return false;
			}
		}

		const int32 NumElements = ClientHintVoiceChannels.Num();
		bool bShouldSendChannelHints = bClientHintVoiceChannelId && (NumElements > 0 && NumElements <= OnsetVoip::MaxVoiceChannelHints);
		Ar << bShouldSendChannelHints;

		if (bShouldSendChannelHints)
		{
			uint8 NumChannelHints = static_cast<uint8>(NumElements);
			Ar << NumChannelHints;

			for (uint8 i = 0; i < NumChannelHints; i++)
			{
				Ar << ClientHintVoiceChannels[i];
			}
		}

		if (Buffer.Num() == 0 || Buffer.Num() > OnsetVoip::MaxCompressedBufferSize)
		{
			UE_LOG(LogOnsetVoipPacket, Warning, TEXT("Buffer size is invalid %i"), Buffer.Num());
			return false;
		}

		bool bSyncAmplitude = Amplitude > 0.0f;
		Ar << bSyncAmplitude;

		if (bSyncAmplitude)
		{
			Ar << Amplitude;
		}

		uint16 Length = Buffer.Num();
		Ar << Length;

		Ar.Serialize(Buffer.GetData(), Length);

		if constexpr (OnsetVoip::bEnableVoiceDataChecksum)
		{
			uint32 CRC = FCrc::MemCrc32(Buffer.GetData(), Length);
			Ar << CRC;
		}
	}
	return true;
}

FString FOnsetVoipPacket::ToDebugString() const
{
	return FString::Printf(TEXT("[FOnsetVoipPacket] BufferSize: %i, Owner: %s"), Buffer.Num(), *GetNameSafe(OwnerPlayerState.Get()));
}

bool FOnsetVoicePacketWrapper::Serialize(FArchive& Ar, UPackageMap* PackageMap)
{
	check(Ar.IsLoading() ? !VoipPacket.IsValid() : VoipPacket.IsValid());

	Ar.SerializeBits(&Relevancy, ONSET_VOICE_NET_RELEVANCY_NUM_BITS);

	if (Ar.IsLoading())
	{
		VoipPacket = MakeShared<FOnsetVoipPacket>();
	}

	return VoipPacket->Serialize(Ar, PackageMap);
}
