#include "common_header.h"

#include "RedstoneBlock.h"
#include "world/level/material/Material.h"
#include "world/level/BlockSource.h"
#include "world/level/dimension/Dimension.h"
#include "world/redstone/Redstone.h"
// #include "world/redstone/circuit/CircuitSystem.h"
// #include "world/redstone/circuit/components/ProducerComponent.h"
#include "world/level/Level.h"

RedstoneBlock::RedstoneBlock(const std::string& nameId, int id)
	: Block(nameId, id, Material::getMaterial(MaterialType::Metal)) {
}

bool RedstoneBlock::isSignalSource() const {
	return true;
}

int RedstoneBlock::getSignal(BlockSource& region, const BlockPos& pos, int dir) const {
	return Redstone::SIGNAL_MAX;
}

bool RedstoneBlock::shouldConnectToRedstone(BlockSource& region, const BlockPos& pos, int direction) const
{
	return true;
}

void RedstoneBlock::onLoaded(BlockSource& region, const BlockPos& pos) const {
}

void RedstoneBlock::onPlace(BlockSource& region, const BlockPos& pos) const {
	onLoaded(region, pos);
	if (!region.getLevel().isClientSide()) {
		region.updateNeighborsAt(pos);
	}
}

void RedstoneBlock::onRemove(BlockSource& region, const BlockPos& pos) const {
	Block::onRemove(region, pos);
	if (!region.getLevel().isClientSide()) {
		region.updateNeighborsAt(pos);
	}
}
