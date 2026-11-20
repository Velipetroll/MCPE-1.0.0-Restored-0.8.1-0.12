/********************************************************
*  (c) Mojang.    All rights reserved.                  *
*  (c) Microsoft. All rights reserved.                  *
*********************************************************/
#pragma once

#include "world/level/BlockPos.h"
#include "world/level/LevelConstants.h"
class Block;
class BlockSource;

class BlockTessellatorCache {
public:
	static const int CACHE_SIDE_X = RENDERCHUNK_SIDE + 4;
	static const int CACHE_SIDE_Y = RENDERCHUNK_SIDE + 4;
	static const int CACHE_SIDE_Z = RENDERCHUNK_SIDE + 4;

	void reset(BlockSource& region, const BlockPos& startPostion);
	BrightnessPair getLightColor(const BlockPos& p);
	const Block& getBlock(const BlockPos& p);
	DataID getData(const BlockPos& pos);
	BlockSource* getRegion() {
		return mRegion;
	}

	void setBlockAtPosition(const Block& block, BlockPos& p);

private:
	Brightness _getLightEmission(const Block& block);
	inline int _getIndexFromPositionInArray(const BlockPos& p) const {
		return ((p.x - pos.x) * CACHE_SIDE_Y * CACHE_SIDE_Z) + ((p.y - pos.y) * CACHE_SIDE_Z) + (p.z - pos.z);
	}

	BlockPos pos = BlockPos::ZERO;
	BlockSource* mRegion = nullptr;
	std::array<BrightnessPair, CACHE_SIDE_X * CACHE_SIDE_Y * CACHE_SIDE_Z> mLightColors;
	std::array<const Block*, CACHE_SIDE_X * CACHE_SIDE_Y * CACHE_SIDE_Z> mBlocks;
	std::array<DataID, CACHE_SIDE_X * CACHE_SIDE_Y * CACHE_SIDE_Z> mData;
};
