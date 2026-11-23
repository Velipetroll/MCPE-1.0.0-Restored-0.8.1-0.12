/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

class Entity;

enum class EntityDamageCause : int {
	None = -1,
	Override = 0,
	Contact = 1,
	EntityAttack = 2,
	Projectile = 3,
	Suffocation = 4,
	Fall = 5,
	Fire = 6,
	FireTick = 7,
	Lava = 8,
	Drowning = 9,
	BlockExplosion = 10,
	EntityExplosion = 11,
	Void = 12,
	Suicide = 13,
	Magic = 14,
	Wither = 15,
	Starve = 16,
	Anvil = 17,
	Thorns = 18,
	FallingBlock = 19,
	Piston = 20,
	FlyIntoWall = 21,
	All = 31
};

class EntityDamageSource {
public:
	explicit EntityDamageSource(EntityDamageCause cause)
		: mCause(cause) {}
	virtual ~EntityDamageSource() = default;

	virtual bool isEntitySource() const { return false; }
	virtual bool isChildEntitySource() const { return false; }
	virtual Entity* getEntity() const { return nullptr; }
	virtual Entity* getDamagingEntity() const { return nullptr; }

	EntityDamageCause getCause() const { return mCause; }

protected:
	EntityDamageCause mCause = EntityDamageCause::None;
};

class EntityDamageByEntitySource : public EntityDamageSource {
public:
	EntityDamageByEntitySource(Entity& entity, EntityDamageCause cause)
		: EntityDamageSource(cause)
		, mEntity(&entity) {}

	bool isEntitySource() const override { return mEntity != nullptr; }
	Entity* getEntity() const override { return mEntity; }
	Entity* getDamagingEntity() const override { return mEntity; }

protected:
	Entity* mEntity = nullptr;
};
