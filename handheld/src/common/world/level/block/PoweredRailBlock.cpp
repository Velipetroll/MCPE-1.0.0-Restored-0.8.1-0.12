/********************************************************
*  (c) Mojang.    All rights reserved.                  *
*  (c) Microsoft. All rights reserved.                  *
*********************************************************/
#include "common_header.h"

#include "world/level/block/PoweredRailBlock.h"

#include "world/level/BlockSource.h"
#include "world/level/Level.h"
#include "world/level/dimension/Dimension.h"
// #include "world/redstone/circuit/CircuitSystem.h"
// #include "world/redstone/circuit/components/BaseRailTransporter.h"

PoweredRailBlock::PoweredRailBlock(const std::string& nameId, int id)
	: BaseRailBlock(nameId, id, true) {
}

int PoweredRailBlock::getVariant(int data) const{
	return Block::mGoldenRail->getBlockState(BlockState::RailDataBit).get<int>(data);
}
 
void PoweredRailBlock::onRedstoneUpdate(BlockSource& region, const BlockPos& pos, int strength, bool isFirstTime) const {
	UNUSED_PARAMETER(isFirstTime);
	if (region.getLevel().isClientSide() == false) {
		FullBlock block = region.getBlockAndData(pos);
		if (strength > 0) {
			Block::mGoldenRail->getBlockState(BlockState::RailDataBit).set(block.data, 1);
		}
		else {
			Block::mGoldenRail->getBlockState(BlockState::RailDataBit).set(block.data, 0);
		}
		region.setBlockAndData(pos, block, Block::UPDATE_ALL);
	}
}

void PoweredRailBlock::updateState(BlockSource& region, const BlockPos& pos, int data, int dir, int type) const {
	UNUSED_PARAMETER(type);
	bool isPowered = (data & RAIL_DATA_BIT) != 0;
	bool shouldPower = region.hasNeighborSignal(pos)
		|| _findPoweredRailSignal(region, pos, data, true, 0)
		|| _findPoweredRailSignal(region, pos, data, false, 0);

	if (shouldPower != isPowered) {
		DataID newData = static_cast<DataID>(shouldPower ? ((dir & RAIL_DIRECTION_MASK) | RAIL_DATA_BIT) : (dir & RAIL_DIRECTION_MASK));
		region.setBlockAndData(pos, mID, newData, Block::UPDATE_ALL);
		region.updateNeighborsAt(pos.below());
		if (dir >= 2 && dir <= 5) {
			region.updateNeighborsAt(pos.above());
		}
	}
}

bool PoweredRailBlock::_findPoweredRailSignal(BlockSource& region, const BlockPos& pos, int data, bool forward, int searchDepth) const {
	if (searchDepth >= 8) {
		return false;
	}

	int x = pos.x;
	int y = pos.y;
	int z = pos.z;
	bool checkBelow = true;
	int dir = data & RAIL_DIRECTION_MASK;

	switch (dir) {
	case 0:
		if (forward) ++z;
		else --z;
		break;
	case 1:
		if (forward) --x;
		else ++x;
		break;
	case 2:
		if (forward) {
			--x;
		} else {
			++x;
			++y;
			checkBelow = false;
		}
		dir = 1;
		break;
	case 3:
		if (forward) {
			--x;
			++y;
			checkBelow = false;
		} else {
			++x;
		}
		dir = 1;
		break;
	case 4:
		if (forward) {
			++z;
		} else {
			--z;
			++y;
			checkBelow = false;
		}
		dir = 0;
		break;
	case 5:
		if (forward) {
			++z;
			++y;
			checkBelow = false;
		} else {
			--z;
		}
		dir = 0;
		break;
	}

	return _isSameRailWithPower(region, BlockPos(x, y, z), forward, searchDepth, dir)
		|| (checkBelow && _isSameRailWithPower(region, BlockPos(x, y - 1, z), forward, searchDepth, dir));
}

bool PoweredRailBlock::_isSameRailWithPower(BlockSource& region, const BlockPos& pos, bool forward, int searchDepth, int dir) const {
	if (region.getBlockID(pos) != mID) {
		return false;
	}
	int meta = region.getData(pos);
	int railDir = meta & RAIL_DIRECTION_MASK;
	if (dir == 1 && (railDir == 0 || railDir == 4 || railDir == 5)) {
		return false;
	}
	if (dir == 0 && (railDir == 1 || railDir == 2 || railDir == 3)) {
		return false;
	}
	if ((meta & RAIL_DATA_BIT) != 0) {
		if (region.hasNeighborSignal(pos)) {
			return true;
		}
		return _findPoweredRailSignal(region, pos, meta, forward, searchDepth + 1);
	}
	return false;
}
