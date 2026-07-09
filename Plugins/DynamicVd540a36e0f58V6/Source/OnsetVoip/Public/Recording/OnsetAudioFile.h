// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Recording/OnsetAudioCodecWav.h"
#include "Recording/OnsetFFmpegConverter.h"

class IOnsetAudioFile : public FNoncopyable
{
public:
	IOnsetAudioFile(const FString& InFilename)
		: Filename(FPaths::ConvertRelativePathToFull(InFilename)) { }

	virtual ~IOnsetAudioFile() = default;

	virtual void Write(const TArrayView<uint8>& InPcmData) = 0;

	FString GetFilename() const
	{
		return Filename;
	}

private:
	const FString Filename;
};

class FOnsetAudioPcmFile : public IOnsetAudioFile
{
public:
	FOnsetAudioPcmFile(const FString& InFilename, const TSharedRef<FArchive>& InAr)
		: IOnsetAudioFile(InFilename)
		, Ar(InAr) { }

	virtual void Write(const TArrayView<uint8>& InPcmData) override
	{
		Ar->Serialize(InPcmData.GetData(), InPcmData.Num());
	}

protected:
	const TSharedRef<FArchive> Ar;
};

class FOnsetAudioWavFile : public IOnsetAudioFile
{
public:
	FOnsetAudioWavFile(const FString& InFilename, const TSharedRef<FArchive>& InAr, uint32 InSampleRate, uint16 InNumChannels)
		: IOnsetAudioFile(InFilename)
		, Wav(MakeShared<FOnsetAudioCodecWavFile>(InAr, InSampleRate, InNumChannels)) { }

	virtual void Write(const TArrayView<uint8>& InPcmData) override
	{
		Wav->Write(InPcmData);
	}

protected:
	TSharedPtr<FOnsetAudioCodecWavFile> Wav;
};

class FOnsetAudioMp3File : public FOnsetAudioWavFile
{
public:
	FOnsetAudioMp3File(const FString& InFilename, const TSharedRef<FArchive>& InAr, uint32 InSampleRate, uint16 InNumChannels)
		: FOnsetAudioWavFile(InFilename, InAr, InSampleRate, InNumChannels)
		, SampleRate(InSampleRate)
		, NumChannels(InNumChannels)
	{ }

	~FOnsetAudioMp3File();

protected:
	uint32 SampleRate;
	uint16 NumChannels;
};
