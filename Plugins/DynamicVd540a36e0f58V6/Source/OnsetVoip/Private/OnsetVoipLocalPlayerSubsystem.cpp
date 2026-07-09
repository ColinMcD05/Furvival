// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetVoipLocalPlayerSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "OnsetVoip.h"
#include "OnsetVoipDataChannel.h"
#include "OnsetVoipPacket.h"
#include "OnsetVoipSettings.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h" // For CreateSound2D

#if !UE_SERVER

#include "OnsetPixelStreamingVoiceCapture.h"
#include "AudioCaptureBlueprintLibrary2.h"
#include "Recording/OnsetVoipRecorderLocal.h"

#if PLATFORM_ANDROID
#include "Modules/ModuleManager.h"
#include "AndroidPermissionFunctionLibrary.h"
#endif

FAutoConsoleCommandWithWorldAndArgs _CmdVoiceDevice(TEXT("voice.device"),
	TEXT("Prints available microphone input devices\n")
	TEXT("Optional: <device>: Part of id or name. If specified, will attempt to change to that device name."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() > 0)
	{
		const FString DesiredDevice = Args[0];
		TWeakObjectPtr<UWorld> WeakWorld = World;
		UAudioCaptureBlueprintLibrary2::GetAvailableAudioInputDevicesFixedCpp(World,
			FOnAudioInputDevicesObtained2Cpp::CreateLambda([WeakWorld, DesiredDevice](const TArray<FAudioInputDeviceInfo2>& Devices)
			{
				if (WeakWorld.IsValid())
				{
					for (const auto& Device : Devices)
					{
						if (Device.DeviceName.Contains(DesiredDevice) || Device.DeviceId.Contains(DesiredDevice))
						{
							if (const ULocalPlayer* LocalPlayer = WeakWorld->GetFirstLocalPlayerFromController())
							{
								if (UOnsetVoipLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UOnsetVoipLocalPlayerSubsystem>())
								{
									if (Subsystem->ChangeVoiceCaptureDevice(Device.DeviceName))
									{
										OnsetVoip::ConsoleLog(WeakWorld.Get(), FString::Printf(TEXT("Changed input device to [%s] %s"), *Device.DeviceId, *Device.DeviceName));
									}
									else
									{
										OnsetVoip::ConsoleLog(WeakWorld.Get(), FString::Printf(TEXT("Failed to change input device to [%s] %s"), *Device.DeviceId, *Device.DeviceName));
									}
									return;
								}
							}
						}
					}

					OnsetVoip::ConsoleLog(WeakWorld.Get(), FString::Printf(TEXT("Device \"%s\" not found or no active voice capture running."), *DesiredDevice));
				}
			})
		);
	}
	else
	{
		TWeakObjectPtr<UWorld> WeakWorld = World;
		UAudioCaptureBlueprintLibrary2::GetAvailableAudioInputDevicesFixedCpp(World,
			FOnAudioInputDevicesObtained2Cpp::CreateLambda([WeakWorld](const TArray<FAudioInputDeviceInfo2>& Devices)
			{
				if (WeakWorld.IsValid())
				{
					if (Devices.Num() > 0)
					{
						OnsetVoip::ConsoleLog(WeakWorld.Get(), FString::Printf(TEXT("There are %i input devices: "), Devices.Num()));

						for (const auto& Device : Devices)
						{
							OnsetVoip::ConsoleLog(WeakWorld.Get(), FString::Printf(TEXT("[%s] %s (InputChannels: %i, PreferredSampleRate: %i)"), *Device.DeviceId, *Device.DeviceName, Device.InputChannels, Device.PreferredSampleRate));
						}
					}
					else
					{
						OnsetVoip::ConsoleLog(WeakWorld.Get(), TEXT("No input devices found."));
					}
				}
			})
		);
	}
}));

FAutoConsoleCommandWithWorld _CmdVoiceDebug(TEXT("voice.debug"), TEXT("Prints debug information to log file."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
	if (World)
	{
		if (const ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController())
		{
			if (const UOnsetVoipLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UOnsetVoipLocalPlayerSubsystem>())
			{
				Subsystem->PrintDebugInformation();
			}
		}
	}

	UOnsetVoipLocalPlayerSubsystem::PrintVoiceRelatedCVarsToLog();
	OnsetVoip::ConsoleLog(World, TEXT("Done, see engine log."));
}));

#endif /* !UE_SERVER */

bool UOnsetVoipLocalPlayerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	checkf(UKismetSystemLibrary::IsDedicatedServer(Outer) == false, TEXT("Local player subsystem should never be created on a dedicated server?"));

	return Super::ShouldCreateSubsystem(Outer);
}

#if !UE_SERVER
void UOnsetVoipLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!InitializeVoiceEncoder())
	{
		return;
	}

	if (!TryInitializeVoiceCapture())
	{
		UE_CLOG(!IsVoiceCapturePermissionGranted(), LogOnsetVoip, Log, TEXT("Voice capture will try to initialize again once permission is granted."));
	}

	static_assert(OnsetVoip::MaxCompressedBufferSize > 0);
	CompressedVoiceData.AddUninitialized(OnsetVoip::MaxCompressedBufferSize);

	PrintVoiceRelatedCVarsToLog();

	UE_LOG(LogOnsetVoip, Log, TEXT("Local VoIP subsystem initiailized."));
}

void UOnsetVoipLocalPlayerSubsystem::Deinitialize()
{
	Super::Deinitialize();

	if (VoiceCapture.IsValid())
	{
#if 0 // Shutdown is already called in dtor, also fixes double free on Android in engine
		VoiceCapture->Shutdown();
#endif
		VoiceCapture = nullptr;
	}

	VoiceEncoder = nullptr;

	UE_LOG(LogOnsetVoip, Log, TEXT("Local VoIP subsystem deinitiailized."));
}
#endif /* !UE_SERVER */

bool UOnsetVoipLocalPlayerSubsystem::ChangeVoiceCaptureDevice(const FString& DeviceName)
{
#if !UE_SERVER
	if (VoiceCapture.IsValid())
	{
		const int32 SampleRate = GetDefault<UOnsetVoipSettings>()->VoiceCaptureSampleRate;
		const int32 NumChannels = GetDefault<UOnsetVoipSettings>()->VoiceCaptureChannels;

		const bool bSuccess = VoiceCapture->ChangeDevice(DeviceName, SampleRate, NumChannels);
		UE_LOG(LogOnsetVoip, Log, TEXT("Changing voice capture device to %s %s."), *DeviceName, bSuccess ? TEXT("succeeded") : TEXT("failed"));
#if !PLATFORM_WINDOWS
		UE_LOG(LogOnsetVoip, Warning, TEXT("Changing voice capture device is not supported on this platform."));
#endif
		return bSuccess;
	}
#endif
	return false;
}

bool UOnsetVoipLocalPlayerSubsystem::ToggleVoiceCapture(bool bCapture)
{
	if (bCapture)
	{
		// Clear this only on capture enable. On disable we might still need the values until device is fully stopped.
		// New values are added after this function in ToggleVoiceCaptureWithChannel(s).
		HintVoiceChannels.Empty();
	}

#if !UE_SERVER

	if (!VoiceCapture.IsValid())
	{
		UE_CLOG(!IsVoiceCapturePermissionGranted(), LogOnsetVoip, Warning, TEXT("Please ensure voice capture permission is granted before calling ToggleVoiceCapture()"));

		if (!TryInitializeVoiceCapture())
		{
			return false;
		}
	}

	if (bCapture)
	{
		if (!VoiceCapture->IsCapturing())
		{
			uint32 NumAvailableVoiceData = 0;
			const EVoiceCaptureState::Type CaptureState = VoiceCapture->GetCaptureState(NumAvailableVoiceData);

			if (CaptureState == EVoiceCaptureState::Stopping)
			{
				UE_LOG(LogOnsetVoip, Log, TEXT("Voice capture is currently stopping. Delaying capture start until device is fully stopped."));
				VoiceCaptureAsyncState = EVoiceCaptureStateAsync::WantsToStart;
				return true;
			}

			VoiceRemainder.Empty();

			const bool bResult = VoiceCapture->Start();
			if (bResult)
			{
				UE_LOG(LogOnsetVoip, Verbose, TEXT("Starting voice capturing."));
			}
			else
			{
				UE_LOG(LogOnsetVoip, Warning, TEXT("Failed to start voice capturing."));
			}
			return bResult;
		}
	}
	else
	{
		VoiceCaptureAsyncState = EVoiceCaptureStateAsync::None;

		if (VoiceCapture->IsCapturing())
		{
			UE_LOG(LogOnsetVoip, Verbose, TEXT("Stopping voice capturing."));
			VoiceCapture->Stop();
			return true;
		}
	}

	UE_LOG(LogOnsetVoip, Warning, TEXT("Attempting to %s voice capturing while it is already %s."),
		bCapture ? TEXT("start") : TEXT("stop"),
		bCapture ? TEXT("started") : TEXT("stopped")
	);
#endif

	return false;
}

bool UOnsetVoipLocalPlayerSubsystem::ToggleVoiceCaptureWithChannel(bool bCapture, int32 VoiceChannelId)
{
	const bool bSuccess = ToggleVoiceCapture(bCapture);
	if (bCapture)
	{
		HintVoiceChannels.Add(VoiceChannelId);
	}
	return bSuccess;
}

bool UOnsetVoipLocalPlayerSubsystem::ToggleVoiceCaptureWithChannels(bool bCapture, const TArray<int32>& VoiceChannelIds)
{
	const bool bSuccess = ToggleVoiceCapture(bCapture);
	if (bCapture)
	{
		HintVoiceChannels = VoiceChannelIds;
		ensureMsgf(HintVoiceChannels.Num() <= OnsetVoip::MaxVoiceChannelHints, TEXT("Exceeded %i maximum voice channels that we may hint with ToggleVoiceCaptureWithChannels()"), OnsetVoip::MaxVoiceChannelHints);
	}
	return bSuccess;
}

bool UOnsetVoipLocalPlayerSubsystem::IsCapturingVoice() const
{
#if !UE_SERVER
	return VoiceCapture.IsValid() && VoiceCapture->IsCapturing();
#else
	return false;
#endif
}

bool UOnsetVoipLocalPlayerSubsystem::IsTalking() const
{
#if !UE_SERVER
	return IsCapturingVoice() && bCachedTalking;
#else
	return false;
#endif
}

float UOnsetVoipLocalPlayerSubsystem::GetCurrentAmplitude() const
{
#if !UE_SERVER
	return IsCapturingVoice() ? VoiceCapture->GetCurrentAmplitude() : 0.0f;
#else
	return -1.0f;
#endif
}

void UOnsetVoipLocalPlayerSubsystem::ToggleLoopback(bool bLoopback)
{
#if !UE_SERVER
	if (LoopbackSoundWave)
	{
		LoopbackSoundWave->ResetAudioBuffer();
		LoopbackSoundWave = nullptr;
	}

	if (LoopbackAudioComponent)
	{
		LoopbackAudioComponent->DestroyComponent();
		LoopbackAudioComponent = nullptr;
	}

	Native_OnVoipMicrophoneAudioCaptured.RemoveAll(this);

	if (bLoopback)
	{
		LoopbackSoundWave = NewObject<UOnsetVoipSoundWave>(this);
		LoopbackAudioComponent = UGameplayStatics::CreateSound2D(this, LoopbackSoundWave, 1.0f, 1.0f, 0.0f, nullptr, false, false);

		Native_OnVoipMicrophoneAudioCaptured.AddUObject(this, &UOnsetVoipLocalPlayerSubsystem::OnMicrophoneAudioCapturedForLoopback);
	}

	const FString Message = FString::Printf(TEXT("Voice loopback %s"), bLoopback ? TEXT("enabled") : TEXT("disabled"));
	OnsetVoip::ConsoleLog(this, Message);
#endif
}

bool UOnsetVoipLocalPlayerSubsystem::IsLoopbackEnabled() const
{
#if UE_SERVER
	return false;
#else
	return LoopbackSoundWave && LoopbackAudioComponent;
#endif
}

#if !UE_SERVER

TSharedPtr<IVoiceCapture> UOnsetVoipLocalPlayerSubsystem::CreateVoiceCaptureInterface(const FString& DeviceName, int32 SampleRate, int32 NumChannels)
{
#if ONSETVOIP_ENABLE_PIXELSTREAMING
	// At least the -PixelStreamingURL should be present on the command line arguments.
	if (FString(FCommandLine::Get()).Find(TEXT("-PixelStreaming")) != INDEX_NONE)
	{
		TSharedPtr<IVoiceCapture> PixelStreamingVoiceCapture = MakeShared<FOnsetPixelStreamingVoiceCapture>();
		if (PixelStreamingVoiceCapture->Init(DeviceName, SampleRate, NumChannels))
		{
			return PixelStreamingVoiceCapture;
		}
	}
#endif
	return FVoiceModule::Get().CreateVoiceCapture(DeviceName, SampleRate, NumChannels);
}

bool UOnsetVoipLocalPlayerSubsystem::TryInitializeVoiceCapture()
{
	if (VoiceCapture.IsValid())
	{
		return true;
	}

	const int32 SampleRate = GetDefault<UOnsetVoipSettings>()->VoiceCaptureSampleRate;
	const int32 NumChannels = GetDefault<UOnsetVoipSettings>()->VoiceCaptureChannels;

	VoiceCapture = CreateVoiceCaptureInterface("", SampleRate, NumChannels); // Empty device name indicates default device.
	if (!VoiceCapture.IsValid())
	{
		if (IsVoiceCapturePermissionGranted())
		{
			UE_LOG(LogOnsetVoip, Error, TEXT("Failed to create voice capture. Did you enable it in the DefaultEngine.ini? [Voice] bEnabled = true"));
			UE_LOG(LogOnsetVoip, Error, TEXT("Also check if these settings are correct: VoiceCaptureSampleRate: %i, VoiceCaptureChannels: %i"), SampleRate, NumChannels);
		}
		return false;
	}

	FrameByteSize = sizeof(int16) * NumChannels;

	VoiceCapture->DumpState();
	return true;
}

bool UOnsetVoipLocalPlayerSubsystem::InitializeVoiceEncoder()
{
	const int32 SampleRate = GetDefault<UOnsetVoipSettings>()->VoiceCaptureSampleRate;
	const int32 NumChannels = GetDefault<UOnsetVoipSettings>()->VoiceCaptureChannels;
	const FVoiceEncoderSettings& VoiceEncoderSettings = GetDefault<UOnsetVoipSettings>()->VoiceEncoderSettings;

	// Convert to original non blueprint type EAudioEncodeHint.
	const EAudioEncodeHint EncodeHint =
		VoiceEncoderSettings.EncodingHint == EOnsetAudioEncodeHint::VoiceEncode_Voice
		? EAudioEncodeHint::VoiceEncode_Voice
		: EAudioEncodeHint::VoiceEncode_Audio;

	ensure(!VoiceEncoder.IsValid());
	VoiceEncoder = FVoiceModule::Get().CreateVoiceEncoder(SampleRate, NumChannels, EncodeHint);
	if (!VoiceEncoder.IsValid())
	{
		UE_LOG(LogOnsetVoip, Error, TEXT("Failed to create a voice encoder."));
		return false;
	}
	if (VoiceEncoderSettings.bAdvancedEncoderSettings)
	{
		if (!VoiceEncoder->SetVBR(VoiceEncoderSettings.bEnableVBR))
		{
			UE_LOG(LogOnsetVoip, Warning, TEXT("Failed to set encoder VBR to %i"), VoiceEncoderSettings.bEnableVBR);
		}
		if (!VoiceEncoder->SetComplexity(VoiceEncoderSettings.Complexity))
		{
			UE_LOG(LogOnsetVoip, Warning, TEXT("Failed to set encoder complexity of %i"), VoiceEncoderSettings.Complexity);
		}
		if (!VoiceEncoder->SetBitrate(VoiceEncoderSettings.BitRate))
		{
			UE_LOG(LogOnsetVoip, Warning, TEXT("Failed to set encoder bitrate of %i"), VoiceEncoderSettings.BitRate);
		}
	}
	VoiceEncoder->DumpState();
	return true;
}

bool UOnsetVoipLocalPlayerSubsystem::IsVoiceCapturePermissionGranted()
{
#if PLATFORM_ANDROID
	return FModuleManager::Get().IsModuleLoaded(TEXT("AndroidPermission")) &&
		UAndroidPermissionFunctionLibrary::CheckPermission(TEXT("android.permission.RECORD_AUDIO"));
#else
	return true;
#endif
}

void UOnsetVoipLocalPlayerSubsystem::OnMicrophoneAudioCapturedForLoopback(const TArrayView<uint8>& AudioData)
{
	if (LoopbackSoundWave)
	{
		LoopbackSoundWave->AddAudioData(AudioData);
	}

	if (LoopbackAudioComponent && !LoopbackAudioComponent->IsPlaying())
	{
		LoopbackAudioComponent->Play();
	}
}
#endif

#if !UE_SERVER

FAutoConsoleCommandWithWorldAndArgs _CmdVoiceLoopback(TEXT("voice.loopback"), TEXT("Toggles loopback of the microphone input\n")TEXT("0: Disable, 1: Enable"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() == 0)
	{
		OnsetVoip::ConsoleLog(World, TEXT("voice.loopback <bEnable>"));
		return;
	}

	if (!World)
		return;

	if (const ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController())
	{
		if (UOnsetVoipLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UOnsetVoipLocalPlayerSubsystem>())
		{
			Subsystem->ToggleLoopback(Args[0].ToBool());
		}
	}
}));

static bool OverrideMicInputGainCommandExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("voice.MicInputGain")))
	{
		UE_LOG(LogOnsetVoip, Warning, TEXT("voice.MicInputGain does not work, use replacement: voice.MicInputGain2"));
		return true;
	}
	return false;
}

FStaticSelfRegisteringExec OverrideMicInputGainCommand(OverrideMicInputGainCommandExec);

static float MicInputGain2Cvar = 1.0f;
FAutoConsoleVariableRef CVarMicInputGain2(
	TEXT("voice.MicInputGain2"),
	MicInputGain2Cvar,
	TEXT("Adjust microphone input volume.\n")
	TEXT("Value: Gain multiplier. Default: 1.0"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* CVar)
		{
			const float MicGainValue = CVar->GetFloat();
			UE_CLOG(!FMath::IsWithinInclusive(MicGainValue, OnsetVoip::MinMicInputGain, OnsetVoip::MaxMicInputGain), LogOnsetVoip, Log, TEXT("MicInputGain %.2f should be within %.2f-%.2f"), MicGainValue, OnsetVoip::MinMicInputGain, OnsetVoip::MaxMicInputGain);
		}),
	ECVF_Default);

static float MicInputAGCCvar = 0.0f;
FAutoConsoleVariableRef CVarMicInputAGC(
	TEXT("voice.MicInputAGC"),
	MicInputAGCCvar,
	TEXT("Automatic Gain Control used to normalize microphone input volume. Experimental.\n")
	TEXT("Value: RMS (Root Mean Square). Example value between 500-8000 where a higher value means a higher volume. To disable set it to 0.0"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* CVar)
		{
			const float MicInputAGC = CVar->GetFloat();
			UE_CLOG(MicInputAGC != 0.0f && !FMath::IsWithinInclusive(MicInputAGC, OnsetVoip::MinMicInputAGC, OnsetVoip::MaxMicInputAGC), LogOnsetVoip, Log, TEXT("MicInputAGC %.2f should be within %.2f-%.2f"), MicInputAGC, OnsetVoip::MinMicInputAGC, OnsetVoip::MaxMicInputAGC);
		}),
	ECVF_Default);

bool UOnsetVoipLocalPlayerSubsystem::Tick(float DeltaTime)
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(LocalPlayer))
	{
		return true;
	}

	if (VoiceCapture.IsValid() && VoiceEncoder.IsValid())
	{
		// Get voice capture state and available voice data size.
		uint32 NumAvailableVoiceData = 0;
		EVoiceCaptureState::Type VoiceState = VoiceCapture->GetCaptureState(NumAvailableVoiceData);

		if (VoiceCaptureAsyncState == EVoiceCaptureStateAsync::WantsToStart &&
			VoiceState == EVoiceCaptureState::NotCapturing)
		{
			VoiceCaptureAsyncState = EVoiceCaptureStateAsync::None;
			VoiceRemainder.Empty();
			const bool bStartResult = VoiceCapture->Start();
			UE_LOG(LogOnsetVoip, Log, TEXT("Starting delayed voice capture: %s"), bStartResult ? TEXT("Succeeded") : TEXT("Failed"));
			return true;
		}

		if ((VoiceState == EVoiceCaptureState::Ok || VoiceState == EVoiceCaptureState::Stopping) && NumAvailableVoiceData > 0)
		{
			const int32 OldVoiceRemainderSize = VoiceRemainder.Num();

			TArray<uint8> RawVoiceData;
			RawVoiceData.AddUninitialized(NumAvailableVoiceData + OldVoiceRemainderSize);

			// If we have voice from the last run that wasn't encoded add it here again.
			if (OldVoiceRemainderSize > 0)
			{
				FMemory::Memmove(RawVoiceData.GetData(), VoiceRemainder.GetData(), OldVoiceRemainderSize);
				VoiceRemainder.Empty();
			}

			decltype(NumAvailableVoiceData) NumAvailableVoiceData_BeforeGetVoiceData = NumAvailableVoiceData;

			// Get the new voice data and add it after the remainder. NumAvailableVoiceData will be set with the amount of added data.
			VoiceState = VoiceCapture->GetVoiceData(RawVoiceData.GetData() + OldVoiceRemainderSize, NumAvailableVoiceData, NumAvailableVoiceData);

			if (
#if PLATFORM_ANDROID
				(OldVoiceRemainderSize == 0 && NumAvailableVoiceData_BeforeGetVoiceData >= 8191) ||
#endif
				NumAvailableVoiceData > NumAvailableVoiceData_BeforeGetVoiceData
				)
			{
				UE_LOG(LogOnsetVoip, Error, TEXT("Skipping invalid voice data, GetCaptureState: %u bytes, GetVoiceData: %u bytes"), NumAvailableVoiceData_BeforeGetVoiceData, NumAvailableVoiceData);
				return true;
			}

			if (VoiceState != EVoiceCaptureState::Ok && VoiceState != EVoiceCaptureState::Stopping)
			{
				UE_LOG(LogOnsetVoip, Error, TEXT("Failed to capture voice: %s"), EVoiceCaptureState::ToString(VoiceState));
				return true;
			}

			const uint32 NumSampleAlignedVoiceBytes = NumAvailableVoiceData - (NumAvailableVoiceData % sizeof(int16));
			if (NumAvailableVoiceData != NumSampleAlignedVoiceBytes)
			{
				UE_LOG(LogOnsetVoip, Warning, TEXT("Voice capture returned %u bytes, dropping %u partial-sample bytes."), NumAvailableVoiceData, NumAvailableVoiceData - NumSampleAlignedVoiceBytes);
			}

			if (NumSampleAlignedVoiceBytes > 0)
			{
				uint8* CapturedVoice = RawVoiceData.GetData() + OldVoiceRemainderSize;

				float GainFactorAGC = 1.0f;

				if (MicInputAGCCvar != 0.0f)
				{
					int64 SumOfSquares = 0;

					for (uint32 i = 0; i < NumSampleAlignedVoiceBytes; i += sizeof(int16))
					{
						const int16 Sample = *(int16*)(CapturedVoice + i);

						SumOfSquares += static_cast<int64>(Sample) * Sample;
					}

					const double MeanSquare = static_cast<double>(SumOfSquares) / (NumSampleAlignedVoiceBytes / 2);
					const float RootMeanSquare = FMath::Sqrt(static_cast<float>(MeanSquare));

					if (RootMeanSquare > 0.0f)
					{
						GainFactorAGC = FMath::Clamp(MicInputAGCCvar / RootMeanSquare, 0.01f, 10.0f);
					}
				}

				// Adjust volume if cvar or AGC is not at default.
				if (MicInputGain2Cvar != 1.0f || GainFactorAGC != 1.0f)
				{
					for (uint32 i = 0; i < NumSampleAlignedVoiceBytes; i += sizeof(int16))
					{
						int16* Ptr = (int16*)(CapturedVoice + i);

						const int16 Sample = *Ptr;

						// Adjust the sample value and handle clipping.
						const int32 ModifiedSample = static_cast<int32>(Sample) * MicInputGain2Cvar * GainFactorAGC;
						*Ptr = FMath::Clamp(ModifiedSample, TNumericLimits<decltype(Sample)>::Min(), TNumericLimits<decltype(Sample)>::Max());
					}
				}

			}

			// Our raw voice data is now the NumAvailableVoiceData which GetVoiceData returned + possible remainder.
			const uint32 RawVoiceDataSize = (NumAvailableVoiceData + OldVoiceRemainderSize) - ((NumAvailableVoiceData + OldVoiceRemainderSize) % sizeof(int16));

			UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("VoiceCapture got %i bytes of voice data. OldRemainderSize: %i"), NumAvailableVoiceData, OldVoiceRemainderSize);

			if (RawVoiceDataSize > 0)
			{
				uint32 NumCompressedVoiceData = CompressedVoiceData.Num(); // OnsetVoip::MaxCompressedBufferSize

				// Compress raw voice data. Pass NumCompressedVoiceData, which contains the maximum compression buffer size. The function sets the new size of the compressed data.
				const int32 NewVoiceRemainderSize = VoiceEncoder->Encode(RawVoiceData.GetData(), RawVoiceDataSize, CompressedVoiceData.GetData(), NumCompressedVoiceData);

				// If not all data was compressed save it for the next run.
				if (NewVoiceRemainderSize > 0)
				{
					VoiceRemainder.Append(RawVoiceData.GetData() + (RawVoiceDataSize - NewVoiceRemainderSize), NewVoiceRemainderSize);
				}

				if (NumCompressedVoiceData > 0)
				{
					UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("Encoded %i bytes of raw voice data to %i bytes. NewRemainderSize: %i"), RawVoiceDataSize - NewVoiceRemainderSize, NumCompressedVoiceData, NewVoiceRemainderSize);

					const TArrayView<uint8> CapturedVoiceView(RawVoiceData.GetData(), RawVoiceDataSize - NewVoiceRemainderSize);
					Native_OnVoipMicrophoneAudioCaptured.Broadcast(CapturedVoiceView);

					const float CurrentAmplitude = VoiceCapture->GetCurrentAmplitude();
					UOnsetVoipDataChannel::ReplicateLocalVoipPacket(LocalPlayer, MakeShared<FOnsetVoipPacket>(CompressedVoiceData.GetData(), NumCompressedVoiceData, HintVoiceChannels, CurrentAmplitude));

					TimeLastVoiceCaptured = FPlatformTime::Seconds();

					if (!bCachedTalking)
					{
						bCachedTalking = true;
						OnVoipTalkingStateChange.Broadcast(true);
					}
				}
			}
		}
	}

	if (bCachedTalking)
	{
		if ((FPlatformTime::Seconds() - TimeLastVoiceCaptured) > OnsetVoip::TimeLocalTalkerConsideredStoppedTalking)
		{
			bCachedTalking = false;
			OnVoipTalkingStateChange.Broadcast(false);
		}
	}

	return true;
}

#endif /* !UE_SERVER */

bool UOnsetVoipLocalPlayerSubsystem::StartRecording(const FString& Filename, EOnsetAudioRecordingFormat RecordingFormat)
{
#if !UE_SERVER
	if (VoipRecorder.IsValid())
	{
		return false;
	}

	VoipRecorder = CreateAudioRecorder<FOnsetVoipRecorderLocal, UOnsetVoipLocalPlayerSubsystem>(this, RecordingFormat, Filename.Len() > 0 ? Filename : TOptional<FString>());

	return VoipRecorder.IsValid();
#else
	return false;
#endif
}

bool UOnsetVoipLocalPlayerSubsystem::StopRecording()
{
#if !UE_SERVER
	if (!VoipRecorder.IsValid())
	{
		return false;
	}

	VoipRecorder = nullptr;

	return true;
#else
	return false;
#endif
}

bool UOnsetVoipLocalPlayerSubsystem::IsRecording() const
{
#if !UE_SERVER
	return VoipRecorder.IsValid();
#else
	return false;
#endif
}

FString UOnsetVoipLocalPlayerSubsystem::GetRecordingFile() const
{
#if !UE_SERVER
	return VoipRecorder.IsValid() ? VoipRecorder->GetRecordingFile() : TEXT("");
#else
	return TEXT("");
#endif
}

#if !UE_SERVER
void UOnsetVoipLocalPlayerSubsystem::PrintDebugInformation() const
{
	if (VoiceCapture.IsValid())
	{
		VoiceCapture->DumpState();
	}
	if (VoiceEncoder.IsValid())
	{
		VoiceEncoder->DumpState();
	}

	UE_LOG(LogOnsetVoip, Log, TEXT("IsCapturingVoice() = %i"), IsCapturingVoice());
	UE_LOG(LogOnsetVoip, Log, TEXT("IsTalking() = %i"), IsTalking());
	UE_LOG(LogOnsetVoip, Log, TEXT("GetCurrentAmplitude() = %f"), GetCurrentAmplitude());
	UE_LOG(LogOnsetVoip, Log, TEXT("IsRecording() = %i"), IsRecording());
	UE_LOG(LogOnsetVoip, Log, TEXT("VoiceRemainderSize = %i"), VoiceRemainder.Num());
	UE_LOG(LogOnsetVoip, Log, TEXT("TimeSinceLastCapture = %f"), TimeLastVoiceCaptured > 0.0 ? FPlatformTime::Seconds() - TimeLastVoiceCaptured : 0.0);
}

void UOnsetVoipLocalPlayerSubsystem::PrintVoiceRelatedCVarsToLog()
{
	const TArray<FString> FloatValueCVarsNames = {
		"voice.SilenceDetectionAttackTime",
		"voice.SilenceDetectionReleaseTime",
		"voice.SilenceDetectionThreshold",
		"voice.MicNoiseAttackTime",
		"voice.MicNoiseReleaseTime",
		"voice.MicNoiseGateThreshold",
		"voice.MicInputGain2",
		"voice.MicInputAGC"
	};

	for (const FString& CVarName : FloatValueCVarsNames)
	{
		if (const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName))
		{
			UE_LOG(LogOnsetVoip, Log, TEXT("%s = %f"), *CVarName, CVar->GetFloat());
		}
	}
}
#endif
