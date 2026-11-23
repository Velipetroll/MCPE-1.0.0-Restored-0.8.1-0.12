/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include "world/attribute/Attribute.h"

enum class EntityDamageCause : int;

enum class AttributeBuffType : int {
	Unknown = 0,
	Magic = 1,
	Injury = 2,
	Heal = 3,
	Harm = 4,
	Poison = 5,
	Saturation = 6,
	Hunger = 7,
	Food = 8,
	None = 9
};

struct AttributeBuffInfo {
	AttributeBuffType mType = AttributeBuffType::Unknown;
	uint64_t mSource = 0;

	AttributeBuffInfo() = default;
	explicit AttributeBuffInfo(AttributeBuffType type, uint64_t source = 0)
		: mType(type)
		, mSource(source) {}
};

class AttributeBuff {
public:
	AttributeBuff(float amount, int operand, AttributeBuffType type);
	AttributeBuff(float amount, int operand, const AttributeBuffInfo& info);
	virtual ~AttributeBuff() = default;

	virtual bool isInstantaneous() const = 0;
	virtual bool isSerializable() const = 0;

	float getAmount() const;
	int getOperand() const;
	AttributeBuffType getType() const;
	const AttributeBuffInfo& getInfo() const;

	static EntityDamageCause buffTypeToDamageCause(AttributeBuffType type);

protected:
	float mAmount = 0.0f;
	AttributeBuffInfo mInfo;
	int mOperand = 0;
};

class InstantaneousAttributeBuff : public AttributeBuff {
public:
	explicit InstantaneousAttributeBuff(float amount, AttributeBuffType type = AttributeBuffType::Food);
	InstantaneousAttributeBuff(float amount, int operand, AttributeBuffType type = AttributeBuffType::Food);

	bool isInstantaneous() const override { return true; }
	bool isSerializable() const override { return false; }
};
