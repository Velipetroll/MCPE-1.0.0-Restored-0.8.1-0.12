#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "common/client/gui/GuiComponent.h"
#include "client/game/MobSystem.h"
#include "client/game/WorldLighting.h"
#include "world/level/BlockSource.h"
#include "world/level/block/Block.h"
#include "world/level/block/BaseRailBlock.h"
#include "world/phys/AABB.h"
#include "world/phys/Vec3.h"
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "nbt/CompoundTag.h"
#include "nbt/ListTag.h"
#include "util/Math.h"
#include "ParticleSystem.h"

namespace MinecartSystem {

struct MinecartQuad {
	Vec3 v[4];
	float u[4]{};
	float tv[4]{};
	Vec3 normal;

	void init(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3,
	          int a, int b, int c, int d, float texW, float texH) {
		v[0] = v0; v[1] = v1; v[2] = v2; v[3] = v3;
		float u0 = static_cast<float>(a) / texW;
		float vTop = static_cast<float>(b) / texH;
		float u1 = static_cast<float>(c) / texW;
		float vBot = static_cast<float>(d) / texH;
		u[0] = u1; tv[0] = vTop;
		u[1] = u0; tv[1] = vTop;
		u[2] = u0; tv[2] = vBot;
		u[3] = u1; tv[3] = vBot;

		Vec3 e1 = v[0] - v[1];
		Vec3 e2 = v[0] - v[3];
		Vec3 n(e1.y * e2.z - e1.z * e2.y,
		       e1.z * e2.x - e1.x * e2.z,
		       e1.x * e2.y - e1.y * e2.x);
		float len = n.length();
		normal = (len > 1e-5f) ? (n / len) : Vec3(0.0f, -1.0f, 0.0f);
	}
};

struct MinecartCube {
	MinecartQuad quads[6];

	MinecartCube(int u, int v, float x, float y, float z, int w, int h, int d,
	             float inflate, float texW, float texH) {
		float x1 = x + static_cast<float>(w);
		float y1 = y + static_cast<float>(h);
		float z1 = z + static_cast<float>(d);

		float v20 = x - inflate;
		float v21 = y - inflate;
		float v22 = z - inflate;
		float v23 = x1 + inflate;
		float v24 = y1 + inflate;
		float v25 = z1 + inflate;
		std::swap(v20, v23);

		Vec3 p[8] = {
			Vec3(v23, v21, v22),
			Vec3(v20, v21, v22),
			Vec3(v20, v24, v22),
			Vec3(v23, v24, v22),
			Vec3(v23, v21, v25),
			Vec3(v20, v21, v25),
			Vec3(v20, v24, v25),
			Vec3(v23, v24, v25),
		};

		int uMid = u + d + w;
		int vMid = v + d;
		quads[0].init(p[5], p[1], p[2], p[6], uMid, vMid, uMid + d, vMid + h, texW, texH);
		quads[1].init(p[0], p[4], p[7], p[3], u, vMid, u + d, vMid + h, texW, texH);
		quads[2].init(p[5], p[4], p[0], p[1], u + d, v, uMid, v + d, texW, texH);
		quads[3].init(p[2], p[3], p[7], p[6], uMid, v + d, uMid + w, v, texW, texH);
		quads[4].init(p[1], p[0], p[3], p[2], u + d, vMid, uMid, vMid + h, texW, texH);
		quads[5].init(p[4], p[5], p[6], p[7], uMid + d, vMid, uMid + d + w, vMid + h, texW, texH);
	}
};

struct MinecartModelPart {
	float xOffset = 0.0f;
	float yOffset = 0.0f;
	float zOffset = 0.0f;
	float xRot = 0.0f;
	float yRot = 0.0f;
	float zRot = 0.0f;
	int texOffX = 0;
	int texOffY = 0;
	float texW = 64.0f;
	float texH = 32.0f;
	std::vector<MinecartCube> cubes;

	MinecartModelPart() = default;
	MinecartModelPart(int u, int v, int tw = 64, int th = 32)
		: texOffX(u), texOffY(v), texW(static_cast<float>(tw)), texH(static_cast<float>(th)) {}

	void setPos(float x, float y, float z) {
		xOffset = x;
		yOffset = y;
		zOffset = z;
	}

	void addBox(float x, float y, float z, int w, int h, int d, float inflate = 0.0f) {
		cubes.emplace_back(texOffX, texOffY, x, y, z, w, h, d, inflate, texW, texH);
	}

	void render(float scale, float light) const {
		static constexpr float kRadToDeg = 57.2957795f;
		glPushMatrix();
		if (xOffset != 0.0f || yOffset != 0.0f || zOffset != 0.0f) {
			glTranslatef(scale * xOffset, scale * yOffset, scale * zOffset);
		}
		if (zRot != 0.0f) glRotatef(zRot * kRadToDeg, 0.0f, 0.0f, 1.0f);
		if (yRot != 0.0f) glRotatef(yRot * kRadToDeg, 0.0f, 1.0f, 0.0f);
		if (xRot != 0.0f) glRotatef(xRot * kRadToDeg, 1.0f, 0.0f, 0.0f);

		glm::mat3 rot(1.0f);
		if (zRot != 0.0f) {
			float c = std::cos(zRot), s = std::sin(zRot);
			rot = rot * glm::mat3(c, s, 0.0f, -s, c, 0.0f, 0.0f, 0.0f, 1.0f);
		}
		if (yRot != 0.0f) {
			float c = std::cos(yRot), s = std::sin(yRot);
			rot = rot * glm::mat3(c, 0.0f, -s, 0.0f, 1.0f, 0.0f, s, 0.0f, c);
		}
		if (xRot != 0.0f) {
			float c = std::cos(xRot), s = std::sin(xRot);
			rot = rot * glm::mat3(1.0f, 0.0f, 0.0f, 0.0f, c, s, 0.0f, -s, c);
		}

		glBegin(GL_QUADS);
		for (const auto& cube : cubes) {
			for (int qi = 0; qi < 6; ++qi) {
				const MinecartQuad& q = cube.quads[qi];
				glm::vec3 wn = rot * glm::vec3(q.normal.x, q.normal.y, q.normal.z);
				float up = -wn.y;
				float faceShade = 0.88f;
				if (up > 0.55f) faceShade = 1.0f;
				else if (up < -0.55f) faceShade = 0.68f;
				else if (std::abs(wn.z) > 0.55f) faceShade = 0.94f;
				else faceShade = 0.82f;
				float c = light * faceShade;
				glColor4f(c, c, c, 1.0f);
				for (int vi = 0; vi < 4; ++vi) {
					glTexCoord2f(q.u[vi], q.tv[vi]);
					glVertex3f(q.v[vi].x * scale, q.v[vi].y * scale, q.v[vi].z * scale);
				}
			}
		}
		glEnd();
		glPopMatrix();
	}
};

struct MinecartModel {
	MinecartModelPart sides[6];

	MinecartModel() {
		static constexpr float kHalfPi = 3.14159265f * 0.5f;
		sides[0] = MinecartModelPart(0, 10, 64, 32);
		sides[1] = MinecartModelPart(0, 0, 64, 32);
		sides[2] = MinecartModelPart(0, 0, 64, 32);
		sides[3] = MinecartModelPart(0, 0, 64, 32);
		sides[4] = MinecartModelPart(0, 0, 64, 32);
		sides[5] = MinecartModelPart(44, 10, 64, 32);

		sides[0].addBox(-10.0f, -8.0f, -1.0f, 20, 16, 2, 0.0f);
		sides[0].setPos(0.0f, 4.0f, 0.0f);

		sides[5].addBox(-9.0f, -7.0f, -1.0f, 18, 14, 1, 0.0f);
		sides[5].setPos(0.0f, 4.0f, 0.0f);

		sides[1].addBox(-8.0f, -9.0f, -1.0f, 16, 8, 2, 0.0f);
		sides[1].setPos(-9.0f, 4.0f, 0.0f);

		sides[2].addBox(-8.0f, -9.0f, -1.0f, 16, 8, 2, 0.0f);
		sides[2].setPos(9.0f, 4.0f, 0.0f);

		sides[3].addBox(-8.0f, -9.0f, -1.0f, 16, 8, 2, 0.0f);
		sides[3].setPos(0.0f, 4.0f, -7.0f);

		sides[4].addBox(-8.0f, -9.0f, -1.0f, 16, 8, 2, 0.0f);
		sides[4].setPos(0.0f, 4.0f, 7.0f);

		sides[0].xRot = kHalfPi;
		sides[2].yRot = kHalfPi;
		sides[1].yRot = kHalfPi * 3.0f;
		sides[5].xRot = -kHalfPi;
		sides[3].yRot = kHalfPi + kHalfPi;
	}

	void render(float yOffParam, float scale, float light) {
		sides[5].yOffset = 4.0f - yOffParam;
		for (int i = 0; i < 6; ++i) {
			sides[i].render(scale, light);
		}
	}
};

struct MinecartEntity {
	static constexpr float kWidth = 0.98f;
	static constexpr float kHeight = 0.7f;
	static constexpr float kRidingHeight = 0.35f; // entityHeight * 0.5f

	static inline const int EXITS[10][2][3] = {
		{{0, 0, -1}, {0, 0, 1}},
		{{-1, 0, 0}, {1, 0, 0}},
		{{-1, -1, 0}, {1, 0, 0}},
		{{-1, 0, 0}, {1, -1, 0}},
		{{0, 0, -1}, {0, -1, 1}},
		{{0, -1, -1}, {0, 0, 1}},
		{{0, 0, 1}, {1, 0, 0}},
		{{0, 0, 1}, {-1, 0, 0}},
		{{0, 0, -1}, {-1, 0, 0}},
		{{0, 0, -1}, {1, 0, 0}}
	};

	int entityId = 1;
	float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
	float prevPosX = 0.0f, prevPosY = 0.0f, prevPosZ = 0.0f;
	float motionX = 0.0f, motionY = 0.0f, motionZ = 0.0f;
	float yaw = 0.0f, prevYaw = 0.0f;
	float pitch = 0.0f, prevPitch = 0.0f;
	bool onGround = false;
	bool isInReverse = false;
	float fallDistance = 0.0f;
	int hurtTime = 0;
	int hurtDir = 1;
	float damage = 0.0f;
	AABB bb;

	bool riderIsPlayer = false;
	bool riderForward = false;
	float riderFwdX = 0.0f;
	float riderFwdZ = 0.0f;
	int riderMobId = 0;

	bool hasRider() const {
		return riderIsPlayer || riderMobId > 0;
	}

	float getRideHeight() const {
		return -0.3f;
	}

	void setPos(float x, float y, float z) {
		posX = x;
		posY = y;
		posZ = z;
		float r = kWidth * 0.5f;
		float minY = y - kRidingHeight;
		bb.set(x - r, minY, z - r, x + r, minY + kHeight, z + r);
	}

	void applyNaturalSlowdown() {
		float f = hasRider() ? 0.997f : 0.96f;
		motionX *= f;
		motionY = 0.0f;
		motionZ *= f;
	}

	void push(float dx, float dy, float dz) {
		motionX += dx;
		motionY += dy;
		motionZ += dz;
	}

	static bool getPos(BlockSource& region, Vec3& result, float x, float y, float z) {
		int bx = Math::floor(x);
		int by = Math::floor(y);
		int bz = Math::floor(z);
		if (BaseRailBlock::isRail(region, BlockPos(bx, by - 1, bz))) {
			--by;
		}
		int id = region.getBlockID(bx, by, bz);
		if (!BaseRailBlock::isRail(id)) {
			return false;
		}
		int meta = region.getData(BlockPos(bx, by, bz));
		const Block* blk = Block::mBlocks[id];
		if (blk && blk->isRailBlock() && static_cast<const BaseRailBlock*>(blk)->isUsesDataBit()) {
			meta &= 7;
		}
		if (meta < 0 || meta > 9) {
			meta = 0;
		}

		float v16 = (static_cast<float>(bx) + 0.5f) + static_cast<float>(EXITS[meta][0][0]) * 0.5f;
		float v17 = (static_cast<float>(by) + 0.5f) + static_cast<float>(EXITS[meta][0][1]) * 0.5f;
		float v18 = (static_cast<float>(bz) + 0.5f) + static_cast<float>(EXITS[meta][0][2]) * 0.5f;
		float v19 = ((static_cast<float>(bx) + 0.5f) + static_cast<float>(EXITS[meta][1][0]) * 0.5f) - v16;
		float v20 = (((static_cast<float>(by) + 0.5f) + static_cast<float>(EXITS[meta][1][1]) * 0.5f) - v17) * 2.0f;
		float v21 = ((static_cast<float>(bz) + 0.5f) + static_cast<float>(EXITS[meta][1][2]) * 0.5f) - v18;

		float t = 0.0f;
		if (v19 == 0.0f) {
			t = z - static_cast<float>(bz);
		} else if (v21 == 0.0f) {
			t = x - static_cast<float>(bx);
		} else {
			t = ((z - v18) * v21 + (x - v16) * v19) * 2.0f;
		}

		result.x = v16 + v19 * t;
		float ry = v17 + v20 * t;
		if (v20 < 0.0f) ry += 1.0f;
		if (v20 > 0.0f) ry += 0.5f;
		result.y = ry;
		result.z = v18 + v21 * t;
		return true;
	}

	static bool getPosOffs(BlockSource& region, Vec3& result, float x, float y, float z, float offs) {
		int bx = Math::floor(x);
		int by = Math::floor(y);
		int bz = Math::floor(z);
		if (BaseRailBlock::isRail(region, BlockPos(bx, by - 1, bz))) {
			--by;
		}
		int id = region.getBlockID(bx, by, bz);
		if (!BaseRailBlock::isRail(id)) {
			return false;
		}
		int meta = region.getData(BlockPos(bx, by, bz));
		const Block* blk = Block::mBlocks[id];
		if (blk && blk->isRailBlock() && static_cast<const BaseRailBlock*>(blk)->isUsesDataBit()) {
			meta &= 7;
		}
		if (meta < 0 || meta > 9) {
			meta = 0;
		}

		float py = static_cast<float>(by);
		if (meta >= 2 && meta <= 5) {
			py += 1.0f;
		}

		int e00 = EXITS[meta][0][0], e01 = EXITS[meta][0][1], e02 = EXITS[meta][0][2];
		int e10 = EXITS[meta][1][0], e11 = EXITS[meta][1][1], e12 = EXITS[meta][1][2];

		float dx = static_cast<float>(e10 - e00);
		float dz = static_cast<float>(e12 - e02);
		float dist = std::sqrt(dx * dx + dz * dz);
		if (dist < 1e-5f) return false;

		float nx = x + (dx / dist) * offs;
		float nz = z + (dz / dist) * offs;
		if (e01 != 0 && Math::floor(nx) - bx == e00 && Math::floor(nz) - bz == e02) {
			py += static_cast<float>(e01);
		} else if (e11 != 0 && Math::floor(nx) - bx == e10 && Math::floor(nz) - bz == e12) {
			py += static_cast<float>(e11);
		}
		return getPos(region, result, nx, py, nz);
	}

	void move(BlockSource& region, float dx, float dy, float dz) {
		float origDx = dx, origDy = dy, origDz = dz;
		std::vector<AABB>& boxes = region.fetchCollisionShapes(region, bb.expanded(Vec3(dx, dy, dz)), nullptr, false, nullptr);

		for (const auto& box : boxes) {
			dy = box.clipYCollide(bb, dy);
		}
		bb.move(0.0f, dy, 0.0f);

		for (const auto& box : boxes) {
			dx = box.clipXCollide(bb, dx);
		}
		bb.move(dx, 0.0f, 0.0f);

		for (const auto& box : boxes) {
			dz = box.clipZCollide(bb, dz);
		}
		bb.move(0.0f, 0.0f, dz);

		onGround = (origDy != dy && origDy < 0.0f);
		if (origDx != dx) motionX = 0.0f;
		if (origDy != dy) motionY = 0.0f;
		if (origDz != dz) motionZ = 0.0f;

		posX = (bb.min.x + bb.max.x) * 0.5f;
		posY = bb.min.y + kRidingHeight;
		posZ = (bb.min.z + bb.max.z) * 0.5f;
	}

	void moveAlongTrack(BlockSource& region, int x, int y, int z, float maxSpeed, float boost, int id, int rawMeta) {
		fallDistance = 0.0f;
		Vec3 posVec1(0.0f, 0.0f, 0.0f);
		bool hasTrackPos = getPos(region, posVec1, posX, posY, posZ);
		posY = static_cast<float>(y);

		bool isGoldenRail = (Block::mGoldenRail && id == Block::mGoldenRail->mID);
		bool powered = isGoldenRail && ((rawMeta & BaseRailBlock::RAIL_DATA_BIT) != 0);

		int meta = rawMeta;
		const Block* blk = (id > 0 && id < 256) ? Block::mBlocks[id] : nullptr;
		if (blk && blk->isRailBlock() && static_cast<const BaseRailBlock*>(blk)->isUsesDataBit()) {
			meta &= 7;
		}
		if (meta < 0 || meta > 9) {
			meta = 0;
		}

		if (meta >= 2 && meta <= 5) {
			posY = static_cast<float>(y) + 1.0f;
		}
		if (meta == 2) motionX -= boost;
		else if (meta == 3) motionX += boost;
		else if (meta == 4) motionZ += boost;
		else if (meta == 5) motionZ -= boost;

		int mat00 = EXITS[meta][0][0], mat01 = EXITS[meta][0][1], mat02 = EXITS[meta][0][2];
		int mat10 = EXITS[meta][1][0], mat11 = EXITS[meta][1][1], mat12 = EXITS[meta][1][2];

		float matXDiff = static_cast<float>(mat10 - mat00);
		float matZDiff = static_cast<float>(mat12 - mat02);
		float matDiffTotal = std::sqrt(matXDiff * matXDiff + matZDiff * matZDiff);
		if (matDiffTotal > 1e-5f) {
			if (motionZ * matZDiff + motionX * matXDiff < 0.0f) {
				matXDiff = -matXDiff;
				matZDiff = -matZDiff;
			}
			float speedTotal = std::sqrt(motionX * motionX + motionZ * motionZ);
			if (speedTotal > 2.0f) speedTotal = 2.0f;
			motionX = (speedTotal * matXDiff) / matDiffTotal;
			motionZ = (speedTotal * matZDiff) / matDiffTotal;
		}

		if (riderIsPlayer && riderForward) {
			if (motionX * motionX + motionZ * motionZ < 0.01f) {
				motionX += riderFwdX * 0.1f;
				motionZ += riderFwdZ * 0.1f;
			}
		}

		float v38 = (static_cast<float>(x) + 0.5f) + static_cast<float>(mat00) * 0.5f;
		float v39 = (static_cast<float>(z) + 0.5f) + static_cast<float>(mat02) * 0.5f;
		float v40 = ((static_cast<float>(x) + 0.5f) + static_cast<float>(mat10) * 0.5f) - v38;
		float v41 = ((static_cast<float>(z) + 0.5f) + static_cast<float>(mat12) * 0.5f) - v39;

		float v42 = 0.0f;
		if (v40 == 0.0f) {
			v42 = posZ - static_cast<float>(z);
		} else if (v41 == 0.0f) {
			v42 = posX - static_cast<float>(x);
		} else {
			v42 = ((posZ - v39) * v41 + (posX - v38) * v40) * 2.0f;
		}

		float newX = v38 + v40 * v42;
		float newZ = v39 + v41 * v42;
		setPos(newX, posY + kRidingHeight + 0.00001f, newZ);

		float dx = motionX;
		float dz = motionZ;
		if (hasRider()) {
			dx *= 0.75f;
			dz *= 0.75f;
		}
		dx = std::clamp(dx, -maxSpeed, maxSpeed);
		dz = std::clamp(dz, -maxSpeed, maxSpeed);
		move(region, dx, 0.0f, dz);

		if (mat01 != 0 && Math::floor(posX) - x == mat00 && Math::floor(posZ) - z == mat02) {
			setPos(posX, posY + static_cast<float>(mat01), posZ);
		} else if (mat11 != 0 && Math::floor(posX) - x == mat10 && Math::floor(posZ) - z == mat12) {
			setPos(posX, posY + static_cast<float>(mat11), posZ);
		}

		applyNaturalSlowdown();

		Vec3 posVec2(0.0f, 0.0f, 0.0f);
		if (getPos(region, posVec2, posX, posY, posZ) && hasTrackPos) {
			float yDiff = (posVec1.y - posVec2.y) * 0.05f;
			float spd = std::sqrt(motionX * motionX + motionZ * motionZ);
			if (spd > 0.0f) {
				motionX = (motionX / spd) * (spd + yDiff);
				motionZ = (motionZ / spd) * (spd + yDiff);
			}
			setPos(posX, posVec2.y, posZ);
		}

		int nx = Math::floor(posX);
		int nz = Math::floor(posZ);
		if (nx != x || nz != z) {
			float spd = std::sqrt(motionX * motionX + motionZ * motionZ);
			motionX = spd * static_cast<float>(nx - x);
			motionZ = spd * static_cast<float>(nz - z);
		}

		if (isGoldenRail) {
			if (powered) {
				float totalSpeed = std::sqrt(motionX * motionX + motionZ * motionZ);
				if (totalSpeed > 0.01f) {
					motionX += (motionX / totalSpeed) * 0.06f;
					motionZ += (motionZ / totalSpeed) * 0.06f;
				} else if (meta == 1) {
					if (region.isSolidBlockingBlock(BlockPos(x - 1, y, z))) {
						motionX = 0.02f;
					} else if (region.isSolidBlockingBlock(BlockPos(x + 1, y, z))) {
						motionX = -0.02f;
					}
				} else if (meta == 0) {
					if (region.isSolidBlockingBlock(BlockPos(x, y, z - 1))) {
						motionZ = 0.02f;
					} else if (region.isSolidBlockingBlock(BlockPos(x, y, z + 1))) {
						motionZ = -0.02f;
					}
				}
			} else {
				float totalSpeed = std::sqrt(motionX * motionX + motionZ * motionZ);
				if (totalSpeed < 0.03f) {
					motionX = 0.0f;
					motionY = 0.0f;
					motionZ = 0.0f;
				} else {
					motionX *= 0.5f;
					motionY = 0.0f;
					motionZ *= 0.5f;
				}
			}
		}
	}

	void comeOffTrack(BlockSource& region, float maxSpeed) {
		motionX = std::clamp(motionX, -maxSpeed, maxSpeed);
		motionZ = std::clamp(motionZ, -maxSpeed, maxSpeed);
		if (onGround) {
			motionX *= 0.5f;
			motionY *= 0.5f;
			motionZ *= 0.5f;
		}
		move(region, motionX, motionY, motionZ);
		if (!onGround) {
			motionX *= 0.95f;
			motionY *= 0.95f;
			motionZ *= 0.95f;
		}
	}

	void tick(BlockSource& region) {
		if (hurtTime > 0) --hurtTime;
		if (damage > 0.0f) damage = std::max(0.0f, damage - 1.0f);

		prevPosX = posX;
		prevPosY = posY;
		prevPosZ = posZ;
		prevYaw = yaw;
		prevPitch = pitch;

		motionY -= 0.04f;
		int bx = Math::floor(posX);
		int by = Math::floor(posY);
		int bz = Math::floor(posZ);
		if (BaseRailBlock::isRail(region, BlockPos(bx, by - 1, bz))) {
			--by;
		}
		int id = region.getBlockID(bx, by, bz);
		if (BaseRailBlock::isRail(id)) {
			int meta = region.getData(BlockPos(bx, by, bz));
			moveAlongTrack(region, bx, by, bz, 0.4f, 0.0078125f, id, meta);
		} else {
			comeOffTrack(region, 0.4f);
		}

		float dx = prevPosX - posX;
		float dz = prevPosZ - posZ;
		pitch = 0.0f;
		if (dx * dx + dz * dz > 0.001f) {
			yaw = std::atan2(dz, dx) * (180.0f / 3.14159265f);
			if (isInReverse) {
				yaw += 180.0f;
			}
		}
		float yawDiff = std::fmod(yaw - prevYaw, 360.0f);
		if (yawDiff >= 180.0f) yawDiff -= 360.0f;
		if (yawDiff < -180.0f) yawDiff += 360.0f;
		if (yawDiff < -170.0f || yawDiff >= 170.0f) {
			yaw += 180.0f;
			isInReverse = !isInReverse;
		}
		while (yaw - prevYaw < -180.0f) prevYaw -= 360.0f;
		while (yaw - prevYaw >= 180.0f) prevYaw += 360.0f;
	}

	bool hurt(int dmg, bool isCreative, BlockSource& region) {
		hurtDir = -hurtDir;
		hurtTime = 10;
		damage += static_cast<float>(10 * dmg);

		if (!isCreative && damage <= 40.0f) {
			return false;
		}

		for (int i = 0; i < 4; ++i) {
			float sx = posX + (ParticleSystem::rand01() * 0.3f) - 0.15f;
			float sy = posY + (ParticleSystem::rand01() * 0.1f);
			float sz = posZ + (ParticleSystem::rand01() * 0.3f) - 0.15f;
			ParticleSystem::spawnSmoke(sx, sy, sz, 0.0f, 0.0f, 0.0f, 100, &region);
		}
		return true;
	}
};

inline std::vector<MinecartEntity>& getMinecarts() {
	static std::vector<MinecartEntity> minecarts;
	return minecarts;
}

inline int& getNextEntityId() {
	static int nextId = 1;
	return nextId;
}

inline MinecartModel& getModel() {
	static MinecartModel model;
	return model;
}

inline void clear() {
	getMinecarts().clear();
	getNextEntityId() = 1;
}

inline MinecartEntity& spawnMinecart(float x, float y, float z) {
	MinecartEntity mc;
	mc.entityId = getNextEntityId()++;
	mc.setPos(x, y, z);
	mc.prevPosX = x;
	mc.prevPosY = y;
	mc.prevPosZ = z;
	getMinecarts().push_back(mc);
	return getMinecarts().back();
}

inline MinecartEntity* findById(int id) {
	if (id <= 0) return nullptr;
	for (auto& mc : getMinecarts()) {
		if (mc.entityId == id) return &mc;
	}
	return nullptr;
}

inline int raycastMinecart(const Vec3& origin, const Vec3& dir, float maxDist, float& outDist) {
	const auto& carts = getMinecarts();
	float bestT = maxDist;
	int bestIdx = -1;
	for (size_t i = 0; i < carts.size(); ++i) {
		const AABB& b = carts[i].bb;
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
	outDist = bestT;
	return bestIdx;
}

inline void pushCartFromEntity(MinecartEntity& cart, float otherX, float otherZ, Vec3* otherDelta) {
	float dx = otherX - cart.posX;
	float dz = otherZ - cart.posZ;
	float sq = dx * dx + dz * dz;
	if (sq < 0.0001f) return;
	float dist = std::sqrt(sq);
	float nx = dx / dist;
	float nz = dz / dist;
	float inv = std::min(1.0f, 1.0f / dist);
	float pushZ = nz * inv * 0.1f * 0.5f;
	float pushX = nx * inv * 0.1f * 0.5f;
	cart.push(-pushX, 0.0f, -pushZ);
	if (otherDelta) {
		otherDelta->x += pushX * 0.25f;
		otherDelta->z += pushZ * 0.25f;
	}
}

inline void syncMobRiders(std::vector<WorldMobEntity>& mobs) {
	for (auto& mc : getMinecarts()) {
		if (mc.riderMobId <= 0) continue;
		bool found = false;
		for (auto& m : mobs) {
			if (m.id == mc.riderMobId && !m.isDying) {
				m.pos = Vec3(mc.posX, mc.posY + mc.getRideHeight(), mc.posZ);
				m.vel = Vec3::ZERO;
				m.onGround = true;
				found = true;
				break;
			}
		}
		if (!found) {
			mc.riderMobId = 0;
		}
	}
}

inline void tickAll(BlockSource& region,
                    std::vector<WorldMobEntity>& mobs,
                    const AABB& playerBB,
                    const Vec3& playerPos,
                    Vec3& playerDelta,
                    int& ridingMinecartId) {
	auto& carts = getMinecarts();
	for (size_t i = 0; i < carts.size(); ) {
		MinecartEntity& mc = carts[i];
		mc.riderIsPlayer = (ridingMinecartId == mc.entityId);
		mc.tick(region);
		if (mc.posY < -64.0f) {
			if (ridingMinecartId == mc.entityId) {
				ridingMinecartId = 0;
			}
			carts[i] = carts.back();
			carts.pop_back();
			continue;
		}
		++i;
	}

	// Collisions between minecarts (exact 0.8.1 Minecart::push)
	for (size_t i = 0; i < carts.size(); ++i) {
		MinecartEntity& a = carts[i];
		AABB expanded(a.bb.min.x - 0.2f, a.bb.min.y, a.bb.min.z - 0.2f,
		              a.bb.max.x + 0.2f, a.bb.max.y, a.bb.max.z + 0.2f);
		for (size_t j = i + 1; j < carts.size(); ++j) {
			MinecartEntity& b = carts[j];
			if (!expanded.intersects(b.bb)) continue;

			float dx = a.posX - b.posX;
			float dz = a.posZ - b.posZ;
			float sq = dx * dx + dz * dz;
			if (sq < 0.0001f) continue;

			float dist = std::sqrt(sq);
			float nx = dx / dist;
			float nz = dz / dist;
			float inv = std::min(1.0f, 1.0f / dist);
			float v12 = nx * inv * 0.1f * 0.5f;
			float v11 = nz * inv * 0.1f * 0.5f;

			float yawRad = (b.yaw * 3.14159265f) / 180.0f;
			float dot = std::abs(nx * std::cos(yawRad) + nz * std::sin(yawRad));
			if (dot >= 0.8f) {
				float avgX = (a.motionX + b.motionX) * 0.5f;
				float avgZ = (a.motionZ + b.motionZ) * 0.5f;
				b.motionX *= 0.2f;
				b.motionZ *= 0.2f;
				b.push(avgX - v12, 0.0f, avgZ - v11);
				a.motionX *= 0.2f;
				a.motionZ *= 0.2f;
				a.push(avgX + v12, 0.0f, avgZ + v11);
			}
		}

		// Player collision / push
		if (ridingMinecartId != a.entityId && expanded.intersects(playerBB)) {
			pushCartFromEntity(a, playerPos.x, playerPos.z, &playerDelta);
		}

		// Mob collision / automatic mounting
		for (auto& m : mobs) {
			if (m.isDying || m.id == a.riderMobId) continue;
			if (!expanded.intersects(m.getAABB())) continue;

			float speedSq = a.motionX * a.motionX + a.motionZ * a.motionZ;
			if (speedSq > 0.01f && !a.hasRider()) {
				bool alreadyRidingOther = false;
				for (const auto& other : carts) {
					if (other.riderMobId == m.id) {
						alreadyRidingOther = true;
						break;
					}
				}
				if (!alreadyRidingOther) {
					a.riderMobId = m.id;
					continue;
				}
			}
			Vec3 mobDelta(0.0f, 0.0f, 0.0f);
			pushCartFromEntity(a, m.pos.x, m.pos.z, &mobDelta);
			m.vel.x += mobDelta.x;
			m.vel.z += mobDelta.z;
		}
	}

	syncMobRiders(mobs);
}

inline void renderAll(BlockSource& region, float alpha, float sunFactor) {
	const auto& carts = getMinecarts();
	if (carts.empty()) return;
	if (!GuiComponent::bindTexture("textures/entity/minecart.png")) return;

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.1f);
	glDisable(GL_CULL_FACE);

	MinecartModel& model = getModel();
	for (const auto& e : carts) {
		glPushMatrix();

		int64_t v13 = 493286711LL * static_cast<int64_t>(e.entityId);
		int64_t v15 = v13 * v13 * 4392167121LL + v13 * 98761LL;
		glTranslatef(((((v15 >> 16) & 7) + 0.5f) * 0.125f - 0.5f) * 0.004f,
		             ((((v15 >> 20) & 7) + 0.5f) * 0.125f - 0.5f) * 0.004f,
		             ((((v15 >> 24) & 7) + 0.5f) * 0.125f - 0.5f) * 0.004f);

		float ix = e.prevPosX + (e.posX - e.prevPosX) * alpha;
		float iy = e.prevPosY + (e.posY - e.prevPosY) * alpha;
		float iz = e.prevPosZ + (e.posZ - e.prevPosZ) * alpha;

		float yawDiff = e.yaw - e.prevYaw;
		while (yawDiff < -180.0f) yawDiff += 360.0f;
		while (yawDiff >= 180.0f) yawDiff -= 360.0f;
		float renderYaw = e.prevYaw + yawDiff * alpha;
		float renderPitch = e.prevPitch + (e.pitch - e.prevPitch) * alpha;

		float rx = ix, ry = iy, rz = iz;
		Vec3 posVec(0.0f, 0.0f, 0.0f);
		if (MinecartEntity::getPos(region, posVec, ix, iy, iz)) {
			Vec3 offsPos(0.0f, 0.0f, 0.0f);
			Vec3 offsNeg(0.0f, 0.0f, 0.0f);
			if (!MinecartEntity::getPosOffs(region, offsPos, ix, iy, iz, 0.3f)) {
				offsPos = posVec;
			}
			if (!MinecartEntity::getPosOffs(region, offsNeg, ix, iy, iz, -0.3f)) {
				offsNeg = posVec;
			}
			rx += (posVec.x - ix);
			ry += (((offsPos.y + offsNeg.y) * 0.5f) - iy);
			rz += (posVec.z - iz);

			float dx = offsNeg.x - offsPos.x;
			float dy = offsNeg.y - offsPos.y;
			float dz = offsNeg.z - offsPos.z;
			float len = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (len >= 0.0001f) {
				float xx = dx / len;
				float yy = dy / len;
				float zz = dz / len;
				renderYaw = std::atan2(zz, xx) * (180.0f / 3.14159265f);
				renderPitch = std::atan(yy) * 73.0f;
			}
		}

		glTranslatef(rx, ry, rz);
		glRotatef(180.0f - renderYaw, 0.0f, 1.0f, 0.0f);
		glRotatef(-renderPitch, 0.0f, 0.0f, 1.0f);

		float hurtT = static_cast<float>(e.hurtTime) - alpha;
		float dmg = std::max(0.0f, e.damage - alpha);
		if (hurtT > 0.0f) {
			float wobble = (hurtT * std::sin(hurtT) * dmg) / 10.0f;
			glRotatef(wobble * static_cast<float>(e.hurtDir), 1.0f, 0.0f, 0.0f);
		}

		glScalef(-1.0f, -1.0f, 1.0f);

		float light = WorldLighting::sampleWorldBrightness(
			region, BlockPos(Math::floor(ix), Math::floor(iy), Math::floor(iz)), sunFactor);
		light = std::clamp(light, 0.25f, 1.0f);

		model.render(-0.1f, 0.0625f, light);
		glPopMatrix();
	}

	glDisable(GL_ALPHA_TEST);
	glEnable(GL_CULL_FACE);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

inline Unique<CompoundTag> saveMinecartTag(const MinecartEntity& mc) {
	auto tag = make_unique<CompoundTag>();
	tag->putInt("id", 84);
	tag->put("Pos", ListTagFloatAdder(mc.posX)(mc.posY)(mc.posZ).done());
	tag->put("Motion", ListTagFloatAdder(mc.motionX)(mc.motionY)(mc.motionZ).done());
	tag->put("Rotation", ListTagFloatAdder(mc.yaw)(mc.pitch).done());
	return tag;
}

inline bool loadMinecartTag(const CompoundTag& tag) {
	if (tag.getInt("id") != 84) return false;
	const ListTag* posList = tag.getList("Pos");
	if (!posList || posList->size() < 3) return false;

	MinecartEntity& mc = spawnMinecart(posList->getFloat(0), posList->getFloat(1), posList->getFloat(2));
	const ListTag* motList = tag.getList("Motion");
	if (motList && motList->size() >= 3) {
		mc.motionX = motList->getFloat(0);
		mc.motionY = motList->getFloat(1);
		mc.motionZ = motList->getFloat(2);
	}
	const ListTag* rotList = tag.getList("Rotation");
	if (rotList && rotList->size() >= 2) {
		mc.yaw = mc.prevYaw = rotList->getFloat(0);
		mc.pitch = mc.prevPitch = rotList->getFloat(1);
	}
	return true;
}

} // namespace MinecartSystem
