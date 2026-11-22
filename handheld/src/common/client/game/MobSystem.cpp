#include "common_header.h"
#include "client/game/MobSystem.h"
#include "client/game/WorldLighting.h"
#include "client/gui/GuiComponent.h"
#include "util/Math.h"
#include "util/Random.h"
#include "world/item/Item.h"
#include "world/level/BlockPos.h"
#include "world/level/BlockSource.h"
#include "world/level/LightLayer.h"
#include "world/level/block/Block.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/material/Material.h"
#include "world/phys/HitResult.h"
#include "stb_image.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <unordered_map>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;
constexpr float kPxScale = 0.0625f;

Random& mobRng() {
	static Random rng;
	return rng;
}

float wrapDegrees(float deg) {
	while (deg >= 180.0f) deg -= 360.0f;
	while (deg < -180.0f) deg += 360.0f;
	return deg;
}

float approachAngle(float current, float target, float maxDelta) {
	float diff = wrapDegrees(target - current);
	if (diff > maxDelta) diff = maxDelta;
	else if (diff < -maxDelta) diff = -maxDelta;
	return wrapDegrees(current + diff);
}

// MCPE 0.8.1 yaw convention: 0 = +Z, -90 = +X, +90 = -X, +-180 = -Z
float dirToMcpeYaw(float dx, float dz) {
	return std::atan2(dz, dx) * kRadToDeg - 90.0f;
}

float rayIntersectsAABB(const Vec3& origin, const Vec3& dir, const AABB& box, float maxDist) {
	float tMin = 0.0f;
	float tMax = maxDist;
	const float orig[3] = { origin.x, origin.y, origin.z };
	const float d[3] = { dir.x, dir.y, dir.z };
	const float bmin[3] = { box.min.x, box.min.y, box.min.z };
	const float bmax[3] = { box.max.x, box.max.y, box.max.z };

	for (int i = 0; i < 3; ++i) {
		if (std::abs(d[i]) < 1e-6f) {
			if (orig[i] < bmin[i] || orig[i] > bmax[i]) return -1.0f;
		} else {
			float invD = 1.0f / d[i];
			float t1 = (bmin[i] - orig[i]) * invD;
			float t2 = (bmax[i] - orig[i]) * invD;
			if (t1 > t2) std::swap(t1, t2);
			tMin = std::max(tMin, t1);
			tMax = std::min(tMax, t2);
			if (tMin > tMax) return -1.0f;
		}
	}
	return (tMin >= 0.0f && tMin <= maxDist) ? tMin : -1.0f;
}

// ============================================================================
// MCPE 0.8.1 faithful Cube + PolygonQuad + ModelPart renderer
// ============================================================================

struct MobQuad {
	Vec3 v[4];
	float u[4];
	float tv[4];
	Vec3 normal;

	void init(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3,
	          int a, int b, int c, int d, float texW, float texH, bool mirror) {
		v[0] = v0; v[1] = v1; v[2] = v2; v[3] = v3;
		float u0 = static_cast<float>(a) / texW;
		float vTop = static_cast<float>(b) / texH;
		float u1 = static_cast<float>(c) / texW;
		float vBot = static_cast<float>(d) / texH;
		u[0] = u1; tv[0] = vTop;
		u[1] = u0; tv[1] = vTop;
		u[2] = u0; tv[2] = vBot;
		u[3] = u1; tv[3] = vBot;

		if (mirror) {
			std::swap(v[0], v[3]);
			std::swap(v[1], v[2]);
			std::swap(u[0], u[3]);
			std::swap(u[1], u[2]);
			std::swap(tv[0], tv[3]);
			std::swap(tv[1], tv[2]);
		}

		// Match PolygonQuad::render normal calculation (cross(v0 - v1, v0 - v3))
		Vec3 e1 = v[0] - v[1];
		Vec3 e2 = v[0] - v[3];
		Vec3 n(e1.y * e2.z - e1.z * e2.y,
		       e1.z * e2.x - e1.x * e2.z,
		       e1.x * e2.y - e1.y * e2.x);
		float len = n.length();
		normal = (len > 1e-5f) ? (n / len) : Vec3(0.0f, -1.0f, 0.0f);
	}
};

struct MobCube {
	MobQuad quads[6];

	MobCube(int u, int v, float x, float y, float z, int w, int h, int d,
	        float inflate, float texW, float texH, bool mirror) {
		float x1 = x + static_cast<float>(w);
		float y1 = y + static_cast<float>(h);
		float z1 = z + static_cast<float>(d);

		float v20 = x - inflate;
		float v21 = y - inflate;
		float v22 = z - inflate;
		float v23 = x1 + inflate;
		float v24 = y1 + inflate;
		float v25 = z1 + inflate;
		if (!mirror) {
			std::swap(v20, v23);
		}

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
		quads[0].init(p[5], p[1], p[2], p[6], uMid, vMid, uMid + d, vMid + h, texW, texH, mirror);
		quads[1].init(p[0], p[4], p[7], p[3], u, vMid, u + d, vMid + h, texW, texH, mirror);
		quads[2].init(p[5], p[4], p[0], p[1], u + d, v, uMid, v + d, texW, texH, mirror);
		quads[3].init(p[2], p[3], p[7], p[6], uMid, v + d, uMid + w, v, texW, texH, mirror);
		quads[4].init(p[1], p[0], p[3], p[2], u + d, vMid, uMid, vMid + h, texW, texH, mirror);
		quads[5].init(p[4], p[5], p[6], p[7], uMid + d, vMid, uMid + d + w, vMid + h, texW, texH, mirror);
	}
};

struct MobModelPart {
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
	bool mirror = false;
	std::vector<MobCube> cubes;

	MobModelPart() = default;
	MobModelPart(int u, int v, int tw = 64, int th = 32, bool mir = false)
		: texOffX(u), texOffY(v), texW(static_cast<float>(tw)), texH(static_cast<float>(th)), mirror(mir) {}

	MobModelPart& setPos(float x, float y, float z) {
		xOffset = x;
		yOffset = y;
		zOffset = z;
		return *this;
	}

	MobModelPart& texOffs(int u, int v) {
		texOffX = u;
		texOffY = v;
		return *this;
	}

	MobModelPart& addBox(float x, float y, float z, int w, int h, int d, float inflate = 0.0f) {
		cubes.emplace_back(texOffX, texOffY, x, y, z, w, h, d, inflate, texW, texH, mirror);
		return *this;
	}

	void translateTo(float scale) const {
		if (xOffset != 0.0f || yOffset != 0.0f || zOffset != 0.0f) {
			glTranslatef(scale * xOffset, scale * yOffset, scale * zOffset);
		}
		if (zRot != 0.0f) glRotatef(zRot * kRadToDeg, 0.0f, 0.0f, 1.0f);
		if (yRot != 0.0f) glRotatef(yRot * kRadToDeg, 0.0f, 1.0f, 0.0f);
		if (xRot != 0.0f) glRotatef(xRot * kRadToDeg, 1.0f, 0.0f, 0.0f);
	}

	void render(float scale, float tr, float tg, float tb, float ta = 1.0f, bool flatOverlay = false) const {
		glPushMatrix();
		translateTo(scale);

		glm::mat3 rot(1.0f);
		if (!flatOverlay) {
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
		}

		glBegin(GL_QUADS);
		for (const auto& cube : cubes) {
			for (int qi = 0; qi < 6; ++qi) {
				const MobQuad& q = cube.quads[qi];
				if (flatOverlay) {
					glColor4f(tr, tg, tb, ta);
				} else {
					glm::vec3 wn = rot * glm::vec3(q.normal.x, q.normal.y, q.normal.z);
					float up = -wn.y;
					float faceShade = 0.88f;
					if (up > 0.55f) faceShade = 1.0f;
					else if (up < -0.55f) faceShade = 0.68f;
					else if (std::abs(wn.z) > 0.55f) faceShade = 0.94f;
					else faceShade = 0.82f;

					glColor4f(std::min(1.0f, tr * faceShade),
					          std::min(1.0f, tg * faceShade),
					          std::min(1.0f, tb * faceShade), ta);
				}
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

// ============================================================================
// Extruded 2D item sprite renderer (for Skeleton's bow in hand)
// ============================================================================

struct MobExtrudedEdge {
	float x0, y0, x1, y1, z0, z1;
	float u, v, shade;
	int axis;
};

const std::vector<MobExtrudedEdge>& getMobExtrudedEdges(const std::string& texPath) {
	static std::unordered_map<std::string, std::vector<MobExtrudedEdge>> cache;
	auto it = cache.find(texPath);
	if (it != cache.end()) return it->second;

	std::vector<MobExtrudedEdge> edges;
	std::string fullPath = GuiComponent::resolveAssetPath(texPath);
	int w = 0, h = 0, ch = 0;
	stbi_set_flip_vertically_on_load(0);
	unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &ch, 4);
	if (data && w > 0 && h > 0 && w <= 64 && h <= 64) {
		auto isOpaque = [&](int px, int py) -> bool {
			if (px < 0 || py < 0 || px >= w || py >= h) return false;
			return data[(py * w + px) * 4 + 3] >= 128;
		};
		const float halfZ = 0.03125f;
		const float fw = static_cast<float>(w);
		const float fh = static_cast<float>(h);
		for (int py = 0; py < h; ++py) {
			float y1 = 0.5f - static_cast<float>(py) / fh;
			float y0 = 0.5f - static_cast<float>(py + 1) / fh;
			float v = (static_cast<float>(py) + 0.5f) / fh;
			for (int px = 0; px < w; ++px) {
				if (!isOpaque(px, py)) continue;
				float x0 = -0.5f + static_cast<float>(px) / fw;
				float x1 = -0.5f + static_cast<float>(px + 1) / fw;
				float u = (static_cast<float>(px) + 0.5f) / fw;

				if (!isOpaque(px - 1, py)) edges.push_back({ x0, y0, x0, y1, -halfZ, halfZ, u, v, 0.72f, 0 });
				if (!isOpaque(px + 1, py)) edges.push_back({ x1, y0, x1, y1, -halfZ, halfZ, u, v, 0.72f, 0 });
				if (!isOpaque(px, py - 1)) edges.push_back({ x0, y1, x1, y1, -halfZ, halfZ, u, v, 0.92f, 1 });
				if (!isOpaque(px, py + 1)) edges.push_back({ x0, y0, x1, y0, -halfZ, halfZ, u, v, 0.62f, 1 });
			}
		}
	}
	if (data) stbi_image_free(data);
	auto res = cache.emplace(texPath, std::move(edges));
	return res.first->second;
}

void drawMobExtrudedSprite3D(const std::string& texPath, float tr, float tg, float tb) {
	if (!GuiComponent::bindTexture(texPath)) return;
	const float halfZ = 0.03125f;

	glBegin(GL_QUADS);
	glColor4f(std::min(1.0f, tr), std::min(1.0f, tg), std::min(1.0f, tb), 1.0f);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, -0.5f, halfZ);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(0.5f, -0.5f, halfZ);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(0.5f, 0.5f, halfZ);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, 0.5f, halfZ);

	glColor4f(std::min(1.0f, tr * 0.85f), std::min(1.0f, tg * 0.85f), std::min(1.0f, tb * 0.85f), 1.0f);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, -0.5f, -halfZ);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, 0.5f, -halfZ);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(0.5f, 0.5f, -halfZ);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(0.5f, -0.5f, -halfZ);

	const auto& edges = getMobExtrudedEdges(texPath);
	for (const auto& q : edges) {
		glColor4f(std::min(1.0f, tr * q.shade), std::min(1.0f, tg * q.shade), std::min(1.0f, tb * q.shade), 1.0f);
		glTexCoord2f(q.u, q.v);
		if (q.axis == 0) {
			glVertex3f(q.x0, q.y0, q.z0);
			glVertex3f(q.x0, q.y0, q.z1);
			glVertex3f(q.x0, q.y1, q.z1);
			glVertex3f(q.x0, q.y1, q.z0);
		} else {
			glVertex3f(q.x0, q.y0, q.z0);
			glVertex3f(q.x1, q.y0, q.z0);
			glVertex3f(q.x1, q.y0, q.z1);
			glVertex3f(q.x0, q.y0, q.z1);
		}
	}
	glEnd();
}

// ============================================================================
// MCPE 0.8.1 Mob Model Definitions (Pig, Cow, Sheep, Chicken, Zombie, Skeleton, Creeper)
// ============================================================================

struct PigModelData {
	MobModelPart head, body, leg0, leg1, leg2, leg3;
	PigModelData()
		: head(0, 0, 64, 32), body(28, 8, 64, 32),
		  leg0(0, 16, 64, 32), leg1(0, 16, 64, 32), leg2(0, 16, 64, 32), leg3(0, 16, 64, 32) {
		head.addBox(-4.0f, -4.0f, -8.0f, 8, 8, 8).texOffs(16, 16).addBox(-2.0f, 0.0f, -9.0f, 4, 3, 1).setPos(0.0f, 12.0f, -6.0f);
		body.addBox(-5.0f, -10.0f, -7.0f, 10, 16, 8).setPos(0.0f, 11.0f, 2.0f);
		body.xRot = 90.0f * kDegToRad;
		leg0.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4).setPos(-3.0f, 18.0f, 7.0f);
		leg1.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4).setPos(3.0f, 18.0f, 7.0f);
		leg2.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4).setPos(-3.0f, 18.0f, -5.0f);
		leg3.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4).setPos(3.0f, 18.0f, -5.0f);
	}
};

struct CowModelData {
	MobModelPart head, body, leg0, leg1, leg2, leg3;
	CowModelData()
		: head(0, 0, 64, 32), body(18, 4, 64, 32),
		  leg0(0, 16, 64, 32), leg1(0, 16, 64, 32), leg2(0, 16, 64, 32), leg3(0, 16, 64, 32) {
		head.addBox(-4.0f, -4.0f, -6.0f, 8, 8, 6)
			.texOffs(22, 0).addBox(-5.0f, -5.0f, -4.0f, 1, 3, 1)
			.texOffs(22, 0).addBox(4.0f, -5.0f, -4.0f, 1, 3, 1)
			.setPos(0.0f, 4.0f, -8.0f);
		body.addBox(-6.0f, -10.0f, -7.0f, 12, 18, 10)
			.texOffs(52, 0).addBox(-2.0f, 2.0f, -8.0f, 4, 6, 1)
			.setPos(0.0f, 5.0f, 2.0f);
		body.xRot = 90.0f * kDegToRad;
		leg0.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4).setPos(-4.0f, 12.0f, 7.0f);
		leg1.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4).setPos(4.0f, 12.0f, 7.0f);
		leg2.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4).setPos(-4.0f, 12.0f, -6.0f);
		leg3.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4).setPos(4.0f, 12.0f, -6.0f);
	}
};

struct SheepModelData {
	MobModelPart head, body, leg0, leg1, leg2, leg3;
	MobModelPart headWool, bodyWool, leg0Wool, leg1Wool, leg2Wool, leg3Wool;
	SheepModelData()
		: head(0, 0, 64, 64), body(28, 8, 64, 64),
		  leg0(0, 16, 64, 64), leg1(0, 16, 64, 64), leg2(0, 16, 64, 64), leg3(0, 16, 64, 64),
		  headWool(0, 32, 64, 64), bodyWool(28, 40, 64, 64),
		  leg0Wool(0, 48, 64, 64), leg1Wool(0, 48, 64, 64), leg2Wool(0, 48, 64, 64), leg3Wool(0, 48, 64, 64) {
		head.addBox(-3.0f, -4.0f, -6.0f, 6, 6, 8).setPos(0.0f, 6.0f, -8.0f);
		body.addBox(-4.0f, -10.0f, -7.0f, 8, 16, 6).setPos(0.0f, 5.0f, 2.0f);
		body.xRot = 90.0f * kDegToRad;
		leg0.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4).setPos(-3.0f, 12.0f, 7.0f);
		leg1.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4).setPos(3.0f, 12.0f, 7.0f);
		leg2.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4).setPos(-3.0f, 12.0f, -5.0f);
		leg3.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4).setPos(3.0f, 12.0f, -5.0f);

		headWool.addBox(-3.0f, -4.0f, -4.0f, 6, 6, 6, 0.6f).setPos(0.0f, 6.0f, -8.0f);
		bodyWool.addBox(-4.0f, -10.0f, -7.0f, 8, 16, 6, 1.75f).setPos(0.0f, 5.0f, 2.0f);
		bodyWool.xRot = 90.0f * kDegToRad;
		leg0Wool.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4, 0.5f).setPos(-3.0f, 12.0f, 7.0f);
		leg1Wool.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4, 0.5f).setPos(3.0f, 12.0f, 7.0f);
		leg2Wool.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4, 0.5f).setPos(-3.0f, 12.0f, -5.0f);
		leg3Wool.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4, 0.5f).setPos(3.0f, 12.0f, -5.0f);
	}
};

struct ChickenModelData {
	MobModelPart head, beak, wattle, body, rightLeg, leftLeg, rightWing, leftWing;
	ChickenModelData()
		: head(0, 0, 64, 32), beak(14, 0, 64, 32), wattle(14, 4, 64, 32),
		  body(0, 9, 64, 32), rightLeg(26, 0, 64, 32), leftLeg(26, 0, 64, 32),
		  rightWing(24, 13, 64, 32), leftWing(24, 13, 64, 32) {
		head.addBox(-2.0f, -6.0f, -2.0f, 4, 6, 3).setPos(0.0f, 15.0f, -4.0f);
		beak.addBox(-2.0f, -4.0f, -4.0f, 4, 2, 2).setPos(0.0f, 15.0f, -4.0f);
		wattle.addBox(-1.0f, -2.0f, -3.0f, 2, 2, 2).setPos(0.0f, 15.0f, -4.0f);
		body.addBox(-3.0f, -4.0f, -3.0f, 6, 8, 6).setPos(0.0f, 16.0f, 0.0f);
		body.xRot = 90.0f * kDegToRad;
		rightLeg.addBox(-1.0f, 0.0f, -3.0f, 3, 5, 3).setPos(-2.0f, 19.0f, 1.0f);
		leftLeg.addBox(-1.0f, 0.0f, -3.0f, 3, 5, 3).setPos(1.0f, 19.0f, 1.0f);
		rightWing.addBox(0.0f, 0.0f, -3.0f, 1, 4, 6).setPos(-4.0f, 13.0f, 0.0f);
		leftWing.addBox(-1.0f, 0.0f, -3.0f, 1, 4, 6).setPos(4.0f, 13.0f, 0.0f);
	}
};

struct ZombieModelData {
	MobModelPart head, body, rightArm, leftArm, rightLeg, leftLeg;
	ZombieModelData()
		: head(0, 0, 64, 32), body(16, 16, 64, 32),
		  rightArm(40, 16, 64, 32), leftArm(40, 16, 64, 32, true),
		  rightLeg(0, 16, 64, 32), leftLeg(0, 16, 64, 32, true) {
		head.addBox(-4.0f, -8.0f, -4.0f, 8, 8, 8).setPos(0.0f, 0.0f, 0.0f);
		body.addBox(-4.0f, 0.0f, -2.0f, 8, 12, 4).setPos(0.0f, 0.0f, 0.0f);
		rightArm.addBox(-3.0f, -2.0f, -2.0f, 4, 12, 4).setPos(-5.0f, 2.0f, 0.0f);
		leftArm.addBox(-1.0f, -2.0f, -2.0f, 4, 12, 4).setPos(5.0f, 2.0f, 0.0f);
		rightLeg.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4).setPos(-2.0f, 12.0f, 0.0f);
		leftLeg.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4).setPos(2.0f, 12.0f, 0.0f);
	}
};

struct SkeletonModelData {
	MobModelPart head, body, rightArm, leftArm, rightLeg, leftLeg;
	SkeletonModelData()
		: head(0, 0, 64, 32), body(16, 16, 64, 32),
		  rightArm(40, 16, 64, 32), leftArm(40, 16, 64, 32, true),
		  rightLeg(0, 16, 64, 32), leftLeg(0, 16, 64, 32, true) {
		head.addBox(-4.0f, -8.0f, -4.0f, 8, 8, 8).setPos(0.0f, 0.0f, 0.0f);
		body.addBox(-4.0f, 0.0f, -2.0f, 8, 12, 4).setPos(0.0f, 0.0f, 0.0f);
		rightArm.addBox(-1.0f, -2.0f, -1.0f, 2, 12, 2).setPos(-5.0f, 2.0f, 0.0f);
		leftArm.addBox(-1.0f, -2.0f, -1.0f, 2, 12, 2).setPos(5.0f, 2.0f, 0.0f);
		rightLeg.addBox(-1.0f, 0.0f, -1.0f, 2, 12, 2).setPos(-2.0f, 12.0f, 0.0f);
		leftLeg.addBox(-1.0f, 0.0f, -1.0f, 2, 12, 2).setPos(2.0f, 12.0f, 0.0f);
	}
};

struct CreeperModelData {
	MobModelPart head, body, leg0, leg1, leg2, leg3;
	CreeperModelData()
		: head(0, 0, 64, 32), body(16, 16, 64, 32),
		  leg0(0, 16, 64, 32), leg1(0, 16, 64, 32), leg2(0, 16, 64, 32), leg3(0, 16, 64, 32) {
		// Feet rest cleanly at model y=24 -> world y=0 (head y=6, body y=6, legs y=18)
		head.addBox(-4.0f, -8.0f, -4.0f, 8, 8, 8).setPos(0.0f, 6.0f, 0.0f);
		body.addBox(-4.0f, 0.0f, -2.0f, 8, 12, 4).setPos(0.0f, 6.0f, 0.0f);
		leg0.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4).setPos(-2.0f, 18.0f, 4.0f);
		leg1.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4).setPos(2.0f, 18.0f, 4.0f);
		leg2.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4).setPos(-2.0f, 18.0f, -4.0f);
		leg3.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4).setPos(2.0f, 18.0f, -4.0f);
	}
};

void getSheepFleeceRGB(uint8_t color, float& r, float& g, float& b) {
	switch (color) {
	case 6:  r = 0.95f; g = 0.65f; b = 0.78f; break; // Pink
	case 7:  r = 0.30f; g = 0.30f; b = 0.30f; break; // Gray
	case 8:  r = 0.62f; g = 0.62f; b = 0.62f; break; // Light Gray
	case 12: r = 0.48f; g = 0.30f; b = 0.18f; break; // Brown
	case 15: r = 0.14f; g = 0.14f; b = 0.14f; break; // Black
	default: r = 1.0f;  g = 1.0f;  b = 1.0f;  break; // White
	}
}

uint8_t pickRandomSheepColor(Random& rng) {
	int r = rng.nextInt(100);
	if (r < 5) return 15; // black
	if (r < 10) return 7; // dark gray
	if (r < 15) return 8; // light gray
	if (r < 18) return 12; // brown
	if (rng.nextInt(500) == 0) return 6; // rare pink
	return 0; // white
}

int getMaxHealthForType(MobType type) {
	switch (type) {
	case MobType::Pig:      return 10;
	case MobType::Cow:      return 10;
	case MobType::Sheep:    return 8;
	case MobType::Chicken:  return 4;
	case MobType::Zombie:   return 20;
	case MobType::Creeper:  return 20;
	case MobType::Skeleton: return 20;
	}
	return 10;
}

const char* getMobTexturePath(MobType type) {
	switch (type) {
	case MobType::Pig:      return "textures/entity/pig/pig.png";
	case MobType::Cow:      return "textures/entity/cow/cow.png";
	case MobType::Sheep:    return "textures/entity/sheep/sheep.tga";
	case MobType::Chicken:  return "textures/entity/chicken.png";
	case MobType::Zombie:   return "textures/entity/zombie/zombie.png";
	case MobType::Creeper:  return "textures/entity/creeper/creeper.png";
	case MobType::Skeleton: return "textures/entity/skeleton/skeleton.png";
	}
	return "textures/entity/pig/pig.png";
}

// ============================================================================
// Physics & Pathfinding Helpers on BlockSource
// ============================================================================

bool checkAABBAnyLiquid(BlockSource& region, const AABB& box, MaterialType matType, bool& outSubmerged, float midY) {
	int x0 = Math::floor(box.min.x);
	int x1 = Math::floor(box.max.x);
	int y0 = Math::floor(box.min.y);
	int y1 = Math::floor(box.max.y);
	int z0 = Math::floor(box.min.z);
	int z1 = Math::floor(box.max.z);
	bool any = false;
	outSubmerged = false;
	int midBy = Math::floor(midY);
	for (int x = x0; x <= x1; ++x) {
		for (int z = z0; z <= z1; ++z) {
			for (int y = y0; y <= y1; ++y) {
				if (region.getMaterial(x, y, z).isType(matType)) {
					any = true;
					if (y >= midBy) {
						outSubmerged = true;
					}
				}
			}
		}
	}
	return any;
}

bool getColumnFloorY(BlockSource& region, int bx, int hintY, int bz, float mobHeight, float& outFloorY, bool& outInWater) {
	outInWater = false;
	for (int dy = 1; dy >= -2; --dy) {
		int ty = hintY + dy;
		if (ty < 1 || ty > 125) continue;
		const Material& feetMat = region.getMaterial(bx, ty, bz);
		if (feetMat.isType(MaterialType::Lava)) continue;

		AABB floorProbe(bx + 0.2f, ty - 0.95f, bz + 0.2f, bx + 0.8f, ty + 0.05f, bz + 0.8f);
		std::vector<AABB>& floorShapes = region.fetchCollisionShapes(region, floorProbe, nullptr, false, nullptr);
		if (floorShapes.empty()) continue;

		float topY = static_cast<float>(ty);
		for (const auto& s : floorShapes) {
			topY = std::max(topY, s.max.y);
		}
		if (topY > static_cast<float>(ty) + 0.6f) continue; // e.g. fence

		AABB headProbe(bx + 0.2f, topY + 0.05f, bz + 0.2f, bx + 0.8f, topY + mobHeight - 0.05f, bz + 0.8f);
		if (!region.fetchCollisionShapes(region, headProbe, nullptr, false, nullptr).empty()) continue;

		if (feetMat.isType(MaterialType::Water) || region.getMaterial(bx, ty + 1, bz).isType(MaterialType::Water)) {
			outInWater = true;
		}
		outFloorY = topY;
		return true;
	}
	return false;
}

bool hasDirectWalkableLine(BlockSource& region, const Vec3& from, const Vec3& to, float mobHeight) {
	float dx = to.x - from.x;
	float dz = to.z - from.z;
	float hDist = std::sqrt(dx * dx + dz * dz);
	if (hDist < 0.4f) return true;
	if (hDist > 12.0f) return false;

	int steps = static_cast<int>(std::ceil(hDist / 0.5f));
	float prevY = from.y;
	for (int s = 1; s <= steps; ++s) {
		float t = static_cast<float>(s) / static_cast<float>(steps);
		float wx = from.x + dx * t;
		float wz = from.z + dz * t;
		int bx = Math::floor(wx);
		int bz = Math::floor(wz);
		float floorY = 0.0f;
		bool inWat = false;
		if (!getColumnFloorY(region, bx, Math::floor(prevY + 0.3f), bz, mobHeight, floorY, inWat)) {
			return false;
		}
		if (inWat || (floorY - prevY) > 1.05f || (prevY - floorY) > 2.2f) {
			return false;
		}
		prevY = floorY;
	}
	return true;
}

std::vector<Vec3> computeGridPath(BlockSource& region, const Vec3& start, const Vec3& goal, float mobHeight) {
	std::vector<Vec3> result;
	int sx = Math::floor(start.x);
	int sy = Math::floor(start.y + 0.25f);
	int sz = Math::floor(start.z);
	int gx = Math::floor(goal.x);
	int gy = Math::floor(goal.y + 0.25f);
	int gz = Math::floor(goal.z);

	if (sx == gx && sz == gz && std::abs(sy - gy) <= 1) return result;
	if (std::abs(gx - sx) > 20 || std::abs(gz - sz) > 20) return result;

	auto packKey = [](int x, int y, int z) -> int64_t {
		return (static_cast<int64_t>(x & 0xFFFFF) << 40) |
		       (static_cast<int64_t>(y & 0xFFF) << 28) |
		       static_cast<int64_t>(z & 0xFFFFF);
	};

	struct ANode {
		int x, y, z;
		float g, f;
		int64_t parent;
	};
	struct Cmp {
		bool operator()(const ANode& a, const ANode& b) const { return a.f > b.f; }
	};

	auto heuristic = [&](int x, int y, int z) -> float {
		float dx = static_cast<float>(gx - x);
		float dy = static_cast<float>(gy - y);
		float dz = static_cast<float>(gz - z);
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	};

	std::priority_queue<ANode, std::vector<ANode>, Cmp> open;
	std::unordered_map<int64_t, ANode> closed;

	int64_t startKey = packKey(sx, sy, sz);
	ANode startNode{ sx, sy, sz, 0.0f, heuristic(sx, sy, sz), startKey };
	open.push(startNode);
	std::unordered_map<int64_t, float> bestG;
	bestG[startKey] = 0.0f;

	static const int dx4[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	static const int dz4[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

	int expansions = 0;
	const int maxExpansions = 220;
	int64_t bestKey = startKey;
	float bestH = heuristic(sx, sy, sz);

	while (!open.empty() && expansions < maxExpansions) {
		ANode cur = open.top();
		open.pop();
		int64_t cKey = packKey(cur.x, cur.y, cur.z);
		if (closed.find(cKey) != closed.end()) continue;
		closed[cKey] = cur;
		++expansions;

		float ch = heuristic(cur.x, cur.y, cur.z);
		if (ch < bestH) {
			bestH = ch;
			bestKey = cKey;
		}
		if (cur.x == gx && cur.z == gz && std::abs(cur.y - gy) <= 1) {
			bestKey = cKey;
			break;
		}

		for (int dir = 0; dir < 8; ++dir) {
			int nx = cur.x + dx4[dir];
			int nz = cur.z + dz4[dir];
			if (std::abs(nx - sx) > 20 || std::abs(nz - sz) > 20) continue;

			float nFloorY = 0.0f;
			bool nWater = false;
			if (!getColumnFloorY(region, nx, cur.y, nz, mobHeight, nFloorY, nWater)) continue;
			int ny = Math::floor(nFloorY + 0.1f);
			if (ny - cur.y > 1 || cur.y - ny > 2) continue;

			// If stepping up, verify current cell has headroom for jump
			if (ny > cur.y) {
				AABB jumpHead(cur.x + 0.2f, cur.y + mobHeight, cur.z + 0.2f,
				              cur.x + 0.8f, cur.y + mobHeight + 1.0f, cur.z + 0.8f);
				if (!region.fetchCollisionShapes(region, jumpHead, nullptr, false, nullptr).empty()) continue;
			}

			// Diagonal corner clearance
			if (dir >= 4) {
				float fy1 = 0.0f, fy2 = 0.0f;
				bool w1 = false, w2 = false;
				if (!getColumnFloorY(region, cur.x + dx4[dir], cur.y, cur.z, mobHeight, fy1, w1)) continue;
				if (!getColumnFloorY(region, cur.x, cur.y, cur.z + dz4[dir], mobHeight, fy2, w2)) continue;
				if (std::abs(fy1 - cur.y) > 0.6f || std::abs(fy2 - cur.y) > 0.6f) continue;
			}

			int64_t nKey = packKey(nx, ny, nz);
			if (closed.find(nKey) != closed.end()) continue;

			float stepCost = (dir < 4 ? 1.0f : 1.414f) + (nWater ? 4.0f : 0.0f) + (ny > cur.y ? 0.4f : 0.0f);
			float ng = cur.g + stepCost;
			auto git = bestG.find(nKey);
			if (git == bestG.end() || ng < git->second) {
				bestG[nKey] = ng;
				open.push(ANode{ nx, ny, nz, ng, ng + heuristic(nx, ny, nz) * 1.15f, cKey });
			}
		}
	}

	if (bestKey == startKey) return result;
	int64_t trace = bestKey;
	while (trace != startKey) {
		auto it = closed.find(trace);
		if (it == closed.end()) break;
		result.push_back(Vec3(it->second.x + 0.5f, static_cast<float>(it->second.y), it->second.z + 0.5f));
		trace = it->second.parent;
	}
	std::reverse(result.begin(), result.end());
	return result;
}

void moveMobWithPhysics(WorldMobEntity& mob, float dt, BlockSource& region) {
	AABB bbOrg = mob.getAABB();
	bool submerged = false;
	AABB waterProbe(bbOrg.min.x + 0.06f, bbOrg.min.y + 0.12f, bbOrg.min.z + 0.06f,
	                bbOrg.max.x - 0.06f, bbOrg.max.y - 0.10f, bbOrg.max.z - 0.06f);
	mob.inWater = checkAABBAnyLiquid(region, waterProbe, MaterialType::Water, submerged,
	                                 bbOrg.min.y + mob.height() * 0.42f);

	float grav = 28.0f;
	float tickScale = dt / 0.05f;
	if (mob.inWater) {
		if (submerged) {
			mob.vel.y += 16.0f * dt;
			if (mob.vel.y > 2.2f) mob.vel.y = 2.2f;
		} else {
			mob.vel.y -= 5.0f * dt;
			if (mob.vel.y < -1.5f) mob.vel.y = -1.5f;
		}
		mob.vel.y *= std::pow(0.82f, tickScale);
	} else {
		mob.vel.y -= grav * dt;
		// MCPE 0.8.1 Chicken.cpp L92-97: soft glide (motionY *= 0.6 per tick when falling)
		if (mob.type == MobType::Chicken && !mob.onGround && mob.vel.y < 0.0f) {
			mob.vel.y *= std::pow(0.60f, tickScale);
		} else if (mob.vel.y < -30.0f) {
			mob.vel.y = -30.0f;
		}
	}

	float dragBase = mob.inWater ? 0.80f : (mob.onGround ? 0.546f : 0.91f);
	float drag = std::pow(dragBase, tickScale);

	// Exact MCPE 0.8.1 Mob::travel order: apply moveRelative acceleration BEFORE Entity::move, then multiply by friction
	if (mob.desiredSpeed > 0.0f && !mob.isDying) {
		float moveGain = mob.onGround ? (1.0f - drag) : (mob.inWater ? (1.0f - drag) * 0.75f : (1.0f - drag) * 0.35f);
		float spdScale = mob.inWater ? 0.65f : 1.0f;
		mob.vel.x += (mob.moveDirX * mob.desiredSpeed * spdScale) * moveGain;
		mob.vel.z += (mob.moveDirZ * mob.desiredSpeed * spdScale) * moveGain;
	}

	bool wasOnGround = mob.onGround;
	Vec3 step = mob.vel * dt;
	AABB bb = bbOrg;
	std::vector<AABB>& shapes = region.fetchCollisionShapes(region, bb.expanded(step), nullptr, false, nullptr);

	float xaOrg = step.x;
	float yaOrg = step.y;
	float zaOrg = step.z;

	Vec3 axisSteps[3] = {
		Vec3(0.0f, step.y, 0.0f),
		Vec3(step.x, 0.0f, 0.0f),
		Vec3(0.0f, 0.0f, step.z)
	};
	for (int axis = 0; axis < 3; ++axis) {
		for (const auto& s : shapes) {
			axisSteps[axis] = s.clipCollide(bb, axisSteps[axis], false);
		}
		bb.move(axisSteps[axis]);
	}

	bool hitY = (std::abs(axisSteps[0].y - yaOrg) > 1e-5f);
	bool onGroundNow = hitY && (yaOrg < 0.0f);
	bool hitHoriz = (std::abs(axisSteps[1].x - xaOrg) > 1e-5f || std::abs(axisSteps[2].z - zaOrg) > 1e-5f);

	// Smooth 0.55-block auto-step (slabs, stairs, snow layers) matching MCPE stepHeight=0.5
	if (hitHoriz && (wasOnGround || onGroundNow)) {
		const float maxStepUp = 0.55f;
		AABB bbStep = bbOrg;
		Vec3 orgExpand(xaOrg, maxStepUp, zaOrg);
		std::vector<AABB>& stepShapes = region.fetchCollisionShapes(region, bbStep.expanded(orgExpand), nullptr, false, nullptr);

		Vec3 sY(0.0f, maxStepUp, 0.0f);
		Vec3 sX(xaOrg, 0.0f, 0.0f);
		Vec3 sZ(0.0f, 0.0f, zaOrg);
		for (const auto& s : stepShapes) sY = s.clipCollide(bbStep, sY, false);
		bbStep.move(sY);
		for (const auto& s : stepShapes) sX = s.clipCollide(bbStep, sX, false);
		bbStep.move(sX);
		for (const auto& s : stepShapes) sZ = s.clipCollide(bbStep, sZ, false);
		bbStep.move(sZ);

		Vec3 revY(0.0f, -sY.y, 0.0f);
		for (const auto& s : stepShapes) revY = s.clipCollide(bbStep, revY, false);
		bbStep.move(revY);

		bool headClear = region.fetchCollisionShapes(region, bbStep, nullptr, false, nullptr).empty();
		float origHorizSq = axisSteps[1].x * axisSteps[1].x + axisSteps[2].z * axisSteps[2].z;
		float stepHorizSq = sX.x * sX.x + sZ.z * sZ.z;
		if (headClear && stepHorizSq > origHorizSq + 1e-6f) {
			bb = bbStep;
			axisSteps[1].x = sX.x;
			axisSteps[2].z = sZ.z;
			onGroundNow = true;
			hitHoriz = (std::abs(sX.x - xaOrg) > 1e-5f || std::abs(sZ.z - zaOrg) > 1e-5f);
		}
	}

	float prevX = mob.pos.x;
	float prevZ = mob.pos.z;
	mob.pos.x = (bb.min.x + bb.max.x) * 0.5f;
	mob.pos.y = bb.min.y;
	mob.pos.z = (bb.min.z + bb.max.z) * 0.5f;

	if (std::abs(axisSteps[1].x - xaOrg) > 1e-5f) mob.vel.x = 0.0f;
	if (std::abs(axisSteps[2].z - zaOrg) > 1e-5f) mob.vel.z = 0.0f;
	if (hitY) {
		mob.onGround = (yaOrg < 0.0f);
		mob.vel.y = 0.0f;
	} else {
		mob.onGround = onGroundNow;
	}

	// MCPE 0.8.1 jump rules:
	// - In water + collided horizontally: hop out onto shore if +0.6 blocks above is free (Mob.cpp L981-985)
	// - With an active path: only jump when MoveControl/PathfinderMob requests it (waypoint higher & within 1 block)
	// - Without an active path: jump on horizontal collision if 1-block ledge has clearance (PathfinderMob.cpp L170)
	// - Sheep eating grass or dying mobs cannot jump (Sheep.cpp L202-206, Mob.cpp L1100)
	bool blockedByState = mob.isDying || (mob.type == MobType::Sheep && mob.eatTimer > 0.0f);
	if (!blockedByState) {
		if (mob.inWater && hitHoriz && (std::abs(mob.moveDirX) > 0.05f || std::abs(mob.moveDirZ) > 0.05f)) {
			AABB shoreCheck = bb.translated(Vec3(0.0f, 0.60f, 0.0f));
			if (region.fetchCollisionShapes(region, shoreCheck, nullptr, false, nullptr).empty()) {
				mob.vel.y = std::max(mob.vel.y, 6.0f);
			}
		} else if (mob.onGround) {
			if (mob.hasActivePath) {
				if (mob.wantsJump) {
					mob.vel.y = 8.4f;
					mob.onGround = false;
				}
			} else if (hitHoriz && (std::abs(mob.moveDirX) > 0.05f || std::abs(mob.moveDirZ) > 0.05f)) {
				float dLen = std::sqrt(mob.moveDirX * mob.moveDirX + mob.moveDirZ * mob.moveDirZ);
				float nx = (dLen > 0.001f) ? (mob.moveDirX / dLen) : 0.0f;
				float nz = (dLen > 0.001f) ? (mob.moveDirZ / dLen) : 0.0f;
				AABB jumpCheck = bb.translated(Vec3(nx * 0.35f, 1.05f, nz * 0.35f));
				if (region.fetchCollisionShapes(region, jumpCheck, nullptr, false, nullptr).empty()) {
					mob.vel.y = 8.4f;
					mob.onGround = false;
				}
			}
		}
	}

	// Exact MCPE 0.8.1 Mob::updateWalkAnim() (Mob.cpp L1047-1089)
	float dx = mob.pos.x - prevX;
	float dz = mob.pos.z - prevZ;
	float dist = std::sqrt(dx * dx + dz * dz);
	mob.walkDist += dist;
	float v2 = (mob.isBurning || mob.hurtTimer > 0.0f) ? 1.5f : 1.0f;
	if (!mob.onGround && mob.vel.y > 0.0f) {
		v2 *= 0.35f;
	}
	float distPerTick = (tickScale > 0.001f) ? (dist / tickScale) : 0.0f;
	float v9 = std::min(1.0f, distPerTick * 4.0f);
	float animBlend = 1.0f - std::pow(0.60f, tickScale);
	mob.walkSpeedVis += (v9 * v2 - mob.walkSpeedVis) * animBlend;
	mob.walkAnimPos += mob.walkSpeedVis * tickScale;

	mob.vel.x *= drag;
	mob.vel.z *= drag;
}

} // namespace

bool WorldMobEntity::isHostile() const {
	return type == MobType::Zombie || type == MobType::Creeper || type == MobType::Skeleton;
}

float WorldMobEntity::halfWidth() const {
	float s = isBaby ? 0.5f : 1.0f;
	switch (type) {
	case MobType::Pig:      return 0.45f * s;
	case MobType::Cow:      return 0.45f * s;
	case MobType::Sheep:    return 0.45f * s;
	case MobType::Chicken:  return 0.25f * s;
	case MobType::Zombie:   return 0.30f;
	case MobType::Creeper:  return 0.30f;
	case MobType::Skeleton: return 0.30f;
	}
	return 0.35f * s;
}

float WorldMobEntity::height() const {
	float s = isBaby ? 0.5f : 1.0f;
	switch (type) {
	case MobType::Pig:      return 0.90f * s;
	case MobType::Cow:      return 1.30f * s;
	case MobType::Sheep:    return 1.30f * s;
	case MobType::Chicken:  return 0.70f * s;
	case MobType::Zombie:   return 1.80f;
	case MobType::Creeper:  return 1.65f;
	case MobType::Skeleton: return 1.80f;
	}
	return 1.0f * s;
}

AABB WorldMobEntity::getAABB() const {
	float hw = halfWidth();
	return AABB(pos.x - hw, pos.y, pos.z - hw, pos.x + hw, pos.y + height(), pos.z + hw);
}

void MobManager::clear() {
	mobs.clear();
	arrows.clear();
	mNextId = 1;
	mSpawnTimer = 1.0f;
}

bool MobManager::isAnimalFood(MobType type, const ItemInstance& item) {
	if (item.isNull() || item.getStackSize() <= 0 || !item.getItem()) return false;
	const Item* it = item.getItem();
	switch (type) {
	case MobType::Cow:
	case MobType::Sheep:
		return (Item::mWheat && it == Item::mWheat);
	case MobType::Pig:
		return (Item::mPotato && it == Item::mPotato) ||
		       (Item::mCarrot && it == Item::mCarrot) ||
		       (Item::mBeetroot && it == Item::mBeetroot);
	case MobType::Chicken:
		return (Item::mSeeds_wheat && it == Item::mSeeds_wheat) ||
		       (Item::mSeeds_melon && it == Item::mSeeds_melon) ||
		       (Item::mSeeds_pumpkin && it == Item::mSeeds_pumpkin) ||
		       (Item::mSeeds_beetroot && it == Item::mSeeds_beetroot);
	default:
		return false;
	}
}

void MobManager::spawnDropsForMob(const WorldMobEntity& mob,
                                  const std::function<void(const ItemInstance&, const Vec3&)>& onDropItem) const {
	if (!onDropItem || mob.isBaby) return; // MCPE Mob::die: babies don't drop loot
	Random& rng = mobRng();
	Vec3 dropPos(mob.pos.x, mob.pos.y + mob.height() * 0.5f, mob.pos.z);

	auto drop = [&](const Item* it, int count) {
		if (it && count > 0) {
			onDropItem(ItemInstance(it, count, 0), dropPos);
		}
	};

	switch (mob.type) {
	case MobType::Pig:
		drop(Item::mPorkChop_raw, 1 + rng.nextInt(3));
		break;
	case MobType::Cow:
		drop(Item::mBeef_raw, 1 + rng.nextInt(3));
		drop(Item::mLeather, rng.nextInt(3));
		break;
	case MobType::Sheep:
		if (!mob.isSheared && Block::mWool) {
			onDropItem(ItemInstance(Block::mWool, 1, mob.woolColor), dropPos);
		}
		drop(Item::mMutton_raw, 1 + rng.nextInt(2));
		break;
	case MobType::Chicken:
		drop(Item::mChicken_raw, 1);
		drop(Item::mFeather, rng.nextInt(3));
		break;
	case MobType::Zombie:
		drop(Item::mRotten_flesh, rng.nextInt(3));
		break;
	case MobType::Creeper:
		drop(Item::mSulphur, rng.nextInt(3));
		break;
	case MobType::Skeleton:
		drop(Item::mBone, rng.nextInt(3));
		drop(Item::mArrow, rng.nextInt(3));
		break;
	}
}

bool MobManager::damageMob(int index,
                           int damage,
                           const Vec3& knockDir,
                           const std::function<void(const ItemInstance&, const Vec3&)>& onDropItem) {
	if (index < 0 || index >= static_cast<int>(mobs.size())) return false;
	WorldMobEntity& mob = mobs[static_cast<size_t>(index)];
	// MCPE 0.8.1 Mob::hurt: 10 ticks (0.50s) invulnerability window, ignore if already dying
	if (mob.isDying || mob.hurtTimer > 0.0f) return false;

	mob.health = std::max(0, mob.health - std::max(1, damage));
	mob.hurtTimer = 0.50f;    // hurtTime = 10 ticks
	mob.walkSpeedVis = 1.5f;  // field_16C = 1.5 (leg-kick burst on hit)

	// Exact MCPE 0.8.1 Mob::knockback (Mob.cpp L792-814): halve existing vel, add 0.4 blocks/tick (8.0 m/s)
	float len = std::sqrt(knockDir.x * knockDir.x + knockDir.z * knockDir.z);
	if (len > 0.001f) {
		mob.vel.x = mob.vel.x * 0.5f + (knockDir.x / len) * 8.0f;
		mob.vel.z = mob.vel.z * 0.5f + (knockDir.z / len) * 8.0f;
	}
	mob.vel.y = std::min(8.0f, mob.vel.y * 0.5f + 8.0f);
	mob.onGround = false;

	if (!mob.isHostile()) {
		// Animal::hurt (Animal.cpp L81-86): inPanicTicksMaybe = 60 (3.0s), reset love
		mob.panicTimer = 3.0f;
		mob.inLoveTimer = 0.0f;
		mob.breedProgress = 0.0f;
		mob.eatTimer = 0.0f;
		mob.aiTimer = 0.0f;
		if (len > 0.001f) {
			mob.moveDirX = knockDir.x / len;
			mob.moveDirZ = knockDir.z / len;
		}
	}

	if (mob.health <= 0) {
		// MCPE Mob::die(): drop loot immediately on fatal hit, then play 20-tick (1.0s) fall-over animation
		mob.isDying = true;
		mob.deathTimer = 0.0f;
		mob.desiredSpeed = 0.0f;
		spawnDropsForMob(mob, onDropItem);
	}
	return true;
}

bool MobManager::shearSheep(int index,
                            const std::function<void(const ItemInstance& item, const Vec3& pos)>& onDropItem) {
	if (index < 0 || index >= static_cast<int>(mobs.size())) return false;
	WorldMobEntity& mob = mobs[static_cast<size_t>(index)];
	if (mob.isDying || mob.type != MobType::Sheep || mob.isSheared || mob.isBaby) return false;

	mob.isSheared = true;
	if (onDropItem && Block::mWool) {
		int count = 1 + mobRng().nextInt(3);
		Vec3 dropPos(mob.pos.x, mob.pos.y + 1.0f, mob.pos.z);
		for (int k = 0; k < count; ++k) {
			onDropItem(ItemInstance(Block::mWool, 1, mob.woolColor), dropPos);
		}
	}
	return true;
}

bool MobManager::feedAnimal(int index,
                            const ItemInstance& held,
                            const std::function<void(const Vec3& pos, float hw, float h, int count)>& onHearts) {
	if (index < 0 || index >= static_cast<int>(mobs.size())) return false;
	WorldMobEntity& mob = mobs[static_cast<size_t>(index)];
	if (mob.isDying || mob.isHostile() || mob.isBaby || mob.inLoveTimer > 0.0f || mob.breedCooldown > 0.0f) return false;
	if (!isAnimalFood(mob.type, held)) return false;

	// MCPE 0.8.1 Animal::interactWithPlayer (Animal.cpp L58-77): inLove = 600 ticks (30s), 7 PT_HEART particles
	mob.inLoveTimer = 30.0f;
	mob.heartParticleTimer = 0.8f;
	mob.breedProgress = 0.0f;
	if (onHearts) {
		onHearts(mob.pos, mob.halfWidth(), mob.height(), 7);
	}
	return true;
}

bool MobManager::milkCow(int index) {
	if (index < 0 || index >= static_cast<int>(mobs.size())) return false;
	WorldMobEntity& mob = mobs[static_cast<size_t>(index)];
	if (mob.isDying || mob.type != MobType::Cow || mob.isBaby || mob.milkCooldown > 0.0f) return false;
	mob.milkCooldown = 1.0f; // milkedTicks > 20 in Cow.cpp L45
	return true;
}

int MobManager::raycastMob(const Vec3& rayOrigin, const Vec3& rayDir, float maxDist, float& outDist) const {
	int bestIdx = -1;
	float bestT = maxDist;
	for (size_t i = 0; i < mobs.size(); ++i) {
		if (mobs[i].isDying) continue;
		AABB hitBox = mobs[i].getAABB().expanded(Vec3(0.12f, 0.08f, 0.12f));
		float t = rayIntersectsAABB(rayOrigin, rayDir, hitBox, bestT);
		if (t >= 0.0f && t < bestT) {
			bestT = t;
			bestIdx = static_cast<int>(i);
		}
	}
	if (bestIdx >= 0) {
		outDist = bestT;
	}
	return bestIdx;
}

void MobManager::trySpawnMobs(BlockSource& region, const Vec3& playerPos, float sunFactor) {
	int passiveCount = 0;
	int hostileCount = 0;
	for (const auto& m : mobs) {
		if (m.isHostile()) ++hostileCount;
		else ++passiveCount;
	}

	const int maxPassive = 12;
	const int maxHostile = 10;
	Random& rng = mobRng();

	auto initMob = [&](MobType chosen, float wx, float wy, float wz) -> WorldMobEntity {
		WorldMobEntity m;
		m.id = mNextId++;
		m.type = chosen;
		m.pos = Vec3(wx, wy, wz);
		m.lastStuckPos = m.pos;
		m.yaw = wrapDegrees(rng.nextFloat() * 360.0f - 180.0f);
		m.headYaw = m.yaw;
		m.health = m.maxHealth = getMaxHealthForType(chosen);
		if (chosen == MobType::Sheep) {
			m.woolColor = pickRandomSheepColor(rng);
		}
		return m;
	};

	auto trySpawnOne = [&](bool wantHostile) {
		float angle = rng.nextFloat() * (2.0f * kPi);
		float dist = 14.0f + rng.nextFloat() * 22.0f;
		int wx = Math::floor(playerPos.x + std::cos(angle) * dist);
		int wz = Math::floor(playerPos.z + std::sin(angle) * dist);
		if (!region.hasChunksAt(BlockPos(wx, 64, wz), 1)) return;

		int topY = static_cast<int>(region.getAboveTopSolidBlock(wx, wz, true, false));
		if (topY < 2 || topY >= 124) return;

		BlockPos spawnPos(wx, topY, wz);
		BlockPos groundPos(wx, topY - 1, wz);
		BlockID groundId = region.getBlockID(groundPos);
		BlockID feetId = region.getBlockID(spawnPos);
		BlockID headId = region.getBlockID(spawnPos.above());
		if (groundId == 0 || feetId != 0 || headId != 0) return;

		const Block* groundBlk = Block::mBlocks[groundId];
		if (!groundBlk || groundBlk->getMaterial().isLiquid() || !groundBlk->isSolid()) return;

		if (wantHostile) {
			BrightnessPair lp = region.getLightColor(spawnPos);
			float skyEff = (static_cast<float>(lp.sky) / 15.0f) * sunFactor;
			if (skyEff > 0.45f || lp.block > 7) return;

			static const MobType hostiles[] = { MobType::Zombie, MobType::Creeper, MobType::Skeleton };
			MobType chosen = hostiles[rng.nextInt(3)];
			WorldMobEntity m = initMob(chosen, wx + 0.5f, static_cast<float>(topY), wz + 0.5f);
			if (region.fetchCollisionShapes(region, m.getAABB(), nullptr, false, nullptr).empty()) {
				mobs.push_back(std::move(m));
			}
		} else {
			if (sunFactor < 0.45f) return;
			if (groundBlk != Block::mGrass && groundBlk != Block::mDirt && groundBlk != Block::mSand) return;

			static const MobType passives[] = { MobType::Pig, MobType::Cow, MobType::Sheep, MobType::Chicken };
			MobType chosen = passives[rng.nextInt(4)];
			int pack = 1 + rng.nextInt(2);
			for (int k = 0; k < pack && passiveCount < maxPassive; ++k) {
				int px = wx + (k == 0 ? 0 : (rng.nextInt(5) - 2));
				int pz = wz + (k == 0 ? 0 : (rng.nextInt(5) - 2));
				int py = static_cast<int>(region.getAboveTopSolidBlock(px, pz, true, false));
				if (py < 2 || py >= 124) continue;
				if (region.getBlockID(px, py, pz) != 0 || region.getBlockID(px, py + 1, pz) != 0) continue;
				if (region.getMaterial(px, py - 1, pz).isLiquid()) continue;
				WorldMobEntity m = initMob(chosen, px + 0.5f, static_cast<float>(py), pz + 0.5f);
				if (region.fetchCollisionShapes(region, m.getAABB(), nullptr, false, nullptr).empty()) {
					mobs.push_back(std::move(m));
					++passiveCount;
				}
			}
		}
	};

	if (passiveCount < maxPassive) {
		trySpawnOne(false);
	}
	if (!peaceful && hostileCount < maxHostile && sunFactor < 0.45f) {
		trySpawnOne(true);
	}
}

void MobManager::update(float dt,
                        BlockSource& region,
                        const Vec3& playerEyePos,
                        const AABB& playerBB,
                        float sunFactor,
                        bool isCreative,
                        bool allowCombat,
                        const std::function<void(int, const Vec3&)>& onDamagePlayer,
                        const std::function<void(const Vec3&, float)>& onExplosion,
                        const std::function<void(const ItemInstance&, const Vec3&)>& onDropItem,
                        const ItemInstance* playerHeld,
                        const std::function<void(const Vec3& pos, float hw, float h)>& onDeathPoof,
                        const std::function<void(const Vec3& pos, float hw, float h, int count)>& onHearts) {
	dt = std::min(std::max(dt, 0.0001f), 0.05f);
	Random& rng = mobRng();

	if (peaceful) {
		mobs.erase(std::remove_if(mobs.begin(), mobs.end(), [](const WorldMobEntity& e) { return e.isHostile(); }), mobs.end());
	}

	mSpawnTimer -= dt;
	if (mSpawnTimer <= 0.0f) {
		mSpawnTimer = 1.6f;
		trySpawnMobs(region, playerEyePos, sunFactor);
	}

	Vec3 playerFeet((playerBB.min.x + playerBB.max.x) * 0.5f,
	                playerBB.min.y,
	                (playerBB.min.z + playerBB.max.z) * 0.5f);

	std::vector<WorldMobEntity> newbornBabies;

	for (size_t i = 0; i < mobs.size(); ) {
		WorldMobEntity& m = mobs[i];

		float dxP = playerFeet.x - m.pos.x;
		float dzP = playerFeet.z - m.pos.z;
		float distSq = dxP * dxP + dzP * dzP;
		if (distSq > 62.0f * 62.0f || m.pos.y < -32.0f) {
			mobs[i] = mobs.back();
			mobs.pop_back();
			continue;
		}

		// Pause mob simulation if standing inside an unloaded/generating chunk so mobs don't fall through unloaded ground
		LevelChunk* mobLc = region.getChunk(ChunkPos(Math::floor(m.pos.x) >> 4, Math::floor(m.pos.z) >> 4));
		if (!mobLc || mobLc->getState() != ChunkState::Loaded) {
			++i;
			continue;
		}

		m.age += dt;
		if (m.hurtTimer > 0.0f) m.hurtTimer = std::max(0.0f, m.hurtTimer - dt);
		if (m.attackCooldown > 0.0f) m.attackCooldown = std::max(0.0f, m.attackCooldown - dt);
		if (m.panicTimer > 0.0f) m.panicTimer = std::max(0.0f, m.panicTimer - dt);
		if (m.attackSwing > 0.0f) m.attackSwing = std::max(0.0f, m.attackSwing - dt * 2.8f);
		if (m.pathTimer > 0.0f) m.pathTimer = std::max(0.0f, m.pathTimer - dt);
		if (m.breedCooldown > 0.0f) m.breedCooldown = std::max(0.0f, m.breedCooldown - dt);
		if (m.milkCooldown > 0.0f) m.milkCooldown = std::max(0.0f, m.milkCooldown - dt);
		if (m.isBaby && m.babyTimer > 0.0f) {
			m.babyTimer = std::max(0.0f, m.babyTimer - dt);
			if (m.babyTimer <= 0.0f) m.isBaby = false;
		}
		if (m.inLoveTimer > 0.0f) {
			m.inLoveTimer = std::max(0.0f, m.inLoveTimer - dt);
			m.heartParticleTimer -= dt;
			if (m.heartParticleTimer <= 0.0f) {
				m.heartParticleTimer = 0.8f; // 16 ticks in Animal::aiStep
				if (onHearts) onHearts(m.pos, m.halfWidth(), m.height(), 1);
			}
		}
		if (m.eatTimer > 0.0f) {
			m.eatTimer = std::max(0.0f, m.eatTimer - dt);
			if (m.eatTimer <= 0.0f && m.type == MobType::Sheep) {
				m.isSheared = false; // Sheep::ate() regrows wool
			}
		}

		// MCPE 0.8.1 Mob::baseTick() L527-555: 20-tick (1.0s) death fall-over animation, then 20 PT_EXPLODE particles
		if (m.isDying) {
			m.deathTimer += dt;
			m.desiredSpeed = 0.0f;
			m.wantsJump = false;
			moveMobWithPhysics(m, dt, region);
			if (m.deathTimer >= 1.0f) {
				if (onDeathPoof) {
					onDeathPoof(m.pos, m.halfWidth(), m.height());
				}
				mobs[i] = mobs.back();
				mobs.pop_back();
				continue;
			}
			++i;
			continue;
		}

		// Sunlight burn for Zombie & Skeleton
		m.isBurning = false;
		if ((m.type == MobType::Zombie || m.type == MobType::Skeleton) && sunFactor > 0.68f && !m.inWater) {
			BlockPos headPos(Math::floor(m.pos.x), Math::floor(m.pos.y + 1.5f), Math::floor(m.pos.z));
			if (region.canSeeSky(headPos)) {
				m.isBurning = true;
				m.burnTimer += dt;
				if (m.burnTimer >= 1.0f) {
					m.burnTimer -= 1.0f;
					m.health -= 2;
					m.hurtTimer = 0.50f;
					m.walkSpeedVis = 1.5f;
					if (m.health <= 0) {
						m.isDying = true;
						m.deathTimer = 0.0f;
						m.desiredSpeed = 0.0f;
						spawnDropsForMob(m, onDropItem);
						++i;
						continue;
					}
				}
			}
		}

		// Soft mob-to-mob & mob-to-player separation push (MCPE Mob::pushEntities L184-214)
		for (size_t j = 0; j < mobs.size(); ++j) {
			if (i == j || mobs[j].isDying) continue;
			const WorldMobEntity& other = mobs[j];
			if (std::abs(other.pos.y - m.pos.y) > 1.2f) continue;
			float sx = m.pos.x - other.pos.x;
			float sz = m.pos.z - other.pos.z;
			float minDist = m.halfWidth() + other.halfWidth();
			float d2 = sx * sx + sz * sz;
			if (d2 < minDist * minDist && d2 > 0.0001f) {
				float d = std::sqrt(d2);
				float push = (minDist - d) * 6.0f * dt;
				m.vel.x += (sx / d) * push;
				m.vel.z += (sz / d) * push;
			}
		}
		if (std::abs(playerFeet.y - m.pos.y) < 1.5f) {
			float minPDist = m.halfWidth() + 0.30f;
			if (distSq < minPDist * minPDist && distSq > 0.0001f) {
				float d = std::sqrt(distSq);
				float push = (minPDist - d) * 5.0f * dt;
				m.vel.x -= (dxP / d) * push;
				m.vel.z -= (dzP / d) * push;
			}
		}

		// AI (MCPE 0.8.1 Monster / Animal / PathfinderMob rules)
		float dist = std::sqrt(distSq);
		float dyP = playerFeet.y - m.pos.y;
		float desiredSpeed = 0.0f;
		float targetBodyYaw = m.yaw;
		float targetHeadYaw = m.yaw;
		float targetHeadPitch = 0.0f;
		m.wantsJump = false;
		m.hasActivePath = false;

		Vec3 eyeFrom(m.pos.x, m.pos.y + m.height() * 0.85f, m.pos.z);
		bool canSeePlayer = false;
		if (m.isHostile() && !isCreative && allowCombat && dist < 32.0f) {
			canSeePlayer = !region.clip(eyeFrom, playerEyePos, false, true, 36, false).isHit();
			if (!m.hasTarget) {
				if (dist <= 16.0f && canSeePlayer) {
					m.hasTarget = true;
				}
			} else {
				if (dist > 32.0f && !canSeePlayer) {
					m.hasTarget = false;
				}
			}
		} else {
			m.hasTarget = false;
		}

		if (m.hasTarget) {
			float invD = (dist > 0.001f) ? (1.0f / dist) : 0.0f;
			float dirX = dxP * invD;
			float dirZ = dzP * invD;
			float playerYaw = dirToMcpeYaw(dirX, dirZ);
			targetHeadYaw = playerYaw;
			targetHeadPitch = std::clamp(std::atan2(-(playerEyePos.y - eyeFrom.y), std::max(0.5f, dist)) * kRadToDeg, -38.0f, 38.0f);

			auto followPathToPlayer = [&](float& outDirX, float& outDirZ) {
				if (hasDirectWalkableLine(region, m.pos, playerFeet, m.height())) {
					m.pathWaypoints.clear();
					m.pathIndex = 0;
					m.hasActivePath = false;
					outDirX = dirX;
					outDirZ = dirZ;
					return;
				}
				if (m.pathTimer <= 0.0f || m.pathIndex >= m.pathWaypoints.size()) {
					m.pathWaypoints = computeGridPath(region, m.pos, playerFeet, m.height());
					m.pathIndex = 0;
					m.pathTimer = 0.95f;
				}
				while (m.pathIndex < m.pathWaypoints.size()) {
					const Vec3& wp = m.pathWaypoints[m.pathIndex];
					float wx = wp.x - m.pos.x;
					float wz = wp.z - m.pos.z;
					if (wx * wx + wz * wz < 0.45f * 0.45f && std::abs(wp.y - m.pos.y) < 1.2f) {
						++m.pathIndex;
					} else {
						break;
					}
				}
				if (m.pathIndex < m.pathWaypoints.size()) {
					const Vec3& wp = m.pathWaypoints[m.pathIndex];
					float wx = wp.x - m.pos.x;
					float wz = wp.z - m.pos.z;
					float wLenSq = wx * wx + wz * wz;
					float wLen = std::sqrt(wLenSq);
					m.hasActivePath = true;
					if ((wp.y - m.pos.y) > 0.35f && wLenSq < 1.0f) {
						m.wantsJump = true;
					}
					if (wLen > 0.001f) {
						outDirX = wx / wLen;
						outDirZ = wz / wLen;
						return;
					}
				}
				outDirX = dirX;
				outDirZ = dirZ;
			};

			if (m.type == MobType::Zombie) {
				followPathToPlayer(m.moveDirX, m.moveDirZ);
				if (dist > 0.85f) {
					desiredSpeed = 2.30f;
					targetBodyYaw = dirToMcpeYaw(m.moveDirX, m.moveDirZ);
				} else {
					desiredSpeed = 0.0f;
					targetBodyYaw = playerYaw;
				}
				bool yOverlap = (playerBB.max.y > m.pos.y) && (playerBB.min.y < m.pos.y + m.height());
				if (dist < 2.0f && yOverlap && m.attackCooldown <= 0.0f) {
					m.attackCooldown = 1.0f;
					m.attackSwing = 1.0f;
					if (onDamagePlayer) {
						onDamagePlayer(4, Vec3(dirX, 0.0f, dirZ));
					}
				}
			} else if (m.type == MobType::Creeper) {
				bool shouldSwell = canSeePlayer && ((m.fuseTimer <= 0.0f && dist < 3.0f) || (m.fuseTimer > 0.0f && dist <= 7.0f));
				if (shouldSwell && std::abs(dyP) < 2.5f) {
					m.moveDirX = 0.0f;
					m.moveDirZ = 0.0f;
					desiredSpeed = 0.0f;
					targetBodyYaw = playerYaw;
					m.fuseTimer += dt;
					if (m.fuseTimer >= 1.5f) {
						Vec3 expPos(m.pos.x, m.pos.y + 0.7f, m.pos.z);
						float fullDist = std::sqrt(distSq + dyP * dyP);
						if (onDamagePlayer && fullDist < 4.5f) {
							int expDmg = std::max(1, static_cast<int>((1.0f - fullDist / 4.5f) * 18.0f));
							onDamagePlayer(expDmg, Vec3(dirX * 2.2f, 0.0f, dirZ * 2.2f));
						}
						if (onExplosion) {
							onExplosion(expPos, 2.4f);
						}
						mobs[i] = mobs.back();
						mobs.pop_back();
						continue;
					}
				} else {
					if (m.fuseTimer > 0.0f) {
						m.fuseTimer = std::max(0.0f, m.fuseTimer - dt);
					}
					followPathToPlayer(m.moveDirX, m.moveDirZ);
					desiredSpeed = (dist > 0.9f) ? 2.50f : 0.0f;
					targetBodyYaw = dirToMcpeYaw(m.moveDirX, m.moveDirZ);
				}
			} else if (m.type == MobType::Skeleton) {
				Vec3 aimVec = playerEyePos - eyeFrom;
				float aimDist = aimVec.length();
				bool inRangeLOS = canSeePlayer && dist < 10.0f && aimDist > 0.1f;

				if (!inRangeLOS) {
					followPathToPlayer(m.moveDirX, m.moveDirZ);
					desiredSpeed = 2.50f;
					targetBodyYaw = dirToMcpeYaw(m.moveDirX, m.moveDirZ);
				} else {
					m.moveDirX = 0.0f;
					m.moveDirZ = 0.0f;
					desiredSpeed = 0.0f;
					targetBodyYaw = playerYaw;

					if (m.attackCooldown <= 0.0f) {
						WorldArrowEntity arrow;
						arrow.pos = eyeFrom + (aimVec / aimDist) * 0.45f;
						Vec3 dirNorm = aimVec / aimDist;
						float arrowSpeed = 22.0f;
						float spreadX = (rng.nextFloat() - 0.5f) * 0.06f;
						float spreadZ = (rng.nextFloat() - 0.5f) * 0.06f;
						arrow.vel = Vec3((dirNorm.x + spreadX) * arrowSpeed,
						                 dirNorm.y * arrowSpeed + aimDist * 0.20f,
						                 (dirNorm.z + spreadZ) * arrowSpeed);
						float hVel = std::sqrt(arrow.vel.x * arrow.vel.x + arrow.vel.z * arrow.vel.z);
						arrow.yaw = std::atan2(arrow.vel.x, arrow.vel.z) * kRadToDeg;
						arrow.pitch = std::atan2(arrow.vel.y, hVel) * kRadToDeg;
						arrow.damage = 3;
						arrows.push_back(arrow);
						m.attackCooldown = 3.0f;
					}
				}
			}
		} else {
			if (m.type == MobType::Creeper && m.fuseTimer > 0.0f) {
				m.fuseTimer = std::max(0.0f, m.fuseTimer - dt);
			}
			m.pathWaypoints.clear();

			// Exact MCPE 0.8.1 baseSpeed & steady-state speed formula: v = 30.22 * (baseSpeed * mult)^2
			float mcpeBaseSpeed = (m.type == MobType::Cow) ? 0.20f : (m.type == MobType::Zombie ? 0.23f : 0.25f);
			auto calcMcpeSpeed = [&](float mult) -> float {
				if (m.isHostile()) return 30.22f * mcpeBaseSpeed * mcpeBaseSpeed * mult;
				float s = mcpeBaseSpeed * mult;
				return 30.22f * s * s;
			};

			// Check BreedGoal (inLove > 0, find partner within 8.0 blocks)
			int breedPartnerIdx = -1;
			if (!m.isHostile() && !m.isBaby && m.inLoveTimer > 0.0f && m.panicTimer <= 0.0f) {
				float bestPD2 = 8.0f * 8.0f;
				for (size_t j = 0; j < mobs.size(); ++j) {
					if (i == j || mobs[j].isDying || mobs[j].type != m.type || mobs[j].isBaby || mobs[j].inLoveTimer <= 0.0f) continue;
					float bx = mobs[j].pos.x - m.pos.x;
					float bz = mobs[j].pos.z - m.pos.z;
					float d2 = bx * bx + bz * bz;
					if (d2 < bestPD2) {
						bestPD2 = d2;
						breedPartnerIdx = static_cast<int>(j);
					}
				}
			}

			// Check TemptGoal (player within 10.0 blocks holding favorite food)
			bool temptedByPlayer = (!m.isHostile() && m.panicTimer <= 0.0f && breedPartnerIdx < 0 &&
			                        dist < 10.0f && std::abs(dyP) < 4.0f &&
			                        playerHeld && isAnimalFood(m.type, *playerHeld));

			// Check FollowParentGoal (baby follows adult within 3..16 blocks at 1.1x)
			int parentIdx = -1;
			if (!m.isHostile() && m.isBaby && m.panicTimer <= 0.0f && !temptedByPlayer) {
				float bestAD2 = 16.0f * 16.0f;
				for (size_t j = 0; j < mobs.size(); ++j) {
					if (i == j || mobs[j].isDying || mobs[j].type != m.type || mobs[j].isBaby) continue;
					float bx = mobs[j].pos.x - m.pos.x;
					float bz = mobs[j].pos.z - m.pos.z;
					float d2 = bx * bx + bz * bz;
					if (d2 < bestAD2) {
						bestAD2 = d2;
						parentIdx = static_cast<int>(j);
					}
				}
				if (parentIdx >= 0 && bestAD2 < 3.0f * 3.0f) {
					parentIdx = -1; // already within 3 blocks (FollowParentGoal.cpp L57)
				}
			}

			if (breedPartnerIdx >= 0) {
				WorldMobEntity& partner = mobs[static_cast<size_t>(breedPartnerIdx)];
				float bx = partner.pos.x - m.pos.x;
				float bz = partner.pos.z - m.pos.z;
				float bDist = std::sqrt(bx * bx + bz * bz);
				if (bDist > 0.001f) {
					m.moveDirX = bx / bDist;
					m.moveDirZ = bz / bDist;
					targetBodyYaw = dirToMcpeYaw(m.moveDirX, m.moveDirZ);
					targetHeadYaw = targetBodyYaw;
				}
				desiredSpeed = (bDist > 1.2f) ? calcMcpeSpeed(1.0f) : 0.0f;
				if (bDist < 1.8f) {
					m.breedProgress += dt;
					if (m.breedProgress >= 3.0f && m.id < partner.id) { // 60 ticks in BreedGoal.cpp L173
						m.inLoveTimer = 0.0f;
						partner.inLoveTimer = 0.0f;
						m.breedProgress = 0.0f;
						partner.breedProgress = 0.0f;
						m.breedCooldown = 30.0f;
						partner.breedCooldown = 30.0f;

						WorldMobEntity baby;
						baby.id = mNextId++;
						baby.type = m.type;
						baby.pos = m.pos;
						baby.lastStuckPos = m.pos;
						baby.yaw = m.yaw;
						baby.headYaw = m.yaw;
						baby.health = baby.maxHealth = getMaxHealthForType(m.type);
						baby.isBaby = true;
						baby.babyTimer = 120.0f;
						baby.woolColor = (rng.nextInt(2) == 0) ? m.woolColor : partner.woolColor;
						newbornBabies.push_back(baby);
						if (onHearts) {
							onHearts(m.pos, m.halfWidth(), m.height(), 7);
						}
					}
				}
			} else if (temptedByPlayer) {
				// TemptGoal.cpp L104-118: look at player, walk at 1.0x if dist >= 2.5 (distSq >= 6.25), stop if closer
				float invD = (dist > 0.001f) ? (1.0f / dist) : 0.0f;
				float dirX = dxP * invD;
				float dirZ = dzP * invD;
				float playerYaw = dirToMcpeYaw(dirX, dirZ);
				targetHeadYaw = playerYaw;
				targetHeadPitch = std::clamp(std::atan2(-(playerEyePos.y - eyeFrom.y), std::max(0.5f, dist)) * kRadToDeg, -35.0f, 35.0f);
				if (dist >= 2.5f) {
					m.moveDirX = dirX;
					m.moveDirZ = dirZ;
					desiredSpeed = calcMcpeSpeed(1.0f);
					targetBodyYaw = playerYaw;
				} else {
					m.moveDirX = 0.0f;
					m.moveDirZ = 0.0f;
					desiredSpeed = 0.0f;
					targetBodyYaw = playerYaw;
				}
			} else if (parentIdx >= 0) {
				// FollowParentGoal.cpp: walk toward parent at 1.1x
				const WorldMobEntity& parent = mobs[static_cast<size_t>(parentIdx)];
				float bx = parent.pos.x - m.pos.x;
				float bz = parent.pos.z - m.pos.z;
				float bDist = std::sqrt(bx * bx + bz * bz);
				if (bDist > 0.001f) {
					m.moveDirX = bx / bDist;
					m.moveDirZ = bz / bDist;
					targetBodyYaw = dirToMcpeYaw(m.moveDirX, m.moveDirZ);
					targetHeadYaw = targetBodyYaw;
					desiredSpeed = calcMcpeSpeed(1.1f);
				}
			} else {
				auto pickWanderDirection = [&](bool panic) {
					float bestScore = -1e9f;
					float bestX = 0.0f;
					float bestZ = 0.0f;
					int tries = 10;
					int maxRange = panic ? 5 : 10;
					for (int t = 0; t < tries; ++t) {
						float a = rng.nextFloat() * (2.0f * kPi);
						float rDist = 2.0f + rng.nextFloat() * static_cast<float>(maxRange - 2);
						float cx = std::cos(a);
						float cz = std::sin(a);
						float score = 0.0f;
						int px = Math::floor(m.pos.x + cx * rDist);
						int pz = Math::floor(m.pos.z + cz * rDist);
						float fY = 0.0f;
						bool inWat = false;
						if (getColumnFloorY(region, px, Math::floor(m.pos.y + 0.3f), pz, m.height(), fY, inWat)) {
							score += inWat ? -10.0f : 2.0f;
							if (region.getBlockID(px, Math::floor(fY) - 1, pz) == 2) score += 10.0f;
							if (std::abs(fY - m.pos.y) <= 1.05f) score += 1.5f;
						} else {
							score -= 10.0f;
						}
						score += rng.nextFloat() * 0.8f;
						if (score > bestScore) {
							bestScore = score;
							bestX = cx;
							bestZ = cz;
						}
					}
					m.moveDirX = bestX;
					m.moveDirZ = bestZ;
				};

				// Stuck detection while walking
				if (std::abs(m.moveDirX) > 0.01f || std::abs(m.moveDirZ) > 0.01f) {
					m.stuckTimer += dt;
					if (m.stuckTimer >= 0.45f) {
						float movedSq = (m.pos.x - m.lastStuckPos.x) * (m.pos.x - m.lastStuckPos.x) +
						                (m.pos.z - m.lastStuckPos.z) * (m.pos.z - m.lastStuckPos.z);
						m.lastStuckPos = m.pos;
						m.stuckTimer = 0.0f;
						if (movedSq < 0.06f * 0.06f) {
							m.aiTimer = 0.0f;
						}
					}
				} else {
					m.stuckTimer = 0.0f;
					m.lastStuckPos = m.pos;
				}

				if (m.type == MobType::Sheep && m.eatTimer > 0.0f) {
					m.moveDirX = 0.0f;
					m.moveDirZ = 0.0f;
					desiredSpeed = 0.0f;
				} else {
					m.aiTimer -= dt;
					if (m.aiTimer <= 0.0f) {
						if (m.panicTimer > 0.0f) {
							pickWanderDirection(true);
							m.aiTimer = 0.7f + rng.nextFloat() * 0.5f;
						} else if (rng.nextFloat() < 0.48f) {
							pickWanderDirection(false);
							m.aiTimer = 2.0f + rng.nextFloat() * 2.5f;
						} else {
							m.moveDirX = 0.0f;
							m.moveDirZ = 0.0f;
							m.aiTimer = 2.5f + rng.nextFloat() * 4.5f;
							if (m.type == MobType::Sheep && m.onGround && rng.nextFloat() < 0.14f) {
								int gx = Math::floor(m.pos.x);
								int gy = Math::floor(m.pos.y) - 1;
								int gz = Math::floor(m.pos.z);
								if (region.getBlockID(gx, gy, gz) == 2) {
									m.eatTimer = 2.0f;
								}
							}
						}
					}

					float strollMult = (m.type == MobType::Cow) ? 0.80f : 1.0f;
					float activeMult = (m.panicTimer > 0.0f) ? 1.50f : strollMult;
					bool wantsMove = (std::abs(m.moveDirX) > 0.01f || std::abs(m.moveDirZ) > 0.01f);
					desiredSpeed = wantsMove ? calcMcpeSpeed(activeMult) : 0.0f;
					if (wantsMove) {
						targetBodyYaw = dirToMcpeYaw(m.moveDirX, m.moveDirZ);
						targetHeadYaw = targetBodyYaw;
					} else {
						if (!m.isHostile() && dist < 6.0f && std::abs(dyP) < 3.0f) {
							targetHeadYaw = dirToMcpeYaw(dxP, dzP);
							float maxTurn = 75.0f;
							float diff = wrapDegrees(targetHeadYaw - m.yaw);
							if (diff > maxTurn) targetHeadYaw = wrapDegrees(m.yaw + maxTurn);
							else if (diff < -maxTurn) targetHeadYaw = wrapDegrees(m.yaw - maxTurn);
							targetHeadPitch = std::clamp(std::atan2(-(playerEyePos.y - eyeFrom.y), std::max(0.5f, dist)) * kRadToDeg, -30.0f, 30.0f);
						} else {
							m.lookTimer -= dt;
							if (m.lookTimer <= 0.0f) {
								m.lookTimer = 1.2f + rng.nextFloat() * 2.2f;
								m.lookTargetYaw = wrapDegrees(m.yaw + (rng.nextFloat() - 0.5f) * 90.0f);
							}
							targetHeadYaw = m.lookTargetYaw;
						}
					}
				}
			}
		}

		// Smooth angular interpolation matching MCPE 0.8.1 MoveControl/LookControl/BodyControl
		float bodyTurnRate = (desiredSpeed > 0.0f || m.hasTarget || m.panicTimer > 0.0f) ? 360.0f : 120.0f;
		m.yaw = approachAngle(m.yaw, targetBodyYaw, bodyTurnRate * dt);
		m.headYaw = approachAngle(m.headYaw, targetHeadYaw, 220.0f * dt);
		m.headPitch += (targetHeadPitch - m.headPitch) * std::min(1.0f, dt * 10.0f);

		m.desiredSpeed = desiredSpeed;
		moveMobWithPhysics(m, dt, region);

		// Chicken wing flap animation (matches MCPE Chicken.cpp + ChickenRenderer.cpp)
		if (m.type == MobType::Chicken) {
			if (!m.onGround && !m.inWater) {
				m.wingFlapPhase += dt * 18.0f;
				float targetFlap = (std::sin(m.wingFlapPhase) + 1.0f) * 0.75f;
				m.wingFlap += (targetFlap - m.wingFlap) * std::min(1.0f, dt * 18.0f);
			} else {
				m.wingFlap += (0.0f - m.wingFlap) * std::min(1.0f, dt * 12.0f);
			}
		}

		++i;
	}

	for (auto& nb : newbornBabies) {
		mobs.push_back(std::move(nb));
	}

	// Update skeleton arrows
	for (size_t i = 0; i < arrows.size(); ) {
		WorldArrowEntity& a = arrows[i];
		a.age += dt;
		if (a.age > 12.0f || a.pos.y < -32.0f) {
			arrows[i] = arrows.back();
			arrows.pop_back();
			continue;
		}
		if (!a.stuck) {
			a.vel.y -= 16.0f * dt;
			float hVel = std::sqrt(a.vel.x * a.vel.x + a.vel.z * a.vel.z);
			if (hVel > 0.001f || std::abs(a.vel.y) > 0.001f) {
				a.yaw = std::atan2(a.vel.x, a.vel.z) * kRadToDeg;
				a.pitch = std::atan2(a.vel.y, hVel) * kRadToDeg;
			}
			Vec3 nextPos = a.pos + a.vel * dt;

			if (!isCreative && allowCombat) {
				AABB arrowBB(nextPos.x - 0.15f, nextPos.y - 0.15f, nextPos.z - 0.15f,
				             nextPos.x + 0.15f, nextPos.y + 0.15f, nextPos.z + 0.15f);
				if (arrowBB.intersects(playerBB)) {
					if (onDamagePlayer) {
						onDamagePlayer(a.damage, Vec3(a.vel.x, 0.0f, a.vel.z));
					}
					arrows[i] = arrows.back();
					arrows.pop_back();
					continue;
				}
			}

			HitResult blockHit = region.clip(a.pos, nextPos, false, true, 16, false);
			if (blockHit.isHit() && blockHit.isTile()) {
				a.pos = blockHit.getPos();
				a.vel = Vec3::ZERO;
				a.stuck = true;
			} else {
				a.pos = nextPos;
			}
		}
		++i;
	}
}

void MobManager::render(BlockSource& region,
                        const glm::mat4& projection,
                        const glm::mat4& view,
                        float sunFactor) const {
	if (mobs.empty() && arrows.empty()) return;

	static PigModelData pigModel;
	static CowModelData cowModel;
	static SheepModelData sheepModel;
	static ChickenModelData chickenModel;
	static ZombieModelData zombieModel;
	static SkeletonModelData skeletonModel;
	static CreeperModelData creeperModel;

	glUseProgram(0);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.1f);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(projection));
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(view));

	const glm::mat4 vp = projection * view;
	glm::vec4 fPlanes[6];
	for (int i = 0; i < 4; ++i) {
		fPlanes[0][i] = vp[i][3] + vp[i][0];
		fPlanes[1][i] = vp[i][3] - vp[i][0];
		fPlanes[2][i] = vp[i][3] + vp[i][1];
		fPlanes[3][i] = vp[i][3] - vp[i][1];
		fPlanes[4][i] = vp[i][3] + vp[i][2];
		fPlanes[5][i] = vp[i][3] - vp[i][2];
	}
	for (int p = 0; p < 6; ++p) {
		const float len = std::sqrt(fPlanes[p].x * fPlanes[p].x + fPlanes[p].y * fPlanes[p].y + fPlanes[p].z * fPlanes[p].z);
		if (len > 0.0f) fPlanes[p] /= len;
	}

	int lastBoundMobType = -1;
	bool lastBindOk = false;
	for (const auto& m : mobs) {
		const float cx = m.pos.x;
		const float cy = m.pos.y + m.height() * 0.5f;
		const float cz = m.pos.z;
		const float rad = std::max(m.height(), 1.5f);
		bool vis = true;
		for (int p = 0; p < 6; ++p) {
			if (fPlanes[p].x * cx + fPlanes[p].y * cy + fPlanes[p].z * cz + fPlanes[p].w < -rad) {
				vis = false;
				break;
			}
		}
		if (!vis) continue;
		const int mTypeInt = static_cast<int>(m.type);
		if (mTypeInt != lastBoundMobType) {
			lastBoundMobType = mTypeInt;
			lastBindOk = GuiComponent::bindTexture(getMobTexturePath(m.type));
		}
		if (!lastBindOk) continue;

		BlockPos samplePos(Math::floor(m.pos.x), Math::floor(m.pos.y + m.height() * 0.5f), Math::floor(m.pos.z));
		float light = WorldLighting::sampleWorldBrightness(region, samplePos, sunFactor);

		float tr = light;
		float tg = light;
		float tb = light;

		// Exact MCPE 0.8.1 MobRenderer::render transform:
		// translate(pos), rotate(180 - bodyYaw, Y), rotate(deathFlip, Z), scale(creeperSwell), scale(-1, -1, 1), translate(0, -24*0.0625, 0)
		glPushMatrix();
		glTranslatef(m.pos.x, m.pos.y, m.pos.z);
		glRotatef(180.0f - m.yaw, 0.0f, 1.0f, 0.0f);

		// MCPE 0.8.1 MobRenderer::setupRotations L215-223: 90-degree lateral fall-over during 20-tick death
		if (m.isDying || m.deathTimer > 0.0f) {
			float deathTicks = m.deathTimer * 20.0f;
			float v8 = std::sqrt(std::max(0.0f, (deathTicks / 20.0f) * 1.6f));
			if (v8 > 1.0f) v8 = 1.0f;
			glRotatef(v8 * 90.0f, 0.0f, 0.0f, 1.0f);
		}

		bool creeperWhiteFlash = false;
		float creeperFlashAlpha = 0.0f;
		if (m.type == MobType::Creeper && m.fuseTimer > 0.0f) {
			float sw = std::clamp(m.fuseTimer / 1.5f, 0.0f, 1.0f);
			float v4 = 1.0f + std::sin(sw * 100.0f) * sw * 0.01f;
			float sw4 = sw * sw * sw * sw;
			glScalef((1.0f + sw4 * 0.4f) * v4,
			         (1.0f + sw4 * 0.1f) / v4,
			         (1.0f + sw4 * 0.4f) * v4);
			if ((static_cast<int>(sw * 10.0f) & 1) != 0) {
				creeperWhiteFlash = true;
				creeperFlashAlpha = std::clamp(sw * 0.2f, 0.0f, 1.0f);
			}
		}

		glScalef(-1.0f, -1.0f, 1.0f);
		glTranslatef(0.0f, -24.0f * kPxScale, 0.0f);

		float netHeadYawRad = std::clamp(wrapDegrees(m.headYaw - m.yaw), -65.0f, 65.0f) * kDegToRad;
		float headPitchRad = std::clamp(m.headPitch, -40.0f, 40.0f) * kDegToRad;
		float ageTicks = m.age * 20.0f;
		// MCPE MobRenderer::render L126-130: baby walkPos * 3.0, walkSpeed clamped to 1.0
		float walkPos = m.isBaby ? (m.walkAnimPos * 3.0f) : m.walkAnimPos;
		float walkSpd = std::min(1.0f, m.walkSpeedVis);
		float legSwingRad = std::cos(walkPos * 0.6662f) * 1.4f * walkSpd;

		auto drawCurrentMobGeometry = [&](float cr, float cg, float cb, float ca, bool flatOverlay) {
			auto renderQuadruped = [&](MobModelPart& head, MobModelPart& body,
			                           MobModelPart& l0, MobModelPart& l1, MobModelPart& l2, MobModelPart& l3,
			                           float babyHeadY, float babyHeadZ) {
				if (m.isBaby) {
					glPushMatrix();
					glTranslatef(0.0f, kPxScale * babyHeadY, kPxScale * babyHeadZ);
					head.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					glPopMatrix();
					glPushMatrix();
					glScalef(0.5f, 0.5f, 0.5f);
					glTranslatef(0.0f, kPxScale * 24.0f, 0.0f);
					body.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					l0.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					l1.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					l2.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					l3.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					glPopMatrix();
				} else {
					head.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					body.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					l0.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					l1.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					l2.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					l3.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				}
			};

			switch (m.type) {
			case MobType::Pig: {
				pigModel.head.yRot = netHeadYawRad;
				pigModel.head.xRot = headPitchRad;
				pigModel.leg0.xRot = legSwingRad;
				pigModel.leg3.xRot = legSwingRad;
				pigModel.leg1.xRot = -legSwingRad;
				pigModel.leg2.xRot = -legSwingRad;
				renderQuadruped(pigModel.head, pigModel.body, pigModel.leg0, pigModel.leg1, pigModel.leg2, pigModel.leg3, 4.0f, 4.0f);
				break;
			}
			case MobType::Cow: {
				cowModel.head.yRot = netHeadYawRad;
				cowModel.head.xRot = headPitchRad;
				cowModel.leg0.xRot = legSwingRad;
				cowModel.leg3.xRot = legSwingRad;
				cowModel.leg1.xRot = -legSwingRad;
				cowModel.leg2.xRot = -legSwingRad;
				renderQuadruped(cowModel.head, cowModel.body, cowModel.leg0, cowModel.leg1, cowModel.leg2, cowModel.leg3, 8.0f, 6.0f);
				break;
			}
			case MobType::Sheep: {
				float eatYOff = 6.0f;
				float sheepHeadXRot = headPitchRad;
				if (m.eatTimer > 0.0f) {
					float ticksLeft = m.eatTimer * 20.0f;
					float posScale = 1.0f;
					if (ticksLeft < 4.0f) posScale = ticksLeft * 0.25f;
					else if (ticksLeft > 36.0f) posScale = (40.0f - ticksLeft) * 0.25f;
					eatYOff = 6.0f + posScale * 9.0f;

					if (ticksLeft >= 5.0f && ticksLeft <= 36.0f) {
						sheepHeadXRot = (kPi * 0.07f) * std::sin(((ticksLeft - 4.0f) * 0.03125f) * 28.7f) + (kPi * 0.2f);
					} else {
						sheepHeadXRot = kPi * 0.2f;
					}
				}
				sheepModel.head.yOffset = eatYOff;
				sheepModel.head.yRot = netHeadYawRad;
				sheepModel.head.xRot = sheepHeadXRot;
				sheepModel.leg0.xRot = legSwingRad;
				sheepModel.leg3.xRot = legSwingRad;
				sheepModel.leg1.xRot = -legSwingRad;
				sheepModel.leg2.xRot = -legSwingRad;

				if (!flatOverlay) glDisable(GL_ALPHA_TEST);
				renderQuadruped(sheepModel.head, sheepModel.body, sheepModel.leg0, sheepModel.leg1, sheepModel.leg2, sheepModel.leg3, 8.0f, 4.0f);
				if (!flatOverlay) glEnable(GL_ALPHA_TEST);

				if (!m.isSheared) {
					float fr = 1.0f, fg = 1.0f, fb = 1.0f;
					if (!flatOverlay) getSheepFleeceRGB(m.woolColor, fr, fg, fb);
					float wr = cr * fr, wg = cg * fg, wb = cb * fb;
					sheepModel.headWool.yOffset = eatYOff;
					sheepModel.headWool.yRot = netHeadYawRad;
					sheepModel.headWool.xRot = sheepHeadXRot;
					sheepModel.leg0Wool.xRot = legSwingRad;
					sheepModel.leg3Wool.xRot = legSwingRad;
					sheepModel.leg1Wool.xRot = -legSwingRad;
					sheepModel.leg2Wool.xRot = -legSwingRad;
					renderQuadruped(sheepModel.headWool, sheepModel.bodyWool, sheepModel.leg0Wool, sheepModel.leg1Wool, sheepModel.leg2Wool, sheepModel.leg3Wool, 8.0f, 4.0f);
				}
				break;
			}
			case MobType::Chicken: {
				chickenModel.head.yRot = netHeadYawRad;
				chickenModel.head.xRot = -headPitchRad;
				chickenModel.beak.yRot = netHeadYawRad;
				chickenModel.beak.xRot = -headPitchRad;
				chickenModel.wattle.yRot = netHeadYawRad;
				chickenModel.wattle.xRot = -headPitchRad;
				chickenModel.rightLeg.xRot = legSwingRad;
				chickenModel.leftLeg.xRot = -legSwingRad;
				chickenModel.rightWing.zRot = m.wingFlap;
				chickenModel.leftWing.zRot = -m.wingFlap;
				if (m.isBaby) {
					glPushMatrix();
					glTranslatef(0.0f, kPxScale * 5.0f, kPxScale * 2.0f);
					chickenModel.head.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.beak.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.wattle.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					glPopMatrix();
					glPushMatrix();
					glScalef(0.5f, 0.5f, 0.5f);
					glTranslatef(0.0f, kPxScale * 24.0f, 0.0f);
					chickenModel.body.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.rightLeg.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.leftLeg.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.rightWing.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.leftWing.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					glPopMatrix();
				} else {
					chickenModel.head.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.beak.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.wattle.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.body.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.rightLeg.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.leftLeg.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.rightWing.render(kPxScale, cr, cg, cb, ca, flatOverlay);
					chickenModel.leftWing.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				}
				break;
			}
			case MobType::Zombie: {
				zombieModel.head.yRot = netHeadYawRad;
				zombieModel.head.xRot = headPitchRad;
				zombieModel.rightLeg.xRot = legSwingRad;
				zombieModel.leftLeg.xRot = -legSwingRad;

				float sw = m.attackSwing;
				float v10 = std::sin(sw * kPi);
				float v11 = std::sin((1.0f - (1.0f - sw) * (1.0f - sw)) * kPi);
				float armY = 0.1f - v10 * 0.6f;
				float armBaseX = -kPi * 0.5f - (v10 * 1.2f - v11 * 0.4f);
				float armZ = std::cos(ageTicks * 0.09f) * 0.05f + 0.05f;
				float armBobX = std::sin(ageTicks * 0.067f) * 0.05f;
				zombieModel.leftArm.yRot = armY;
				zombieModel.rightArm.yRot = -armY;
				zombieModel.rightArm.zRot = armZ;
				zombieModel.leftArm.zRot = -armZ;
				zombieModel.rightArm.xRot = armBaseX + armBobX;
				zombieModel.leftArm.xRot = armBaseX - armBobX;

				zombieModel.head.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				zombieModel.body.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				zombieModel.rightArm.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				zombieModel.leftArm.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				zombieModel.rightLeg.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				zombieModel.leftLeg.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				break;
			}
			case MobType::Skeleton: {
				skeletonModel.head.yRot = netHeadYawRad;
				skeletonModel.head.xRot = headPitchRad;
				skeletonModel.rightLeg.xRot = legSwingRad;
				skeletonModel.leftLeg.xRot = -legSwingRad;

				float armZ = std::cos(ageTicks * 0.09f) * 0.05f + 0.05f;
				float armBobX = std::sin(ageTicks * 0.067f) * 0.05f;
				skeletonModel.rightArm.yRot = netHeadYawRad - 0.1f;
				skeletonModel.leftArm.yRot = netHeadYawRad + 0.5f;
				skeletonModel.rightArm.zRot = armZ;
				skeletonModel.leftArm.zRot = -armZ;
				skeletonModel.rightArm.xRot = headPitchRad - kPi * 0.5f + armBobX;
				skeletonModel.leftArm.xRot = headPitchRad - kPi * 0.5f - armBobX;

				skeletonModel.head.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				skeletonModel.body.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				skeletonModel.rightArm.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				skeletonModel.leftArm.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				skeletonModel.rightLeg.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				skeletonModel.leftLeg.render(kPxScale, cr, cg, cb, ca, flatOverlay);

				if (!flatOverlay) {
					glPushMatrix();
					skeletonModel.rightArm.translateTo(kPxScale);
					glTranslatef(-0.0625f, 0.4375f, 0.0625f);
					glTranslatef(0.2f, 0.125f, -0.3875f);
					glRotatef(-180.0f, 1.0f, 0.0f, 0.0f);
					glRotatef(10.0f, 0.0f, 1.0f, 0.0f);
					glScalef(0.625f, 0.625f, 0.625f);
					glRotatef(-100.0f, 1.0f, 0.0f, 0.0f);
					glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
					glScalef(1.5f, 1.5f, 1.5f);
					glRotatef(50.0f, 0.0f, 1.0f, 0.0f);
					glRotatef(335.0f, 0.0f, 0.0f, 1.0f);
					glTranslatef(0.075f, -0.245f, -0.1f);
					glScalef(0.0625f, 0.0625f, 0.0625f);
					glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
					glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
					glRotatef(-180.0f, 1.0f, 0.0f, 0.0f);
					glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
					glTranslatef(0.0f, 0.0f, -16.0f);
					glTranslatef(8.0f, 0.5f, 8.0f);
					glScalef(16.0f, 16.0f, -16.0f);
					glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
					drawMobExtrudedSprite3D("textures/items/bow_standby.png", cr, cg, cb);
					glPopMatrix();
				}
				break;
			}
			case MobType::Creeper: {
				creeperModel.head.yRot = netHeadYawRad;
				creeperModel.head.xRot = headPitchRad;
				creeperModel.leg0.xRot = legSwingRad;
				creeperModel.leg3.xRot = legSwingRad;
				creeperModel.leg1.xRot = -legSwingRad;
				creeperModel.leg2.xRot = -legSwingRad;
				creeperModel.head.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				creeperModel.body.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				creeperModel.leg0.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				creeperModel.leg1.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				creeperModel.leg2.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				creeperModel.leg3.render(kPxScale, cr, cg, cb, ca, flatOverlay);
				break;
			}
			}
		};

		// Pass 1: Textured model
		drawCurrentMobGeometry(tr, tg, tb, 1.0f, false);

		// Pass 2: Exact MCPE 0.8.1 MobRenderer::render L163-198 GL_EQUAL untextured color overlay
		// - Hurt or dying: Color4(brightness, 0.0, 0.0, 0.6)
		// - Burning: Color4(0.8, 0.256, 0.0, 0.45)
		// - Creeper fuse flash: Color4(1.0, 1.0, 1.0, creeperFlashAlpha)
		bool hasHurtOrDeathOverlay = (m.hurtTimer > 0.0f || m.isDying);
		if (hasHurtOrDeathOverlay || m.isBurning || creeperWhiteFlash) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_ALPHA_TEST);
			glDepthFunc(GL_EQUAL);

			if (m.isBurning) {
				drawCurrentMobGeometry(0.80f, 0.256f, 0.0f, 0.45f, true);
			} else if (hasHurtOrDeathOverlay) {
				drawCurrentMobGeometry(light, 0.0f, 0.0f, 0.60f, true);
			}
			if (creeperWhiteFlash) {
				drawCurrentMobGeometry(1.0f, 1.0f, 1.0f, creeperFlashAlpha, true);
			}

			glDepthFunc(GL_LEQUAL);
			glEnable(GL_ALPHA_TEST);
			glEnable(GL_TEXTURE_2D);
			glDisable(GL_BLEND);
		}

		glPopMatrix();
	}

	// Render arrows (preserve yaw/pitch when stuck)
	if (!arrows.empty() && GuiComponent::bindTexture("textures/entity/arrows.png")) {
		for (const auto& a : arrows) {
			glPushMatrix();
			glTranslatef(a.pos.x, a.pos.y, a.pos.z);
			glRotatef(a.yaw, 0.0f, 1.0f, 0.0f);
			glRotatef(-a.pitch, 1.0f, 0.0f, 0.0f);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f);         glVertex3f(0.0f, -0.09f, -0.45f);
			glTexCoord2f(0.5f, 0.0f);         glVertex3f(0.0f, -0.09f,  0.45f);
			glTexCoord2f(0.5f, 5.0f / 32.0f); glVertex3f(0.0f,  0.09f,  0.45f);
			glTexCoord2f(0.0f, 5.0f / 32.0f); glVertex3f(0.0f,  0.09f, -0.45f);

			glTexCoord2f(0.0f, 0.0f);         glVertex3f(-0.09f, 0.0f, -0.45f);
			glTexCoord2f(0.5f, 0.0f);         glVertex3f(-0.09f, 0.0f,  0.45f);
			glTexCoord2f(0.5f, 5.0f / 32.0f); glVertex3f( 0.09f, 0.0f,  0.45f);
			glTexCoord2f(0.0f, 5.0f / 32.0f); glVertex3f( 0.09f, 0.0f, -0.45f);
			glEnd();
			glPopMatrix();
		}
	}

	glDisable(GL_ALPHA_TEST);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

bool MobManager::spawnMobAt(MobType type, float x, float y, float z) {
	static Random eggRng;
	WorldMobEntity m;
	m.id = mNextId++;
	m.type = type;
	m.pos = Vec3(x, y, z);
	m.yaw = eggRng.nextFloat() * 360.0f;
	m.headYaw = m.yaw;
	m.headPitch = 0.0f;
	m.health = m.maxHealth;
	mobs.push_back(std::move(m));
	return true;
}
