/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/effect/MobEffectInstance.h"
#include "world/effect/MobEffect.h"

const MobEffectInstance MobEffectInstance::NO_EFFECT = MobEffectInstance(0, 0, 0, false, false);

MobEffectInstance::MobEffectInstance(int id, int duration, int amplifier, bool ambient, bool visible)
	: mId(id)
	, mDuration(duration)
	, mAmplifier(amplifier)
	, mAmbient(ambient)
	, mEffectVisible(visible) {}

void MobEffectInstance::update(const MobEffectInstance& takeOver) {
	if (mId != takeOver.mId) {
		return;
	}
	if (takeOver.mAmplifier > mAmplifier) {
		mAmplifier = takeOver.mAmplifier;
		mDuration = takeOver.mDuration;
	} else if (takeOver.mAmplifier == mAmplifier && mDuration < takeOver.mDuration) {
		mDuration = takeOver.mDuration;
	} else if (!takeOver.mAmbient && mAmbient) {
		mAmbient = takeOver.mAmbient;
	}
}

bool MobEffectInstance::tick(Mob* mob) {
	if (mId == 0 || mDuration <= 0) {
		return false;
	}
	if (MobEffect* effect = MobEffect::getById(mId)) {
		effect->applyEffects(mob, mDuration, mAmplifier);
	}
	--mDuration;
	return mDuration > 0;
}
