#include "common_header.h"
#include "client/renderer/block/BlockTessellatorCache.h"
#include "world/level/block/Block.h"
#include "world/level/BlockPos.h"
#include "world/level/BlockSource.h"

void BlockTessellatorCache::reset(BlockSource& region, const BlockPos& startPostion){
	mRegion = &region;
	// we can always read one outside the chunk on each side
	pos = startPostion.west(2).north(2).below(2);

	// memset all three cache arrays directly (~88 KB total):
	// BrightnessPair{INVALID,INVALID} == 0xFFFF, DataID unset == 255, nullptr == unset Block*.
	memset(mLightColors.data(), 0xFF, sizeof(mLightColors));
	memset(mBlocks.data(), 0, sizeof(mBlocks));
	memset(mData.data(), 255, sizeof(mData));
}

BrightnessPair BlockTessellatorCache::getLightColor(const BlockPos& p) {
	if (p.y >= mRegion->getMaxHeight()) {
		return{ Brightness::MAX, Brightness::MIN }; //just skylight
	}
	DEBUG_ASSERT(p.x >= pos.x && p.y >= pos.y && p.z >= pos.z, "Point is outside cache, should be possible.");
	DEBUG_ASSERT(p.x - pos.x < CACHE_SIDE_X && p.y - pos.y < CACHE_SIDE_Y && p.z - pos.z < CACHE_SIDE_Z, "Point is outside cache, should be possible.");
	int index = _getIndexFromPositionInArray(p);

	auto cachedLightColor = mLightColors[index];
	if(cachedLightColor.block != Brightness::INVALID && cachedLightColor.sky != Brightness::INVALID) {
		return cachedLightColor;
	}

	const Block& block = getBlock(p);
	auto light = mRegion->getLightColor(p, _getLightEmission(block));

	if(light.sky == 0 && light.block == 0) {
		if(block.hasProperty(BlockProperty::HalfSlab)) {
			const Block& blockBelow = getBlock(p.below());
			// We need to get the raw value here, can't used the cached one since that one
			// can be changed
			light = mRegion->getLightColor(p.below(), _getLightEmission(blockBelow));
		}
	}

	mLightColors[index] = light;
	return light;
}

Brightness BlockTessellatorCache::_getLightEmission(const Block& block) {
	return Block::getLightEmission(block.mID);
}

const Block& BlockTessellatorCache::getBlock(const BlockPos& p) {
	if(p.y >= mRegion->getMaxHeight() || p.y < 0) {
		return *Block::mAir;
	}

	DEBUG_ASSERT(p.x >= pos.x && p.y >= pos.y && p.z >= pos.z, "Point is outside cache, should be possible.");
	DEBUG_ASSERT(p.x - pos.x < CACHE_SIDE_X && p.y - pos.y < CACHE_SIDE_Y && p.z - pos.z < CACHE_SIDE_Z, "Point is outside cache, should be possible.");
	int index = _getIndexFromPositionInArray(p);
	const Block* cachedBlock = mBlocks[index];
	if(cachedBlock != nullptr) {
		return *cachedBlock;
	}

	auto& checkedBlock = mRegion->getBlock(p);
	mBlocks[index] = &checkedBlock;
	return checkedBlock;
}

void BlockTessellatorCache::setBlockAtPosition(const Block& block, BlockPos& p) {
	int index = _getIndexFromPositionInArray(p);
	mBlocks[index] = &block;
}

DataID BlockTessellatorCache::getData(const BlockPos& _pos) {
	if(_pos.y >= mRegion->getMaxHeight() && _pos.y < 0) {
		return 0;
	}

	DEBUG_ASSERT(_pos.x >= pos.x && _pos.y >= pos.y && _pos.z >= pos.z, "Point is outside cache, should be possible.");
	DEBUG_ASSERT(_pos.x - pos.x < CACHE_SIDE_X && _pos.y - pos.y < CACHE_SIDE_Y && _pos.z - pos.z < CACHE_SIDE_Z, "Point is outside cache, should be possible.");
	int index = _getIndexFromPositionInArray(_pos);
	DataID cachedData = mData[index];
	if(cachedData != 255) {
		return cachedData;
	}

	DataID checkedData = mRegion->getData(_pos);
	mData[index] = checkedData;
	return checkedData;
}
