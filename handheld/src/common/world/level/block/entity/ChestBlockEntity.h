#pragma once

#include "world/level/block/entity/BlockEntity.h"
#include "world/item/ItemInstance.h"
#include <array>

class ChestBlockEntity : public BlockEntity {
public:
	static constexpr int CONTAINER_SIZE = 27;

	std::array<ItemInstance, CONTAINER_SIZE> mItems;
	bool mHasPair = false;
	int mPairX = 0;
	int mPairZ = 0;
	float mOpenness = 0.0f;
	float mOldOpenness = 0.0f;
	int mOpenCount = 0;

	ChestBlockEntity(BlockEntityType type, const std::string& id, BlockEntityRendererId rendererId, const BlockPos& pos);
	~ChestBlockEntity() override = default;

	void load(const CompoundTag& tag) override;
	bool save(CompoundTag& tag) override;
	void tick(BlockSource& region) override;
	void onRemoved(BlockSource& region) override;

	ItemInstance* getItem(int slot);
	void setItem(int slot, const ItemInstance& item);
	int getContainerSize() const { return CONTAINER_SIZE; }
};
