// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/FileManager.h"

class ONSETVOIP_API FOnsetAudioCodecWav : public FNoncopyable
{
protected:
	enum WAVE_FORMAT : uint16
	{
		WAVE_FORMAT_PCM = 0x0001,
		//WAVE_FORMAT_IEEE_FLOAT = 0x0003,
		//WAVE_FORMAT_ALAW = 0x0006,
		//WAVE_FORMAT_MULAW = 0x0007,
		//WAVE_FORMAT_EXTENSIBLE = 0xFFFE
	};

	// https://en.wikipedia.org/wiki/WAV
	struct WAVE_HEADER
	{
		char FileTypeBlocID[4] = { 'R', 'I', 'F', 'F' };
		uint32 FileSize;
		char FileFormatID[4] = { 'W', 'A', 'V', 'E' };
		char FormatBlocID[4] = { 'f', 'm', 't', ' ' };
		uint32 BlocSize = 16;
		WAVE_FORMAT AudioFormat;
		uint16 NbrChannels;
		uint32 Frequence;
		uint32 BytePerSec;
		uint16 BytePerBloc;
		uint16 BitsPerSample;
		char DataBlocID[4] = { 'd', 'a', 't', 'a' };
		uint32 DataSize;
	};

	WAVE_HEADER WaveHeader;

	void SetHeader(uint32 InSampleRate, uint16 InNumChannels)
	{
		check(InSampleRate <= 48000);
		check(InNumChannels <= 2);

		WaveHeader.FileSize = 0;
		WaveHeader.DataSize = 0;
		WaveHeader.NbrChannels = InNumChannels;
		WaveHeader.Frequence = InSampleRate;
		WaveHeader.AudioFormat = WAVE_FORMAT::WAVE_FORMAT_PCM;
		const uint16 SampleByteSize = WaveHeader.AudioFormat == WAVE_FORMAT::WAVE_FORMAT_PCM ? 2 : 4;
		WaveHeader.BitsPerSample = SampleByteSize * 8;
		WaveHeader.BytePerSec = InSampleRate * InNumChannels * SampleByteSize;
		WaveHeader.BytePerBloc = InNumChannels * SampleByteSize;
	}

public:
	void PcmToWav(uint32 InSampleRate, uint16 InNumChannels, const TArrayView<uint8>& InPcmData, TArray<uint8>* OutWavData)
	{
		SetHeader(InSampleRate, InNumChannels);

		WaveHeader.DataSize = InPcmData.Num();
		WaveHeader.FileSize = WaveHeader.DataSize + sizeof(WAVE_HEADER) - 8;

		OutWavData->AddUninitialized(sizeof(WAVE_HEADER) + InPcmData.Num());
		FMemory::Memcpy(OutWavData->GetData(), &WaveHeader, sizeof(WAVE_HEADER));
		FMemory::Memcpy(OutWavData->GetData() + sizeof(WAVE_HEADER), InPcmData.GetData(), InPcmData.Num());
	}
};

class ONSETVOIP_API FOnsetAudioCodecWavFile final : public FOnsetAudioCodecWav
{
public:
	FOnsetAudioCodecWavFile(const TSharedRef<FArchive>& InAr, uint32 InSampleRate, uint16 InNumChannels)
		: Ar(InAr)
	{
		SetHeader(InSampleRate, InNumChannels);

		// Write/Preserve initial wave header. Size information for the audio data will be written in the destructor.
		Ar->Serialize(&WaveHeader, sizeof(WAVE_HEADER));
	}

	~FOnsetAudioCodecWavFile()
	{
		// On destruction get the total amount of (audio) data written minus the header.
		const int64 AudioSize = Ar->Tell() - sizeof(WAVE_HEADER);

		// Set the size in header.
		WaveHeader.DataSize = static_cast<decltype(WaveHeader.DataSize)>(AudioSize);
		WaveHeader.FileSize = WaveHeader.DataSize + sizeof(WAVE_HEADER) - 8;

		// Write header with size information again at the beginning.
		Ar->Seek(0);
		Ar->Serialize(&WaveHeader, sizeof(WAVE_HEADER));
	}

	void Write(const TArrayView<uint8>& InPcmData)
	{
		constexpr int64 MaxFileSize = static_cast<int64>(TNumericLimits<int32>::Max() - sizeof(WAVE_HEADER) - 128);
		if (Ar->Tell() < MaxFileSize)
		{
			Ar->Serialize(InPcmData.GetData(), InPcmData.Num());
		}
	}

private:
	const TSharedRef<FArchive> Ar;
};
