// Copyright (C) 2026 Blue Mountains. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerState.h"
#include "OnsetNameplateComponent.generated.h"

/**
 * The Onset nameplate component. Can be added manually to a pawn/character to prevent spawning and attaching one dynamically.
 */
UCLASS(Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class ONSETVOIP_API UOnsetNameplateComponent : public UWidgetComponent
{
	GENERATED_BODY()
	
public:
	UOnsetNameplateComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin USceneComponent Interface
	virtual void OnAttachmentChanged() override;
	//~ End USceneComponent Interface

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	// To test whether the owning pawn of the nameplate is actually visible to the player, we use UPrimitiveComponent::GetLastRenderTimeOnScreen() on a suitable component such as the character's skeletal mesh component.
	UFUNCTION(BlueprintCallable, Category = "OnsetVoip|Nameplate")
	virtual UPrimitiveComponent* FindComponentToTestLastRenderTimeOnScreen();

	// Use this to override the default value in the project settings.
	UPROPERTY(EditAnywhere, Category = "OnsetVoip|Nameplate")
	double NameplateMaxDrawDistance;

#if 0
	// See CachedComponentToTestRenderTime.
	UPrimitiveComponent* GetCachedComponentToTestRenderTime() const
	{
		return CachedComponentToTestRenderTime;
	}
#endif

	// Updates the player state this belongs to.
	void SetPlayerState(APlayerState* PlayerState);

protected:
	// Used to track whether the actor we are attached to is visible. This is determined by CachedComponentToTestRenderTime->GetLastRenderTimeOnScreen().
	bool bCachedIsParentActorOnScreenAndVisible = false;

	// A component of the actor we (OnsetNameplateComponent) are attached to and that we call GetLastRenderTimeOnScreen() on for determining whether we should show the nameplate.
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> CachedComponentToTestRenderTime = nullptr;

	TWeakObjectPtr<APlayerState> CachedPlayerState;

	bool bPlayerStateNeedsUpdate = false;
};
