// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTInventory.h"
#include "UTProjectile.h"
#include "UTATypes.h"

#include "UTWeapon.generated.h"

USTRUCT(BlueprintType)
struct FInstantHitDamageInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageInfo")
	int32 Damage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageInfo")
	TSubclassOf<UDamageType> DamageType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageInfo")
	float Momentum;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageInfo")
	float TraceRange;

	FInstantHitDamageInfo()
		: Damage(10), TraceRange(10000.0f)
	{}
};

USTRUCT()
struct FDelayedProjectileInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TSubclassOf<AUTProjectile> ProjectileClass;

	UPROPERTY()
		FVector SpawnLocation;

	UPROPERTY()
		FRotator SpawnRotation;

	FDelayedProjectileInfo()
		: ProjectileClass(NULL), SpawnLocation(ForceInit), SpawnRotation(ForceInit)
	{}
};

UCLASS(Blueprintable, Abstract, NotPlaceable, Config = Game)
class UNREALTOURNAMENT_API AUTWeapon : public AUTInventory
{
	GENERATED_UCLASS_BODY()

	friend class UUTWeaponState;
	friend class UUTWeaponStateInactive;
	friend class UUTWeaponStateActive;
	friend class UUTWeaponStateEquipping;
	friend class UUTWeaponStateUnequipping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<class AUTWeaponAttachment> AttachmentType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, ReplicatedUsing = OnRep_Ammo, Category = "Weapon")
	int32 Ammo;
	/** handles weapon switch when out of ammo, etc
	 * NOTE: called on server if owner is locally controlled, on client only when owner is remote
	 */
	UFUNCTION()
	virtual void OnRep_Ammo();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Weapon")
	int32 MaxAmmo;
	/** ammo cost for one shot of each fire mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<int32> AmmoCost;

	/** projectile class for fire mode (if applicable) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray< TSubclassOf<AUTProjectile> > ProjClass;

	/** instant hit data for fire mode (if applicable) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<FInstantHitDamageInfo> InstantHitInfo;

	/** firing state for mode, contains core firing sequence and directs to appropriate global firing functions */
	UPROPERTY(Instanced, EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "Weapon")
	TArray<class UUTWeaponStateFiring*> FiringState;

	/** True for melee weapons affected by "stopping power" (momentum added for weapons that don't normally impart much momentum) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	bool bAffectedByStoppingPower;

	virtual	float GetImpartedMomentumMag(AActor* HitActor);

#if WITH_EDITORONLY_DATA
protected:
	/** class of firing state to use (workaround for editor limitations - editinlinenew doesn't work) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TArray< TSubclassOf<class UUTWeaponStateFiring> > FiringStateType;
public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreSave() override
	{
		Super::PreSave();
		ValidateFiringStates();
	}
	virtual void PostLoad() override
	{
		Super::PostLoad();
		ValidateFiringStates();
	}
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	void WeaponBPChanged(UBlueprint* BP);
	void ValidateFiringStates();
#endif

	virtual void Serialize(FArchive& Ar) override
	{
		// prevent AutoSwitchPriority from being serialized using non-config paths
		// without this any local user setting will get pushed to blueprints and then override other users' configuration
		float SavedSwitchPriority = AutoSwitchPriority;
		Super::Serialize(Ar);
		AutoSwitchPriority = SavedSwitchPriority;
	}

	/** Synchronize random seed on server and firing client so projectiles stay synchronized */
	virtual void NetSynchRandomSeed();

	/** time between shots, trigger checks, etc */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.1"))
	TArray<float> FireInterval;
	/** firing spread (random angle added to shots) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<float> Spread;
	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<USoundBase*> FireSound;
	/** looping (ambient) sound to set on owner while firing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<USoundBase*> FireLoopingSound;
	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<UAnimMontage*> FireAnimation;
	/** particle component for muzzle flash */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TArray<UParticleSystemComponent*> MuzzleFlash;
	/** particle system for firing effects (instant hit beam and such)
	 * particles will be sourced at FireOffset and a parameter HitLocation will be set for the target, if applicable
	 */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TArray<UParticleSystem*> FireEffect;
	/** optional effect for instant hit endpoint */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TArray< TSubclassOf<class AUTImpactEffect> > ImpactEffect;
	/** throttling for impact effects - don't spawn another unless last effect is farther than this away or longer ago than MaxImpactEffectSkipTime */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	float ImpactEffectSkipDistance;
	/** throttling for impact effects - don't spawn another unless last effect is farther than ImpactEffectSkipDistance away or longer ago than this */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	float MaxImpactEffectSkipTime;
	/** FlashLocation for last played impact effect */
	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	FVector LastImpactEffectLocation;
	/** last time an impact effect was played */
	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	float LastImpactEffectTime;

	/** first person mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubobjectPtr<USkeletalMeshComponent> Mesh;

	/** causes weapons fire to originate from the center of the player's view when in first person mode (and human controlled)
	 * in other cases the fire start point defaults to the weapon's world position
	 */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	bool bFPFireFromCenter;
	/** Firing offset from weapon for weapons fire. If bFPFireFromCenter is true and it's a player in first person mode, this is from the camera center */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FVector FireOffset;

	/** Delayed projectile information */
	UPROPERTY()
	FDelayedProjectileInfo DelayedProjectile;

	/** Spawn a delayed projectile, delayed because client ping above max forward prediction limit. */
	virtual void SpawnDelayedFakeProjectile();

	/** time to bring up the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float BringUpTime;
	/** time to put down the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float PutDownTime;

	/** equip anims */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UAnimMontage* BringUpAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UAnimMontage* PutDownAnim;

	/** weapon group - NextWeapon() picks the next highest group, PrevWeapon() the next lowest, etc
	 * generally, the corresponding number key is bound to access the weapons in that group
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	int32 Group;
	/** if the player acquires more than one weapon in a group, we assign a unique GroupSlot to keep a consistent order
	 * this value is only set on clients
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon")
	int32 GroupSlot;

	/** user set priority for auto switching and switch to best weapon functionality
	 * this value only has meaning on clients
	 */
	UPROPERTY(Config, Transient, BlueprintReadOnly, Category = "Weapon")
	float AutoSwitchPriority;

	/** return priority for human player auto weapon switch (on pickup if enabled, or switch to best weapon key)
	 * highest value weapon is selected
	 */
	float GetAutoSwitchPriority();

	/** Deactivate any owner spawn protection */
	virtual void DeactivateSpawnProtection();

	/** whether this weapon stays around by default when someone picks it up (i.e. multiple people can pick up from the same spot without waiting for respawn time) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	bool bWeaponStay;

	/** Base offset of first person mesh, cached from offset set up in blueprint. */
	UPROPERTY()
	FVector FirstPMeshOffset;

	/** Base relative rotation of first person mesh, cached from offset set up in blueprint. */
	UPROPERTY()
	FRotator FirstPMeshRotation;

	/** Scaling for 1st person weapon bob */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBob")
	float WeaponBobScaling;

	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;
	virtual void RegisterAllComponents() override
	{
		// don't register in game by default for perf, we'll manually call Super from AttachToOwner()
		if (GetWorld()->WorldType == EWorldType::Editor || GetWorld()->WorldType == EWorldType::Preview)
		{
			Super::RegisterAllComponents();
		}
		else
		{
			RootComponent = NULL; // this was set for the editor view, but we don't want it
		}
	}

	virtual UMeshComponent* GetPickupMeshTemplate_Implementation(FVector& OverrideScale) const override;

	void GotoState(class UUTWeaponState* NewState);
	/** notification of state change (CurrentState is new state)
	 * if a state change triggers another state change (i.e. within BeginState()/EndState()) this function will only be called once, when CurrentState is the final state
	 */
	virtual void StateChanged()
	{}

	/** firing entry point */
	virtual void StartFire(uint8 FireModeNum);
	virtual void StopFire(uint8 FireModeNum);
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerStartFire(uint8 FireModeNum);
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerStopFire(uint8 FireModeNum);

	virtual void BeginFiringSequence(uint8 FireModeNum);
	virtual void EndFiringSequence(uint8 FireModeNum);

	/** hook when the firing state starts; called on both client and server */
	UFUNCTION(BlueprintNativeEvent)
	void OnStartedFiring();
	/** hook for the return to active state (was firing, refire timer expired, trigger released and/or out of ammo)  */
	UFUNCTION(BlueprintNativeEvent)
	void OnStoppedFiring();
	/** hook for when the weapon has fired, the refire delay passes, and the user still wants to fire (trigger still down) so the firing loop will repeat */
	UFUNCTION(BlueprintNativeEvent)
	void OnContinuedFiring();
	/** blueprint hook for pressing one fire mode while another is currently firing (e.g. hold alt, press primary)
	 * CurrentFireMode == current, OtherFireMode == one just pressed
	 */
	UFUNCTION(BlueprintNativeEvent)
	void OnMultiPress(uint8 OtherFireMode);

	/** activates the weapon as part of a weapon switch
	 * (this weapon has already been set to Pawn->Weapon)
	 * @param OverflowTime - overflow from timer of previous weapon PutDown() due to tick delta
	 */
	virtual void BringUp(float OverflowTime = 0.0f);
	/** puts the weapon away as part of a weapon switch
	 * return false to prevent weapon switch (must keep this weapon equipped)
	 */
	virtual bool PutDown();

	/**Hook to do effects when the weapon is raised*/
	UFUNCTION(BlueprintImplementableEvent)
	void OnBringUp();

	/** allows blueprint to prevent the weapon from being switched away from */
	UFUNCTION(BlueprintImplementableEvent)
	bool eventPreventPutDown();

	/** attach the visuals to Owner's first person view */
	UFUNCTION(BlueprintNativeEvent)
	void AttachToOwner();
	/** detach the visuals from the Owner's first person view */
	UFUNCTION(BlueprintNativeEvent)
	void DetachFromOwner();

	/** return number of fire modes */
	virtual uint8 GetNumFireModes()
	{
		return FMath::Min3(255, FiringState.Num(), FireInterval.Num());
	}

	/** returns if the specified fire mode is a charged mode - that is, if the trigger is held firing will be delayed and the effect will improve in some way */
	virtual bool IsChargedFireMode(uint8 TestMode) const;

	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate) override;
	virtual void ClientGivenTo_Internal(bool bAutoActivate) override;

	virtual void Removed() override;
	virtual void ClientRemoved_Implementation() override;

	/** fires a shot and consumes ammo */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void FireShot();
	/** blueprint override for firing
	 * NOTE: do an authority check before spawning projectiles, etc as this function is called on both sides
	 */
	UFUNCTION(BlueprintImplementableEvent)
	bool FireShotOverride();

	/** play firing effects not associated with the shot's results (e.g. muzzle flash but generally NOT emitter to target) */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void PlayFiringEffects();
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void StopFiringEffects();

	/** play effects associated with the shot's impact given the impact point
	 * called only if FlashLocation has been set (instant hit weapon)
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void PlayImpactEffects(const FVector& TargetLoc);

	/** shared code between UTWeapon and UTWeaponAttachment to refine replicated FlashLocation into impact effect transform via trace */
	static FHitResult GetImpactEffectHit(APawn* Shooter, const FVector& StartLoc, const FVector& TargetLoc);

	/** return start point for weapons fire */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	virtual FVector GetFireStartLoc();
	/** return base fire direction for weapons fire (i.e. direction player's weapon is pointing) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	virtual FRotator GetBaseFireRotation();
	/** return adjusted fire rotation after accounting for spread, aim help, and any other secondary factors affecting aim direction (may include randomized components) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintPure, Category = "Weapon")
	FRotator GetAdjustedAim(FVector StartFireLoc);

	/** if owned by a human, set AUTPlayerController::LastShotTargetGuess to closest target to player's aim */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = AI)
	void GuessPlayerTarget(const FVector& StartFireLoc, const FVector& FireDir);

	/** add (or remove via negative number) the ammo held by the weapon */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	virtual void AddAmmo(int32 Amount);

	/** use up AmmoCost units of ammo for the current fire mode
	 * also handles triggering auto weapon switch if out of ammo
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	virtual void ConsumeAmmo(uint8 FireModeNum);

	virtual void FireInstantHit(bool bDealDamage = true, FHitResult* OutHit = NULL);

	UFUNCTION(BlueprintCallable, Category = Firing)
	void K2_FireInstantHit(bool bDealDamage, FHitResult& OutHit);

	/** Handles rewind functionality for net games with ping prediction */
	virtual void HitScanTrace(FVector StartLocation, FVector EndTrace, FHitResult& Hit, float PredictionTime);

	UFUNCTION(BlueprintCallable, Category = Firing)
	virtual AUTProjectile* FireProjectile();

	/** Spawn a projectile on both server and owning client, and forward predict it by 1/2 ping on server. */
	virtual AUTProjectile* SpawnNetPredictedProjectile(TSubclassOf<AUTProjectile> ProjectileClass, FVector SpawnLocation, FRotator SpawnRotation);

	/** returns whether we can meet AmmoCost for the given fire mode */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual bool HasAmmo(uint8 FireModeNum);

	/** returns whether we have ammo for any fire mode */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual bool HasAnyAmmo();

	/** get interval between shots, including any fire rate modifiers */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual float GetRefireTime(uint8 FireModeNum);

	inline uint8 GetCurrentFireMode()
	{
		return CurrentFireMode;
	}

	inline void GotoActiveState()
	{
		GotoState(ActiveState);
	}

	void GotoFireMode(uint8 NewFireMode);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool IsFiring() const;

	virtual bool StackPickup_Implementation(AUTInventory* ContainedInv) override;

	/** update any timers and such due to a weapon timing change; for example, a powerup that changes firing speed */
	virtual void UpdateTiming();

	virtual void Tick(float DeltaTime) override;

	virtual void Destroyed() override;

	virtual void DropFrom(const FVector& StartLocation, const FVector& TossVelocity) override
	{
		if (!HasAnyAmmo())
		{
			Destroy();
		}
		else
		{
			Super::DropFrom(StartLocation, TossVelocity);
		}
	}

	/** we added an editor tool to allow the user to set the MuzzleFlash entries to a component created in the blueprint components view,
	 * but the resulting instances won't be automatically set...
	 * so we need to manually hook it all up
	 * static so we can share with UTWeaponAttachment
	 */
	static void InstanceMuzzleFlashArray(AActor* Weap, TArray<UParticleSystemComponent*>& MFArray);

	inline UUTWeaponState* GetCurrentState()
	{
		return CurrentState;
	}

	/** returns scaling of head for headshot effects
	 * NOTE: not used by base weapon implementation; stub is here for subclasses and firemodes that use it to avoid needing to cast to a specific weapon type
	 */
	virtual float GetHeadshotScale() const
	{
		return 0.0f;
	}

	UFUNCTION(BlueprintNativeEvent)
	void DrawWeaponInfo(UUTHUDWidget* WeaponHudWidget, float RenderDelta);

	/** returns crosshair color taking into account user settings, red flash on hit, etc */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	FLinearColor GetCrosshairColor(UUTHUDWidget* WeaponHudWidget) const;

	/** The player state of the player currently under the crosshair */
	AUTPlayerState* TargetPlayerState;

	/** The time this player was last seen under the crosshaiar */
	float TargetLastSeenTime;

	/** returns whether we should draw the friendly fire indicator on the crosshair */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	bool ShouldDrawFFIndicator(APlayerController* Viewer, AUTPlayerState *& HitPlayerState ) const;

	UFUNCTION(BlueprintNativeEvent)
	void DrawWeaponCrosshair(UUTHUDWidget* WeaponHudWidget, float RenderDelta);

	UFUNCTION()
	void UpdateCrosshairTarget(AUTPlayerState* NewCrosshairTarget, UUTHUDWidget* WeaponHudWidget, float RenderDelta);

	/** helper for shared overlay code between UTWeapon and UTWeaponAttachment
	 * NOTE: can called on default object!
	 */
	virtual void UpdateOverlaysShared(AActor* WeaponActor, AUTCharacter* InOwner, USkeletalMeshComponent* InMesh, USkeletalMeshComponent*& InOverlayMesh) const;
	/** read WeaponOverlayFlags from owner and apply the appropriate overlay material (if any) */
	virtual void UpdateOverlays();

	/** set main skin override for the weapon, NULL to restore to default */
	virtual void SetSkin(UMaterialInterface* NewSkin);

	/** HUD icon for e.g. weapon bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	FCanvasIcon HUDIcon;

	/** human readable localized name for the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FText DisplayName;

	//*********
	// Rotation Lag/Lead

	/** Previous frame's weapon rotation */
	UPROPERTY()
	FRotator LastRotation;

	/** Saved values used for lagging weapon rotation */
	UPROPERTY()
	float	OldRotDiff[2];
	UPROPERTY()
	float	OldLeadMag[2];
	UPROPERTY()
	float	OldMaxDiff[2];

	/** How fast Weapon Rotation offsets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Rotation")
	float	RotChgSpeed; 

	/** How fast Weapon Rotation returns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Rotation")
	float	ReturnChgSpeed;

	/** Max Weapon Rotation Yaw offset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Rotation")
	float	MaxYawLag;

	/** Max Weapon Rotation Pitch offset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Rotation")
	float	MaxPitchLag;

	/** @return whether the weapon's rotation is allowed to lag behind the holder's rotation */
	virtual bool ShouldLagRot();

	/** Lag a component of weapon rotation behind player's rotation. */
	virtual float LagWeaponRotation(float NewValue, float LastValue, float DeltaTime, float MaxDiff, int Index);

	/** Begin unequipping this weapon */
	virtual void UnEquip();

	virtual float BotDesireability_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const;
	virtual float DetourWeight_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const;
	/** base weapon selection rating for AI
	 * this is often used to determine if the AI has a good enough weapon to not pursue further pickups,
	 * since GetAIRating() will fluctuate wildly depending on the combat scenario
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	float BaseAISelectRating;
	/** AI switches to the weapon that returns the highest rating */
	UFUNCTION(BlueprintNativeEvent, Category = AI)
	float GetAISelectRating();

	/** returns whether this weapon has a viable attack against Target
	 * this function should not consider Owner's view rotation
	 * @param Target - Target actor
	 * @param TargetLoc - Target location, not guaranteed to be Target's true location (AI may pass a predicted or guess location)
	 * @param bDirectOnly - if true, only return success if weapon can directly hit Target from its current location (i.e. no need to wait for owner or target to move, no bounce shots, etc)
	 * @param bPreferCurrentMode - if true, only change BestFireMode if current value can't attack target but another mode can
	 * @param BestFireMode (in/out) - (in) current fire mode bot is set to use; (out) the fire mode that would be best to use for the attack
	 * @param OptimalTargetLoc (out) - best position to shoot at to hit TargetLoc (generally TargetLoc unless weapon has an indirect or special attack that doesn't require pointing at the target)
	 */
	UFUNCTION(BlueprintNativeEvent, Category = AI)
	bool CanAttack(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, UPARAM(ref) uint8& BestFireMode, UPARAM(ref) FVector& OptimalTargetLoc);

	/** convenience redirect if the out params are not needed (just checking if firing is a good idea)
	 * would prefer to use pointer params but Blueprints don't support that
	 */
	inline bool CanAttack(AActor* Target, const FVector& TargetLoc, bool bDirectOnly)
	{
		uint8 UnusedFireMode;
		FVector UnusedOptimalLoc;
		return CanAttack(Target, TargetLoc, bDirectOnly, false, UnusedFireMode, UnusedOptimalLoc);
	}

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	UUTWeaponState* CurrentState;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	uint8 CurrentFireMode;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	TSubobjectPtr<UUTWeaponState> ActiveState;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	TSubobjectPtr<class UUTWeaponStateEquipping> EquippingState;
	UPROPERTY(Instanced, BlueprintReadOnly,  Category = "States")
	TSubobjectPtr<UUTWeaponState> UnequippingState;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	TSubobjectPtr<UUTWeaponState> InactiveState;

	void GotoEquippingState(float OverflowTime);

	/** overlay mesh for overlay effects */
	UPROPERTY()
	USkeletalMeshComponent* OverlayMesh;
};