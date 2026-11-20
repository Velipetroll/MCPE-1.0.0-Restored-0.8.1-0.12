#include "common_header.h"
#include "client/game/WorldLighting.h"
#include "world/level/BlockSource.h"
#include "world/level/ChunkBlockPos.h"
#include "world/level/LightLayer.h"
#include "world/level/block/Block.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/dimension/Dimension.h"
#include "util/PerfTimer.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <vector>

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

inline int floorDiv16(int v) {
	return v >= 0 ? (v >> 4) : ~((~v) >> 4);
}

struct ChunkGrid3x3 {
	LevelChunk* c[3][3]{};
	int baseX = 0;
	int baseZ = 0;

	ChunkGrid3x3(BlockSource& region, int cx, int cz)
		: baseX(cx)
		, baseZ(cz) {
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dz = -1; dz <= 1; ++dz) {
				c[dx + 1][dz + 1] = region.getChunk(ChunkPos(cx + dx, cz + dz));
			}
		}
	}

	inline LevelChunk* atChunk(int cx, int cz) const {
		int dx = cx - baseX + 1;
		int dz = cz - baseZ + 1;
		if (static_cast<unsigned>(dx) >= 3u || static_cast<unsigned>(dz) >= 3u) {
			return nullptr;
		}
		return c[dx][dz];
	}

	inline LevelChunk* atWorld(int wx, int wz) const {
		return atChunk(floorDiv16(wx), floorDiv16(wz));
	}
};

inline void markTouchedSubChunkBit(TouchedSubChunkMap* subMap, int cx, int cz, int sy) {
	if (subMap && static_cast<unsigned>(sy) < 8u) {
		(*subMap)[ChunkPos(cx, cz)] |= static_cast<uint8_t>(1u << sy);
	}
}

inline void markTouched(std::unordered_set<ChunkPos>* touched,
                        TouchedSubChunkMap* subMap,
                        int wx, int y, int wz) {
	const int cx = floorDiv16(wx);
	const int cz = floorDiv16(wz);
	const ChunkPos cp(cx, cz);
	if (touched) {
		touched->insert(cp);
	}
	if (subMap) {
		const int sy = y >> 4;
		const int ly = y & 15;
		const int lx = wx & 15;
		const int lz = wz & 15;
		markTouchedSubChunkBit(subMap, cx, cz, sy);
		if (ly == 0 && sy > 0) markTouchedSubChunkBit(subMap, cx, cz, sy - 1);
		if (ly == 15 && sy < 7) markTouchedSubChunkBit(subMap, cx, cz, sy + 1);
		if (lx == 0) {
			if (touched) touched->insert(ChunkPos(cx - 1, cz));
			markTouchedSubChunkBit(subMap, cx - 1, cz, sy);
		} else if (lx == 15) {
			if (touched) touched->insert(ChunkPos(cx + 1, cz));
			markTouchedSubChunkBit(subMap, cx + 1, cz, sy);
		}
		if (lz == 0) {
			if (touched) touched->insert(ChunkPos(cx, cz - 1));
			markTouchedSubChunkBit(subMap, cx, cz - 1, sy);
		} else if (lz == 15) {
			if (touched) touched->insert(ChunkPos(cx, cz + 1));
			markTouchedSubChunkBit(subMap, cx, cz + 1, sy);
		}
	}
}

inline size_t subBlockIdx(int lx, int y, int lz) {
	return static_cast<size_t>((y & 15) | ((lz & 15) << 4) | ((lx & 15) << 8));
}

inline BlockID getBlockFast(LevelChunk* lc, int lx, int y, int lz) {
	const size_t sy = static_cast<size_t>(y >> 4);
	const auto& subs = lc->getSubChunks();
	if (sy >= subs.size()) {
		return BlockID::AIR;
	}
	return subs[sy]->mBlocks[subBlockIdx(lx, y, lz)];
}

inline uint8_t getLightFast(LevelChunk* lc, const LightLayer& layer, int lx, int y, int lz) {
	const size_t sy = static_cast<size_t>(y >> 4);
	const auto& subs = lc->getSubChunks();
	if (sy >= subs.size()) {
		return static_cast<uint8_t>(layer.getSurrounding());
	}
	const auto& dataLayer = layer.isSky() ? subs[sy]->mSkyLight : subs[sy]->mBlockLight;
	return dataLayer.get(subBlockIdx(lx, y, lz));
}

void runLightBfs(const ChunkGrid3x3& grid,
                 const LightLayer& layer,
                 std::vector<LightNode>& q,
                 int minX, int maxX,
                 int minY, int maxY,
                 int minZ, int maxZ,
                 std::unordered_set<ChunkPos>* touched,
                 TouchedSubChunkMap* touchedSubChunks) {
	for (size_t head = 0; head < q.size(); ++head) {
		const LightNode cur = q[head];
		if (cur.light <= 1) continue;

		for (const auto& d : kDirs) {
			const int nx = cur.x + d[0];
			const int ny = cur.y + d[1];
			const int nz = cur.z + d[2];
			if (nx < minX || nx > maxX || ny < minY || ny > maxY || nz < minZ || nz > maxZ) continue;

			LevelChunk* nlc = grid.atWorld(nx, nz);
			if (!nlc) continue;

			const int nlx = nx & 15;
			const int nlz = nz & 15;
			const BlockID nid = getBlockFast(nlc, nlx, ny, nlz);
			const int opacity = std::max(1, static_cast<int>(Block::mLightBlock[nid]));
			const int nextLight = static_cast<int>(cur.light) - opacity;
			if (nextLight <= 0) continue;

			const uint8_t existing = getLightFast(nlc, layer, nlx, ny, nlz);
			if (nextLight > existing) {
				nlc->setBrightness(layer, ChunkBlockPos(nlx, ny, nlz), Brightness(static_cast<uint8_t>(nextLight)));
				markTouched(touched, touchedSubChunks, nx, ny, nz);
				if (nextLight > 1) {
					q.push_back({ nx, ny, nz, static_cast<uint8_t>(nextLight) });
				}
			}
		}
	}
}

struct SubChunkScanFlags {
	uint16_t hasBlocks = 0;
	uint16_t skyAllOne = 0;
	uint16_t skyAllZero = 0;
	uint16_t blkHasLight = 0;

	void build(const LevelChunk* lc) {
		hasBlocks = 0;
		skyAllOne = 0;
		skyAllZero = 0;
		blkHasLight = 0;
		if (!lc) return;
		const auto& subs = lc->getSubChunks();
		const int count = std::min<int>(16, static_cast<int>(subs.size()));
		for (int sy = 0; sy < count; ++sy) {
			const auto& sc = subs[sy];
			const uint16_t bit = static_cast<uint16_t>(1u << sy);
			if (sc->hasBlocks()) hasBlocks |= bit;
			if (Util::isAllOne(sc->mSkyLight.span())) skyAllOne |= bit;
			else if (Util::isAllZero(sc->mSkyLight.span())) skyAllZero |= bit;
			if (!Util::isAllZero(sc->mBlockLight.span())) blkHasLight |= bit;
		}
	}
};

void scanExteriorBorderColumn(LevelChunk* lc,
                              const SubChunkScanFlags& flags,
                              int lx, int lz,
                              int wx, int wz,
                              int maxH,
                              int& outTopSolid,
                              int& outMinSkyY,
                              std::vector<LightNode>& blkQ) {
	outTopSolid = 0;
	outMinSkyY = maxH + 1;
	const auto& subs = lc->getSubChunks();
	if (subs.empty()) {
		outMinSkyY = 0;
		return;
	}

	const int maxSy = std::min(static_cast<int>(subs.size()) - 1, maxH >> 4);
	bool skyDone = false;
	for (int sy = maxSy; sy >= 0; --sy) {
		const auto& sc = subs[sy];
		const int baseY = sy << 4;
		const int topLy = std::min(15, maxH - baseY);
		const uint16_t bit = static_cast<uint16_t>(1u << sy);
		const bool hasBlk = (flags.hasBlocks & bit) != 0;
		const bool isSkyOne = (flags.skyAllOne & bit) != 0;
		const bool isSkyZero = (flags.skyAllZero & bit) != 0;
		const bool hasSky = !isSkyZero;
		const bool hasBlkLight = (flags.blkHasLight & bit) != 0;

		if (!hasBlk && !hasBlkLight) {
			if (!skyDone) {
				if (isSkyOne) {
					outMinSkyY = baseY;
				} else if (hasSky) {
					for (int ly = topLy; ly >= 0; --ly) {
						if (sc->mSkyLight.get(subBlockIdx(lx, ly, lz)) > 1) {
							outMinSkyY = baseY + ly;
						}
					}
				} else {
					skyDone = true;
				}
			}
			continue;
		}

		for (int ly = topLy; ly >= 0; --ly) {
			const int y = baseY + ly;
			const size_t sbIdx = subBlockIdx(lx, ly, lz);
			if (hasBlk) {
				const BlockID id = sc->mBlocks[sbIdx];
				if (outTopSolid == 0 && Block::mLightBlock[id] > 0) {
					outTopSolid = y;
				}
				const uint8_t emit = static_cast<uint8_t>(Block::mLightEmission[id]);
				if (emit > 1) {
					blkQ.push_back({ wx, y, wz, emit });
				} else if (hasBlkLight) {
					const uint8_t b = sc->mBlockLight.get(sbIdx);
					if (b > 1) {
						blkQ.push_back({ wx, y, wz, b });
					}
				}
			} else if (hasBlkLight) {
				const uint8_t b = sc->mBlockLight.get(sbIdx);
				if (b > 1) {
					blkQ.push_back({ wx, y, wz, b });
				}
			}
			if (!skyDone && hasSky) {
				if (sc->mSkyLight.get(sbIdx) > 1) {
					outMinSkyY = y;
				}
			}
		}
	}
}

void seedSeamFromSavedChunk(const ChunkGrid3x3& grid,
                            const ChunkPos& cp,
                            int maxH,
                            const LightLayer& skyLayer,
                            std::vector<LightNode>& skyQ,
                            std::vector<LightNode>& blkQ) {
	static constexpr int kCardinals[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
	LevelChunk* center = grid.atChunk(cp.x, cp.z);
	if (!center) return;
	const int baseX = cp.x * 16;
	const int baseZ = cp.z * 16;

	for (const auto& dir : kCardinals) {
		LevelChunk* nc = grid.atChunk(cp.x + dir[0], cp.z + dir[1]);
		if (!nc) continue;
		const bool neighborPreLit = (nc->getDebugDisplaySavedState() == ChunkDebugDisplaySavedState::Saved &&
		                             !nc->needsUpgradeFix() &&
		                             !nc->getSubChunks().empty());
		if (neighborPreLit) continue;

		const int allocTop = std::min(maxH, std::max(static_cast<int>(center->getAllocatedHeight()),
		                                             static_cast<int>(nc->getAllocatedHeight())));
		for (int t = 0; t < 16; ++t) {
			const int lx = (dir[0] == 1) ? 15 : (dir[0] == -1 ? 0 : t);
			const int lz = (dir[1] == 1) ? 15 : (dir[1] == -1 ? 0 : t);
			const int nlx = (lx + dir[0]) & 15;
			const int nlz = (lz + dir[1]) & 15;
			const int wx = baseX + lx;
			const int wz = baseZ + lz;
			for (int y = allocTop; y >= 0; --y) {
				const uint8_t s = getLightFast(center, skyLayer, lx, y, lz);
				const uint8_t ns = getLightFast(nc, skyLayer, nlx, y, nlz);
				if (s > ns + 1) {
					skyQ.push_back({ wx, y, wz, s });
				}
				const uint8_t b = getLightFast(center, LightLayer::BLOCK, lx, y, lz);
				const uint8_t nb = getLightFast(nc, LightLayer::BLOCK, nlx, y, nlz);
				if (b > nb + 1) {
					blkQ.push_back({ wx, y, wz, b });
				}
			}
		}
	}
}

} // namespace

void propagateChunkLighting(BlockSource& region,
                            const ChunkPos& cp,
                            std::unordered_set<ChunkPos>* outTouchedChunks,
                            TouchedSubChunkMap* outTouchedSubChunks) {
	auto tLight0 = std::chrono::high_resolution_clock::now();
	ChunkPerfMetrics::instance.lightPropagateCalls.fetch_add(1, std::memory_order_relaxed);
	LevelChunk* lc = region.getChunk(cp);
	if (!lc) return;

	const LightLayer& skyLayer = region.getDimension().getSkyLightLayer();
	const int baseX = cp.x * 16;
	const int baseZ = cp.z * 16;
	const int maxH = std::max(0, static_cast<int>(region.getMaxHeight()) - 1);
	const ChunkGrid3x3 grid(region, cp.x, cp.z);

	std::vector<LightNode> skyQ;
	std::vector<LightNode> blkQ;
	skyQ.reserve(1024);
	blkQ.reserve(256);

	const bool isSavedPreLit = (lc->getDebugDisplaySavedState() == ChunkDebugDisplaySavedState::Saved &&
	                            !lc->needsUpgradeFix() &&
	                            !lc->getSubChunks().empty());
	if (isSavedPreLit) {
		seedSeamFromSavedChunk(grid, cp, maxH, skyLayer, skyQ, blkQ);
		if (!skyQ.empty() || !blkQ.empty()) {
			const int minX = baseX - 15;
			const int maxX = baseX + 30;
			const int minZ = baseZ - 15;
			const int maxZ = baseZ + 30;
			if (!skyQ.empty()) {
				runLightBfs(grid, skyLayer, skyQ, minX, maxX, 0, maxH, minZ, maxZ, outTouchedChunks, outTouchedSubChunks);
			}
			if (!blkQ.empty()) {
				runLightBfs(grid, LightLayer::BLOCK, blkQ, minX, maxX, 0, maxH, minZ, maxZ, outTouchedChunks, outTouchedSubChunks);
			}
		}
		auto lightUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - tLight0).count();
		ChunkPerfMetrics::instance.lightUs.fetch_add(static_cast<uint64_t>(lightUs), std::memory_order_relaxed);
		return;
	}

	// 18x18 padded grid for heightmap top solid y and lowest sky>1 y: index [lx+1][lz+1]
	uint16_t topSolid[18][18]{};
	uint16_t minSkyY[18][18]{};
	for (int x = 0; x < 18; ++x) {
		for (int z = 0; z < 18; ++z) {
			minSkyY[x][z] = static_cast<uint16_t>(maxH + 1);
		}
	}

	// Pass 1: Vertical skylight, heightmap, and light emitter collection per subchunk (precompute subchunk masks once!)
	const auto& subs = lc->getSubChunks();
	const int maxSy = std::min(static_cast<int>(subs.size()) - 1, maxH >> 4);
	SubChunkScanFlags centerFlags;
	centerFlags.build(lc);

	for (int lx = 0; lx < 16; ++lx) {
		for (int lz = 0; lz < 16; ++lz) {
			int curSky = 15;
			int colTop = 0;
			int colLowSky = maxH + 1;
			if (maxSy < 0) {
				colLowSky = 0;
			}

			for (int sy = maxSy; sy >= 0; --sy) {
				const auto& sc = subs[sy];
				const int baseY = sy << 4;
				const int topLy = std::min(15, maxH - baseY);
				const uint16_t bit = static_cast<uint16_t>(1u << sy);
				const bool hasBlk = (centerFlags.hasBlocks & bit) != 0;

				if (!hasBlk) {
					if (curSky == 15 && (centerFlags.skyAllOne & bit) != 0) {
						colLowSky = baseY;
						continue;
					}
					if (curSky == 0 && (centerFlags.skyAllZero & bit) != 0) {
						continue;
					}
					for (int ly = topLy; ly >= 0; --ly) {
						const int y = baseY + ly;
						const uint8_t oldSky = sc->mSkyLight.get(subBlockIdx(lx, ly, lz));
						if (oldSky != static_cast<uint8_t>(curSky)) {
							lc->setBrightness(skyLayer, ChunkBlockPos(lx, y, lz), Brightness(static_cast<uint8_t>(curSky)));
							markTouched(outTouchedChunks, outTouchedSubChunks, baseX + lx, y, baseZ + lz);
						}
						if (curSky > 1) {
							colLowSky = y;
						}
					}
					continue;
				}

				for (int ly = topLy; ly >= 0; --ly) {
					const int y = baseY + ly;
					const ChunkBlockPos cbp(lx, y, lz);
					const size_t sbIdx = subBlockIdx(lx, ly, lz);
					const BlockID id = sc->mBlocks[sbIdx];
					const int trans = static_cast<int>(Block::mLightBlock[id]);
					if (trans > 0 && colTop == 0) {
						colTop = y;
					}
					curSky = (trans >= curSky) ? 0 : (curSky - trans);
					if (curSky > 1) {
						colLowSky = y;
					}

					const uint8_t oldSky = sc->mSkyLight.get(sbIdx);
					if (oldSky != static_cast<uint8_t>(curSky)) {
						lc->setBrightness(skyLayer, cbp, Brightness(static_cast<uint8_t>(curSky)));
						markTouched(outTouchedChunks, outTouchedSubChunks, baseX + lx, y, baseZ + lz);
					}

					const uint8_t emit = static_cast<uint8_t>(Block::mLightEmission[id]);
					if (emit > 0) {
						const uint8_t oldBlk = sc->mBlockLight.get(sbIdx);
						if (emit > oldBlk) {
							lc->setBrightness(LightLayer::BLOCK, cbp, Brightness(emit));
							markTouched(outTouchedChunks, outTouchedSubChunks, baseX + lx, y, baseZ + lz);
						}
						const uint8_t seed = std::max(emit, oldBlk);
						if (seed > 1) {
							blkQ.push_back({ baseX + lx, y, baseZ + lz, seed });
						}
					}
				}
			}

			topSolid[lx + 1][lz + 1] = static_cast<uint16_t>(colTop);
			minSkyY[lx + 1][lz + 1] = static_cast<uint16_t>(colLowSky);
		}
	}

	// Scan only the 1-block exterior perimeter columns using precomputed 3x3 subchunk flags
	SubChunkScanFlags gridFlags[3][3];
	for (int dx = 0; dx < 3; ++dx) {
		for (int dz = 0; dz < 3; ++dz) {
			if (dx == 1 && dz == 1) continue;
			gridFlags[dx][dz].build(grid.c[dx][dz]);
		}
	}
	for (int lx = -1; lx <= 16; ++lx) {
		for (int lz = -1; lz <= 16; ++lz) {
			if (lx >= 0 && lx < 16 && lz >= 0 && lz < 16) continue;
			const int wx = baseX + lx;
			const int wz = baseZ + lz;
			const int gx = (lx < 0) ? 0 : (lx >= 16 ? 2 : 1);
			const int gz = (lz < 0) ? 0 : (lz >= 16 ? 2 : 1);
			LevelChunk* nlc = grid.c[gx][gz];
			if (!nlc) continue;
			int nTop = 0;
			int nMinSky = maxH + 1;
			scanExteriorBorderColumn(nlc, gridFlags[gx][gz], wx & 15, wz & 15, wx, wz, maxH, nTop, nMinSky, blkQ);
			topSolid[lx + 1][lz + 1] = static_cast<uint16_t>(nTop);
			minSkyY[lx + 1][lz + 1] = static_cast<uint16_t>(nMinSky);
		}
	}

	// Pass 2 (0.8.1 lightGaps pattern): only seed sky BFS inside each column's height-gap / overhang band [minSkyY .. maxNeighborTop]
	for (int lx = 0; lx < 16; ++lx) {
		for (int lz = 0; lz < 16; ++lz) {
			const int gx = lx + 1;
			const int gz = lz + 1;
			const int wx = baseX + lx;
			const int wz = baseZ + lz;
			int seedTop = std::max({
				static_cast<int>(topSolid[gx][gz]),
				static_cast<int>(topSolid[gx + 1][gz]),
				static_cast<int>(topSolid[gx - 1][gz]),
				static_cast<int>(topSolid[gx][gz + 1]),
				static_cast<int>(topSolid[gx][gz - 1])
			});
			seedTop = std::min(maxH, seedTop);
			const int seedBot = static_cast<int>(minSkyY[gx][gz]);

			for (int y = seedBot; y <= seedTop; ++y) {
				const BlockID id = getBlockFast(lc, lx, y, lz);
				if (Block::mLightBlock[id] >= Brightness::MAX) continue;
				const uint8_t sky = getLightFast(lc, skyLayer, lx, y, lz);
				if (sky <= 1) continue;

				for (const auto& d : kDirs) {
					const int ax = wx + d[0];
					const int ay = y + d[1];
					const int az = wz + d[2];
					if (ay < 0 || ay > maxH) continue;
					LevelChunk* nlc = (ax >= baseX && ax < baseX + 16 && az >= baseZ && az < baseZ + 16)
						? lc
						: grid.atWorld(ax, az);
					if (!nlc) continue;
					const uint8_t ns = getLightFast(nlc, skyLayer, ax & 15, ay, az & 15);
					if (ns + 1 < sky) {
						skyQ.push_back({ wx, y, wz, sky });
						break;
					}
				}
			}
		}
	}

	// Seed horizontal sky light from exterior border strip into interior columns where neighbor is higher/open
	for (int lx = -1; lx <= 16; ++lx) {
		for (int lz = -1; lz <= 16; ++lz) {
			const bool isEdge = (lx == -1 || lx == 16 || lz == -1 || lz == 16);
			if (!isEdge) continue;
			if ((lx == -1 || lx == 16) && (lz == -1 || lz == 16)) continue; // diagonals don't share a cardinal face into [0..15]

			const int wx = baseX + lx;
			const int wz = baseZ + lz;
			LevelChunk* nlc = grid.atWorld(wx, wz);
			if (!nlc) continue;

			const int inLx = Math::clamp(lx, 0, 15);
			const int inLz = Math::clamp(lz, 0, 15);
			const int seedTop = std::min(maxH, std::max(static_cast<int>(topSolid[lx + 1][lz + 1]),
			                                            static_cast<int>(topSolid[inLx + 1][inLz + 1])));
			const int seedBot = static_cast<int>(minSkyY[lx + 1][lz + 1]);
			for (int y = seedBot; y <= seedTop; ++y) {
				const uint8_t sky = getLightFast(nlc, skyLayer, wx & 15, y, wz & 15);
				if (sky <= 1) continue;
				const uint8_t innerSky = getLightFast(lc, skyLayer, inLx, y, inLz);
				if (innerSky + 1 < sky) {
					skyQ.push_back({ wx, y, wz, sky });
				}
			}
		}
	}

	const int minX = baseX - 15;
	const int maxX = baseX + 30;
	const int minZ = baseZ - 15;
	const int maxZ = baseZ + 30;
	if (!skyQ.empty()) {
		runLightBfs(grid, skyLayer, skyQ, minX, maxX, 0, maxH, minZ, maxZ, outTouchedChunks, outTouchedSubChunks);
	}
	if (!blkQ.empty()) {
		runLightBfs(grid, LightLayer::BLOCK, blkQ, minX, maxX, 0, maxH, minZ, maxZ, outTouchedChunks, outTouchedSubChunks);
	}

	auto lightUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - tLight0).count();
	ChunkPerfMetrics::instance.lightUs.fetch_add(static_cast<uint64_t>(lightUs), std::memory_order_relaxed);
}

void updateLocalLighting(BlockSource& region,
                         const BlockPos& center,
                         int blockRadius,
                         std::unordered_set<ChunkPos>* outTouchedChunks,
                         TouchedSubChunkMap* outTouchedSubChunks) {
	const LightLayer& skyLayer = region.getDimension().getSkyLightLayer();
	const int r = std::max(4, std::min(15, blockRadius));
	const int minX = center.x - r;
	const int maxX = center.x + r;
	const int minZ = center.z - r;
	const int maxZ = center.z + r;
	const int minY = std::max(0, center.y - r);
	const int maxY = std::min(127, center.y + r);
	const int boxW = maxX - minX + 1;
	const int boxD = maxZ - minZ + 1;

	const int centerCx = floorDiv16(center.x);
	const int centerCz = floorDiv16(center.z);
	const ChunkGrid3x3 grid(region, centerCx, centerCz);
	SubChunkScanFlags gridFlags[3][3];
	uint16_t minToShellSubMask = 0;
	for (int sy = std::max(0, (minY - 1) >> 4); sy <= std::min(7, (maxY + 1) >> 4); ++sy) {
		minToShellSubMask |= static_cast<uint16_t>(1u << sy);
	}
	bool hasAnyShellBlkLight = false;
	for (int dx = 0; dx < 3; ++dx) {
		for (int dz = 0; dz < 3; ++dz) {
			gridFlags[dx][dz].build(grid.c[dx][dz]);
			if ((gridFlags[dx][dz].blkHasLight & minToShellSubMask) != 0) {
				hasAnyShellBlkLight = true;
			}
		}
	}

	std::vector<LightNode> skyQ;
	std::vector<LightNode> blkQ;
	skyQ.reserve(256);
	blkQ.reserve(128);

	uint16_t colTop[32][32]{};
	uint16_t colMinSky[32][32]{};
	int globalMaxTop = 0;

	// Pass 1: Recompute vertical skylight inside [minX..maxX]x[minY..maxY]x[minZ..maxZ] and track shadow-band bounds
	for (int x = minX; x <= maxX; ++x) {
		const int bx = x - minX;
		const int gx = floorDiv16(x) - centerCx + 1;
		const int lx = x & 15;
		for (int z = minZ; z <= maxZ; ++z) {
			const int bz = z - minZ;
			const int gz = floorDiv16(z) - centerCz + 1;
			const int lz = z & 15;
			LevelChunk* lc = (static_cast<unsigned>(gx) < 3u && static_cast<unsigned>(gz) < 3u) ? grid.c[gx][gz] : nullptr;
			if (!lc) {
				colMinSky[bx][bz] = static_cast<uint16_t>(maxY + 1);
				continue;
			}

			const auto& subs = lc->getSubChunks();
			const SubChunkScanFlags& flags = gridFlags[gx][gz];
			const int allocTop = std::min(127, static_cast<int>(subs.size()) * 16 - 1);
			const int stopY = std::max(0, minY - 1);
			const bool colHasBlkLight = (flags.blkHasLight & minToShellSubMask) != 0;

			int sky = 15;
			int topSolidY = 0;
			int lowSkyY = (allocTop < maxY) ? std::max(minY, allocTop + 1) : (maxY + 1);

			if (allocTop >= stopY) {
				const int topSy = allocTop >> 4;
				const int botSy = stopY >> 4;
				for (int sy = topSy; sy >= botSy; --sy) {
					const auto& sc = subs[sy];
					const int baseY = sy << 4;
					const int startLy = std::min(15, allocTop - baseY);
					const int endLy = std::max(0, stopY - baseY);
					const uint16_t bit = static_cast<uint16_t>(1u << sy);
					const bool hasBlk = (flags.hasBlocks & bit) != 0;

					if (!hasBlk && sky == 15 && (flags.skyAllOne & bit) != 0) {
						if (baseY + endLy <= maxY) {
							lowSkyY = std::max(minY, baseY + endLy);
						}
						if (colHasBlkLight && (flags.blkHasLight & bit) != 0 && baseY + startLy >= minY && baseY + endLy <= maxY) {
							const int clearTop = std::min(maxY - baseY, startLy);
							const int clearBot = std::max(minY - baseY, endLy);
							for (int ly = clearTop; ly >= clearBot; --ly) {
								const int y = baseY + ly;
								if (sc->mBlockLight.get(subBlockIdx(lx, ly, lz)) != 0) {
									lc->setBrightness(LightLayer::BLOCK, ChunkBlockPos(lx, y, lz), Brightness::MIN);
									markTouched(outTouchedChunks, outTouchedSubChunks, x, y, z);
								}
							}
						}
						continue;
					}

					for (int ly = startLy; ly >= endLy; --ly) {
						const int y = baseY + ly;
						const size_t sbIdx = subBlockIdx(lx, ly, lz);
						const BlockID id = hasBlk ? sc->mBlocks[sbIdx] : BlockID::AIR;
						const int lb = static_cast<int>(Block::mLightBlock[id]);
						if (lb > 0 && topSolidY == 0) {
							topSolidY = y;
						}

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
							const ChunkBlockPos cbp(lx, y, lz);
							const uint8_t oldSky = sc->mSkyLight.get(sbIdx);
							if (oldSky != static_cast<uint8_t>(targetSky)) {
								lc->setBrightness(skyLayer, cbp, Brightness(static_cast<uint8_t>(targetSky)));
								markTouched(outTouchedChunks, outTouchedSubChunks, x, y, z);
							}
							if (targetSky > 1) {
								lowSkyY = y;
							}

							const uint8_t emit = hasBlk ? static_cast<uint8_t>(Block::mLightEmission[id]) : 0;
							if (emit > 0 || colHasBlkLight) {
								const uint8_t oldBlk = sc->mBlockLight.get(sbIdx);
								if (oldBlk != emit) {
									lc->setBrightness(LightLayer::BLOCK, cbp, Brightness(emit));
									markTouched(outTouchedChunks, outTouchedSubChunks, x, y, z);
								}
								if (emit > 1) {
									blkQ.push_back({ x, y, z, emit });
									hasAnyShellBlkLight = true;
								}
							}
						} else if ((y == maxY + 1 || y == minY - 1) && targetSky > 1) {
							skyQ.push_back({ x, y, z, static_cast<uint8_t>(targetSky) });
						}
					}
				}
			}

			colTop[bx][bz] = static_cast<uint16_t>(topSolidY);
			colMinSky[bx][bz] = static_cast<uint16_t>(lowSkyY);
			if (topSolidY > globalMaxTop) {
				globalMaxTop = topSolidY;
			}
		}
	}

	// Pass 1b: Seed sky BFS ONLY where a block inside [minX..maxX]x[minY..maxY]x[minZ..maxZ] faces a darker neighbor
	for (int bx = 0; bx < boxW; ++bx) {
		const int x = minX + bx;
		for (int bz = 0; bz < boxD; ++bz) {
			const int z = minZ + bz;
			int nMax = static_cast<int>(colTop[bx][bz]);
			if (bx > 0) nMax = std::max(nMax, static_cast<int>(colTop[bx - 1][bz]));
			if (bx + 1 < boxW) nMax = std::max(nMax, static_cast<int>(colTop[bx + 1][bz]));
			if (bz > 0) nMax = std::max(nMax, static_cast<int>(colTop[bx][bz - 1]));
			if (bz + 1 < boxD) nMax = std::max(nMax, static_cast<int>(colTop[bx][bz + 1]));
			const int seedTop = std::min(maxY, nMax);
			const int seedBot = static_cast<int>(colMinSky[bx][bz]);
			if (seedBot > seedTop) continue;

			LevelChunk* lc = grid.atWorld(x, z);
			if (!lc) continue;
			const int lx = x & 15;
			const int lz = z & 15;
			for (int y = seedBot; y <= seedTop; ++y) {
				const uint8_t sky = getLightFast(lc, skyLayer, lx, y, lz);
				if (sky <= 1) continue;
				for (const auto& d : kDirs) {
					const int ax = x + d[0];
					const int ay = y + d[1];
					const int az = z + d[2];
					if (ax < minX || ax > maxX || ay < minY || ay > maxY || az < minZ || az > maxZ) continue;
					LevelChunk* nlc = grid.atWorld(ax, az);
					if (!nlc) continue;
					if (getLightFast(nlc, skyLayer, ax & 15, ay, az & 15) + 1 < sky) {
						skyQ.push_back({ x, y, z, sky });
						break;
					}
				}
			}
		}
	}

	// Pass 2: Seed from 1-block outer shell around the cleared box ONLY where inward neighbor is darker
	const int shellMinX = minX - 1;
	const int shellMaxX = maxX + 1;
	const int shellMinZ = minZ - 1;
	const int shellMaxZ = maxZ + 1;
	const int shellMinY = std::max(0, minY - 1);
	const int shellMaxY = std::min(127, maxY + 1);

	for (int x = shellMinX; x <= shellMaxX; ++x) {
		for (int z = shellMinZ; z <= shellMaxZ; ++z) {
			const bool isXZBorder = (x == shellMinX || x == shellMaxX || z == shellMinZ || z == shellMaxZ);
			LevelChunk* lc = grid.atWorld(x, z);
			if (!lc) continue;
			const int lx = x & 15;
			const int lz = z & 15;
			const int inX = Math::clamp(x, minX, maxX);
			const int inZ = Math::clamp(z, minZ, maxZ);
			LevelChunk* inLc = isXZBorder ? grid.atWorld(inX, inZ) : lc;
			const int skyShellTop = std::min(shellMaxY, std::max(globalMaxTop + 1, minY));

			for (int y = shellMinY; y <= shellMaxY; ++y) {
				const bool isYBorder = (y == shellMinY || y == shellMaxY);
				if (!isXZBorder && !isYBorder) continue;

				if (y <= skyShellTop) {
					const uint8_t sky = getLightFast(lc, skyLayer, lx, y, lz);
					if (sky > 1) {
						const int inY = Math::clamp(y, minY, maxY);
						if (!inLc || getLightFast(inLc, skyLayer, inX & 15, inY, inZ & 15) + 1 < sky) {
							skyQ.push_back({ x, y, z, sky });
						}
					}
				}
				if (hasAnyShellBlkLight) {
					const uint8_t blk = getLightFast(lc, LightLayer::BLOCK, lx, y, lz);
					const BlockID id = getBlockFast(lc, lx, y, lz);
					const uint8_t emit = static_cast<uint8_t>(Block::mLightEmission[id]);
					const uint8_t seedBlk = std::max(blk, emit);
					if (seedBlk > 1) {
						blkQ.push_back({ x, y, z, seedBlk });
					}
				}
			}
		}
	}

	if (!skyQ.empty()) {
		runLightBfs(grid, skyLayer, skyQ, minX, maxX, minY, maxY, minZ, maxZ, outTouchedChunks, outTouchedSubChunks);
	}
	if (!blkQ.empty()) {
		runLightBfs(grid, LightLayer::BLOCK, blkQ, minX, maxX, minY, maxY, minZ, maxZ, outTouchedChunks, outTouchedSubChunks);
	}
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
