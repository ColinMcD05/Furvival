// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetVoipSettings.h"
#include "UObject/ConstructorHelpers.h"

UOnsetVoipSettings::UOnsetVoipSettings()
{
	SectionName = "OnsetVoip";

	VoiceCaptureSampleRate = 24000;
	VoiceCaptureChannels = 1;

	FNameplateAttachParam AttachNameplateSocket;
	AttachNameplateSocket.AttachSocketName = TEXT("nameplate");
	NameplateAttachParams.Add(AttachNameplateSocket);

	FNameplateAttachParam AttachHead;
	AttachHead.AttachSocketName = TEXT("head");
	AttachHead.bAdjustAttachZAccordingToActorBounds = true;
	NameplateAttachParams.Add(AttachHead);

	static ConstructorHelpers::FObjectFinderOptional<USoundAttenuation> DefaultSoundAttenuation3D_Finder(TEXT("/OnsetVoip/SA_VoIP_3D"));
	if (DefaultSoundAttenuation3D_Finder.Succeeded())
	{
		DefaultSoundAttenuation3D = DefaultSoundAttenuation3D_Finder.Get();
	}
	
	static ConstructorHelpers::FClassFinder<UUserWidget> DefaultNameplateWidget_Finder(TEXT("/OnsetVoip/WBP_Nameplate"));
	if (DefaultNameplateWidget_Finder.Succeeded())
	{
		DefaultNameplateWidget = DefaultNameplateWidget_Finder.Class;
	}
	
	DefaultOnsetVoipAudioComponentClass2D = UOnsetVoipAudioComponent2D::StaticClass();
	DefaultOnsetVoipAudioComponentClass3D = UOnsetVoipAudioComponent::StaticClass();

	DefaultOnsetVoipTalkerClass = UOnsetVoipTalker::StaticClass();
	DefaultNameplateComponentClass = UOnsetNameplateComponent::StaticClass();

	NameplateMaxDrawDistance = 1500.0;
	InitialPlaybackBufferDelay = 0.08;
	MaxBufferPlaybackDelay = 0.18;
	NumberOfFramesToStopPlaybackIfNoAudio = 100;
	bUseDistanceBasedRelevancy = false;
	MaxReplicationDistance = 5000.0;
	DefaultSoundWaveVolume = 2.0f;
}
