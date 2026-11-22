#pragma once

#include "world/level/block/entity/BlockEntity.h"
#include "world/item/ItemInstance.h"
#include <functional>

class NetherReactorBlockEntity : public BlockEntity {
public:
	bool mIsInitialized = false;
	bool mHasFinished = false;
	int mCurLevel = 0;
	int16_t mProgress = 0;

	using SpawnPigZombieCallback = std::function<void(const Vec3&)>;
	using KillPigZombiesCallback = std::function<void(const BlockPos&, float)>;
	using SetNightModeCallback = std::function<void(bool)>;

	static void setCallbacks(SpawnPigZombieCallback spawnEnemyCb,
	                         KillPigZombiesCallback killEnemiesCb,
	                         SetNightModeCallback nightModeCb);

	explicit NetherReactorBlockEntity(const BlockPos& pos);
	~NetherReactorBlockEntity() override = default;

	void load(const CompoundTag& tag) override;
	bool save(CompoundTag& tag) override;
	void tick(BlockSource& region) override;

	void lightItUp(BlockSource& region, int x, int y, int z);
	void finishReactorRun(BlockSource& region);

private:
	void _buildDome(BlockSource& region, int x, int y, int z);
	void _buildFloorVolume(BlockSource& region, int x, int y, int z, int r, int h, BlockID id);
	void _buildHollowedVolume(BlockSource& region, int x, int y, int z, int r, int h, BlockID wallId, BlockID innerId);
	void _buildCrockedRoofVolume(BlockSource& region, bool invert, int x, int y, int z, int r, int h, BlockID id);
	void _deterioateDome(BlockSource& region, int x, int y, int z);
	void _deterioateHollowedVolume(BlockSource& region, int x, int y, int z, int r, int h, BlockID id);
	void _deterioateCrockedRoofVolume(BlockSource& region, bool invert, int x, int y, int z, int r, int h, BlockID id);
	bool _isEdge(int x, int y, int z) const;
	bool _checkLevelChange(int sec) const;
	int _getNumItemsPerLevel(int level) const;
	int _getNumEnemiesPerLevel(int level) const;
	void _tickGlowingRedstoneTransformation(BlockSource& region, int sec);
	void _turnLayerToGlowingObsidian(BlockSource& region, int layer, BlockID matchId);
	void _turnGlowingObsidianLayerToObsidian(BlockSource& region, int layer);
	ItemInstance _getSpawnItem(Random& random) const;
	ItemInstance _getLowOddsSpawnItem(Random& random) const;
	Vec3 _getSpawnPosition(Random& random, float minDist, float spanDist, float yOff) const;
	void _spawnItems(BlockSource& region, int count);
	void _trySpawnPigZombies(BlockSource& region, int maxEnemies);
};
