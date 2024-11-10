#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"



UENUM(BlueprintType)
enum class EHitbox : uint8
{
	//0
	EH_None			UMETA(DisplayName = "None"),
	//1
	EH_Legs			UMETA(DisplayName = "Legs"),
	//2
	EH_Body			UMETA(DisplayName = "Body"),
	//3
	EH_Head			UMETA(DisplayName = "Head"),

	EH_MAX			UMETA(DisplayName = "DefaultMAX")
};

static TMap<FName, EHitbox> HitboxTypes =
{
	{FName("head"), EHitbox::EH_Head },
	{FName("neck_01"), EHitbox::EH_Head },
	{FName("pelvis"), EHitbox::EH_Body },
	{FName("spine_01"), EHitbox::EH_Body },
	{FName("spine_02"), EHitbox::EH_Body },
	{FName("spine_03"), EHitbox::EH_Body },
	{FName("upperarm_l"), EHitbox::EH_Body },
	{FName("upperarm_r"), EHitbox::EH_Body },
	{FName("lowerarm_l"), EHitbox::EH_Body },
	{FName("lowerarm_r"), EHitbox::EH_Body },
	{FName("hand_l"), EHitbox::EH_Body },
	{FName("hand_r"), EHitbox::EH_Body },
	{FName("thigh_l"), EHitbox::EH_Legs },
	{FName("thigh_r"), EHitbox::EH_Legs },
	{FName("calf_l"), EHitbox::EH_Legs },
	{FName("calf_r"), EHitbox::EH_Legs },
	{FName("foot_l"), EHitbox::EH_Legs },
	{FName("foot_r"), EHitbox::EH_Legs }
};

inline float GetDamage(EHitbox Type, float WeaponDamage)
{
	float Damage = WeaponDamage;
	if (Type == EHitbox::EH_Legs) { Damage = WeaponDamage * 0.85f; }
	else if (Type == EHitbox::EH_Body) { Damage = WeaponDamage * 1.25f; }
	else if (Type == EHitbox::EH_Head) { Damage = WeaponDamage * 5.f; }
	return Damage;
}

class BLASTER_API Hitbox
{
	
};