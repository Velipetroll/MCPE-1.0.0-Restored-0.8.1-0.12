#include "common_header.h"
#include "client/game/WorldLighting.h"
#include "world/level/BlockSource.h"
#include "world/level/ChunkBlockPos.h"
#include "world/level/LightLayer.h"
#include "world/level/block/Block.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/dimension/Dimension.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace WorldLighting {

namespace {

struct LightNode {
	int x, y, z;
	uint8_t light;
};

constexpr int kDirs[6][3] = {
	{  1,  0,  0 },
	{ -1,  0,  0 },
	{  0,  1,  0 },
	{  0, -1,  0 },
	{  0,  0,  1 },
	{  0,  0, -1 }
};

void runSkyBfs(BlockSource& region,
               const LightLayer& skyLayer,
               std::queue<LightNode>& q,
               int minX, int maxX,
               int minY, int maxY,
               int minZ, int maxZ,
               std::unordered_set<ChunkPos>* touched) {
	while (!q.empty()) {
		LightNode cur = q.front();
		q.pop();
		if (cur.light <= 1) continue;

		for (const auto& d : kDirs) {
			int nx = cur.x + d[0];
			int ny = cur.y + d[1];
			int nz = cur.z + d[2];
			if (nx < minX || nx > maxX || ny < minY || ny > maxY || nz < minZ || nz > maxZ) continue;

			BlockPos np(nx, ny, nz);
			LevelChunk* nlc = region.getChunk(np);
			if (!nlc) continue;

			BlockID nid = nlc->getBlock(ChunkBlockPos(np));
			int opacity = std::max(1, static_cast<int>(Block::mLightBlock[nid]));
			int nextLight = static_cast<int>(cur.light) - opacity;
			if (nextLight <= 0) continue;

			ChunkBlockPos cbp(np);
			uint8_t existing = static_cast<uint8_t>(nlc->getBrightness(skyLayer, cbp));
			if (nextLight > existing) {
				nlc->setBrightness(skyLayer, cbp, Brightness(static_cast<uint8_t>(nextLight)));
				if (touched) touched->insert(ChunkPos(np));
				if (nextLight > 1) {
					q.push({ nx, ny, nz, static_cast<uint8_t>(nextLight) });
				}
			}
		}
	}
}

void runBlockBfs(BlockSource& region,
                 std::queue<LightNode>& q,
                 int minX, int maxX,
                 int minY, int maxY,
                 int minZ, int maxZ,
                 std::unordered_set<ChunkPos>* touched) {
	while (!q.empty()) {
		LightNode cur = q.front();
		q.pop();
		if (cur.light <= 1) continue;

		for (const auto& d : kDirs) {
			int nx = cur.x + d[0];
			int ny = cur.y + d[1];
			int nz = cur.z + d[2];
			if (nx < minX || nx > maxX || ny < minY || ny > maxY || nz < minZ || nz > maxZ) continue;

			BlockPos np(nx, ny, nz);
			LevelChunk* nlc = region.getChunk(np);
			if (!nlc) continue;

			BlockID nid = nlc->getBlock(ChunkBlockPos(np));
			int opacity = std::max(1, static_cast<int>(Block::mLightBlock[nid]));
			int nextLight = static_cast<int>(cur.light) - opacity;
			if (nextLight <= 0) continue;

			ChunkBlockPos cbp(np);
			uint8_t existing = static_cast<uint8_t>(nlc->getBrightness(LightLayer::BLOCK, cbp));
			if (nextLight > existing) {
				nlc->setBrightness(LightLayer::BLOCK, cbp, Brightness(static_cast<uint8_t>(nextLight)));
				if (touched) touched->insert(ChunkPos(np));
				if (nextLight > 1) {
					q.push({ nx, ny, nz, static_cast<uint8_t>(nextLight) });
				}
			}
		}
	}
}

} // namespace

void propagateChunkLighting(BlockSource& region, const ChunkPos& cp) {
	LevelChunk* lc = region.getChunk(cp);
	if (!lc) return;

	const LightLayer& skyLayer = region.getDimension().getSkyLightLayer();
	int baseX = cp.x * 16;
	int baseZ = cp.z * 16;
	int allocH = std::min(126, static_cast<int>(lc->getAllocatedHeight()) + 1);

	std::queue<LightNode> skyQ;
	std::queue<LightNode> blkQ;

	// Scan chunk interior + 1-block horizontal border for light emitters & sky-bleed seeds
	for (int lx = -1; lx <= 16; ++lx) {
		for (int lz = -1; lz <= 16; ++lz) {
			int wx = baseX + lx;
			int wz = baseZ + lz;
			BlockPos colPos(wx, 0, wz);
			LevelChunk* colLc = (lx >= 0 && lx < 16 && lz >= 0 && lz < 16) ? lc : region.getChunk(colPos);
			if (!colLc) continue;

			int colTop = std::min(126, std::max(allocH, static_cast<int>(colLc->getAllocatedHeight()) + 1));
			for (int y = 0; y <= colTop; ++y) {
				ChunkBlockPos cbp(wx & 15, y, wz & 15);
				BlockID id = colLc->getBlock(cbp);
				uint8_t emit = static_cast<uint8_t>(Block::mLightEmission[id]);
				if (emit > 0) {
					uint8_t curBlk = static_cast<uint8_t>(colLc->getBrightness(LightLayer::BLOCK, cbp));
					if (emit > curBlk) {
						colLc->setBrightness(LightLayer::BLOCK, cbp, Brightness(emit));
					}
					blkQ.push({ wx, y, wz, std::max(emit, curBlk) });
				} else if (lx < 0 || lx >= 16 || lz < 0 || lz >= 16) {
					uint8_t b = static_cast<uint8_t>(colLc->getBrightness(LightLayer::BLOCK, cbp));
					if (b > 1) {
						blkQ.push({ wx, y, wz, b });
					}
				}

				uint8_t sky = static_cast<uint8_t>(colLc->getBrightness(skyLayer, cbp));
				if (sky > 1 && Block::mLightBlock[id] < Brightness::MAX) {
					// Seed if adjacent block is darker so BFS fills overhangs/caves
					bool needSeed = false;
					if (lx < 0 || lx >= 16 || lz < 0 || lz >= 16) {
						needSeed = true;
					} else {
						for (int i = 0; i < 4; ++i) {
							int ax = wx + kDirs[i][0];
							int az = wz + kDirs[i][2];
							if (ax < baseX || ax >= baseX + 16 || az < baseZ || az >= baseZ + 16) {
								needSeed = true;
								break;
							}
							uint8_t ns = static_cast<uint8_t>(colLc->getBrightness(skyLayer, ChunkBlockPos(ax & 15, y, az & 15)));
							if (ns + 1 < sky) {
								needSeed = true;
								break;
							}
						}
					}
					if (needSeed) {
						skyQ.push({ wx, y, wz, sky });
					}
				}
			}
		}
	}

	const int minX = baseX - 15;
	const int maxX = baseX + 30;
	const int minZ = baseZ - 15;
	const int maxZ = baseZ + 30;
	runSkyBfs(region, skyLayer, skyQ, minX, maxX, 0, 127, minZ, maxZ, nullptr);
	runBlockBfs(region, blkQ, minX, maxX, 0, 127, minZ, maxZ, nullptr);
}

void updateLocalLighting(BlockSource& region,
                         const BlockPos& center,
                         int blockRadius,
                         std::unordered_set<ChunkPos>* outTouchedChunks) {
	const LightLayer& skyLayer = region.getDimension().getSkyLightLayer();
	const int r = std::max(4, std::min(15, blockRadius));
	const int minX = center.x - r;
	const int maxX = center.x + r;
	const int minZ = center.z - r;
	const int maxZ = center.z + r;
	const int minY = std::max(0, center.y - r);
	const int maxY = std::min(127, center.y + r);

	std::queue<LightNode> skyQ;
	std::queue<LightNode> blkQ;

	// Pass 1: Recompute vertical skylight columns & clear local box lights
	for (int x = minX; x <= maxX; ++x) {
		for (int z = minZ; z <= maxZ; ++z) {
			LevelChunk* lc = region.getChunk(BlockPos(x, 0, z));
			if (!lc) continue;

			int sky = 15;
			for (int y = 127; y >= 0; --y) {
				ChunkBlockPos cbp(x & 15, y, z & 15);
				BlockID id = lc->getBlock(cbp);
				int lb = static_cast<int>(Block::mLightBlock[id]);

				int targetSky = 0;
				if (sky > 0) {
					if (lb >= sky) {
						sky = 0;
						targetSky = 0;
					} else {
						sky -= lb;
						targetSky = sky;
					}
				}

				if (y >= minY && y <= maxY) {
					uint8_t oldSky = static_cast<uint8_t>(lc->getBrightness(skyLayer, cbp));
					if (oldSky != static_cast<uint8_t>(targetSky)) {
						lc->setBrightness(skyLayer, cbp, Brightness(static_cast<uint8_t>(targetSky)));
						if (outTouchedChunks) outTouchedChunks->insert(ChunkPos(BlockPos(x, y, z)));
					}
					if (targetSky > 1) {
						skyQ.push({ x, y, z, static_cast<uint8_t>(targetSky) });
					}

					uint8_t emit = static_cast<uint8_t>(Block::mLightEmission[id]);
					uint8_t oldBlk = static_cast<uint8_t>(lc->getBrightness(LightLayer::BLOCK, cbp));
					if (oldBlk != emit) {
						lc->setBrightness(LightLayer::BLOCK, cbp, Brightness(emit));
						if (outTouchedChunks) outTouchedChunks->insert(ChunkPos(BlockPos(x, y, z)));
					}
					if (emit > 1) {
						blkQ.push({ x, y, z, emit });
					}
				} else if (y == maxY + 1 && targetSky > 1) {
					skyQ.push({ x, y, z, static_cast<uint8_t>(targetSky) });
				} else if (y == minY - 1 && targetSky > 1) {
					skyQ.push({ x, y, z, static_cast<uint8_t>(targetSky) });
				}
			}
		}
	}

	// Pass 2: Seed from outer 1-block shell around the cleared box so external light spills back in
	const int shellMinX = minX - 1;
	const int shellMaxX = maxX + 1;
	const int shellMinZ = minZ - 1;
	const int shellMaxZ = maxZ + 1;
	const int shellMinY = std::max(0, minY - 1);
	const int shellMaxY = std::min(127, maxY + 1);

	for (int x = shellMinX; x <= shellMaxX; ++x) {
		for (int z = shellMinZ; z <= shellMaxZ; ++z) {
			bool isXZBorder = (x == shellMinX || x == shellMaxX || z == shellMinZ || z == shellMaxZ);
			LevelChunk* lc = region.getChunk(BlockPos(x, 0, z));
			if (!lc) continue;
			for (int y = shellMinY; y <= shellMaxY; ++y) {
				bool isYBorder = (y == shellMinY || y == shellMaxY);
				if (!isXZBorder && !isYBorder) continue;

				ChunkBlockPos cbp(x & 15, y, z & 15);
				uint8_t sky = static_cast<uint8_t>(lc->getBrightness(skyLayer, cbp));
				if (sky > 1) {
					skyQ.push({ x, y, z, sky });
				}
				uint8_t blk = static_cast<uint8_t>(lc->getBrightness(LightLayer::BLOCK, cbp));
				BlockID id = lc->getBlock(cbp);
				uint8_t emit = static_cast<uint8_t>(Block::mLightEmission[id]);
				uint8_t seedBlk = std::max(blk, emit);
				if (seedBlk > 1) {
					blkQ.push({ x, y, z, seedBlk });
				}
			}
		}
	}

	runSkyBfs(region, skyLayer, skyQ, minX, maxX, minY, maxY, minZ, maxZ, outTouchedChunks);
	runBlockBfs(region, blkQ, minX, maxX, minY, maxY, minZ, maxZ, outTouchedChunks);
}

float sampleWorldBrightness(BlockSource& region, const BlockPos& pos, float sunFactor) {
	if (pos.y < 0) return 0.12f;
	if (pos.y >= 128) return std::max(0.16f, sunFactor);
	BrightnessPair bp = region.getLightColor(pos);
	float skyEff = (bp.sky == 0 ? 0.0f : std::pow(0.82f, static_cast<float>(15 - bp.sky))) * sunFactor;
	float blkEff = (bp.block == 0 ? 0.0f : std::pow(0.80f, static_cast<float>(15 - bp.block)));
	return std::min(1.0f, std::max(0.12f, std::max(skyEff, blkEff)));
}

} // namespace WorldLighting
