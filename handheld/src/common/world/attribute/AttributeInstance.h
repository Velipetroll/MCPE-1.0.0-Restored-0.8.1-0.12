/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include "world/attribute/Attribute.h"
#include "world/attribute/AttributeBuff.h"
#include "world/attribute/AttributeInstanceDelegate.h"
#include "world/attribute/AttributeModifier.h"

#include <limits>
#include <memory>
#include <vector>

class BaseAttributeMap;

class AttributeInstance {
public:
	AttributeInstance();
	AttributeInstance(BaseAttributeMap* attributeMap, const Attribute* attribute);
	~AttributeInstance() = default;

	bool isValid() const { return mAttribute != nullptr; }
	const Attribute* getAttribute() const { return mAttribute; }

	float getMinValue() const;
	float getMaxValue() const;
	float getCurrentValue() const;
	float getDefaultValue(int operand) const;

	void setDefaultValue(float defaultValue, int operand);
	void setRange(float min, float defaultVal, float max);
	void setMaxValue(float max);

	void resetToDefaultValue();
	void resetToMaxValue();
	void resetToMinValue();

	void serializationSetValue(float currentValue, int operand, float maxValue = 0.0f);
	void serializationSetRange(float min, float base, float max);

	void addBuff(const AttributeBuff& buff);
	void addBuff(std::shared_ptr<AttributeBuff> buff);
	void removeBuff(std::shared_ptr<AttributeBuff> buff);
	const std::vector<AttributeBuffInfo>& getBuffs() const { return mBuffs; }

	void addModifier(const AttributeModifier& modifier);
	void addModifier(std::shared_ptr<AttributeModifier> modifier);
	void removeModifier(const AttributeModifier& modifier);
	void removeModifier(const std::shared_ptr<AttributeModifier>& modifier);
	void removeModifier(const mce::UUID& id);
	bool hasModifier(const AttributeModifier& modifier) const;
	bool hasModifier(const std::shared_ptr<AttributeModifier>& modifier) const;
	const std::vector<std::shared_ptr<AttributeModifier>>& getModifiers() const { return mModifierList; }

	void setDelegate(std::shared_ptr<AttributeInstanceDelegate> delegate);
	void registerListener(const AttributeInstance& listener);
	const std::vector<AttributeInstanceHandle>& getListeners() const { return mListeners; }

	void tick();
	void notify(int64_t type);

	AttributeInstanceHandle getHandle() const;

private:
	float _calculateValue(const AttributeBuff& buff) const;
	float _sanitizeValue(float value) const;
	void _recalculate();

	BaseAttributeMap* mAttributeMap = nullptr;
	const Attribute* mAttribute = nullptr;
	std::shared_ptr<AttributeInstanceDelegate> mDelegate;

	std::vector<AttributeInstanceHandle> mListeners;
	std::vector<std::shared_ptr<AttributeModifier>> mModifierList;
	std::vector<AttributeBuffInfo> mBuffs;

	float mDefaultValues[enum_cast(AttributeOperands::TOTAL_OPERANDS)] = {
		0.0f,
		std::numeric_limits<float>::max(),
		0.0f
	};
	float mBaseValues[enum_cast(AttributeOperands::TOTAL_OPERANDS)] = {
		0.0f,
		std::numeric_limits<float>::max(),
		0.0f
	};
	float mCurrentValues[enum_cast(AttributeOperands::TOTAL_OPERANDS)] = {
		0.0f,
		std::numeric_limits<float>::max(),
		0.0f
	};
};
