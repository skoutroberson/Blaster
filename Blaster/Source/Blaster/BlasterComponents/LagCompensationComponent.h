// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LagCompensationComponent.generated.h"

USTRUCT(BlueprintType)
struct FBoxInformation
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	FVector BoxExtent;
};

USTRUCT(BlueprintType)
struct FCapsuleInformation
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	float Radius;

	UPROPERTY()
	float Length;

};

USTRUCT(BlueprintType)
struct FFramePackage
{
	GENERATED_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY()
	TMap<FName, FBoxInformation> HitBoxInfo;

	UPROPERTY()
	ABlasterCharacter* Character;
};

USTRUCT()
struct FCapsuleInformations
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCapsuleInformation> CapsuleInfos;
};

USTRUCT(BlueprintType)
struct FFramePackageCapsule
{
	GENERATED_BODY()

	UPROPERTY()
	float Time;
	UPROPERTY()
	TMap<FName, FCapsuleInformations> HitCapsulesInfo;

	UPROPERTY()
	ABlasterCharacter* Character;
};

USTRUCT(BlueprintType)
struct FServerSideRewindResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bHitConfirmed;

	UPROPERTY()
	bool bHeadShot;
};

USTRUCT(BlueprintType)
struct FShotgunServerSideRewindResult
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<ABlasterCharacter*, uint32> HeadShots;
	UPROPERTY()
	TMap<ABlasterCharacter*, uint32> BodyShots;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API ULagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	ULagCompensationComponent();
	friend class ABlasterCharacter;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void ShowFramePackage(const FFramePackage& Package, const FColor& Color);

	void ShowFramePackageCapsule(const FFramePackageCapsule& Package, const FColor& Color);

	FServerSideRewindResult ServerSideRewind(class ABlasterCharacter* HitCharacter, 
		const FVector_NetQuantize& TraceStart, 
		const FVector_NetQuantize& HitLocation, 
		float HitTime);



	FShotgunServerSideRewindResult ShotgunServerSideRewind(
		const TArray<ABlasterCharacter*>& HitCharacters,
		const FVector_NetQuantize& TraceStart,
		const TArray<FVector_NetQuantize>& HitLocations,
		float HitTime);

	UFUNCTION(Server, Reliable)
	void ServerScoreRequest(
		ABlasterCharacter* HitCharacter,
		const FVector_NetQuantize& TraceStart,
		const FVector_NetQuantize& HitLocation,
		float HitTime,
		class AWeapon* DamageCauser
	);

	UFUNCTION(Server, Reliable)
	void ShotgunServerScoreRequest(
		const TArray<ABlasterCharacter*>& HitCharacters,
		const FVector_NetQuantize& TraceStart,
		const TArray<FVector_NetQuantize>& HitLocations,
		float HitTime,
		AWeapon* DamageCauser
	);

protected:
	virtual void BeginPlay() override;	
	void SaveFramePackage(FFramePackage& Package);
	void SaveFramePackageCapsule(FFramePackageCapsule& Package);
	FFramePackage InterpBetweenFrames(const FFramePackage& OlderFrame, const FFramePackage& YoungerFrame, float HitTime);
	FFramePackageCapsule InterpBetweenFramesCapsule(const FFramePackageCapsule& OlderFrame, const FFramePackageCapsule& YoungerFrame, float HitTime);

	FServerSideRewindResult ConfirmHit(
		const FFramePackage& Package, 
		ABlasterCharacter* HitCharacter, 
		const FVector_NetQuantize& TraceStart, 
		const FVector_NetQuantize& HitLocation);

	FServerSideRewindResult ConfirmHitCapsule(
		const FFramePackageCapsule& Package,
		ABlasterCharacter* HitCharacter,
		const FVector_NetQuantize& TraceStart,
		const FVector_NetQuantize& HitLocation);

	void CacheBoxPositions(ABlasterCharacter* HitCharacter, FFramePackage& OutFramePackage);
	void CacheCapsulePositions(ABlasterCharacter* HitCharacter, FFramePackageCapsule& OutFramePackage);
	void MoveBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& Package);
	void MoveCapsules(ABlasterCharacter* HitCharacter, const FFramePackageCapsule& Package);
	void ResetHitBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& Package);
	void ResetHitCapsules(ABlasterCharacter* HitCharacter, const FFramePackageCapsule& Package);
	void EnableCharacterMeshCollision(ABlasterCharacter* HitCharacter, ECollisionEnabled::Type CollisionEnabled);
	void SaveFramePackage();
	void SaveFramePackageCapsule();
	FFramePackage GetFrameToCheck(ABlasterCharacter* HitCharacter, float HitTime);
	FFramePackageCapsule GetFrameToCheckCapsule(ABlasterCharacter* HitCharacter, float HitTime);

	/**
	* Shotgun
	*/

	FShotgunServerSideRewindResult ShotgunConfirmHit(
		const TArray<FFramePackage>& FramePackages,
		const FVector_NetQuantize& TraceStart,
		const TArray<FVector_NetQuantize>& HitLocations
		);

	void DrawDebugCapsules();

private:

	UPROPERTY()
	ABlasterCharacter* Character;

	UPROPERTY()
	class ABlasterPlayerController* Controller;

	TDoubleLinkedList<FFramePackage> FrameHistory;
	TDoubleLinkedList<FFramePackageCapsule> FrameHistoryCapsule;

	UPROPERTY(EditAnywhere)
	float MaxRecordTime = 0.5f;

public:

};
