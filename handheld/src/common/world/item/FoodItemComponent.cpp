/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/item/FoodItemComponent.h"
#include "world/effect/MobEffect.h"
#include "world/effect/MobEffectInstance.h"
#include "world/entity/player/Player.h"
#include "world/food/FoodConstants.h"
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "world/level/Level.h"

FoodItemComponent::FoodItemComponent(Item& owner)
	: mOwner(owner) {}

bool FoodItemComponent::init(Json::Value& data) {
	if (!data.isObject()) {
		return false;
	}
	mNutrition = data["nutrition"].asInt(0);
	Json::Value& sat = data["saturation_modifier"];
	if (sat.isString()) {
		mSaturationModifier = FoodSaturationFromString(sat.asString(""));
	} else {
		mSaturationModifier = sat.asFloat(FoodConstants::FOOD_SATURATION_NORMAL);
	}
	mIsMeat = data["is_meat"].asBool(false);
	mUsingConvertsTo = data["using_converts_to"].asString("");
	mEatSound = data["eat_sound"].asString("random.burp");
	_loadEffects(mEffects, data["effects"]);
	return true;
}

void FoodItemComponent::_loadEffects(std::vector<Effect>& effects, Json::Value& data) {
	if (!data.isArray()) {
		return;
	}
	for (Json::ArrayIndex i = 0; i < data.size(); ++i) {
		Json::Value& entry = data[i];
		if (!entry.isObject()) {
			continue;
		}
		std::string name = Util::toLower(entry["name"].asString(""));
		if (MobEffect* eff = MobEffect::getByName(name)) {
			Effect e;
			e.descriptionId = eff->getDescriptionId();
			e.id = eff->getId();
			e.durationTicks = entry["duration"].asInt(1) * SharedConstants::TicksPerSecond;
			e.amplifier = entry["amplifier"].asInt(1);
			e.chance = entry["chance"].asFloat(1.0f);
			effects.push_back(std::move(e));
		}
	}
}

void FoodItemComponent::use(ItemInstance& instance, Player& player) {
	const bool isGoldApple = (Item::mApple_gold && instance.getId() == Item::mApple_gold->getId()) ||
	                         (Item::mApple_enchanted && instance.getId() == Item::mApple_enchanted->getId());
	if (!player.mAbilities.mInstabuild && (player.isHungry() || isGoldApple)) {
		player.startUsingItem(instance, instance.getMaxUseDuration());
	}
}

bool FoodItemComponent::useOn(ItemInstance& instance, Entity& entity, const BlockPos& pos, signed char face, const Vec3& clickPos) {
	UNUSED_PARAMETER(instance, entity, pos, face, clickPos);
	return false;
}

Item* FoodItemComponent::useTimeDepleted(ItemInstance& instance, Player& player, Level& level) {
	_applyEatEffects(instance, player, level);
	player.eat(instance);
	player.useItem(instance);

	if (!mUsingConvertsTo.empty()) {
		return Item::lookupByName(mUsingConvertsTo, true);
	}
	return nullptr;
}

void FoodItemComponent::_applyEatEffects(ItemInstance& instance, Player& player, Level& level) {
	UNUSED_PARAMETER(instance);
	if (level.isClientSide()) {
		return;
	}
	for (const Effect& e : mEffects) {
		if (e.chance == 1.0f || level.getRandom().nextFloat() <= e.chance) {
			MobEffectInstance inst(e.id, e.durationTicks, e.amplifier);
			player.addEffect(inst);
		}
	}
}
