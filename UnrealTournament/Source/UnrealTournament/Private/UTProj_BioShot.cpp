// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_BioShot.h"
#include "UnrealNetwork.h"
#include "UTImpactEffect.h"
#include "UTLift.h"
#include "UTWeap_LinkGun.h"
#include "UTProjectileMovementComponent.h"

static const float GOO_TIMER_TICK = 0.5f;

AUTProj_BioShot::AUTProj_BioShot(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProjectileMovement->InitialSpeed = 4000.0f;
	ProjectileMovement->MaxSpeed = 6000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bInitialVelocityInLocalSpace = true;
	ProjectileMovement->ProjectileGravityScale = 0.9f;

	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->Bounciness = 0.1f;
	ProjectileMovement->Friction = 0.008f;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 140.f;

	DamageParams.BaseDamage = 21.0f;

	Momentum = 40000.0f;

	SurfaceNormal = FVector(0.0f, 0.0f, 1.0f);
	SurfaceType = EHitType::HIT_None;
	SurfaceWallThreshold = 0.3f;

	RestTime = 7.f;
	bAlwaysShootable = true;

	GlobStrength = 1.f;
	MaxRestingGlobStrength = 6;
	DamageRadiusGain = 60.0f;
	InitialLifeSpan = 0.f;
	ExtraRestTimePerStrength = 1.5f;

	SplashSpread = 0.8f;
	bSpawningGloblings = false;
	bLanded = false;
	bHasMerged = false;
	bCanTrack = false;
	WebLifeBoost = 20.f; // @TODO FIXMESTEVE reduce once have link recharging
	MaxLinkDistance = 1000.f;

	LandedOverlapRadius = 16.f;
	LandedOverlapScaling = 7.f;
	MaxSlideSpeed = 1500.f;
	TrackingRange = 2000.f;
	MaxTrackingSpeed = 1200.f;
	ProjectileMovement->HomingAccelerationMagnitude = 800.f;
	UUTProjectileMovementComponent* UTProjMovement = Cast<UUTProjectileMovementComponent>(ProjectileMovement);
	if (UTProjMovement)
	{
		UTProjMovement->bPreventZHoming = true;
	}
	GlobRadiusScaling = 4.f;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.SetTickFunctionEnable(false);

	BlobPulseTime = 0.f;
	InitialBlobPulseRate = 3.f;
	MaxBlobPulseRate = 8.f;
	BlobOverCharge = 0.f;
	BlobWebThreshold = 100.f;
	BlobPulseTime = 0.f;
	BlobPulseScaling = 0.1f;
}

void AUTProj_BioShot::BeginPlay()
{
	Super::BeginPlay();
	if (!IsPendingKillPending())
	{
		SetGlobStrength(GlobStrength);

		// failsafe if never land
		GetWorld()->GetTimerManager().SetTimer(this, &AUTProj_BioShot::BioStabilityTimer, RestTime, false);
		bSpawningGloblings = false;
		SurfaceNormal = GetActorRotation().Vector();
	}
}

void AUTProj_BioShot::OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	Super::OnBounce(ImpactResult, ImpactVelocity);
	ProjectileMovement->MaxSpeed = MaxSlideSpeed;
	bCanTrack = true;

	// only one bounce sound
	BounceSound = NULL;
}

void AUTProj_BioShot::Destroyed()
{
	Super::Destroyed();

	while (WebLinks.Num() > 0)
	{
		RemoveWebLink(WebLinks[0].LinkedBio);
	}

	// @TODO FIXMESTEVE - fix up WebChild list?
}

void AUTProj_BioShot::ShutDown()
{
	Super::ShutDown();
	if (!bPendingKillPending)
	{
		while (WebLinks.Num() > 0)
		{
			RemoveWebLink(WebLinks[0].LinkedBio);
		}
	}
}

void AUTProj_BioShot::RemoveWebLink(AUTProj_BioShot* LinkedBio)
{
	if (bRemovingWebLink)
	{
		// prevent re-entry
		return;
	}

	// find the associated link
	bool bFoundLink = false;
	int32 i = 0;
	for (i=0; i<WebLinks.Num(); i++)
	{
		if (WebLinks[i].LinkedBio == LinkedBio)
		{
			bFoundLink = true;
			break;
		}
	}
	if (WebLinkOne == LinkedBio)
	{
		WebLinkOne = NULL;
	}
	if (WebLinkTwo == LinkedBio)
	{
		WebLinkTwo = NULL;
	}

	if (bFoundLink)
	{
		// remove the link
		bRemovingWebLink = true;
		if (WebLinks[i].LinkedBio && !WebLinks[i].LinkedBio->IsPendingKillPending())
		{
			WebLinks[i].LinkedBio->RemoveWebLink(this);
			WebLinks[i].LinkedBio = NULL;
		}
		if (WebLinks[i].WebLink)
		{
			WebLinks[i].WebLink->DeactivateSystem();
			WebLinks[i].WebLink->bAutoDestroy = true;
		}
		WebLinks.RemoveAt(i);
		bRemovingWebLink = false;
	}
}

void AUTProj_BioShot::OnRep_WebLinkOne()
{
	if (WebLinkOne && !WebLinkOne->IsPendingKillPending() && !WebLinkOne->bExploded)
	{
		AddWebLink(WebLinkOne);
	}
}

void AUTProj_BioShot::OnRep_WebLinkTwo()
{
	if (WebLinkTwo && !WebLinkTwo->IsPendingKillPending() && !WebLinkTwo->bExploded)
	{
		AddWebLink(WebLinkTwo);
	}
}

void AUTProj_BioShot::OnRep_WebMaster()
{
	if (WebMaster && !WebMaster->IsPendingKillPending() && !WebMaster->bExploded)
	{
		AddWebLink(WebMaster);
	}
}

bool AUTProj_BioShot::AddWebLink(AUTProj_BioShot* LinkedBio)
{
	if (bAddingWebLink || !LinkedBio)
	{
		// no re-entry
		return false;
	}
	static FName NAME_HitLocation(TEXT("HitLocation"));
	static FName NAME_LocalHitLocation(TEXT("LocalHitLocation"));

	// find already existing associated link
	int32 i = 0;
	for (i = 0; i<WebLinks.Num(); i++)
	{
		if (WebLinks[i].LinkedBio == LinkedBio)
		{
			return false;
		}
	}

	bCanTrack = false;
	bAddingWebLink = true;
	PrimaryActorTick.SetTickFunctionEnable(true);
	if (WebLinks.Num() == 0)
	{
		// lifespan boost for being connected to web
		float RemainingRestTime = GetWorld()->GetTimerManager().GetTimerRemaining(this, &AUTProj_BioShot::BioStabilityTimer) + WebLifeBoost;
		GetWorld()->GetTimerManager().SetTimer(this, &AUTProj_BioShot::BioStabilityTimer, RemainingRestTime, false);
	}
	UParticleSystemComponent* NewWebLinkEffect = NULL;
	UUTGameplayStatics::UTPlaySound(GetWorld(), WebLinkSound, this, ESoundReplicationType::SRT_IfSourceNotReplicated);
	if (!LinkedBio->bAddingWebLink)
	{
		float Dist2D = (GetActorLocation() - LinkedBio->GetActorLocation()).Size2D();
		FVector Sag = /*(Dist2D > 200.f) ? FVector(0.f, 0.f, 0.25*Dist2D) :*/ FVector(0.f);
		NewWebLinkEffect = UGameplayStatics::SpawnEmitterAttached(WebLinkEffect, RootComponent, NAME_None, GetActorLocation(), (LinkedBio->GetActorLocation() - GetActorLocation() - Sag).Rotation(), EAttachLocation::KeepWorldPosition, false);
		if (NewWebLinkEffect)
		{
			NewWebLinkEffect->bAutoActivate = false;
			NewWebLinkEffect->bAutoDestroy = false;
			NewWebLinkEffect->SecondsBeforeInactive = 0.0f;
			NewWebLinkEffect->ActivateSystem();
			NewWebLinkEffect->SetVectorParameter(NAME_HitLocation, LinkedBio->GetActorLocation());
			NewWebLinkEffect->SetVectorParameter(NAME_LocalHitLocation, NewWebLinkEffect->ComponentToWorld.InverseTransformPositionNoScale(LinkedBio->GetActorLocation()));
			NewWebLinkEffect->SetWorldRotation((LinkedBio->GetActorLocation() - GetActorLocation()).Rotation());
		}
		// replication support (temp)
		if ((Role == ROLE_Authority) && (this != LinkedBio->WebMaster) && (WebMaster != LinkedBio))
		{
			if (!WebLinkOne)
			{
				WebLinkOne = LinkedBio;
			}
			else if (!WebLinkTwo)
			{
				WebLinkTwo = LinkedBio;
			}
			else if (!LinkedBio->WebLinkOne)
			{
				LinkedBio->WebLinkOne = this;
			}
			else if (!LinkedBio->WebLinkTwo)
			{
				LinkedBio->WebLinkTwo = this;
			}
		}
	}
	new(WebLinks) FBioWebLink(LinkedBio, NewWebLinkEffect);
	LinkedBio->AddWebLink(this);
	bAddingWebLink = false;
	return true;
}

bool AUTProj_BioShot::CanWebLinkTo(AUTProj_BioShot* LinkedBio)
{
	if (LinkedBio && !LinkedBio->IsPendingKillPending() && (GetActorLocation() - LinkedBio->GetActorLocation()).Size() < MaxLinkDistance)
	{
		// verify line of sight
		FHitResult Hit;
		static FName NAME_BioLinkTrace(TEXT("BioLinkTrace"));

		bool bBlockingHit = GetWorld()->LineTraceSingle(Hit, GetActorLocation() + 2.f*SurfaceNormal, LinkedBio->GetActorLocation() + 2.f*LinkedBio->SurfaceNormal, COLLISION_TRACE_WEAPON, FCollisionQueryParams(NAME_BioLinkTrace, false, this));
		return (!bBlockingHit || Cast<AUTProj_BioShot>(Hit.Actor.Get()));
	}
	return false;
}

float AUTProj_BioShot::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	if (Role == ROLE_Authority)
	{
		if (InstigatorController && EventInstigator && Cast<AUTWeap_LinkGun>(DamageCauser))
		{
			if ((GlobStrength >= 1.f) && bLanded)
			{
				AUTGameState* GameState = InstigatorController->GetWorld()->GetGameState<AUTGameState>();
				if ((EventInstigator == InstigatorController) || (GameState && GameState->OnSameTeam(InstigatorController, EventInstigator)))
				{
					// ignore link damage from teammates and start linking process
					AUTWeap_LinkGun *Linker = Cast<AUTWeap_LinkGun>(DamageCauser);
					if (Linker->LinkedBio && CanWebLinkTo(Linker->LinkedBio))
					{
						if (AddWebLink(Linker->LinkedBio))
						{
							if (Linker->LinkedBio->WebLinkOne && (Linker->LinkedBio->WebLinkOne != this))
							{
								AddWebLink(Linker->LinkedBio->WebLinkOne);
							}
							else if (Linker->LinkedBio->WebLinkTwo && (Linker->LinkedBio->WebLinkTwo != this))
							{
								AddWebLink(Linker->LinkedBio->WebLinkTwo);
							}
							else
							{
								for (int32 i = 0; i<Linker->LinkedBio->WebLinks.Num(); i++)
								{
									if ((Linker->LinkedBio->WebLinks[i].LinkedBio != this) && (Linker->LinkedBio->WebLinks[i].LinkedBio != NULL))
									{
										AddWebLink(Linker->LinkedBio->WebLinks[i].LinkedBio);
										break;
									}
								}
							}
						}

						// this costs ammo!
						// flash line when low, allow recharge
						// spider web trap springing
					}
					if (GlobStrength >= MaxRestingGlobStrength)
					{
						BlobOverCharge += DamageAmount;
						if (BlobOverCharge >= BlobWebThreshold)
						{
							FVector ShotDirection = (DamageEvent.IsOfType(FUTPointDamageEvent::ClassID))
								? ((const FUTPointDamageEvent&)DamageEvent).ShotDirection
								: FVector_NetQuantizeNormal(0.f, 0.f, 1.f);
							BlobToWeb(ShotDirection);
							return DamageAmount;
						}
					}
					Linker->LinkedBio = this;
				}
			}
			return 0.f;
		}
		if ((bLanded || TrackedPawn) && !bExploded)
		{
			Explode(GetActorLocation(), SurfaceNormal);
		}
	}
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AUTProj_BioShot::BlobToWeb(const FVector& NormalDir)
{
	FActorSpawnParameters Params;
	Params.Instigator = Instigator;
	Params.Owner = Instigator;

	//Spawn globlings for as many globs above 1, and weblink them
	bSpawningGloblings = true;
	//Adjust a bit so globlings don't spawn in the floor
	FVector FloorOffset = GetActorLocation() + (SurfaceNormal * 10.0f);
	int32 NumGloblings = int32(GlobStrength);
	FVector CrossVector = (NormalDir ^ SurfaceNormal).SafeNormal();

	for (int32 i = 0; i<NumGloblings; i++)
	{
		// 2d splash in plane with link beam as normal 
		float Offset = -0.3f + 1.5f*float(NumGloblings - 2.f*i) / float(NumGloblings);
		FVector Dir = SurfaceNormal * FMath::Cos(Offset) + CrossVector * FMath::Sin(Offset);
		AUTProj_BioShot* Globling = GetWorld()->SpawnActor<AUTProj_BioShot>(GetClass(), FloorOffset, Dir.Rotation(), Params);
		if (Globling)
		{
			Globling->bSpawningGloblings = true; 
			Globling->ProjectileMovement->InitialSpeed *= 0.4f;
			Globling->ProjectileMovement->Velocity *= 0.4f;
			Globling->WebMaster = this;
		}
	}
	bSpawningGloblings = false;
	SetGlobStrength(1.f);
}

void AUTProj_BioShot::BioStabilityTimer()
{
	if (!bExploded && (!bFakeClientProjectile || !MasterProjectile) && (Role== ROLE_Authority))
	{
		Explode(GetActorLocation(), SurfaceNormal);
	}
}

void AUTProj_BioShot::OnRep_BioLanded()
{
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->Velocity = FVector::ZeroVector;
}

void AUTProj_BioShot::Landed(UPrimitiveComponent* HitComp, const FVector& HitLocation)
{
	if (!bLanded)
	{
		bCanTrack = true;
		bLanded = true;
		bCanHitInstigator = true;

		//Change the collision so that weapons make it explode
		CollisionComp->SetCollisionProfileName("ProjectileShootable");

		//Rotate away from the floor
		FRotator NormalRotation = (SurfaceNormal).Rotation();
		NormalRotation.Roll = FMath::FRand() * 360.0f;	//Random spin
		SetActorRotation(NormalRotation);

		//Spawn Effects
		OnLanded();
		if ((LandedEffects != NULL) && (GetNetMode() != NM_DedicatedServer) && !MyFakeProjectile)
		{
			LandedEffects.GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(NormalRotation, HitLocation), HitComp, this, InstigatorController);
		}

		//Start the explosion timer
		if (GlobStrength < 1.f)
		{
			BioStabilityTimer();
		}
		else if (!bPendingKillPending)
		{
			//Stop the projectile
			ProjectileMovement->ProjectileGravityScale = 0.0f;
			ProjectileMovement->Velocity = FVector::ZeroVector;

			if (Role == ROLE_Authority)
			{
				bReplicateUTMovement = true;
				GetWorld()->GetTimerManager().SetTimer(this, &AUTProj_BioShot::BioStabilityTimer, RestTime + (GlobStrength - 1.f) * ExtraRestTimePerStrength, false);
				SetGlobStrength(GlobStrength);
			}

			if (WebMaster && CanWebLinkTo(WebMaster))
			{
				AddWebLink(WebMaster);

				if (Role == ROLE_Authority)
				{
					WebChild = WebMaster->WebChild;
					WebMaster->WebChild = this;

					// connect to furthest with no weblink yet
					AUTProj_BioShot* FurthestBio = NULL;
					float FurthestDist = 0.f;
					AUTProj_BioShot* NextBio = WebChild;
					while (NextBio)
					{
						if (!WebChild->IsPendingKillPending())
						{
							float Dist = (WebChild->GetActorLocation() - GetActorLocation()).SizeSquared();
							if (!FurthestBio || (Dist > FurthestDist))
							{
								FurthestBio = WebChild;
								FurthestDist = Dist;
							}
						}
						NextBio = NextBio->WebChild;
					}
					if (FurthestBio)
					{
						AddWebLink(FurthestBio);
					}
				}
			}
		}
	}
	// uncomment to easily test tracking (also remove instigator checks in Track())
	//Track(Cast<AUTCharacter>(Instigator));
}

void AUTProj_BioShot::OnLanded_Implementation()
{
}

bool AUTProj_BioShot::CanInteractWithBio()
{
	return (!bExploded && !bHasMerged && !bSpawningGloblings && !bFakeClientProjectile && (Role == ROLE_Authority));
}

void AUTProj_BioShot::MergeWithGlob(AUTProj_BioShot* OtherBio)
{
	if (OtherBio == NULL || bPendingKillPending || OtherBio->bPendingKillPending || !CanInteractWithBio() || !OtherBio->CanInteractWithBio() || (GlobStrength < 1.f))
	{
		//Let the globlings pass through so they dont explode the glob, ignore exploded bio
		return;
	}
	// let the landed glob grow
	if (!bLanded && OtherBio->bLanded)
	{
		return;
	}
	OtherBio->bHasMerged = true;

	if (OtherBio->GlobStrength > GlobStrength)
	{
		InstigatorController = OtherBio->InstigatorController;
		Instigator = OtherBio->Instigator;
	}

	SetGlobStrength(GlobStrength + OtherBio->GlobStrength);
	if (TrackedPawn != NULL && !TrackedPawn->bPendingKillPending && !TrackedPawn->GetCapsuleComponent()->IsPendingKill()) // we check in tick but it's still possible they get gibbed first
	{
		ProjectileMovement->bIsHomingProjectile = true;
		ProjectileMovement->SetUpdatedComponent(CollisionComp);
		ProjectileMovement->MaxSpeed = MaxTrackingSpeed / FMath::Sqrt(GlobStrength);
		ProjectileMovement->HomingTargetComponent = TrackedPawn->GetCapsuleComponent(); // warning: TWeakObjectPtr will return NULL if this is already pending kill!
		ProjectileMovement->bShouldBounce = true;
		bLanded = false;
		if (ProjectileMovement->Velocity.Size() < 1.5f*ProjectileMovement->BounceVelocityStopSimulatingThreshold)
		{
			ProjectileMovement->Velocity = 1.5f*ProjectileMovement->BounceVelocityStopSimulatingThreshold * (ProjectileMovement->HomingTargetComponent->GetComponentLocation() - GetActorLocation()).SafeNormal();
		}
	}
	else if (OtherBio->TrackedPawn)
	{
		Track(OtherBio->TrackedPawn);
	}
		
	OtherBio->Destroy();
}

void AUTProj_BioShot::Track(AUTCharacter* NewTrackedPawn)
{
	if (IsPendingKillPending() || !bCanTrack || !NewTrackedPawn || NewTrackedPawn->bPendingKillPending || ((NewTrackedPawn->GetActorLocation() - GetActorLocation()).SizeSquared() > FMath::Square(TrackingRange)) || (NewTrackedPawn == Instigator) || !ProjectileMovement)
	{
		return;
	}
	if (InstigatorController)
	{
		// don't track teammates
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS && GS->OnSameTeam(InstigatorController, NewTrackedPawn))
		{
			return;
		}
	}

	ProjectileMovement->ProjectileGravityScale = 0.9f;
	// track closest
	if (!TrackedPawn || !ProjectileMovement->HomingTargetComponent.IsValid() || ((ProjectileMovement->HomingTargetComponent->GetComponentLocation() - GetActorLocation()).SizeSquared() > (NewTrackedPawn->GetCapsuleComponent()->GetComponentLocation() - GetActorLocation()).SizeSquared()))
	{
		TrackedPawn = NewTrackedPawn;
		ProjectileMovement->bIsHomingProjectile = true;
		ProjectileMovement->SetUpdatedComponent(CollisionComp);
		ProjectileMovement->MaxSpeed = MaxTrackingSpeed / FMath::Sqrt(GlobStrength);
		ProjectileMovement->HomingTargetComponent = TrackedPawn->GetCapsuleComponent();
		ProjectileMovement->bShouldBounce = true;
		bLanded = false;
		PrimaryActorTick.SetTickFunctionEnable(true);

		if (ProjectileMovement->Velocity.Size() < 1.5f*ProjectileMovement->BounceVelocityStopSimulatingThreshold)
		{
			ProjectileMovement->Velocity = 1.5f*ProjectileMovement->BounceVelocityStopSimulatingThreshold * (ProjectileMovement->HomingTargetComponent->GetComponentLocation() - GetActorLocation()).SafeNormal();
		}
	}
}

void AUTProj_BioShot::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	if (ProjectileMovement->bIsHomingProjectile)
	{
		if (!ProjectileMovement->HomingTargetComponent.IsValid() || ((ProjectileMovement->HomingTargetComponent->GetComponentLocation() - GetActorLocation()).SizeSquared() > FMath::Square(1.5f * TrackingRange)))
		{
			ProjectileMovement->HomingTargetComponent = NULL;
			ProjectileMovement->bIsHomingProjectile = false;
			TrackedPawn = NULL;
			bLanded = true;
			PrimaryActorTick.SetTickFunctionEnable(false);
		}
	}
	else
	{
		for (int32 i = 0; i<WebLinks.Num(); i++)
		{
			if (WebLinks[i].LinkedBio) // && WebLinks[i].WebLink) @TODO FIXMESTEVE WHY NOT SET PROPERLY
			{
				FHitResult Hit;
				static FName NAME_BioLinkTrace(TEXT("BioLinkTrace"));
				bool bBlockingHit = GetWorld()->LineTraceSingle(Hit, GetActorLocation(), WebLinks[i].LinkedBio->GetActorLocation(), COLLISION_TRACE_WEAPON, FCollisionQueryParams(NAME_BioLinkTrace, false, this));
				if (Cast<AUTCharacter>(Hit.Actor.Get()))
				{
					ProcessHit(Hit.Actor.Get(), Hit.Component.Get(), Hit.Location, Hit.Normal);
					if (IsPendingKillPending())
					{
						return;
					}
					break;
				}
			}
		}

	}

	if ((GlobStrength >= MaxRestingGlobStrength) || ProjectileMovement->bIsHomingProjectile)
	{
		float PulseRate = InitialBlobPulseRate + BlobOverCharge * (MaxBlobPulseRate - InitialBlobPulseRate)/BlobWebThreshold;
		BlobPulseTime += DeltaTime * PulseRate;

		float GlobScalingSqrt = FMath::Sqrt(GlobStrength);
		FVector GlobScaling(GlobScalingSqrt);
		GlobScaling.X *= (1.f + BlobPulseScaling*FMath::Square(FMath::Cos(BlobPulseTime)));
		GlobScaling.Y = GlobScaling.X;
		GlobScaling.Z *= (1.f + BlobPulseScaling*FMath::Square(FMath::Sin(BlobPulseTime)));
		GetRootComponent()->SetRelativeScale3D(GlobScaling);
	}
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
}


void AUTProj_BioShot::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	// FIXME: temporary? workaround for synchronization issues on clients where it explodes on client but not on server
	if (GetNetMode() == NM_Client && (Cast<APawn>(OtherActor) != NULL || Cast<AUTProjectile>(OtherActor) != NULL))
	{
		return;
	}

	if (bTriggeringWeb)
	{
		return;
	}
	if (bHasMerged)
	{
		ShutDown(); 
		return;
	}

	AUTProj_BioShot* OtherBio = Cast<AUTProj_BioShot>(OtherActor);
	if (OtherBio)
	{
		MergeWithGlob(OtherBio);
	}
	else if (Cast<APawn>(OtherActor) != NULL || Cast<AUTProjectile>(OtherActor) != NULL)
	{
		AUTCharacter* TargetCharacter = Cast<AUTCharacter>(OtherActor);
		if (TargetCharacter && ((TargetCharacter != Instigator) || bCanHitInstigator))
		{
			if (!bFakeClientProjectile && (Role == ROLE_Authority) && (TargetCharacter != Instigator))
			{
				// tell nearby bio that is on ground @TODO FIXMESTEVE OPTIMIZE
				for (TActorIterator<AUTProj_BioShot> It(GetWorld()); It; ++It)
				{
					AUTProj_BioShot* Glob = *It;
					if (Glob != this)
					{
						Glob->Track(TargetCharacter);
					}
				}
			}
			bTriggeringWeb = true;
			for (int32 i = 0; i<WebLinks.Num(); i++)
			{
				if (WebLinks[i].LinkedBio)
				{
					WebLinks[i].LinkedBio->ProcessHit(OtherActor, OtherComp, HitLocation, HitNormal);
				}
			}
			// set different damagetype for charged shots
			MyDamageType = (GlobStrength > 1.f) ? ChargedDamageType : GetClass()->GetDefaultObject<AUTProjectile>()->MyDamageType;
			float GlobScalingSqrt = FMath::Sqrt(GlobStrength);
			DamageParams = GetClass()->GetDefaultObject<AUTProjectile>()->DamageParams;
			DamageParams.BaseDamage *= GlobStrength;
			DamageParams.OuterRadius += DamageRadiusGain * (GlobStrength - 1.0f);
			Momentum = GetClass()->GetDefaultObject<AUTProjectile>()->Momentum * GlobScalingSqrt;
		}
		Super::ProcessHit_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
	}
	else if (!bLanded)
	{
		//Determine if we hit a Wall/Floor/Ceiling
		SurfaceNormal = HitNormal;
		if (FMath::Abs(SurfaceNormal.Z) > SurfaceWallThreshold) // A wall will have a low Z in the HitNormal since it's a unit vector
		{
			SurfaceType = (SurfaceNormal.Z >= 0) ? HIT_Floor : HIT_Ceiling;
		}
		else
		{
			SurfaceType = HIT_Wall;
		}

		SetActorLocation(HitLocation + HitNormal);
		Landed(OtherComp, HitLocation);

		AUTLift* Lift = Cast<AUTLift>(OtherActor);
		if (Lift && Lift->GetEncroachComponent())
		{
			AttachRootComponentTo(Lift->GetEncroachComponent(), NAME_None, EAttachLocation::KeepWorldPosition);
		}
	}
}

void AUTProj_BioShot::OnRep_GlobStrength()
{
	if (Cast<AUTProj_BioShot>(MyFakeProjectile))
	{
		Cast<AUTProj_BioShot>(MyFakeProjectile)->SetGlobStrength(GlobStrength);
	}
	else
	{
		SetGlobStrength(GlobStrength);
	}
}

void AUTProj_BioShot::SetGlobStrength(float NewStrength)
{
	if (bHasMerged)
	{
		return;
	}
	float OldStrength = GlobStrength;
	GlobStrength = NewStrength;
	ProjectileMovement->bShouldBounce = (GlobStrength < 1.f);

	if (!bLanded && !TrackedPawn)
	{
		//Set the projectile speed here so the client can mirror the strength speed
		ProjectileMovement->InitialSpeed = ProjectileMovement->InitialSpeed * (0.4f + GlobStrength) / (1.35f * GlobStrength);
		ProjectileMovement->MaxSpeed = ProjectileMovement->InitialSpeed;
	}
	// don't reduce remaining time for strength lost (i.e. SplashGloblings())
	else if (GlobStrength > OldStrength)
	{
		if (Role == ROLE_Authority)
		{
			float RemainingRestTime = GetWorld()->GetTimerManager().GetTimerRemaining(this, &AUTProj_BioShot::BioStabilityTimer) + (GlobStrength - OldStrength) * ExtraRestTimePerStrength;
			GetWorld()->GetTimerManager().SetTimer(this, &AUTProj_BioShot::BioStabilityTimer, RemainingRestTime, false);

		}
		// Glob merge effects
		UUTGameplayStatics::UTPlaySound(GetWorld(), MergeSound, this, ESoundReplicationType::SRT_IfSourceNotReplicated);
		if (GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MergeEffect, GetActorLocation() - CollisionComp->GetUnscaledSphereRadius(), SurfaceNormal.Rotation(), true);
		}
	}

	//Increase The collision of the flying Glob if over a certain strength
	float GlobScalingSqrt = FMath::Sqrt(GlobStrength);
	if (GlobStrength > 3.f)
	{
		CollisionComp->SetSphereRadius(GlobRadiusScaling * GlobScalingSqrt);
	}

	if (bLanded && (GlobStrength > MaxRestingGlobStrength))
	{
		if (Role == ROLE_Authority)
		{
			FActorSpawnParameters Params;
			Params.Instigator = Instigator;
			Params.Owner = Instigator;

			//Adjust a bit so globlings don't spawn in the floor
			FVector FloorOffset = GetActorLocation() + (SurfaceNormal * 10.0f);

			//Spawn globlings for as many Glob's above MaxRestingGlobStrength
			bSpawningGloblings = true;
			int32 NumGloblings = int32(GlobStrength) - MaxRestingGlobStrength;
			for (int32 i = 0; i<NumGloblings; i++)
			{
				FVector Dir = SurfaceNormal + FMath::VRand() * SplashSpread;
				AUTProj_BioShot* Globling = GetWorld()->SpawnActor<AUTProj_BioShot>(GetClass(), FloorOffset, Dir.Rotation(), Params);
				if (Globling)
				{
					Globling->bSpawningGloblings = true;
					Globling->ProjectileMovement->InitialSpeed *= 0.2f;
					Globling->ProjectileMovement->Velocity *= 0.2f;
				}
			}
			bSpawningGloblings = false;
		}
		GlobStrength = MaxRestingGlobStrength;
	}

	//Update any effects
	OnSetGlobStrength();

	if (bLanded)
	{
		PawnOverlapSphere->SetSphereRadius(LandedOverlapRadius + LandedOverlapScaling*GlobScalingSqrt, false);
		//PawnOverlapSphere->bHiddenInGame = false;
		//PawnOverlapSphere->bVisible = true;
	}
	GetRootComponent()->SetRelativeScale3D(FVector(GlobScalingSqrt));
}

void AUTProj_BioShot::OnSetGlobStrength_Implementation()
{
}

void AUTProj_BioShot::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTProj_BioShot, GlobStrength, COND_None);
	DOREPLIFETIME_CONDITION(AUTProj_BioShot, WebLinkOne, COND_None);
	DOREPLIFETIME_CONDITION(AUTProj_BioShot, WebLinkTwo, COND_None);
	DOREPLIFETIME_CONDITION(AUTProj_BioShot, WebMaster, COND_None);
	DOREPLIFETIME(AUTProj_BioShot, bLanded);
}