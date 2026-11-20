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
#include "world/level/biome/Biome.h"
#include "world/level/block/GrassBlock.h"
#include "world/level/block/LiquidBlock.h"
#include "world/level/material/Material.h"

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

namespace {
	const bool* getFullOpaqueCubeTable() {
		static std::once_flag sOpaqueCubeOnce;
		static bool sIsFullOpaqueCube[256]{};
		std::call_once(sOpaqueCubeOnce, []() {
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
		});
		return sIsFullOpaqueCube;
	}

	inline DataID normalizeBlockDataForLod(BlockID id, DataID data, uint8_t lodLevel) {
		if (lodLevel < 2) return data;
		// Ignore orientation metadata on logs, hay, quartz pillars, furnaces, pumpkins, dispensers
		if (id == 17 || id == 162) return static_cast<DataID>(data & 3);
		if (id == 170) return 0;
		if (id == 155 && data >= 2) return 2;
		if (id == 61 || id == 62 || id == 86 || id == 91 || id == 23 || id == 158) return 0;
		return data;
	}

	inline bool isSmallClutterShape(BlockShape shape) {
		return shape == BlockShape::CROSS_TEXTURE ||
		       shape == BlockShape::CROSS_TEXTURE_POLY ||
		       shape == BlockShape::DOUBLE_PLANT ||
		       shape == BlockShape::DOUBLE_PLANT_POLY ||
		       shape == BlockShape::TORCH ||
		       shape == BlockShape::STEM ||
		       shape == BlockShape::ROWS;
	}

	struct GreedyCell {
		bool valid = false;
		BlockID id = BlockID::AIR;
		DataID normData = 0;
		uint8_t skyLight = 15;
		uint8_t blkLight = 0;
		uint32_t rgbPacked = 0xFFFFFF;
		TextureUVCoordinateSet tex;
	};

	inline void emitGreedyQuad(Tessellator& t, int face, float aX, float aY, float aZ,
	                           float bX, float bY, float bZ,
	                           const TextureUVCoordinateSet& tex,
	                           uint8_t r, uint8_t g, uint8_t b, uint8_t a,
	                           uint8_t skyLight, uint8_t blkLight, bool merged) {
		t.quadFacing(static_cast<FacingID>(face));
		t.tex1(static_cast<float>(blkLight) / 16.0f, static_cast<float>(skyLight) / 16.0f);
		// Encode (10 + face) in alpha for merged greedy quads so alpha==0 from LeafBlock::getColor() never collides.
		t.color(r, g, b, merged ? static_cast<byte>(10 + face) : static_cast<byte>(a));

		const float u0 = merged ? tex._u0 : tex._u0;
		const float v0 = merged ? tex._v0 : tex._v0;
		const float u1 = merged ? tex._u0 : tex._u1;
		const float v1 = merged ? tex._v0 : tex._v1;

		switch (face) {
		case Facing::DOWN:
			t.vertexUV(aX, aY, bZ, u0, v1);
			t.vertexUV(aX, aY, aZ, u0, v0);
			t.vertexUV(bX, aY, aZ, u1, v0);
			t.vertexUV(bX, aY, bZ, u1, v1);
			break;
		case Facing::UP:
			t.vertexUV(bX, bY, bZ, u1, v1);
			t.vertexUV(bX, bY, aZ, u1, v0);
			t.vertexUV(aX, bY, aZ, u0, v0);
			t.vertexUV(aX, bY, bZ, u0, v1);
			break;
		case Facing::NORTH:
			t.vertexUV(aX, bY, aZ, u1, v0);
			t.vertexUV(bX, bY, aZ, u0, v0);
			t.vertexUV(bX, aY, aZ, u0, v1);
			t.vertexUV(aX, aY, aZ, u1, v1);
			break;
		case Facing::SOUTH:
			t.vertexUV(aX, bY, bZ, u0, v0);
			t.vertexUV(aX, aY, bZ, u0, v1);
			t.vertexUV(bX, aY, bZ, u1, v1);
			t.vertexUV(bX, bY, bZ, u1, v0);
			break;
		case Facing::WEST:
			t.vertexUV(aX, bY, bZ, u1, v0);
			t.vertexUV(aX, bY, aZ, u0, v0);
			t.vertexUV(aX, aY, aZ, u0, v1);
			t.vertexUV(aX, aY, bZ, u1, v1);
			break;
		case Facing::EAST:
		default:
			t.vertexUV(bX, aY, bZ, u0, v1);
			t.vertexUV(bX, aY, aZ, u1, v1);
			t.vertexUV(bX, bY, aZ, u1, v0);
			t.vertexUV(bX, bY, bZ, u0, v0);
			break;
		}
	}
}

bool RenderChunkBuilder::_sortBlocks(BlockSource& region, RenderChunk& rc, VisibilityExtimator* VE, bool transparentLeaves, uint8_t lodLevel) {
	mHasGreedyOpaque = false;
	mHasGreedyWater = false;
	mNeedsBlockTessCache = false;

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

	const bool* sIsFullOpaqueCube = getFullOpaqueCubeTable();

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

				auto fetchNeighbors = [&](BlockID& nDown, BlockID& nUp, BlockID& nNorth, BlockID& nSouth, BlockID& nWest, BlockID& nEast) {
					if (lx > 0 && lx < 15 && ly > 0 && ly < 15 && lz > 0 && lz < 15) {
						nDown = sc->mBlocks[sbIdx - 1];
						nUp = sc->mBlocks[sbIdx + 1];
						nNorth = sc->mBlocks[sbIdx - 16];
						nSouth = sc->mBlocks[sbIdx + 16];
						nWest = sc->mBlocks[sbIdx - 256];
						nEast = sc->mBlocks[sbIdx + 256];
					} else {
						nDown = (ly > 0) ? sc->mBlocks[sbIdx - 1]
							: (scBelow ? scBelow->mBlocks[static_cast<size_t>(15 | (lz << 4) | (lx << 8))] : BlockID::AIR);
						nUp = (ly < 15) ? sc->mBlocks[sbIdx + 1]
							: (scAbove ? scAbove->mBlocks[static_cast<size_t>(0 | (lz << 4) | (lx << 8))] : BlockID::AIR);
						nNorth = (lz > 0) ? sc->mBlocks[sbIdx - 16]
							: (scNorth ? scNorth->mBlocks[static_cast<size_t>(ly | (15 << 4) | (lx << 8))] : BlockID::AIR);
						nSouth = (lz < 15) ? sc->mBlocks[sbIdx + 16]
							: (scSouth ? scSouth->mBlocks[static_cast<size_t>(ly | (0 << 4) | (lx << 8))] : BlockID::AIR);
						nWest = (lx > 0) ? sc->mBlocks[sbIdx - 256]
							: (scWest ? scWest->mBlocks[static_cast<size_t>(ly | (lz << 4) | (15 << 8))] : BlockID::AIR);
						nEast = (lx < 15) ? sc->mBlocks[sbIdx + 256]
							: (scEast ? scEast->mBlocks[static_cast<size_t>(ly | (lz << 4) | (0 << 8))] : BlockID::AIR);
					}
				};

				// Fast-cull completely buried full-opaque cubes (interior + subchunk borders)
				if (sIsFullOpaqueCube[id]) {
					BlockID nDown, nUp, nNorth, nSouth, nWest, nEast;
					fetchNeighbors(nDown, nUp, nNorth, nSouth, nWest, nEast);
					if (sIsFullOpaqueCube[nDown] && sIsFullOpaqueCube[nUp] &&
					    sIsFullOpaqueCube[nNorth] && sIsFullOpaqueCube[nSouth] &&
					    sIsFullOpaqueCube[nWest] && sIsFullOpaqueCube[nEast]) {
						continue;
					}
					if (lodLevel >= 1) {
						// Handled by Greedy Meshing in RENDERLAYER_OPAQUE
						mHasGreedyOpaque = true;
						if (VE) VE->setBlock(BlockPos(wx, min.y + ly, wz), Block::mBlocks[id]);
						continue;
					}
				}

				// LOD 2+: Cull interior leaf blocks (leaf-on-leaf) and skip far decorative micro-clutter
				if (lodLevel >= 2) {
					const Block* blk = Block::mBlocks[id];
					const BlockGraphics* bg = BlockGraphics::mBlocks[id];
					if (bg && isSmallClutterShape(bg->getBlockShape())) {
						continue;
					}
					if (blk && blk->hasProperty(BlockProperty::Leaf)) {
						BlockID nDown, nUp, nNorth, nSouth, nWest, nEast;
						fetchNeighbors(nDown, nUp, nNorth, nSouth, nWest, nEast);
						auto isLeafOrOpaque = [&](BlockID nid) -> bool {
							if (sIsFullOpaqueCube[nid]) return true;
							const Block* nb = Block::mBlocks[nid];
							return nb && nb->hasProperty(BlockProperty::Leaf);
						};
						if (isLeafOrOpaque(nDown) && isLeafOrOpaque(nUp) &&
						    isLeafOrOpaque(nNorth) && isLeafOrOpaque(nSouth) &&
						    isLeafOrOpaque(nWest) && isLeafOrOpaque(nEast)) {
							continue;
						}
					}
					if ((id == 8 || id == 9) && (sc->mData.get(sbIdx) & 7) == 0) {
						BlockID nDown, nUp, nNorth, nSouth, nWest, nEast;
						fetchNeighbors(nDown, nUp, nNorth, nSouth, nWest, nEast);
						auto isWaterOrOpaque = [&](BlockID nid) -> bool {
							return nid == 8 || nid == 9 || sIsFullOpaqueCube[nid];
						};
						if (nUp != 8 && nUp != 9 && !sIsFullOpaqueCube[nUp] &&
						    nDown != BlockID::AIR &&
						    isWaterOrOpaque(nNorth) && isWaterOrOpaque(nSouth) &&
						    isWaterOrOpaque(nWest) && isWaterOrOpaque(nEast)) {
							// Flat ocean/lake surface block: merged by Greedy Water Mesher
							mHasGreedyWater = true;
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
							mNeedsBlockTessCache = true;
						}
					}
				}

				const int extraLayers = block.getExtraRenderLayers();
				if (extraLayers != 0) {
					for (int i = 0; i < enum_cast(BlockRenderLayer::_RENDERLAYER_COUNT); i++) {
						if ((extraLayers & (1 << i)) != 0) {
							mQueues[i].add(p);
							mNeedsBlockTessCache = true;
						}
					}
				}

				mQueues[renderLayer].add(p);
				mNeedsBlockTessCache = true;
			}
		}
	}

	return true;
}

void RenderChunkBuilder::_greedyMeshOpaque(RenderChunk& rc, BlockSource& region, uint8_t lodLevel) {
	const BlockPos rcPos = rc.getPosition();
	LevelChunk* lc = region.getChunk(rcPos);
	if (!lc) return;
	const size_t subY = static_cast<size_t>(rcPos.y >> 4);
	const auto subs = lc->getSubChunks();
	const LevelChunk::SubChunk* sc = (subY < subs.size()) ? subs[subY].get() : nullptr;
	if (!sc || !sc->hasBlocks()) return;

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

	const bool* sIsFullOpaqueCube = getFullOpaqueCubeTable();
	const int maxH = region.getMaxHeight();
	auto& t = renderer->getTessellator();

	auto sampleNeighborBlockAndLight = [&](int lx, int ly, int lz, int face, BlockID& outId, uint8_t& outSky, uint8_t& outBlk) {
		static const int kDx[6] = { 0, 0, 0, 0, -1, 1 };
		static const int kDy[6] = { -1, 1, 0, 0, 0, 0 };
		static const int kDz[6] = { 0, 0, -1, 1, 0, 0 };
		int nx = lx + kDx[face];
		int ny = ly + kDy[face];
		int nz = lz + kDz[face];
		const LevelChunk::SubChunk* nsc = sc;
		if (ny < 0) {
			if (rcPos.y + ny < 0) {
				outId = BlockID::AIR; outSky = 0; outBlk = 0; return;
			}
			nsc = scBelow;
			ny = 15;
		} else if (ny >= 16) {
			if (rcPos.y + ny >= maxH) {
				outId = BlockID::AIR; outSky = 15; outBlk = 0; return;
			}
			nsc = scAbove;
			ny = 0;
		} else if (nx < 0) {
			nsc = scWest; nx = 15;
		} else if (nx >= 16) {
			nsc = scEast; nx = 0;
		} else if (nz < 0) {
			nsc = scNorth; nz = 15;
		} else if (nz >= 16) {
			nsc = scSouth; nz = 0;
		}
		if (!nsc) {
			outId = BlockID::AIR; outSky = 15; outBlk = 0;
			return;
		}
		const size_t nIdx = static_cast<size_t>(ny | (nz << 4) | (nx << 8));
		outId = nsc->mBlocks[nIdx];
		outSky = nsc->mSkyLight.get(nIdx);
		outBlk = nsc->mBlockLight.get(nIdx);
	};

	std::array<GreedyCell, 256> grid;
	std::array<uint8_t, 256> used;
	const float faceShade[6] = { 0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f };
	const bool ignoreLightDiff = (lodLevel >= 3);
	const bool ignoreColorVariation = (lodLevel >= 3);
	const bool ignoreUvRotation = (lodLevel >= 2);
	const uint8_t grassId = Block::mGrass ? Block::mGrass->mID : 2;

	for (int face = 0; face < 6; ++face) {
		for (int plane = 0; plane < RENDERCHUNK_SIDE; ++plane) {
			for (auto& cell : grid) cell.valid = false;
			bool anyInPlane = false;

			for (int v = 0; v < RENDERCHUNK_SIDE; ++v) {
				for (int u = 0; u < RENDERCHUNK_SIDE; ++u) {
					int lx, ly, lz;
					if (face <= 1) {
						lx = u; ly = plane; lz = v;
					} else if (face <= 3) {
						lx = u; ly = v; lz = plane;
					} else {
						lx = plane; ly = v; lz = u;
					}

					const size_t sbIdx = static_cast<size_t>(ly | (lz << 4) | (lx << 8));
					const BlockID id = sc->mBlocks[sbIdx];
					if (!sIsFullOpaqueCube[id]) continue;

					BlockID nid;
					uint8_t skyL, blkL;
					sampleNeighborBlockAndLight(lx, ly, lz, face, nid, skyL, blkL);
					if (sIsFullOpaqueCube[nid]) continue;

					const uint8_t emit = static_cast<uint8_t>(Block::mLightEmission[id]);
					if (emit > blkL) blkL = emit;

					// Progressive light quantization by LOD level while preserving full daylight brightness (15 -> 15)
					uint8_t quantSky = skyL;
					uint8_t quantBlk = blkL;
					if (lodLevel == 1) {
						quantSky = (skyL >= 14) ? 15 : static_cast<uint8_t>(skyL & ~1u);
						quantBlk = (blkL >= 14) ? 15 : static_cast<uint8_t>(blkL & ~1u);
					} else if (lodLevel == 2) {
						quantSky = (skyL >= 13) ? 15 : (skyL >= 9 ? 11 : (skyL >= 5 ? 7 : (skyL >= 1 ? 3 : 0)));
						quantBlk = (blkL >= 13) ? 15 : (blkL >= 9 ? 11 : (blkL >= 5 ? 7 : (blkL >= 1 ? 3 : 0)));
					}

					const Block& block = *Block::mBlocks[id];
					const DataID data = sc->mData.get(sbIdx);
					const DataID normData = normalizeBlockDataForLod(id, data, lodLevel);
					const BlockPos wp(rcPos.x + lx, rcPos.y + ly, rcPos.z + lz);

					const int variant = (lodLevel >= 2 && id != grassId)
						? block.getVariant(normData)
						: block.calcVariant(region, wp, data);
					const TextureUVCoordinateSet& tex = BlockGraphics::mBlocks[id]->getTexture(static_cast<FacingID>(face), variant);

					uint32_t argb = 0xFFFFFFFFu;
					if (id == grassId) {
						if (face == Facing::UP) {
							argb = static_cast<uint32_t>(block.getColor(region, wp, data)) | 0xFF000000u;
						}
					} else if (id != 1 && id != 3 && id != 4 && id != 12 && id != 13 && id != 24) {
						argb = static_cast<uint32_t>(block.getColor(region, wp, data)) | 0xFF000000u;
					}

					if (ignoreColorVariation && argb != 0xFFFFFFFFu) {
						// Quantize biome tint in LOD 3 by rounding (never darkening white/bright colors)
						auto quantCh = [](uint32_t c) -> uint32_t {
							return std::min<uint32_t>(255u, (c + 16u) & 0xE0u);
						};
						const uint32_t cr = quantCh((argb >> 16) & 0xFFu);
						const uint32_t cg = quantCh((argb >> 8) & 0xFFu);
						const uint32_t cb = quantCh(argb & 0xFFu);
						argb = 0xFF000000u | (cr << 16) | (cg << 8) | cb;
					}

					GreedyCell& cell = grid[v * RENDERCHUNK_SIDE + u];
					cell.valid = true;
					cell.id = id;
					cell.normData = normData;
					cell.skyLight = quantSky;
					cell.blkLight = quantBlk;
					cell.rgbPacked = argb;
					cell.tex = tex;
					anyInPlane = true;
				}
			}

			if (!anyInPlane) continue;
			used.fill(0);

			auto sameCell = [&](const GreedyCell& a, const GreedyCell& b) -> bool {
				if (!b.valid || a.id != b.id || a.normData != b.normData) return false;
				if (!ignoreLightDiff) {
					if (a.skyLight != b.skyLight || a.blkLight != b.blkLight) return false;
				} else {
					// Keep pitch-dark caves separate from sunlit terrain
					const bool aLit = (a.skyLight > 0 || a.blkLight > 0);
					const bool bLit = (b.skyLight > 0 || b.blkLight > 0);
					if (aLit != bLit) return false;
				}
				if (a.rgbPacked != b.rgbPacked) return false;
				if (!ignoreUvRotation) {
					if (a.tex._u0 != b.tex._u0 || a.tex._v0 != b.tex._v0) return false;
				}
				return true;
			};

			for (int v = 0; v < RENDERCHUNK_SIDE; ++v) {
				for (int u = 0; u < RENDERCHUNK_SIDE; ++u) {
					const int gIdx = v * RENDERCHUNK_SIDE + u;
					if (used[gIdx] || !grid[gIdx].valid) continue;

					const GreedyCell& seed = grid[gIdx];
					int w = 1;
					while (u + w < RENDERCHUNK_SIDE) {
						const int qi = v * RENDERCHUNK_SIDE + (u + w);
						if (used[qi] || !sameCell(seed, grid[qi])) break;
						++w;
					}

					int h = 1;
					while (v + h < RENDERCHUNK_SIDE) {
						bool ok = true;
						for (int du = 0; du < w; ++du) {
							const int qi = (v + h) * RENDERCHUNK_SIDE + (u + du);
							if (used[qi] || !sameCell(seed, grid[qi])) {
								ok = false;
								break;
							}
						}
						if (!ok) break;
						++h;
					}

					uint8_t quadSky = seed.skyLight;
					uint8_t quadBlk = seed.blkLight;
					for (int dv = 0; dv < h; ++dv) {
						for (int du = 0; du < w; ++du) {
							const int mi = (v + dv) * RENDERCHUNK_SIDE + (u + du);
							used[mi] = 1;
							if (ignoreLightDiff) {
								quadSky = std::max(quadSky, grid[mi].skyLight);
								quadBlk = std::max(quadBlk, grid[mi].blkLight);
							}
						}
					}

					float aX, aY, aZ, bX, bY, bZ;
					if (face <= 1) {
						aX = static_cast<float>(rcPos.x + u);
						bX = aX + static_cast<float>(w);
						aY = static_cast<float>(rcPos.y + plane);
						bY = aY + 1.0f;
						aZ = static_cast<float>(rcPos.z + v);
						bZ = aZ + static_cast<float>(h);
					} else if (face <= 3) {
						aX = static_cast<float>(rcPos.x + u);
						bX = aX + static_cast<float>(w);
						aY = static_cast<float>(rcPos.y + v);
						bY = aY + static_cast<float>(h);
						aZ = static_cast<float>(rcPos.z + plane);
						bZ = aZ + 1.0f;
					} else {
						aX = static_cast<float>(rcPos.x + plane);
						bX = aX + 1.0f;
						aY = static_cast<float>(rcPos.y + v);
						bY = aY + static_cast<float>(h);
						aZ = static_cast<float>(rcPos.z + u);
						bZ = aZ + static_cast<float>(w);
					}

					const float gamma = Block::mBlocks[seed.id]->getBrightnessGamma();
					const float baseShade = faceShade[face];
					const float shade = (gamma == 1.0f) ? baseShade : std::pow(baseShade, gamma);
					const uint8_t cr = static_cast<uint8_t>(std::min(255, static_cast<int>(((seed.rgbPacked >> 16) & 0xFFu) * shade)));
					const uint8_t cg = static_cast<uint8_t>(std::min(255, static_cast<int>(((seed.rgbPacked >> 8) & 0xFFu) * shade)));
					const uint8_t cb = static_cast<uint8_t>(std::min(255, static_cast<int>((seed.rgbPacked & 0xFFu) * shade)));

					emitGreedyQuad(t, face, aX, aY, aZ, bX, bY, bZ,
					               seed.tex, cr, cg, cb, 255,
					               quadSky, quadBlk, (w > 1 || h > 1));
				}
			}
		}
	}
}

void RenderChunkBuilder::_greedyMeshWater(RenderChunk& rc, BlockSource& region, uint8_t lodLevel) {
	const BlockPos rcPos = rc.getPosition();
	LevelChunk* lc = region.getChunk(rcPos);
	if (!lc) return;
	const size_t subY = static_cast<size_t>(rcPos.y >> 4);
	const auto subs = lc->getSubChunks();
	const LevelChunk::SubChunk* sc = (subY < subs.size()) ? subs[subY].get() : nullptr;
	if (!sc || !sc->hasBlocks()) return;

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
	const bool* sIsFullOpaqueCube = getFullOpaqueCubeTable();
	auto& t = renderer->getTessellator();
	const Block* waterBlk = Block::mStillWater ? Block::mStillWater : Block::mBlocks[9];
	const TextureUVCoordinateSet& waterTex = BlockGraphics::mBlocks[9]->getTexture(Facing::UP, 0);

	std::array<GreedyCell, 256> grid;
	std::array<uint8_t, 256> used;
	const bool ignoreLightDiff = (lodLevel >= 3);
	const bool ignoreColorVariation = (lodLevel >= 3);

	for (int ly = 0; ly < RENDERCHUNK_SIDE; ++ly) {
		for (auto& cell : grid) cell.valid = false;
		bool anyWater = false;

		for (int lz = 0; lz < RENDERCHUNK_SIDE; ++lz) {
			for (int lx = 0; lx < RENDERCHUNK_SIDE; ++lx) {
				const size_t sbIdx = static_cast<size_t>(ly | (lz << 4) | (lx << 8));
				const BlockID id = sc->mBlocks[sbIdx];
				if ((id != 8 && id != 9) || (sc->mData.get(sbIdx) & 7) != 0) continue;

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
				auto isWaterOrOpaque = [&](BlockID nid) -> bool {
					return nid == 8 || nid == 9 || sIsFullOpaqueCube[nid];
				};
				if (nUp == 8 || nUp == 9 || sIsFullOpaqueCube[nUp] || nDown == BlockID::AIR ||
				    !isWaterOrOpaque(nNorth) || !isWaterOrOpaque(nSouth) ||
				    !isWaterOrOpaque(nWest) || !isWaterOrOpaque(nEast)) {
					continue;
				}

				uint8_t skyL = 15, blkL = 0;
				if (ly < 15) {
					skyL = sc->mSkyLight.get(sbIdx + 1);
					blkL = sc->mBlockLight.get(sbIdx + 1);
				} else if (scAbove) {
					const size_t aIdx = static_cast<size_t>(0 | (lz << 4) | (lx << 8));
					skyL = scAbove->mSkyLight.get(aIdx);
					blkL = scAbove->mBlockLight.get(aIdx);
				}
				if (lodLevel == 2) {
					skyL = (skyL >= 13) ? 15 : (skyL >= 9 ? 11 : (skyL >= 5 ? 7 : (skyL >= 1 ? 3 : 0)));
					blkL = (blkL >= 13) ? 15 : (blkL >= 9 ? 11 : (blkL >= 5 ? 7 : (blkL >= 1 ? 3 : 0)));
				}

				const BlockPos wp(rcPos.x + lx, rcPos.y + ly, rcPos.z + lz);
				uint32_t argb = waterBlk ? (static_cast<uint32_t>(waterBlk->getColor(region, wp, 0)) | 0xFF000000u) : 0xFFFFFFFFu;
				if (ignoreColorVariation && argb != 0xFFFFFFFFu) {
					auto quantCh = [](uint32_t c) -> uint32_t {
						return std::min<uint32_t>(255u, (c + 16u) & 0xE0u);
					};
					argb = 0xFF000000u | (quantCh((argb >> 16) & 0xFFu) << 16) | (quantCh((argb >> 8) & 0xFFu) << 8) | quantCh(argb & 0xFFu);
				}

				GreedyCell& cell = grid[lz * RENDERCHUNK_SIDE + lx];
				cell.valid = true;
				cell.id = BlockID(9);
				cell.skyLight = skyL;
				cell.blkLight = blkL;
				cell.rgbPacked = argb;
				cell.tex = waterTex;
				anyWater = true;
			}
		}

		if (!anyWater) continue;
		used.fill(0);

		for (int v = 0; v < RENDERCHUNK_SIDE; ++v) {
			for (int u = 0; u < RENDERCHUNK_SIDE; ++u) {
				const int gIdx = v * RENDERCHUNK_SIDE + u;
				if (used[gIdx] || !grid[gIdx].valid) continue;
				const GreedyCell& seed = grid[gIdx];
				auto sameWater = [&](const GreedyCell& b) -> bool {
					if (!b.valid) return false;
					if (!ignoreLightDiff && (seed.skyLight != b.skyLight || seed.blkLight != b.blkLight)) return false;
					return seed.rgbPacked == b.rgbPacked;
				};

				int w = 1;
				while (u + w < RENDERCHUNK_SIDE && !used[v * RENDERCHUNK_SIDE + (u + w)] && sameWater(grid[v * RENDERCHUNK_SIDE + (u + w)])) {
					++w;
				}
				int h = 1;
				while (v + h < RENDERCHUNK_SIDE) {
					bool ok = true;
					for (int du = 0; du < w; ++du) {
						const int qi = (v + h) * RENDERCHUNK_SIDE + (u + du);
						if (used[qi] || !sameWater(grid[qi])) {
							ok = false;
							break;
						}
					}
					if (!ok) break;
					++h;
				}
				uint8_t quadSky = seed.skyLight;
				uint8_t quadBlk = seed.blkLight;
				for (int dv = 0; dv < h; ++dv) {
					for (int du = 0; du < w; ++du) {
						const int mi = (v + dv) * RENDERCHUNK_SIDE + (u + du);
						used[mi] = 1;
						if (ignoreLightDiff) {
							quadSky = std::max(quadSky, grid[mi].skyLight);
							quadBlk = std::max(quadBlk, grid[mi].blkLight);
						}
					}
				}

				const float aX = static_cast<float>(rcPos.x + u);
				const float bX = aX + static_cast<float>(w);
				const float aY = static_cast<float>(rcPos.y + ly);
				const float bY = aY + (8.0f / 9.0f);
				const float aZ = static_cast<float>(rcPos.z + v);
				const float bZ = aZ + static_cast<float>(h);

				const uint8_t cr = static_cast<uint8_t>((seed.rgbPacked >> 16) & 0xFFu);
				const uint8_t cg = static_cast<uint8_t>((seed.rgbPacked >> 8) & 0xFFu);
				const uint8_t cb = static_cast<uint8_t>(seed.rgbPacked & 0xFFu);
				emitGreedyQuad(t, Facing::UP, aX, aY, aZ, bX, bY, bZ,
				               seed.tex, cr, cg, cb, 204,
				               quadSky, quadBlk, (w > 1 || h > 1));
			}
		}
	}
}

bool RenderChunkBuilder::_tessellateQueues(RenderChunk& rc, BlockSource& region, uint8_t lodLevel) {
	auto& t = renderer->getTessellator();
	bool cacheReset = false;

	const BlockPos rcPos = rc.getPosition();
	LevelChunk* lc = region.getChunk(rcPos);
	const size_t subY = static_cast<size_t>(rcPos.y >> 4);
	const auto subs = lc ? lc->getSubChunks() : buffer_span<std::unique_ptr<LevelChunk::SubChunk>>{};
	const LevelChunk::SubChunk* sc = (subY < subs.size()) ? subs[subY].get() : nullptr;

	for (int l = 0; l < BlockRenderLayer::_RENDERLAYER_COUNT; ++l) {
		const bool greedyOpaqueHere = (l == BlockRenderLayer::RENDERLAYER_OPAQUE && lodLevel >= 1 && mHasGreedyOpaque);
		const bool greedyWaterHere = (l == BlockRenderLayer::RENDERLAYER_WATER && lodLevel >= 2 && mHasGreedyWater);

		if (!mQueues[l].empty() || greedyOpaqueHere || greedyWaterHere) {
			if (!t.isTessellating()) {	//lazy-begin tessellating
 				t.begin(12000);	//made up average
				t.setOffset(-rcPos);
			}

			auto existingVertices = t.getVertexCount();

			if (greedyOpaqueHere) {
				_greedyMeshOpaque(rc, region, lodLevel);
			}
			if (greedyWaterHere) {
				_greedyMeshWater(rc, region, lodLevel);
			}

			if (!mQueues[l].empty()) {
				if (!cacheReset) {
					renderer->resetCache(rcPos, region);
					cacheReset = true;
				}
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
			}

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

	// Pre-sort quad indices by layer & facing bucket directly in one linear scan of quadFacingList
	mSortedIndices16.clear();
	mSortedIndices32.clear();
	memset(mSortedRenderLayerRange, 0, sizeof(mSortedRenderLayerRange));
	memset(mSortedTerrainLayerRange, 0, sizeof(mSortedTerrainLayerRange));

	const auto& faces = renderer->getTessellator().getFaces();
	mSortedNumFaces = static_cast<uint32_t>(faces.size());
	if (mSortedNumFaces == 0) {
		mSortedIndexSize = 0;
		return;
	}

	constexpr int INDICES_PER_FACE = 4;
	mSortedIndexSize = ((mSortedNumFaces * INDICES_PER_FACE + 3) <= std::numeric_limits<uint16_t>::max())
		? sizeof(uint16_t)
		: sizeof(uint32_t);

	if (mSortedIndexSize == sizeof(uint16_t)) {
		mSortedIndices16.reserve(static_cast<size_t>(mSortedNumFaces) * 6);
	} else {
		mSortedIndices32.reserve(static_cast<size_t>(mSortedNumFaces) * 6);
	}

	static const int kFaceOrder[7] = {
		Facing::EAST, Facing::WEST, Facing::UP, Facing::DOWN,
		Facing::SOUTH, Facing::NORTH, Facing::NOT_DEFINED
	};
	int outIdx = 0;

	for (int rangeIdx = 0; rangeIdx < BlockRenderLayer::_RENDERLAYER_COUNT; ++rangeIdx) {
		const uint32_t startFace = static_cast<uint32_t>(mRenderLayerRange[rangeIdx].mStart) / 6;
		const uint32_t faceCount = static_cast<uint32_t>(mRenderLayerRange[rangeIdx].mCount) / 6;
		mSortedRenderLayerRange[rangeIdx].mStart = outIdx;
		if (faceCount == 0) {
			mSortedRenderLayerRange[rangeIdx].mCount = 0;
			continue;
		}
		for (int b = 0; b < Facing::NUM_CULLING_IDS; ++b) {
			mSortBuckets[b].clear();
		}
		const uint32_t endFace = std::min(startFace + faceCount, mSortedNumFaces);
		for (uint32_t f = startFace; f < endFace; ++f) {
			mSortBuckets[faces[f]].push_back(f);
		}
		for (int k = 0; k < 7; ++k) {
			const auto& bucket = mSortBuckets[kFaceOrder[k]];
			if (mSortedIndexSize == sizeof(uint16_t)) {
				for (uint32_t f : bucket) {
					const uint16_t baseidx = static_cast<uint16_t>(f * INDICES_PER_FACE);
					mSortedIndices16.push_back(baseidx + 1);
					mSortedIndices16.push_back(baseidx + 2);
					mSortedIndices16.push_back(baseidx + 0);
					mSortedIndices16.push_back(baseidx + 0);
					mSortedIndices16.push_back(baseidx + 2);
					mSortedIndices16.push_back(baseidx + 3);
					outIdx += 6;
				}
			} else {
				for (uint32_t f : bucket) {
					const uint32_t baseidx = f * INDICES_PER_FACE;
					mSortedIndices32.push_back(baseidx + 1);
					mSortedIndices32.push_back(baseidx + 2);
					mSortedIndices32.push_back(baseidx + 0);
					mSortedIndices32.push_back(baseidx + 0);
					mSortedIndices32.push_back(baseidx + 2);
					mSortedIndices32.push_back(baseidx + 3);
					outIdx += 6;
				}
			}
		}
		mSortedRenderLayerRange[rangeIdx].mCount = outIdx - mSortedRenderLayerRange[rangeIdx].mStart;
	}

	mRenderChunkSorter.setupIndexRanges(mSortedTerrainLayerRange, mSortedRenderLayerRange);
}


VisibilityExtimator* RenderChunkBuilder::_getVisibilityHelper() {
	// Visibility graph (canSeeChunkLookingFrom) is unused; frustum culling handles chunk visibility directly
	return nullptr;
}

void RenderChunkBuilder::build(RenderChunk& rc, bool transparentLeaves, bool smoothLighting, uint8_t lodLevel) {

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
		renderer->setSmoothLightingEnabled(smoothLighting && lodLevel == 0);
		skyLit = _seesSkyDirectly(rc, region);
		allDark = true;
 		mQueues = queuePool.getLocal().data();

		for (int i = 0; i < BlockRenderLayer::_RENDERLAYER_COUNT; ++i) {
			mQueues[i].reset();
		}
	}

	if (!_sortBlocks(region, rc, VE, transparentLeaves, lodLevel)) {
		return;
	}

	averageSkyLight = skyLit ? 15.f : 0;

	if (!_tessellateQueues(rc, region, lodLevel)) {
		return;
	}

	_buildRanges();

	if (VE) {
		mVisibility = VE->finish();
	}
}
