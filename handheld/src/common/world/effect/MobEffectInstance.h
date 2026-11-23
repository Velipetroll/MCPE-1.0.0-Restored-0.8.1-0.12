/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

class Mob;

class MobEffectInstance {
public:
	static const MobEffectInstance NO_EFFECT;

	MobEffectInstance() = default;
	explicit MobEffectInstance(int id, int duration = 0, int amplifier = 0, bool ambient = false, bool visible = true);

	int getId() const { return mId; }
	int getDuration() const { return mDuration; }
	int getAmplifier() const { return mAmplifier; }
	bool isAmbient() const { return mAmbient; }
	bool isVisible() const { return mEffectVisible; }
	bool isNoCounter() const { return mDuration < 0; }

	void update(const MobEffectInstance& takeOver);
	bool tick(Mob* mob);

	bool operator==(const MobEffectInstance& rhs) const {
		return mId == rhs.mId && mDuration == rhs.mDuration && mAmplifier == rhs.mAmplifier;
	}
	bool operator!=(const MobEffectInstance& rhs) const {
		return !(*this == rhs);
	}

private:
	int mId = 0;
	int mDuration = 0;
	int mAmplifier = 0;
	bool mAmbient = false;
	bool mEffectVisible = true;
};
