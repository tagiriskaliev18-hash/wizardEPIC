// WizardTypes.h - shared enums, structs, delegates and content-path constants for the WizardEPIC module.
//
// Everything that two or more game systems must agree on lives here so the systems can be
// implemented independently: spell data, destruction events, FX sprite kinds and the asset paths
// produced by Scripts/forge (see Content/Wizard after the forge has run).

#pragma once

#include "CoreMinimal.h"
#include "WizardTypes.generated.h"

class AActor;
class USpellDataAsset;
class UTexture2D;

DECLARE_LOG_CATEGORY_EXTERN(LogWizard, Log, All);

/** The four elemental schools. The enum value is also the HUD spell-slot index (0..3). */
UENUM(BlueprintType)
enum class ESpellElement : uint8
{
	Fire        UMETA(DisplayName = "Fire"),
	Earth       UMETA(DisplayName = "Earth"),
	Wind        UMETA(DisplayName = "Wind"),
	Lightning   UMETA(DisplayName = "Lightning")
};

/** How a spell travels from the caster to its impact. */
UENUM(BlueprintType)
enum class ESpellDelivery : uint8
{
	/** A spawned actor flies to the target and detonates on contact. */
	Projectile UMETA(DisplayName = "Projectile"),
	/** A sequence of stone pillars erupts along the ground toward the target. */
	GroundLine UMETA(DisplayName = "Ground Line"),
	/** A short-lived directional volume in front of the caster. */
	Cone UMETA(DisplayName = "Cone"),
	/** An instantaneous bolt on the target point, repeated a few times. */
	Strike UMETA(DisplayName = "Strike")
};

/** Physical families used for damage multipliers, debris FX, sounds and scoring. */
UENUM(BlueprintType)
enum class EDestructibleMaterial : uint8
{
	Stone   UMETA(DisplayName = "Stone"),
	Wood    UMETA(DisplayName = "Wood"),
	Pottery UMETA(DisplayName = "Pottery"),
	Rock    UMETA(DisplayName = "Rock"),
	Crystal UMETA(DisplayName = "Crystal"),
	Foliage UMETA(DisplayName = "Foliage")
};

/**
 * Kinds of sprite particles. Each kind is one texture + one blend mode, both produced by the forge:
 * Smoke (T_FX_Smoke, translucent), Dust (T_FX_Dust, translucent), Leaf (T_FX_Leaf, translucent),
 * Splinter (T_FX_Splinter, translucent), Shard (T_FX_Shard, translucent),
 * Spark (T_FX_Spark, additive), Ember (T_FX_Ember, additive), Flash (T_FX_Flash, additive),
 * Glow (T_FX_Glow, additive), Sparkle (T_FX_Sparkle, additive), Rune (T_FX_Rune, additive).
 */
UENUM(BlueprintType)
enum class EFXSpriteKind : uint8
{
	Smoke, Dust, Leaf, Splinter, Shard, Spark, Ember, Flash, Glow, Sparkle, Rune
};

/** Damage multiplier per destructible material family. A spell of one element cares little for some families. */
USTRUCT(BlueprintType)
struct FMaterialDamageTable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Damage)
	float Stone = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Damage)
	float Wood = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Damage)
	float Pottery = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Damage)
	float Rock = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Damage)
	float Crystal = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Damage)
	float Foliage = 1.f;

	/** Multiplier for one material family. */
	float For(EDestructibleMaterial Material) const;
};

/** Everything a destructible needs to know about one spell impact. Built from a USpellDataAsset. */
USTRUCT(BlueprintType)
struct FSpellImpact
{
	GENERATED_BODY()

	/** Centre of the blast / origin of the cone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Impact)
	FVector Origin = FVector::ZeroVector;

	/** Outward direction for directional pushes (cone spells, earth launch bias). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Impact)
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Impact)
	ESpellElement Element = ESpellElement::Fire;

	/** Raw damage at the centre of the blast, before material multipliers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Impact)
	float Damage = 100.f;

	/** Pieces / props whose centre lies inside this radius take damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Impact)
	float Radius = 600.f;

	/** Launch speed (cm/s) given to a released piece at the centre of the blast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Impact)
	float LaunchSpeed = 1400.f;

	/** Fraction of LaunchSpeed added straight up on top of the radial direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Impact)
	float UpBias = 0.25f;

	/** Random angular speed (deg/s) given to released pieces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Impact)
	float Spin = 420.f;

	/** 1 = tap cast, up to the spell's ChargeScaleMax when fully charged. Scales damage, radius, FX and feel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Impact)
	float ChargeScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Impact)
	FMaterialDamageTable Multipliers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Impact)
	TObjectPtr<AActor> Instigator = nullptr;

	/** The spell that caused this impact (may be null for non-spell sources). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Impact)
	TObjectPtr<USpellDataAsset> Spell = nullptr;

	/** Effective damage (material multiplier not applied) at a world location: full at the centre, 35% at the radius edge, 0 outside. */
	float DamageAt(const FVector& WorldLocation) const;

	/** Launch velocity (radial + up bias + falloff) for a piece at a world location. */
	FVector LaunchVelocityAt(const FVector& WorldLocation) const;
};

/** One broken thing. Emitted by the destruction subsystem, batched once per frame. */
USTRUCT(BlueprintType)
struct FDestructibleEvent
{
	GENERATED_BODY()

	/** World-space centre of the broken piece or prop. */
	UPROPERTY(BlueprintReadOnly, Category = Event)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = Event)
	EDestructibleMaterial Material = EDestructibleMaterial::Stone;

	/** Points awarded for this break before the combo multiplier. */
	UPROPERTY(BlueprintReadOnly, Category = Event)
	int32 Points = 0;

	/** Longest extent of the broken thing in cm - drives FX and sound scale. */
	UPROPERTY(BlueprintReadOnly, Category = Event)
	float Size = 50.f;

	/** True for crystal targets (worth bonus points, distinct FX). */
	UPROPERTY(BlueprintReadOnly, Category = Event)
	bool bIsCrystal = false;

	/** True when an intact whole prop (pot, crate, tree...) broke for the first time. */
	UPROPERTY(BlueprintReadOnly, Category = Event)
	bool bWasWholeProp = false;

	/** True when the piece was released because it lost its support, not by direct damage. */
	UPROPERTY(BlueprintReadOnly, Category = Event)
	bool bFromCascade = false;

	/** True for the extra event emitted when a structure mostly collapses (Points = collapse bonus). */
	UPROPERTY(BlueprintReadOnly, Category = Event)
	bool bIsCollapseBonus = false;

	UPROPERTY(BlueprintReadOnly, Category = Event)
	ESpellElement Element = ESpellElement::Fire;

	UPROPERTY(BlueprintReadOnly, Category = Event)
	float ChargeScale = 1.f;
};

/** Floating score text request. */
USTRUCT(BlueprintType)
struct FScorePopupInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = Score)
	FText Text;

	UPROPERTY(BlueprintReadOnly, Category = Score)
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = Score)
	FVector WorldLocation = FVector::ZeroVector;

	/** Relative size (1 = normal piece, 1.6 = crystal, 2.2 = structure collapse). */
	UPROPERTY(BlueprintReadOnly, Category = Score)
	float Scale = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = Score)
	float Life = 1.2f;
};

/** All break events of one frame. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDestructibleBatch, const TArray<FDestructibleEvent>&, Events);

/** The arena was reset (all rubble removed, all structures restored). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArenaReset);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnScoreChanged, int64, Score, int64, BestScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboChanged, float, Multiplier, float, TimeLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScorePopup, const FScorePopupInfo&, Info);

/**
 * Asset paths produced by Scripts/forge. A path is a package path; load it with
 * FSoftObjectPath(Path).TryLoad() - assets are never hard-referenced from C++ so missing content
 * degrades gracefully instead of failing to load the game module.
 */
namespace WizardPaths
{
	/** The level. Also set as EditorStartupMap / GameDefaultMap in DefaultEngine.ini. */
	constexpr const TCHAR* ArenaLevel = TEXT("/Game/Wizard/Levels/L_Arena");

	/** Animated third-person pawn from the engine's Third Person template (copied into Content/). */
	constexpr const TCHAR* TemplatePawnClass = TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter");

	/** Spell data assets in HUD-slot order. */
	constexpr const TCHAR* Spells[] =
	{
		TEXT("/Game/Wizard/Core/DA_Spell_Fire"),
		TEXT("/Game/Wizard/Core/DA_Spell_Earth"),
		TEXT("/Game/Wizard/Core/DA_Spell_Wind"),
		TEXT("/Game/Wizard/Core/DA_Spell_Lightning")
	};

	/** HUD icon per element, in ESpellElement order. */
	constexpr const TCHAR* SpellIcons[] =
	{
		TEXT("/Game/Wizard/UI/Icons/T_Icon_Fire"),
		TEXT("/Game/Wizard/UI/Icons/T_Icon_Earth"),
		TEXT("/Game/Wizard/UI/Icons/T_Icon_Wind"),
		TEXT("/Game/Wizard/UI/Icons/T_Icon_Lightning")
	};

	/** Heavy display face for the wordmark / score, regular face for labels. Both are UFont assets. */
	constexpr const TCHAR* DisplayFont = TEXT("/Game/Wizard/UI/Fonts/F_Display");
	constexpr const TCHAR* BodyFont = TEXT("/Game/Wizard/UI/Fonts/F_Body");

	/** Folder roots. Build full paths as FString(WizardPaths::FXMaterials) + TEXT("/M_FX_Add"). */
	constexpr const TCHAR* FXMaterials = TEXT("/Game/Wizard/FX/Materials");
	constexpr const TCHAR* FXMeshes = TEXT("/Game/Wizard/FX/Meshes");
	constexpr const TCHAR* FXTextures = TEXT("/Game/Wizard/FX/Textures");
	constexpr const TCHAR* SFX = TEXT("/Game/Wizard/Audio/SFX");
	constexpr const TCHAR* Music = TEXT("/Game/Wizard/Audio/Music");
}
