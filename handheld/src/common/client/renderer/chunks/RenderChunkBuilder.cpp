/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "client/renderer/chunks/RenderChunkBuilder.h"

#include "world/level/chunk/ChunkSource.h"
#include "client/renderer/renderer/Tessellator.h"
// #include "world/level/LightLayer.h"
#include "world/level/BlockSource.h"
#include "client/renderer/chunks/RenderChunk.h"
#include "world/level/block/Block.h"
#include "client/renderer/chunks/VisibilityExtimator.h"
#include "world/level/chunk/ChunkViewSource.h"
#include "world/level/block/LeafBlock.h"
#include "world/level/block/OldLeafBlock.h"
#include "client/renderer/block/BlockGraphics.h"
#include "client/renderer/block/BlockTessellator.h"
#include "client/renderer/chunks/BlockQueue.h"
// #include "Renderer/GlobalConstantBufferManager.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/dimension/Dimension.h"

typedef std::array<BlockQueue, BlockRenderLayer::_RENDERLAYER_COUNT> QueuePool;


 static ThreadLocal<QueuePool> queuePool([] (){
 	return make_unique<QueuePool>();
 } );

RenderChunkBuilder::RenderChunkBuilder(ChunkSource& mainSource, Tessellator* t, MemoryTracker* tracker){
	//make a new chunkSource grabbing chunks from the main source
	localSource = make_unique<ChunkViewSource>(mainSource, ChunkSource::LoadMode::None);

	if (!t) {
		ownedTessellator = make_unique<Tessellator>(tracker);
		t = ownedTessellator.get();
	}

	renderer = make_unique<BlockTessellator>(*t);
}

RenderChunkBuilder::~RenderChunkBuilder() {

}

void RenderChunkBuilder::trim() {
	if (ownedTessellator) {
		ownedTessellator->trim();
	}
}

void RenderChunkBuilder::_checkAllDark(BlockSource& region, const BlockPos& p) {
	if (allDark) {
		if (auto lc = region.getChunkAt(p)) {
			if (lc->getRawBrightness(ChunkBlockPos(p.above()), Brightness::MIN) > 2) {
				allDark = false;
			}
		}
	}
}

Index RenderChunkBuilder::_setRange(TerrainLayer r, Index start, Index count) {
	return mTerrainLayerRange[static_cast<int>(r)].set(start, count);
}

Index RenderChunkBuilder::_setRenderLayerRange(BlockRenderLayer r, Index start, Index count) {
	return mRenderLayerRange[(int)r].set(start, count);
}

void RenderChunkBuilder::_checkPropagatedBrightness(BlockSource& source, const BlockPos& p) {
 	if (!skyLit || allDark) {
 		auto br = source.getBrightness(source.getDimension().getSkyLightLayer(), p);
 
 		if (br) {
 			skyLit = true;
 		}
 
 		if (allDark) {
 			br += source.getBrightness(LightLayer::BLOCK, p);
 			if (br > 2) {
 				allDark = false;
 			}
 		}
 	}
}

bool RenderChunkBuilder::_seesSkyDirectly(RenderChunk& rc, BlockSource& region) {
	BlockPos min = rc.getPosition();
	BlockPos max = min + RENDERCHUNK_SIDE;
	LevelChunk* lc = region.getChunk(min);
	if (!lc) return false;

	for (int lx = 0; lx < RENDERCHUNK_SIDE; ++lx) {
		for (int lz = 0; lz < RENDERCHUNK_SIDE; ++lz) {
			int y = static_cast<int>(lc->getHeightmap(ChunkBlockPos(lx, 0, lz)));
			while (y > 0 && Block::mTranslucency[lc->getBlock(ChunkBlockPos(lx, y, lz))] > 0) {
				--y;
			}
			if (y >= min.y && y <= max.y) {
				return true;
			}
		}
	}
	return false;
}

bool RenderChunkBuilder::_sortBlocks(BlockSource& region, RenderChunk& rc, VisibilityExtimator* VE, bool transparentLeaves) {
	BlockPos min = rc.getPosition();
	LevelChunk* lc = region.getChunk(min);
	if (!lc) {
		return true;
	}
	const size_t subY = static_cast<size_t>(min.y >> 4);
	const auto subs = lc->getSubChunks();
	const LevelChunk::SubChunk* sc = (subY < subs.size()) ? subs[subY].get() : nullptr;
	if (!sc || !sc->hasBlocks()) {
		if (!skyLit) {
			if (subY >= subs.size() || !Util::isAllZero(subs[subY]->mSkyLight.span())) {
				skyLit = true;
			}
		}
		if (allDark) {
			if (subY >= subs.size()) {
				allDark = lc->getRawBrightness(ChunkBlockPos(0, min.y, 0), Brightness::MIN) <= 2;
			} else if (!Util::isAllZero(sc->mSkyLight.span()) || !Util::isAllZero(sc->mBlockLight.span())) {
				allDark = false;
			}
		}
		return true;
	}

	const LevelChunk::SubChunk* scAbove = (subY + 1 < subs.size()) ? subs[subY + 1].get() : nullptr;
	const LevelChunk::SubChunk* scBelow = (subY > 0) ? subs[subY - 1].get() : nullptr;
	auto getNeighborSubChunk = [&](int dx, int dz) -> const LevelChunk::SubChunk* {
		LevelChunk* nlc = region.getChunk(ChunkPos(lc->getPosition().x + dx, lc->getPosition().z + dz));
		if (!nlc) return nullptr;
		const auto nsubs = nlc->getSubChunks();
		return (subY < nsubs.size()) ? nsubs[subY].get() : nullptr;
	};
	const LevelChunk::SubChunk* scWest = getNeighborSubChunk(-1, 0);
	const LevelChunk::SubChunk* scEast = getNeighborSubChunk(1, 0);
	const LevelChunk::SubChunk* scNorth = getNeighborSubChunk(0, -1);
	const LevelChunk::SubChunk* scSouth = getNeighborSubChunk(0, 1);
	const bool checkTopRow = (min.y + RENDERCHUNK_SIDE) < region.getMaxHeight();

	static bool sOpaqueCubeInit = false;
	static bool sIsFullOpaqueCube[256]{};
	if (!sOpaqueCubeInit) {
		sOpaqueCubeInit = true;
		AABB dummyShape;
		for (int bid = 1; bid < 256; ++bid) {
			const Block* blk = Block::mBlocks[bid];
			const BlockGraphics* bg = BlockGraphics::mBlocks[bid];
			if (!blk || !bg) continue;
			if (!Block::mSolid[bid] || !blk->isSolidBlockingBlock() ||
			    Block::mLightBlock[bid] < Brightness::MAX ||
			    blk->getRenderLayer() != BlockRenderLayer::RENDERLAYER_OPAQUE ||
			    bg->getBlockShape() != BlockShape::BLOCK ||
			    blk->canHaveExtraData() ||
			    blk->hasProperty(BlockProperty::Leaf) ||
			    blk->hasProperty(BlockProperty::TopSnow)) {
				continue;
			}
			const AABB& sh = blk->getVisualShape(0, dummyShape, false);
			if (sh.min.x <= 0.001f && sh.min.y <= 0.001f && sh.min.z <= 0.001f &&
			    sh.max.x >= 0.999f && sh.max.y >= 0.999f && sh.max.z >= 0.999f) {
				sIsFullOpaqueCube[bid] = true;
			}
		}
	}

	for (int lx = 0; lx < RENDERCHUNK_SIDE; ++lx) {
		const int wx = min.x + lx;
		for (int lz = 0; lz < RENDERCHUNK_SIDE; ++lz) {
			const int wz = min.z + lz;

			if (checkTopRow && (!skyLit || allDark)) {
				const size_t topIdx = static_cast<size_t>(0 | (lz << 4) | (lx << 8));
				const uint8_t topSky = scAbove ? scAbove->mSkyLight.get(topIdx) : static_cast<uint8_t>(lc->getBrightness(region.getDimension().getSkyLightLayer(), ChunkBlockPos(lx, min.y + RENDERCHUNK_SIDE, lz)));
				if (topSky > 0) skyLit = true;
				if (allDark && (topSky + (scAbove ? scAbove->mBlockLight.get(topIdx) : 0)) > 2) {
					allDark = false;
				}
			}

			for (int ly = 0; ly < RENDERCHUNK_SIDE; ++ly) {
				const size_t sbIdx = static_cast<size_t>(ly | (lz << 4) | (lx << 8));
				const BlockID id = sc->mBlocks[sbIdx];

				if (!skyLit || allDark) {
					const uint8_t s = sc->mSkyLight.get(sbIdx);
					if (s > 0) skyLit = true;
					if (allDark && (s + sc->mBlockLight.get(sbIdx)) > 2) {
						allDark = false;
					}
				}

				if (id == BlockID::AIR) {
					continue;
				}

				// Fast-cull completely buried full-opaque cubes (interior + subchunk borders) before BlockTessellator/BlockOccluder work
				if (sIsFullOpaqueCube[id]) {
					if (lx > 0 && lx < 15 && ly > 0 && ly < 15 && lz > 0 && lz < 15) {
						if (sIsFullOpaqueCube[sc->mBlocks[sbIdx - 1]] &&
						    sIsFullOpaqueCube[sc->mBlocks[sbIdx + 1]] &&
						    sIsFullOpaqueCube[sc->mBlocks[sbIdx - 16]] &&
						    sIsFullOpaqueCube[sc->mBlocks[sbIdx + 16]] &&
						    sIsFullOpaqueCube[sc->mBlocks[sbIdx - 256]] &&
						    sIsFullOpaqueCube[sc->mBlocks[sbIdx + 256]]) {
							continue;
						}
					} else {
						const BlockID nDown = (ly > 0) ? sc->mBlocks[sbIdx - 1]
							: (scBelow ? scBelow->mBlocks[static_cast<size_t>(15 | (lz << 4) | (lx << 8))] : BlockID::AIR);
						const BlockID nUp = (ly < 15) ? sc->mBlocks[sbIdx + 1]
							: (scAbove ? scAbove->mBlocks[static_cast<size_t>(0 | (lz << 4) | (lx << 8))] : BlockID::AIR);
						const BlockID nNorth = (lz > 0) ? sc->mBlocks[sbIdx - 16]
							: (scNorth ? scNorth->mBlocks[static_cast<size_t>(ly | (15 << 4) | (lx << 8))] : BlockID::AIR);
						const BlockID nSouth = (lz < 15) ? sc->mBlocks[sbIdx + 16]
							: (scSouth ? scSouth->mBlocks[static_cast<size_t>(ly | (0 << 4) | (lx << 8))] : BlockID::AIR);
						const BlockID nWest = (lx > 0) ? sc->mBlocks[sbIdx - 256]
							: (scWest ? scWest->mBlocks[static_cast<size_t>(ly | (lz << 4) | (15 << 8))] : BlockID::AIR);
						const BlockID nEast = (lx < 15) ? sc->mBlocks[sbIdx + 256]
							: (scEast ? scEast->mBlocks[static_cast<size_t>(ly | (lz << 4) | (0 << 8))] : BlockID::AIR);
						if (sIsFullOpaqueCube[nDown] && sIsFullOpaqueCube[nUp] &&
						    sIsFullOpaqueCube[nNorth] && sIsFullOpaqueCube[nSouth] &&
						    sIsFullOpaqueCube[nWest] && sIsFullOpaqueCube[nEast]) {
							continue;
						}
					}
				}

				const BlockPos p(wx, min.y + ly, wz);
				const Block& block = *Block::mBlocks[id];
				if (VE) {
					VE->setBlock(p, &block);
				}

				BlockRenderLayer renderLayer = block.getRenderLayer(region, p);
				if (block.hasProperty(BlockProperty::Leaf)) {
					const bool isSeasonsTinted = (renderLayer == BlockRenderLayer::RENDERLAYER_SEASONS_OPAQUE || renderLayer == BlockRenderLayer::RENDERLAYER_SEASONS_OPTIONAL_ALPHATEST);
					const int leafType = Block::mLeaves->getBlockState(BlockState::MappedType).get<int>(sc->mData.get(sbIdx));
					if (leafType == enum_cast(OldLeafBlock::LeafType::Jungle)) {
						renderLayer = transparentLeaves && isSeasonsTinted ? BlockRenderLayer::RENDERLAYER_SEASONS_OPAQUE : BlockRenderLayer::RENDERLAYER_OPAQUE;
					}
					else if (LeafBlock::isDeepLeafBlock(region, p)) {
						renderLayer = transparentLeaves && isSeasonsTinted ? BlockRenderLayer::RENDERLAYER_SEASONS_OPAQUE : BlockRenderLayer::RENDERLAYER_OPAQUE;
					}
				}
				else if (block.canHaveExtraData()) {
					const uint16_t extraData = region.getExtraData(p);
					if (extraData != 0) {
						auto& pBlock = Block::mBlocks[extraData & 0xFF];
						if (pBlock != nullptr) {
							mQueues[pBlock->getRenderLayer()].add(p);
						}
					}
				}

				const int extraLayers = block.getExtraRenderLayers();
				if (extraLayers != 0) {
					for (int i = 0; i < enum_cast(BlockRenderLayer::_RENDERLAYER_COUNT); i++) {
						if ((extraLayers & (1 << i)) != 0) {
							mQueues[i].add(p);
						}
					}
				}

				mQueues[renderLayer].add(p);
			}
		}
	}

	return true;
}

bool RenderChunkBuilder::_tessellateQueues(RenderChunk& rc, BlockSource& region) {
	auto& t = renderer->getTessellator();
	bool cacheReset = false;

	const BlockPos rcPos = rc.getPosition();
	LevelChunk* lc = region.getChunk(rcPos);
	const size_t subY = static_cast<size_t>(rcPos.y >> 4);
	const auto subs = lc ? lc->getSubChunks() : buffer_span<std::unique_ptr<LevelChunk::SubChunk>>{};
	const LevelChunk::SubChunk* sc = (subY < subs.size()) ? subs[subY].get() : nullptr;

	for (int l = 0; l < BlockRenderLayer::_RENDERLAYER_COUNT; ++l) {

		if (!mQueues[l].empty()) {
			if (!t.isTessellating()) {	//lazy-begin tessellating
 				t.begin(12000);	//made up average
				t.setOffset(-rcPos);
			}
			if (!cacheReset) {
				renderer->resetCache(rcPos, region);
				cacheReset = true;
			}

			auto existingVertices = t.getVertexCount();

			renderer->setRenderLayer(l);
			renderer->setForceOpaque(l == BlockRenderLayer::RENDERLAYER_DOUBLE_SIDED);

			for (BlockPos& pos : mQueues[l]) {
				FullBlock fullBlock = sc
					? FullBlock(sc->mBlocks[static_cast<size_t>((pos.y & 15) | ((pos.z & 15) << 4) | ((pos.x & 15) << 8))],
					            sc->mData.get(static_cast<size_t>((pos.y & 15) | ((pos.z & 15) << 4) | ((pos.x & 15) << 8))))
					: region.getBlockAndData(pos);
				const Block* block = Block::mBlocks[fullBlock.id];
				renderer->tessellateInWorld(*block, pos, fullBlock.data);
			}
			renderer->setForceOpaque(false);

			mQueues[l].setIndexCount((t.getVertexCount() - existingVertices) / 4 * 6);
		}
	}

	return true;
}

void RenderChunkBuilder::_buildRanges() {

	//calculate the "near" index ranges
	memset(mTerrainLayerRange, 0, sizeof(mTerrainLayerRange));
	memset(mRenderLayerRange, 0, sizeof(mRenderLayerRange));

	//WARNING: the order in which _setRenderLayerRange is called is *VERY* important and depends on the ordering of the renderlayers!!
	int offset = 0;
	offset = _setRenderLayerRange(BlockRenderLayer::RENDERLAYER_DOUBLE_SIDED, offset, mQueues[BlockRenderLayer::RENDERLAYER_DOUBLE_SIDED].getIndexCount());
	offset = _setRenderLayerRange(BlockRenderLayer::RENDERLAYER_BLEND, offset, mQueues[BlockRenderLayer::RENDERLAYER_BLEND].getIndexCount());
	offset = _setRenderLayerRange(BlockRenderLayer::RENDERLAYER_WATER, offset, mQueues[BlockRenderLayer::RENDERLAYER_WATER].getIndexCount());
	offset = _setRenderLayerRange(BlockRenderLayer::RENDERLAYER_OPAQUE, offset, mQueues[BlockRenderLayer::RENDERLAYER_OPAQUE].getIndexCount());
	offset = _setRenderLayerRange(BlockRenderLayer::RENDERLAYER_LAVA, offset, mQueues[BlockRenderLayer::RENDERLAYER_LAVA].getIndexCount());
	offset = _setRenderLayerRange(BlockRenderLayer::RENDERLAYER_ENDPORTAL, offset, mQueues[BlockRenderLayer::RENDERLAYER_ENDPORTAL].getIndexCount());
	offset = _setRenderLayerRange(BlockRenderLayer::RENDERLAYER_OPTIONAL_ALPHATEST, offset, mQueues[BlockRenderLayer::RENDERLAYER_OPTIONAL_ALPHATEST].getIndexCount());
	offset = _setRenderLayerRange(BlockRenderLayer::RENDERLAYER_ALPHATEST, offset, mQueues[BlockRenderLayer::RENDERLAYER_ALPHATEST].getIndexCount());
	offset = _setRenderLayerRange(BlockRenderLayer::RENDERLAYER_SEASONS_OPAQUE, offset, mQueues[BlockRenderLayer::RENDERLAYER_SEASONS_OPAQUE].getIndexCount());
	offset = _setRenderLayerRange(BlockRenderLayer::RENDERLAYER_SEASONS_OPTIONAL_ALPHATEST, offset, mQueues[BlockRenderLayer::RENDERLAYER_SEASONS_OPTIONAL_ALPHATEST].getIndexCount());
	offset = _setRenderLayerRange(BlockRenderLayer::RENDERLAYER_ALPHATEST_SINGLE_SIDE, offset, mQueues[BlockRenderLayer::RENDERLAYER_ALPHATEST_SINGLE_SIDE].getIndexCount());

	mRenderChunkSorter.setupIndexRanges(mTerrainLayerRange, mRenderLayerRange);
}


VisibilityExtimator* RenderChunkBuilder::_getVisibilityHelper() {
	// Visibility graph (canSeeChunkLookingFrom) is unused; frustum culling handles chunk visibility directly
	return nullptr;
}

void RenderChunkBuilder::build(RenderChunk& rc, bool transparentLeaves, bool smoothLighting) {

	//create a new BlockSource
	BlockSource region(localSource->getLevel(), localSource->getDimension(), *localSource, false);

	DEBUG_ASSERT(rc.isBuildState(RenderChunk::BuildState::Building), "Wrong state, wrong thread");

	if (auto lc = region.getChunk(rc.getCenter())) {
		lc->updateCachedData(region);
	}

	mVisibility = VisibilityNode(true);
	//get our local visibility extimator if needed
	auto VE = _getVisibilityHelper();
	if (VE) {
		VE->start(rc);
	}

	{
		renderer->setRegion(region);
		renderer->setSmoothLightingEnabled(smoothLighting);
		skyLit = _seesSkyDirectly(rc, region);
		allDark = true;
 		mQueues = queuePool.getLocal().data();

		for (int i = 0; i < BlockRenderLayer::_RENDERLAYER_COUNT; ++i) {
			mQueues[i].reset();
		}
	}

	if (!_sortBlocks(region, rc, VE, transparentLeaves)) {
		return;
	}

	averageSkyLight = skyLit ? 15.f : 0;

	if (!_tessellateQueues(rc, region)) {
		return;
	}

	_buildRanges();

	if (VE) {
		mVisibility = VE->finish();
	}
}
