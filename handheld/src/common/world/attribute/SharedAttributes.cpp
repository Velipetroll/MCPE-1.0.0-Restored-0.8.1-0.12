/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/attribute/SharedAttributes.h"
#include "world/attribute/AttributeMap.h"
#include "nbt/CompoundTag.h"
#include "nbt/ListTag.h"

const Attribute SharedAttributes::HEALTH = Attribute("minecraft:health", RedefinitionMode::KeepCurrent, true);
const Attribute SharedAttributes::ABSORPTION = Attribute("minecraft:absorption", RedefinitionMode::KeepCurrent, true);
const Attribute SharedAttributes::KNOCKBACK_RESISTANCE = Attribute("minecraft:knockback_resistance", RedefinitionMode::KeepCurrent, false);
const Attribute SharedAttributes::MOVEMENT_SPEED = Attribute("minecraft:movement", RedefinitionMode::KeepCurrent, true);
const Attribute SharedAttributes::LUCK = Attribute("minecraft:luck", RedefinitionMode::KeepCurrent, true);
const Attribute SharedAttributes::FALL_DAMAGE = Attribute("minecraft:fall_damage", RedefinitionMode::KeepCurrent, false);
const Attribute SharedAttributes::FOLLOW_RANGE = Attribute("minecraft:follow_range", RedefinitionMode::KeepCurrent, false);
const Attribute SharedAttributes::ATTACK_DAMAGE = Attribute("minecraft:attack_damage", RedefinitionMode::KeepCurrent, false);

const std::shared_ptr<AttributeModifier> SharedModifiers::SPRINTING_BOOST = std::make_shared<AttributeModifier>(
	mce::UUID::fromString("D208FC00-42AA-4AAD-9276-D5446530DE43"),
	"Sprinting speed boost",
	0.3f,
	enum_cast(AttributeModifierOperation::OPERATION_MULTIPLY_TOTAL),
	enum_cast(AttributeOperands::OPERAND_CURRENT),
	false
);

std::unique_ptr<ListTag> SharedAttributes::saveAttributes(BaseAttributeMap* attributes) {
	auto list = make_unique<ListTag>();
	if (!attributes) {
		return list;
	}
	for (const auto& kv : *attributes) {
		const AttributeInstance& inst = kv.second;
		if (!inst.getAttribute()) continue;
		auto tag = make_unique<CompoundTag>();
		tag->putString("Name", inst.getAttribute()->getName());
		tag->putFloat("Base", inst.getDefaultValue(enum_cast(AttributeOperands::OPERAND_CURRENT)));
		tag->putFloat("Current", inst.getCurrentValue());
		tag->putFloat("Max", inst.getMaxValue());
		list->add(std::move(tag));
	}
	return list;
}

void SharedAttributes::loadAttributes(BaseAttributeMap* attributes, const ListTag* list) {
	if (!attributes || !list) {
		return;
	}
	for (int i = 0; i < list->size(); ++i) {
		const Tag* raw = list->get(i);
		if (!raw || raw->getId() != Tag::Type::Compound) continue;
		const CompoundTag* tag = static_cast<const CompoundTag*>(raw);
		std::string name = tag->getString("Name");
		if (name.empty() || !AttributeCollection::hasAttribute(name)) continue;
		if (AttributeInstance* inst = attributes->getMutableInstance(name)) {
			float maxVal = tag->contains("Max") ? tag->getFloat("Max") : 0.0f;
			float curVal = tag->contains("Current") ? tag->getFloat("Current") : tag->getFloat("Base");
			inst->serializationSetValue(curVal, enum_cast(AttributeOperands::OPERAND_CURRENT), maxVal);
		}
	}
}
