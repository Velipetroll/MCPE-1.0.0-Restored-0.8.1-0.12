/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include <cstdint>

class Attribute;
class AttributeInstance;
class BaseAttributeMap;
struct AttributeBuffInfo;
class AttributeBuff;

class AttributeInstanceHandle {
public:
	AttributeInstanceHandle() = default;
	AttributeInstanceHandle(BaseAttributeMap* attributeMap, const Attribute* attribute)
		: mAttributeMap(attributeMap)
		, mAttribute(attribute) {}

	const AttributeInstance& getInstance() const;
	AttributeInstance* getMutableInstance() const;

	bool isValid() const { return mAttributeMap != nullptr && mAttribute != nullptr; }

private:
	BaseAttributeMap* mAttributeMap = nullptr;
	const Attribute* mAttribute = nullptr;
};

class AttributeInstanceDelegate {
public:
	explicit AttributeInstanceDelegate(const AttributeInstance& instance);
	virtual ~AttributeInstanceDelegate() = default;

	virtual void tick() {}
	virtual void notify(int64_t type) { (void)type; }
	virtual bool change(float oldValue, float newValue, const AttributeBuffInfo& buffInfo);
	virtual float getBuffValue(const AttributeBuff& buff) const;

protected:
	const AttributeInstance& _getInstance() const;
	AttributeInstance* _getMutableInstance() const;

	AttributeInstanceHandle mAttributeHandle;
};
