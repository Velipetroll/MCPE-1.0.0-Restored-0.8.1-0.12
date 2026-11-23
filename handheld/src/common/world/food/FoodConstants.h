/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include <string>

namespace FoodConstants {
	extern const int MAX_FOOD;
	extern const int HEALTH_TICK_COUNT;
	extern const int HEAL_LEVEL;
	extern const float MAX_SATURATION;
	extern const float START_SATURATION;
	extern const float FOOD_SPRINT_LEVEL;
	extern const float MAX_EXHAUSTION;
	extern const int STARVE_LEVEL;

	extern const float FOOD_SATURATION_POOR;
	extern const float FOOD_SATURATION_LOW;
	extern const float FOOD_SATURATION_NORMAL;
	extern const float FOOD_SATURATION_GOOD;
	extern const float FOOD_SATURATION_MAX;
	extern const float FOOD_SATURATION_SUPERNATURAL;

	extern const float EXHAUSTION_HEAL;
	extern const float EXHAUSTION_JUMP;
	extern const float EXHAUSTION_SPRINT_JUMP;
	extern const float EXHAUSTION_MINE;
	extern const float EXHAUSTION_ATTACK;
	extern const float EXHAUSTION_DAMAGE;
	extern const float EXHAUSTION_WALK;
	extern const float EXHAUSTION_SPRINT;
	extern const float EXHAUSTION_SWIM;
}

float FoodSaturationFromString(const std::string& str);
