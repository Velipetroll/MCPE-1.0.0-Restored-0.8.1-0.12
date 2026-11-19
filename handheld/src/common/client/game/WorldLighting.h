#pragma once

#include "world/level/BlockPos.h"
#include "world/level/ChunkPos.h"
#include <unordered_set>

class BlockSource;

namespace WorldLighting {

void propagateChunkLighting(BlockSource& region, const ChunkPos& cp);

void updateLocalLighting(BlockSource& region,
                         const BlockPos& center,
                         int blockRadius = 15,
                         std::unordered_set<ChunkPos>* outTouchedChunks = nullptr);

float sampleWorldBrightness(BlockSource& region, const BlockPos& pos, float sunFactor);

} // namespace WorldLighting
