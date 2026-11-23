/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/attribute/AttributeInstanceDelegate.h"
#include "world/attribute/AttributeBuff.h"
#include "world/attribute/AttributeInstance.h"
#include "world/attribute/AttributeMap.h"

const AttributeInstance& AttributeInstanceHandle::getInstance() const {
	return mAttributeMap->getInstance(*mAttribute);
}

AttributeInstance* AttributeInstanceHandle::getMutableInstance() const {
	return mAttributeMap ? mAttributeMap->getMutableInstance(*mAttribute) : nullptr;
}

AttributeInstanceDelegate::AttributeInstanceDelegate(const AttributeInstance& instance)
	: mAttributeHandle(instance.getHandle()) {}

bool AttributeInstanceDelegate::change(float oldValue, float newValue, const AttributeBuffInfo& buffInfo) {
	UNUSED_PARAMETER(oldValue, newValue, buffInfo);
	return true;
}

float AttributeInstanceDelegate::getBuffValue(const AttributeBuff& buff) const {
	return buff.getAmount();
}

const AttributeInstance& AttributeInstanceDelegate::_getInstance() const {
	return mAttributeHandle.getInstance();
}

AttributeInstance* AttributeInstanceDelegate::_getMutableInstance() const {
	return mAttributeHandle.getMutableInstance();
}
