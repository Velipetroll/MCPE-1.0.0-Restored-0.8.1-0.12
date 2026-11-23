/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/effect/MobEffect.h"
#include "world/attribute/AttributeInstance.h"
#include "world/attribute/SharedAttributes.h"
#include "world/entity/EntityDamageSource.h"
#include "world/entity/Mob.h"
#include "world/entity/player/Player.h"

static std::unique_ptr<MobEffect> sOwnedMobEffects[MobEffect::NUM_EFFECTS];
static bool sEffectsInitialized = false;

MobEffect* MobEffect::mMobEffects[MobEffect::NUM_EFFECTS] = { nullptr };

MobEffect* MobEffect::MOVEMENT_SPEED = nullptr;
MobEffect* MobEffect::MOVEMENT_SLOWDOWN = nullptr;
MobEffect* MobEffect::DIG_SPEED = nullptr;
MobEffect* MobEffect::DIG_SLOWDOWN = nullptr;
MobEffect* MobEffect::DAMAGE_BOOST = nullptr;
MobEffect* MobEffect::HEAL = nullptr;
MobEffect* MobEffect::HARM = nullptr;
MobEffect* MobEffect::JUMP = nullptr;
MobEffect* MobEffect::CONFUSION = nullptr;
MobEffect* MobEffect::REGENERATION = nullptr;
MobEffect* MobEffect::DAMAGE_RESISTANCE = nullptr;
MobEffect* MobEffect::FIRE_RESISTANCE = nullptr;
MobEffect* MobEffect::WATER_BREATHING = nullptr;
MobEffect* MobEffect::INVISIBILITY = nullptr;
MobEffect* MobEffect::BLINDNESS = nullptr;
MobEffect* MobEffect::NIGHT_VISION = nullptr;
MobEffect* MobEffect::HUNGER = nullptr;
MobEffect* MobEffect::WEAKNESS = nullptr;
MobEffect* MobEffect::POISON = nullptr;
MobEffect* MobEffect::WITHER = nullptr;
MobEffect* MobEffect::HEALTH_BOOST = nullptr;
MobEffect* MobEffect::ABSORPTION = nullptr;
MobEffect* MobEffect::SATURATION = nullptr;
MobEffect* MobEffect::LEVITATION = nullptr;

MobEffect::MobEffect(int id, const std::string& resourceName, const std::string& locName, bool isHarmful, int color, int icon)
	: mId(id)
	, mIsHarmful(isHarmful)
	, mColor(color)
	, mDescriptionId(locName)
	, mResourceName(resourceName)
	, mIcon(icon) {
	if (id >= 0 && id < NUM_EFFECTS) {
		mMobEffects[id] = this;
	}
}

void MobEffect::applyEffects(Mob* mob, int durationTicks, int amplifier) const {
	if (!mob || mob->getLevel().isClientSide()) {
		return;
	}
	if (mId == 10 /* REGENERATION */) {
		int interval = std::max(1, 50 >> std::min(amplifier, 5));
		if ((durationTicks % interval) == 0 && mob->getHealth() > 0 && mob->getHealth() < mob->getMaxHealth()) {
			mob->heal(1);
		}
	} else if (mId == 19 /* POISON */) {
		int interval = std::max(1, 25 >> std::min(amplifier, 5));
		if ((durationTicks % interval) == 0 && mob->getHealth() > 1) {
			EntityDamageSource src(EntityDamageCause::Magic);
			mob->hurt(src, 1, false, false);
		}
	} else if (mId == 20 /* WITHER */) {
		int interval = std::max(1, 40 >> std::min(amplifier, 5));
		if ((durationTicks % interval) == 0 && mob->getHealth() > 0) {
			EntityDamageSource src(EntityDamageCause::Wither);
			mob->hurt(src, 1, false, false);
		}
	} else if (mId == 17 /* HUNGER */) {
		if (mob->hasCategory(EntityCategory::Player)) {
			static_cast<Player*>(mob)->causeFoodExhaustion(0.025f * static_cast<float>(amplifier + 1));
		}
	} else if (mId == 22 /* ABSORPTION */) {
		if (AttributeInstance* abs = mob->getMutableAttribute(SharedAttributes::ABSORPTION)) {
			float desired = static_cast<float>(4 * (amplifier + 1));
			if (abs->getCurrentValue() < desired) {
				abs->serializationSetValue(desired, enum_cast(AttributeOperands::OPERAND_CURRENT), std::max(abs->getMaxValue(), desired));
			}
		}
	}
}

void MobEffect::removeEffects(Mob* mob) const {
	if (!mob) {
		return;
	}
	if (mId == 22 /* ABSORPTION */) {
		if (AttributeInstance* abs = mob->getMutableAttribute(SharedAttributes::ABSORPTION)) {
			abs->serializationSetValue(0.0f, enum_cast(AttributeOperands::OPERAND_CURRENT));
		}
	}
}

static MobEffect* makeEffect(int id, const char* resName, const char* locName, bool harmful, int color, int icon) {
	sOwnedMobEffects[id] = make_unique<MobEffect>(id, resName, locName, harmful, color, icon);
	return sOwnedMobEffects[id].get();
}

void MobEffect::initEffects() {
	if (sEffectsInitialized) {
		return;
	}
	sEffectsInitialized = true;

	MOVEMENT_SPEED = makeEffect(1, "speed", "potion.moveSpeed", false, 0x7CAFC6, 0);
	MOVEMENT_SLOWDOWN = makeEffect(2, "slowness", "potion.moveSlowdown", true, 0x5A6C81, 1);
	DIG_SPEED = makeEffect(3, "haste", "potion.digSpeed", false, 0xD9C043, 2);
	DIG_SLOWDOWN = makeEffect(4, "mining_fatigue", "potion.digSlowDown", true, 0x4A4217, 3);
	DAMAGE_BOOST = makeEffect(5, "strength", "potion.damageBoost", false, 0x932423, 4);
	HEAL = makeEffect(6, "instant_health", "potion.heal", false, 0xF82423, -1);
	HARM = makeEffect(7, "instant_damage", "potion.harm", true, 0x430A09, -1);
	JUMP = makeEffect(8, "jump_boost", "potion.jump", false, 0x22FF4C, 10);
	CONFUSION = makeEffect(9, "nausea", "potion.confusion", true, 0x551D4A, 11);
	REGENERATION = makeEffect(10, "regeneration", "potion.regeneration", false, 0xCD5CAB, 7);
	DAMAGE_RESISTANCE = makeEffect(11, "resistance", "potion.resistance", false, 0x99453A, 14);
	FIRE_RESISTANCE = makeEffect(12, "fire_resistance", "potion.fireResistance", false, 0xE49A3A, 15);
	WATER_BREATHING = makeEffect(13, "water_breathing", "potion.waterBreathing", false, 0x2E5299, 16);
	INVISIBILITY = makeEffect(14, "invisibility", "potion.invisibility", false, 0x7F8392, 17);
	BLINDNESS = makeEffect(15, "blindness", "potion.blindness", true, 0x1F1F23, 13);
	NIGHT_VISION = makeEffect(16, "night_vision", "potion.nightVision", false, 0x1F1FA1, 12);
	HUNGER = makeEffect(17, "hunger", "potion.hunger", true, 0x587653, 9);
	WEAKNESS = makeEffect(18, "weakness", "potion.weakness", true, 0x484D48, 5);
	POISON = makeEffect(19, "poison", "potion.poison", true, 0x4E9331, 6);
	WITHER = makeEffect(20, "wither", "potion.wither", true, 0x352A27, 19);
	HEALTH_BOOST = makeEffect(21, "health_boost", "potion.healthBoost", false, 0xF87D23, 18);
	ABSORPTION = makeEffect(22, "absorption", "potion.absorption", false, 0x2552A5, 20);
	SATURATION = makeEffect(23, "saturation", "potion.saturation", false, 0xF82423, -1);
	LEVITATION = makeEffect(24, "levitation", "potion.levitation", true, 0xCEFFFF, 22);
}

void MobEffect::teardownEffects() {
	for (int i = 0; i < NUM_EFFECTS; ++i) {
		mMobEffects[i] = nullptr;
		sOwnedMobEffects[i].reset();
	}
	sEffectsInitialized = false;
}

MobEffect* MobEffect::getById(int id) {
	initEffects();
	if (id >= 0 && id < NUM_EFFECTS) {
		return mMobEffects[id];
	}
	return nullptr;
}

MobEffect* MobEffect::getByName(const std::string& name) {
	initEffects();
	std::string lower = Util::toLower(name);
	for (int i = 0; i < NUM_EFFECTS; ++i) {
		if (mMobEffects[i] && mMobEffects[i]->getResourceName() == lower) {
			return mMobEffects[i];
		}
	}
	return nullptr;
}
