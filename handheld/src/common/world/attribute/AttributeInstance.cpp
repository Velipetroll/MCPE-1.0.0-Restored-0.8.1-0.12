/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/attribute/AttributeInstance.h"
#include "world/attribute/AttributeMap.h"

AttributeInstance::AttributeInstance() = default;

AttributeInstance::AttributeInstance(BaseAttributeMap* attributeMap, const Attribute* attribute)
	: mAttributeMap(attributeMap)
	, mAttribute(attribute) {}

float AttributeInstance::getMinValue() const {
	return mCurrentValues[enum_cast(AttributeOperands::OPERAND_MIN)];
}

float AttributeInstance::getMaxValue() const {
	return mCurrentValues[enum_cast(AttributeOperands::OPERAND_MAX)];
}

float AttributeInstance::getCurrentValue() const {
	return mCurrentValues[enum_cast(AttributeOperands::OPERAND_CURRENT)];
}

float AttributeInstance::getDefaultValue(int operand) const {
	if (operand < 0 || operand >= enum_cast(AttributeOperands::TOTAL_OPERANDS)) {
		return 0.0f;
	}
	return mDefaultValues[operand];
}

void AttributeInstance::setDefaultValue(float defaultValue, int operand) {
	if (operand < 0 || operand >= enum_cast(AttributeOperands::TOTAL_OPERANDS)) {
		return;
	}
	mDefaultValues[operand] = defaultValue;
	mBaseValues[operand] = defaultValue;
	_recalculate();
}

void AttributeInstance::setRange(float min, float defaultVal, float max) {
	constexpr int minOp = enum_cast(AttributeOperands::OPERAND_MIN);
	constexpr int maxOp = enum_cast(AttributeOperands::OPERAND_MAX);
	constexpr int curOp = enum_cast(AttributeOperands::OPERAND_CURRENT);

	mDefaultValues[minOp] = min;
	mDefaultValues[maxOp] = max;
	mDefaultValues[curOp] = defaultVal;

	mBaseValues[minOp] = min;
	mBaseValues[maxOp] = max;
	mBaseValues[curOp] = defaultVal;

	_recalculate();
}

void AttributeInstance::setMaxValue(float max) {
	constexpr int maxOp = enum_cast(AttributeOperands::OPERAND_MAX);
	constexpr int curOp = enum_cast(AttributeOperands::OPERAND_CURRENT);

	mDefaultValues[maxOp] = max;
	mBaseValues[maxOp] = max;
	if (mBaseValues[curOp] > max) {
		mBaseValues[curOp] = max;
	}
	_recalculate();
}

void AttributeInstance::resetToDefaultValue() {
	constexpr int curOp = enum_cast(AttributeOperands::OPERAND_CURRENT);
	mBaseValues[curOp] = mDefaultValues[curOp];
	_recalculate();
}

void AttributeInstance::resetToMaxValue() {
	constexpr int curOp = enum_cast(AttributeOperands::OPERAND_CURRENT);
	constexpr int maxOp = enum_cast(AttributeOperands::OPERAND_MAX);
	mBaseValues[curOp] = mCurrentValues[maxOp];
	_recalculate();
}

void AttributeInstance::resetToMinValue() {
	constexpr int curOp = enum_cast(AttributeOperands::OPERAND_CURRENT);
	constexpr int minOp = enum_cast(AttributeOperands::OPERAND_MIN);
	mBaseValues[curOp] = mCurrentValues[minOp];
	_recalculate();
}

void AttributeInstance::serializationSetValue(float currentValue, int operand, float maxValue) {
	if (operand < 0 || operand >= enum_cast(AttributeOperands::TOTAL_OPERANDS)) {
		return;
	}
	if (maxValue > 0.0f && operand == enum_cast(AttributeOperands::OPERAND_CURRENT)) {
		mDefaultValues[enum_cast(AttributeOperands::OPERAND_MAX)] = maxValue;
		mBaseValues[enum_cast(AttributeOperands::OPERAND_MAX)] = maxValue;
	}
	mBaseValues[operand] = currentValue;
	if (operand != enum_cast(AttributeOperands::OPERAND_CURRENT)) {
		mDefaultValues[operand] = currentValue;
	}
	_recalculate();
}

void AttributeInstance::serializationSetRange(float min, float base, float max) {
	setRange(min, base, max);
}

float AttributeInstance::_sanitizeValue(float value) const {
	float minV = mCurrentValues[enum_cast(AttributeOperands::OPERAND_MIN)];
	float maxV = mCurrentValues[enum_cast(AttributeOperands::OPERAND_MAX)];
	if (value < minV) value = minV;
	if (value > maxV) value = maxV;
	return value;
}

float AttributeInstance::_calculateValue(const AttributeBuff& buff) const {
	float delta = mDelegate ? mDelegate->getBuffValue(buff) : buff.getAmount();
	int op = buff.getOperand();
	if (op == enum_cast(AttributeOperands::OPERAND_CURRENT)) {
		return _sanitizeValue(mBaseValues[op] + delta);
	}
	return mBaseValues[op] + delta;
}

void AttributeInstance::addBuff(const AttributeBuff& buff) {
	if (!buff.isInstantaneous()) {
		return;
	}
	int op = buff.getOperand();
	if (op < 0 || op >= enum_cast(AttributeOperands::TOTAL_OPERANDS)) {
		return;
	}

	float oldCurrent = mCurrentValues[enum_cast(AttributeOperands::OPERAND_CURRENT)];
	float oldBase = mBaseValues[op];
	float delta = mDelegate ? mDelegate->getBuffValue(buff) : buff.getAmount();

	mBaseValues[op] += delta;
	if (op == enum_cast(AttributeOperands::OPERAND_CURRENT)) {
		mBaseValues[op] = _sanitizeValue(mBaseValues[op]);
	}
	_recalculate();

	float newCurrent = mCurrentValues[enum_cast(AttributeOperands::OPERAND_CURRENT)];
	if (mDelegate && !mDelegate->change(oldCurrent, newCurrent, buff.getInfo())) {
		mBaseValues[op] = oldBase;
		_recalculate();
		return;
	}

	if (oldCurrent != mCurrentValues[enum_cast(AttributeOperands::OPERAND_CURRENT)] && mAttributeMap) {
		mAttributeMap->onAttributeModified(*this);
	}
}

void AttributeInstance::addBuff(std::shared_ptr<AttributeBuff> buff) {
	if (buff) {
		addBuff(*buff);
	}
}

void AttributeInstance::removeBuff(std::shared_ptr<AttributeBuff> buff) {
	UNUSED_PARAMETER(buff);
}

void AttributeInstance::addModifier(const AttributeModifier& modifier) {
	addModifier(std::make_shared<AttributeModifier>(modifier));
}

void AttributeInstance::addModifier(std::shared_ptr<AttributeModifier> modifier) {
	if (!modifier || hasModifier(modifier)) {
		return;
	}
	mModifierList.push_back(std::move(modifier));
	_recalculate();
	if (mAttributeMap) {
		mAttributeMap->onAttributeModified(*this);
	}
}

void AttributeInstance::removeModifier(const AttributeModifier& modifier) {
	removeModifier(modifier.getId());
}

void AttributeInstance::removeModifier(const std::shared_ptr<AttributeModifier>& modifier) {
	if (modifier) {
		removeModifier(modifier->getId());
	}
}

void AttributeInstance::removeModifier(const mce::UUID& id) {
	auto it = std::remove_if(mModifierList.begin(), mModifierList.end(),
		[&](const std::shared_ptr<AttributeModifier>& m) {
			return m && m->getId() == id;
		});
	if (it != mModifierList.end()) {
		mModifierList.erase(it, mModifierList.end());
		_recalculate();
		if (mAttributeMap) {
			mAttributeMap->onAttributeModified(*this);
		}
	}
}

bool AttributeInstance::hasModifier(const AttributeModifier& modifier) const {
	for (const auto& m : mModifierList) {
		if (m && m->getId() == modifier.getId()) {
			return true;
		}
	}
	return false;
}

bool AttributeInstance::hasModifier(const std::shared_ptr<AttributeModifier>& modifier) const {
	if (!modifier) {
		return false;
	}
	return hasModifier(*modifier);
}

void AttributeInstance::setDelegate(std::shared_ptr<AttributeInstanceDelegate> delegate) {
	mDelegate = std::move(delegate);
}

void AttributeInstance::registerListener(const AttributeInstance& listener) {
	mListeners.push_back(listener.getHandle());
}

void AttributeInstance::tick() {
	if (mDelegate) {
		mDelegate->tick();
	}
}

void AttributeInstance::notify(int64_t type) {
	if (mDelegate) {
		mDelegate->notify(type);
	}
}

AttributeInstanceHandle AttributeInstance::getHandle() const {
	return AttributeInstanceHandle(mAttributeMap, mAttribute);
}

void AttributeInstance::_recalculate() {
	for (int op = 0; op < enum_cast(AttributeOperands::TOTAL_OPERANDS); ++op) {
		float base = mBaseValues[op];
		float val = base;

		for (const auto& mod : mModifierList) {
			if (mod && mod->getOperand() == op &&
			    mod->getOperation() == enum_cast(AttributeModifierOperation::OPERATION_ADDITION)) {
				val += mod->getAmount();
			}
		}

		float multBase = val;
		for (const auto& mod : mModifierList) {
			if (mod && mod->getOperand() == op &&
			    mod->getOperation() == enum_cast(AttributeModifierOperation::OPERATION_MULTIPLY_BASE)) {
				val += multBase * mod->getAmount();
			}
		}

		for (const auto& mod : mModifierList) {
			if (mod && mod->getOperand() == op &&
			    mod->getOperation() == enum_cast(AttributeModifierOperation::OPERATION_MULTIPLY_TOTAL)) {
				val *= (1.0f + mod->getAmount());
			}
		}

		mCurrentValues[op] = val;
	}

	constexpr int curOp = enum_cast(AttributeOperands::OPERAND_CURRENT);
	mCurrentValues[curOp] = _sanitizeValue(mCurrentValues[curOp]);
}
