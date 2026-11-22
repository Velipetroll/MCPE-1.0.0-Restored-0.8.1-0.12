#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <glad/glad.h>
#include "common/client/gui/GuiComponent.h"
#include "world/level/BlockSource.h"
#include "world/level/block/Block.h"
#include "world/level/material/Material.h"
#include "world/phys/AABB.h"
#include "world/phys/Vec3.h"
#include "world/Direction.h"
#include "world/Facing.h"
#include "nbt/CompoundTag.h"

namespace PaintingSystem {

struct MotiveDef {
	const char* name;
	int w, h;
	int uo, vo;
	bool isPublic;
};

inline const std::vector<MotiveDef>& getMotives() {
	static const std::vector<MotiveDef> motives = {
		{ "Kebab",            16, 16,   0,   0, true },
		{ "Aztec",            16, 16,  16,   0, true },
		{ "Alban",            16, 16,  32,   0, true },
		{ "Aztec2",           16, 16,  48,   0, true },
		{ "Bomb",             16, 16,  64,   0, true },
		{ "Plant",            16, 16,  80,   0, true },
		{ "Wasteland",        16, 16,  96,   0, true },
		{ "Pool",             32, 16,   0,  32, true },
		{ "Courbet",          32, 16,  32,  32, true },
		{ "Sea",              32, 16,  64,  32, true },
		{ "Sunset",           32, 16,  96,  32, true },
		{ "Creebet",          32, 16, 128,  32, true },
		{ "Wanderer",         16, 32,   0,  64, true },
		{ "Graham",           16, 32,  16,  64, true },
		{ "Match",            32, 32,   0, 128, true },
		{ "Bust",             32, 32,  32, 128, true },
		{ "Stage",            32, 32,  64, 128, true },
		{ "Void",             32, 32,  96, 128, true },
		{ "SkullAndRoses",    32, 32, 128, 128, true },
		{ "Fighters",         64, 32,   0,  96, true },
		{ "Pointer",          64, 64,   0, 192, true },
		{ "Pigscene",         64, 64,  64, 192, true },
		{ "BurningSkull",     64, 64, 128, 192, true },
		{ "Skeleton",         64, 48, 192,  64, true },
		{ "DonkeyKong",       64, 48, 192, 112, true },
		{ "Earth",            32, 32,   0, 160, false },
		{ "Wind",             32, 32,  32, 160, false },
		{ "Fire",             32, 32,  64, 160, false },
		{ "Water",            32, 32,  96, 160, false },
	};
	return motives;
}

inline int findMotiveByName(const std::string& name) {
	const auto& m = getMotives();
	for (size_t i = 0; i < m.size(); ++i) {
		if (name == m[i].name) return static_cast<int>(i);
	}
	return 0;
}

struct PaintingEntity {
	int tileX = 0, tileY = 0, tileZ = 0;
	int dir = 0; // 0=S, 1=W, 2=N, 3=E
	int motiveIdx = 0;
	float x = 0.0f, y = 0.0f, z = 0.0f;
	AABB bb;
	int tickCounter = 0;

	static float offs(int size) {
		return (size == 32 || size == 64) ? 0.5f : 0.0f;
	}

	void setDir(int d) {
		dir = d & 3;
		const MotiveDef& m = getMotives()[static_cast<size_t>(motiveIdx)];
		float px = static_cast<float>(tileX) + 0.5f;
		float py = static_cast<float>(tileY) + 0.5f + offs(m.h);
		float pz = static_cast<float>(tileZ) + 0.5f;

		if (dir == 2) { pz -= 0.5625f; px -= offs(m.w); }
		else if (dir == 1) { px -= 0.5625f; pz += offs(m.w); }
		else if (dir == 0) { pz += 0.5625f; px += offs(m.w); }
		else if (dir == 3) { px += 0.5625f; pz -= offs(m.w); }

		x = px;
		y = py;
		z = pz;

		float hx = (dir == 0 || dir == 2) ? (m.w * 0.03125f) : 0.0625f;
		float hy = m.h * 0.03125f;
		float hz = (dir == 0 || dir == 2) ? 0.0625f : (m.w * 0.03125f);
		bb.set(px - hx + 0.03125f, py - hy + 0.03125f, pz - hz + 0.03125f,
		       px + hx - 0.03125f, py + hy - 0.03125f, pz + hz - 0.03125f);
	}

	bool survives(BlockSource& region, const std::vector<PaintingEntity>& all, int selfIdx) const {
		// Check no solid block intersects painting AABB
		int x0 = static_cast<int>(std::floor(bb.min.x));
		int x1 = static_cast<int>(std::floor(bb.max.x));
		int y0 = static_cast<int>(std::floor(bb.min.y));
		int y1 = static_cast<int>(std::floor(bb.max.y));
		int z0 = static_cast<int>(std::floor(bb.min.z));
		int z1 = static_cast<int>(std::floor(bb.max.z));
		for (int bx = x0; bx <= x1; ++bx) {
			for (int by = y0; by <= y1; ++by) {
				for (int bz = z0; bz <= z1; ++bz) {
					BlockPos bp(bx, by, bz);
					const Block& blk = region.getBlock(bp);
					if (blk.mID != BlockID::AIR && blk.getMaterial().isSolid()) {
						AABB tmp;
						if (!blk.getAABB(region, bp, tmp).isEmpty() && tmp.intersects(bb)) {
							return false;
						}
					}
				}
			}
		}
		// Check solid support behind all tiles of the painting
		const MotiveDef& m = getMotives()[static_cast<size_t>(motiveIdx)];
		int tw = std::max(1, m.w / 16);
		int th = std::max(1, m.h / 16);
		int sx = tileX;
		int sy = static_cast<int>(std::floor(bb.min.y));
		int sz = tileZ;
		for (int u = 0; u < tw; ++u) {
			for (int v = 0; v < th; ++v) {
				int cx = sx, cz = sz;
				if (dir == 2 || dir == 0) {
					cx = static_cast<int>(std::floor(bb.min.x)) + u;
				} else {
					cz = static_cast<int>(std::floor(bb.min.z)) + u;
				}
				// Block behind is at tileX/tileZ
				BlockPos supportPos((dir == 1 || dir == 3) ? tileX : cx, sy + v, (dir == 0 || dir == 2) ? tileZ : cz);
				if (!region.getMaterial(supportPos).isSolid()) {
					return false;
				}
			}
		}
		// Check no overlapping painting
		for (size_t i = 0; i < all.size(); ++i) {
			if (static_cast<int>(i) == selfIdx) continue;
			if (all[i].bb.intersects(bb)) return false;
		}
		return true;
	}
};

inline std::vector<PaintingEntity>& getPaintings() {
	static std::vector<PaintingEntity> paintings;
	return paintings;
}

inline void clear() {
	getPaintings().clear();
}

inline bool tryPlacePainting(BlockSource& region, const BlockPos& hitPos, FacingID face) {
	if (face == Facing::DOWN || face == Facing::UP || face >= Facing::MAX) {
		return false;
	}
	// Direction::FACING_DIRECTION: NORTH(2)->2, SOUTH(3)->0, WEST(4)->1, EAST(5)->3
	int dir = 0;
	switch (face) {
	case Facing::NORTH: dir = 2; break;
	case Facing::SOUTH: dir = 0; break;
	case Facing::WEST:  dir = 1; break;
	case Facing::EAST:  dir = 3; break;
	default: return false;
	}

	auto& paintings = getPaintings();
	const auto& motives = getMotives();
	std::vector<int> valid;
	PaintingEntity cand;
	cand.tileX = hitPos.x;
	cand.tileY = hitPos.y;
	cand.tileZ = hitPos.z;
	for (size_t i = 0; i < motives.size(); ++i) {
		if (!motives[i].isPublic) continue;
		cand.motiveIdx = static_cast<int>(i);
		cand.setDir(dir);
		if (cand.survives(region, paintings, -1)) {
			valid.push_back(static_cast<int>(i));
		}
	}
	if (valid.empty()) {
		cand.motiveIdx = 0;
		cand.setDir(dir);
		if (!cand.survives(region, paintings, -1)) {
			return false;
		}
	} else {
		cand.motiveIdx = valid[static_cast<size_t>(std::rand() % valid.size())];
		cand.setDir(dir);
	}
	paintings.push_back(cand);
	return true;
}

inline int raycastPainting(const Vec3& origin, const Vec3& dir, float maxDist) {
	const auto& paintings = getPaintings();
	float bestT = maxDist;
	int bestIdx = -1;
	for (size_t i = 0; i < paintings.size(); ++i) {
		const AABB& b = paintings[i].bb;
		float tmin = 0.0f, tmax = bestT;
		for (int a = 0; a < 3; ++a) {
			float o = (a == 0) ? origin.x : (a == 1 ? origin.y : origin.z);
			float d = (a == 0) ? dir.x : (a == 1 ? dir.y : dir.z);
			float bmin = (a == 0) ? b.min.x : (a == 1 ? b.min.y : b.min.z);
			float bmax = (a == 0) ? b.max.x : (a == 1 ? b.max.y : b.max.z);
			if (std::abs(d) < 1e-6f) {
				if (o < bmin || o > bmax) { tmin = 1e9f; break; }
			} else {
				float inv = 1.0f / d;
				float t1 = (bmin - o) * inv;
				float t2 = (bmax - o) * inv;
				if (t1 > t2) std::swap(t1, t2);
				tmin = std::max(tmin, t1);
				tmax = std::min(tmax, t2);
				if (tmin > tmax) { tmin = 1e9f; break; }
			}
		}
		if (tmin < bestT && tmin >= 0.0f) {
			bestT = tmin;
			bestIdx = static_cast<int>(i);
		}
	}
	return bestIdx;
}

inline void renderPaintings(BlockSource& region) {
	const auto& paintings = getPaintings();
	if (paintings.empty()) return;
	GuiTextureInfo tex = GuiComponent::loadTexture("textures/painting/kz.png");
	if (tex.id == 0) return;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex.id);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.1f);
	glDisable(GL_CULL_FACE);

	const auto& motives = getMotives();
	for (const auto& p : paintings) {
		const MotiveDef& m = motives[static_cast<size_t>(p.motiveIdx)];
		float light = region.getBrightness(BlockPos(static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y)), static_cast<int>(std::floor(p.z))));
		light = std::max(0.15f, light);

		glPushMatrix();
		glTranslatef(p.x, p.y, p.z);
		float yaw = 0.0f;
		if (p.dir == 0) yaw = 180.0f;
		else if (p.dir == 1) yaw = 90.0f;
		else if (p.dir == 2) yaw = 0.0f;
		else if (p.dir == 3) yaw = 270.0f;
		glRotatef(yaw, 0.0f, 1.0f, 0.0f);
		glScalef(0.0625f, 0.0625f, 0.0625f);

		float x0 = -static_cast<float>(m.w) * 0.5f;
		float y0 = -static_cast<float>(m.h) * 0.5f;
		float z0 = -0.5f, z1 = 0.5f;

		int tilesX = m.w / 16;
		int tilesY = m.h / 16;
		glBegin(GL_QUADS);
		for (int tx = 0; tx < tilesX; ++tx) {
			for (int ty = 0; ty < tilesY; ++ty) {
				float px0 = x0 + tx * 16.0f;
				float px1 = px0 + 16.0f;
				float py0 = y0 + ty * 16.0f;
				float py1 = py0 + 16.0f;

				float u0 = static_cast<float>(m.uo + (tilesX - 1 - tx) * 16) / 256.0f;
				float u1 = static_cast<float>(m.uo + (tilesX - tx) * 16) / 256.0f;
				float v0 = static_cast<float>(m.vo + (tilesY - 1 - ty) * 16) / 256.0f;
				float v1 = static_cast<float>(m.vo + (tilesY - ty) * 16) / 256.0f;

				// Front (-Z)
				glColor4f(light, light, light, 1.0f);
				glTexCoord2f(u1, v1); glVertex3f(px0, py0, z0);
				glTexCoord2f(u0, v1); glVertex3f(px1, py0, z0);
				glTexCoord2f(u0, v0); glVertex3f(px1, py1, z0);
				glTexCoord2f(u1, v0); glVertex3f(px0, py1, z0);

				// Back (+Z) wood
				glColor4f(light * 0.8f, light * 0.8f, light * 0.8f, 1.0f);
				glTexCoord2f(0.75f, 0.0625f); glVertex3f(px0, py0, z1);
				glTexCoord2f(0.8125f, 0.0625f); glVertex3f(px1, py0, z1);
				glTexCoord2f(0.8125f, 0.0f); glVertex3f(px1, py1, z1);
				glTexCoord2f(0.75f, 0.0f); glVertex3f(px0, py1, z1);

				// 4 border edges (0.5px wood)
				glTexCoord2f(0.75f, 0.001953125f); glVertex3f(px0, py1, z0);
				glTexCoord2f(0.8125f, 0.001953125f); glVertex3f(px1, py1, z0);
				glTexCoord2f(0.8125f, 0.0f); glVertex3f(px1, py1, z1);
				glTexCoord2f(0.75f, 0.0f); glVertex3f(px0, py1, z1);

				glTexCoord2f(0.75f, 0.001953125f); glVertex3f(px0, py0, z1);
				glTexCoord2f(0.8125f, 0.001953125f); glVertex3f(px1, py0, z1);
				glTexCoord2f(0.8125f, 0.0f); glVertex3f(px1, py0, z0);
				glTexCoord2f(0.75f, 0.0f); glVertex3f(px0, py0, z0);

				glTexCoord2f(0.75f, 0.0625f); glVertex3f(px0, py0, z1);
				glTexCoord2f(0.751953125f, 0.0625f); glVertex3f(px0, py0, z0);
				glTexCoord2f(0.751953125f, 0.0f); glVertex3f(px0, py1, z0);
				glTexCoord2f(0.75f, 0.0f); glVertex3f(px0, py1, z1);

				glTexCoord2f(0.75f, 0.0625f); glVertex3f(px1, py0, z0);
				glTexCoord2f(0.751953125f, 0.0625f); glVertex3f(px1, py0, z1);
				glTexCoord2f(0.751953125f, 0.0f); glVertex3f(px1, py1, z1);
				glTexCoord2f(0.75f, 0.0f); glVertex3f(px1, py1, z0);
			}
		}
		glEnd();
		glPopMatrix();
	}
	glEnable(GL_CULL_FACE);
}

inline Unique<CompoundTag> savePaintingTag(const PaintingEntity& p) {
	auto tag = make_unique<CompoundTag>();
	tag->putInt("id", 83);
	tag->putString("Motive", getMotives()[static_cast<size_t>(p.motiveIdx)].name);
	tag->putByte("Direction", static_cast<char>(p.dir));
	tag->putInt("TileX", p.tileX);
	tag->putInt("TileY", p.tileY);
	tag->putInt("TileZ", p.tileZ);
	return tag;
}

inline bool loadPaintingTag(const CompoundTag& tag) {
	if (tag.getInt("id") != 83) return false;
	PaintingEntity p;
	p.motiveIdx = findMotiveByName(tag.getString("Motive"));
	p.tileX = tag.getInt("TileX");
	p.tileY = tag.getInt("TileY");
	p.tileZ = tag.getInt("TileZ");
	p.setDir(tag.getByte("Direction") & 3);
	getPaintings().push_back(p);
	return true;
}

} // namespace PaintingSystem
