#include "common_header.h"
#include "world/level/block/entity/ChestBlockEntity.h"
#include "world/level/BlockSource.h"
#include "world/level/block/Block.h"
#include "nbt/CompoundTag.h"
#include "nbt/ListTag.h"

ChestBlockEntity::ChestBlockEntity(BlockEntityType type, const std::string& id, BlockEntityRendererId rendererId, const BlockPos& pos)
	: BlockEntity(type, pos, id) {
	mRendererId = rendererId;
}

void ChestBlockEntity::load(const CompoundTag& tag) {
	BlockEntity::load(tag);
	for (auto& slot : mItems) {
		slot.setNull();
	}
	if (const ListTag* items = tag.getList("Items")) {
		for (int i = 0; i < items->size(); ++i) {
			const Tag* entry = items->get(i);
			if (!entry || entry->getId() != Tag::Type::Compound) continue;
			const auto* itemTag = static_cast<const CompoundTag*>(entry);
			int slot = itemTag->getByte("Slot") & 0xFF;
			if (slot >= 0 && slot < CONTAINER_SIZE) {
				mItems[slot].load(*itemTag);
			}
		}
	}
	if (tag.contains("pairx") && tag.contains("pairz")) {
		mHasPair = true;
		mPairX = tag.getInt("pairx");
		mPairZ = tag.getInt("pairz");
	} else {
		mHasPair = false;
	}
}

bool ChestBlockEntity::save(CompoundTag& tag) {
	if (!BlockEntity::save(tag)) {
		return false;
	}
	auto itemsList = make_unique<ListTag>();
	for (int i = 0; i < CONTAINER_SIZE; ++i) {
		if (!mItems[i].isNull()) {
			if (auto itemTag = mItems[i].save()) {
				itemTag->putByte("Slot", static_cast<char>(i));
				itemsList->add(std::move(itemTag));
			}
		}
	}
	tag.put("Items", std::move(itemsList));
	if (mHasPair) {
		tag.putInt("pairx", mPairX);
		tag.putInt("pairz", mPairZ);
	}
	return true;
}

void ChestBlockEntity::tick(BlockSource& region) {
	BlockEntity::tick(region);
	mOldOpenness = mOpenness;
	const float speed = 0.1f;
	if (mOpenCount > 0) {
		mOpenness = std::min(1.0f, mOpenness + speed);
	} else {
		mOpenness = std::max(0.0f, mOpenness - speed);
	}
}

void ChestBlockEntity::onRemoved(BlockSource& region) {
	for (auto& slot : mItems) {
		if (!slot.isNull()) {
			if (Block::mChest) {
				Block::mChest->popResource(region, mPosition, slot);
			}
			slot.setNull();
		}
	}
	BlockEntity::onRemoved(region);
}

ItemInstance* ChestBlockEntity::getItem(int slot) {
	if (slot < 0 || slot >= CONTAINER_SIZE) return nullptr;
	return &mItems[slot];
}

void ChestBlockEntity::setItem(int slot, const ItemInstance& item) {
	if (slot < 0 || slot >= CONTAINER_SIZE) return;
	mItems[slot] = item;
	setChanged();
}
