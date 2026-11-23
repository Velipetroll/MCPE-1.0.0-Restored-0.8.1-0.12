/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/attribute/AttributeModifier.h"

AttributeModifier::AttributeModifier() = default;

AttributeModifier::AttributeModifier(
	const mce::UUID& id,
	const std::string& name,
	float amount,
	int operation,
	int operand,
	bool serialize)
	: mAmount(amount)
	, mOperation(operation)
	, mOperand(operand)
	, mName(name)
	, mId(id)
	, mSerialize(serialize) {}

const mce::UUID& AttributeModifier::getId() const {
	return mId;
}

const std::string& AttributeModifier::getName() const {
	return mName;
}

float AttributeModifier::getAmount() const {
	return mAmount;
}

int AttributeModifier::getOperation() const {
	return mOperation;
}

int AttributeModifier::getOperand() const {
	return mOperand;
}

bool AttributeModifier::isSerializable() const {
	return mSerialize;
}

void AttributeModifier::setSerialize(bool serialize) {
	mSerialize = serialize;
}

bool AttributeModifier::operator==(const AttributeModifier& rhs) const {
	return mId == rhs.mId;
}

bool AttributeModifier::operator!=(const AttributeModifier& rhs) const {
	return !(*this == rhs);
}
