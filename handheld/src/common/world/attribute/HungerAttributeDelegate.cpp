/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/attribute/HungerAttributeDelegate.h"
#include "world/attribute/AttributeBuff.h"
#include "world/attribute/AttributeInstance.h"
#include "world/attribute/SharedAttributes.h"
#include "world/entity/EntityDamageSource.h"
#include "world/entity/player/Player.h"
#include "world/food/FoodConstants.h"
#include "world/level/Level.h"

HungerAttributeDelegate::HungerAttributeDelegate(const AttributeInstance& instance, Player* player)
	: AttributeInstanceDelegate(instance)
	, mHealthTickCount(0)
	, mTickCounter(0)
	, mLastHunger(instance.getCurrentValue())
	, mPlayer(player) {}

void HungerAttributeDelegate::notify(int64_t type) {
	if (type != 0 || !mPlayer || mPlayer->isCreative()) {
		return;
	}

	int difficulty = static_cast<int>(mPlayer->getLevel().getDifficulty());
	AttributeInstance* sat = mPlayer->getMutableAttribute(Player::SATURATION);

	if (sat && sat->getCurrentValue() > 0.0f) {
		InstantaneousAttributeBuff buff(-1.0f, AttributeBuffType::Food);
		sat->addBuff(buff);
	} else if (difficulty != 0 /* Peaceful */) {
		if (AttributeInstance* hunger = _getMutableInstance()) {
			InstantaneousAttributeBuff buff(-1.0f, AttributeBuffType::Food);
			hunger->addBuff(buff);
		}
	}
}

void HungerAttributeDelegate::tick() {
	if (!mPlayer || mPlayer->isCreative()) {
		return;
	}

	int difficulty = static_cast<int>(mPlayer->getLevel().getDifficulty());
	float hunger = _getInstance().getCurrentValue();
	mLastHunger = hunger;
	mTickCounter++;

	// Peaceful hunger regeneration: +1 hunger every 10 ticks
	if (difficulty == 0 && (mTickCounter % 10) == 0) {
		if (AttributeInstance* mutHunger = _getMutableInstance()) {
			InstantaneousAttributeBuff buff(1.0f, AttributeBuffType::Food);
			mutHunger->addBuff(buff);
		}
	}

	// Natural health regeneration: hunger >= 18 and player is hurt (0 < health < maxHealth)
	if (hunger >= static_cast<float>(FoodConstants::HEAL_LEVEL) && mPlayer->isHurt()) {
		mHealthTickCount++;
		if (mHealthTickCount < FoodConstants::HEALTH_TICK_COUNT) {
			return;
		}
		if (AttributeInstance* health = mPlayer->getMutableAttribute(SharedAttributes::HEALTH)) {
			health->addBuff(InstantaneousAttributeBuff(1.0f, AttributeBuffType::Heal));
		}
		if (AttributeInstance* exh = mPlayer->getMutableAttribute(Player::EXHAUSTION)) {
			exh->addBuff(InstantaneousAttributeBuff(FoodConstants::EXHAUSTION_HEAL, AttributeBuffType::Food));
		}
	}
	// Starvation damage: hunger <= 0
	else if (hunger <= static_cast<float>(FoodConstants::STARVE_LEVEL)) {
		mHealthTickCount++;
		if (mHealthTickCount < FoodConstants::HEALTH_TICK_COUNT) {
			return;
		}
		int hp = mPlayer->getHealth();
		// Hard (3): lethal (down to 0)
		// Normal (2): down to 1 HP (hp >= 2)
		// Easy (1) / Peaceful (0): stops at 10 HP (hp > 10)
		if (difficulty == 3 || hp > 10 || (difficulty == 2 && hp >= 2)) {
			if (AttributeInstance* health = mPlayer->getMutableAttribute(SharedAttributes::HEALTH)) {
				health->addBuff(InstantaneousAttributeBuff(-1.0f, AttributeBuffType::Unknown));
			}
			EntityDamageSource src(EntityDamageCause::Starve);
			mPlayer->hurt(src, 1, true, false);
		}
	}

	mHealthTickCount = 0;
}
