/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include "Core/Utility/UUID.h"
#include "world/attribute/Attribute.h"

enum class AttributeModifierOperation : int {
	OPERATION_ADDITION = 0,
	OPERATION_MULTIPLY_BASE = 1,
	OPERATION_MULTIPLY_TOTAL = 2,
	TOTAL_OPERATIONS = 3,
	OPERATION_INVALID = TOTAL_OPERATIONS
};

class AttributeModifier {
public:
	AttributeModifier();
	AttributeModifier(
		const mce::UUID& id,
		const std::string& name,
		float amount,
		int operation = enum_cast(AttributeModifierOperation::OPERATION_ADDITION),
		int operand = enum_cast(AttributeOperands::OPERAND_CURRENT),
		bool serialize = true);
	virtual ~AttributeModifier() = default;

	const mce::UUID& getId() const;
	const std::string& getName() const;
	float getAmount() const;
	int getOperation() const;
	int getOperand() const;
	bool isSerializable() const;
	void setSerialize(bool serialize);

	bool operator==(const AttributeModifier& rhs) const;
	bool operator!=(const AttributeModifier& rhs) const;

protected:
	float mAmount = 0.0f;
	int mOperation = enum_cast(AttributeModifierOperation::OPERATION_ADDITION);
	int mOperand = enum_cast(AttributeOperands::OPERAND_CURRENT);
	std::string mName;
	mce::UUID mId;
	bool mSerialize = true;
};
