// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define ECC_SkeletalMesh ECollisionChannel::ECC_GameTraceChannel1
#define ECC_Blood ECollisionChannel::ECC_GameTraceChannel2 // need to set the player mesh to block this for blood particles to show
#define ECC_PlayerHitBox ECollisionChannel::ECC_GameTraceChannel3