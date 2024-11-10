// Fill out your copyright notice in the Description page of Project Settings.


#include "LagCompensationComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Blaster/Weapon/Weapon.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine\PhysicsAsset.h"
#include "Math/Vector.h"

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
			true
		);
	}
}

void ULagCompensationComponent::ShowFramePackageCapsule(const FFramePackageCapsule& Package, const FColor& Color)
{
	for (auto& CapsuleInfo : Package.HitCapsulesInfo)
	{
		DrawDebugSphere(
			GetWorld(),
			CapsuleInfo.A,
			CapsuleInfo.Radius,
			12,
			Color,
			false,
			MaxRecordTime
		);
		DrawDebugSphere(
			GetWorld(),
			CapsuleInfo.B,
			CapsuleInfo.Radius,
			12,
			Color,
			false,
			MaxRecordTime
		);
	}
}

FServerSideRewindResult ULagCompensationComponent::ServerSideRewind(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime)
{
	FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	return ConfirmHit(FrameToCheck, HitCharacter, TraceStart, HitLocation);
}

FServerSideRewindResultCapsule ULagCompensationComponent::ServerSideRewindCapsule(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime)
{
	FFramePackageCapsule FrameToCheck = GetFrameToCheckCapsule(HitCharacter, HitTime);
	return ConfirmHitCapsule(FrameToCheck, HitCharacter, TraceStart, HitLocation);
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
	DamageCauserWeapon = DamageCauser;
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

void ULagCompensationComponent::ServerScoreRequestCapsule_Implementation(ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime, AWeapon* DamageCauser)
{
	DamageCauserWeapon = DamageCauser;
	FServerSideRewindResultCapsule Confirm = ServerSideRewindCapsule(HitCharacter, TraceStart, HitLocation, HitTime);

	if (Character && HitCharacter && DamageCauser && Confirm.HitType != EHitbox::EH_None)
	{
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			GetDamage(Confirm.HitType, DamageCauser->GetDamage()),
			Character->Controller,
			DamageCauser,
			UDamageType::StaticClass()
		);
		HitCharacter->Multicast_SpawnBlood(Confirm.HitLocation, -Confirm.HitNormal, DamageCauser->GetWeaponType());
	}
}

void ULagCompensationComponent::ShotgunServerScoreRequest_Implementation(const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, float HitTime, AWeapon* DamageCauser)
{
	DamageCauserWeapon = DamageCauser;
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

	// assumes that physics asset holds SphylElems in the same order
	for (int i = 0; i < YoungerFrame.HitCapsulesInfo.Num(); ++i)
	{
		const FCapsuleInformation& OlderCapsule = OlderFrame.HitCapsulesInfo[i];
		const FCapsuleInformation& YoungerCapsule = YoungerFrame.HitCapsulesInfo[i];

		FCapsuleInformation InterpCapsuleInfo;
		InterpCapsuleInfo.A = FMath::VInterpTo(OlderCapsule.A, YoungerCapsule.A, 1.f, InterpFraction);
		InterpCapsuleInfo.B = FMath::VInterpTo(OlderCapsule.B, YoungerCapsule.B, 1.f, InterpFraction);
		InterpCapsuleInfo.Radius = YoungerCapsule.Radius;
		InterpCapsuleInfo.Length = YoungerCapsule.Length;
		InterpCapsuleInfo.HitboxType = YoungerCapsule.HitboxType;
		
		InterpFramePackage.HitCapsulesInfo.Add(InterpCapsuleInfo);
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
	//ShowFramePackage(Package, FColor::Black);

	// Enable collision for the head first
	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);

	FHitResult ConfirmHitResult;
	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
	//TraceAgainstCapsules(TraceStart, TraceEnd, HitCharacter);
	UWorld* World = GetWorld();
	if (World && DamageCauserWeapon)
	{
		World->LineTraceSingleByChannel(
			ConfirmHitResult,
			TraceStart,
			TraceEnd,
			ECollisionChannel::ECC_Visibility,
			DamageCauserWeapon->GetTraceQueryParams()
		);
		DrawDebugLine(World, TraceStart, TraceEnd, FColor::Cyan, true);
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
				TraceEnd,
				ECollisionChannel::ECC_Visibility,
				DamageCauserWeapon->GetTraceQueryParams()
			);
			DrawDebugLine(World, TraceStart, TraceEnd, FColor::Cyan, true);
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

FServerSideRewindResultCapsule ULagCompensationComponent::ConfirmHitCapsule(const FFramePackageCapsule& Package, ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation)
{
	if (HitCharacter == nullptr) return FServerSideRewindResultCapsule();
	
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);

	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
	FHitInfo HitInfo = TraceAgainstCapsules(Package, HitCharacter, TraceStart, TraceEnd);

	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
	return FServerSideRewindResultCapsule{HitInfo.HitType, HitInfo.Location, HitInfo.Normal};
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
		if (World && DamageCauserWeapon)
		{
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceStart + TraceEnd,
				ECollisionChannel::ECC_Visibility,
				DamageCauserWeapon->GetTraceQueryParams()
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
				ECollisionChannel::ECC_Visibility,
				DamageCauserWeapon->GetTraceQueryParams()
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

	//SaveFramePackage();
	SaveFramePackageCapsule();
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

		//ShowFramePackage(ThisFrame, FColor::Red);
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
		ShowFramePackageCapsule(ThisFrame, FColor::Red);
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
	if (Character == nullptr || Character->GetMesh() == nullptr || Character->GetMesh()->GetPhysicsAsset() == nullptr) return;

	Package.Time = GetWorld()->GetTimeSeconds();
	Package.Character = Character;

	USkeletalMeshComponent* Mesh = Character->GetMesh();

	for (auto& SkeletalBodySetup : Mesh->GetPhysicsAsset()->SkeletalBodySetups)
	{
		const FName& BName = SkeletalBodySetup->BoneName;
		const FTransform& BoneWorldTransform = Mesh->GetBoneTransform(Mesh->GetBoneIndex(BName));
		const EHitbox BoneHitboxType = HitboxTypes.Contains(BName) ? HitboxTypes[BName] : EHitbox::EH_None;
		for (auto& Sphyl : SkeletalBodySetup->AggGeom.SphylElems)
		{
			const FTransform& LocTransform = Sphyl.GetTransform();
			const FTransform WorldTransform = LocTransform * BoneWorldTransform;
			const float Radius = Sphyl.GetScaledRadius(WorldTransform.GetScale3D());
			const FVector CapsuleCenter = WorldTransform.GetLocation();
			const float CapsuleLength = Sphyl.GetScaledHalfLength(WorldTransform.GetScale3D()) - Radius;
			const FVector CapsuleAxis = WorldTransform.GetUnitAxis(EAxis::Z);
			const FVector A = CapsuleCenter + CapsuleAxis * CapsuleLength;
			const FVector B = CapsuleCenter - CapsuleAxis * CapsuleLength;

			FCapsuleInformation CapsuleInformation;
			CapsuleInformation.A = A;
			CapsuleInformation.B = B;
			CapsuleInformation.Radius = Radius;
			CapsuleInformation.Length = CapsuleLength;
			CapsuleInformation.HitboxType = BoneHitboxType;
			Package.HitCapsulesInfo.Add(CapsuleInformation);
		}
	}
}

void ULagCompensationComponent::DrawCapsuleHitBox()
{
	for (auto& x : Character->GetMesh()->GetPhysicsAsset()->SkeletalBodySetups)
	{
		auto BName = x->BoneName;
		auto BoneWorldTransform = Character->GetMesh()->GetBoneTransform(Character->GetMesh()->GetBoneIndex(BName));
		for (auto& y : x->AggGeom.SphylElems)
		{
			auto LocTransform = y.GetTransform();
			auto WorldTransform = LocTransform * BoneWorldTransform;
			/*
			DrawDebugCapsule(
				GetWorld(),
				WorldTransform.GetLocation(),
				y.Length / 2 + y.Radius,
				y.Radius, WorldTransform.GetRotation(),
				FColor::Red
			);*/
			const float Radius = y.GetScaledRadius(WorldTransform.GetScale3D());
			const FVector CapsuleCenter = WorldTransform.GetLocation();
			const float CapsuleLength = y.GetScaledHalfLength(WorldTransform.GetScale3D()) - Radius;
			const FVector CapsuleAxis = WorldTransform.GetUnitAxis(EAxis::Z);
			const FVector A = CapsuleCenter + CapsuleAxis * CapsuleLength;
			const FVector B = CapsuleCenter - CapsuleAxis * CapsuleLength;
			DrawDebugLine(GetWorld(), A, B, FColor::Green);

			TArray<FSphereInfo> Spheres;
			CapsuleToSpheres(FVector(A), FVector(B), Radius, CapsuleLength, Spheres);

			for (auto Sphere : Spheres)
			{
				DrawDebugSphere(GetWorld(), FVector(Sphere.Center), Sphere.Radius, 12, FColor::White);
			}

			// check for bullet vector intersection for each capsule, saving closest intersection point

			//DrawDebugSphere(GetWorld(), A, Radius, 12, FColor::White);
			//DrawDebugSphere(GetWorld(), B, Radius, 12, FColor::White);
		}
	}
	//UE_LOG(LogTemp, Warning, TEXT("Spheres: %d"), SphereCount);
	//SphereCount = 0;
}

void ULagCompensationComponent::Test(const FVector& TraceStart, const FVector& TraceEnd, const ABlasterCharacter* HitCharacter)
{
	if (HitCharacter == nullptr || HitCharacter->GetMesh() == nullptr || HitCharacter->GetMesh()->GetPhysicsAsset() == nullptr) return;

	uint8 HitSpheres = 0;
	//EHitbox HitType = EHitbox::EH_None;

	for (auto& x : HitCharacter->GetMesh()->GetPhysicsAsset()->SkeletalBodySetups)
	{
		auto BName = x->BoneName;
		auto BoneWorldTransform = HitCharacter->GetMesh()->GetBoneTransform(HitCharacter->GetMesh()->GetBoneIndex(BName));
		for (auto& y : x->AggGeom.SphylElems)
		{
			auto LocTransform = y.GetTransform();
			auto WorldTransform = LocTransform * BoneWorldTransform;
			/*
			DrawDebugCapsule(
				GetWorld(),
				WorldTransform.GetLocation(),
				y.Length / 2 + y.Radius,
				y.Radius, WorldTransform.GetRotation(),
				FColor::Red
			);*/
			const float Radius = y.GetScaledRadius(WorldTransform.GetScale3D());
			const FVector CapsuleCenter = WorldTransform.GetLocation();
			const float CapsuleLength = y.GetScaledHalfLength(WorldTransform.GetScale3D()) - Radius;
			const FVector CapsuleAxis = WorldTransform.GetUnitAxis(EAxis::Z);
			const FVector A = CapsuleCenter + CapsuleAxis * CapsuleLength;
			const FVector B = CapsuleCenter - CapsuleAxis * CapsuleLength;
			DrawDebugLine(GetWorld(), A, B, FColor::Green);

			TArray<FSphereInfo> Spheres;
			CapsuleToSpheres(FVector(A), FVector(B), Radius, CapsuleLength, Spheres);
			const FVector Dir = (TraceEnd - TraceStart).GetSafeNormal();
			const float Length = (TraceEnd - TraceStart).Size();
			for (auto& Sphere : Spheres)
			{
				if (LineSphereIntersection(TraceStart, Dir, Length, Sphere.Center, Sphere.Radius))
				{
					const FVector IntersectionPoint = FirstIntersectionPoint(TraceStart, TraceEnd, Sphere.Center, Sphere.Radius);
					DrawDebugSphere(GetWorld(), FVector(Sphere.Center), Sphere.Radius, 20, FColor::White, true);
					DrawDebugPoint(GetWorld(), IntersectionPoint, 4.f, FColor::Red, true);
					
					++HitSpheres;
					if (HitSpheres > 1)
					{

						return;
					}
				}
			}
		}
	}
}

void ULagCompensationComponent::CapsuleToSpheres(const FVector& A, const FVector& B, const float Radius, const float Length, TArray<FSphereInfo>& OutSpheres)
{
	OutSpheres.Add(FSphereInfo{A, Radius});
	OutSpheres.Add(FSphereInfo{ (A+B)*0.5f, Radius });
	OutSpheres.Add(FSphereInfo{ B, Radius });
}

inline bool ULagCompensationComponent::LineSphereIntersection(const FVector& Start, const FVector& Dir, float Length, const FVector& Origin, float Radius)
{
	const FVector	EO = Start - Origin;
	const float		v = (Dir | (Origin - Start));
	const float		disc = Radius * Radius - ((EO | EO) - v * v);

	if (disc >= 0)
	{
		const float	Time = (v - FMath::Sqrt(disc)) / Length;

		if (Time >= 0 && Time <= 1)
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

inline FVector const ULagCompensationComponent::FirstIntersectionPoint(const FVector& LineStart, const FVector& LineEnd, const FVector& Center, const float Radius)
{
	const FVector Disp = LineEnd - LineStart;
	const FVector Dir = (LineEnd - LineStart).GetSafeNormal();
	const FVector ClosestPoint = FMath::ClosestPointOnLine(LineStart, LineEnd, Center);
	const float ClosestLength = (ClosestPoint - Center).Size();
	const float IntersectionLength = FMath::Sqrt(Radius * Radius - ClosestLength * ClosestLength);

	return ClosestPoint - Dir * IntersectionLength;
}

FHitInfo ULagCompensationComponent::TraceAgainstCapsules(const FFramePackageCapsule& Package, const ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector& TraceEnd)
{
	if (HitCharacter == nullptr || HitCharacter->GetMesh() == nullptr || HitCharacter->GetMesh()->GetPhysicsAsset() == nullptr) return FHitInfo();
	
	FHitInfo HitInfo;
	uint8 HitSpheres = 0;
	const FVector Dir = (TraceEnd - TraceStart).GetSafeNormal();
	const float Length = (TraceEnd - TraceStart).Size();

	for (auto& Capsule : Package.HitCapsulesInfo)
	{
		// convert capsule to spheres for optimized collision check
		TArray<FSphereInfo> Spheres;
		CapsuleToSpheres(Capsule.A, Capsule.B, Capsule.Radius, Capsule.Length, Spheres);

		for (auto& Sphere : Spheres)
		{
			// check if weapon trace intersects with each sphere of this capsule
			if (LineSphereIntersection(TraceStart, Dir, Length, Sphere.Center, Sphere.Radius))
			{
				++HitSpheres;
				const FVector IntersectionPoint = FirstIntersectionPoint(TraceStart, TraceEnd, Sphere.Center, Sphere.Radius);
				if (HitInfo.HitType < Capsule.HitboxType)
				{
					HitInfo.Location = IntersectionPoint;
					HitInfo.HitType = Capsule.HitboxType;
					HitInfo.Normal = (HitInfo.Location - Sphere.Center) / Sphere.Radius;
				}
				else if (HitInfo.HitType == Capsule.HitboxType)
				{
					const float Dist1 = FVector::DistSquared(HitInfo.Location, TraceStart);
					const float Dist2 = FVector::DistSquared(IntersectionPoint, TraceStart);
					if (Dist1 > Dist2)
					{
						HitInfo.Location = IntersectionPoint;
						HitInfo.HitType = Capsule.HitboxType;
						HitInfo.Normal = (HitInfo.Location - Sphere.Center) / Sphere.Radius;
					}
				}
				if (HitSpheres >= MaxSpheresHit)
				{
					return HitInfo;
				}
			}
		}
	}
	return HitInfo;
}