/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#pragma once

#include "world/item/Item.h"

class ChorusFruit : public Item {
public:
	ChorusFruit(const std::string& nameId, short id);

	ItemInstance& use(ItemInstance& instance, Player& player) override;
	void useTimeDepleted(ItemInstance* inoutInstance, Level* level, Player* player) override;
};
