/********************************************************
*  (c) Mojang.    All rights reserved.                  *
*  (c) Microsoft. All rights reserved.                  *
*********************************************************/
#pragma once

#include "world/level/block/BaseRailBlock.h"

class RailBlock : public BaseRailBlock {
public:
	RailBlock(const std::string& nameId, int id);
	virtual int getVariant(int data) const override;

protected:
	virtual void updateState(BlockSource& region, const BlockPos& pos, int data, int dir, int type) const override;
};
