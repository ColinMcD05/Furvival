// Copyright Epic Games, Inc. All Rights Reserved.
// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AudioCaptureBlueprintLibrary2.generated.h"

/**
 * Copied from AudioCaptureBlueprintLibrary.h to avoid linkage to AudioCapture module.
 * 
 * Platform audio input device info, in a Blueprint-readable format
 */
USTRUCT(BlueprintType)
struct ONSETVOIP_API FAudioInputDeviceInfo2
{
	GENERATED_USTRUCT_BODY()

	FAudioInputDeviceInfo2() :
		DeviceName(""),
		DeviceId(""),
		InputChannels(0),
		PreferredSampleRate(0),
		bSupportsHardwareAEC(true) 
	{};

	/** The name of the audio device */
	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	FString DeviceName;

	/** ID of the device. */
	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	FString DeviceId;

	/** The number of channels supported by the audio device */
	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	int32 InputChannels;

	/** The preferred sample rate of the audio device */
	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	int32 PreferredSampleRate;

	/** Whether or not the device supports Acoustic Echo Canceling */
	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	uint8 bSupportsHardwareAEC : 1;
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAudioInputDevicesObtained2, const TArray<FAudioInputDeviceInfo2>&, AvailableDevices);
DECLARE_DELEGATE_OneParam(FOnAudioInputDevicesObtained2Cpp, const TArray<FAudioInputDeviceInfo2>&);

/**
 * Class contains a fixed version of the GetAvailableAudioInputDevices in UAudioCaptureBlueprintLibrary.
 * Should be removed once fixed in Unreal.
 */
UCLASS()
class ONSETVOIP_API UAudioCaptureBlueprintLibrary2 : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	// Copied from engine because original function wrongly calls back from non gamethread resulting in a crash.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip", meta = (WorldContext = "WorldContextObject"))
	static void GetAvailableAudioInputDevicesFixed(const UObject* WorldContextObject, const FOnAudioInputDevicesObtained2& OnObtainDevicesEvent);
	
	static void GetAvailableAudioInputDevicesFixedCpp(const UObject* WorldContextObject, const FOnAudioInputDevicesObtained2Cpp& OnObtainDevicesEvent);
};
