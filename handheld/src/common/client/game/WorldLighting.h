#pragma once

#include "world/level/BlockPos.h"
#include "world/level/ChunkPos.h"
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

class BlockSource;

namespace WorldLighting {

using TouchedSubChunkMap = std::unordered_map<ChunkPos, uint8_t>;

void propagateChunkLighting(BlockSource& region,
                            const ChunkPos& cp,
                            std::unordered_set<ChunkPos>* outTouchedChunks = nullptr,
                            TouchedSubChunkMap* outTouchedSubChunks = nullptr);

void updateLocalLighting(BlockSource& region,
                         const BlockPos& center,
                         int blockRadius = 15,
                         std::unordered_set<ChunkPos>* outTouchedChunks = nullptr,
                         TouchedSubChunkMap* outTouchedSubChunks = nullptr);

float sampleWorldBrightness(BlockSource& region, const BlockPos& pos, float sunFactor);

} // namespace WorldLighting
