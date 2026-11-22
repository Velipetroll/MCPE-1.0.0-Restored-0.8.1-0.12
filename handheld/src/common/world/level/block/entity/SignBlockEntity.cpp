#include "common_header.h"
#include "world/level/block/entity/SignBlockEntity.h"
#include "world/level/BlockSource.h"
#include "world/level/block/Block.h"
#include "nbt/CompoundTag.h"

namespace {
std::string clampUtf8(const std::string& s, size_t maxChars) {
	size_t chars = 0;
	size_t i = 0;
	while (i < s.size() && chars < maxChars) {
		unsigned char c = static_cast<unsigned char>(s[i]);
		if ((c & 0x80) == 0) i += 1;
		else if ((c & 0xE0) == 0xC0) i += 2;
		else if ((c & 0xF0) == 0xE0) i += 3;
		else i += 4;
		++chars;
	}
	if (i > s.size()) i = s.size();
	return s.substr(0, i);
}
}

SignBlockEntity::SignBlockEntity(const BlockPos& pos)
	: BlockEntity(BlockEntityType::Sign, pos, "Sign") {
	mRendererId = TR_SIGN_RENDERER;
}

void SignBlockEntity::setMessage(int line, const std::string& msg) {
	if (line >= 0 && line < NUM_LINES) {
		mTextLines[line] = clampUtf8(msg, MAX_LINE_LENGTH);
	}
}

void SignBlockEntity::load(const CompoundTag& tag) {
	mIsEditable = false;
	BlockEntity::load(tag);
	mTextLines[0] = clampUtf8(tag.getString("Text1"), MAX_LINE_LENGTH);
	mTextLines[1] = clampUtf8(tag.getString("Text2"), MAX_LINE_LENGTH);
	mTextLines[2] = clampUtf8(tag.getString("Text3"), MAX_LINE_LENGTH);
	mTextLines[3] = clampUtf8(tag.getString("Text4"), MAX_LINE_LENGTH);
}

bool SignBlockEntity::save(CompoundTag& tag) {
	if (BlockEntity::save(tag)) {
		tag.putString("Text1", mTextLines[0]);
		tag.putString("Text2", mTextLines[1]);
		tag.putString("Text3", mTextLines[2]);
		tag.putString("Text4", mTextLines[3]);
		return true;
	}
	return false;
}

void SignBlockEntity::onUpdatePacket(const CompoundTag& data, BlockSource& region) {
	load(data);
}

float SignBlockEntity::getShadowRadius(BlockSource& region) const {
	if (region.getBlockID(mPosition) == Block::mSign->mID) {
		return 0.2f;
	}
	return 0.0f;
}
