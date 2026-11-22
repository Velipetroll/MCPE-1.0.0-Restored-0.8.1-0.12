#pragma once
// 1:1 MCPE 0.8.1 / 1.0.0 BlockTickingQueue
#include "world/level/Tick.h"
#include "world/level/BlockPos.h"
#include "CommonTypes.h"
#include <vector>

class BlockSource;
class CompoundTag;

struct TickNextTickData {
	BlockPos pos;
	BlockID id{0};
	Tick tick;
	int priority = 0;
	uint64_t seq = 0;
};

class BlockTickingQueue {
public:
	BlockTickingQueue() = default;
	explicit BlockTickingQueue(Tick t) : mCurrentTick(t) {}

	void tickAllPendingTicks(BlockSource& bs);
	bool tickPendingTicks(BlockSource& bs, Tick currentTick, int maxTicks = 100, bool force = false);

	void add(BlockSource& bs, const BlockPos& pos, BlockID id, int delay, int priority = 0);
	void remove(const BlockPos& pos, BlockID id);
	bool isBlockToBeTickedAt(const BlockPos& pos, BlockID id) const;

	bool isInstaticking() const { return mInstatick; }
	void setInstaticking(bool v) { mInstatick = v; }

	bool isEmpty() const { return mQueue.empty(); }

	void save(CompoundTag& tag) const {}
	void load(const CompoundTag& tag) {}

private:
	std::vector<TickNextTickData> mQueue;
	Tick mCurrentTick{0};
	uint64_t mNextSeq = 0;
	bool mInstatick = false;
};
