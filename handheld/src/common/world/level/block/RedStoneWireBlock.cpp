/********************************************************
*  (c) Mojang.    All rights reserved.                  *
*  (c) Microsoft. All rights reserved.                  *
*********************************************************/
#include "common_header.h"

#include "world/level/block/RedStoneWireBlock.h"
#include "world/Direction.h"
#include "world/Facing.h"
// #include "world/entity/ParticleType.h"
#include "world/level/BlockSource.h"
#include "world/level/material/Material.h"
#include "world/level/Level.h"
#include "world/level/dimension/Dimension.h"
#include "world/redstone/Redstone.h"
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "PistonBlock.h"
#include <queue>
#include <unordered_map>

namespace {
struct BlockPosHash {
	size_t operator()(const BlockPos& p) const noexcept {
		return (static_cast<size_t>(p.x) * 73856093u) ^ (static_cast<size_t>(p.y) * 19349663u) ^ (static_cast<size_t>(p.z) * 83492791u);
	}
};

static thread_local bool sWireShouldSignal = true;
static thread_local bool sUpdatingWirePower = false;

struct WireSignalGuard {
	bool prev;
	explicit WireSignalGuard(bool val) : prev(sWireShouldSignal) { sWireShouldSignal = val; }
	~WireSignalGuard() { sWireShouldSignal = prev; }
};
}

RedStoneWireBlock::RedStoneWireBlock(const std::string& nameId, int id)
	: Block(nameId, id, Material::getMaterial(MaterialType::RedstoneWire)) {
	setVisualShape(Vec3::ZERO, Vec3(1, 1 / 16.0f, 1));
	setSolid(false);
	mProperties = BlockProperty::BreakOnPush;

	mRenderLayer = RENDERLAYER_ALPHATEST;
	mThickness = 0.07f;

	mTranslucency[id] = std::max(0.8f, mMaterial.getTranslucency());
}

const AABB& RedStoneWireBlock::getAABB(BlockSource& region, const BlockPos& pos, AABB& bufferValue, int optionalData, bool isClipping, int clipData) const {
	if (isClipping) {
		bufferValue = getVisualShape(region, pos, bufferValue, true);
		return bufferValue.move(pos);
	}
	return AABB::EMPTY;
}

bool RedStoneWireBlock::canSurvive(BlockSource& region, const BlockPos& pos) const {
	auto& blockRef = region.getBlock(pos.below());
	bool haveSupport = region.canProvideSupport(pos.below(), Facing::UP, BlockSupportType::Center) && region.canProvideSupport(pos.below(), Facing::UP, BlockSupportType::Edge);
	return haveSupport || blockRef.isType(Block::mGlowStone) ||
		blockRef.isType(Block::mUnlitRedStoneLamp) || blockRef.isType(Block::mLitRedStoneLamp);
}

bool RedStoneWireBlock::mayPlace(BlockSource& region, const BlockPos& pos) const {
	return canSurvive(region, pos);
}

void RedStoneWireBlock::onLoaded(BlockSource& region, const BlockPos& pos) const {
	UNUSED_PARAMETER(region, pos);
}

void RedStoneWireBlock::onPlace(BlockSource& region, const BlockPos& pos) const {
	Block::onPlace(region, pos);
	if (!region.getLevel().isClientSide()) {
		_updatePower(region, pos);
		_notifyWireNeighbors(region, pos);
	}
}

void RedStoneWireBlock::onRemove(BlockSource& region, const BlockPos& pos) const {
	Block::onRemove(region, pos);
	if (!region.getLevel().isClientSide()) {
		_notifyWireNeighbors(region, pos);
		static const BlockPos offsets[4] = { BlockPos(1,0,0), BlockPos(-1,0,0), BlockPos(0,0,1), BlockPos(0,0,-1) };
		for (const auto& o : offsets) {
			BlockPos np = pos + o;
			if (region.getBlockID(np) == mID) _updatePower(region, np);
			if (region.getBlockID(np.above()) == mID) _updatePower(region, np.above());
			if (region.getBlockID(np.below()) == mID) _updatePower(region, np.below());
		}
	}
}

void RedStoneWireBlock::onRedstoneUpdate(BlockSource& region, const BlockPos& pos, int strength, bool isFirstTime) const {
	UNUSED_PARAMETER(isFirstTime);
	if (!region.getLevel().isClientSide()) {
		FullBlock fullBlock = region.getBlockAndData(pos);
		region.setBlockAndData(pos, fullBlock.id, static_cast<DataID>(strength & 0xF), Block::UPDATE_ALL);
	}
}

void RedStoneWireBlock::neighborChanged(BlockSource& region, const BlockPos& pos, const BlockPos& neighborPos) const {
	UNUSED_PARAMETER(neighborPos);
	if (!region.getLevel().isClientSide()) {
		if (!canSurvive(region, pos)) {
			spawnResources(region, pos, 0, 1);
			region.removeBlock(pos);
		} else if (!sUpdatingWirePower) {
			_updatePower(region, pos);
		}
	}
}

bool RedStoneWireBlock::isSignalSource() const {
	return sWireShouldSignal;
}

void RedStoneWireBlock::_getWireConnectivity(BlockSource& region, const BlockPos& pos, bool& connN, bool& connS, bool& connW, bool& connE) {
	BlockPos west = pos.west();
	BlockPos east = pos.east();
	BlockPos south = pos.south();
	BlockPos north = pos.north();
	connW = shouldConnectTo(region, west, Direction::WEST) || (!region.isSolidBlockingBlock(west) && region.getBlockID(west.below()) == Block::mRedStoneDust->mID);
	connE = shouldConnectTo(region, east, Direction::EAST) || (!region.isSolidBlockingBlock(east) && region.getBlockID(east.below()) == Block::mRedStoneDust->mID);
	connS = shouldConnectTo(region, south, Direction::SOUTH) || (!region.isSolidBlockingBlock(south) && region.getBlockID(south.below()) == Block::mRedStoneDust->mID);
	connN = shouldConnectTo(region, north, Direction::NORTH) || (!region.isSolidBlockingBlock(north) && region.getBlockID(north.below()) == Block::mRedStoneDust->mID);
	if (!region.isSolidBlockingBlock(pos.above())) {
		if (region.getBlockID(west.above()) == Block::mRedStoneDust->mID) connW = true;
		if (region.getBlockID(east.above()) == Block::mRedStoneDust->mID) connE = true;
		if (region.getBlockID(north.above()) == Block::mRedStoneDust->mID) connN = true;
		if (region.getBlockID(south.above()) == Block::mRedStoneDust->mID) connS = true;
	}
}

int RedStoneWireBlock::getSignal(BlockSource& region, const BlockPos& pos, int dir) const {
	if (!sWireShouldSignal) {
		return Redstone::SIGNAL_NONE;
	}
	int signal = getBlockState(BlockState::RedstoneSignal).get<int>(region.getData(pos));
	if (signal <= 0) {
		return Redstone::SIGNAL_NONE;
	}
	if (dir == Facing::UP) {
		return signal;
	}
	if (dir == Facing::DOWN) {
		return Redstone::SIGNAL_NONE;
	}
	bool connN = false, connS = false, connW = false, connE = false;
	_getWireConnectivity(region, pos, connN, connS, connW, connE);
	int oppDir = Facing::OPPOSITE_FACING[dir];
	if ((oppDir == Facing::NORTH && (connN || (!connW && !connE))) ||
	    (oppDir == Facing::SOUTH && (connS || (!connW && !connE))) ||
	    (oppDir == Facing::WEST && (connW || (!connN && !connS))) ||
	    (oppDir == Facing::EAST && (connE || (!connN && !connS)))) {
		return signal;
	}
	return Redstone::SIGNAL_NONE;
}

int RedStoneWireBlock::getDirectSignal(BlockSource& region, const BlockPos& pos, int dir) const {
	return sWireShouldSignal ? getSignal(region, pos, dir) : Redstone::SIGNAL_NONE;
}

void RedStoneWireBlock::_notifyWireNeighbors(BlockSource& region, const BlockPos& pos) {
	region.updateNeighborsAt(pos);
	static const BlockPos dirs[6] = {
		BlockPos(0, -1, 0), BlockPos(0, 1, 0),
		BlockPos(0, 0, -1), BlockPos(0, 0, 1),
		BlockPos(-1, 0, 0), BlockPos(1, 0, 0)
	};
	for (const auto& d : dirs) {
		BlockPos np = pos + d;
		if (region.isSolidBlockingBlock(np)) {
			region.updateNeighborsAt(np);
		}
	}
}

void RedStoneWireBlock::_updatePower(BlockSource& region, const BlockPos& startPos) {
	if (sUpdatingWirePower || region.getBlockID(startPos) != Block::mRedStoneDust->mID) {
		return;
	}
	sUpdatingWirePower = true;

	// Collect connected wire component
	std::vector<BlockPos> nodes;
	std::unordered_map<BlockPos, int, BlockPosHash> nodeIndex;
	std::queue<BlockPos> q;
	nodes.push_back(startPos);
	nodeIndex[startPos] = 0;
	q.push(startPos);

	auto gatherWireNeighbors = [&](const BlockPos& cur, std::vector<BlockPos>& out) {
		out.clear();
		bool aboveBlocked = region.isSolidBlockingBlock(cur.above());
		static const BlockPos horiz[4] = {
			BlockPos(1, 0, 0), BlockPos(-1, 0, 0), BlockPos(0, 0, 1), BlockPos(0, 0, -1)
		};
		for (const auto& h : horiz) {
			BlockPos np = cur + h;
			if (region.getBlockID(np) == Block::mRedStoneDust->mID) {
				out.push_back(np);
			} else if (!region.isSolidBlockingBlock(np) && region.getBlockID(np.below()) == Block::mRedStoneDust->mID) {
				out.push_back(np.below());
			}
			if (!aboveBlocked && region.isSolidBlockingBlock(np) && region.getBlockID(np.above()) == Block::mRedStoneDust->mID) {
				out.push_back(np.above());
			}
		}
	};

	std::vector<BlockPos> adj;
	while (!q.empty() && nodes.size() < 512) {
		BlockPos cur = q.front();
		q.pop();
		gatherWireNeighbors(cur, adj);
		for (const auto& np : adj) {
			if (nodeIndex.find(np) == nodeIndex.end()) {
				nodeIndex[np] = static_cast<int>(nodes.size());
				nodes.push_back(np);
				q.push(np);
			}
		}
	}

	std::vector<int> oldPower(nodes.size(), 0);
	std::vector<int> newPower(nodes.size(), 0);
	{
		WireSignalGuard guard(false);
		for (size_t i = 0; i < nodes.size(); ++i) {
			oldPower[i] = Block::mRedStoneDust->getBlockState(BlockState::RedstoneSignal).get<int>(region.getData(nodes[i]));
			newPower[i] = std::min(15, std::max(0, region.getBestNeighborSignal(nodes[i])));
		}
	}

	// Propagate signal strength (max-first)
	using PQItem = std::pair<int, int>;
	std::priority_queue<PQItem> pq;
	for (size_t i = 0; i < nodes.size(); ++i) {
		if (newPower[i] > 1) {
			pq.emplace(newPower[i], static_cast<int>(i));
		}
	}
	while (!pq.empty()) {
		auto [str, idx] = pq.top();
		pq.pop();
		if (str < newPower[idx] || str <= 1) continue;
		gatherWireNeighbors(nodes[idx], adj);
		for (const auto& np : adj) {
			auto it = nodeIndex.find(np);
			if (it == nodeIndex.end()) continue;
			int nIdx = it->second;
			if (str - 1 > newPower[nIdx]) {
				newPower[nIdx] = str - 1;
				pq.emplace(newPower[nIdx], nIdx);
			}
		}
	}

	std::vector<BlockPos> changed;
	for (size_t i = 0; i < nodes.size(); ++i) {
		if (newPower[i] != oldPower[i]) {
			region.setBlockAndData(nodes[i], Block::mRedStoneDust->mID, static_cast<DataID>(newPower[i] & 0xF), Block::UPDATE_CLIENTS);
			changed.push_back(nodes[i]);
		}
	}

	sUpdatingWirePower = false;

	for (const BlockPos& cp : changed) {
		_notifyWireNeighbors(region, cp);
	}
}

int RedStoneWireBlock::getColor(BlockSource& region, const BlockPos& pos, DataID auxData) const {
	return getColor(auxData);
}

int RedStoneWireBlock::getColor(int data) const {
	int signal = getBlockState(BlockState::RedstoneSignal).get<int>(data);
	Color color;

	float pow = (signal / 15.0f);
	color.r = pow * 0.6f + 0.4f;
	if (signal == 0) {
		color.r = 0.3f;
	}

	color.g = pow * pow * 0.7f - 0.5f;
	color.b = pow * pow * 0.6f - 0.7f;
	if (color.g < 0) {
		color.g = 0;
	}
	if (color.b < 0) {
		color.b = 0;
	}

	return color.toARGB();
}


int RedStoneWireBlock::getResource(Random& random, int data, int bonusLootLevel) const {
	return Item::mRedStone->getId();
}

ItemInstance RedStoneWireBlock::asItemInstance(BlockSource& region, const BlockPos& pos, int blockData) const {
	return ItemInstance(Item::mRedStone);
}

ItemInstance RedStoneWireBlock::getSilkTouchItemInstance(DataID data) const {
	return ItemInstance(Item::mRedStone);
}

void RedStoneWireBlock::animateTick(BlockSource& region, const BlockPos& pos, Random& random) const {

	// TODO: This assumes that the power amount is stored in data

	int data = region.getData(pos);
	int signal = getBlockState(BlockState::RedstoneSignal).get<int>(data);

 	if (signal > 0) {
		float xx = pos.x + 0.5f + (random.nextFloat() - 0.5f) * 0.2f;
		float yy = pos.y + 1.0f / 16.0f;
		float zz = pos.z + 0.5f + (random.nextFloat() - 0.5f) * 0.2f;


		auto& level = region.getLevel();
		// TODO: rherlitz
// 		level.addParticle(ParticleType::RedDust, Vec3(xx, yy, zz), Vec3(0.0f, 0.0f, 0.0f));
	}
}

bool RedStoneWireBlock::shouldConnectToRedstone(BlockSource& region, const BlockPos& pos, int direction) const
{
	return true;
}

bool RedStoneWireBlock::shouldConnectTo(BlockSource &region, const BlockPos& pos, int direction) {
	const Block& block = region.getBlock(pos);
	return block.shouldConnectToRedstone(region, pos, direction);
	// DG:TODO remove function and just call shouldConnectToRedstone
}

