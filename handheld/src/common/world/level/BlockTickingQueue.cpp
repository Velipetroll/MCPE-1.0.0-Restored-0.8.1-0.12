#include "world/level/BlockTickingQueue.h"
#include "world/level/BlockSource.h"
#include "world/level/Level.h"
#include "world/level/block/Block.h"
#include <algorithm>

void BlockTickingQueue::add(BlockSource& bs, const BlockPos& pos, BlockID id, int delay, int priority) {
	if (id == BlockID::AIR) {
		return;
	}
	if (mInstatick) {
		if (bs.getBlockID(pos) == id) {
			Block::mBlocks[id]->tick(bs, pos, bs.getLevel().getRandom());
		}
		return;
	}
	for (const auto& entry : mQueue) {
		if (entry.pos == pos && entry.id == id) {
			return;
		}
	}
	TickNextTickData data;
	data.pos = pos;
	data.id = id;
	data.tick = mCurrentTick + std::max(1, delay);
	data.priority = priority;
	data.seq = mNextSeq++;
	mQueue.push_back(data);
}

void BlockTickingQueue::remove(const BlockPos& pos, BlockID id) {
	mQueue.erase(
		std::remove_if(mQueue.begin(), mQueue.end(), [&](const TickNextTickData& e) {
			return e.pos == pos && (id == BlockID::AIR || e.id == id);
		}),
		mQueue.end());
}

bool BlockTickingQueue::isBlockToBeTickedAt(const BlockPos& pos, BlockID id) const {
	for (const auto& entry : mQueue) {
		if (entry.pos == pos && entry.id == id) {
			return true;
		}
	}
	return false;
}

bool BlockTickingQueue::tickPendingTicks(BlockSource& bs, Tick currentTick, int maxTicks, bool force) {
	mCurrentTick = currentTick;
	if (mQueue.empty()) {
		return false;
	}

	std::stable_sort(mQueue.begin(), mQueue.end(), [](const TickNextTickData& a, const TickNextTickData& b) {
		if (a.tick.getTimeStamp() != b.tick.getTimeStamp()) {
			return a.tick < b.tick;
		}
		if (a.priority != b.priority) {
			return a.priority < b.priority;
		}
		return a.seq < b.seq;
	});

	std::vector<TickNextTickData> toRun;
	int count = 0;
	auto it = mQueue.begin();
	while (it != mQueue.end() && count < maxTicks) {
		if (!force && it->tick > currentTick) {
			break;
		}
		toRun.push_back(*it);
		++it;
		++count;
	}
	if (toRun.empty()) {
		return false;
	}
	mQueue.erase(mQueue.begin(), it);

	for (const auto& entry : toRun) {
		if (bs.getBlockID(entry.pos) == entry.id && entry.id != BlockID::AIR && Block::mBlocks[entry.id]) {
			Block::mBlocks[entry.id]->tick(bs, entry.pos, bs.getLevel().getRandom());
		}
	}
	return true;
}

void BlockTickingQueue::tickAllPendingTicks(BlockSource& bs) {
	tickPendingTicks(bs, mCurrentTick, static_cast<int>(mQueue.size()), true);
}
