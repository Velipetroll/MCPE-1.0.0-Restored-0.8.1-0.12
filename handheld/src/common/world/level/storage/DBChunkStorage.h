/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include "common_header.h"

#include <condition_variable>
#include <mutex>

// #include "AppPlatformListener.h"
#include "Core/Utility/MemoryTracker.h"
 #include "platform/threading/ThreadLocal.h"
#include "world/level/chunk/ChunkSource.h"

class DBStorage;
class ChunkViewSource;
class ChunkKey;
enum class StructureFeatureType : int8_t;

namespace leveldb {
	class WriteBatch;
}

class DBChunkStorage : public ChunkSource, 
// 	public AppPlatformListener, 
	public MemoryTracker {
public:
	DBChunkStorage(Unique<ChunkSource> parent, DBStorage* storage);

	virtual ~DBChunkStorage();

	virtual void loadChunk(LevelChunk& c, bool forceImmediateReplacementDataLoad) override;

	virtual bool postProcess(ChunkViewSource& neighborhood) override;

	///save a chunk that is still owned by the main thread
	virtual bool saveLiveChunk(LevelChunk& lc) override;
	virtual bool saveLiveChunkSync(LevelChunk& lc) override;

	virtual void acquireDiscarded(ChunkPtr&& lc) override;

	virtual void hintDiscardBatchBegin() override;
	virtual void hintDiscardBatchEnd() override;

	virtual void onLowMemory() /*override*/;

	virtual void compact() override;

	virtual void waitDiscardFinished() override;

 	virtual MemoryStats getStats() const override;

	virtual bool findNearestFeature(StructureFeatureType feature, Player& player, BlockPos& pos) override;

	Unique<CompoundTag> getStructureTag() const override;
	void readStructureTag(const CompoundTag& tag) override;

private:

	bool _replaceChunkDataIfSaved(LevelChunk& lc, ChunkViewSource& neighborhood);
	bool _loadChunkFromDB(LevelChunk& lc);
	bool _hasChunk(const ChunkKey &key);

 	static ThreadLocal<leveldb::WriteBatch> threadBatch;
 	static ThreadLocal<std::string> threadBuffer;

	DBStorage* mStorage;
	std::vector<std::shared_ptr<leveldb::WriteBatch>> mBufferPool;
	std::vector<ChunkPtr> mDiscardBatch;
	std::unordered_set<ChunkPos> mLiveChunksBeingSaved;
	std::unordered_map<ChunkPos, ChunkPtr> mDiscardedWhileLiveSaved;	//TODO maybe we should just have true refcounting?
	bool mBatch = false;

 	std::shared_ptr<leveldb::WriteBatch> _getBuffer();

	void _writeBatch();
	void _serializeChunk(const LevelChunk& lc, leveldb::WriteBatch& batch);

	std::atomic<int> mPending;

	std::mutex mPendingWriteMutex;
	std::condition_variable mPendingWriteCv;
	std::unordered_map<ChunkPos, int> mPendingWriteCounts;

	void _markPendingWrite(const ChunkPos& pos);
	void _unmarkPendingWrite(const ChunkPos& pos);
	void _waitForPendingSave(const ChunkPos& pos, int radius = 0);

	struct ChunkPresenceKey {
		int32_t x;
		int32_t z;
		int32_t dim;

		bool operator==(const ChunkPresenceKey& other) const {
			return x == other.x && z == other.z && dim == other.dim;
		}
	};

	struct ChunkPresenceHash {
		size_t operator()(const ChunkPresenceKey& k) const {
			uint64_t h = (static_cast<uint64_t>(static_cast<uint32_t>(k.x)) * 0x9e3779b97f4a7c15ULL) ^
			             (static_cast<uint64_t>(static_cast<uint32_t>(k.z)) * 0xc2b2ae3d27d4eb4fULL) ^
			             (static_cast<uint64_t>(static_cast<uint32_t>(k.dim)) * 0x165667b19e3779f9ULL);
			return static_cast<size_t>(h ^ (h >> 33));
		}
	};

	static constexpr uint8_t PRESENCE_KNOWN = 0x01;
	static constexpr uint8_t PRESENCE_EXISTS = 0x02;
	static constexpr uint8_t PRESENCE_DATA2D_KNOWN = 0x04;
	static constexpr uint8_t PRESENCE_DATA2D_SAVED = 0x08;

	mutable std::mutex mChunkPresenceMutex;
	mutable std::unordered_map<ChunkPresenceKey, uint8_t, ChunkPresenceHash> mChunkPresenceCache;

	uint8_t _getChunkPresenceFlags(const ChunkPos& pos, int dimId) const;
	void _updateChunkPresenceFlags(const ChunkPos& pos, int dimId, uint8_t setMask, uint8_t valueBits) const;
};
