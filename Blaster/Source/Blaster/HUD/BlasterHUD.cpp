// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterHUD.h"
#include "GameFramework/PlayerController.h"
#include  "CharacterOverlay.h"
#include "Announcement.h"
#include "ElimAnnouncement.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/HorizontalBox.h"
#include "Components/CanvasPanelSlot.h"

void ABlasterHUD::BeginPlay()
{
	Super::BeginPlay();

	AddElimAnnouncement("Player1", "Player2");
}

void ABlasterHUD::AddCharacterOverlay()
{
	OwningPlayer = OwningPlayer == nullptr ? GetOwningPlayerController() : OwningPlayer;
	if (OwningPlayer && CharacterOverlayClass && !CharacterOverlay)
	{
		CharacterOverlay = CreateWidget<UCharacterOverlay>(OwningPlayer, CharacterOverlayClass);
		CharacterOverlay->AddToViewport();
	}
}

void ABlasterHUD::AddAnnouncement()
{
	OwningPlayer = OwningPlayer == nullptr ? GetOwningPlayerController() : OwningPlayer;
	if (OwningPlayer && AnnouncementClass)
	{
		Announcement = CreateWidget<UAnnouncement>(OwningPlayer, AnnouncementClass);
		Announcement->AddToViewport();
	}
}

void ABlasterHUD::AddElimAnnouncement(FString AttackerName, FString VictimName)
{
	OwningPlayer = OwningPlayer == nullptr ? GetOwningPlayerController() : OwningPlayer;
	if (OwningPlayer && ElimAnnouncementClass)
	{
		UElimAnnouncement* ElimAnnouncementWidget = CreateWidget<UElimAnnouncement>(OwningPlayer, ElimAnnouncementClass);
		if (ElimAnnouncementWidget)
		{
			ElimAnnouncementWidget->SetElimAnnouncementText(AttackerName, VictimName);
			ElimAnnouncementWidget->AddToViewport();
			UpdateEliminationStack(ElimAnnouncementWidget);
			ShowElimAnnouncements();
			GetWorldTimerManager().SetTimer(
				ElimAnnouncementHandle,
				this,
				&ABlasterHUD::ElimAnnouncementTimerFinished,
				4.0f,
				false,
				4.0f
			);
		}
	}
}

void ABlasterHUD::DrawHUD()
{
	Super::DrawHUD();

	FVector2D ViewportSize;
	if (GEngine)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		const FVector2D ViewportCenter(ViewportSize.X * 0.5f, ViewportSize.Y * 0.5f);

		float SpreadScaled = CrosshairSpreadMax * HUDPackage.CrosshairSpread;

		if (HUDPackage.CrosshairsCenter)
		{
			FVector2D Spread(0.f, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsCenter, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsLeft)
		{
			FVector2D Spread(-SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsLeft, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsRight)
		{
			FVector2D Spread(SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsRight, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsTop)
		{
			FVector2D Spread(0.f, -SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsTop, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsBottom)
		{
			FVector2D Spread(0.f, SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsBottom, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
	}
}

void ABlasterHUD::DrawCrosshair(UTexture2D* Texture, FVector2D ViewportCenter, FVector2D Spread, FLinearColor CrosshairColor)
{
	const float TextureWidth = Texture->GetSizeX();
	const float TextureHeight = Texture->GetSizeY();
	const FVector2D TextureDrawPoint(
		ViewportCenter.X - (TextureWidth * 0.5f) + Spread.X,
		ViewportCenter.Y - (TextureHeight * 0.5f) + Spread.Y
	);

	DrawTexture(
		Texture,
		TextureDrawPoint.X,
		TextureDrawPoint.Y,
		TextureWidth,
		TextureHeight,
		0.f,
		0.f,
		1.f,
		1.f,
		CrosshairColor
	);
}

void ABlasterHUD::UpdateEliminationStack(UElimAnnouncement* NewElimAnnounement)
{
	ElimAnnouncementQueue.Add(NewElimAnnounement);

	if (ElimAnnouncementQueue.Num() > MaxElimAnnouncements)
	{
		UElimAnnouncement* ElimAnnouncementToRemove = ElimAnnouncementQueue[0];
		ElimAnnouncementQueue.RemoveAt(0);
		ElimAnnouncementToRemove->RemoveFromParent();
		/*
		for (UElimAnnouncement* CurrentAnnouncement : ElimAnnouncementStack)
		{
			if (CurrentAnnouncement && CurrentAnnouncement->AnnouncementBox)
			{
				UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(CurrentAnnouncement->AnnouncementBox);
				if (CanvasSlot)
				{
					FVector2D Position = CanvasSlot->GetPosition();
					FVector2D NewPosition(Position.X, Position.Y + CanvasSlot->GetSize().Y);
					CanvasSlot->SetPosition(NewPosition);
				}
			}
		}
		*/
	}
}

void ABlasterHUD::ElimAnnouncementTimerFinished()
{
	for (UElimAnnouncement* CurrentAnnouncement : ElimAnnouncementQueue)
	{
		if (CurrentAnnouncement && CurrentAnnouncement->FadeOutAnnouncement)
		{
			CurrentAnnouncement->PlayAnimation(CurrentAnnouncement->FadeOutAnnouncement);
		}
	}
}

void ABlasterHUD::ShowElimAnnouncements()
{
	GetWorldTimerManager().ClearTimer(ElimAnnouncementHandle);

	for (UElimAnnouncement* CurrentAnnouncement : ElimAnnouncementQueue)
	{
		if (CurrentAnnouncement && CurrentAnnouncement->AnnouncementBox && CurrentAnnouncement->AnnouncementText)
		{
			if (CurrentAnnouncement->IsAnimationPlaying(CurrentAnnouncement->FadeOutAnnouncement))
			{
				CurrentAnnouncement->StopAnimation(CurrentAnnouncement->FadeOutAnnouncement);
			}

			UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(CurrentAnnouncement->AnnouncementBox);
			if (CanvasSlot)
			{
				FVector2D Position = CanvasSlot->GetPosition();
				FVector2D NewPosition(Position.X, Position.Y - CanvasSlot->GetSize().Y);
				CanvasSlot->SetPosition(NewPosition);
			}

			CurrentAnnouncement->AnnouncementText->SetRenderOpacity(1.0f);
		}
	}
}
