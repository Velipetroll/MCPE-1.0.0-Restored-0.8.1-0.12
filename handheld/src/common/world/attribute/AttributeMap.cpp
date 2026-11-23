/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/attribute/AttributeMap.h"

const AttributeInstance BaseAttributeMap::mInvalidInstance;

AttributeInstance& BaseAttributeMap::registerAttribute(const Attribute& attribute) {
	auto it = mAttributes.find(attribute.getName());
	if (it != mAttributes.end()) {
		return it->second;
	}
	auto inserted = mAttributes.emplace(attribute.getName(), AttributeInstance(this, &attribute));
	return inserted.first->second;
}

AttributeInstance* BaseAttributeMap::getMutableInstance(const Attribute& attribute) {
	return getMutableInstance(attribute.getName());
}

AttributeInstance* BaseAttributeMap::getMutableInstance(const std::string& name) {
	auto it = mAttributes.find(name);
	return (it != mAttributes.end()) ? &it->second : nullptr;
}

const AttributeInstance& BaseAttributeMap::getInstance(const Attribute& attribute) const {
	return getInstance(attribute.getName());
}

const AttributeInstance& BaseAttributeMap::getInstance(const std::string& name) const {
	auto it = mAttributes.find(name);
	return (it != mAttributes.end()) ? it->second : mInvalidInstance;
}

void BaseAttributeMap::onAttributeModified(const AttributeInstance& instance) {
	if (instance.getAttribute() && instance.getAttribute()->isClientSyncable()) {
		mDirtyAttributes.push_back(&instance);
	}
}

void BaseAttributeMap::clearDirtyAttributes() {
	mDirtyAttributes.clear();
}
