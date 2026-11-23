/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include "world/attribute/AttributeInstanceDelegate.h"

class Player;

class HungerAttributeDelegate : public AttributeInstanceDelegate {
public:
	HungerAttributeDelegate(const AttributeInstance& instance, Player* player);

	void tick() override;
	void notify(int64_t type) override;

private:
	int mHealthTickCount = 0;
	int mTickCounter = 0;
	float mLastHunger = 0.0f;
	Player* mPlayer = nullptr;
};
