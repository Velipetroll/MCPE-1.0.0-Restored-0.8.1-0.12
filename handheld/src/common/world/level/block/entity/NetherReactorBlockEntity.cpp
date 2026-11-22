#include "common_header.h"
#include "world/level/block/entity/NetherReactorBlockEntity.h"
#include "world/level/block/NetherReactor.h"
#include "world/level/block/NetherReactorPattern.h"
#include "world/level/block/Block.h"
#include "world/level/BlockSource.h"
#include "world/level/Level.h"
#include "world/item/Item.h"
#include "nbt/CompoundTag.h"
#include <cmath>

namespace {
NetherReactorBlockEntity::SpawnPigZombieCallback gSpawnPigZombieCb;
NetherReactorBlockEntity::KillPigZombiesCallback gKillPigZombiesCb;
NetherReactorBlockEntity::SetNightModeCallback gSetNightModeCb;
}

void NetherReactorBlockEntity::setCallbacks(SpawnPigZombieCallback spawnEnemyCb,
                                            KillPigZombiesCallback killEnemiesCb,
                                            SetNightModeCallback nightModeCb) {
	gSpawnPigZombieCb = std::move(spawnEnemyCb);
	gKillPigZombiesCb = std::move(killEnemiesCb);
	gSetNightModeCb = std::move(nightModeCb);
}

NetherReactorBlockEntity::NetherReactorBlockEntity(const BlockPos& pos)
	: BlockEntity(BlockEntityType::NetherReactor, pos, "NetherReactor") {
}

void NetherReactorBlockEntity::load(const CompoundTag& tag) {
	BlockEntity::load(tag);
	mIsInitialized = tag.getByte("IsInitialized") != 0;
	if (mIsInitialized) {
		mProgress = tag.getShort("Progress");
		mHasFinished = tag.getByte("HasFinished") != 0;
	}
}

bool NetherReactorBlockEntity::save(CompoundTag& tag) {
	if (!BlockEntity::save(tag)) {
		return false;
	}
	tag.putByte("IsInitialized", mIsInitialized ? 1 : 0);
	tag.putShort("Progress", mProgress);
	tag.putByte("HasFinished", mHasFinished ? 1 : 0);
	return true;
}

bool NetherReactorBlockEntity::_isEdge(int x, int y, int z) const {
	return x == -y || x == y || z == -y || z == y;
}

bool NetherReactorBlockEntity::_checkLevelChange(int sec) const {
	static const int kThresholds[10] = { 10, 13, 20, 22, 25, 30, 34, 36, 38, 40 };
	for (int t : kThresholds) {
		if (t == sec) return true;
	}
	return false;
}

int NetherReactorBlockEntity::_getNumItemsPerLevel(int level) const {
	if (level == 1) return 9;
	if (level <= 4) return 15;
	if (level <= 8) return 9;
	return 6;
}

int NetherReactorBlockEntity::_getNumEnemiesPerLevel(int level) const {
	if (level <= 2) return 2;
	if (level <= 6) return 3;
	return 1;
}

void NetherReactorBlockEntity::_buildFloorVolume(BlockSource& region, int x, int y, int z, int r, int h, BlockID id) {
	for (int yo = 0; yo < h; ++yo) {
		for (int xo = -r; xo <= r; ++xo) {
			for (int zo = -r; zo <= r; ++zo) {
				region.setBlock(x + xo, y + yo, z + zo, id, Block::UPDATE_ALL);
			}
		}
	}
}

void NetherReactorBlockEntity::_buildHollowedVolume(BlockSource& region, int x, int y, int z, int r, int h, BlockID wallId, BlockID innerId) {
	for (int yo = 0; yo < h; ++yo) {
		for (int xo = -r; xo <= r; ++xo) {
			for (int zo = -r; zo <= r; ++zo) {
				if (xo == -r || xo == r || zo == -r || zo == r) {
					region.setBlock(x + xo, y + yo, z + zo, wallId, Block::UPDATE_ALL);
				} else if (yo > 2 || xo < -1 || xo > 1 || zo < -1 || zo > 1) {
					region.setBlock(x + xo, y + yo, z + zo, innerId, Block::UPDATE_ALL);
				}
			}
		}
	}
}

void NetherReactorBlockEntity::_buildCrockedRoofVolume(BlockSource& region, bool invert, int x, int y, int z, int r, int h, BlockID id) {
	for (int i = -r; i <= r; ++i) {
		for (int j = -r; j <= r; ++j) {
			int v10 = invert ? (-i - j) : (j + i);
			int limit = v10 / 2 + h + r;
			for (int k = 0; k < h + r + r; ++k) {
				if (limit >= k && (_isEdge(i, r, j) || limit == k)) {
					region.setBlock(i + x, k + y, j + z, id, Block::UPDATE_ALL);
				}
			}
		}
	}
}

void NetherReactorBlockEntity::_buildDome(BlockSource& region, int x, int y, int z) {
	BlockID netherrack = Block::mNetherrack->mID;
	_buildFloorVolume(region, x, y - 3, z, 8, 2, netherrack);
	_buildHollowedVolume(region, x, y - 1, z, 8, 4, netherrack, BlockID::AIR);
	_buildFloorVolume(region, x, y + 3, z, 8, 1, netherrack);
	_buildCrockedRoofVolume(region, false, x, y + 4, z, 8, 1, netherrack);
	_buildCrockedRoofVolume(region, true, x, y + 5, z, 5, 8, netherrack);
	_buildCrockedRoofVolume(region, false, x, y + 11, z, 3, 14, netherrack);
}

void NetherReactorBlockEntity::_deterioateHollowedVolume(BlockSource& region, int x, int y, int z, int r, int h, BlockID id) {
	auto& rng = region.getLevel().getRandom();
	for (int yo = 0; yo < h; ++yo) {
		for (int i = -r; i <= r; ++i) {
			for (int j = -r; j <= r; ++j) {
				if ((i == -r || i == r || j == -r || j == r) && rng.nextInt(3) == 0) {
					region.setBlock(i + x, y + yo, j + z, id, Block::UPDATE_ALL);
				}
			}
		}
	}
}

void NetherReactorBlockEntity::_deterioateCrockedRoofVolume(BlockSource& region, bool invert, int x, int y, int z, int r, int h, BlockID id) {
	auto& rng = region.getLevel().getRandom();
	for (int i = -r; i <= r; ++i) {
		for (int j = -r; j <= r; ++j) {
			int v10 = invert ? (-i - j) : (j + i);
			int limit = v10 / 2 + h + r;
			for (int k = 0; k < h + r + r; ++k) {
				if (limit >= k && _isEdge(i, r, j) && rng.nextInt(4) == 0) {
					region.setBlock(i + x, k + y, j + z, id, Block::UPDATE_ALL);
				}
			}
		}
	}
}

void NetherReactorBlockEntity::_deterioateDome(BlockSource& region, int x, int y, int z) {
	_deterioateHollowedVolume(region, x, y - 1, z, 8, 5, BlockID::AIR);
	_deterioateCrockedRoofVolume(region, false, x, y + 4, z, 8, 1, BlockID::AIR);
	_deterioateCrockedRoofVolume(region, true, x, y + 5, z, 5, 8, BlockID::AIR);
	_deterioateCrockedRoofVolume(region, false, x, y + 11, z, 3, 14, BlockID::AIR);
}

void NetherReactorBlockEntity::_turnLayerToGlowingObsidian(BlockSource& region, int layer, BlockID matchId) {
	NetherReactorPattern pat;
	for (int i = -1; i <= 1; ++i) {
		for (int j = -1; j <= 1; ++j) {
			if (pat.getBlockAt(layer, i + 1, j + 1) == matchId) {
				region.setBlock(mPosition.x + i, mPosition.y - 1 + layer, mPosition.z + j, Block::mGlowingObsidian->mID, Block::UPDATE_ALL);
			}
		}
	}
}

void NetherReactorBlockEntity::_turnGlowingObsidianLayerToObsidian(BlockSource& region, int layer) {
	for (int i = -1; i <= 1; ++i) {
		for (int j = -1; j <= 1; ++j) {
			BlockPos p(mPosition.x + i, mPosition.y - 1 + layer, mPosition.z + j);
			if (region.getBlockID(p) != Block::mNetherReactor->mID) {
				region.setBlock(p.x, p.y, p.z, Block::mObsidian->mID, Block::UPDATE_ALL);
			}
		}
	}
}

void NetherReactorBlockEntity::_tickGlowingRedstoneTransformation(BlockSource& region, int sec) {
	switch (sec) {
	case 2: _turnLayerToGlowingObsidian(region, 0, Block::mCobblestone->mID); break;
	case 3: _turnLayerToGlowingObsidian(region, 1, Block::mCobblestone->mID); break;
	case 4: _turnLayerToGlowingObsidian(region, 2, Block::mCobblestone->mID); break;
	case 7: _turnLayerToGlowingObsidian(region, 0, Block::mGoldBlock->mID); break;
	case 8: _turnLayerToGlowingObsidian(region, 1, Block::mGoldBlock->mID); break;
	case 9: _turnLayerToGlowingObsidian(region, 2, Block::mGoldBlock->mID); break;
	default: break;
	}
}

ItemInstance NetherReactorBlockEntity::_getLowOddsSpawnItem(Random& random) const {
	const Item* items[] = {
		Item::mArrow, Item::mBed, Item::mBone, Item::mBow,
		Item::mBowl, Item::mFeather, Item::mPainting, Item::mDoor_wood
	};
	return ItemInstance(items[random.nextInt(8)]);
}

ItemInstance NetherReactorBlockEntity::_getSpawnItem(Random& random) const {
	switch (random.nextInt(9)) {
	case 0: return ItemInstance(Item::mYellowDust, 3);
	case 1: return ItemInstance(Item::mSeeds_melon);
	case 2: return ItemInstance(Block::mBrownMushroom);
	case 3: return ItemInstance(Block::mRedMushroom);
	case 4: return ItemInstance(Item::mReeds);
	case 5: return ItemInstance(Block::mCactus);
	case 6: return ItemInstance(Item::mNetherQuartz, 4);
	case 7: return ItemInstance(Item::mSeeds_pumpkin);
	default: return _getLowOddsSpawnItem(random);
	}
}

Vec3 NetherReactorBlockEntity::_getSpawnPosition(Random& random, float minDist, float spanDist, float yOff) const {
	float dist = minDist + random.nextFloat() * spanDist;
	float angle = random.nextFloat() * 6.2831853f;
	float x = static_cast<float>(mPosition.x) + std::sin(angle) * dist;
	float y = static_cast<float>(mPosition.y) + yOff;
	float z = static_cast<float>(mPosition.z) + std::cos(angle) * dist;
	return Vec3(x, y, z);
}

void NetherReactorBlockEntity::_spawnItems(BlockSource& region, int count) {
	auto& rng = region.getLevel().getRandom();
	for (int i = 0; i < count; ++i) {
		Vec3 sp = _getSpawnPosition(rng, 3.0f, 4.0f, -1.0f);
		BlockPos bp(static_cast<int>(std::floor(sp.x)), static_cast<int>(std::floor(sp.y)), static_cast<int>(std::floor(sp.z)));
		if (Block::mNetherReactor) {
			Block::mNetherReactor->popResource(region, bp, _getSpawnItem(rng));
		}
	}
}

void NetherReactorBlockEntity::_trySpawnPigZombies(BlockSource& region, int maxEnemies) {
	if (!gSpawnPigZombieCb) return;
	auto& rng = region.getLevel().getRandom();
	for (int i = 0; i < maxEnemies; ++i) {
		Vec3 sp = _getSpawnPosition(rng, 3.0f, 4.0f, -1.0f);
		gSpawnPigZombieCb(sp);
	}
}

void NetherReactorBlockEntity::lightItUp(BlockSource& region, int x, int y, int z) {
	mCurLevel = 0;
	mProgress = 0;
	mHasFinished = false;
	NetherReactorBlock::setPhase(region, BlockPos(x, y, z), 1);
	mIsInitialized = true;
	_buildDome(region, x, y, z);
	if (gSetNightModeCb) gSetNightModeCb(true);
	setChanged();
}

void NetherReactorBlockEntity::finishReactorRun(BlockSource& region) {
	NetherReactorBlock::setPhase(region, mPosition, 2);
	if (gSetNightModeCb) gSetNightModeCb(false);
	_deterioateDome(region, mPosition.x, mPosition.y, mPosition.z);
	mHasFinished = true;
	setChanged();
}

void NetherReactorBlockEntity::tick(BlockSource& region) {
	BlockEntity::tick(region);
	if (region.getLevel().isClientSide()) {
		return;
	}
	if (!mIsInitialized || mHasFinished) {
		if (mHasFinished && (mProgress % 20 == 0)) {
			// Keep finished state peaceful if players are far
			if (gKillPigZombiesCb) {
				gKillPigZombiesCb(mPosition, 40.0f);
			}
		}
		return;
	}
	++mProgress;
	if ((mProgress % 20) == 0) {
		int sec = mProgress / 20;
		if (sec <= 9) {
			_tickGlowingRedstoneTransformation(region, sec);
		}
		if (sec >= 43 && sec <= 45) {
			_turnGlowingObsidianLayerToObsidian(region, 45 - sec);
		}
		if (_checkLevelChange(sec)) {
			++mCurLevel;
			_spawnItems(region, _getNumItemsPerLevel(mCurLevel));
			_trySpawnPigZombies(region, _getNumEnemiesPerLevel(mCurLevel));
		}
	}
	if (mProgress > 920) {
		finishReactorRun(region);
	}
}
