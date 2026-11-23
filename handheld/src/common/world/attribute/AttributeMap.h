/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include "world/attribute/Attribute.h"
#include "world/attribute/AttributeInstance.h"

#include <unordered_map>
#include <vector>

class BaseAttributeMap {
public:
	using MapType = std::unordered_map<std::string, AttributeInstance>;

	BaseAttributeMap() = default;
	virtual ~BaseAttributeMap() = default;

	AttributeInstance& registerAttribute(const Attribute& attribute);
	AttributeInstance* getMutableInstance(const Attribute& attribute);
	AttributeInstance* getMutableInstance(const std::string& name);
	const AttributeInstance& getInstance(const Attribute& attribute) const;
	const AttributeInstance& getInstance(const std::string& name) const;

	void onAttributeModified(const AttributeInstance& instance);
	void clearDirtyAttributes();
	const std::vector<const AttributeInstance*>& getDirtyAttributes() const { return mDirtyAttributes; }

	MapType::iterator begin() { return mAttributes.begin(); }
	MapType::iterator end() { return mAttributes.end(); }
	MapType::const_iterator begin() const { return mAttributes.begin(); }
	MapType::const_iterator end() const { return mAttributes.end(); }

	static const AttributeInstance mInvalidInstance;

private:
	MapType mAttributes;
	std::vector<const AttributeInstance*> mDirtyAttributes;
};
