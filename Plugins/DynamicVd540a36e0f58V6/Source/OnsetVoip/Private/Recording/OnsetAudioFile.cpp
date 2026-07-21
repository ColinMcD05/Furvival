// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "Recording/OnsetAudioFile.h"
#include "OnsetVoip.h"
#include "Async/AsyncWork.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

FOnsetAudioMp3File::~FOnsetAudioMp3File()
{
	// Close the file here.
	Wav = nullptr;

	// Task wrapper
	class FOnsetAsyncMp3Conversion : public FNonAbandonableTask
	{
	public:
		FOnsetAsyncMp3Conversion(const FString& InFilename, const TSharedPtr<IOnsetFFmpegConverter>& InConverter)
			: Filename(InFilename)
			, Converter(InConverter) {	}

		void DoWork()
		{
			if (Converter->Start())
			{
				Converter = nullptr; // blocking wait for complete in this generic thread

				// Delete old tmp file.
				IFileManager::Get().Delete(*Filename);
			}
			else
			{
				UE_LOG(LogOnsetVoip, Error, TEXT("Failed to convert audio recording to mp3. Did you configure FFmpeg?"));
			}
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FOnsetAsyncMp3Conversion, STATGROUP_ThreadPoolAsyncTasks);
		}

	private:
		FString Filename;
		TSharedPtr<IOnsetFFmpegConverter> Converter;
	};

	// Rename to intermediate file so that FFmpeg can convert it to mp3
	const FString TmpFile = FPaths::ChangeExtension(GetFilename(), "tmp");
	if (IFileManager::Get().Move(*TmpFile, *GetFilename()))
	{
		(new FAutoDeleteAsyncTask<FOnsetAsyncMp3Conversion>(
			TmpFile,
			MakeShared<FOnsetFFmpegAudioFileConverter>(TmpFile, ".mp3", SampleRate, NumChannels, 192000))
			)->StartBackgroundTask();
	}
	else
	{
		UE_LOG(LogOnsetVoip, Error, TEXT("Failed to convert audio recording to mp3. %s"), *GetFilename());
	}
}
