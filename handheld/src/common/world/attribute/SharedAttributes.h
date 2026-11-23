/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include "world/attribute/Attribute.h"
#include "world/attribute/AttributeModifier.h"

#include <memory>

class BaseAttributeMap;
class ListTag;

class SharedAttributes {
public:
	static const Attribute HEALTH;
	static const Attribute ABSORPTION;
	static const Attribute KNOCKBACK_RESISTANCE;
	static const Attribute MOVEMENT_SPEED;
	static const Attribute LUCK;
	static const Attribute FALL_DAMAGE;
	static const Attribute FOLLOW_RANGE;
	static const Attribute ATTACK_DAMAGE;

	static std::unique_ptr<ListTag> saveAttributes(BaseAttributeMap* attributes);
	static void loadAttributes(BaseAttributeMap* attributes, const ListTag* tag);
};

class SharedModifiers {
public:
	static const std::shared_ptr<AttributeModifier> SPRINTING_BOOST;
};
