/********************************************************
*  (c) Mojang.    All rights reserved.                  *
*  (c) Microsoft. All rights reserved.                  *
*********************************************************/
#include "common_header.h"

#include "world/level/block/NetherReactor.h"
#include "world/level/block/NetherReactorPattern.h"
#include "world/level/Level.h"
#include "world/level/block/entity/NetherReactorBlockEntity.h"
#include "world/level/BlockSource.h"
#include "world/entity/player/Player.h"
#include "world/item/Item.h"

NetherReactorBlock::NetherReactorBlock(const std::string& nameId, int id, const Material& material)
	: EntityBlock(nameId, id, material) {
	mBlockEntityType = BlockEntityType::NetherReactor;
}

void NetherReactorBlock::spawnResources(BlockSource& region, const BlockPos& pos, int data, float odds, int bonusLootLevel) const {
	if (region.getLevel().isClientSide()) {
		return;
	}

	for (int i = 0; i < 6; i++) {
		_dropItem(region, pos, Item::mIronIngot);
	}

	for (int i = 0; i < 3; i++) {
		_dropItem(region, pos, Item::mDiamond);
	}
}

void NetherReactorBlock::_dropItem(BlockSource& region, const BlockPos& pos, Item* item) const {
	if (item) {
		popResource(region, pos, ItemInstance(item, 1));
	}
}

void NetherReactorBlock::setPhase(BlockSource& region, const BlockPos& pos, int phase) {
	if (region.getData(pos) != phase) {
		region.setBlockAndData(pos, Block::mNetherReactor->mID, static_cast<DataID>(phase), Block::UPDATE_CLIENTS);
	}
}

bool NetherReactorBlock::activateReactor(BlockSource& region, const BlockPos& pos, const Vec3& playerPos, bool isCreative, std::string& outMessage) {
	if (isCreative) {
		return false;
	}
	NetherReactorPattern pat;
	for (int yy = 0; yy < 3; ++yy) {
		for (int xx = 0; xx < 3; ++xx) {
			for (int zz = 0; zz < 3; ++zz) {
				BlockPos checkPos(pos.x - 1 + xx, pos.y - 1 + yy, pos.z - 1 + zz);
				if (static_cast<unsigned int>(region.getBlockID(checkPos)) != pat.getBlockAt(yy, xx, zz)) {
					outMessage = "Not the correct pattern!";
					return false;
				}
			}
		}
	}
	if (playerPos.x < pos.x - 5 || playerPos.x > pos.x + 5 ||
	    playerPos.y < pos.y - 1 || playerPos.y > pos.y + 1 ||
	    playerPos.z < pos.z - 5 || playerPos.z > pos.z + 5) {
		outMessage = "All players need to be close to the reactor.";
		return false;
	}
	if (pos.y > 100) {
		outMessage = "The nether reactor needs to be built lower down.";
		return false;
	}
	if (pos.y <= 1) {
		outMessage = "The nether reactor needs to be built higher up.";
		return false;
	}
	outMessage = "Active!";
	BlockEntity* be = region.getBlockEntity(pos);
	if (be && be->isType(BlockEntityType::NetherReactor)) {
		static_cast<NetherReactorBlockEntity*>(be)->lightItUp(region, pos.x, pos.y, pos.z);
	}
	return true;
}

bool NetherReactorBlock::use(Player& player, const BlockPos& pos) const {
	std::string msg;
	bool isCreative = player.getLevel().getLevelData().getGameType() == GameType::Creative;
	return activateReactor(player.getRegion(), pos, player.getPos(), isCreative, msg);
}
