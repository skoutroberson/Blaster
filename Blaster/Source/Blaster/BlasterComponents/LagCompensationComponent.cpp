// Fill out your copyright notice in the Description page of Project Settings.


#include "LagCompensationComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Blaster/Weapon/Weapon.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsAsset.h"

ULagCompensationComponent::ULagCompensationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

}

void ULagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();
}

void ULagCompensationComponent::ShowFramePackage(const FFramePackage& Package, const FColor& Color)
{
	for (auto& BoxInfo : Package.HitBoxInfo)
	{
		DrawDebugBox(
			GetWorld(),
			BoxInfo.Value.Location,
			BoxInfo.Value.BoxExtent,
			FQuat(BoxInfo.Value.Rotation),
			Color,
			false,
			MaxRecordTime
		);
	}
}

void ULagCompensationComponent::ShowFramePackageCapsule(const FFramePackageCapsule& Package, const FColor& Color)
{
	if (Character == nullptr || Character->GetMesh() == nullptr) return;
	for (auto& CapsuleInfos : Package.HitCapsulesInfo)
	{
		for (auto& CapsuleInfo : CapsuleInfos.Value.CapsuleInfos)
		{
			DrawDebugCapsule(
				GetWorld(),
				CapsuleInfo.Location,
				CapsuleInfo.Length * 0.5f,
				CapsuleInfo.Radius,
				FQuat(CapsuleInfo.Rotation),
				Color,
				false,
				MaxRecordTime
			);
		}
	}
}

FServerSideRewindResult ULagCompensationComponent::ServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime)
{
	FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	return ConfirmHit(FrameToCheck, HitCharacter, TraceStart, HitLocation);
	//FFramePackageCapsule FrameToCheck = GetFrameToCheckCapsule(HitCharacter, HitTime);
	//return ConfirmHitCapsule(FrameToCheck, HitCharacter, TraceStart, HitLocation);
}

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunServerSideRewind(const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, float HitTime)
{
	TArray<FFramePackage> FramesToCheck;
	for (ABlasterCharacter* HitCharacter : HitCharacters)
	{
		FramesToCheck.Add(GetFrameToCheck(HitCharacter, HitTime));
	}

	return ShotgunConfirmHit(FramesToCheck, TraceStart, HitLocations);
}

FFramePackage ULagCompensationComponent::GetFrameToCheck(ABlasterCharacter* HitCharacter, float HitTime)
{
	bool bReturn =
		HitCharacter == nullptr ||
		HitCharacter->GetLagCompensation() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistory.GetHead() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistory.GetTail() == nullptr;
	if (bReturn) return FFramePackage();

	// Frame package that we check to verify a hit
	FFramePackage FrameToCheck;
	bool bShouldInterpolate = true;

	// Frame history of the HitCharacter
	const TDoubleLinkedList<FFramePackage>& History = HitCharacter->GetLagCompensation()->FrameHistory;
	const float OldestHistoryTime = History.GetTail()->GetValue().Time;
	const float NewestHistoryTime = History.GetHead()->GetValue().Time;
	if (OldestHistoryTime > HitTime)
	{
		// too far back - too laggy to do SSR
		return FFramePackage();
	}
	if (OldestHistoryTime == HitTime)
	{
		FrameToCheck = History.GetTail()->GetValue();
		bShouldInterpolate = false;
	}
	if (NewestHistoryTime <= HitTime)
	{
		FrameToCheck = History.GetHead()->GetValue();
		bShouldInterpolate = false;
	}

	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Younger = History.GetHead();
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Older = Younger;

	while (Older->GetValue().Time > HitTime) // is Older still younger than HitTime
	{
		// March back until: OlderTime < HitTime < YoungerTime
		if (Older->GetNextNode() == nullptr) break;
		Older = Older->GetNextNode();
		if (Older->GetValue().Time > HitTime)
		{
			Younger = Older;
		}
	}
	if (Older->GetValue().Time == HitTime) // highly unlikely, but we found our frame to check
	{
		FrameToCheck = Older->GetValue();
		bShouldInterpolate = false;
	}
	if (bShouldInterpolate)
	{
		// Interpolate between Younger and Older
		FrameToCheck = InterpBetweenFrames(Older->GetValue(), Younger->GetValue(), HitTime);
	}
	FrameToCheck.Character = HitCharacter;
	return FrameToCheck;
}

FFramePackageCapsule ULagCompensationComponent::GetFrameToCheckCapsule(ABlasterCharacter* HitCharacter, float HitTime)
{
	bool bReturn =
		HitCharacter == nullptr ||
		HitCharacter->GetLagCompensation() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistoryCapsule.GetHead() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistoryCapsule.GetTail() == nullptr;
	if (bReturn) return FFramePackageCapsule();

	// Frame package that we check to verify a hit
	FFramePackageCapsule FrameToCheck;
	bool bShouldInterpolate = true;

	// Frame history of the HitCharacter
	const TDoubleLinkedList<FFramePackageCapsule>& History = HitCharacter->GetLagCompensation()->FrameHistoryCapsule;
	const float OldestHistoryTime = History.GetTail()->GetValue().Time;
	const float NewestHistoryTime = History.GetHead()->GetValue().Time;
	if (OldestHistoryTime > HitTime)
	{
		// too far back - too laggy to do SSR
		return FFramePackageCapsule();
	}
	if (OldestHistoryTime == HitTime)
	{
		FrameToCheck = History.GetTail()->GetValue();
		bShouldInterpolate = false;
	}
	if (NewestHistoryTime <= HitTime)
	{
		FrameToCheck = History.GetHead()->GetValue();
		bShouldInterpolate = false;
	}

	TDoubleLinkedList<FFramePackageCapsule>::TDoubleLinkedListNode* Younger = History.GetHead();
	TDoubleLinkedList<FFramePackageCapsule>::TDoubleLinkedListNode* Older = Younger;

	while (Older->GetValue().Time > HitTime) // is Older still younger than HitTime
	{
		// March back until: OlderTime < HitTime < YoungerTime
		if (Older->GetNextNode() == nullptr) break;
		Older = Older->GetNextNode();
		if (Older->GetValue().Time > HitTime)
		{
			Younger = Older;
		}
	}
	if (Older->GetValue().Time == HitTime) // highly unlikely, but we found our frame to check
	{
		FrameToCheck = Older->GetValue();
		bShouldInterpolate = false;
	}
	if (bShouldInterpolate)
	{
		// Interpolate between Younger and Older
		FrameToCheck = InterpBetweenFramesCapsule(Older->GetValue(), Younger->GetValue(), HitTime);
	}
	FrameToCheck.Character = HitCharacter;
	return FrameToCheck;
}

void ULagCompensationComponent::ServerScoreRequest_Implementation(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime, AWeapon* DamageCauser)
{
	FServerSideRewindResult Confirm = ServerSideRewind(HitCharacter, TraceStart, HitLocation, HitTime);

	if (Character && HitCharacter && DamageCauser && Confirm.bHitConfirmed)
	{
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			DamageCauser->GetDamage(),
			Character->Controller,
			DamageCauser,
			UDamageType::StaticClass()
		);
	}
}

void ULagCompensationComponent::ShotgunServerScoreRequest_Implementation(const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, float HitTime, AWeapon* DamageCauser)
{
	FShotgunServerSideRewindResult Confirm = ShotgunServerSideRewind(HitCharacters, TraceStart, HitLocations, HitTime);

	if (DamageCauser == nullptr || Character == nullptr) return;

	for (auto& HitCharacter : HitCharacters)
	{
		if (HitCharacter == nullptr) continue;
		float TotalDamage = 0.f;
		if (Confirm.HeadShots.Contains(HitCharacter))
		{
			float HeadShotDamage = Confirm.HeadShots[HitCharacter] * DamageCauser->GetDamage();
			TotalDamage += HeadShotDamage;
		}
		if (Confirm.BodyShots.Contains(HitCharacter))
		{
			float BodyShotDamage = Confirm.BodyShots[HitCharacter] * DamageCauser->GetDamage();
			TotalDamage += BodyShotDamage;
		}
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			TotalDamage,
			Character->Controller,
			DamageCauser,
			UDamageType::StaticClass()
		);
	}
}

FFramePackage ULagCompensationComponent::InterpBetweenFrames(const FFramePackage& OlderFrame, const FFramePackage& YoungerFrame, float HitTime)
{
	const float Distance = YoungerFrame.Time - OlderFrame.Time;
	const float InterpFraction = FMath::Clamp((HitTime - OlderFrame.Time) / Distance, 0.f, 1.f);

	FFramePackage InterpFramePackage;
	InterpFramePackage.Time = HitTime;

	for (auto& YoungerPair : YoungerFrame.HitBoxInfo)
	{
		const FName& BoxInfoName = YoungerPair.Key;

		const FBoxInformation& OlderBox = OlderFrame.HitBoxInfo[BoxInfoName];
		const FBoxInformation& YoungerBox = YoungerFrame.HitBoxInfo[BoxInfoName];

		FBoxInformation InterpBoxInfo;

		InterpBoxInfo.Location = FMath::VInterpTo(OlderBox.Location, YoungerBox.Location, 1.f, InterpFraction);
		InterpBoxInfo.Rotation = FMath::RInterpTo(OlderBox.Rotation, YoungerBox.Rotation, 1.f, InterpFraction);
		InterpBoxInfo.BoxExtent = YoungerBox.BoxExtent;

		InterpFramePackage.HitBoxInfo.Add(BoxInfoName, InterpBoxInfo);
	}

	return InterpFramePackage;
}

FFramePackageCapsule ULagCompensationComponent::InterpBetweenFramesCapsule(const FFramePackageCapsule& OlderFrame, const FFramePackageCapsule& YoungerFrame, float HitTime)
{
	const float Distance = YoungerFrame.Time - OlderFrame.Time;
	const float InterpFraction = FMath::Clamp((HitTime - OlderFrame.Time) / Distance, 0.f, 1.f);

	FFramePackageCapsule InterpFramePackage;
	InterpFramePackage.Time = HitTime;

	for (auto& YoungerPair : YoungerFrame.HitCapsulesInfo)
	{
		const FName& CapsuleInfoName = YoungerPair.Key;
		const TArray<FCapsuleInformation>& OlderCapsules = OlderFrame.HitCapsulesInfo[CapsuleInfoName].CapsuleInfos;
		const TArray<FCapsuleInformation>& YoungerCapsules = YoungerFrame.HitCapsulesInfo[CapsuleInfoName].CapsuleInfos;

		TArray<FCapsuleInformation> InterpCapsuleInfosArray;
		
		for (int i = 0; i < OlderCapsules.Num(); ++i)
		{
			FCapsuleInformation InterpCapsuleInfo;

			InterpCapsuleInfo.Location = FMath::VInterpTo(OlderCapsules[i].Location, YoungerCapsules[i].Location, 1.f, InterpFraction);
			InterpCapsuleInfo.Rotation = FMath::RInterpTo(OlderCapsules[i].Rotation, YoungerCapsules[i].Rotation, 1.f, InterpFraction);
			InterpCapsuleInfo.Radius = YoungerCapsules[i].Radius;
			InterpCapsuleInfo.Length = YoungerCapsules[i].Length;
			InterpCapsuleInfosArray.Add(InterpCapsuleInfo);
		}
		FCapsuleInformations InterpCapsuleInfos;
		InterpCapsuleInfos.CapsuleInfos.Append(InterpCapsuleInfosArray);
		InterpFramePackage.HitCapsulesInfo.Add(CapsuleInfoName, InterpCapsuleInfos);
	}

	return InterpFramePackage;
}

FServerSideRewindResult ULagCompensationComponent::ConfirmHit(const FFramePackage& Package, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation)
{
	if (HitCharacter == nullptr) return FServerSideRewindResult();
	
	FFramePackage CurrentFrame;
	CacheBoxPositions(HitCharacter, CurrentFrame);
	MoveBoxes(HitCharacter, Package);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);

	// Enable collision for the head first
	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);

	FHitResult ConfirmHitResult;
	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
	UWorld* World = GetWorld();
	if (World)
	{
		World->LineTraceSingleByChannel(
			ConfirmHitResult,
			TraceStart,
			TraceStart + TraceEnd,
			ECollisionChannel::ECC_Visibility
		);
		if (ConfirmHitResult.bBlockingHit) // we hit the head, return early
		{
			ResetHitBoxes(HitCharacter, CurrentFrame);
			EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
			return FServerSideRewindResult{ true, true };
		}
		else // didn't hit the head, check the rest of the boxes
		{
			for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
			{
				if (HitBoxPair.Value != nullptr)
				{
					HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					HitBoxPair.Value->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
				}
			}
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceStart + TraceEnd,
				ECollisionChannel::ECC_Visibility
			);
			if (ConfirmHitResult.bBlockingHit)
			{
				ResetHitBoxes(HitCharacter, CurrentFrame);
				EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
				return FServerSideRewindResult{ true, false };
			}
		}
	}
	ResetHitBoxes(HitCharacter, CurrentFrame);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
	return FServerSideRewindResult{ false, false };
}

FServerSideRewindResult ULagCompensationComponent::ConfirmHitCapsule(const FFramePackageCapsule& Package, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation)
{
	if (HitCharacter == nullptr) return FServerSideRewindResult();

	FFramePackageCapsule CurrentFrame;
	CacheCapsulePositions(HitCharacter, CurrentFrame);
	MoveCapsules(HitCharacter, Package);
	
	FHitResult ConfirmHitResult;
	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
	UWorld* World = GetWorld();
	if (World)
	{
		World->LineTraceSingleByChannel(
			ConfirmHitResult,
			TraceStart,
			TraceStart + TraceEnd,
			ECollisionChannel::ECC_Visibility
		);
		if (ConfirmHitResult.bBlockingHit)
		{
			ResetHitCapsules(HitCharacter, CurrentFrame);
			return FServerSideRewindResult{ true, false };
		}
	}

	ResetHitCapsules(HitCharacter, CurrentFrame);
	return FServerSideRewindResult{ false, false };
}

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunConfirmHit(const TArray<FFramePackage>& FramePackages, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations)
{
	for (auto& Frame : FramePackages)
	{
		if (Frame.Character == nullptr) return FShotgunServerSideRewindResult();
	}
	FShotgunServerSideRewindResult ShotgunResult;
	TArray<FFramePackage> CurrentFrames;
	for (auto& Frame : FramePackages)
	{
		FFramePackage CurrentFrame;
		CurrentFrame.Character = Frame.Character;
		CacheBoxPositions(Frame.Character, CurrentFrame);
		MoveBoxes(Frame.Character, Frame);
		EnableCharacterMeshCollision(Frame.Character, ECollisionEnabled::NoCollision);
		CurrentFrames.Add(CurrentFrame);
	}

	for (auto& Frame : FramePackages)
	{
		// Enable collision for the head first
		UBoxComponent* HeadBox = Frame.Character->HitCollisionBoxes[FName("head")];
		HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		HeadBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	}

	UWorld* World = GetWorld();
	// check for headshots
	for (auto& HitLocation : HitLocations)
	{
		FHitResult ConfirmHitResult;
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		if (World)
		{
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceStart + TraceEnd,
				ECollisionChannel::ECC_Visibility
			);
			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(ConfirmHitResult.GetActor());
			if (BlasterCharacter)
			{
				if (ShotgunResult.HeadShots.Contains(BlasterCharacter))
				{
					ShotgunResult.HeadShots[BlasterCharacter]++;
				}
				else
				{
					ShotgunResult.HeadShots.Emplace(BlasterCharacter, 1);
				}
			}
		}
	}

	// enable collision for all boxes, then disable for head box
	for (auto& Frame : FramePackages)
	{
		for (auto& HitBoxPair : Frame.Character->HitCollisionBoxes)
		{
			if (HitBoxPair.Value != nullptr)
			{
				HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				HitBoxPair.Value->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
			}
		}
		UBoxComponent* HeadBox = Frame.Character->HitCollisionBoxes[FName("head")];
		HeadBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// check for body shots
	for (auto& HitLocation : HitLocations)
	{
		FHitResult ConfirmHitResult;
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		if (World && Character && Character->GetEquippedWeapon())
		{
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceStart + TraceEnd,
				ECollisionChannel::ECC_Visibility
			);
			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(ConfirmHitResult.GetActor());
			if (BlasterCharacter)
			{
				if (ShotgunResult.BodyShots.Contains(BlasterCharacter))
				{
					ShotgunResult.BodyShots[BlasterCharacter]++;
				}
				else
				{
					ShotgunResult.BodyShots.Emplace(BlasterCharacter, 1);
				}
			}
		}
	}

	for (auto& Frame : CurrentFrames)
	{
		ResetHitBoxes(Frame.Character, Frame);
		EnableCharacterMeshCollision(Frame.Character, ECollisionEnabled::QueryAndPhysics);
	}
	
	return ShotgunResult;
}

void ULagCompensationComponent::DrawDebugCapsules()
{
	if (!Character) return;

	for (auto x : Character->GetMesh()->GetPhysicsAsset()->SkeletalBodySetups)
	{
		auto BName = x->BoneName;
		auto BoneWorldTransform = Character->GetMesh()->GetBoneTransform(Character->GetMesh()->GetBoneIndex(BName));
		for (auto y : x->AggGeom.SphylElems)
		{
			auto LocTransform = y.GetTransform();
			auto WorldTransform = LocTransform * BoneWorldTransform;
			DrawDebugCapsule(
				GetWorld(),
				WorldTransform.GetLocation(),
				y.Length / 2 + y.Radius,
				y.Radius, WorldTransform.GetRotation(),
				FColor::Red
			);
		}
	}
}

void ULagCompensationComponent::CacheBoxPositions(ABlasterCharacter* HitCharacter, FFramePackage& OutFramePackage)
{
	if (HitCharacter == nullptr) return;
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		if (HitBoxPair.Value != nullptr)
		{
			FBoxInformation BoxInfo;
			BoxInfo.Location = HitBoxPair.Value->GetComponentLocation();
			BoxInfo.Rotation = HitBoxPair.Value->GetComponentRotation();
			BoxInfo.BoxExtent = HitBoxPair.Value->GetScaledBoxExtent();
			OutFramePackage.HitBoxInfo.Add(HitBoxPair.Key, BoxInfo);
		}
	}
}

void ULagCompensationComponent::CacheCapsulePositions(ABlasterCharacter* HitCharacter, FFramePackageCapsule& OutFramePackage)
{
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : Character;
	if (Character && Character->GetMesh() && Character->GetMesh()->GetPhysicsAsset())
	{
		const TArray<TObjectPtr<USkeletalBodySetup>>& HitCharacterSkeletalBodies = Character->GetMesh()->GetPhysicsAsset()->SkeletalBodySetups;

		for (auto& SkeletalBody : HitCharacterSkeletalBodies)
		{
			if (SkeletalBody == nullptr) continue;

			const TArray<FKSphylElem>& SphylElems = SkeletalBody->AggGeom.SphylElems;
			const FName& BoneName = SkeletalBody->BoneName;

			FCapsuleInformations CapsuleInformations;

			for (int i = 0; i < SphylElems.Num(); ++i)
			{
				const FKSphylElem& SphylElem = SphylElems[i];
				FCapsuleInformation CapsuleInformation;
				const FTransform SphylTransform = SphylElem.GetTransform();

				CapsuleInformation.Location = SphylTransform.GetLocation();
				CapsuleInformation.Rotation = SphylTransform.GetRotation().Rotator();
				CapsuleInformation.Radius = SphylElem.Radius;
				CapsuleInformation.Length = SphylElem.Length / 2 + SphylElem.Radius;
				CapsuleInformations.CapsuleInfos.Add(CapsuleInformation);
			}

			OutFramePackage.HitCapsulesInfo.Add(BoneName, CapsuleInformations);
		}
	}
}

void ULagCompensationComponent::MoveBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& Package)
{
	if (HitCharacter == nullptr) return;
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		if (HitBoxPair.Value != nullptr)
		{
			HitBoxPair.Value->SetWorldLocation(Package.HitBoxInfo[HitBoxPair.Key].Location);
			HitBoxPair.Value->SetWorldRotation(Package.HitBoxInfo[HitBoxPair.Key].Rotation);
			HitBoxPair.Value->SetBoxExtent(Package.HitBoxInfo[HitBoxPair.Key].BoxExtent);
		}
	}
}

void ULagCompensationComponent::MoveCapsules(ABlasterCharacter* HitCharacter, const FFramePackageCapsule& Package)
{
	if (HitCharacter == nullptr || HitCharacter->GetMesh() == nullptr || HitCharacter->GetMesh()->GetPhysicsAsset()) return;

	TArray<TObjectPtr<USkeletalBodySetup>>& HitCharacterSkeletalBodies = HitCharacter->GetMesh()->GetPhysicsAsset()->SkeletalBodySetups;

	for (auto& SkeletalBody : HitCharacterSkeletalBodies)
	{
		TArray<FKSphylElem>& SphylElems = SkeletalBody->AggGeom.SphylElems;

		const FName& BoneName = SkeletalBody->BoneName;

		for (int i = 0; i < SphylElems.Num(); ++i)
		{
			FKSphylElem& SphylElem = SphylElems[i];
			FTransform NewTransform;
			NewTransform.SetLocation(Package.HitCapsulesInfo[BoneName].CapsuleInfos[i].Location);
			NewTransform.SetRotation(FQuat(Package.HitCapsulesInfo[BoneName].CapsuleInfos[i].Rotation));
			SphylElem.SetTransform(NewTransform);
		}
	}

	/*
	* for (auto x : Character->GetMesh()->GetPhysicsAsset()->SkeletalBodySetups)
	{
		auto BName = x->BoneName;
		auto BoneWorldTransform = Character->GetMesh()->GetBoneTransform(Character->GetMesh()->GetBoneIndex(BName));
		for (auto y : x->AggGeom.SphylElems)
		{
			auto LocTransform = y.GetTransform();
			auto WorldTransform = LocTransform * BoneWorldTransform;
			DrawDebugCapsule(
				GetWorld(),
				WorldTransform.GetLocation(),
				y.Length / 2 + y.Radius,
				y.Radius, WorldTransform.GetRotation(),
				FColor::Red
			);
		}
	}
	TArray<TObjectPtr<USkeletalBodySetup>>& HitCharacterSkeletalBodies = HitCharacter->GetMesh()->GetPhysicsAsset()->SkeletalBodySetups;
	//HitCharacterSkeletalBodies

	for (int i = 0; i < HitCharacterSkeletalBodies.Num(); ++i)
	{
		TObjectPtr<USkeletalBodySetup> SkeletalBodySetup = HitCharacterSkeletalBodies[i];
		TArray<FKSphylElem>& SphylElems = SkeletalBodySetup->AggGeom.SphylElems;
		for (int k = 0; k < SphylElems.Num(); ++k)
		{
			FKSphylElem& SphylElem = SphylElems[k];

			FTransform NewTransform;
			NewTransform.SetLocation(Package.HitCapsulesInfo[k].Location);
			NewTransform.SetRotation(FQuat(Package.HitCapsulesInfo[k].Rotation));
			SphylElem.SetTransform(NewTransform);
			SphylElem.Radius = Package.HitCapsulesInfo[k].Radius;
			SphylElem.Length = Package.HitCapsulesInfo[k].Length;
		}
	}

	
	for (auto x : HitCharacter->GetMesh()->GetPhysicsAsset()->SkeletalBodySetups)
	{
		auto BName = x->BoneName;
		auto BoneWorldTransform = HitCharacter->GetMesh()->GetBoneTransform(HitCharacter->GetMesh()->GetBoneIndex(BName));
		for (auto y : x->AggGeom.SphylElems)
		{
			auto LocTransform = y.GetTransform();
			auto WorldTransform = LocTransform * BoneWorldTransform;

			FCapsuleInformation CapsuleInformation;
			CapsuleInformation.Location = WorldTransform.GetLocation();
			CapsuleInformation.Rotation = WorldTransform.GetRotation().Rotator();
			CapsuleInformation.Radius = y.Radius;
			CapsuleInformation.Length = y.GetScaledHalfLength(WorldTransform.GetScale3D()) * 2.f;
		}
	}
	*/
}

void ULagCompensationComponent::ResetHitBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& Package)
{
	if (HitCharacter == nullptr) return;
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		if (HitBoxPair.Value != nullptr)
		{
			HitBoxPair.Value->SetWorldLocation(Package.HitBoxInfo[HitBoxPair.Key].Location);
			HitBoxPair.Value->SetWorldRotation(Package.HitBoxInfo[HitBoxPair.Key].Rotation);
			HitBoxPair.Value->SetBoxExtent(Package.HitBoxInfo[HitBoxPair.Key].BoxExtent);
			HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void ULagCompensationComponent::ResetHitCapsules(ABlasterCharacter* HitCharacter, const FFramePackageCapsule& Package)
{
	if (HitCharacter == nullptr || HitCharacter->GetMesh() || HitCharacter->GetMesh()->GetPhysicsAsset()) return;

	TArray<TObjectPtr<USkeletalBodySetup>>& HitCharacterSkeletalBodies = HitCharacter->GetMesh()->GetPhysicsAsset()->SkeletalBodySetups;

	for (auto& SkeletalBody : HitCharacterSkeletalBodies)
	{
		if (SkeletalBody == nullptr) continue;

		TArray<FKSphylElem>& SphylElems = SkeletalBody->AggGeom.SphylElems;
		const FName& BoneName = SkeletalBody->BoneName;

		for (int i = 0; i < SphylElems.Num(); ++i)
		{
			FKSphylElem& SphylElem = SphylElems[i];
			FTransform NewTransform;
			NewTransform.SetLocation(Package.HitCapsulesInfo[BoneName].CapsuleInfos[i].Location);
			NewTransform.SetRotation(FQuat(Package.HitCapsulesInfo[BoneName].CapsuleInfos[i].Rotation));
			SphylElem.SetTransform(NewTransform);
		}
	}
}

void ULagCompensationComponent::EnableCharacterMeshCollision(ABlasterCharacter* HitCharacter, ECollisionEnabled::Type CollisionEnabled)
{
	if (HitCharacter && HitCharacter->GetMesh())
	{
		HitCharacter->GetMesh()->SetCollisionEnabled(CollisionEnabled);
	}
}

void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SaveFramePackage();
	//DrawDebugCapsules();
	//SaveFramePackageCapsule();
}

void ULagCompensationComponent::SaveFramePackage()
{
	if (Character == nullptr || !Character->HasAuthority()) return;
	if (FrameHistory.Num() <= 1)
	{
		FFramePackage ThisFrame;
		SaveFramePackage(ThisFrame);
		FrameHistory.AddHead(ThisFrame);
	}
	else
	{
		float HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		while (HistoryLength > MaxRecordTime)
		{
			FrameHistory.RemoveNode(FrameHistory.GetTail());
			HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		}
		FFramePackage ThisFrame;
		SaveFramePackage(ThisFrame);
		FrameHistory.AddHead(ThisFrame);


		if (Character && Character->IsLocallyControlled())
		{
			//ShowFramePackage(ThisFrame, FColor::Red);
		}
	}
}

void ULagCompensationComponent::SaveFramePackageCapsule()
{
	if (Character == nullptr || !Character->HasAuthority()) return;
	if (FrameHistoryCapsule.Num() <= 1)
	{
		FFramePackageCapsule ThisFrame;
		SaveFramePackageCapsule(ThisFrame);
		FrameHistoryCapsule.AddHead(ThisFrame);
	}
	else
	{
		float HistoryLength = FrameHistoryCapsule.GetHead()->GetValue().Time - FrameHistoryCapsule.GetTail()->GetValue().Time;
		while (HistoryLength > MaxRecordTime)
		{
			FrameHistoryCapsule.RemoveNode(FrameHistoryCapsule.GetTail());
			HistoryLength = FrameHistoryCapsule.GetHead()->GetValue().Time - FrameHistoryCapsule.GetTail()->GetValue().Time;
		}
		FFramePackageCapsule ThisFrame;
		SaveFramePackageCapsule(ThisFrame);
		FrameHistoryCapsule.AddHead(ThisFrame);


		//if (Character && Character->IsLocallyControlled())
		//{
			//ShowFramePackageCapsule(ThisFrame, FColor::Red);
		//}
	}
}

void ULagCompensationComponent::SaveFramePackage(FFramePackage& Package)
{
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : Character;
	if (Character)
	{
		Package.Time = GetWorld()->GetTimeSeconds();
		Package.Character = Character;
		for (auto& BoxPair : Character->HitCollisionBoxes)
		{
			FBoxInformation BoxInformation;
			BoxInformation.Location = BoxPair.Value->GetComponentLocation();
			BoxInformation.Rotation = BoxPair.Value->GetComponentRotation();
			BoxInformation.BoxExtent = BoxPair.Value->GetScaledBoxExtent();
			Package.HitBoxInfo.Add(BoxPair.Key, BoxInformation);
		}
	}
}

void ULagCompensationComponent::SaveFramePackageCapsule(FFramePackageCapsule& Package)
{
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : Character;
	if (Character && Character->GetMesh() && Character->GetMesh()->GetPhysicsAsset())
	{
		Package.Time = GetWorld()->GetTimeSeconds();
		Package.Character = Character;

		const TArray<TObjectPtr<USkeletalBodySetup>>& HitCharacterSkeletalBodies = Character->GetMesh()->GetPhysicsAsset()->SkeletalBodySetups;

		for (auto& SkeletalBody : HitCharacterSkeletalBodies)
		{
			if (SkeletalBody == nullptr) continue;

			const TArray<FKSphylElem>& SphylElems = SkeletalBody->AggGeom.SphylElems;
			const FName& BoneName = SkeletalBody->BoneName;
			const FTransform BoneWorldTransform = Character->GetMesh()->GetBoneTransform(Character->GetMesh()->GetBoneIndex(BoneName));

			FCapsuleInformations CapsuleInformations;

			for (int i = 0; i < SphylElems.Num(); ++i)
			{
				const FKSphylElem& SphylElem = SphylElems[i];
				FCapsuleInformation CapsuleInformation;
				const FTransform SphylTransform = SphylElem.GetTransform();
				const FTransform WorldTransform = SphylTransform * BoneWorldTransform;

				CapsuleInformation.Location = WorldTransform.GetLocation();
				CapsuleInformation.Rotation = WorldTransform.GetRotation().Rotator();
				CapsuleInformation.Radius = SphylElem.Radius;
				CapsuleInformation.Length = SphylElem.Length + SphylElem.Radius * 2.f;
				CapsuleInformations.CapsuleInfos.Add(CapsuleInformation);
			}

			Package.HitCapsulesInfo.Add(BoneName, CapsuleInformations);
		}
	}
}

