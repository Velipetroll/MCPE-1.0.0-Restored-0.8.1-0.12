/********************************************************
*   (c) Mojang. All rights reserved                     *
*   (c) Microsoft. All rights reserved.                 *
*********************************************************/
#include "common_header.h"

#include "client/renderer/chunks/RenderChunk.h"
#include "client/renderer/renderer/Tessellator.h"
#include "client/renderer/renderer/RenderMaterialGroup.h"
#include "client/renderer/block/BlockTessellator.h"
#include "client/renderer/chunks/RenderChunkBuilder.h"
#include "client/renderer/game/LevelRenderer.h"
#include "client/renderer/game/RenderChunkRenderParameters.h"
#include "world/entity/Entity.h"
#include "world/level/BlockSource.h"
#include "world/level/chunk/ChunkViewSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "util/DebugStats.h"
#include "util/PerfTimer.h"
#include "util/Math.h"
#include "Core/Debug/Log.h"
#include "Renderer/materialptr.h"
#include "Renderer/rendermaterial.h"
#include "Renderer/hal/interface/RenderContextImmediate.h"
#include <TextureGroup.h>

 mce::MaterialPtr RenderChunk::sFadingChunksMaterial;
 mce::MaterialPtr RenderChunk::sFadingSeasonsChunksMaterial;
 mce::MaterialPtr RenderChunk::sFadingSeasonsAlphaChunkMaterial;
 mce::MaterialPtr RenderChunk::sBelowWaterStencilMaterial;
 mce::MaterialPtr RenderChunk::sMaterialMap[enum_cast(TerrainLayer::Count)] = { };
 mce::MaterialPtr RenderChunk::sFogMaterialMap[enum_cast(TerrainLayer::Count)] = { };

namespace {
	float sChunkOrigin[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	uint32_t sChunkOriginSerial = 1; // shaders start at 0, so the first render always uploads
	bool sInTerrainPass = false;
	const mce::RenderMaterial* sLastTerrainMaterial = nullptr;
	const mce::Texture* sLastTerrainTex0 = nullptr;
	const mce::Texture* sLastTerrainTex1 = nullptr;
	const mce::Texture* sLastTerrainTex2 = nullptr;
}

void RenderChunk::setChunkOrigin(float x, float y, float z, float scale) {
	if (sChunkOrigin[0] == x && sChunkOrigin[1] == y && sChunkOrigin[2] == z && sChunkOrigin[3] == scale) {
		return;
	}
	sChunkOrigin[0] = x;
	sChunkOrigin[1] = y;
	sChunkOrigin[2] = z;
	sChunkOrigin[3] = scale;
	if (++sChunkOriginSerial == 0) {
		sChunkOriginSerial = 1;
	}
}

void RenderChunk::beginTerrainPass() {
	sInTerrainPass = true;
	sLastTerrainMaterial = nullptr;
	sLastTerrainTex0 = nullptr;
	sLastTerrainTex1 = nullptr;
	sLastTerrainTex2 = nullptr;
}

void RenderChunk::endTerrainPass() {
	sInTerrainPass = false;
	sLastTerrainMaterial = nullptr;
	sLastTerrainTex0 = nullptr;
	sLastTerrainTex1 = nullptr;
	sLastTerrainTex2 = nullptr;
	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
}
 mce::TexturePtr RenderChunk::endPortalTexture;
 mce::TexturePtr RenderChunk::endPortalColorTexture;
 
 mce::TexturePtr RenderChunk::atlasTexture;
 mce::TexturePtr RenderChunk::foliageTexture;
 mce::TexturePtr RenderChunk::brightnessTexture;

void RenderChunk::_initLayers(mce::MaterialPtr* materials, const std::string& postfix) {
 	materials[enum_cast(TerrainLayer::Opaque)] = mce::RenderMaterialGroup::switchable.getMaterial("terrain_opaque" + postfix);
 	materials[enum_cast(TerrainLayer::OpaqueSeasons)] = mce::RenderMaterialGroup::switchable.getMaterial("terrain_opaque_seasons" + postfix);
 	materials[enum_cast(TerrainLayer::Blend)] = mce::RenderMaterialGroup::switchable.getMaterial("terrain_blend" + postfix);
 	materials[enum_cast(TerrainLayer::Water)] = mce::RenderMaterialGroup::switchable.getMaterial("terrain_water" + postfix);
 	materials[enum_cast(TerrainLayer::Alpha)] = mce::RenderMaterialGroup::switchable.getMaterial("terrain_alpha" + postfix);
 	materials[enum_cast(TerrainLayer::AlphaSingleSide)] = mce::RenderMaterialGroup::switchable.getMaterial("terrain_alpha_single_side" + postfix);
 	materials[enum_cast(TerrainLayer::AlphaSeasons)] = mce::RenderMaterialGroup::switchable.getMaterial("terrain_alpha_seasons" + postfix);
 	materials[enum_cast(TerrainLayer::DoubleSide)] = mce::RenderMaterialGroup::switchable.getMaterial("terrain_doubleside" + postfix);
 	materials[enum_cast(TerrainLayer::Far)] = mce::RenderMaterialGroup::switchable.getMaterial("terrain_far" + postfix);
 	materials[enum_cast(TerrainLayer::FarSeasons)] = mce::RenderMaterialGroup::switchable.getMaterial("terrain_seasons_far" + postfix);
 	materials[enum_cast(TerrainLayer::FarSeasonsAlpha)] = mce::RenderMaterialGroup::switchable.getMaterial("terrain_seasons_far_alpha" + postfix);
 	materials[enum_cast(TerrainLayer::EndPortal)] = mce::RenderMaterialGroup::switchable.getMaterial("portal_base" + postfix);
}

 void RenderChunk::initMaterials(mce::TextureGroup& textures) {
 	sFadingChunksMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_fading_in");
 	sFadingSeasonsChunksMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_seasons_fading_in");
 	sFadingSeasonsAlphaChunkMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_seasons_fading_in_alpha");
 	sBelowWaterStencilMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_water_below_stencil");
 
 	atlasTexture = textures.getTexture(ResourceLocation("atlas.terrain"));
#ifdef TAC_COMPILE
 	foliageTexture = textures.getTexture(DynamicTexture::SeasonFoliageTexture);
 	brightnessTexture = textures.getTexture(DynamicTexture::BrightnessTexture);
#endif
 	endPortalTexture = textures.getTexture(ResourceLocation("textures/entity/end_portal"));
 	endPortalColorTexture = textures.getTexture(ResourceLocation("textures/environment/end_portal_colors"));
 
 	_initLayers(sMaterialMap, Util::EMPTY_STRING);
 	_initLayers(sFogMaterialMap, "_fog");

 	auto disableDepthWrite = [](const std::string& name) {
 		if (auto* mat = mce::RenderMaterialGroup::switchable._getMaterialPtr(name)) {
 			mat->setDepthWrite(false);
 		}
 	};
 	disableDepthWrite("terrain_water");
 	disableDepthWrite("terrain_water_below");
 	disableDepthWrite("terrain_water_fog");
 	disableDepthWrite("terrain_water_below_fog");
 	disableDepthWrite("terrain_blend");
 	disableDepthWrite("terrain_blend_below");
 	disableDepthWrite("terrain_blend_fog");
 	disableDepthWrite("terrain_blend_below_fog");
 }

RenderChunk::RenderChunk(const BlockPos& p) :
	mBuildState(BuildState::Dirty),
	mSortState(SortState::SortDirty) {
	setPos(p);
}

RenderChunk::~RenderChunk() {
	reset();
	Util::freeVectorMemory(mData);
	Util::freeVectorMemory(mData32bit);
	for (int i = 0; i < Facing::NUM_CULLING_IDS; ++i) {
		Util::freeVectorMemory(mFaces[i]);
	}
}

void RenderChunk::setPos(const BlockPos& p) {
	if (p == mPosition) {
		return;
	}

	reset();
	mPosition = p;
	mCenter = p + BlockPos(RENDERCHUNK_SIDE / 2);

	mBoundingBox = AABB(mPosition, (float)RENDERCHUNK_SIDE);

	//compute face centers
	const static int HALF_SIZE = RENDERCHUNK_SIDE / 2;

	for (auto i : range(6)) {
		BlockPos faceCenterBlock = mCenter + (Facing::DIRECTION[i] * HALF_SIZE);
		mFaceCenters[i] = Vec3((float)faceCenterBlock.x, (float)faceCenterBlock.y, (float)faceCenterBlock.z);
	}

	//compute bounds

	int r = 1;
	BlockPos min = p - r;
	BlockPos max = p + (RENDERCHUNK_SIDE + r);
	min.y = max.y = 0;

	mDataBounds = Bounds(min, max, RENDERCHUNK_SIDE);
}

RenderChunk::DataState RenderChunk::getDataState(BlockSource& region) {
	//check if all the chunks are existing & loaded
	for (auto& c : mDataBounds) {
		auto lc = region.getChunkSource().getExistingChunk(ChunkPos(c.x, c.z));

		if (lc == nullptr) {//not going to exist
			return DataState::WontLoad;
		}
		else if (lc->getState() < ChunkState::Loaded) {
			return DataState::Loading;
		}
	}
	return DataState::Loaded;
}

void RenderChunk::startRebuild(Unique<RenderChunkBuilder> set) {
	DEBUG_ASSERT(set, "Invalid set");
	DEBUG_ASSERT(isBuildState(BuildState::Dirty), "Render chunk is in an invalid state");

	mCurrentBuilder = std::move(set);

	//move the set region to our bounds
	mCurrentBuilder->localSource->move(mPosition - 1, mPosition + RENDERCHUNK_SIDE + 1);

	mCurrentBuilder->visibilityChanged = mVisibilityChanged;	// double buffer this
	mVisibilityChanged = false;
	mCurrentBuilder->mVisibility = mVisibility;

	mImmediateChange = false;

	//reset the dirty tick so that changes happening during rebuild are registered
	mLastChangeTick = mFirstChangeTick = Tick::MAX; 

	// change state and assign to a worker thread
	_changeBuildState(BuildState::Dirty, BuildState::Building);

	// Sorting state is now dirty
	_tryChangeSortState(SortState::Ready, SortState::SortDirty);

	// If the render chunk is already being sorted, restart the sort
	_tryChangeSortState(SortState::Sorting, SortState::RestartSort);

	// if sort state is already SortDirty or RestartSort, there's no need to change it.
}

void RenderChunk::startFaceSort(Unique<RenderChunkSorter> set) {
	DEBUG_ASSERT(set, "Invalid set");
	DEBUG_ASSERT(isSortState(SortState::SortDirty), "Render chunk is in an invalid state");

	mCurrentSorter = std::move(set);

	//change state and assign to a worker thread
	_changeSortState(SortState::SortDirty, SortState::Sorting);
}

void RenderChunk::faceSort() {

	if (mCurrentSorter == nullptr) {
		LOGI("Renderchunk is attempting to sort, but 'currentBuilder' is empty!!!\n");
		return;
	}

	mCurrentSorter->sortFaces(*this);
}

Unique<RenderChunkSorter>&& RenderChunk::endFaceSort() {
	DEBUG_ASSERT_MAIN_THREAD;

	DEBUG_ASSERT(mSortState == SortState::Sorting || mSortState == SortState::RestartSort, "Invalid state for ending sorting");

 	ScopedProfile("End face sort");

	auto& builder = *mCurrentSorter;

	if (mSortState == SortState::Sorting) {
		// Update the index buffer for the sorted indices
		if (mIndexSize == sizeof(uint16_t)) {
			if (mData.size() > 0) {
 				mMesh.loadIndexBuffer(mIndexSize, mData.data(), mData.size());
			}
		}
		else {
			if (mData32bit.size() > 0) {
 				mMesh.loadIndexBuffer(mIndexSize, mData32bit.data(), mData32bit.size());
			}
		}

		// Set up the terrain layer ranges from the render layer ranges
		builder.setupIndexRanges(mSortedIndexRange, mSortedRenderLayerIndexRange);

		if (mSortState == SortState::Sorting) {
			_changeSortState(SortState::Sorting, SortState::Ready);
		}
	}
	else if (mSortState == SortState::RestartSort) {
		_changeSortState(SortState::RestartSort, SortState::SortDirty);
	}

	return std::move(mCurrentSorter);
}

void RenderChunk::rebuild(bool transparentLeaves, bool smoothLighting, uint8_t lodLevel) {

	DEBUG_ASSERT(mCurrentBuilder, "Renderchunk is attempting to rebuild, but 'currentBuilder' is empty!!!\n");
	if (mCurrentBuilder == nullptr) {
		LOGI("Renderchunk is attempting to rebuild, but 'currentBuilder' is empty!!!\n");
		return;
	}

	mLodLevel = lodLevel;
	mCurrentBuilder->build(*this, transparentLeaves, smoothLighting, lodLevel);
}
#ifndef TAC_COMPILE
//Basically what it is in ghidra
double getTimeS()
{
#ifdef _WIN32
	LARGE_INTEGER frequency;
	LARGE_INTEGER counter;

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&counter);

	return static_cast<double>(counter.QuadPart) /
		static_cast<double>(frequency.QuadPart);
#else
	using clock = std::chrono::steady_clock;
	return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
#endif
}

const mce::TexturePtr mce::TexturePtr::NONE = mce::TexturePtr();
mce::RenderMaterialGroup mce::RenderMaterialGroup::switchable = mce::RenderMaterialGroup();
#endif

Unique<RenderChunkBuilder>&& RenderChunk::endRebuild(const Vec3& viewPos) {
	DEBUG_ASSERT_MAIN_THREAD;

	DEBUG_ASSERT(mBuildState == BuildState::Building, "Invalid state for ending rebuild");

 	ScopedProfile("EndRebuild");

	//end the thing
	auto& builder = *mCurrentBuilder;
	auto& t = builder.renderer->getTessellator();

	builder.localSource->clear();

	if (mBuildState == BuildState::Building) {
		const bool preSortedReady = (t.isTessellating() && builder.mSortedNumFaces > 0);
		if (t.isTessellating()) {
 			ScopedProfile("Upload RenderChunk");

 			mMesh = t.END();
			mNumFaces = builder.mSortedNumFaces;
			mIndexSize = builder.mSortedIndexSize;

			if (mNumFaces > 0) {
				if (mIndexSize == sizeof(uint16_t)) {
					mMesh.loadIndexBuffer(mIndexSize, builder.mSortedIndices16.data(), static_cast<unsigned int>(builder.mSortedIndices16.size()));
				} else {
					mMesh.loadIndexBuffer(mIndexSize, builder.mSortedIndices32.data(), static_cast<unsigned int>(builder.mSortedIndices32.size()));
				}
			}
			for (int i = 0; i < Facing::NUM_CULLING_IDS; i++) {
				mFaces[i].clear();
			}

			t.setOffset(0, 0, 0);

			//update polycounts
#ifdef DEBUG_STATS

			for (int i = 0; i < enum_cast(TerrainLayer::Count); ++i) {
				mPolyCount[i] = builder.mTerrainLayerRange[i].getPolyCount();
			}
#endif
		}
		else {
 			mMesh.reset();
			mNumFaces = 0;
			mData.clear();
			mData32bit.clear();
			for (int i = 0; i < Facing::NUM_CULLING_IDS; ++i) {
				mFaces[i].clear();
			}

#ifdef DEBUG_STATS
			memset(mPolyCount, 0, sizeof(mPolyCount));
#endif
		}

		//copy the new state
		mVisibility = builder.mVisibility;
		mAverageSkyLight = builder.averageSkyLight;
		mAllDark = builder.allDark;
		mSkyLit = builder.skyLit;
		memcpy(mIndexRange, builder.mTerrainLayerRange, sizeof(mIndexRange));
		memcpy(mRenderLayerIndexRange, builder.mRenderLayerRange, sizeof(mRenderLayerIndexRange));

		if (preSortedReady) {
			memcpy(mSortedIndexRange, builder.mSortedTerrainLayerRange, sizeof(mSortedIndexRange));
			memcpy(mSortedRenderLayerIndexRange, builder.mSortedRenderLayerRange, sizeof(mSortedRenderLayerIndexRange));
		}

		if (mLastChangeTick < Tick::MAX) {
			//if the dirty tick was set while rebuilding, rebuild again... there might be some changes we didn't account for
			_changeBuildState(BuildState::Building, BuildState::Dirty);
		}
		else {
			_changeBuildState(BuildState::Building, isEmpty() ? BuildState::Empty : BuildState::Ready);
		}

		if (mNumFaces == 0 || preSortedReady) {
			_tryChangeSortState(SortState::SortDirty, SortState::Ready);
			_tryChangeSortState(SortState::RestartSort, SortState::Ready);
			_tryChangeSortState(SortState::Sorting, SortState::Ready);
		}
		else {
			updateFaceSortState(viewPos);
		}

		if (!mCanRender) {
			//store when the chunk was first ready to do the fade in
 			mReadyTime = getTimeS();
		}

		mCanRender = true;

#ifdef DEBUG_STATS
		DebugStats::instance.mRebuiltChunks.push_back(ChunkPos(mPosition));
#endif
	}

	DEBUG_ASSERT(mCurrentBuilder, "Invalid set");
	return std::move(mCurrentBuilder);
}

void RenderChunk::makeReadyAsEmpty(bool hasNoSkyLight) {

	mVisibilityChanged = false;
	mImmediateChange = false;

 	mMesh.reset();
	mNumFaces = 0;
	memset(mIndexRange, 0, sizeof(mIndexRange));
	memset(mSortedIndexRange, 0, sizeof(mSortedIndexRange));
	memset(mRenderLayerIndexRange, 0, sizeof(mRenderLayerIndexRange));
	memset(mSortedRenderLayerIndexRange, 0, sizeof(mSortedRenderLayerIndexRange));
	mData.clear();
	mData32bit.clear();
	for (int i = 0; i < Facing::NUM_CULLING_IDS; ++i) {
		mFaces[i].clear();
	}

	mVisibility = VisibilityNode(true);

	mAverageSkyLight = hasNoSkyLight ? Brightness::MIN : Brightness::MAX;
	mSkyLit = hasNoSkyLight;
	mAllDark = !hasNoSkyLight;

	mLastChangeTick = mFirstChangeTick = Tick::MAX;

	mCanRender = true;

#ifdef DEBUG_STATS
	memset(mPolyCount, 0, sizeof(mPolyCount));
	DebugStats::instance.mRebuiltChunks.push_back(ChunkPos(mPosition));
#endif

	if (!_tryChangeBuildState(BuildState::Dirty, BuildState::Ready)) {
		_tryChangeBuildState(BuildState::Empty, BuildState::Ready);
	}

	if (!_tryChangeSortState(SortState::SortDirty, SortState::Ready)) {
		_tryChangeSortState(SortState::RestartSort, SortState::Ready);
	}
}


 const mce::MaterialPtr& RenderChunk::_chooseMaterial(TerrainLayer layer, double currentTime, bool forceFog) {
 	if (layer == TerrainLayer::Far || layer == TerrainLayer::FarSeasons || layer == TerrainLayer::FarSeasonsAlpha) {
 		const float fadeInTime = 1.2f;
 		double timediff = currentTime - mReadyTime;
 		if (timediff < fadeInTime) {
#ifdef TAC_COMPILE
 			Color color = currentShaderColor.getColor();
 			color.r = (float)((fadeInTime - timediff) / 5.);
 			currentShaderColor.setColor(color);
#endif
 			switch (layer) {
 				case TerrainLayer::Far:
 					return sFadingChunksMaterial;
 				case TerrainLayer::FarSeasons:
 					return sFadingSeasonsChunksMaterial;
 				case TerrainLayer::FarSeasonsAlpha:
 					return sFadingSeasonsAlphaChunkMaterial;
 				default:
 					break;
 			}
 		}
 	}
 	else if (layer == TerrainLayer::WaterStencil) {
 		return sBelowWaterStencilMaterial;
 	}
 
 	auto& materialSet = forceFog ? sFogMaterialMap : sMaterialMap;
 	if (forceFog && (!materialSet[(int)layer].operator->() || !materialSet[(int)layer]->myShader)) {
 		return sMaterialMap[(int)layer];
 	}
 	return materialSet[(int)layer];
 }

void RenderChunk::updateFaceSortState(BlockPos cameraPos) {
	if (mNumFaces == 0) {
		_tryChangeSortState(SortState::SortDirty, SortState::Ready);
		_tryChangeSortState(SortState::RestartSort, SortState::Ready);
		return;
	}

	if (!(mSortState == SortState::Ready || mSortState == SortState::Sorting || mSortState == SortState::RestartSort || mSortState == SortState::SortDirty)) {
		return;
	}

	// Determine chunk position relative to the camera using floor division so negative coordinates align with chunk boundaries
	BlockPos chunkPos(
		Math::floor((float)mPosition.x / RENDERCHUNK_SIDE),
		Math::floor((float)mPosition.y / RENDERCHUNK_SIDE),
		Math::floor((float)mPosition.z / RENDERCHUNK_SIDE));
	BlockPos camPos(
		Math::floor((float)cameraPos.x / RENDERCHUNK_SIDE),
		Math::floor((float)cameraPos.y / RENDERCHUNK_SIDE),
		Math::floor((float)cameraPos.z / RENDERCHUNK_SIDE));
	BlockPos cameraDelta = chunkPos - camPos;

	cameraDelta.x = Math::clamp(cameraDelta.x, -1, 1);
	cameraDelta.y = Math::clamp(cameraDelta.y, -1, 1);
	cameraDelta.z = Math::clamp(cameraDelta.z, -1, 1);

	// sortFaces() emits all 7 face buckets in fixed order, so already-Ready EBOs remain valid across boundary moves.
	mLastSortCameraDelta = cameraDelta;
}

void RenderChunk::sortFaces() {
	DEBUG_ASSERT(mSortState == SortState::Sorting || mSortState == SortState::RestartSort, "Invalid state.");

	const int INDICES_PER_FACE = 4;

	// Count the indices
	int indexCount = 0;
	mIndexSize = ((mNumFaces * INDICES_PER_FACE + 3) <= std::numeric_limits<uint16_t>::max()) ? sizeof(uint16_t) : sizeof(uint32_t);
	for (int rangeIdx = 0; rangeIdx < BlockRenderLayer::_RENDERLAYER_COUNT; rangeIdx++) {
		indexCount += mRenderLayerIndexRange[rangeIdx].mCount;
	}

	// Parse each range and generate a new sorted index list for it.
	mData.clear();
	mData32bit.clear();

	int idx = 0;
	for (int rangeIdx = 0; rangeIdx < BlockRenderLayer::_RENDERLAYER_COUNT; rangeIdx++) {

		// Emit all 7 face buckets across every layer; hardware backface culling handles single-sided faces
		// while preventing sub-block/double-sided/translucent faces from vanishing in negative coords or during sort transitions.
		int faceTypesToRender[7] = {
			Facing::EAST,
			Facing::WEST,
			Facing::UP,
			Facing::DOWN,
			Facing::SOUTH,
			Facing::NORTH,
			Facing::NOT_DEFINED
		};
		const unsigned int numFaceTypesToRender = 7;

		// record the start index for the sorted indices for this render layer 
		mSortedRenderLayerIndexRange[rangeIdx].mStart = idx;

		for (uint32_t curFaceIdx = 0; curFaceIdx < numFaceTypesToRender; curFaceIdx++) {
			int i = faceTypesToRender[curFaceIdx];
			for (uint32_t j = 0; j < mFaces[i].size(); j++) {
				// If the face is within the render layer range, add its indices to the index cache;
				const int VERTS_PER_FACE = 6;
				if (mFaces[i][j] * VERTS_PER_FACE >= (uint32_t)mRenderLayerIndexRange[rangeIdx].mStart
					&& mFaces[i][j] * VERTS_PER_FACE < (uint32_t)mRenderLayerIndexRange[rangeIdx].mStart + (uint32_t)mRenderLayerIndexRange[rangeIdx].mCount
					) {
					uint32_t baseidx = mFaces[i][j] * INDICES_PER_FACE;

					// check the index value
					DEBUG_ASSERT((((uint64_t)baseidx) + 3) < (((uint64_t)1) << (mIndexSize * 8)), "Index value overflow");
					if (mIndexSize == sizeof(uint16_t)) {
						mData.push_back(baseidx + 1);
						mData.push_back(baseidx + 2);
						mData.push_back(baseidx + 0);
						mData.push_back(baseidx + 0);
						mData.push_back(baseidx + 2);
						mData.push_back(baseidx + 3);
					}
					else {
						mData32bit.push_back(baseidx + 1);
						mData32bit.push_back(baseidx + 2);
						mData32bit.push_back(baseidx + 0);
						mData32bit.push_back(baseidx + 0);
						mData32bit.push_back(baseidx + 2);
						mData32bit.push_back(baseidx + 3);
					}

					idx += 6;
				}
			}
		}

		// record the count for the sorted indices for this render layer
		mSortedRenderLayerIndexRange[rangeIdx].mCount = idx - mSortedRenderLayerIndexRange[rangeIdx].mStart;
	}
}

RenderChunk::CutawayVisibilityBuffer& RenderChunk::getCutawayVisibilityBuffer() {
	if (!mCutawayVisibilityBuffer) {
		mCutawayVisibilityBuffer = std::make_unique<CutawayVisibilityBuffer>();
		memset(mCutawayVisibilityBuffer->data(), 0, mCutawayVisibilityBuffer->size());
	}

	return *mCutawayVisibilityBuffer;
}

void RenderChunk::render(const RenderChunkRenderParameters& renderChunkRenderParameters, const double currentTime) {
 	TerrainLayer geometryLayer = LevelRenderer::terrainRenderLayerToGeometryLayer(renderChunkRenderParameters.layer);
 
 	DEBUG_ASSERT(hasLayer(geometryLayer), "Can't render this layer, pls");
 
 	auto& material = _chooseMaterial(renderChunkRenderParameters.layer, currentTime, renderChunkRenderParameters.forceFog);
	const mce::RenderMaterial* matPtr = material.operator->();

 	auto& tex1 = (renderChunkRenderParameters.layer == TerrainLayer::EndPortal) ? endPortalTexture : atlasTexture;
 	auto& tex2 = (renderChunkRenderParameters.layer == TerrainLayer::EndPortal) ? endPortalColorTexture : brightnessTexture;

	if (sInTerrainPass) {
		if (sLastTerrainMaterial != matPtr) {
			if (matPtr) matPtr->activate();
			sLastTerrainMaterial = matPtr;
		}
		if (matPtr && matPtr->myShader && matPtr->myShader->mChunkOriginSerial != sChunkOriginSerial) {
			const mce::Shader* shader = matPtr->myShader;
			if (shader->mChunkOriginLoc == -2) {
				shader->mChunkOriginLoc = shader->getUniformLocation("CHUNK_ORIGIN_AND_SCALE");
			}
			glUniform4f(shader->mChunkOriginLoc, sChunkOrigin[0], sChunkOrigin[1], sChunkOrigin[2], sChunkOrigin[3]);
			shader->mChunkOriginSerial = sChunkOriginSerial;
		}
		const mce::Texture* t3 = *foliageTexture;
		const mce::Texture* t2 = *tex2;
		const mce::Texture* t1 = *tex1;
		if (t3 != nullptr && t3 != sLastTerrainTex2) {
			t3->bindTexture(mce::RenderContextImmediate::get(), 2, 0);
			sLastTerrainTex2 = t3;
		}
		if (t2 != nullptr && t2 != sLastTerrainTex1) {
			t2->bindTexture(mce::RenderContextImmediate::get(), 1, 0);
			sLastTerrainTex1 = t2;
		}
		if (t1 != nullptr && t1 != sLastTerrainTex0) {
			t1->bindTexture(mce::RenderContextImmediate::get(), 0, 0);
			sLastTerrainTex0 = t1;
		}

		if (mSortState == SortState::Ready && !renderChunkRenderParameters.forceUnsorted) {
			auto& ri = mSortedIndexRange[(int)geometryLayer];
			if (ri.mCount > 0) {
				mMesh.setIndexBufferUsageMode(mce::IndexBufferUsageMode::Normal);
				mMesh.renderRangeNoUnbind(ri.mStart, ri.mCount);
			}
		} else {
			auto& ri = mIndexRange[(int)geometryLayer];
			mMesh.setIndexBufferUsageMode(mce::IndexBufferUsageMode::ForceUseUnsorted);
			mMesh.renderRangeNoUnbind(ri.mStart, ri.mCount);
		}
		return;
	}

	// Non-batched fallback path (outside beginTerrainPass/endTerrainPass)
	if (matPtr && matPtr->myShader) {
		const mce::Shader* shader = matPtr->myShader;
		if (shader->mChunkOriginSerial != sChunkOriginSerial) {
			if (shader->mChunkOriginLoc == -2) {
				shader->mChunkOriginLoc = shader->getUniformLocation("CHUNK_ORIGIN_AND_SCALE");
			}
			shader->activate();
			glUniform4f(shader->mChunkOriginLoc, sChunkOrigin[0], sChunkOrigin[1], sChunkOrigin[2], sChunkOrigin[3]);
			shader->mChunkOriginSerial = sChunkOriginSerial;
		}
	}
 
 	if (mSortState == SortState::Ready && !renderChunkRenderParameters.forceUnsorted) {
 		// The sorted index buffer is ready, so use it
 		auto& ri = mSortedIndexRange[(int)geometryLayer];
 		if (ri.mCount > 0) {
 			mMesh.setIndexBufferUsageMode(mce::IndexBufferUsageMode::Normal);
 			mMesh.render(material, tex1, tex2, foliageTexture, ri.mStart, ri.mCount);
 		}
 	}
 	else {
 		// Render the mesh with the default (non-sorted) index buffer
 		auto& ri = mIndexRange[(int)geometryLayer];
 		mMesh.setIndexBufferUsageMode(mce::IndexBufferUsageMode::ForceUseUnsorted);
 		mMesh.render(material, tex1, tex2, foliageTexture, ri.mStart, ri.mCount);
 	}
}

bool RenderChunk::isEmpty() const {
	return mMesh.VBO == static_cast<unsigned int>(-1) && mNumFaces == 0;
}

float RenderChunk::distanceToSqr(const Entity* player) const {
	float xd = (float)(player->mPos.x - mCenter.x);
	float yd = (float)(player->mPos.y - mCenter.y);
	float zd = (float)(player->mPos.z - mCenter.z);
	return xd * xd + yd * yd + zd * zd;
}

bool RenderChunk::hasLayer(TerrainLayer r) const {
	return mIndexRange[static_cast<int>(r)].mCount > 0;
}

bool RenderChunk::isCulled() const {
	return !mVisible;
}

bool RenderChunk::isAllDark() const {
	return mAllDark;
}

float RenderChunk::getAverageBrightness() const {
	return mAverageSkyLight;
}

bool RenderChunk::canSeeChunkLookingFrom(unsigned char facingFrom, unsigned char facingTo) const {
	return mVisibility.from(facingFrom).contains(facingTo);
}

void RenderChunk::reset() {
	mVisible = false;
	mCanRender = false;
	mSkyLit = false;
	mAllDark = false;
	mLodLevel = 0;
	mAverageSkyLight = 0.0f;
	mReadyTime = 0;
	mImmediateChange = false;
	mVisibilityChanged = true;
	mVisibility = VisibilityNode(false);
	mLastChangeTick = Tick(0);
	mFirstChangeTick = Tick::MAX;
	mLastSortCameraDelta = BlockPos(INT_MAX);
	mPosition = BlockPos(INT_MAX);
	distanceFromPlayer2 = FLT_MAX;
	mIndexSize = 0;
	mNumFaces = 0;

	memset(mIndexRange, 0, sizeof(mIndexRange));
	memset(mSortedIndexRange, 0, sizeof(mSortedIndexRange));
	memset(mRenderLayerIndexRange, 0, sizeof(mRenderLayerIndexRange));
	memset(mSortedRenderLayerIndexRange, 0, sizeof(mSortedRenderLayerIndexRange));

	mData.clear();
	mData32bit.clear();
	for (int i = 0; i < Facing::NUM_CULLING_IDS; ++i) {
		mFaces[i].clear();
	}
	mCutawayVisibilityBuffer.reset();
	mCurrentBuilder.reset();
	mCurrentSorter.reset();

	mBuildState = BuildState::Dirty;
	mSortState = SortState::SortDirty;

 	mMesh.reset();
}

bool RenderChunk::isVisible(int frameID) const {
	return mVisible && isVisited(frameID);
}

bool RenderChunk::wasVisibileInPastFrames() const {
	return mVisible;
}

bool RenderChunk::isBottom() const {
	return mPosition.y == 0;
}

bool RenderChunk::isFaceSortDirty() const {
	return mSortState == SortState::SortDirty;
}

bool RenderChunk::hasImmediateChange() const {
	return mImmediateChange;
}

void RenderChunk::setVisible(bool v, unsigned int frameID) {
	mVisible = v;
	mLastFrameVisited = frameID;
}

void RenderChunk::updateDistanceFromPlayer(const Vec3& from) {
	distanceFromPlayer2 = from.distanceToSqr(mCenter);
}

int RenderChunk::isVisited(unsigned int frameID) const {
	return mLastFrameVisited >= frameID;
}

bool RenderChunk::isSkyLit() const {
	return mSkyLit;
}

int RenderChunk::getPriority(const RenderChunk& r) {
	if (r.distanceFromPlayer2 == distanceFromPlayer2) {
		return 0;
	}
	else {
		return r.distanceFromPlayer2 > distanceFromPlayer2 ? -1 : 1;
	}
}

BlockPos RenderChunk::getFacing(int facing) const {
	return mCenter + (Facing::DIRECTION[facing] * RENDERCHUNK_SIDE);
}

const Vec3& RenderChunk::getFaceCenter(int facing) const {
	return mFaceCenters[facing];
}

bool RenderChunk::_tryChangeBuildState(BuildState old, BuildState current) {

	return mBuildState.compare_exchange_strong(old, current);
}

void RenderChunk::_changeBuildState(BuildState old, BuildState current) {
	bool stateChanged = _tryChangeBuildState(old, current);
	DEBUG_ASSERT(stateChanged, "Wrong state change. This is really bad. This should not be just a warning!");
	UNUSED_VARIABLE(stateChanged);
}

bool RenderChunk::_tryChangeSortState(SortState old, SortState current) {

	return mSortState.compare_exchange_strong(old, current);
}

void RenderChunk::_changeSortState(SortState old, SortState current) {
	bool stateChanged = _tryChangeSortState(old, current);
	DEBUG_ASSERT(stateChanged, "Wrong state change. This is really bad. This should not be just a warning!");
	UNUSED_VARIABLE(stateChanged);
}

void RenderChunk::setDirty(Tick now, bool immediateChange) {
	mImmediateChange |= immediateChange;

	//always store the time of last change. We could be in the dirty state, 
	//but if a change happens while rebuilding we need to rebuild again as soon as we're done
	mLastChangeTick = now;

	//store the first change only once
	if(mFirstChangeTick == Tick::MAX) {
		mFirstChangeTick = now;
	}

	// If the chunk was in the middle of being sorted, tell it to start over
	_tryChangeSortState(SortState::Sorting, SortState::RestartSort);

	//try to perform one of the valid transitions
	_tryChangeBuildState(BuildState::Ready, BuildState::Dirty) || _tryChangeBuildState(BuildState::Empty, BuildState::Dirty);
}

bool RenderChunk::isDirty(Tick now, uint32_t minTicksSinceLastChange, uint32_t minTicksSinceFirstChange) const {
	if(hasImmediateChange()) {
		minTicksSinceLastChange = 0;
	}
	return isBuildState(BuildState::Dirty) && (now > (mLastChangeTick + minTicksSinceLastChange) || now > (mFirstChangeTick + minTicksSinceFirstChange));
}

void RenderChunk::setVisibilityChanged() {
	mVisibilityChanged = true;
}

bool RenderChunk::isPending() const {
	return mBuildState == BuildState::Dirty || mBuildState == BuildState::Building;
}

bool RenderChunk::isReady() {
	return mCanRender;
}

#ifdef DEBUG_STATS
int RenderChunk::getPolygonCount(TerrainLayer layer) const {
	return mPolyCount[enum_cast(layer)];
}
#endif // DEBUG_STATS

bool RenderChunk::isBuildState(BuildState s) const {
	return mBuildState == s;
}

bool RenderChunk::isSortState(SortState s) const {
	return mSortState == s;
}

void RenderChunk::setMaterialsBasedOnCameraUnderLiquid(const bool isUnderwater, const bool isUnderLava) {
	// Called every frame: only re-resolve the materials (5 name lookups) when the liquid state changes.
	static int sLastState = -1;
	const int state = (isUnderwater ? 1 : 0) | (isUnderLava ? 2 : 0);
	if (state == sLastState) {
		return;
	}
	sLastState = state;

 	sFogMaterialMap[enum_cast(TerrainLayer::Opaque)] = mce::RenderMaterialGroup::switchable.getMaterial(isUnderLava ? "terrain_opaque_fog_no_cull" : "terrain_opaque_fog");
 
 	sMaterialMap[enum_cast(TerrainLayer::Blend)] = mce::RenderMaterialGroup::switchable.getMaterial(isUnderwater ? "terrain_blend_below" : "terrain_blend");
 	sFogMaterialMap[enum_cast(TerrainLayer::Blend)] = mce::RenderMaterialGroup::switchable.getMaterial(isUnderwater ? "terrain_blend_below_fog" : "terrain_blend_fog");
 	sMaterialMap[enum_cast(TerrainLayer::Water)] = mce::RenderMaterialGroup::switchable.getMaterial(isUnderwater ? "terrain_water_below" : "terrain_water");
 	sFogMaterialMap[enum_cast(TerrainLayer::Water)] = mce::RenderMaterialGroup::switchable.getMaterial(isUnderwater ? "terrain_water_below_fog" : "terrain_water_fog");
}

BlockPos RenderChunk::getPosition() const {
	return mPosition;
}

void RenderChunk::setPosition(BlockPos Position) {
	mPosition = Position;
}

BlockPos RenderChunk::getCenter() const {
	return mCenter;
}

void RenderChunk::setCenter(BlockPos Center) {
	mCenter = Center;
}

Bounds RenderChunk::getDataBounds() const {
	return mDataBounds;
}

void RenderChunk::setDataBounds(Bounds DataBounds) {
	mDataBounds = DataBounds;
}

AABB RenderChunk::getBoundingBox() const {
	return mBoundingBox;
}

void RenderChunk::setBoundingBox(AABB BoundingBox) {
	mBoundingBox = BoundingBox;
}
