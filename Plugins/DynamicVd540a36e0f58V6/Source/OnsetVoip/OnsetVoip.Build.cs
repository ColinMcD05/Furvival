// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

using System.Collections.Generic;
using UnrealBuildTool;

public class OnsetVoip : ModuleRules
{
	public OnsetVoip(ReadOnlyTargetRules Target) : base(Target)
	{
        // CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error; for >= 5.6
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        /*
         * Set to true to enable support for PixelStreaming.
         */
        bool bEnablePixelStreamingSupport = false;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
                "CoreOnline",
                "Engine",
                "DeveloperSettings",
                "UMG"
            }
		);

        if (bEnablePixelStreamingSupport && Target.Type != TargetType.Server)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "PixelStreaming",
                    "AudioPlatformConfiguration" // For Audio::Resample
                }
            );
            PublicDefinitions.Add("ONSETVOIP_ENABLE_PIXELSTREAMING=1");
        }
        else
        {
            PublicDefinitions.Add("ONSETVOIP_ENABLE_PIXELSTREAMING=0");
        }

        if (Target.Type != TargetType.Server)
		{
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "AudioExtensions",
                    "AudioCaptureCore"
                }
            );

            if (Target.Platform == UnrealTargetPlatform.Android)
            {
                PublicDependencyModuleNames.Add("AndroidPermission");
            }
        }

        /*
         * This definition controls if network incoming voice is processed.
         * Processing includes:
         * - Decoding of compressed voice data.
         * - Tracking talking state and calling delegates. (UOnsetVoipTalker::IsTalking, GetLastVoipRelevancy, OnVoipAudioDataReceived, GetCurrentAmplitude)
         * - Calling OnVoipAudioDataReceived and OnVoipTalkingStateChange delegates.
         * - Playback of audio (Client/ListenServer only)
         * - Recording features.
         * 
         * Replication is not affected by this.
         */

        bool bEnableVoiceProcessing = true;
        if (Target.Type == TargetType.Server)
        {
            // If you need the above features on a dedicated server then you can enable it here.
            bEnableVoiceProcessing = false;
        }

        PublicDefinitions.Add("ONSETVOIP_ENABLE_VOICE_PROCESSING=" + (bEnableVoiceProcessing ? '1' : '0'));
        if (bEnableVoiceProcessing)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Voice",
                    "SignalProcessing" // For "DSP/" headers currently used for recording single audio file server side.
                }
            );
        }
    }
}
