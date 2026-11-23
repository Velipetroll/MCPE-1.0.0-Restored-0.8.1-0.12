/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/attribute/AttributeBuff.h"
#include "world/entity/EntityDamageSource.h"

AttributeBuff::AttributeBuff(float amount, int operand, AttributeBuffType type)
	: mAmount(amount)
	, mInfo(type)
	, mOperand(operand) {}

AttributeBuff::AttributeBuff(float amount, int operand, const AttributeBuffInfo& info)
	: mAmount(amount)
	, mInfo(info)
	, mOperand(operand) {}

float AttributeBuff::getAmount() const {
	return mAmount;
}

int AttributeBuff::getOperand() const {
	return mOperand;
}

AttributeBuffType AttributeBuff::getType() const {
	return mInfo.mType;
}

const AttributeBuffInfo& AttributeBuff::getInfo() const {
	return mInfo;
}

EntityDamageCause AttributeBuff::buffTypeToDamageCause(AttributeBuffType type) {
	switch (type) {
	case AttributeBuffType::Unknown:
		return EntityDamageCause::Starve;
	default:
		return EntityDamageCause::None;
	}
}

InstantaneousAttributeBuff::InstantaneousAttributeBuff(float amount, AttributeBuffType type)
	: AttributeBuff(amount, enum_cast(AttributeOperands::OPERAND_CURRENT), type) {}

InstantaneousAttributeBuff::InstantaneousAttributeBuff(float amount, int operand, AttributeBuffType type)
	: AttributeBuff(amount, operand, type) {}
