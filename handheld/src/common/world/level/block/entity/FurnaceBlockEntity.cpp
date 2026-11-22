#include "common_header.h"
#include "world/level/block/entity/FurnaceBlockEntity.h"
#include "world/level/BlockSource.h"
#include "world/level/block/Block.h"
#include "world/level/block/FurnaceBlock.h"
#include "world/level/material/Material.h"
#include "world/item/Item.h"
#include "nbt/CompoundTag.h"
#include "nbt/ListTag.h"

FurnaceBlockEntity::FurnaceBlockEntity(const BlockPos& pos)
	: BlockEntity(BlockEntityType::Furnace, pos, "Furnace") {
}

int FurnaceBlockEntity::getBurnDuration(const ItemInstance& item) {
	if (item.isNull()) {
		return 0;
	}
	int id = item.getId();
	if (id > 0 && id <= 255 && Block::mBlocks[id] && Block::mBlocks[id]->getMaterial().isType(MaterialType::Wood)) {
		return 300;
	}
	if (Item::mStick && id == Item::mStick->getId()) {
		return 100;
	}
	if (Item::mCoal && id == Item::mCoal->getId()) {
		return 1600;
	}
	if (Item::mBucket && id == Item::mBucket->getId() &&
	    (item.getAuxValue() == Block::mFlowingLava->mID || item.getAuxValue() == Block::mStillLava->mID)) {
		return 20000;
	}
	if (Block::mSapling && id == Block::mSapling->mID) {
		return 100;
	}
	if (Block::mCoalBlock && id == Block::mCoalBlock->mID) {
		return 16000;
	}
	if ((Item::mBow && id == Item::mBow->getId()) ||
	    (Item::mBowl && id == Item::mBowl->getId()) ||
	    (Item::mShovel_wood && id == Item::mShovel_wood->getId()) ||
	    (Item::mHoe_wood && id == Item::mHoe_wood->getId()) ||
	    (Item::mPickAxe_wood && id == Item::mPickAxe_wood->getId()) ||
	    (Item::mSword_wood && id == Item::mSword_wood->getId())) {
		return 200;
	}
	return 0;
}

bool FurnaceBlockEntity::isFuel(const ItemInstance& item) {
	return getBurnDuration(item) > 0;
}

int FurnaceBlockEntity::getLitProgress(int maxHeight) const {
	int dur = mLitDuration > 0 ? mLitDuration : BURN_INTERVAL;
	int r = mLitTime * maxHeight / dur;
	return (r >= maxHeight) ? maxHeight : r;
}

void FurnaceBlockEntity::load(const CompoundTag& tag) {
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
	mLitTime = tag.getShort("BurnTime");
	mCookingProgress = tag.getShort("CookTime");
	mLitDuration = getBurnDuration(mItems[SLOT_FUEL]);
}

bool FurnaceBlockEntity::save(CompoundTag& tag) {
	if (!BlockEntity::save(tag)) {
		return false;
	}
	tag.putShort("BurnTime", static_cast<short>(mLitTime));
	tag.putShort("CookTime", static_cast<short>(mCookingProgress));
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
	return true;
}

void FurnaceBlockEntity::tick(BlockSource& region) {
	BlockEntity::tick(region);
	if (mLitTime > 0) {
		--mLitTime;
	}
}

void FurnaceBlockEntity::onRemoved(BlockSource& region) {
	for (auto& slot : mItems) {
		if (!slot.isNull()) {
			if (Block::mFurnace) {
				Block::mFurnace->popResource(region, mPosition, slot);
			}
			slot.setNull();
		}
	}
	BlockEntity::onRemoved(region);
}

ItemInstance* FurnaceBlockEntity::getItem(int slot) {
	if (slot < 0 || slot >= CONTAINER_SIZE) return nullptr;
	return &mItems[slot];
}

void FurnaceBlockEntity::setItem(int slot, const ItemInstance& item) {
	if (slot < 0 || slot >= CONTAINER_SIZE) return;
	mItems[slot] = item;
	setChanged();
}
