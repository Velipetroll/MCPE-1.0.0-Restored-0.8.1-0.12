/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include "common_header.h"

#include "CommonTypes.h"
#include "client/renderer/chunks/VisibilityNode.h"
#include "client/renderer/renderer/TerrainLayer.h"
#include "client/renderer/chunks/RangeIndices.h"
#include "world/level/LevelConstants.h"
#include "world/level/BlockPos.h"
#include "world/level/block/Block.h"
#include "client/renderer/chunks/RenderChunkSorter.h"

#include <array>
// #include "platform/threading/ThreadLocal.h"

class ChunkSource;
class Tessellator;
class MemoryTracker;
class ChunkViewSource;
class BlockTessellator;
class RenderChunk;
class BlockSource;
class BlockPos;
class RebuildData;
class BlockQueue;
class VisibilityExtimator;

class RenderChunkBuilder {
public:

	Unique<ChunkViewSource> localSource;
	Unique<BlockTessellator> renderer;

	bool visibilityChanged, allDark, skyLit;
	float averageSkyLight;
	VisibilityNode mVisibility;

	RenderChunkBuilder(ChunkSource& mainSource, Tessellator* maybeTessellator = nullptr, MemoryTracker* tracker = nullptr);
	RenderChunkBuilder() = delete;
	RenderChunkBuilder(const RenderChunkBuilder&) = delete;

	~RenderChunkBuilder();

	void build(RenderChunk& rc, bool transparentLeaves, bool smoothLighting, uint8_t lodLevel = 0, bool greedyMeshing = false, bool allowCluster = false);

	VisibilityExtimator* _getVisibilityHelper();


	RangeIndices mTerrainLayerRange[enum_cast(TerrainLayer::Count)];
	RangeIndices mRenderLayerRange[enum_cast(BlockRenderLayer::_RENDERLAYER_COUNT)];

	// Pre-sorted index buffer produced during build() so endRebuild() uploads VBO+EBO
	// immediately without waiting for a separate faceSort() pass.
	std::vector<uint16_t> mSortedIndices16;
	std::vector<uint32_t> mSortedIndices32;
	uint32_t mTboRenderLayerMask = 0;
	std::array<std::vector<uint32_t>, 7> mSortBuckets;
	uint32_t mSortedIndexSize = 0;
	uint32_t mSortedNumFaces = 0;
	RangeIndices mSortedRenderLayerRange[enum_cast(BlockRenderLayer::_RENDERLAYER_COUNT)];
	RangeIndices mSortedTerrainLayerRange[enum_cast(TerrainLayer::Count)];
	std::vector<std::array<uint32_t, 4>> mGreedyCellBuffer;

	void trim();

protected:

	BlockQueue* mQueues = nullptr;

	Unique<Tessellator> ownedTessellator;
	RenderChunkSorter mRenderChunkSorter;

	Index _setRange(TerrainLayer r, Index start, Index count);
	Index _setRenderLayerRange(BlockRenderLayer r, Index start, Index count);

	void _checkPropagatedBrightness(BlockSource& source, const BlockPos& p);
	bool _seesSkyDirectly(RenderChunk& rc, BlockSource& region);
	bool _sortBlocks(BlockSource& region, RenderChunk& rc, VisibilityExtimator* VE, bool transparentLeaves, uint8_t lodLevel, bool greedyMeshing);
	void _buildLightAoCache(const BlockPos& rcPos, BlockSource& region);
	void _greedyMeshPass(RenderChunk& rc, BlockSource& region, uint8_t lodLevel, bool smoothLighting, bool transparentLeaves, BlockRenderLayer targetLayer);
	bool _tessellateQueues(RenderChunk& rc, BlockSource& region, uint8_t lodLevel, bool greedyMeshing, bool smoothLighting, bool transparentLeaves);
	void _buildRanges();
	void _checkAllDark(BlockSource& region, const BlockPos& p);

	bool mHasGreedyLayer[enum_cast(BlockRenderLayer::_RENDERLAYER_COUNT)]{};
	bool mHasAnyGreedy = false;
	bool mNeedsBlockTessCache = false;
	bool mLightAoBuilt = false;
	std::array<uint8_t, 16 * 16 * 16> mGreedyVoxelLayer{};
	std::array<uint8_t, 16 * 16 * 16> mGreedyVoxelFaceMask{};
	uint16_t mGreedyActivePlanes[enum_cast(BlockRenderLayer::_RENDERLAYER_COUNT)][6]{};
	std::array<uint16_t, 18 * 18 * 18> mLightAoCache{};
};
