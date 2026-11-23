/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/item/ChorusFruit.h"
#include "world/entity/player/Player.h"
#include "world/food/FoodConstants.h"
#include "world/item/ItemInstance.h"
#include "world/item/UseAnim.h"
#include "world/level/BlockPos.h"
#include "world/level/BlockSource.h"
#include "world/level/Level.h"
#include "world/level/block/Block.h"
#include "world/level/material/Material.h"

ChorusFruit::ChorusFruit(const std::string& nameId, short id)
	: Item(nameId, id) {
	setUseAnimation(UseAnimation::Eat);
	setMaxUseDuration(32);
}

ItemInstance& ChorusFruit::use(ItemInstance& instance, Player& player) {
	player.startUsingItem(instance, instance.getMaxUseDuration());
	return instance;
}

void ChorusFruit::useTimeDepleted(ItemInstance* inoutInstance, Level* level, Player* player) {
	if (!inoutInstance || !level || !player) {
		return;
	}

	player->eat(4, FoodConstants::FOOD_SATURATION_LOW);
	player->useItem(*inoutInstance);

	if (!level->isClientSide()) {
		Random& rng = level->getRandom();
		BlockSource& region = player->getRegion();
		const Vec3 orig = player->mPos;

		for (int attempt = 0; attempt < 16; ++attempt) {
			float tx = orig.x + (rng.nextFloat() - 0.5f) * 16.0f;
			float ty = orig.y + static_cast<float>(rng.nextInt(16) - 8);
			float tz = orig.z + (rng.nextFloat() - 0.5f) * 16.0f;
			if (ty < 1.0f) ty = 1.0f;
			if (ty > 250.0f) ty = 250.0f;

			int bx = Math::floor(tx);
			int by = Math::floor(ty);
			int bz = Math::floor(tz);

			while (by > 1 && !region.getMaterial(BlockPos(bx, by - 1, bz)).getBlocksMotion()) {
				--by;
			}
			if (by <= 1) {
				continue;
			}

			const Material& feetMat = region.getMaterial(BlockPos(bx, by, bz));
			const Material& headMat = region.getMaterial(BlockPos(bx, by + 1, bz));
			if (!feetMat.getBlocksMotion() && !feetMat.isLiquid() &&
			    !headMat.getBlocksMotion() && !headMat.isLiquid()) {
				player->teleportTo(Vec3(static_cast<float>(bx) + 0.5f, static_cast<float>(by) + player->mHeightOffset, static_cast<float>(bz) + 0.5f));
				break;
			}
		}
	}
}
