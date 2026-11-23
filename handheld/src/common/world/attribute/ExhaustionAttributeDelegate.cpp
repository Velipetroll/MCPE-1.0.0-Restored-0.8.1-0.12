/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "world/attribute/ExhaustionAttributeDelegate.h"
#include "world/attribute/AttributeBuff.h"
#include "world/attribute/AttributeInstance.h"
#include "world/food/FoodConstants.h"

ExhaustionAttributeDelegate::ExhaustionAttributeDelegate(const AttributeInstance& instance)
	: AttributeInstanceDelegate(instance) {}

void ExhaustionAttributeDelegate::tick() {
	AttributeInstance* inst = _getMutableInstance();
	if (!inst) {
		return;
	}

	float exhaustion = inst->getCurrentValue();
	float maxExh = FoodConstants::MAX_EXHAUSTION;
	if (exhaustion >= maxExh) {
		InstantaneousAttributeBuff buff(-maxExh, AttributeBuffType::Food);
		inst->addBuff(buff);

		for (const AttributeInstanceHandle& handle : inst->getListeners()) {
			if (AttributeInstance* listener = handle.getMutableInstance()) {
				listener->notify(0LL);
			}
		}
	}
}
