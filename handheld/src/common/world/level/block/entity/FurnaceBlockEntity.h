#pragma once

#include "world/level/block/entity/BlockEntity.h"
#include "world/item/ItemInstance.h"
#include <array>

class FurnaceBlockEntity : public BlockEntity {
public:
	static constexpr int SLOT_INPUT = 0;
	static constexpr int SLOT_FUEL = 1;
	static constexpr int SLOT_RESULT = 2;
	static constexpr int CONTAINER_SIZE = 3;
	static constexpr int BURN_INTERVAL = 200;

	std::array<ItemInstance, CONTAINER_SIZE> mItems;
	int mLitTime = 0;
	int mLitDuration = 0;
	int mCookingProgress = 0;

	explicit FurnaceBlockEntity(const BlockPos& pos);
	~FurnaceBlockEntity() override = default;

	static int getBurnDuration(const ItemInstance& item);
	static bool isFuel(const ItemInstance& item);

	bool isLit() const { return mLitTime > 0; }
	int getBurnProgress(int maxWidth) const { return mCookingProgress * maxWidth / BURN_INTERVAL; }
	int getLitProgress(int maxHeight) const;

	void load(const CompoundTag& tag) override;
	bool save(CompoundTag& tag) override;
	void tick(BlockSource& region) override;
	void onRemoved(BlockSource& region) override;

	ItemInstance* getItem(int slot);
	void setItem(int slot, const ItemInstance& item);
};
