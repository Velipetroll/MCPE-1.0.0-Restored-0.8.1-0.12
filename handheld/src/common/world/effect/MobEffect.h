/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include <memory>
#include <string>

class Mob;
class MobEffectInstance;

class MobEffect {
public:
	static const int NUM_EFFECTS = 32;

	static MobEffect* mMobEffects[NUM_EFFECTS];

	static MobEffect* MOVEMENT_SPEED;
	static MobEffect* MOVEMENT_SLOWDOWN;
	static MobEffect* DIG_SPEED;
	static MobEffect* DIG_SLOWDOWN;
	static MobEffect* DAMAGE_BOOST;
	static MobEffect* HEAL;
	static MobEffect* HARM;
	static MobEffect* JUMP;
	static MobEffect* CONFUSION;
	static MobEffect* REGENERATION;
	static MobEffect* DAMAGE_RESISTANCE;
	static MobEffect* FIRE_RESISTANCE;
	static MobEffect* WATER_BREATHING;
	static MobEffect* INVISIBILITY;
	static MobEffect* BLINDNESS;
	static MobEffect* NIGHT_VISION;
	static MobEffect* HUNGER;
	static MobEffect* WEAKNESS;
	static MobEffect* POISON;
	static MobEffect* WITHER;
	static MobEffect* HEALTH_BOOST;
	static MobEffect* ABSORPTION;
	static MobEffect* SATURATION;
	static MobEffect* LEVITATION;

	static void initEffects();
	static void teardownEffects();
	static MobEffect* getByName(const std::string& name);
	static MobEffect* getById(int id);

	MobEffect(int id, const std::string& resourceName, const std::string& locName, bool isHarmful, int color, int icon);
	virtual ~MobEffect() = default;

	int getId() const { return mId; }
	const std::string& getResourceName() const { return mResourceName; }
	const std::string& getDescriptionId() const { return mDescriptionId; }
	bool isHarmful() const { return mIsHarmful; }
	int getColor() const { return mColor; }
	bool isDisabled() const { return mDisabled; }
	virtual bool isInstantaneous() const { return false; }

	virtual void applyEffects(Mob* mob, int durationTicks, int amplifier) const;
	virtual void removeEffects(Mob* mob) const;

protected:
	int mId = 0;
	bool mIsHarmful = false;
	int mColor = 0;
	std::string mDescriptionId;
	std::string mResourceName;
	int mIcon = 0;
	bool mDisabled = false;
};
