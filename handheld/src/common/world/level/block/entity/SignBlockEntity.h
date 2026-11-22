#pragma once

#include "world/level/block/entity/BlockEntity.h"
#include <string>

class SignBlockEntity : public BlockEntity {
public:
	static constexpr int NUM_LINES = 4;
	static constexpr int MAX_LINE_LENGTH = 15;

	std::string mTextLines[NUM_LINES];
	int mSelectedLine = -1;
	bool mIsEditable = true;

	explicit SignBlockEntity(const BlockPos& pos);
	~SignBlockEntity() override = default;

	bool isEditable() const { return mIsEditable; }
	void setEditable(bool editable) { mIsEditable = editable; }

	const std::string& getMessage(int line) const { return mTextLines[line]; }
	void setMessage(int line, const std::string& msg);

	void load(const CompoundTag& tag) override;
	bool save(CompoundTag& tag) override;
	void onUpdatePacket(const CompoundTag& data, BlockSource& region) override;
	float getShadowRadius(BlockSource& region) const override;
};
