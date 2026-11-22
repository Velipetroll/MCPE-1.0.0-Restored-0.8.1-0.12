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
#include "world/level/FoliageColor.h"
#include "world/level/block/GrassBlock.h"
#include "world/level/block/LiquidBlock.h"
#include "world/level/block/RotatedPillarBlock.h"
#include "world/level/block/SlabBlock.h"
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
	std::vector<std::array<uint32_t, 4>>().swap(mGreedyCellBuffer);
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
	struct GreedyBlockTable {
		bool isFullOpaqueCube[256]{};
		bool isFullAndOpaqueStatic[256]{};
		bool canGreedyCube[256]{};
		bool isGlassOrIce[256]{};
		bool isLeaf[256]{};
		bool isEmissive[256]{};
		bool useAO[256]{};
		bool isHalfSlab[256]{};
		bool isTranslucent[256]{};
	};

	const GreedyBlockTable& getGreedyBlockTable() {
		static std::once_flag sTableOnce;
		static GreedyBlockTable sTbl;
		std::call_once(sTableOnce, []() {
			AABB dummyShape;
			for (int bid = 1; bid < 256; ++bid) {
				const Block* blk = Block::mBlocks[bid];
				const BlockGraphics* bg = BlockGraphics::mBlocks[bid];
				if (!blk || !bg) continue;
				const BlockShape shape = bg->getBlockShape();
				const BlockRenderLayer rl = blk->getRenderLayer();
				const bool cubeShape = (shape == BlockShape::BLOCK || shape == BlockShape::TREE);
				const AABB& sh = blk->getVisualShape(0, dummyShape, false);
				const bool fullAABB = (sh.min.x <= 0.001f && sh.min.y <= 0.001f && sh.min.z <= 0.001f &&
				                       sh.max.x >= 0.999f && sh.max.y >= 0.999f && sh.max.z >= 0.999f);
				const bool leaf = blk->hasProperty(BlockProperty::Leaf);
				sTbl.isLeaf[bid] = leaf;
				sTbl.isGlassOrIce[bid] = (Block::mGlass && bid == Block::mGlass->mID) || (Block::mIce && bid == Block::mIce->mID);
				sTbl.isEmissive[bid] = (Block::mLightEmission[bid] > 0);
				sTbl.useAO[bid] = (Block::mLightEmission[bid] == 0) && (Block::mTranslucency[bid] < 0.9f);
				sTbl.isHalfSlab[bid] = blk->hasProperty(BlockProperty::HalfSlab);
				sTbl.isTranslucent[bid] = (blk->getMaterial().getTranslucency() != 0.0f);
				sTbl.isFullAndOpaqueStatic[bid] = cubeShape && (rl == BlockRenderLayer::RENDERLAYER_OPAQUE || rl == BlockRenderLayer::RENDERLAYER_SEASONS_OPAQUE);

				if (Block::mSolid[bid] && blk->isSolidBlockingBlock() &&
				    Block::mLightBlock[bid] >= Brightness::MAX &&
				    rl == BlockRenderLayer::RENDERLAYER_OPAQUE &&
				    cubeShape && fullAABB &&
				    !blk->canHaveExtraData() && !leaf &&
				    !blk->hasProperty(BlockProperty::TopSnow)) {
					sTbl.isFullOpaqueCube[bid] = true;
				}

				const bool isLiquid = (bid == 8 || bid == 9 || bid == 10 || bid == 11);
				if (!isLiquid && bid != 36 && cubeShape && fullAABB &&
				    !blk->canHaveExtraData() &&
				    !blk->hasProperty(BlockProperty::TopSnow) &&
				    !blk->hasProperty(BlockProperty::Portal) &&
				    blk->getExtraRenderLayers() == 0) {
					sTbl.canGreedyCube[bid] = true;
				}
			}
		});
		return sTbl;
	}

	inline bool isFullAndOpaqueAt(BlockID id, bool transparentLeaves, const GreedyBlockTable& tbl) {
		if (tbl.isLeaf[id]) return !transparentLeaves;
		return tbl.isFullAndOpaqueStatic[id];
	}

	inline bool isDeepLeafOccluder(BlockSource& region, const BlockPos& p, bool transparentLeaves, const GreedyBlockTable& tbl) {
		if (!transparentLeaves) return true;
		if (!isFullAndOpaqueAt(region.getBlockID(p.below()), true, tbl)) return false;
		if (!isFullAndOpaqueAt(region.getBlockID(p.north()), true, tbl)) return false;
		if (!isFullAndOpaqueAt(region.getBlockID(p.south()), true, tbl)) return false;
		if (!isFullAndOpaqueAt(region.getBlockID(p.west()), true, tbl)) return false;
		if (!isFullAndOpaqueAt(region.getBlockID(p.east()), true, tbl)) return false;
		const BlockID upId = region.getBlockID(p.above());
		if (upId == BlockID::AIR) return false;
		if (Block::mTranslucency[upId] > 0.f) {
			const Block* upBlk = Block::mBlocks[upId];
			if (!upBlk || !upBlk->hasProperty(BlockProperty::Leaf | BlockProperty::TopSnow)) {
				return false;
			}
		}
		return true;
	}

	inline BlockRenderLayer getEffectiveRenderLayer(const Block& block, BlockSource& region, const BlockPos& p, DataID data, bool transparentLeaves) {
		BlockRenderLayer renderLayer = block.getRenderLayer(region, p);
		if (block.hasProperty(BlockProperty::Leaf)) {
			const bool isSeasonsTinted = (renderLayer == BlockRenderLayer::RENDERLAYER_SEASONS_OPAQUE || renderLayer == BlockRenderLayer::RENDERLAYER_SEASONS_OPTIONAL_ALPHATEST);
			const int leafType = Block::mLeaves->getBlockState(BlockState::MappedType).get<int>(data);
			if (leafType == enum_cast(OldLeafBlock::LeafType::Jungle)) {
				renderLayer = transparentLeaves && isSeasonsTinted ? BlockRenderLayer::RENDERLAYER_SEASONS_OPAQUE : BlockRenderLayer::RENDERLAYER_OPAQUE;
			} else if (LeafBlock::isDeepLeafBlock(region, p)) {
				renderLayer = transparentLeaves && isSeasonsTinted ? BlockRenderLayer::RENDERLAYER_SEASONS_OPAQUE : BlockRenderLayer::RENDERLAYER_OPAQUE;
			}
		}
		return renderLayer;
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

	inline bool isClusterCompleteAt(BlockSource& region, int clusterX, int clusterZ) {
		for (int dx = 0; dx < 2; ++dx) {
			for (int dz = 0; dz < 2; ++dz) {
				LevelChunk* lc = region.getChunk(ChunkPos(clusterX + dx, clusterZ + dz));
				if (!lc || lc->getState() < ChunkState::Loaded) {
					return false;
				}
			}
		}
		return true;
	}

	inline float findWaterDepth(BlockSource& region, const BlockPos& p, float baseDepth) {
		float depth = 0.0f;
		const float maxDepth = 20.0f;
		for (BlockPos pp = p; pp.y > 0 && depth <= maxDepth; --pp.y) {
			const Material& material = region.getBlock(pp).getMaterial();
			if (material.isType(MaterialType::Air)) {
				depth += 1.0f;
			} else if (!material.isType(MaterialType::Water)) {
				break;
			}
		}
		return std::min(1.0f, (depth / maxDepth) + baseDepth);
	}

	inline float getWaterCornerHeight(BlockSource& region, const BlockPos& p, const Material& waterMat) {
		int count = 0;
		float h = 0.0f;
		for (int i = 0; i < 4; ++i) {
			const BlockPos n(p.x - (i & 1), p.y, p.z - ((i >> 1) & 1));
			if (region.getMaterial(n.above()) == waterMat) {
				return 1.0f;
			}
			const Material& tm = region.getMaterial(n);
			if (tm == waterMat) {
				const int d = region.getData(n);
				if (d >= 8 || d == 0) {
					h += LiquidBlock::getHeightFromData(d) * 10.0f;
					count += 10;
				}
				h += LiquidBlock::getHeightFromData(d);
				++count;
			} else if (!tm.isSolid()) {
				h += 1.0f;
				++count;
			}
		}
		return 1.0f - h / static_cast<float>(count);
	}

	inline bool isFlatStillWater(BlockSource& region, const BlockPos& wp, BlockID id, DataID data, const GreedyBlockTable& tbl, bool transparentLeaves) {
		if ((id != 8 && id != 9) || (data & 7) != 0) {
			return false;
		}
		const FullBlock up = region.getBlockAndData(wp.above());
		if (up.id == 8 || up.id == 9 || up.id == 79 || isFullAndOpaqueAt(up.id, transparentLeaves, tbl)) {
			return false;
		}
		const Material& waterMat = Material::getMaterial(MaterialType::Water);
		const float h0 = getWaterCornerHeight(region, wp, waterMat);
		const float h1 = getWaterCornerHeight(region, wp.south(), waterMat);
		const float h2 = getWaterCornerHeight(region, BlockPos(wp.x + 1, wp.y, wp.z + 1), waterMat);
		const float h3 = getWaterCornerHeight(region, wp.east(), waterMat);
		const float expected = 8.0f / 9.0f;
		if (std::abs(h0 - expected) > 1e-4f || std::abs(h1 - expected) > 1e-4f ||
		    std::abs(h2 - expected) > 1e-4f || std::abs(h3 - expected) > 1e-4f) {
			return false;
		}
		if (LiquidBlock::getSlopeAngle(region, wp, waterMat) > -999.0) {
			return false;
		}
		// Keep water blocks with visible side faces (e.g. next to glass) on tessellateLiquidInWorld
		static const int kSideOff[4][2] = { {0, -1}, {0, 1}, {-1, 0}, {1, 0} };
		for (int s = 0; s < 4; ++s) {
			const BlockID nid = region.getBlockID(BlockPos(wp.x + kSideOff[s][0], wp.y, wp.z + kSideOff[s][1]));
			if (nid != 8 && nid != 9 && nid != 79 && !isFullAndOpaqueAt(nid, transparentLeaves, tbl)) {
				return false;
			}
		}
		return true;
	}

	inline uint32_t packABGR(float r, float g, float b, float a) {
		const uint32_t rb = static_cast<uint32_t>(Math::clamp(static_cast<int>(r * 255.0f + 0.5f), 0, 255));
		const uint32_t gb = static_cast<uint32_t>(Math::clamp(static_cast<int>(g * 255.0f + 0.5f), 0, 255));
		const uint32_t bb = static_cast<uint32_t>(Math::clamp(static_cast<int>(b * 255.0f + 0.5f), 0, 255));
		const uint32_t ab = static_cast<uint32_t>(Math::clamp(static_cast<int>(a * 255.0f + 0.5f), 0, 255));
		return (ab << 24) | (bb << 16) | (gb << 8) | rb;
	}

	inline float exactUint16ToNorm(uint16_t k) {
		return (k == 0) ? 0.0f : ((static_cast<float>(k) - 0.25f) / 65535.0f);
	}

	struct GreedyCell {
		bool valid = false;
		BlockID id = BlockID::AIR;
		uint8_t rot = 0;
		bool isIsotropic = false;
		bool tintOnlyTop = false;
		bool isWater = false;
		bool isEmissive = false;
		bool enableAO = false;
		bool isFlat = false;
		bool canMergeU = false;
		bool canMergeV = false;
		// Stored in canonical (u,v) corner order: 0=C00(0,0), 1=C10(1,0), 2=C11(1,1), 3=C01(0,1)
		uint8_t blkL4[4] = { 0, 0, 0, 0 };
		uint8_t skyL4[4] = { 60, 60, 60, 60 };
		TextureUVCoordinateSet tex;
		uint32_t c[4] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
	};

	inline bool greedySameBase(const GreedyCell& a, const GreedyCell& b, uint8_t /*lodLevel*/) {
		if (!a.valid || !b.valid) return false;
		if (a.id != b.id) return false;
		if (a.rot != b.rot || a.isIsotropic != b.isIsotropic ||
		    a.isEmissive != b.isEmissive || a.enableAO != b.enableAO) return false;
		if (a.tex._u0 != b.tex._u0 || a.tex._v0 != b.tex._v0 ||
		    a.tex._u1 != b.tex._u1 || a.tex._v1 != b.tex._v1) {
			return false;
		}
		return true;
	}

	inline bool greedyCanMergeFlat(const GreedyCell& a, const GreedyCell& b, uint8_t lodLevel) {
		return greedySameBase(a, b, lodLevel) && b.isFlat &&
		       a.c[0] == b.c[0] && a.blkL4[0] == b.blkL4[0] && a.skyL4[0] == b.skyL4[0];
	}

	inline bool greedyCanMergeU(const GreedyCell& a, const GreedyCell& b, uint8_t lodLevel) {
		return greedySameBase(a, b, lodLevel) && b.canMergeU &&
		       a.c[0] == b.c[0] && a.c[3] == b.c[3] &&
		       a.blkL4[0] == b.blkL4[0] && a.blkL4[3] == b.blkL4[3] &&
		       a.skyL4[0] == b.skyL4[0] && a.skyL4[3] == b.skyL4[3];
	}

	inline bool greedyCanMergeV(const GreedyCell& a, const GreedyCell& b, uint8_t lodLevel) {
		return greedySameBase(a, b, lodLevel) && b.canMergeV &&
		       a.c[0] == b.c[0] && a.c[1] == b.c[1] &&
		       a.blkL4[0] == b.blkL4[0] && a.blkL4[1] == b.blkL4[1] &&
		       a.skyL4[0] == b.skyL4[0] && a.skyL4[1] == b.skyL4[1];
	}

	inline void emitGreedyQuad(Tessellator& t, int face,
	                           float aX, float aY, float aZ,
	                           float bX, float bY, float bZ,
	                           const GreedyCell& cell, bool merged, uint8_t cpuRot = 0,
	                           bool useCellBuffer = false, uint16_t cellU1 = 0, uint16_t cellV1 = 0) {
		t.quadFacing(static_cast<FacingID>(face));

		const float u0 = cell.tex._u0;
		const float v0 = cell.tex._v0;
		const float u1 = merged ? cell.tex._u0 : cell.tex._u1;
		const float v1 = merged ? cell.tex._v0 : cell.tex._v1;

		// Corner UVs for 1x1 unmerged quad (v0..v3 order per face), rotated by cpuRot (0..3)
		float cu[4], cv[4];
		switch (face) {
		case Facing::DOWN:
			cu[0]=u0; cv[0]=v1; cu[1]=u0; cv[1]=v0; cu[2]=u1; cv[2]=v0; cu[3]=u1; cv[3]=v1; break;
		case Facing::UP:
			cu[0]=u1; cv[0]=v1; cu[1]=u1; cv[1]=v0; cu[2]=u0; cv[2]=v0; cu[3]=u0; cv[3]=v1; break;
		case Facing::NORTH:
			cu[0]=u1; cv[0]=v0; cu[1]=u0; cv[1]=v0; cu[2]=u0; cv[2]=v1; cu[3]=u1; cv[3]=v1; break;
		case Facing::SOUTH:
			cu[0]=u0; cv[0]=v0; cu[1]=u0; cv[1]=v1; cu[2]=u1; cv[2]=v1; cu[3]=u1; cv[3]=v0; break;
		case Facing::WEST:
			cu[0]=u1; cv[0]=v0; cu[1]=u0; cv[1]=v0; cu[2]=u0; cv[2]=v1; cu[3]=u1; cv[3]=v1; break;
		case Facing::EAST:
		default:
			cu[0]=u0; cv[0]=v1; cu[1]=u1; cv[1]=v1; cu[2]=u1; cv[2]=v0; cu[3]=u0; cv[3]=v0; break;
		}
		if (!merged && cpuRot != 0) {
			uint8_t effRot = cpuRot;
			if ((face == Facing::DOWN || face == Facing::NORTH || face == Facing::EAST) && (effRot == 1 || effRot == 2)) {
				effRot = static_cast<uint8_t>(3 - effRot);
			}
			const bool flipRotV = (face == Facing::NORTH || face == Facing::EAST) && (effRot == 1 || effRot == 2);
			auto rotUV = [&](float& u, float& v) {
				const float nu = (u - u0) / (u1 - u0);
				const float nv = (v - v0) / (v1 - v0);
				float ru = nu, rv = nv;
				if (effRot == 1) { ru = nv; rv = flipRotV ? nu : (1.0f - nu); }
				else if (effRot == 2) { ru = 1.0f - nv; rv = flipRotV ? (1.0f - nu) : nu; }
				else if (effRot == 3) { ru = 1.0f - nu; rv = 1.0f - nv; }
				u = u0 + ru * (u1 - u0);
				v = v0 + rv * (v1 - v0);
			};
			for (int i = 0; i < 4; ++i) rotUV(cu[i], cv[i]);
		}

		// Maps each face's vertex index 0..3 to canonical (u,v) corner 0=C00, 1=C10, 2=C11, 3=C01
		static const int kVertToCorner[6][4] = {
			{ 3, 0, 1, 2 }, // DOWN
			{ 2, 1, 0, 3 }, // UP
			{ 3, 2, 1, 0 }, // NORTH
			{ 3, 0, 1, 2 }, // SOUTH
			{ 2, 3, 0, 1 }, // WEST
			{ 1, 0, 3, 2 }  // EAST
		};
		auto emitVert = [&](int vi, float vx, float vy, float vz) {
			const int ci = kVertToCorner[face][vi];
			uint16_t u1 = 0;
			uint16_t v1 = 0;
			if (!merged) {
				u1 = static_cast<uint16_t>((static_cast<uint32_t>(cell.blkL4[ci] & 63u) << 10));
				v1 = static_cast<uint16_t>((static_cast<uint32_t>(cell.skyL4[ci] & 63u) << 10));
			} else if (!useCellBuffer) {
				u1 = static_cast<uint16_t>(((static_cast<uint32_t>(cell.blkL4[ci]) & 63u) << 10) |
				                           static_cast<uint32_t>(1 + face));
				v1 = static_cast<uint16_t>(((static_cast<uint32_t>(cell.skyL4[ci]) & 63u) << 10) |
				                           (cell.isIsotropic ? 4u : 0u) |
				                           static_cast<uint32_t>(cell.rot & 3u));
			} else {
				u1 = cellU1;
				v1 = cellV1;
			}
			t.tex1(exactUint16ToNorm(u1), exactUint16ToNorm(v1));
			t.colorABGR(static_cast<int>(cell.c[ci]));
			t.vertexUV(vx, vy, vz, cu[vi], cv[vi]);
		};

		switch (face) {
		case Facing::DOWN:
			emitVert(0, aX, aY, bZ);
			emitVert(1, aX, aY, aZ);
			emitVert(2, bX, aY, aZ);
			emitVert(3, bX, aY, bZ);
			break;
		case Facing::UP:
			emitVert(0, bX, bY, bZ);
			emitVert(1, bX, bY, aZ);
			emitVert(2, aX, bY, aZ);
			emitVert(3, aX, bY, bZ);
			break;
		case Facing::NORTH:
			emitVert(0, aX, bY, aZ);
			emitVert(1, bX, bY, aZ);
			emitVert(2, bX, aY, aZ);
			emitVert(3, aX, aY, aZ);
			break;
		case Facing::SOUTH:
			emitVert(0, aX, bY, bZ);
			emitVert(1, aX, aY, bZ);
			emitVert(2, bX, aY, bZ);
			emitVert(3, bX, bY, bZ);
			break;
		case Facing::WEST:
			emitVert(0, aX, bY, bZ);
			emitVert(1, aX, bY, aZ);
			emitVert(2, aX, aY, aZ);
			emitVert(3, aX, aY, bZ);
			break;
		case Facing::EAST:
		default:
			emitVert(0, bX, aY, bZ);
			emitVert(1, bX, aY, aZ);
			emitVert(2, bX, bY, aZ);
			emitVert(3, bX, bY, bZ);
			break;
		}
	}
}

bool RenderChunkBuilder::_sortBlocks(BlockSource& region, RenderChunk& rc, VisibilityExtimator* VE, bool transparentLeaves, uint8_t lodLevel, bool greedyMeshing) {
	std::fill(std::begin(mHasGreedyLayer), std::end(mHasGreedyLayer), false);
	mHasAnyGreedy = false;
	mNeedsBlockTessCache = false;
	mLightAoBuilt = false;
	if (greedyMeshing) {
		mGreedyVoxelLayer.fill(0);
		std::memset(mGreedyActivePlanes, 0, sizeof(mGreedyActivePlanes));
	}

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

	const GreedyBlockTable& tbl = getGreedyBlockTable();

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

				auto recordGreedyVoxel = [&](BlockRenderLayer rl, uint8_t faceMask) {
					const int rlIdx = static_cast<int>(rl);
					mGreedyVoxelLayer[sbIdx] = static_cast<uint8_t>(rlIdx + 1);
					mGreedyVoxelFaceMask[sbIdx] = faceMask;
					if (faceMask & (1u << Facing::DOWN))  mGreedyActivePlanes[rlIdx][Facing::DOWN]  |= static_cast<uint16_t>(1u << ly);
					if (faceMask & (1u << Facing::UP))    mGreedyActivePlanes[rlIdx][Facing::UP]    |= static_cast<uint16_t>(1u << ly);
					if (faceMask & (1u << Facing::NORTH)) mGreedyActivePlanes[rlIdx][Facing::NORTH] |= static_cast<uint16_t>(1u << lz);
					if (faceMask & (1u << Facing::SOUTH)) mGreedyActivePlanes[rlIdx][Facing::SOUTH] |= static_cast<uint16_t>(1u << lz);
					if (faceMask & (1u << Facing::WEST))  mGreedyActivePlanes[rlIdx][Facing::WEST]  |= static_cast<uint16_t>(1u << lx);
					if (faceMask & (1u << Facing::EAST))  mGreedyActivePlanes[rlIdx][Facing::EAST]  |= static_cast<uint16_t>(1u << lx);
					mHasGreedyLayer[rlIdx] = true;
					mHasAnyGreedy = true;
				};

				// Fast-cull completely buried full-opaque cubes (interior + subchunk borders)
				if (tbl.isFullOpaqueCube[id]) {
					BlockID nDown, nUp, nNorth, nSouth, nWest, nEast;
					fetchNeighbors(nDown, nUp, nNorth, nSouth, nWest, nEast);
					uint8_t faceMask = 0;
					if (!isFullAndOpaqueAt(nDown, transparentLeaves, tbl))  faceMask |= (1u << Facing::DOWN);
					if (!isFullAndOpaqueAt(nUp, transparentLeaves, tbl))    faceMask |= (1u << Facing::UP);
					if (!isFullAndOpaqueAt(nNorth, transparentLeaves, tbl)) faceMask |= (1u << Facing::NORTH);
					if (!isFullAndOpaqueAt(nSouth, transparentLeaves, tbl)) faceMask |= (1u << Facing::SOUTH);
					if (!isFullAndOpaqueAt(nWest, transparentLeaves, tbl))  faceMask |= (1u << Facing::WEST);
					if (!isFullAndOpaqueAt(nEast, transparentLeaves, tbl))  faceMask |= (1u << Facing::EAST);
					if (faceMask == 0) {
						continue;
					}
					if (greedyMeshing) {
						recordGreedyVoxel(BlockRenderLayer::RENDERLAYER_OPAQUE, faceMask);
						if (VE) VE->setBlock(BlockPos(wx, min.y + ly, wz), Block::mBlocks[id]);
						continue;
					}
				}

				// Water: fast-cull completely occluded water, and send flat still water at ALL LODs to greedy
				if (id == 8 || id == 9) {
					BlockID nDown, nUp, nNorth, nSouth, nWest, nEast;
					fetchNeighbors(nDown, nUp, nNorth, nSouth, nWest, nEast);
					auto isWaterOccludedBy = [&](BlockID nid) -> bool {
						return nid == 8 || nid == 9 || nid == 79 || isFullAndOpaqueAt(nid, transparentLeaves, tbl);
					};
					if (isWaterOccludedBy(nDown) && isWaterOccludedBy(nUp) &&
					    isWaterOccludedBy(nNorth) && isWaterOccludedBy(nSouth) &&
					    isWaterOccludedBy(nWest) && isWaterOccludedBy(nEast)) {
						continue;
					}
					const BlockPos wp(wx, min.y + ly, wz);
					if (greedyMeshing && isFlatStillWater(region, wp, id, sc->mData.get(sbIdx), tbl, transparentLeaves)) {
						uint8_t faceMask = 0;
						if (!isWaterOccludedBy(nDown)) faceMask |= (1u << Facing::DOWN);
						if (!isWaterOccludedBy(nUp))   faceMask |= (1u << Facing::UP);
						if (faceMask != 0) {
							recordGreedyVoxel(BlockRenderLayer::RENDERLAYER_WATER, faceMask);
							if (VE) VE->setBlock(wp, Block::mBlocks[id]);
						}
						continue;
					}
				}

				// LOD 2+: Cull interior leaf blocks (leaf-on-leaf) and skip far decorative micro-clutter
				if (lodLevel >= 2) {
					const BlockGraphics* bg = BlockGraphics::mBlocks[id];
					if (bg && isSmallClutterShape(bg->getBlockShape())) {
						continue;
					}
					if (tbl.isLeaf[id]) {
						BlockID nDown, nUp, nNorth, nSouth, nWest, nEast;
						fetchNeighbors(nDown, nUp, nNorth, nSouth, nWest, nEast);
						auto isLeafOrOpaque = [&](BlockID nid) -> bool {
							return tbl.isFullOpaqueCube[nid] || tbl.isLeaf[nid];
						};
						if (isLeafOrOpaque(nDown) && isLeafOrOpaque(nUp) &&
						    isLeafOrOpaque(nNorth) && isLeafOrOpaque(nSouth) &&
						    isLeafOrOpaque(nWest) && isLeafOrOpaque(nEast)) {
							continue;
						}
					}
				}

				const BlockPos p(wx, min.y + ly, wz);
				const Block& block = *Block::mBlocks[id];
				if (VE) {
					VE->setBlock(p, &block);
				}

				const DataID cellData = sc->mData.get(sbIdx);
				const BlockRenderLayer renderLayer = getEffectiveRenderLayer(block, region, p, cellData, transparentLeaves);

				// Fast-cull completely occluded glass/ice/fast-leaves, and route all other full-cube blocks to greedy
				if (greedyMeshing && tbl.canGreedyCube[id]) {
					BlockID nDown, nUp, nNorth, nSouth, nWest, nEast;
					fetchNeighbors(nDown, nUp, nNorth, nSouth, nWest, nEast);
					const BlockID nArr[6] = { nDown, nUp, nNorth, nSouth, nWest, nEast };
					uint8_t faceMask = 0;

					if (tbl.isGlassOrIce[id]) {
						for (int f = 0; f < 6; ++f) {
							const BlockID nid = nArr[f];
							if (nid != id && !isFullAndOpaqueAt(nid, transparentLeaves, tbl)) {
								faceMask |= static_cast<uint8_t>(1u << f);
							}
						}
					} else if (tbl.isLeaf[id]) {
						if (!transparentLeaves) {
							for (int f = 0; f < 6; ++f) {
								if (!isFullAndOpaqueAt(nArr[f], transparentLeaves, tbl)) {
									faceMask |= static_cast<uint8_t>(1u << f);
								}
							}
						} else {
							static const int kNx[6] = { 0, 0, 0, 0, -1, 1 };
							static const int kNy[6] = { -1, 1, 0, 0, 0, 0 };
							static const int kNz[6] = { 0, 0, -1, 1, 0, 0 };
							const bool thisOpaque = isDeepLeafOccluder(region, p, transparentLeaves, tbl);
							for (int f = 0; f < 6; ++f) {
								const BlockID nid = nArr[f];
								if (thisOpaque) {
									if (!isFullAndOpaqueAt(nid, transparentLeaves, tbl) &&
									    !isDeepLeafOccluder(region, BlockPos(p.x + kNx[f], p.y + kNy[f], p.z + kNz[f]), transparentLeaves, tbl)) {
										faceMask |= static_cast<uint8_t>(1u << f);
									}
								} else if (nid == BlockID::AIR) {
									faceMask |= static_cast<uint8_t>(1u << f);
								} else {
									if (Block::mInvisibleBedrock && nid == Block::mInvisibleBedrock->mID) continue;
									if (tbl.isLeaf[nid]) {
										if (f == Facing::WEST || f == Facing::NORTH || f == Facing::UP) continue;
										if (isDeepLeafOccluder(region, BlockPos(p.x + kNx[f], p.y + kNy[f], p.z + kNz[f]), transparentLeaves, tbl)) continue;
										faceMask |= static_cast<uint8_t>(1u << f);
									} else if ((Block::mStoneSlab && nid == Block::mStoneSlab->mID) ||
									           (Block::mWoodenSlab && nid == Block::mWoodenSlab->mID)) {
										const DataID ndata = region.getData(BlockPos(p.x + kNx[f], p.y + kNy[f], p.z + kNz[f]));
										const bool isBottom = SlabBlock::isBottomSlab(ndata);
										if (!((f == Facing::UP && isBottom) || (f == Facing::DOWN && !isBottom))) {
											faceMask |= static_cast<uint8_t>(1u << f);
										}
									} else if (!isFullAndOpaqueAt(nid, transparentLeaves, tbl)) {
										faceMask |= static_cast<uint8_t>(1u << f);
									}
								}
							}
						}
					} else {
						for (int f = 0; f < 6; ++f) {
							if (!isFullAndOpaqueAt(nArr[f], transparentLeaves, tbl)) {
								faceMask |= static_cast<uint8_t>(1u << f);
							}
						}
					}

					if (faceMask != 0) {
						recordGreedyVoxel(renderLayer, faceMask);
					}
					continue;
				}

				if (block.canHaveExtraData()) {
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

void RenderChunkBuilder::_buildLightAoCache(const BlockPos& rcPos, BlockSource& region) {
	if (mLightAoBuilt) return;
	mLightAoBuilt = true;

	const GreedyBlockTable& tbl = getGreedyBlockTable();
	LevelChunk* lc = region.getChunk(rcPos);
	const size_t subY = static_cast<size_t>(rcPos.y >> 4);
	auto getSub = [&](LevelChunk* c, size_t sy) -> const LevelChunk::SubChunk* {
		if (!c) return nullptr;
		const auto subs = c->getSubChunks();
		return (sy < subs.size()) ? subs[sy].get() : nullptr;
	};
	const LevelChunk::SubChunk* sc = getSub(lc, subY);
	const LevelChunk::SubChunk* scAbove = (rcPos.y + RENDERCHUNK_SIDE < region.getMaxHeight()) ? getSub(lc, subY + 1) : nullptr;
	const LevelChunk::SubChunk* scBelow = (subY > 0) ? getSub(lc, subY - 1) : nullptr;
	auto getNeighborSub = [&](int dx, int dz) -> const LevelChunk::SubChunk* {
		if (!lc) return nullptr;
		LevelChunk* nlc = region.getChunk(ChunkPos(lc->getPosition().x + dx, lc->getPosition().z + dz));
		return getSub(nlc, subY);
	};
	const LevelChunk::SubChunk* scWest  = getNeighborSub(-1, 0);
	const LevelChunk::SubChunk* scEast  = getNeighborSub(1, 0);
	const LevelChunk::SubChunk* scNorth = getNeighborSub(0, -1);
	const LevelChunk::SubChunk* scSouth = getNeighborSub(0, 1);

	auto packCell = [&](BlockID bid, uint8_t sky, uint8_t rawBlk, int wx, int wy, int wz) -> uint16_t {
		uint8_t blk = std::max<uint8_t>(rawBlk, Block::mLightEmission[bid]);
		if (sky == 0 && blk == 0 && tbl.isHalfSlab[bid] && wy > 0) {
			const BlockPos belowPos(wx, wy - 1, wz);
			const BlockID bBid = region.getBlockID(belowPos);
			const BrightnessPair bBp = region.getLightColor(belowPos, Block::getLightEmission(bBid));
			sky = bBp.sky;
			blk = bBp.block;
		}
		const uint16_t flags = (Block::mSolid[bid] ? 0x100u : 0u) |
		                       ((Block::mSolid[bid] || tbl.isLeaf[bid]) ? 0x200u : 0u) |
		                       (Block::mTranslucency[bid] > 0.5f ? 0x400u : 0u);
		return static_cast<uint16_t>(((sky & 15) << 4) | (blk & 15) | flags);
	};

	auto packSub = [&](const LevelChunk::SubChunk* s, int sx, int sy, int sz, int wx, int wy, int wz) -> uint16_t {
		if (!s) return static_cast<uint16_t>((15u << 4) | 0x400u);
		const size_t idx = static_cast<size_t>(sy | (sz << 4) | (sx << 8));
		return packCell(s->mBlocks[idx], s->mSkyLight.get(idx), s->mBlockLight.get(idx), wx, wy, wz);
	};

	for (int z = 0; z < 18; ++z) {
		const int lz = z - 1;
		const int wz = rcPos.z + lz;
		for (int y = 0; y < 18; ++y) {
			const int ly = y - 1;
			const int wy = rcPos.y + ly;
			const size_t rowBase = static_cast<size_t>((z * 18 + y) * 18);
			const bool inY = (ly >= 0 && ly < 16);
			const bool inZ = (lz >= 0 && lz < 16);
			if (sc && inY && inZ) {
				// Fast branchless interior loop for x = 1..16 (lx = 0..15)
				mLightAoCache[rowBase] = scWest
					? packSub(scWest, 15, ly, lz, rcPos.x - 1, wy, wz)
					: [&]() {
						const BlockPos wp(rcPos.x - 1, wy, wz);
						const BlockID bid = region.getBlockID(wp);
						const BrightnessPair bp = region.getLightColor(wp, Block::getLightEmission(bid));
						return packCell(bid, bp.sky, bp.block, rcPos.x - 1, wy, wz);
					}();
				if (!skyLit && ((mLightAoCache[rowBase] >> 4) & 15u) > 0) skyLit = true;
				for (int lx = 0; lx < 16; ++lx) {
					const size_t idx = static_cast<size_t>(ly | (lz << 4) | (lx << 8));
					const uint16_t val = packCell(sc->mBlocks[idx], sc->mSkyLight.get(idx), sc->mBlockLight.get(idx), rcPos.x + lx, wy, wz);
					mLightAoCache[rowBase + static_cast<size_t>(lx + 1)] = val;
				}
				mLightAoCache[rowBase + 17] = scEast
					? packSub(scEast, 0, ly, lz, rcPos.x + 16, wy, wz)
					: [&]() {
						const BlockPos wp(rcPos.x + 16, wy, wz);
						const BlockID bid = region.getBlockID(wp);
						const BrightnessPair bp = region.getLightColor(wp, Block::getLightEmission(bid));
						return packCell(bid, bp.sky, bp.block, rcPos.x + 16, wy, wz);
					}();
				if (!skyLit && ((mLightAoCache[rowBase + 17] >> 4) & 15u) > 0) skyLit = true;
				continue;
			}
			for (int x = 0; x < 18; ++x) {
				const int lx = x - 1;
				const int wx = rcPos.x + lx;
				const int inX = (lx >= 0 && lx < 16) ? 1 : 0;
				uint16_t val;
				if (inX & (inY ? 1 : 0) & (inZ ? 1 : 0)) {
					val = packSub(sc, lx, ly, lz, wx, wy, wz);
				} else if (inX && inZ && ly == -1 && wy >= 0) {
					val = packSub(scBelow, lx, 15, lz, wx, wy, wz);
				} else if (inX && inZ && ly == 16 && wy < region.getMaxHeight()) {
					val = packSub(scAbove, lx, 0, lz, wx, wy, wz);
				} else if (inX && inY && lz == -1) {
					val = packSub(scNorth, lx, ly, 15, wx, wy, wz);
				} else if (inX && inY && lz == 16) {
					val = packSub(scSouth, lx, ly, 0, wx, wy, wz);
				} else if (inY && inZ && lx == -1) {
					val = packSub(scWest, 15, ly, lz, wx, wy, wz);
				} else if (inY && inZ && lx == 16) {
					val = packSub(scEast, 0, ly, lz, wx, wy, wz);
				} else {
					const BlockPos wp(wx, wy, wz);
					const BlockID bid = region.getBlockID(wp);
					const BrightnessPair bp = region.getLightColor(wp, Block::getLightEmission(bid));
					val = packCell(bid, bp.sky, bp.block, wx, wy, wz);
				}
				if (!skyLit && ((val >> 4) & 15u) > 0) skyLit = true;
				mLightAoCache[rowBase + static_cast<size_t>(x)] = val;
			}
		}
	}
}

void RenderChunkBuilder::_greedyMeshPass(RenderChunk& rc, BlockSource& region, uint8_t lodLevel, bool smoothLighting, bool transparentLeaves, BlockRenderLayer targetLayer) {
	auto& t = renderer->getTessellator();
	const BlockPos rcPos = rc.getPosition();
	const GreedyBlockTable& tbl = getGreedyBlockTable();
	const bool isWaterPass = (targetLayer == BlockRenderLayer::RENDERLAYER_WATER);
	const bool isSeasonsLayer = (targetLayer == BlockRenderLayer::RENDERLAYER_SEASONS_OPAQUE ||
	                             targetLayer == BlockRenderLayer::RENDERLAYER_SEASONS_OPTIONAL_ALPHATEST);
	const int rlIdx = static_cast<int>(targetLayer);
	const uint8_t layerTag = static_cast<uint8_t>(rlIdx + 1);

	static std::atomic<uint64_t> sGreedyInputFaces{0};
	static std::atomic<uint64_t> sGreedyOutputQuads{0};
	static std::atomic<uint64_t> sGrassIn{0}, sGrassOut{0};
	static std::atomic<uint64_t> sDirtIn{0}, sDirtOut{0};
	static std::atomic<uint64_t> sStoneIn{0}, sStoneOut{0};
	static std::atomic<uint64_t> sWaterIn{0}, sWaterOut{0};
	static std::atomic<uint64_t> sLeavesIn{0}, sLeavesOut{0};
	static std::atomic<uint64_t> sOtherIn{0}, sOtherOut{0};
	static const bool sLogStats = (std::getenv("MCPE_GREEDY_STATS") != nullptr);

	const bool upsideDownLight = region.getDimensionConst().hasUpsideDownLight();
	const float kFaceShade[6] = {
		upsideDownLight ? 0.95f : 0.5f,
		upsideDownLight ? 0.7f : 1.0f,
		0.8f, 0.8f, 0.6f, 0.6f
	};
	static const float kEmissiveFaceShade[6] = { 0.875f, 1.0f, 0.95f, 0.95f, 0.9f, 0.9f };
	static const int kNx[6] = { 0, 0, 0, 0, -1, 1 };
	static const int kNy[6] = { -1, 1, 0, 0, 0, 0 };
	static const int kNz[6] = { 0, 0, -1, 1, 0, 0 };
	static const int kCornerSU[4] = { -1, 1, 1, -1 };
	static const int kCornerSV[4] = { -1, -1, 1, 1 };

	LevelChunk* lc = region.getChunk(rcPos);
	const size_t subY = static_cast<size_t>(rcPos.y >> 4);
	const auto subs = lc ? lc->getSubChunks() : buffer_span<std::unique_ptr<LevelChunk::SubChunk>>{};
	const LevelChunk::SubChunk* sc = (subY < subs.size()) ? subs[subY].get() : nullptr;

	auto getCache = [&](int tx, int ty, int tz) -> uint16_t {
		return mLightAoCache[static_cast<size_t>((tz * 18 + ty) * 18 + tx)];
	};

	uint32_t grassColCache[16 * 16]{};
	bool grassColValid[16 * 16]{};

	std::array<GreedyCell, 16 * 16> grid;
	std::array<uint8_t, 16 * 16> used;

	uint64_t chunkInFaces = 0;
	uint64_t chunkOutQuads = 0;
	uint64_t cGrassIn = 0, cGrassOut = 0;
	uint64_t cDirtIn = 0, cDirtOut = 0;
	uint64_t cStoneIn = 0, cStoneOut = 0;
	uint64_t cWaterIn = 0, cWaterOut = 0;
	uint64_t cLeavesIn = 0, cLeavesOut = 0;
	uint64_t cOtherIn = 0, cOtherOut = 0;

	const float topYOffset = isWaterPass ? (8.0f / 9.0f - 0.001f) : 1.0f;

	for (int face = Facing::DOWN; face <= Facing::EAST; ++face) {
		uint16_t planeMask = mGreedyActivePlanes[rlIdx][face];
		if (!planeMask) continue;

		const int nxOff = kNx[face], nyOff = kNy[face], nzOff = kNz[face];
		const int t1x = (face < 4) ? 1 : 0;
		const int t1z = (face < 4) ? 0 : 1;
		const int t2y = (face < 2) ? 0 : 1;
		const int t2z = (face < 2) ? 1 : 0;

		while (planeMask) {
			const int plane = __builtin_ctz(planeMask);
			planeMask &= static_cast<uint16_t>(planeMask - 1);

			for (int v = 0; v < RENDERCHUNK_SIDE; ++v) {
				for (int u = 0; u < RENDERCHUNK_SIDE; ++u) {
					const int gIdx = v * RENDERCHUNK_SIDE + u;
					grid[gIdx].valid = false;
					used[gIdx] = 0;

					int lx, ly, lz;
					if (face < 2) {
						lx = u; ly = plane; lz = v;
					} else if (face < 4) {
						lx = u; ly = v; lz = plane;
					} else {
						lx = plane; ly = v; lz = u;
					}

					const size_t sbIdx = static_cast<size_t>(ly | (lz << 4) | (lx << 8));
					if (mGreedyVoxelLayer[sbIdx] != layerTag ||
					    (mGreedyVoxelFaceMask[sbIdx] & (1u << face)) == 0) {
						continue;
					}

					const FullBlock fb = sc
						? FullBlock(sc->mBlocks[sbIdx], sc->mData.get(sbIdx))
						: region.getBlockAndData(BlockPos(rcPos.x + lx, rcPos.y + ly, rcPos.z + lz));
					if (fb.id == BlockID::AIR) continue;

					const BlockPos wp(rcPos.x + lx, rcPos.y + ly, rcPos.z + lz);
					const int ntx = lx + 1 + nxOff;
					const int nty = ly + 1 + nyOff;
					const int ntz = lz + 1 + nzOff;
					const uint16_t nRaw = getCache(ntx, nty, ntz);
					const uint8_t nBlk = static_cast<uint8_t>(nRaw & 15u);
					const uint8_t nSky = static_cast<uint8_t>((nRaw >> 4) & 15u);
					if (nSky > 0) skyLit = true;

					GreedyCell& cell = grid[gIdx];
					cell.valid = true;
					++chunkInFaces;
					if (isWaterPass) ++cWaterIn;
					else if (fb.id == 2) ++cGrassIn;
					else if (fb.id == 3) ++cDirtIn;
					else if (fb.id == 1) ++cStoneIn;
					else if (tbl.isLeaf[fb.id]) ++cLeavesIn;
					else ++cOtherIn;

					float tintR = 1.0f, tintG = 1.0f, tintB = 1.0f, baseAlpha = 1.0f;
					bool enableAO = false;
					bool smoothLightHere = smoothLighting && (lodLevel < 3);
					const float shade = (isWaterPass ? kFaceShade[face] : (tbl.isEmissive[fb.id] ? kEmissiveFaceShade[face] : kFaceShade[face]));

					if (isWaterPass) {
						cell.id = BlockID(9);
						cell.rot = 0;
						cell.isIsotropic = false;
						cell.tintOnlyTop = false;
						cell.isWater = true;
						cell.isEmissive = false;
						const BlockGraphics* bg = BlockGraphics::mBlocks[9];
						cell.tex = bg ? bg->getTexture(face == Facing::DOWN ? Facing::DOWN : Facing::UP, 0) : TextureUVCoordinateSet();
						enableAO = false;
						cell.enableAO = false;
						smoothLightHere = smoothLightHere && (face == Facing::UP);

						const Color* biomeCols[9];
						const Color* firstCol = nullptr;
						bool useSimpleDepthBlend = true;
						for (int bx = -1; bx <= 1; ++bx) {
							for (int bz = -1; bz <= 1; ++bz) {
								const Color* bc = &region.getBiome(BlockPos(wp.x + bx, wp.y, wp.z + bz)).mWaterColor;
								if (firstCol && bc != firstCol) useSimpleDepthBlend = false;
								firstCol = bc;
								biomeCols[(bx + 1) * 3 + (bz + 1)] = bc;
							}
						}

						float cR[4], cG[4], cB[4], cA[4];
						if (useSimpleDepthBlend) {
							const float d = findWaterDepth(region, wp, firstCol->a);
							for (int ci = 0; ci < 4; ++ci) {
								cR[ci] = firstCol->r;
								cG[ci] = firstCol->g;
								cB[ci] = firstCol->b;
								cA[ci] = d;
							}
						} else {
							float inR[9], inG[9], inB[9], inA[9];
							for (int bx = -1; bx <= 1; ++bx) {
								for (int bz = -1; bz <= 1; ++bz) {
									const int idx9 = (bx + 1) * 3 + (bz + 1);
									const Color* bc = biomeCols[idx9];
									inR[idx9] = bc->r;
									inG[idx9] = bc->g;
									inB[idx9] = bc->b;
									inA[idx9] = findWaterDepth(region, BlockPos(wp.x + bx, wp.y, wp.z + bz), bc->a);
								}
							}
							// Canonical corners: 0=C00(0,0), 1=C10(1,0), 2=C11(1,1), 3=C01(0,1)
							static const int kCornerX[4] = { 0, 1, 1, 0 };
							static const int kCornerZ[4] = { 0, 0, 1, 1 };
							for (int ci = 0; ci < 4; ++ci) {
								const int x0 = kCornerX[ci], z0 = kCornerZ[ci];
								const int i00 = x0 * 3 + z0;
								const int i10 = (x0 + 1) * 3 + z0;
								const int i11 = (x0 + 1) * 3 + (z0 + 1);
								const int i01 = x0 * 3 + (z0 + 1);
								cR[ci] = (inR[i00] + inR[i10] + inR[i11] + inR[i01]) * 0.25f;
								cG[ci] = (inG[i00] + inG[i10] + inG[i11] + inG[i01]) * 0.25f;
								cB[ci] = (inB[i00] + inB[i10] + inB[i11] + inB[i01]) * 0.25f;
								cA[ci] = (inA[i00] + inA[i10] + inA[i11] + inA[i01]) * 0.25f;
							}
						}

						if (face == Facing::DOWN) {
							const uint32_t pk = packABGR(cR[0] * shade, cG[0] * shade, cB[0] * shade, cA[0] * shade);
							const uint8_t b4 = static_cast<uint8_t>(nBlk << 2);
							const uint8_t s4 = static_cast<uint8_t>(nSky << 2);
							for (int ci = 0; ci < 4; ++ci) {
								cell.c[ci] = pk;
								cell.blkL4[ci] = b4;
								cell.skyL4[ci] = s4;
							}
							cell.isFlat = true;
							cell.canMergeU = true;
							cell.canMergeV = true;
						} else {
							const uint16_t selfRaw = getCache(lx + 1, ly + 1, lz + 1);
							const uint8_t thisBlk = (selfRaw & 0x400u) ? static_cast<uint8_t>(selfRaw & 15u) : 11u;
							const uint8_t thisSky = (selfRaw & 0x400u) ? static_cast<uint8_t>((selfRaw >> 4) & 15u) : 11u;
							auto unpackWaterBlk = [&](uint16_t d) -> uint8_t {
								return (d & 0x400u) ? static_cast<uint8_t>(d & 15u) : thisBlk;
							};
							auto unpackWaterSky = [&](uint16_t d) -> uint8_t {
								return (d & 0x400u) ? static_cast<uint8_t>((d >> 4) & 15u) : thisSky;
							};
							const uint8_t l0b = unpackWaterBlk(nRaw);
							const uint8_t l0s = unpackWaterSky(nRaw);
							if (!smoothLightHere) {
								const uint8_t b4 = static_cast<uint8_t>(l0b << 2);
								const uint8_t s4 = static_cast<uint8_t>(l0s << 2);
								for (int ci = 0; ci < 4; ++ci) {
									cell.c[ci] = packABGR(cR[ci] * shade, cG[ci] * shade, cB[ci] * shade, cA[ci]);
									cell.blkL4[ci] = b4;
									cell.skyL4[ci] = s4;
								}
							} else {
								for (int ci = 0; ci < 4; ++ci) {
									const int sU = kCornerSU[ci];
									const int sV = kCornerSV[ci];
									const int d1x = sU * t1x, d1z = sU * t1z;
									const int d2y = sV * t2y, d2z = sV * t2z;
									const uint16_t d1 = getCache(ntx + d1x, nty, ntz + d1z);
									const uint16_t d2 = getCache(ntx, nty + d2y, ntz + d2z);
									const uint16_t dc = getCache(ntx + d1x, nty + d2y, ntz + d1z + d2z);
									cell.c[ci] = packABGR(cR[ci] * shade, cG[ci] * shade, cB[ci] * shade, cA[ci]);
									cell.blkL4[ci] = static_cast<uint8_t>(l0b + unpackWaterBlk(d1) + unpackWaterBlk(d2) + unpackWaterBlk(dc));
									cell.skyL4[ci] = static_cast<uint8_t>(l0s + unpackWaterSky(d1) + unpackWaterSky(d2) + unpackWaterSky(dc));
								}
							}
							cell.canMergeU = (cell.c[0] == cell.c[1] && cell.c[3] == cell.c[2] &&
							                  cell.blkL4[0] == cell.blkL4[1] && cell.blkL4[3] == cell.blkL4[2] &&
							                  cell.skyL4[0] == cell.skyL4[1] && cell.skyL4[3] == cell.skyL4[2]);
							cell.canMergeV = (cell.c[0] == cell.c[3] && cell.c[1] == cell.c[2] &&
							                  cell.blkL4[0] == cell.blkL4[3] && cell.blkL4[1] == cell.blkL4[2] &&
							                  cell.skyL4[0] == cell.skyL4[3] && cell.skyL4[1] == cell.skyL4[2]);
							cell.isFlat = cell.canMergeU && cell.canMergeV &&
							              (cell.c[0] == cell.c[3] && cell.blkL4[0] == cell.blkL4[3] && cell.skyL4[0] == cell.skyL4[3]);
						}
					} else {
						const Block& block = *Block::mBlocks[fb.id];
						const BlockGraphics& bg = *BlockGraphics::mBlocks[fb.id];
						const DataID calcData = block.calcVariant(region, wp, fb.data);
						const FacingID mappedFace = block.getMappedFace(static_cast<FacingID>(face), calcData);

						cell.id = fb.id;
						cell.tex = bg.getTexture(mappedFace, block.getVariant(calcData));
						cell.rot = 0;
						if (bg.getBlockShape() == BlockShape::TREE) {
							const int facing = Block::mQuartzBlock->getBlockState(BlockState::Direction).get<int>(calcData);
							if (facing == RotatedPillarBlock::FACING_X) {
								if (face == Facing::NORTH || face == Facing::SOUTH || face == Facing::UP || face == Facing::DOWN) {
									cell.rot = 1;
								}
							} else if (facing == RotatedPillarBlock::FACING_Z) {
								if (face == Facing::EAST || face == Facing::WEST) {
									cell.rot = 1;
								}
							}
						}
						cell.isIsotropic = (cell.rot == 0) && bg.isTextureIsotropic(static_cast<FacingID>(face));
						cell.isEmissive = tbl.isEmissive[fb.id];
						cell.tintOnlyTop = (fb.id == 2);
						cell.isWater = false;
						enableAO = smoothLightHere && tbl.useAO[fb.id];
						cell.enableAO = enableAO;

						if (!cell.tintOnlyTop || face == Facing::UP) {
							if (fb.id == 2) {
								const int colIdx = lz * 16 + lx;
								if (!grassColValid[colIdx]) {
									grassColCache[colIdx] = lc
										? static_cast<uint32_t>(lc->getGrassColor(ChunkBlockPos(lx, 0, lz)))
										: static_cast<uint32_t>(region.getGrassColor(wp));
									grassColValid[colIdx] = true;
								}
								const uint32_t gcol = grassColCache[colIdx];
								tintR = static_cast<float>((gcol >> 16) & 0xFF) / 255.0f;
								tintG = static_cast<float>((gcol >> 8) & 0xFF) / 255.0f;
								tintB = static_cast<float>(gcol & 0xFF) / 255.0f;
							} else {
								const int argb = block.getColor(region, wp, calcData);
								if (argb != -1 && argb != 0xFFFFFF) {
									tintR = static_cast<float>((argb >> 16) & 0xFF) / 255.0f;
									tintG = static_cast<float>((argb >> 8) & 0xFF) / 255.0f;
									tintB = static_cast<float>(argb & 0xFF) / 255.0f;
								}
							}
						}
						if (isSeasonsLayer) {
							tintB = 1.0f;
						}

						const float gamma = block.getBrightnessGamma();
						if (!enableAO) {
							const float flatShade = (gamma == 1.0f || face == Facing::UP) ? shade : std::pow(shade, gamma);
							const uint32_t pk = isSeasonsLayer
								? packABGR(tintR, tintG, tintB, flatShade)
								: packABGR(tintR * flatShade, tintG * flatShade, tintB * flatShade, baseAlpha);
							const uint8_t b4 = static_cast<uint8_t>(nBlk << 2);
							const uint8_t s4 = static_cast<uint8_t>(nSky << 2);
							for (int ci = 0; ci < 4; ++ci) {
								cell.c[ci] = pk;
								cell.blkL4[ci] = b4;
								cell.skyL4[ci] = s4;
							}
							cell.isFlat = true;
							cell.canMergeU = true;
							cell.canMergeV = true;
						} else {
							float topGamma = gamma;
							if (face >= Facing::NORTH) {
								const BlockID topId = (ly < 15 && sc) ? sc->mBlocks[sbIdx + 1] : region.getBlockID(wp.above());
								const Block* topBlk = Block::mBlocks[topId];
								const BlockGraphics* topBg = BlockGraphics::mBlocks[topId];
								if (topBlk && topBg && topBg->getBlockShape() != BlockShape::INVISIBLE) {
									topGamma = topBlk->getBrightnessGamma();
								}
							}
							const bool isTranslucent = tbl.isTranslucent[fb.id];
							const bool hasTint = (tintR != 1.0f || tintG != 1.0f || tintB != 1.0f);
							const uint16_t defRaw = Block::mSolid[fb.id] ? nRaw : getCache(lx + 1, ly + 1, lz + 1);
							const uint8_t defBlk = static_cast<uint8_t>(defRaw & 15u);
							const uint8_t defSky = static_cast<uint8_t>((defRaw >> 4) & 15u);
							const int fShade = (nRaw & 0x200u) ? 1 : 0;

							for (int ci = 0; ci < 4; ++ci) {
								const int sU = kCornerSU[ci];
								const int sV = kCornerSV[ci];
								const int d1x = sU * t1x, d1z = sU * t1z;
								const int d2y = sV * t2y, d2z = sV * t2z;
								const uint16_t d1 = getCache(ntx + d1x, nty, ntz + d1z);
								const uint16_t d2 = getCache(ntx, nty + d2y, ntz + d2z);
								const uint16_t dc = getCache(ntx + d1x, nty + d2y, ntz + d1z + d2z);

								const int s1 = (d1 & 0x200u) ? 1 : 0;
								const int s2 = (d2 & 0x200u) ? 1 : 0;
								const int b1 = (d1 & 0x100u) ? 1 : 0;
								const int b2 = (d2 & 0x100u) ? 1 : 0;
								const int scBit = (b1 && b2) ? 1 : ((dc & 0x200u) ? 1 : 0);
								const float ll = 1.0f - 0.2f * static_cast<float>(fShade + s1 + s2 + scBit);
								const float baseAo = shade * ll;
								const float useGamma = (face >= Facing::NORTH && (ci == 2 || ci == 3)) ? topGamma : gamma;
								const float ao = (useGamma == 1.0f) ? baseAo : std::pow(baseAo, useGamma);

								cell.c[ci] = isSeasonsLayer
									? packABGR(tintR, tintG, tintB, ao)
									: (hasTint
										? packABGR(tintR * ao, tintG * ao, tintB * ao, baseAlpha * ao)
										: packABGR(ao, ao, ao, isTranslucent ? 1.0f : ao));

								const uint16_t occDc = (b1 && b2)
									? (((face == Facing::DOWN || face == Facing::SOUTH || face == Facing::WEST) ? ((ci & 1) ? d2 : d1) : ((ci & 1) ? d1 : d2)))
									: dc;
								auto unpackBlk = [&](uint16_t d) -> uint8_t {
									return ((d & 0xFFu) == 0u) ? defBlk : static_cast<uint8_t>(d & 15u);
								};
								auto unpackSky = [&](uint16_t d) -> uint8_t {
									return ((d & 0xFFu) == 0u) ? defSky : static_cast<uint8_t>((d >> 4) & 15u);
								};
								cell.blkL4[ci] = static_cast<uint8_t>((defBlk + unpackBlk(d1) + unpackBlk(d2) + unpackBlk(occDc)) & ~3u);
								cell.skyL4[ci] = static_cast<uint8_t>((defSky + unpackSky(d1) + unpackSky(d2) + unpackSky(occDc)) & ~3u);
							}

							cell.canMergeU = (cell.c[0] == cell.c[1] && cell.c[3] == cell.c[2] &&
							                  cell.blkL4[0] == cell.blkL4[1] && cell.blkL4[3] == cell.blkL4[2] &&
							                  cell.skyL4[0] == cell.skyL4[1] && cell.skyL4[3] == cell.skyL4[2]);
							cell.canMergeV = (cell.c[0] == cell.c[3] && cell.c[1] == cell.c[2] &&
							                  cell.blkL4[0] == cell.blkL4[3] && cell.blkL4[1] == cell.blkL4[2] &&
							                  cell.skyL4[0] == cell.skyL4[3] && cell.skyL4[1] == cell.skyL4[2]);
							cell.isFlat = cell.canMergeU && cell.canMergeV &&
							              (cell.c[0] == cell.c[3] && cell.blkL4[0] == cell.blkL4[3] && cell.skyL4[0] == cell.skyL4[3]);
						}
					}
				}
			}

			for (int v = 0; v < RENDERCHUNK_SIDE; ++v) {
				for (int u = 0; u < RENDERCHUNK_SIDE; ++u) {
					const int gIdx = v * RENDERCHUNK_SIDE + u;
					if (used[gIdx] || !grid[gIdx].valid) continue;

					const GreedyCell& seed = grid[gIdx];
					int w = 1;
					int h = 1;

					if (!isWaterPass && mGreedyCellBuffer.size() < 4095) {
						while (u + w < RENDERCHUNK_SIDE &&
						       mGreedyCellBuffer.size() + static_cast<size_t>(w + 1) <= 4095) {
							const int qIdx = v * RENDERCHUNK_SIDE + (u + w);
							if (used[qIdx] || !greedySameBase(seed, grid[qIdx], lodLevel)) break;
							++w;
						}
						while (v + h < RENDERCHUNK_SIDE &&
						       mGreedyCellBuffer.size() + static_cast<size_t>(w * (h + 1)) <= 4095) {
							bool rowOk = true;
							for (int du = 0; du < w; ++du) {
								const int qIdx = (v + h) * RENDERCHUNK_SIDE + (u + du);
								if (used[qIdx] || !greedySameBase(seed, grid[qIdx], lodLevel)) {
									rowOk = false;
									break;
								}
							}
							if (!rowOk) break;
							++h;
						}
					} else if (seed.isFlat) {
						while (u + w < RENDERCHUNK_SIDE) {
							const int qIdx = v * RENDERCHUNK_SIDE + (u + w);
							if (used[qIdx] || !greedyCanMergeFlat(seed, grid[qIdx], lodLevel)) break;
							++w;
						}
						while (v + h < RENDERCHUNK_SIDE) {
							bool rowOk = true;
							for (int du = 0; du < w; ++du) {
								const int qIdx = (v + h) * RENDERCHUNK_SIDE + (u + du);
								if (used[qIdx] || !greedyCanMergeFlat(seed, grid[qIdx], lodLevel)) {
									rowOk = false;
									break;
								}
							}
							if (!rowOk) break;
							++h;
						}
					} else if (seed.canMergeU) {
						while (u + w < RENDERCHUNK_SIDE) {
							const int qIdx = v * RENDERCHUNK_SIDE + (u + w);
							if (used[qIdx] || !greedyCanMergeU(seed, grid[qIdx], lodLevel)) break;
							++w;
						}
					} else if (seed.canMergeV) {
						while (v + h < RENDERCHUNK_SIDE) {
							const int qIdx = (v + h) * RENDERCHUNK_SIDE + u;
							if (used[qIdx] || !greedyCanMergeV(seed, grid[qIdx], lodLevel)) break;
							++h;
						}
					}

					for (int dv = 0; dv < h; ++dv) {
						for (int du = 0; du < w; ++du) {
							used[(v + dv) * RENDERCHUNK_SIDE + (u + du)] = 1;
						}
					}

					float aX, aY, aZ, bX, bY, bZ;
					if (face < 2) {
						aX = static_cast<float>(rcPos.x + u);
						bX = aX + static_cast<float>(w);
						aY = static_cast<float>(rcPos.y + plane);
						bY = aY + topYOffset;
						aZ = static_cast<float>(rcPos.z + v);
						bZ = aZ + static_cast<float>(h);
					} else if (face < 4) {
						aX = static_cast<float>(rcPos.x + u);
						bX = aX + static_cast<float>(w);
						aY = static_cast<float>(rcPos.y + v);
						bY = aY + static_cast<float>(h - 1) + topYOffset;
						aZ = static_cast<float>(rcPos.z + plane);
						bZ = aZ + 1.0f;
					} else {
						aX = static_cast<float>(rcPos.x + plane);
						bX = aX + 1.0f;
						aY = static_cast<float>(rcPos.y + v);
						bY = aY + static_cast<float>(h - 1) + topYOffset;
						aZ = static_cast<float>(rcPos.z + u);
						bZ = aZ + static_cast<float>(w);
					}

					const bool isMulti = (w > 1 || h > 1);
					if (!isMulti) {
						uint8_t cpuRot = seed.rot;
						if (seed.isIsotropic) {
							int lx = 0, ly = 0, lz = 0;
							if (face < 2) { lx = u; ly = plane; lz = v; }
							else if (face < 4) { lx = u; ly = v; lz = plane; }
							else { lx = plane; ly = v; lz = u; }
							const int wx = rcPos.x + lx, wy = rcPos.y + ly, wz = rcPos.z + lz;
							int32_t hs = (wx * 3129871) ^ (wz * 116129781l) ^ wy;
							hs = hs * hs * 42317861 + hs * 11;
							cpuRot = static_cast<uint8_t>((hs >> 24) & 0x3);
						}
						emitGreedyQuad(t, face, aX, aY, aZ, bX, bY, bZ, seed, false, cpuRot);
					} else {
						bool isVertexExact = false;
						if ((h == 1 && seed.canMergeU) || (w == 1 && seed.canMergeV) || seed.isFlat) {
							isVertexExact = true;
							for (int dv = 0; dv < h && isVertexExact; ++dv) {
								for (int du = 0; du < w; ++du) {
									const GreedyCell& cell = grid[(v + dv) * RENDERCHUNK_SIDE + (u + du)];
									for (int ci = 0; ci < 4; ++ci) {
										if (cell.c[ci] != seed.c[ci] ||
										    cell.blkL4[ci] != seed.blkL4[ci] ||
										    cell.skyL4[ci] != seed.skyL4[ci]) {
											isVertexExact = false;
											break;
										}
									}
									if (!isVertexExact) break;
								}
							}
						}

						if (isVertexExact) {
							emitGreedyQuad(t, face, aX, aY, aZ, bX, bY, bZ, seed, true, 0, false, 0, 0);
						} else {
							mTboRenderLayerMask |= (1u << rlIdx);
							const uint16_t baseOffset = static_cast<uint16_t>(mGreedyCellBuffer.size());
							for (int dv = 0; dv < h; ++dv) {
								for (int du = 0; du < w; ++du) {
									const GreedyCell& cell = grid[(v + dv) * RENDERCHUNK_SIDE + (u + du)];
									auto packCorner = [&](int idx) -> uint32_t {
										const uint32_t blkL = static_cast<uint32_t>((cell.blkL4[idx] >> 2) & 15u);
										const uint32_t skyL = static_cast<uint32_t>((cell.skyL4[idx] >> 2) & 15u);
										if (isSeasonsLayer) {
											return (cell.c[idx] & 0x0000FFFFu) |
											       ((cell.c[idx] >> 8) & 0x00FF0000u) |
											       (blkL << 24) |
											       (skyL << 28);
										}
										return (cell.c[idx] & 0x00FFFFFFu) |
										       (blkL << 24) |
										       (skyL << 28);
									};
									mGreedyCellBuffer.push_back({
										packCorner(0),
										packCorner(1),
										packCorner(3),
										packCorner(2)
									});
								}
							}
							const uint16_t cellU1 = static_cast<uint16_t>(
								((w - 1) << 12) | (v << 8) | (u << 4) | (1 + face));
							const uint16_t cellV1 = static_cast<uint16_t>(
								(baseOffset << 4) | 8u | (seed.isIsotropic ? 4u : 0u) | (seed.rot & 3u));
							emitGreedyQuad(t, face, aX, aY, aZ, bX, bY, bZ, seed, true, 0, true, cellU1, cellV1);
						}
					}

					++chunkOutQuads;
					if (isWaterPass) ++cWaterOut;
					else if (seed.id == 2) ++cGrassOut;
					else if (seed.id == 3) ++cDirtOut;
					else if (seed.id == 1) ++cStoneOut;
					else if (tbl.isLeaf[seed.id]) ++cLeavesOut;
					else ++cOtherOut;
				}
			}
		}
	}

	if (sLogStats && chunkInFaces > 0) {
		const uint64_t totIn = sGreedyInputFaces.fetch_add(chunkInFaces, std::memory_order_relaxed) + chunkInFaces;
		const uint64_t totOut = sGreedyOutputQuads.fetch_add(chunkOutQuads, std::memory_order_relaxed) + chunkOutQuads;
		const uint64_t gIn = sGrassIn.fetch_add(cGrassIn, std::memory_order_relaxed) + cGrassIn;
		const uint64_t gOut = sGrassOut.fetch_add(cGrassOut, std::memory_order_relaxed) + cGrassOut;
		const uint64_t dIn = sDirtIn.fetch_add(cDirtIn, std::memory_order_relaxed) + cDirtIn;
		const uint64_t dOut = sDirtOut.fetch_add(cDirtOut, std::memory_order_relaxed) + cDirtOut;
		const uint64_t sIn = sStoneIn.fetch_add(cStoneIn, std::memory_order_relaxed) + cStoneIn;
		const uint64_t sOut = sStoneOut.fetch_add(cStoneOut, std::memory_order_relaxed) + cStoneOut;
		const uint64_t wIn = sWaterIn.fetch_add(cWaterIn, std::memory_order_relaxed) + cWaterIn;
		const uint64_t wOut = sWaterOut.fetch_add(cWaterOut, std::memory_order_relaxed) + cWaterOut;
		const uint64_t lIn = sLeavesIn.fetch_add(cLeavesIn, std::memory_order_relaxed) + cLeavesIn;
		const uint64_t lOut = sLeavesOut.fetch_add(cLeavesOut, std::memory_order_relaxed) + cLeavesOut;
		const uint64_t oIn = sOtherIn.fetch_add(cOtherIn, std::memory_order_relaxed) + cOtherIn;
		const uint64_t oOut = sOtherOut.fetch_add(cOtherOut, std::memory_order_relaxed) + cOtherOut;
		if ((totOut % 1000) < chunkOutQuads || cWaterOut > 0 || cLeavesOut > 0) {
			LOGI("GREEDY_STATS: total=%llu->%llu (%.2fx) grass=%llu->%llu dirt=%llu->%llu stone=%llu->%llu water=%llu->%llu leaves=%llu->%llu other=%llu->%llu\n",
			     static_cast<unsigned long long>(totIn), static_cast<unsigned long long>(totOut),
			     totOut ? static_cast<double>(totIn) / static_cast<double>(totOut) : 1.0,
			     static_cast<unsigned long long>(gIn), static_cast<unsigned long long>(gOut),
			     static_cast<unsigned long long>(dIn), static_cast<unsigned long long>(dOut),
			     static_cast<unsigned long long>(sIn), static_cast<unsigned long long>(sOut),
			     static_cast<unsigned long long>(wIn), static_cast<unsigned long long>(wOut),
			     static_cast<unsigned long long>(lIn), static_cast<unsigned long long>(lOut),
			     static_cast<unsigned long long>(oIn), static_cast<unsigned long long>(oOut));
		}
	}
}

bool RenderChunkBuilder::_tessellateQueues(RenderChunk& rc, BlockSource& region, uint8_t lodLevel, bool greedyMeshing, bool smoothLighting, bool transparentLeaves) {
	auto& t = renderer->getTessellator();
	bool cacheReset = false;

	const BlockPos rcPos = rc.getPosition();
	LevelChunk* lc = region.getChunk(rcPos);
	const size_t subY = static_cast<size_t>(rcPos.y >> 4);
	const auto subs = lc ? lc->getSubChunks() : buffer_span<std::unique_ptr<LevelChunk::SubChunk>>{};
	const LevelChunk::SubChunk* sc = (subY < subs.size()) ? subs[subY].get() : nullptr;

	if (greedyMeshing && mHasAnyGreedy) {
		_buildLightAoCache(rcPos, region);
	}

	for (int l = 0; l < BlockRenderLayer::_RENDERLAYER_COUNT; ++l) {
		const bool greedyHere = greedyMeshing && mHasGreedyLayer[l];

		if (!mQueues[l].empty() || greedyHere) {
			if (!t.isTessellating()) {	//lazy-begin tessellating
 				t.begin(12000);	//made up average
				t.setOffset(-rcPos);
			}

			auto existingVertices = t.getVertexCount();

			if (greedyHere) {
				_greedyMeshPass(rc, region, lodLevel, smoothLighting, transparentLeaves, static_cast<BlockRenderLayer>(l));
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

void RenderChunkBuilder::build(RenderChunk& rc, bool transparentLeaves, bool smoothLighting, uint8_t lodLevel, bool greedyMeshing, bool /*allowCluster*/) {

	//create a new BlockSource
	BlockSource region(localSource->getLevel(), localSource->getDimension(), *localSource, false);

	DEBUG_ASSERT(rc.isBuildState(RenderChunk::BuildState::Building), "Wrong state, wrong thread");

	rc.setGreedyClusterOwner(false);
	rc.setBuiltWithCluster(false);
	mGreedyCellBuffer.clear();
	mTboRenderLayerMask = 0;

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

	if (!_sortBlocks(region, rc, VE, transparentLeaves, lodLevel, greedyMeshing)) {
		return;
	}

	averageSkyLight = skyLit ? 15.f : 0;

	if (!_tessellateQueues(rc, region, lodLevel, greedyMeshing, smoothLighting, transparentLeaves)) {
		return;
	}

	_buildRanges();

	if (VE) {
		mVisibility = VE->finish();
	}
}
