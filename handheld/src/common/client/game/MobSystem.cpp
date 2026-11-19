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
#include "world/level/material/Material.h"
#include "world/phys/HitResult.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace {

Random& mobRng() {
	static Random rng;
	return rng;
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

void drawMobBox3D(float x0, float y0, float z0,
                  float w, float h, float d,
                  int u, int v, int iw, int ih, int id,
                  float texW, float texH,
                  float tr, float tg, float tb) {
	float x1 = x0 + w;
	float y1 = y0 + h;
	float z1 = z0 + d;

	auto quad = [&](float ax, float ay, float az,
	                float bx, float by, float bz,
	                float cx, float cy, float cz,
	                float dx, float dy, float dz,
	                int su, int sv, int sw, int sh, float faceShade) {
		float u0 = static_cast<float>(su) / texW;
		float v0 = static_cast<float>(sv) / texH;
		float u1 = static_cast<float>(su + sw) / texW;
		float v1 = static_cast<float>(sv + sh) / texH;
		glColor4f(std::min(1.0f, tr * faceShade),
		          std::min(1.0f, tg * faceShade),
		          std::min(1.0f, tb * faceShade), 1.0f);
		glTexCoord2f(u0, v1); glVertex3f(ax, ay, az);
		glTexCoord2f(u1, v1); glVertex3f(bx, by, bz);
		glTexCoord2f(u1, v0); glVertex3f(cx, cy, cz);
		glTexCoord2f(u0, v0); glVertex3f(dx, dy, dz);
	};

	glBegin(GL_QUADS);
	// Front (+Z)
	quad(x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1, u + id, v + id, iw, ih, 0.96f);
	// Back (-Z)
	quad(x1, y0, z0, x0, y0, z0, x0, y1, z0, x1, y1, z0, u + id + iw + id, v + id, iw, ih, 0.86f);
	// Right (-X)
	quad(x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0, u, v + id, id, ih, 0.80f);
	// Left (+X)
	quad(x1, y0, z1, x1, y0, z0, x1, y1, z0, x1, y1, z1, u + id + iw, v + id, id, ih, 0.80f);
	// Top (+Y)
	quad(x0, y1, z1, x1, y1, z1, x1, y1, z0, x0, y1, z0, u + id, v, iw, id, 1.0f);
	// Bottom (-Y)
	quad(x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, u + id + iw, v, iw, id, 0.65f);
	glEnd();
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

void moveMobWithPhysics(WorldMobEntity& mob, float dt, BlockSource& region) {
	int bx = Math::floor(mob.pos.x);
	int by = Math::floor(mob.pos.y + 0.35f);
	int bz = Math::floor(mob.pos.z);
	mob.inWater = region.getMaterial(bx, by, bz).isType(MaterialType::Water);

	if (mob.inWater) {
		mob.vel.y += 18.0f * dt;
		if (mob.vel.y > 3.2f) mob.vel.y = 3.2f;
	} else {
		float grav = (mob.type == MobType::Chicken) ? 14.0f : 28.0f;
		mob.vel.y -= grav * dt;
		if (mob.type == MobType::Chicken && mob.vel.y < -2.6f) {
			mob.vel.y = -2.6f;
		} else if (mob.vel.y < -30.0f) {
			mob.vel.y = -30.0f;
		}
	}

	Vec3 step = mob.vel * dt;
	AABB bb = mob.getAABB();
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

	float prevX = mob.pos.x;
	float prevZ = mob.pos.z;
	mob.pos.x = (bb.min.x + bb.max.x) * 0.5f;
	mob.pos.y = bb.min.y;
	mob.pos.z = (bb.min.z + bb.max.z) * 0.5f;

	bool hitHoriz = (axisSteps[1].x != xaOrg || axisSteps[2].z != zaOrg);
	if (axisSteps[1].x != xaOrg) mob.vel.x = 0.0f;
	if (axisSteps[2].z != zaOrg) mob.vel.z = 0.0f;

	if (axisSteps[0].y != yaOrg) {
		mob.onGround = (yaOrg < 0.0f);
		mob.vel.y = 0.0f;
	} else {
		mob.onGround = false;
	}

	// Jump over 1-block obstacles when moving
	if (hitHoriz && mob.onGround && (std::abs(mob.moveDirX) > 0.05f || std::abs(mob.moveDirZ) > 0.05f)) {
		mob.vel.y = 8.4f;
		mob.onGround = false;
	}

	float dx = mob.pos.x - prevX;
	float dz = mob.pos.z - prevZ;
	float dist = std::sqrt(dx * dx + dz * dz);
	mob.walkDist += dist;
	float instSpeed = (dt > 0.0001f) ? (dist / dt) : 0.0f;
	mob.walkSpeedVis += (std::min(1.0f, instSpeed / 2.8f) - mob.walkSpeedVis) * std::min(1.0f, dt * 10.0f);

	float tickScale = dt / 0.05f;
	float drag = std::pow(mob.onGround ? 0.65f : 0.90f, tickScale);
	mob.vel.x *= drag;
	mob.vel.z *= drag;
}

} // namespace

bool WorldMobEntity::isHostile() const {
	return type == MobType::Zombie || type == MobType::Creeper || type == MobType::Skeleton;
}

float WorldMobEntity::halfWidth() const {
	switch (type) {
	case MobType::Pig:      return 0.45f;
	case MobType::Cow:      return 0.45f;
	case MobType::Sheep:    return 0.45f;
	case MobType::Chicken:  return 0.28f;
	case MobType::Zombie:   return 0.32f;
	case MobType::Creeper:  return 0.32f;
	case MobType::Skeleton: return 0.32f;
	}
	return 0.35f;
}

float WorldMobEntity::height() const {
	switch (type) {
	case MobType::Pig:      return 0.90f;
	case MobType::Cow:      return 1.35f;
	case MobType::Sheep:    return 1.30f;
	case MobType::Chicken:  return 0.72f;
	case MobType::Zombie:   return 1.95f;
	case MobType::Creeper:  return 1.70f;
	case MobType::Skeleton: return 1.95f;
	}
	return 1.0f;
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

void MobManager::spawnDropsForMob(const WorldMobEntity& mob,
                                  const std::function<void(const ItemInstance&, const Vec3&)>& onDropItem) const {
	if (!onDropItem) return;
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
		if (Block::mWool) {
			onDropItem(ItemInstance(Block::mWool, 1, 0), dropPos);
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
	if (mob.hurtTimer > 0.15f) return false;

	mob.health = std::max(0, mob.health - std::max(1, damage));
	mob.hurtTimer = 0.40f;

	float len = std::sqrt(knockDir.x * knockDir.x + knockDir.z * knockDir.z);
	if (len > 0.001f) {
		mob.vel.x += (knockDir.x / len) * 6.5f;
		mob.vel.z += (knockDir.z / len) * 6.5f;
	}
	mob.vel.y = std::max(mob.vel.y, 4.2f);

	if (!mob.isHostile()) {
		mob.panicTimer = 3.8f;
		if (len > 0.001f) {
			mob.moveDirX = knockDir.x / len;
			mob.moveDirZ = knockDir.z / len;
		}
	}

	if (mob.health <= 0) {
		spawnDropsForMob(mob, onDropItem);
		mobs[static_cast<size_t>(index)] = mobs.back();
		mobs.pop_back();
	}
	return true;
}

int MobManager::raycastMob(const Vec3& rayOrigin, const Vec3& rayDir, float maxDist, float& outDist) const {
	int bestIdx = -1;
	float bestT = maxDist;
	for (size_t i = 0; i < mobs.size(); ++i) {
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

	auto trySpawnOne = [&](bool wantHostile) {
		float angle = rng.nextFloat() * 6.283185f;
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
			WorldMobEntity m;
			m.id = mNextId++;
			m.type = chosen;
			m.pos = Vec3(wx + 0.5f, static_cast<float>(topY), wz + 0.5f);
			m.yaw = rng.nextFloat() * 360.0f;
			m.health = m.maxHealth = getMaxHealthForType(chosen);
			mobs.push_back(m);
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
				WorldMobEntity m;
				m.id = mNextId++;
				m.type = chosen;
				m.pos = Vec3(px + 0.5f, static_cast<float>(py), pz + 0.5f);
				m.yaw = rng.nextFloat() * 360.0f;
				m.health = m.maxHealth = getMaxHealthForType(chosen);
				mobs.push_back(m);
				++passiveCount;
			}
		}
	};

	if (passiveCount < maxPassive) {
		trySpawnOne(false);
	}
	if (hostileCount < maxHostile && sunFactor < 0.45f) {
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
                        const std::function<void(const ItemInstance&, const Vec3&)>& onDropItem) {
	dt = std::min(std::max(dt, 0.0001f), 0.05f);
	Random& rng = mobRng();

	mSpawnTimer -= dt;
	if (mSpawnTimer <= 0.0f) {
		mSpawnTimer = 1.6f;
		trySpawnMobs(region, playerEyePos, sunFactor);
	}

	Vec3 playerFeet((playerBB.min.x + playerBB.max.x) * 0.5f,
	                playerBB.min.y,
	                (playerBB.min.z + playerBB.max.z) * 0.5f);

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

		if (m.hurtTimer > 0.0f) m.hurtTimer = std::max(0.0f, m.hurtTimer - dt);
		if (m.attackCooldown > 0.0f) m.attackCooldown = std::max(0.0f, m.attackCooldown - dt);
		if (m.panicTimer > 0.0f) m.panicTimer = std::max(0.0f, m.panicTimer - dt);

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
					m.hurtTimer = 0.25f;
					if (m.health <= 0) {
						spawnDropsForMob(m, onDropItem);
						mobs[i] = mobs.back();
						mobs.pop_back();
						continue;
					}
				}
			}
		}

		// AI
		float dist = std::sqrt(distSq);
		float dyP = playerFeet.y - m.pos.y;
		float desiredSpeed = 0.0f;
		bool targetPlayer = m.isHostile() && !isCreative && allowCombat && dist < 18.0f && std::abs(dyP) < 10.0f;

		if (targetPlayer) {
			float invD = (dist > 0.001f) ? (1.0f / dist) : 0.0f;
			float dirX = dxP * invD;
			float dirZ = dzP * invD;
			m.yaw = std::atan2(dirX, dirZ) * (180.0f / 3.14159265f);

			if (m.type == MobType::Zombie) {
				m.moveDirX = dirX;
				m.moveDirZ = dirZ;
				desiredSpeed = 2.45f;
				if (dist < 1.45f && std::abs(dyP) < 1.6f && m.attackCooldown <= 0.0f) {
					m.attackCooldown = 1.0f;
					if (onDamagePlayer) {
						onDamagePlayer(3, Vec3(dirX, 0.0f, dirZ));
					}
				}
			} else if (m.type == MobType::Creeper) {
				m.moveDirX = dirX;
				m.moveDirZ = dirZ;
				if (dist < 2.6f && std::abs(dyP) < 2.0f) {
					m.fuseTimer += dt;
					desiredSpeed = 0.4f;
					if (m.fuseTimer >= 1.5f) {
						Vec3 expPos(m.pos.x, m.pos.y + 0.7f, m.pos.z);
						float fullDist = std::sqrt(distSq + dyP * dyP);
						if (onDamagePlayer && fullDist < 4.5f) {
							int expDmg = std::max(1, static_cast<int>((1.0f - fullDist / 4.5f) * 18.0f));
							onDamagePlayer(expDmg, Vec3(dirX * 2.2f, 0.0f, dirZ * 2.2f));
						}
						if (onExplosion) {
							onExplosion(expPos, 2.6f);
						}
						mobs[i] = mobs.back();
						mobs.pop_back();
						continue;
					}
				} else {
					if (dist > 4.2f) {
						m.fuseTimer = std::max(0.0f, m.fuseTimer - dt * 1.5f);
					}
					desiredSpeed = 2.35f;
				}
			} else if (m.type == MobType::Skeleton) {
				if (dist > 9.5f) {
					m.moveDirX = dirX;
					m.moveDirZ = dirZ;
					desiredSpeed = 2.25f;
				} else if (dist < 4.5f) {
					m.moveDirX = -dirX;
					m.moveDirZ = -dirZ;
					desiredSpeed = 1.85f;
				} else {
					// Strafe slightly
					m.moveDirX = -dirZ * 0.5f;
					m.moveDirZ = dirX * 0.5f;
					desiredSpeed = 1.1f;
				}

				if (m.attackCooldown <= 0.0f && dist < 16.0f) {
					Vec3 eyeFrom(m.pos.x, m.pos.y + 1.55f, m.pos.z);
					Vec3 aimVec = playerEyePos - eyeFrom;
					float aimDist = aimVec.length();
					if (aimDist > 0.1f) {
						HitResult los = region.clip(eyeFrom, playerEyePos, false, true, 40, false);
						if (!los.isHit()) {
							WorldArrowEntity arrow;
							arrow.pos = eyeFrom + (aimVec / aimDist) * 0.45f;
							Vec3 dirNorm = aimVec / aimDist;
							float arrowSpeed = 22.0f;
							arrow.vel = Vec3(dirNorm.x * arrowSpeed,
							                 dirNorm.y * arrowSpeed + aimDist * 0.16f,
							                 dirNorm.z * arrowSpeed);
							arrow.damage = 3;
							arrows.push_back(arrow);
							m.attackCooldown = 2.1f;
						} else {
							m.attackCooldown = 0.5f;
						}
					}
				}
			}
		} else {
			if (m.type == MobType::Creeper && m.fuseTimer > 0.0f) {
				m.fuseTimer = std::max(0.0f, m.fuseTimer - dt * 1.5f);
			}
			m.aiTimer -= dt;
			if (m.aiTimer <= 0.0f) {
				if (m.panicTimer > 0.0f || rng.nextFloat() < 0.68f) {
					float a = rng.nextFloat() * 6.283185f;
					m.moveDirX = std::cos(a);
					m.moveDirZ = std::sin(a);
					m.aiTimer = m.panicTimer > 0.0f ? 0.6f : (2.0f + rng.nextFloat() * 3.0f);
				} else {
					m.moveDirX = 0.0f;
					m.moveDirZ = 0.0f;
					m.aiTimer = 1.5f + rng.nextFloat() * 2.5f;
				}
			}
			float baseWalk = (m.type == MobType::Chicken) ? 1.25f : 1.45f;
			desiredSpeed = (std::abs(m.moveDirX) > 0.01f || std::abs(m.moveDirZ) > 0.01f)
				? (m.panicTimer > 0.0f ? baseWalk * 2.1f : baseWalk)
				: 0.0f;
			if (desiredSpeed > 0.01f) {
				m.yaw = std::atan2(m.moveDirX, m.moveDirZ) * (180.0f / 3.14159265f);
			}
		}

		if (desiredSpeed > 0.0f) {
			float accel = m.onGround ? 14.0f : 4.5f;
			m.vel.x += (m.moveDirX * desiredSpeed - m.vel.x) * std::min(1.0f, accel * dt);
			m.vel.z += (m.moveDirZ * desiredSpeed - m.vel.z) * std::min(1.0f, accel * dt);
		}

		moveMobWithPhysics(m, dt, region);
		++i;
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

	for (const auto& m : mobs) {
		if (!GuiComponent::bindTexture(getMobTexturePath(m.type))) continue;

		BlockPos samplePos(Math::floor(m.pos.x), Math::floor(m.pos.y + m.height() * 0.5f), Math::floor(m.pos.z));
		float light = WorldLighting::sampleWorldBrightness(region, samplePos, sunFactor);

		float tr = light;
		float tg = light;
		float tb = light;
		if (m.hurtTimer > 0.0f) {
			tr = std::min(1.0f, light * 1.4f + 0.35f);
			tg = light * 0.35f;
			tb = light * 0.35f;
		} else if (m.isBurning) {
			tr = std::min(1.0f, light + 0.45f);
			tg = light * 0.65f;
			tb = light * 0.25f;
		}

		float creeperSwell = 1.0f;
		if (m.type == MobType::Creeper && m.fuseTimer > 0.0f) {
			float f = std::min(1.0f, m.fuseTimer / 1.5f);
			float pulse = 0.5f + 0.5f * std::sin(m.fuseTimer * 26.0f);
			creeperSwell = 1.0f + f * 0.28f * pulse;
			if (pulse > 0.5f) {
				tr = std::min(1.0f, light + f * 0.85f);
				tg = std::min(1.0f, light + f * 0.85f);
				tb = std::min(1.0f, light + f * 0.85f);
			}
		}

		float limbSwing = std::sin(m.walkDist * 2.2f) * 42.0f * m.walkSpeedVis;

		glPushMatrix();
		glTranslatef(m.pos.x, m.pos.y, m.pos.z);
		glRotatef(m.yaw, 0.0f, 1.0f, 0.0f);
		const float pxScale = 0.0625f;
		glScalef(pxScale * creeperSwell, pxScale * (2.0f - creeperSwell * 0.85f > 0.5f ? (1.0f + (creeperSwell - 1.0f) * 0.3f) : 1.0f), pxScale * creeperSwell);

		switch (m.type) {
		case MobType::Zombie: {
			// Legs (y=0..12)
			glPushMatrix();
			glTranslatef(-2.0f, 12.0f, 0.0f);
			glRotatef(limbSwing, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 0, 16, 4, 12, 4, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			glPushMatrix();
			glTranslatef(2.0f, 12.0f, 0.0f);
			glRotatef(-limbSwing, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 0, 16, 4, 12, 4, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			// Torso (y=12..24)
			drawMobBox3D(-4.0f, 12.0f, -2.0f, 8.0f, 12.0f, 4.0f, 16, 16, 8, 12, 4, 64.0f, 32.0f, tr, tg, tb);

			// Head (y=24..32)
			drawMobBox3D(-4.0f, 24.0f, -4.0f, 8.0f, 8.0f, 8.0f, 0, 0, 8, 8, 8, 64.0f, 32.0f, tr, tg, tb);

			// Zombie arms outstretched forward
			glPushMatrix();
			glTranslatef(-6.0f, 22.0f, 0.0f);
			glRotatef(-85.0f + limbSwing * 0.15f, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-2.0f, -10.0f, -2.0f, 4.0f, 12.0f, 4.0f, 40, 16, 4, 12, 4, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			glPushMatrix();
			glTranslatef(6.0f, 22.0f, 0.0f);
			glRotatef(-85.0f - limbSwing * 0.15f, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-2.0f, -10.0f, -2.0f, 4.0f, 12.0f, 4.0f, 40, 16, 4, 12, 4, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();
			break;
		}
		case MobType::Skeleton: {
			glPushMatrix();
			glTranslatef(-2.0f, 12.0f, 0.0f);
			glRotatef(limbSwing, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-1.0f, -12.0f, -1.0f, 2.0f, 12.0f, 2.0f, 0, 16, 2, 12, 2, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			glPushMatrix();
			glTranslatef(2.0f, 12.0f, 0.0f);
			glRotatef(-limbSwing, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-1.0f, -12.0f, -1.0f, 2.0f, 12.0f, 2.0f, 0, 16, 2, 12, 2, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			drawMobBox3D(-4.0f, 12.0f, -2.0f, 8.0f, 12.0f, 4.0f, 16, 16, 8, 12, 4, 64.0f, 32.0f, tr, tg, tb);
			drawMobBox3D(-4.0f, 24.0f, -4.0f, 8.0f, 8.0f, 8.0f, 0, 0, 8, 8, 8, 64.0f, 32.0f, tr, tg, tb);

			glPushMatrix();
			glTranslatef(-5.0f, 22.0f, 0.0f);
			glRotatef(-85.0f, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-1.0f, -10.0f, -1.0f, 2.0f, 12.0f, 2.0f, 40, 16, 2, 12, 2, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			glPushMatrix();
			glTranslatef(5.0f, 22.0f, 0.0f);
			glRotatef(-85.0f, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-1.0f, -10.0f, -1.0f, 2.0f, 12.0f, 2.0f, 40, 16, 2, 12, 2, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();
			break;
		}
		case MobType::Creeper: {
			auto drawLeg = [&](float lx, float lz, float ang) {
				glPushMatrix();
				glTranslatef(lx, 6.0f, lz);
				glRotatef(ang, 1.0f, 0.0f, 0.0f);
				drawMobBox3D(-2.0f, -6.0f, -2.0f, 4.0f, 6.0f, 4.0f, 0, 16, 4, 6, 4, 64.0f, 32.0f, tr, tg, tb);
				glPopMatrix();
			};
			drawLeg(-2.0f, 4.0f, limbSwing);
			drawLeg(2.0f, 4.0f, -limbSwing);
			drawLeg(-2.0f, -4.0f, -limbSwing);
			drawLeg(2.0f, -4.0f, limbSwing);

			drawMobBox3D(-4.0f, 6.0f, -2.0f, 8.0f, 12.0f, 4.0f, 16, 16, 8, 12, 4, 64.0f, 32.0f, tr, tg, tb);
			drawMobBox3D(-4.0f, 18.0f, -4.0f, 8.0f, 8.0f, 8.0f, 0, 0, 8, 8, 8, 64.0f, 32.0f, tr, tg, tb);
			break;
		}
		case MobType::Pig: {
			auto drawLeg = [&](float lx, float lz, float ang) {
				glPushMatrix();
				glTranslatef(lx, 6.0f, lz);
				glRotatef(ang, 1.0f, 0.0f, 0.0f);
				drawMobBox3D(-2.0f, -6.0f, -2.0f, 4.0f, 6.0f, 4.0f, 0, 16, 4, 6, 4, 64.0f, 32.0f, tr, tg, tb);
				glPopMatrix();
			};
			drawLeg(-3.0f, 5.0f, limbSwing);
			drawLeg(3.0f, 5.0f, -limbSwing);
			drawLeg(-3.0f, -7.0f, -limbSwing);
			drawLeg(3.0f, -7.0f, limbSwing);

			// Body (10x16x8 rotated horizontal)
			glPushMatrix();
			glTranslatef(0.0f, 10.0f, 0.0f);
			glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-5.0f, -8.0f, -4.0f, 10.0f, 16.0f, 8.0f, 28, 8, 10, 16, 8, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			// Head + Snout
			drawMobBox3D(-4.0f, 8.0f, 6.0f, 8.0f, 8.0f, 8.0f, 0, 0, 8, 8, 8, 64.0f, 32.0f, tr, tg, tb);
			drawMobBox3D(-2.0f, 9.0f, 14.0f, 4.0f, 3.0f, 1.0f, 16, 16, 4, 3, 1, 64.0f, 32.0f, tr, tg, tb);
			break;
		}
		case MobType::Cow: {
			auto drawLeg = [&](float lx, float lz, float ang) {
				glPushMatrix();
				glTranslatef(lx, 12.0f, lz);
				glRotatef(ang, 1.0f, 0.0f, 0.0f);
				drawMobBox3D(-2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 0, 16, 4, 12, 4, 64.0f, 32.0f, tr, tg, tb);
				glPopMatrix();
			};
			drawLeg(-4.0f, 6.0f, limbSwing);
			drawLeg(4.0f, 6.0f, -limbSwing);
			drawLeg(-4.0f, -6.0f, -limbSwing);
			drawLeg(4.0f, -6.0f, limbSwing);

			glPushMatrix();
			glTranslatef(0.0f, 17.0f, 0.0f);
			glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-6.0f, -9.0f, -5.0f, 12.0f, 18.0f, 10.0f, 18, 4, 12, 18, 10, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			drawMobBox3D(-4.0f, 16.0f, 8.0f, 8.0f, 8.0f, 6.0f, 0, 0, 8, 8, 6, 64.0f, 32.0f, tr, tg, tb);
			drawMobBox3D(-5.0f, 22.0f, 10.0f, 1.0f, 3.0f, 1.0f, 22, 0, 1, 3, 1, 64.0f, 32.0f, tr, tg, tb);
			drawMobBox3D(4.0f, 22.0f, 10.0f, 1.0f, 3.0f, 1.0f, 22, 0, 1, 3, 1, 64.0f, 32.0f, tr, tg, tb);
			break;
		}
		case MobType::Sheep: {
			// Inner skin + outer wool coat (both in 64x64 sheep.tga)
			auto drawLeg = [&](float lx, float lz, float ang) {
				glPushMatrix();
				glTranslatef(lx, 12.0f, lz);
				glRotatef(ang, 1.0f, 0.0f, 0.0f);
				drawMobBox3D(-2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 0, 16, 4, 12, 4, 64.0f, 64.0f, tr, tg, tb);
				drawMobBox3D(-2.5f, -6.5f, -2.5f, 5.0f, 7.0f, 5.0f, 0, 48, 4, 6, 4, 64.0f, 64.0f, tr, tg, tb);
				glPopMatrix();
			};
			drawLeg(-3.0f, 5.0f, limbSwing);
			drawLeg(3.0f, 5.0f, -limbSwing);
			drawLeg(-3.0f, -5.0f, -limbSwing);
			drawLeg(3.0f, -5.0f, limbSwing);

			glPushMatrix();
			glTranslatef(0.0f, 17.0f, 0.0f);
			glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-4.0f, -8.0f, -3.0f, 8.0f, 16.0f, 6.0f, 28, 8, 8, 16, 6, 64.0f, 64.0f, tr, tg, tb);
			drawMobBox3D(-5.75f, -9.75f, -4.75f, 11.5f, 19.5f, 9.5f, 28, 40, 8, 16, 6, 64.0f, 64.0f, tr, tg, tb);
			glPopMatrix();

			drawMobBox3D(-3.0f, 15.0f, 6.0f, 6.0f, 6.0f, 8.0f, 0, 0, 6, 6, 8, 64.0f, 64.0f, tr, tg, tb);
			drawMobBox3D(-3.6f, 14.4f, 5.4f, 7.2f, 7.2f, 7.2f, 0, 32, 6, 6, 6, 64.0f, 64.0f, tr, tg, tb);
			break;
		}
		case MobType::Chicken: {
			// 2 Legs
			glPushMatrix();
			glTranslatef(-1.5f, 5.0f, 0.0f);
			glRotatef(limbSwing, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-1.5f, -5.0f, -1.5f, 3.0f, 5.0f, 3.0f, 26, 0, 3, 5, 3, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			glPushMatrix();
			glTranslatef(1.5f, 5.0f, 0.0f);
			glRotatef(-limbSwing, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-1.5f, -5.0f, -1.5f, 3.0f, 5.0f, 3.0f, 26, 0, 3, 5, 3, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			// Body
			glPushMatrix();
			glTranslatef(0.0f, 8.0f, 0.0f);
			glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
			drawMobBox3D(-3.0f, -4.0f, -3.0f, 6.0f, 8.0f, 6.0f, 0, 9, 6, 8, 6, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			// Wings (flap when falling)
			float flap = m.onGround ? 0.0f : (30.0f + 25.0f * std::sin(m.walkDist * 4.0f + 12.0f));
			glPushMatrix();
			glTranslatef(-3.5f, 11.0f, 0.0f);
			glRotatef(flap, 0.0f, 0.0f, 1.0f);
			drawMobBox3D(-0.5f, -4.0f, -3.0f, 1.0f, 4.0f, 6.0f, 24, 13, 1, 4, 6, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			glPushMatrix();
			glTranslatef(3.5f, 11.0f, 0.0f);
			glRotatef(-flap, 0.0f, 0.0f, 1.0f);
			drawMobBox3D(-0.5f, -4.0f, -3.0f, 1.0f, 4.0f, 6.0f, 24, 13, 1, 4, 6, 64.0f, 32.0f, tr, tg, tb);
			glPopMatrix();

			// Head + Beak + Wattle
			drawMobBox3D(-2.0f, 8.0f, 2.0f, 4.0f, 6.0f, 3.0f, 0, 0, 4, 6, 3, 64.0f, 32.0f, tr, tg, tb);
			drawMobBox3D(-2.0f, 10.0f, 5.0f, 4.0f, 2.0f, 2.0f, 14, 0, 4, 2, 2, 64.0f, 32.0f, tr, tg, tb);
			drawMobBox3D(-1.0f, 8.0f, 4.0f, 2.0f, 2.0f, 2.0f, 14, 4, 2, 2, 2, 64.0f, 32.0f, tr, tg, tb);
			break;
		}
		}

		glPopMatrix();
	}

	// Render arrows
	if (!arrows.empty() && GuiComponent::bindTexture("textures/entity/arrows.png")) {
		for (const auto& a : arrows) {
			glPushMatrix();
			glTranslatef(a.pos.x, a.pos.y, a.pos.z);
			float yawDeg = std::atan2(a.vel.x, a.vel.z) * (180.0f / 3.14159265f);
			float hLen = std::sqrt(a.vel.x * a.vel.x + a.vel.z * a.vel.z);
			float pitchDeg = std::atan2(a.vel.y, hLen) * (180.0f / 3.14159265f);
			glRotatef(yawDeg, 0.0f, 1.0f, 0.0f);
			glRotatef(-pitchDeg, 1.0f, 0.0f, 0.0f);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f); glVertex3f(0.0f, -0.09f, -0.45f);
			glTexCoord2f(0.5f, 0.0f); glVertex3f(0.0f, -0.09f,  0.45f);
			glTexCoord2f(0.5f, 5.0f / 32.0f); glVertex3f(0.0f,  0.09f,  0.45f);
			glTexCoord2f(0.0f, 5.0f / 32.0f); glVertex3f(0.0f,  0.09f, -0.45f);

			glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.09f, 0.0f, -0.45f);
			glTexCoord2f(0.5f, 0.0f); glVertex3f(-0.09f, 0.0f,  0.45f);
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
