/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include "world/attribute/AttributeInstanceDelegate.h"

class Mob;

class HealthAttributeDelegate : public AttributeInstanceDelegate {
public:
	HealthAttributeDelegate(const AttributeInstance& instance, Mob* mob);

	void tick() override;
	bool change(float oldValue, float newValue, const AttributeBuffInfo& buffInfo) override;
	float getBuffValue(const AttributeBuff& buff) const override;

private:
	int mTickCounter = 0;
	Mob* mMob = nullptr;
};
