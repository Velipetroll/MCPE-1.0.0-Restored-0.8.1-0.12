
#include "common_header.h"

#include "world/level/block/RedstoneTorchBlock.h"

#include "world/Facing.h"
#include "world/item/ItemInstance.h"
#include "world/level/BlockSource.h"
#include "world/level/BlockTickingQueue.h"
#include "world/level/Level.h"
#include "world/level/dimension/Dimension.h"
#include "world/redstone/Redstone.h"
// #include "world/redstone/circuit/CircuitSystem.h"
// #include "world/redstone/circuit/components/RedstoneTorchCapacitor.h"
// #include "world/entity/ParticleType.h"

RedstoneTorchBlock::RedstoneTorchBlock(const std::string& nameId, int id, bool on)
	: TorchBlock(nameId, id) 
	, mOn(on) {

	setTicking(false);

	mRenderLayer = BlockRenderLayer::RENDERLAYER_ALPHATEST_SINGLE_SIDE;	
}

void RedstoneTorchBlock::onRedstoneUpdate(BlockSource& region, const BlockPos& pos, int strength, bool isFirstTime) const {

	if (region.getLevel().isClientSide() == false) {

		DataID data = region.getData(pos);
		BlockID current = region.getBlockID(pos);
// 		auto &circuit = region.getDimension().getCircuitSystem();
		//Minecraft will change, but we don't want our graph to change during this time
// 		circuit.lockGraph(true);
		if (strength == 0) {
			if (current != Block::mUnlitRedStoneTorch->mID) {
				region.setBlockAndData(pos, Block::mUnlitRedStoneTorch->mID, data, UPDATE_CLIENTS);
			}
		}
		else {
			if (current != Block::mLitRedStoneTorch->mID) {
				region.setBlockAndData(pos, Block::mLitRedStoneTorch->mID, data, UPDATE_CLIENTS);
			}
		}
		//Ok, let others change now
// 		circuit.lockGraph(false);
	}
}

void RedstoneTorchBlock::onLoaded(BlockSource& region, const BlockPos& pos) const {
	installCircuit(region, pos, true);
}

void RedstoneTorchBlock::onPlace(BlockSource& region, const BlockPos& pos) const {
	TorchBlock::onPlace(region, pos);
	if (!region.getLevel().isClientSide() && region.getBlockID(pos) == mID) {
		if (mOn) {
			region.updateNeighborsAt(pos);
			region.updateNeighborsAt(pos.above());
		}
		bool powered = _isAttachedPowered(region, pos);
		if ((mOn && powered) || (!mOn && !powered)) {
			if (auto* tq = region.getTickQueue(pos)) {
				tq->add(region, pos, mID, getTickDelay());
			}
		}
	}
}

void RedstoneTorchBlock::onRemove(BlockSource& region, const BlockPos& pos) const {
	if (!region.getLevel().isClientSide() && mOn) {
		region.updateNeighborsAt(pos);
		region.updateNeighborsAt(pos.above());
	}
	TorchBlock::onRemove(region, pos);
}

bool RedstoneTorchBlock::_isAttachedPowered(BlockSource& region, const BlockPos& pos) const {
	int torchDir = getBlockState(BlockState::FacingDirection).get<int>(region.getData(pos));
	if (torchDir == TORCH_WEST) return region.getSignal(pos.west(), Facing::WEST) > 0;
	if (torchDir == TORCH_EAST) return region.getSignal(pos.east(), Facing::EAST) > 0;
	if (torchDir == TORCH_NORTH) return region.getSignal(pos.north(), Facing::NORTH) > 0;
	if (torchDir == TORCH_SOUTH) return region.getSignal(pos.south(), Facing::SOUTH) > 0;
	return region.getSignal(pos.below(), Facing::DOWN) > 0;
}

bool RedstoneTorchBlock::isSignalSource() const {
	return true;
}

int RedstoneTorchBlock::getSignal(BlockSource& region, const BlockPos& pos, int dir) const {
	if (!mOn) {
		return Redstone::SIGNAL_NONE;
	}
	int torchDir = getBlockState(BlockState::FacingDirection).get<int>(region.getData(pos));
	if (torchDir == TORCH_TOP && dir == Facing::UP) return Redstone::SIGNAL_NONE;
	if (torchDir == TORCH_WEST && dir == Facing::EAST) return Redstone::SIGNAL_NONE;
	if (torchDir == TORCH_EAST && dir == Facing::WEST) return Redstone::SIGNAL_NONE;
	if (torchDir == TORCH_NORTH && dir == Facing::SOUTH) return Redstone::SIGNAL_NONE;
	if (torchDir == TORCH_SOUTH && dir == Facing::NORTH) return Redstone::SIGNAL_NONE;
	if (torchDir == TORCH_UNKNOWN && dir == Facing::UP) return Redstone::SIGNAL_NONE;
	return Redstone::SIGNAL_MAX;
}

int RedstoneTorchBlock::getDirectSignal(BlockSource& region, const BlockPos& pos, int dir) const {
	return (mOn && dir == Facing::DOWN) ? getSignal(region, pos, dir) : Redstone::SIGNAL_NONE;
}

void RedstoneTorchBlock::neighborChanged(BlockSource& region, const BlockPos& pos, const BlockPos& neighborPos) const {
	TorchBlock::neighborChanged(region, pos, neighborPos);
	if (region.getBlockID(pos) == mID && !region.getLevel().isClientSide()) {
		bool powered = _isAttachedPowered(region, pos);
		if ((mOn && powered) || (!mOn && !powered)) {
			if (auto* tq = region.getTickQueue(pos)) {
				tq->add(region, pos, mID, getTickDelay());
			}
		}
	}
}

void RedstoneTorchBlock::installCircuit(BlockSource &source, const BlockPos& pos, bool bLoading) const
{
	UNUSED_PARAMETER(source, pos, bLoading);
}

void RedstoneTorchBlock::tick(BlockSource& region, const BlockPos& pos, Random& random) const {
	UNUSED_PARAMETER(random);
	if (region.getLevel().isClientSide()) {
		return;
	}
	bool powered = _isAttachedPowered(region, pos);
	DataID data = region.getData(pos);
	if (mOn && powered) {
		region.setBlockAndData(pos, Block::mUnlitRedStoneTorch->mID, data, Block::UPDATE_ALL);
		region.updateNeighborsAt(pos.above());
	} else if (!mOn && !powered) {
		region.setBlockAndData(pos, Block::mLitRedStoneTorch->mID, data, Block::UPDATE_ALL);
		region.updateNeighborsAt(pos.above());
	}
}

void RedstoneTorchBlock::animateTick(BlockSource& region, const BlockPos& pos, Random& random) const {

	if(!mOn) {
		return;
	}

	auto &level = region.getLevel();

	int dir = getBlockState(BlockState::FacingDirection).get<int>(region.getData(pos));
	float x = pos.x + 0.5f + (random.nextFloat() - 0.5f) * 0.2f;
	float y = pos.y + 0.7f + (random.nextFloat() - 0.5f) * 0.2f;
	float z = pos.z + 0.5f + (random.nextFloat() - 0.5f) * 0.2f;
	float h = 0.22f;
	float r = 0.27f;

// 	if(dir == Facing::UP) {
// 		level.addParticle(ParticleType::RedDust, Vec3(x - r, y + h, z), Vec3::ZERO);
// 	}
// 	else if(dir == Facing::NORTH) {
// 		level.addParticle(ParticleType::RedDust, Vec3(x + r, y + h, z), Vec3::ZERO);
// 	}
// 	else if(dir == Facing::SOUTH) {
// 		level.addParticle(ParticleType::RedDust, Vec3(x, y + h, z - r), Vec3::ZERO);
// 	}
// 	else if(dir == Facing::WEST) {
// 		level.addParticle(ParticleType::RedDust, Vec3(x, y + h, z + r), Vec3::ZERO);
// 	}
// 	else {
// 		level.addParticle(ParticleType::RedDust, Vec3(x, y, z), Vec3::ZERO);
// 	}
}

int RedstoneTorchBlock::getTickDelay() const {
	return 2;
}

bool RedstoneTorchBlock::shouldConnectToRedstone(BlockSource& region, const BlockPos& pos, int direction) const
{
	return true;
}

ItemInstance RedstoneTorchBlock::asItemInstance(BlockSource& region, const BlockPos& pos, int blockData) const {
	return ItemInstance(Block::mLitRedStoneTorch);
}

ItemInstance RedstoneTorchBlock::getSilkTouchItemInstance(DataID data) const {
	return ItemInstance(Block::mLitRedStoneTorch);
}

int RedstoneTorchBlock::getResource(Random& random, int data, int bonusLootLevel) const {
	return Block::mLitRedStoneTorch->mID;
}
