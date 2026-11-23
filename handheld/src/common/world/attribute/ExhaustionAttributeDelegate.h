/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include "world/attribute/AttributeInstanceDelegate.h"

class ExhaustionAttributeDelegate : public AttributeInstanceDelegate {
public:
	explicit ExhaustionAttributeDelegate(const AttributeInstance& instance);

	void tick() override;
};
