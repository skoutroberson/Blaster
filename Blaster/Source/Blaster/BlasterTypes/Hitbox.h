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

const static TMap<FName, EHitbox> HitboxTypes =
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

#define LEG_DAMAGE_MULTIPLIER 0.85f
#define BODY_DAMAGE_MULTIPLIER 1.25f
#define HEAD_DAMAGE_MULTIPLIER 5.f

struct FHitbox
{
	static inline float GetDamage(EHitbox Type, float WeaponDamage)
	{
		float Damage = WeaponDamage;
		if (Type == EHitbox::EH_Legs) { Damage = WeaponDamage * LEG_DAMAGE_MULTIPLIER; }
		else if (Type == EHitbox::EH_Body) { Damage = WeaponDamage * BODY_DAMAGE_MULTIPLIER; }
		else if (Type == EHitbox::EH_Head) { Damage = WeaponDamage * HEAD_DAMAGE_MULTIPLIER; }
		return Damage;
	}
};


class BLASTER_API Hitbox
{
	
};