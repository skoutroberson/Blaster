// Fill out your copyright notice in the Description page of Project Settings.


#include "Casing.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

ACasing::ACasing()
{
	PrimaryActorTick.bCanEverTick = false;

	CasingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CasingMesh"));
	SetRootComponent(CasingMesh);
	CasingMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CasingMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
	CasingMesh->SetSimulatePhysics(true);
	CasingMesh->SetEnableGravity(true);
	CasingMesh->SetNotifyRigidBodyCollision(true);
	ShellEjectionImpulse = 10.f;
}

void ACasing::BeginPlay()
{
	Super::BeginPlay();

	CasingMesh->OnComponentHit.AddDynamic(this, &ACasing::OnHit);

	float RandRot = FMath::FRandRange(-15.f, 15.f);
	FRotator DeltaRot(RandRot, RandRot, RandRot);
	SetActorRotation(GetActorRotation() + DeltaRot);
	
	CasingMesh->AddImpulse(GetActorForwardVector() * ShellEjectionImpulse);

	GetWorldTimerManager().SetTimer(DestroyTimerHandle, this, &ACasing::DestroyCasing, 0.75f, false, 0.75f);
}

void ACasing::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (ShellSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ShellSound, GetActorLocation());
	}
	CasingMesh->SetNotifyRigidBodyCollision(false);

	//Destroy();
}

void ACasing::DestroyCasing()
{
	Destroy();
}

