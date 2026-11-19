#pragma once

#include "world/item/ItemInstance.h"
#include "world/phys/AABB.h"
#include "world/phys/Vec3.h"
#include <functional>
#include <glm/glm.hpp>
#include <vector>

class BlockSource;

enum class MobType {
	Pig,
	Cow,
	Sheep,
	Chicken,
	Zombie,
	Creeper,
	Skeleton
};

struct WorldMobEntity {
	int id = 0;
	MobType type = MobType::Pig;
	Vec3 pos = Vec3::ZERO; // feet center (x, minY, z)
	Vec3 vel = Vec3::ZERO;
	float yaw = 0.0f;
	float walkDist = 0.0f;
	float walkSpeedVis = 0.0f;
	int health = 10;
	int maxHealth = 10;
	float hurtTimer = 0.0f;
	float attackCooldown = 0.0f;
	float aiTimer = 0.0f;
	float moveDirX = 0.0f;
	float moveDirZ = 0.0f;
	float panicTimer = 0.0f;
	float fuseTimer = 0.0f;
	float burnTimer = 0.0f;
	bool isBurning = false;
	bool onGround = false;
	bool inWater = false;

	bool isHostile() const;
	float halfWidth() const;
	float height() const;
	AABB getAABB() const;
};

struct WorldArrowEntity {
	Vec3 pos = Vec3::ZERO;
	Vec3 vel = Vec3::ZERO;
	float age = 0.0f;
	bool stuck = false;
	int damage = 3;
};

class MobManager {
public:
	std::vector<WorldMobEntity> mobs;
	std::vector<WorldArrowEntity> arrows;

	void clear();

	void update(float dt,
	            BlockSource& region,
	            const Vec3& playerEyePos,
	            const AABB& playerBB,
	            float sunFactor,
	            bool isCreative,
	            bool allowCombat,
	            const std::function<void(int dmg, const Vec3& knockDir)>& onDamagePlayer,
	            const std::function<void(const Vec3& pos, float radius)>& onExplosion,
	            const std::function<void(const ItemInstance& item, const Vec3& pos)>& onDropItem);

	int raycastMob(const Vec3& rayOrigin, const Vec3& rayDir, float maxDist, float& outDist) const;

	bool damageMob(int index,
	               int damage,
	               const Vec3& knockDir,
	               const std::function<void(const ItemInstance& item, const Vec3& pos)>& onDropItem);

	void render(BlockSource& region,
	            const glm::mat4& projection,
	            const glm::mat4& view,
	            float sunFactor) const;

private:
	int mNextId = 1;
	float mSpawnTimer = 1.0f;

	void trySpawnMobs(BlockSource& region, const Vec3& playerPos, float sunFactor);
	void spawnDropsForMob(const WorldMobEntity& mob,
	                      const std::function<void(const ItemInstance& item, const Vec3& pos)>& onDropItem) const;
};
