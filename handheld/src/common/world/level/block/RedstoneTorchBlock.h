#pragma once

#include "world/level/block/TorchBlock.h"

class Level;
class TileSource;
class Item;
class Random;

class RedstoneTorchBlock : public TorchBlock {
public:
	RedstoneTorchBlock(const std::string& nameId, int id, bool on);

	virtual void onLoaded(BlockSource& region, const BlockPos& pos) const override;

	virtual void onPlace(BlockSource& region, const BlockPos& pos) const override;
	virtual void onRemove(BlockSource& region, const BlockPos& pos) const override;
	virtual void neighborChanged(BlockSource& region, const BlockPos& pos, const BlockPos& neighborPos) const override;
	virtual void onRedstoneUpdate(BlockSource& region, const BlockPos& pos, int strength, bool isFirstTime) const override;

	virtual bool isSignalSource() const override;
	virtual int getSignal(BlockSource& region, const BlockPos& pos, int dir) const override;
	virtual int getDirectSignal(BlockSource& region, const BlockPos& pos, int dir) const override;

	virtual void tick(BlockSource& region, const BlockPos& pos, Random& random) const override;
	virtual void animateTick(BlockSource& region, const BlockPos& pos, Random& random) const override;

	virtual int getTickDelay() const /*override*/;

	virtual bool shouldConnectToRedstone(BlockSource& region, const BlockPos& pos, int direction) const override;

	virtual ItemInstance asItemInstance(BlockSource& region, const BlockPos& pos, int blockData) const override;
	virtual ItemInstance getSilkTouchItemInstance(DataID data) const override;
	virtual int getResource(Random& random, int data, int bonusLootLevel = 0) const override;

	void installCircuit(BlockSource &source, const BlockPos& pos, bool bLoading) const;

private:
	bool _isAttachedPowered(BlockSource& region, const BlockPos& pos) const;
	bool mOn = false;
};