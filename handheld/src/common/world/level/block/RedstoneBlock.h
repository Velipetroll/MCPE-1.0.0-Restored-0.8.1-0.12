#pragma once

#include "Block.h"


class RedstoneBlock : public Block {

public:
	RedstoneBlock(const std::string& nameId, int id);

	virtual bool isSignalSource() const override;
	virtual int getSignal(BlockSource& region, const BlockPos& pos, int dir) const override;
	virtual bool shouldConnectToRedstone(BlockSource& region, const BlockPos& pos, int direction) const override;
	virtual void onLoaded(BlockSource& region, const BlockPos& pos) const override;
	virtual void onPlace(BlockSource& region, const BlockPos& pos) const override;
	virtual void onRemove(BlockSource& region, const BlockPos& pos) const override;
 
private:
};
 