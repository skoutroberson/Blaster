// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundCue.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Blaster.h"
#include "Components/SkeletalMeshComponent.h"

AProjectile::AProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECC_SkeletalMesh, ECollisionResponse::ECR_Block);

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
}

void AProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	if (Tracer)
	{
		TracerComponent = UGameplayStatics::SpawnEmitterAttached(
			Tracer,
			CollisionBox,
			FName(),
			GetActorLocation(),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition
		);
	}

	if (HasAuthority())
	{
		CollisionBox->OnComponentHit.AddDynamic(this, &AProjectile::OnHit);
	}
}

void AProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	/*
	bool bHitPlayer = false;
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	if (BlasterCharacter)
	{
		bHitPlayer = true;
	}
	if(HasAuthority())
	{
		Multicast_OnHit(bHitPlayer);
	}
	*/
	int32 HitBoneIndex;
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	if (BlasterCharacter && !Hit.BoneName.IsNone())
	{
		HitBoneIndex = BlasterCharacter->GetMesh()->GetBoneIndex(Hit.BoneName);
	}
	if (HasAuthority())
	{
		if (HitBoneIndex != INDEX_NONE)
		{
			Multicast_OnHit(HitBoneIndex, BlasterCharacter);
		}
		else
		{
			Multicast_OnHit();
		}
	}

	Destroy();
}


void AProjectile::Multicast_OnHit_Implementation(int32 HitBone, ACharacter* HitCharacter)
{
	// spawn blood if projectile hit player, or other particles if not
	if (HitBone != INDEX_NONE && HitCharacter != nullptr)
	{
		FTransform BloodTransform;
		BloodTransform.SetLocation(GetActorLocation());
		BloodTransform.SetRotation(GetVelocity().ToOrientationQuat());
		
		const FName BoneName = HitCharacter->GetMesh()->GetBoneName(HitBone);
		const FVector BoneLocation = HitCharacter->GetMesh()->GetBoneLocation(BoneName);
		FHitResult HitResult;

		if (!BoneName.IsNone() && !BoneLocation.IsZero())
		{
			GetWorld()->LineTraceSingleByChannel(
				HitResult,
				GetActorLocation(),
				BoneLocation,
				ECC_Blood
			);
			if (HitResult.bBlockingHit)
			{
				BloodTransform.SetLocation(HitResult.ImpactPoint);
				BloodTransform.SetRotation(HitResult.ImpactNormal.ToOrientationQuat());
			}
			else
			{
				BloodTransform.SetLocation(BoneLocation);
			}
		}
			
		if (BloodParticles)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodParticles, BloodTransform);
		}
		if (ImpactBodySound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ImpactBodySound, GetActorLocation());
		}

		ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(HitCharacter);
		if (BlasterCharacter && BoneName.IsEqual(FName("head")))
		{
			BlasterCharacter->SetDeathAnimState(EDeathAnimState::EDAS_Headshot);
		}
		else
		{
			BlasterCharacter->SetDeathAnimState(BlasterCharacter->CalculateDeathAnimState(BloodTransform.GetLocation()));
		}
	}
	else
	{
		if (ImpactParticles)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, GetActorTransform());
		}
		if (ImpactSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
		}
	}
	Destroy();
}


void AProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AProjectile::Destroyed()
{
	Super::Destroyed();
	
	/*
	if (ImpactParticles)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, GetActorTransform());
	}
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}
	*/
}

