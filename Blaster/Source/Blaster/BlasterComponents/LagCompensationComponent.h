// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blaster/BlasterTypes/Hitbox.h"
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
	FVector A;

	UPROPERTY()
	FVector B;

	UPROPERTY()
	float Radius;

	UPROPERTY()
	float Length;

	UPROPERTY()
	EHitbox HitboxType = EHitbox::EH_None;
};

USTRUCT(BlueprintType)
struct FSphereInfo
{
	GENERATED_BODY()

	FVector Center;
	float Radius;
};

// info for the weapon fire trace hit
USTRUCT(BlueprintType)
struct FHitInfo
{
	GENERATED_BODY()

	// used for spawning blood impact particles
	UPROPERTY()
	FVector Location;

	UPROPERTY()
	FVector Normal;

	// used to determine how much damage to apply to player
	UPROPERTY()
	EHitbox HitType = EHitbox::EH_None;
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

USTRUCT(BlueprintType)
struct FFramePackageCapsule
{
	GENERATED_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY()
	TArray<FCapsuleInformation> HitCapsulesInfo;

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
struct FServerSideRewindResultCapsule
{
	GENERATED_BODY()

	UPROPERTY()
	EHitbox HitType = EHitbox::EH_None;

	UPROPERTY()
	FVector HitLocation;

	UPROPERTY()
	FVector HitNormal;
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

	FServerSideRewindResultCapsule ServerSideRewindCapsule(
		ABlasterCharacter* HitCharacter,
		const FVector_NetQuantize& TraceStart,
		const FVector_NetQuantize& HitLocation,
		float HitTime
	);

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
	void ServerScoreRequestCapsule(
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

	FServerSideRewindResultCapsule ConfirmHitCapsule(
		const FFramePackageCapsule& Package,
		ABlasterCharacter* HitCharacter,
		const FVector_NetQuantize& TraceStart,
		const FVector_NetQuantize& HitLocation
	);

	void CacheBoxPositions(ABlasterCharacter* HitCharacter, FFramePackage& OutFramePackage);
	void MoveBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& Package);
	void ResetHitBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& Package);
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

private:

	UPROPERTY()
	ABlasterCharacter* Character;

	UPROPERTY()
	class ABlasterPlayerController* Controller;

	TDoubleLinkedList<FFramePackage> FrameHistory;
	TDoubleLinkedList<FFramePackageCapsule> FrameHistoryCapsule;

	UPROPERTY(EditAnywhere)
	float MaxRecordTime = 2.f;

	UPROPERTY()
	AWeapon* DamageCauserWeapon;

	void DrawCapsuleHitBox();

	void Test(const FVector& TraceStart, const FVector& TraceEnd, const ABlasterCharacter* HitCharacter);

	void CapsuleToSpheres(const FVector& A, const FVector& B, const float Radius, const float Length, TArray<FSphereInfo>& OutSpheres);

	int SphereCount = 0;

	// copied from Vector.h to avoid having to cast to FVector3f every time we call this
	inline bool LineSphereIntersection(const FVector& Start, const FVector& Dir, float Length, const FVector& Origin, float Radius);

	// Returns first intersection point from a line intersecting with a sphere. Assumes that the line does intersect with the sphere.
	inline FVector const FirstIntersectionPoint(const FVector& LineStart, const FVector& LineEnd, const FVector& Center, const float Radius);

	FHitInfo TraceAgainstCapsules(const FFramePackageCapsule& Package, const ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector& TraceEnd);

	// Maximum number of hits to check in TraceAgainstCapsules
	UPROPERTY(EditDefaultsOnly)
	uint8 MaxSpheresHit = 4;

public:

};
