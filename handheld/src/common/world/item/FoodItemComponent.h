/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include <string>
#include <vector>

namespace Json {
	class Value;
}

class BlockPos;
class Entity;
class Item;
class ItemInstance;
class Level;
class Player;
class Vec3;

class FoodItemComponent {
public:
	struct Effect {
		std::string descriptionId;
		int id = 0;
		int durationTicks = 0;
		int amplifier = 0;
		float chance = 1.0f;
	};

	explicit FoodItemComponent(Item& owner);

	bool init(Json::Value& data);

	int getNutrition() const { return mNutrition; }
	float getSaturationModifier() const { return mSaturationModifier; }
	bool isMeat() const { return mIsMeat; }
	const std::string& getEatSound() const { return mEatSound; }
	const std::string& getUsingConvertsTo() const { return mUsingConvertsTo; }
	const std::vector<Effect>& getEffects() const { return mEffects; }

	void use(ItemInstance& instance, Player& player);
	bool useOn(ItemInstance& instance, Entity& entity, const BlockPos& pos, signed char face, const Vec3& clickPos);
	Item* useTimeDepleted(ItemInstance& instance, Player& player, Level& level);

private:
	void _loadEffects(std::vector<Effect>& effects, Json::Value& data);
	void _applyEatEffects(ItemInstance& instance, Player& player, Level& level);

	Item& mOwner;
	int mNutrition = 0;
	float mSaturationModifier = 1.0f;
	bool mIsMeat = false;
	std::string mEatSound = "random.burp";
	std::string mUsingConvertsTo;
	std::vector<Effect> mEffects;
};
