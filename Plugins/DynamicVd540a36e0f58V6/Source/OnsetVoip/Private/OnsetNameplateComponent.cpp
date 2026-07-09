// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#include "OnsetNameplateComponent.h"
#include "OnsetVoip.h"
#include "OnsetVoipSettings.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "OnsetVoipNameWidgetInterface.h"
#include "Runtime/Launch/Resources/Version.h"

static FName Name_NameplateComponentToTestRenderTime(TEXT("NameplateComponentToTestRenderTime"));
static FName Name_LegacyNameplateComponentToTestRenderTime(TEXT("NameplateComponenetToTestRenderTime"));

UOnsetNameplateComponent::UOnsetNameplateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Space = EWidgetSpace::Screen;
	bEnableAutoLODGeneration = false;
	bVisibleInReflectionCaptures = false;
	bVisibleInRealTimeSkyCaptures = false;
	bVisibleInRayTracing = false;
	bReceivesDecals = false;
	bUseAsOccluder = false;
	CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
	BoundsScale = 0.0f;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	SetGenerateOverlapEvents(false);
	bCanEverAffectNavigation = false;
	bAllowCullDistanceVolume = false;
	bAutoActivate = true;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.02f;

	NameplateMaxDrawDistance = -1.0;
}

void UOnsetNameplateComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();

	UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("%s::OnAttachmentChanged Owner: %s, AttachParentActor: %s"), *GetPathName(), *GetNameSafe(GetOwner()), *GetNameSafe(GetAttachParentActor()));

	// On attachment change do an update
	bCachedIsParentActorOnScreenAndVisible = false;
	CachedComponentToTestRenderTime = FindComponentToTestLastRenderTimeOnScreen();
}

void UOnsetNameplateComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("%s::BeginPlay()"), *GetPathName());

	if (NameplateMaxDrawDistance < 0.0)
	{
		NameplateMaxDrawDistance = GetDefault<UOnsetVoipSettings>()->NameplateMaxDrawDistance;
	}
}

void UOnsetNameplateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	UE_LOG(LogOnsetVoip, VeryVerbose, TEXT("%s::EndPlay(%i)"), *GetPathName(), int(EndPlayReason));
}

void UOnsetNameplateComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CachedComponentToTestRenderTime && CachedPlayerState.IsValid())
	{
		if (const UWorld* World = GetWorld())
		{
			if (const APlayerController* PC = World->GetFirstPlayerController())
			{
				bCachedIsParentActorOnScreenAndVisible = (World->TimeSeconds - CachedComponentToTestRenderTime->GetLastRenderTimeOnScreen()) < (0.1f + DeltaTime);

				if (bCachedIsParentActorOnScreenAndVisible && NameplateMaxDrawDistance > 0.0)
				{
					if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
					{
						bCachedIsParentActorOnScreenAndVisible = (CameraManager->GetCameraLocation() - GetComponentLocation()).SizeSquared() < (NameplateMaxDrawDistance * NameplateMaxDrawDistance);
					}
				}

				// SetVisibility internally checks if the visibility changed so we don't have to check.
				SetVisibility(bCachedIsParentActorOnScreenAndVisible);
			}
		}
	}

	if (bPlayerStateNeedsUpdate)
	{
		if (UUserWidget* _Widget = GetWidget())
		{
			bPlayerStateNeedsUpdate = false;

			if (_Widget->Implements<UOnsetVoipNameWidgetInterface>())
			{
				IOnsetVoipNameWidgetInterface::Execute_SetOwningPlayerState(_Widget, CachedPlayerState.Get());
			}
		}
	}
}

UPrimitiveComponent* UOnsetNameplateComponent::FindComponentToTestLastRenderTimeOnScreen()
{
	UPrimitiveComponent* PrimitiveComponent = nullptr;

	if (const AActor* AttachedParentActor = GetAttachParentActor())
	{
		UE_CLOG(AttachedParentActor->IsA(APawn::StaticClass()) == false, LogOnsetVoip, Error, TEXT("%s component is attached to a non pawn actor %s."), *GetName(), *AttachedParentActor->GetName());

		// First try to see if a component was tagged to be used.
#if ENGINE_MINOR_VERSION >= 3
		PrimitiveComponent = AttachedParentActor->FindComponentByTag<UPrimitiveComponent>(Name_NameplateComponentToTestRenderTime);
		if (PrimitiveComponent == nullptr)
		{
			PrimitiveComponent = AttachedParentActor->FindComponentByTag<UPrimitiveComponent>(Name_LegacyNameplateComponentToTestRenderTime);
		}
#else
		TArray<UActorComponent*> TaggedComponents = AttachedParentActor->GetComponentsByTag(UPrimitiveComponent::StaticClass(), Name_NameplateComponentToTestRenderTime);
		if (TaggedComponents.Num() == 0)
		{
			TaggedComponents = AttachedParentActor->GetComponentsByTag(UPrimitiveComponent::StaticClass(), Name_LegacyNameplateComponentToTestRenderTime);
		}
		if (TaggedComponents.Num() > 0)
		{
			PrimitiveComponent = Cast<UPrimitiveComponent>(TaggedComponents[0]);
		}
#endif

		if (PrimitiveComponent == nullptr)
		{
			// If it's a character with a visible mesh use that.
			if (const ACharacter* Character = Cast<ACharacter>(AttachedParentActor))
			{
				if (Character->GetMesh() && Character->GetMesh()->IsVisible())
				{
					PrimitiveComponent = Character->GetMesh();
				}
			}

			// If not found look for the first visible USkeletalMeshComponent.
			if (PrimitiveComponent == nullptr)
			{
				TArray<USkeletalMeshComponent*> SKMComponents;
				AttachedParentActor->GetComponents<USkeletalMeshComponent>(SKMComponents);

				for (USkeletalMeshComponent* Component : SKMComponents)
				{
					if (Component->IsVisible())
					{
						PrimitiveComponent = Component;
						break;
					}
				}
			}

			// If still not found look for the first visible UStaticMeshComponent.
			if (PrimitiveComponent == nullptr)
			{
				TArray<UStaticMeshComponent*> SMComponents;
				AttachedParentActor->GetComponents<UStaticMeshComponent>(SMComponents);

				for (UStaticMeshComponent* Component : SMComponents)
				{
					if (Component->IsVisible())
					{
						PrimitiveComponent = Component;
						break;
					}
				}
			}
		}

		UE_CLOG(PrimitiveComponent == nullptr, LogOnsetVoip, Warning, TEXT("Did not find a parent component for visibility testing. Either tag a mesh component with '%s' or ensure your pawn has a visible mesh. (Nameplate: %s)"), *Name_NameplateComponentToTestRenderTime.ToString(), *GetPathName());
		UE_CLOG(PrimitiveComponent != nullptr, LogOnsetVoip, VeryVerbose, TEXT("%s PrimitiveComponent = %s"), *GetName(), *GetNameSafe(PrimitiveComponent));
	}

	return PrimitiveComponent;
}

void UOnsetNameplateComponent::SetPlayerState(APlayerState* PlayerState)
{
	CachedPlayerState = PlayerState;
	
	SetVisibility(false);

	bPlayerStateNeedsUpdate = true;
}
