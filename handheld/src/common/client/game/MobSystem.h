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
	Skeleton,
	Spider,
	PigZombie
};

struct WorldMobEntity {
	int id = 0;
	MobType type = MobType::Pig;
	Vec3 pos = Vec3::ZERO; // feet center (x, minY, z)
	Vec3 vel = Vec3::ZERO;
	float yaw = 0.0f;       // MCPE body yaw (deg: 0=+Z, -90=+X, 90=-X, 180=-Z)
	float headYaw = 0.0f;   // MCPE head yaw (deg)
	float headPitch = 0.0f; // MCPE head pitch (deg)
	float walkDist = 0.0f;
	float walkAnimPos = 0.0f;
	float walkSpeedVis = 0.0f;
	float age = 0.0f;
	float attackSwing = 0.0f;
	float wingFlap = 0.0f;
	float wingFlapPhase = 0.0f;
	int health = 10;
	int maxHealth = 10;
	float hurtTimer = 0.0f;
	float attackCooldown = 0.0f;
	float aiTimer = 0.0f;
	float lookTimer = 0.0f;
	float lookTargetYaw = 0.0f;
	bool lookingAtPlayer = false;
	float moveDirX = 0.0f;
	float moveDirZ = 0.0f;
	float panicTimer = 0.0f;
	float fuseTimer = 0.0f;
	float burnTimer = 0.0f;
	float stuckTimer = 0.0f;
	Vec3 lastStuckPos = Vec3::ZERO;
	float pathTimer = 0.0f;
	std::vector<Vec3> pathWaypoints;
	size_t pathIndex = 0;
	bool hasTarget = false;
	bool wantsJump = false;
	bool hasActivePath = false;
	float eatTimer = 0.0f;
	float desiredSpeed = 0.0f;
	bool isDying = false;
	float deathTimer = 0.0f;
	float inLoveTimer = 0.0f;
	float heartParticleTimer = 0.0f;
	float breedProgress = 0.0f;
	float breedCooldown = 0.0f;
	bool isBaby = false;
	float babyTimer = 0.0f;
	float milkCooldown = 0.0f;
	bool isBurning = false;
	bool onGround = false;
	bool inWater = false;
	bool isSheared = false;
	uint8_t woolColor = 0;

	bool isHostile() const;
	float halfWidth() const;
	float height() const;
	AABB getAABB() const;
};

struct WorldArrowEntity {
	Vec3 pos = Vec3::ZERO;
	Vec3 vel = Vec3::ZERO;
	float yaw = 0.0f;
	float pitch = 0.0f;
	float age = 0.0f;
	bool stuck = false;
	int damage = 3;
};

class MobManager {
public:
	std::vector<WorldMobEntity> mobs;
	std::vector<WorldArrowEntity> arrows;

	void clear();

	static bool isAnimalFood(MobType type, const ItemInstance& item);

	bool peaceful = false; // Difficulty == Peaceful: no hostile mobs

	void update(float dt,
	            BlockSource& region,
	            const Vec3& playerEyePos,
	            const AABB& playerBB,
	            float sunFactor,
	            bool isCreative,
	            bool allowCombat,
	            const std::function<void(int dmg, const Vec3& knockDir)>& onDamagePlayer,
	            const std::function<void(const Vec3& pos, float radius)>& onExplosion,
	            const std::function<void(const ItemInstance& item, const Vec3& pos)>& onDropItem,
	            const ItemInstance* playerHeld = nullptr,
	            const std::function<void(const Vec3& pos, float hw, float h)>& onDeathPoof = nullptr,
	            const std::function<void(const Vec3& pos, float hw, float h, int count)>& onHearts = nullptr);

	int raycastMob(const Vec3& rayOrigin, const Vec3& rayDir, float maxDist, float& outDist) const;

	bool damageMob(int index,
	               int damage,
	               const Vec3& knockDir,
	               const std::function<void(const ItemInstance& item, const Vec3& pos)>& onDropItem);

	bool shearSheep(int index,
	                const std::function<void(const ItemInstance& item, const Vec3& pos)>& onDropItem);

	bool feedAnimal(int index,
	                const ItemInstance& held,
	                const std::function<void(const Vec3& pos, float hw, float h, int count)>& onHearts);

	bool milkCow(int index);
	bool dyeSheep(int index, int woolColor);
	void removePigZombiesNear(const BlockPos& pos, float radius);

	bool spawnMobAt(MobType type, float x, float y, float z);

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
