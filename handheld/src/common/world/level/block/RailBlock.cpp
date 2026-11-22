/********************************************************
*  (c) Mojang.    All rights reserved.                  *
*  (c) Microsoft. All rights reserved.                  *
*********************************************************/
#include "common_header.h"

#include "world/level/block/RailBlock.h"

RailBlock::RailBlock(const std::string& nameId, int id)
	: BaseRailBlock(nameId, id, false) {
}

int RailBlock::getVariant(int data) const {
	int railDir = getBlockState(BlockState::RailDirection).get<int>(data);
	bool dataBit = getBlockState(BlockState::RailDataBit).isInitialized() ? getBlockState(BlockState::RailDataBit).getBool(data) : false;
	return (dataBit || railDir >= 6) ? 1 : 0;
}

void RailBlock::updateState(BlockSource& region, const BlockPos& pos, int data, int dir, int type) const {
	UNUSED_PARAMETER(data, dir);
	if (type > 0 && Block::mBlocks[type] && Block::mBlocks[type]->isSignalSource()) {
		if (BaseRailBlock::Rail(region, pos).countPotentialConnections() == 3) {
			updateDir(region, pos, false);
		}
	}
}
