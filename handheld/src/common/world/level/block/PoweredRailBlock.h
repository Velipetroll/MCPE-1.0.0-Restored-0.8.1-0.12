/********************************************************
*  (c) Mojang.    All rights reserved.                  *
*  (c) Microsoft. All rights reserved.                  *
*********************************************************/
#pragma once

#include "world/level/block/BaseRailBlock.h"

class PoweredRailBlock : public BaseRailBlock {
public:
	PoweredRailBlock(const std::string& nameId, int id);
	
	virtual int getVariant(int data) const override;
 	virtual void onRedstoneUpdate(BlockSource& region, const BlockPos& pos, int strength, bool isFirstTime) const override;

protected:
	virtual void updateState(BlockSource& region, const BlockPos& pos, int data, int dir, int type) const override;

private:
	bool _findPoweredRailSignal(BlockSource& region, const BlockPos& pos, int data, bool forward, int searchDepth) const;
	bool _isSameRailWithPower(BlockSource& region, const BlockPos& pos, bool forward, int searchDepth, int dir) const;
};
