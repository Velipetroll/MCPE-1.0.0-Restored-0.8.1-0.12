/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/attribute/HealthAttributeDelegate.h"
#include "world/attribute/AttributeBuff.h"
#include "world/attribute/AttributeInstance.h"
#include "world/entity/EntityDamageSource.h"
#include "world/entity/Mob.h"
#include "world/level/Level.h"

HealthAttributeDelegate::HealthAttributeDelegate(const AttributeInstance& instance, Mob* mob)
	: AttributeInstanceDelegate(instance)
	, mTickCounter(0)
	, mMob(mob) {}

void HealthAttributeDelegate::tick() {
	if (!mMob || mMob->getLevel().isClientSide()) {
		return;
	}

	++mTickCounter;
	if (mMob->getLevel().getDifficulty() == Difficulty::Peaceful &&
	    mMob->hasCategory(EntityCategory::Player) &&
	    (mTickCounter % 20) == 0) {
		if (AttributeInstance* health = _getMutableInstance()) {
			health->addBuff(InstantaneousAttributeBuff(1.0f, AttributeBuffType::Heal));
		}
	}
}

bool HealthAttributeDelegate::change(float oldValue, float newValue, const AttributeBuffInfo& buffInfo) {
	if (!mMob) {
		return true;
	}

	int oldInt = static_cast<int>(std::floor(oldValue + 0.5f));
	int newInt = static_cast<int>(std::floor(newValue + 0.5f));
	int deltaInt = oldInt - newInt;
	if (deltaInt == 0) {
		return true;
	}

	AttributeBuffType type = buffInfo.mType;
	if (mMob->isInvertedHealAndHarm()) {
		if (type == AttributeBuffType::Heal) {
			type = AttributeBuffType::Harm;
		} else if (type == AttributeBuffType::Harm) {
			type = AttributeBuffType::Heal;
		}
	}

	if (type == AttributeBuffType::Unknown) {
		EntityDamageSource src(EntityDamageCause::Starve);
		return mMob->hurtEffects(src, deltaInt, true, false);
	} else if (type == AttributeBuffType::Heal) {
		mMob->healEffects(-deltaInt);
	}
	return true;
}

float HealthAttributeDelegate::getBuffValue(const AttributeBuff& buff) const {
	if (mMob && mMob->isInvertedHealAndHarm()) {
		if (buff.getType() == AttributeBuffType::Heal || buff.getType() == AttributeBuffType::Harm) {
			return -buff.getAmount();
		}
	}
	return buff.getAmount();
}
