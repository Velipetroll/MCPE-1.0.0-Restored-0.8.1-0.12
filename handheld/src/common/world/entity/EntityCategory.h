/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include <cstdint>

enum class EntityCategory : int {
	None = 0,
	Player = 1 << 0,
	Mob = 1 << 1,
	UndeadMob = 1 << 2,
	Monster = 1 << 3,
	Animal = 1 << 4,
	WaterAnimal = 1 << 5,
	TamableAnimal = 1 << 6,
	Ambient = 1 << 7,
	PathfinderMob = 1 << 8,
	BoatRideable = 1 << 9
};

inline EntityCategory operator|(EntityCategory a, EntityCategory b) {
	return static_cast<EntityCategory>(static_cast<int>(a) | static_cast<int>(b));
}

inline EntityCategory& operator|=(EntityCategory& a, EntityCategory b) {
	a = a | b;
	return a;
}

inline EntityCategory operator&(EntityCategory a, EntityCategory b) {
	return static_cast<EntityCategory>(static_cast<int>(a) & static_cast<int>(b));
}
