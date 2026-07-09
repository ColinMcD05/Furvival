// Copyright Epic Games, Inc. All Rights Reserved.
// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "AudioCaptureBlueprintLibrary2.h"

#if !UE_SERVER
#include "Async/Async.h"
#include "AudioCaptureCore.h"
#include "AudioThread.h"
#endif

void UAudioCaptureBlueprintLibrary2::GetAvailableAudioInputDevicesFixed(const UObject* WorldContextObject, const FOnAudioInputDevicesObtained2& OnObtainDevicesEvent)
{
#if !UE_SERVER
	// Copied from AudioCaptureBlueprintLibrary.cpp

	FAudioThread::RunCommandOnAudioThread([WorldContextObject, OnObtainDevicesEvent]()
	{
		TArray<FAudioInputDeviceInfo2> AvailableDeviceInfos; //The array of audio device info to return

		Audio::FAudioCapture AudioCapture;
		TArray<Audio::FCaptureDeviceInfo> InputDevices;

		AudioCapture.GetCaptureDevicesAvailable(InputDevices);

		for (const Audio::FCaptureDeviceInfo& Device : InputDevices)
		{
			FAudioInputDeviceInfo2 Fixed;
			Fixed.DeviceName = Device.DeviceName;
			Fixed.DeviceId = Device.DeviceId;
			Fixed.InputChannels = Device.InputChannels;
			Fixed.PreferredSampleRate = Device.PreferredSampleRate;
			Fixed.bSupportsHardwareAEC = Device.bSupportsHardwareAEC;
			AvailableDeviceInfos.Add(MoveTemp(Fixed));
		}

		AsyncTask(ENamedThreads::GameThread, [WorldContextObject, OnObtainDevicesEvent, AvailableDeviceInfos]()
		{
			if (IsValid(WorldContextObject))
			{
				// Call delegate event, and send the info there
				OnObtainDevicesEvent.ExecuteIfBound(AvailableDeviceInfos);
			}
		});
	});
#endif
}

void UAudioCaptureBlueprintLibrary2::GetAvailableAudioInputDevicesFixedCpp(const UObject* WorldContextObject, const FOnAudioInputDevicesObtained2Cpp& OnObtainDevicesEvent)
{
#if !UE_SERVER
	// Copied from AudioCaptureBlueprintLibrary.cpp

	FAudioThread::RunCommandOnAudioThread([WorldContextObject, OnObtainDevicesEvent]()
	{
		TArray<FAudioInputDeviceInfo2> AvailableDeviceInfos; //The array of audio device info to return

		Audio::FAudioCapture AudioCapture;
		TArray<Audio::FCaptureDeviceInfo> InputDevices;

		AudioCapture.GetCaptureDevicesAvailable(InputDevices);

		for (const Audio::FCaptureDeviceInfo& Device : InputDevices)
		{
			FAudioInputDeviceInfo2 Fixed;
			Fixed.DeviceName = Device.DeviceName;
			Fixed.DeviceId = Device.DeviceId;
			Fixed.InputChannels = Device.InputChannels;
			Fixed.PreferredSampleRate = Device.PreferredSampleRate;
			Fixed.bSupportsHardwareAEC = Device.bSupportsHardwareAEC;
			AvailableDeviceInfos.Add(MoveTemp(Fixed));
		}

		AsyncTask(ENamedThreads::GameThread, [WorldContextObject, OnObtainDevicesEvent, AvailableDeviceInfos]()
		{
			if (IsValid(WorldContextObject))
			{
				// Call delegate event, and send the info there
				OnObtainDevicesEvent.ExecuteIfBound(AvailableDeviceInfos);
			}
		});
	});
#endif
}
