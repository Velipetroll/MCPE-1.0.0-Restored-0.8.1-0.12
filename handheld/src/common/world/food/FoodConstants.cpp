/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/food/FoodConstants.h"

namespace FoodConstants {
	const int MAX_FOOD = 20;
	const int HEALTH_TICK_COUNT = 80;
	const int HEAL_LEVEL = 18;
	const float MAX_SATURATION = 20.0f;
	const float START_SATURATION = 5.0f;
	const float FOOD_SPRINT_LEVEL = 6.0f;
	const float MAX_EXHAUSTION = 4.0f;
	const int STARVE_LEVEL = 0;

	const float FOOD_SATURATION_POOR = 0.1f;
	const float FOOD_SATURATION_LOW = 0.3f;
	const float FOOD_SATURATION_NORMAL = 0.6f;
	const float FOOD_SATURATION_GOOD = 0.8f;
	const float FOOD_SATURATION_MAX = 1.0f;
	const float FOOD_SATURATION_SUPERNATURAL = 1.2f;

	const float EXHAUSTION_HEAL = 3.0f;
	const float EXHAUSTION_JUMP = 0.2f;
	const float EXHAUSTION_SPRINT_JUMP = 0.8f;
	const float EXHAUSTION_MINE = 0.025f;
	const float EXHAUSTION_ATTACK = 0.3f;
	const float EXHAUSTION_DAMAGE = 0.1f;
	const float EXHAUSTION_WALK = 0.01f;
	const float EXHAUSTION_SPRINT = 0.099999994f;
	const float EXHAUSTION_SWIM = 0.015f;
}

float FoodSaturationFromString(const std::string& str) {
	if (str.empty()) {
		return FoodConstants::FOOD_SATURATION_NORMAL;
	}

	static std::unordered_map<std::string, float> LOOKUP_MAP;
	if (LOOKUP_MAP.empty()) {
		LOOKUP_MAP["poor"] = FoodConstants::FOOD_SATURATION_POOR;
		LOOKUP_MAP["low"] = FoodConstants::FOOD_SATURATION_LOW;
		LOOKUP_MAP["normal"] = FoodConstants::FOOD_SATURATION_NORMAL;
		LOOKUP_MAP["good"] = FoodConstants::FOOD_SATURATION_GOOD;
		LOOKUP_MAP["max"] = FoodConstants::FOOD_SATURATION_MAX;
		LOOKUP_MAP["supernatural"] = FoodConstants::FOOD_SATURATION_SUPERNATURAL;
	}

	auto it = LOOKUP_MAP.find(Util::toLower(str));
	return (it != LOOKUP_MAP.end()) ? it->second : FoodConstants::FOOD_SATURATION_NORMAL;
}
