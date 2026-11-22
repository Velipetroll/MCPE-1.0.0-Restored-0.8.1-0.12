//Thanks learnopengl.com (losely based of some of that code)

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <world/level/Level.h>
#include <renderer/chunks/RenderChunkBuilder.h>
#include "world/level/dimension/Dimension.h"
#include <renderer/game/RenderChunkRenderParameters.h>
#include "renderer/chunks/RenderChunk.h"
#include "renderer/renderer/Tessellator.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "renderer/texture/TextureAtlas.h"
#include "Shader.h"
#include <world/level/FoliageColor.h>
#include "renderer/renderer/RenderMaterialGroup.h"
#include "client/gui/GuiContext.h"
#include "client/gui/Options.h"
#include "client/gui/Font.h"
#include "client/gui/Gui.h"
#include "client/gui/ChatScreen.h"
#include "client/gui/InventoryScreens.h"
#include "client/gui/StartMenuScreen.h"
#include "client/gui/DeathScreen.h"
#include "client/gui/InBedScreen.h"
#include "client/gui/GuiComponent.h"
#include "client/gui/WorldPersistence.h"
#include "client/game/WorldLighting.h"
#include "client/game/MobSystem.h"
#include "world/crafting/CraftingRecipes.h"
#include "world/level/storage/DBStorage.h"
#include "platform/threading/WorkerPool.h"
#include "world/phys/HitResult.h"
#include "world/Facing.h"
#include "world/item/Item.h"
#include "SoundEngine.h"
#include "ParticleSystem.h"
#include "ProjectileTntSystem.h"
#include "PaintingSystem.h"
#include "world/level/block/FireBlock.h"
#include "world/level/BlockSourceListener.h"
#include "world/level/block/Block.h"
#include "world/level/block/BedBlock.h"
#include "world/level/block/DoorBlock.h"
#include "world/level/block/TrapDoorBlock.h"
#include "world/level/block/FenceGateBlock.h"
#include "world/level/block/TorchBlock.h"
#include "world/level/block/VineBlock.h"
#include "world/level/block/LeverBlock.h"
#include "world/level/block/RotatedPillarBlock.h"
#include "world/level/block/SlabBlock.h"
#include "world/level/block/NetherReactor.h"
#include "world/level/block/HeavyBlock.h"
#include "world/level/block/entity/NetherReactorBlockEntity.h"
#include "world/level/block/entity/SignBlockEntity.h"
#include "world/level/BlockTickingQueue.h"
#include "world/Direction.h"
#include "world/level/GameType.h"
#include "world/level/material/Material.h"
#include "util/Random.h"
#include "nbt/NbtIo.h"
#include "nbt/CompoundTag.h"
#include "nbt/ListTag.h"
#include "util/StringByteInput.h"
#include "util/StringByteOutput.h"
#include "client/gui/ProgressScreen.h"
#include "util/PerfTimer.h"
#include "Renderer/Mesh.h"
#include "stb_image.h"
#include <chrono>
#include <cstdio>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

bool gBenchChunksMode = false;
std::string gBenchWorldDir = "./worlds/__bench_tmp";
int64_t gBenchWorldSeed = 12345;
int gBenchWalkFrames = 180;


int screenx = 1600;
int screeny = 900;

static GuiContext gGuiContext;
static Options gGuiOptions;
static Font gGuiFont;
static Gui gGuiHud;
static bool gCursorLocked = true;

static void updateGuiScale()
{
	int autoScale = 1;
	if (screenx >= 1000) autoScale = 4;
	else if (screenx >= 800) autoScale = 3;
	else if (screenx >= 400) autoScale = 2;

	int scale = (gGuiOptions.guiScale > 0) ? gGuiOptions.guiScale : autoScale;
	gGuiContext.windowWidth = screenx;
	gGuiContext.windowHeight = screeny;
	gGuiContext.guiScale = scale;
	gGuiContext.guiWidth = std::max(1, static_cast<int>(screenx / static_cast<float>(scale)));
	gGuiContext.guiHeight = std::max(1, static_cast<int>(screeny / static_cast<float>(scale)));
}

//Shader* sptr;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, width, height);//Set the height again
	screenx = width;
	screeny = height;
	updateGuiScale();
	if (gGuiContext.currentScreen) {
		gGuiContext.currentScreen->init(&gGuiContext, gGuiContext.guiWidth, gGuiContext.guiHeight);
	}

	glm::mat4 projection = glm::perspective(glm::radians(85.0f),
		(float)screenx / (float)screeny,
		0.1f, 1000.0f);

	//sptr->setMatrix4("projection", glm::value_ptr(projection));

}

class GLInput
{
public:
	virtual bool KeyDown(int key);
	virtual bool KeyUp(int key);
	virtual bool MouseDown(int key);
	virtual bool MouseUp(int key);
	virtual bool NewKeyDown(int key);
	virtual bool NewKeyUp(int key);
	virtual bool NewMouseDown(int key);
	virtual bool NewMouseUp(int key);
	void Clear();

	bool lookup_table[GLFW_KEY_LAST];
	int last_table[GLFW_KEY_LAST];
	bool mlookup_table[GLFW_MOUSE_BUTTON_LAST];
	int mlast_table[GLFW_MOUSE_BUTTON_LAST];
	float mouseXPos;
	float mouseYPos;
} glinput;

bool GLInput::KeyDown(int key)
{
	return lookup_table[key] == GLFW_PRESS;
}

bool GLInput::KeyUp(int key)
{
	return lookup_table[key] == GLFW_RELEASE;
}

bool GLInput::NewKeyDown(int key)
{
	return last_table[key] == 1;
}

bool GLInput::NewKeyUp(int key)
{
	return last_table[key] == -1;
}

bool GLInput::MouseDown(int key)
{
	return mlookup_table[key] == GLFW_PRESS;
}

bool GLInput::MouseUp(int key)
{
	return mlookup_table[key] == GLFW_RELEASE;
}

bool GLInput::NewMouseDown(int key)
{
	return mlast_table[key] == 1;
}

bool GLInput::NewMouseUp(int key)
{
	return mlast_table[key] == -1;
}

void GLInput::Clear()
{
	memset(mlast_table, 0, GLFW_MOUSE_BUTTON_LAST * sizeof(int));
	memset(last_table, 0, GLFW_KEY_LAST * sizeof(int));
}



struct CCTX
{
	std::atomic<bool> running;
	Level* level;
	Dimension* dim;
};

extern CCTX commandContext;

void char_callback(GLFWwindow* window, unsigned int codepoint)
{
	if (gGuiContext.currentScreen) {
		gGuiContext.currentScreen->charTyped(codepoint);
	} else if (gGuiContext.state == GameState::InGame && codepoint == '/') {
		gGuiContext.setScreen(new ChatScreen("/"));
	}
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		if (gGuiContext.currentScreen) {
			if (!gGuiContext.currentScreen->handleBackEvent(true)) {
				gGuiContext.currentScreen->keyPressed(key);
			}
		} else if (gGuiContext.state == GameState::InGame) {
			gGuiContext.pauseGame();
		} else if (gGuiContext.state == GameState::Paused) {
			gGuiContext.resumeGame();
		}
		return;
	}
	if (action == GLFW_PRESS && gGuiContext.state == GameState::InGame && !gGuiContext.currentScreen) {
		if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
			gGuiHud.selectedSlot = key - GLFW_KEY_1;
			gGuiContext.selectedHotbarSlot = key - GLFW_KEY_1;
		} else if (key == GLFW_KEY_V) {
			gGuiContext.isCreative = !gGuiContext.isCreative;
			if (commandContext.level) {
				commandContext.level->getLevelData().setGameType(gGuiContext.isCreative ? GameType::Creative : GameType::Survival);
			}
			if (!gGuiContext.isCreative && gGuiContext.health <= 0) {
				gGuiContext.resetSurvivalStats();
			}
			gGuiContext.chatHistory.push_back(gGuiContext.isCreative ? "Mode: Creative (Double-Space to Fly)" : "Mode: Survival");
			return;
		} else if (key == GLFW_KEY_T) {
			gGuiContext.setScreen(new ChatScreen(""));
			return;
		} else if (key == GLFW_KEY_SLASH) {
			gGuiContext.setScreen(new ChatScreen("/"));
			return;
		} else if (key == GLFW_KEY_E) {
			if (gGuiContext.isCreative) {
				gGuiContext.setScreen(new CreativeInventoryScreen());
			} else {
				gGuiContext.setScreen(new InventoryScreen());
			}
			return;
		} else if (key == GLFW_KEY_F5) {
			gGuiOptions.thirdPerson = !gGuiOptions.thirdPerson;
			return;
		} else if (key == GLFW_KEY_O) {
			gGuiOptions.greedyMeshing = !gGuiOptions.greedyMeshing;
			gGuiContext.chatHistory.push_back(gGuiOptions.greedyMeshing ? "Greedy Meshing: ON" : "Greedy Meshing: OFF");
			return;
		}
	}
	if (action == GLFW_PRESS && gGuiContext.currentScreen) {
		gGuiContext.currentScreen->keyPressed(key);
	}
	if (key == GLFW_KEY_0 && action == GLFW_PRESS) {
		static bool sWireframe = false;
		sWireframe = !sWireframe;
		glPolygonMode(GL_FRONT_AND_BACK, sWireframe ? GL_LINE : GL_FILL);
	}

	bool pressed = (action != GLFW_RELEASE);

	if (pressed != glinput.lookup_table[key])
	{
		glinput.last_table[key] = pressed ? 1 : -1;
		glinput.lookup_table[key] = pressed;
	}
}

static float gBowPullTime = 0.0f;
static bool gBowPulling = false;
static int gPlayerFireTicks = 0;

struct FallingBlockEnt {
	float x, y, z, vy, startY;
	BlockID id;
	DataID data;
	bool creative;
};
static std::vector<FallingBlockEnt> sFallingBlocks;
static float gItemUseSlowTimer = 0.0f;

mce::TextureGroup texgroup;

class Camera
{
public:
	glm::vec3 mPos = glm::vec3(0, 100, 0);
	glm::vec3 cameraFront = glm::vec3(0, -1, 0);
	glm::vec3 cameraMoveFront = glm::vec3(1, 0, 0);
	glm::vec3 cameraUp = glm::vec3(0, 1, 0);



	double lastX;
	double lastY;
	bool firstMouse = true;
	int mIgnoreMouseFrames = 0;
	double yaw = 0;
	double pitch = 0;

	BlockSource* mRegion;//commandContext.dim->getChunkSource()
	Level* mLevel;
	Vec3 mPosDelta;
	float mMaxAutoStep = 0.5f;
	bool mCollidableMobNear = false;
	float mLastPenetration = 0;
	bool mHorizontalCollision = false;
	bool mVerticalCollision = false;
	bool mCollision = false;
	bool mStuckInCollider = false;
	bool mOnGround = false;
	bool mIsStuckInWeb = false;
	bool mNoGravity = false;
	bool mIsFlying = false;
	float mFallDistance = 0.0f;
	float mWalkDist = 0.0f;
	float mSmoothBob = 0.0f;
	float mSwingProgress = 0.0f;
	double mLastSpacePressTime = -1.0;
	float mTickAccum = 0.0f;
	glm::vec3 mPrevPos = glm::vec3(0.0f, 99.62f, 0.0f);
	glm::vec3 mCurrPos = glm::vec3(0.0f, 99.62f, 0.0f);
	float mPrevFeetY = 98.0f;
	float mCurrFeetY = 98.0f;
	float mRenderFeetY = 98.0f;
	float mPrevWalkDist = 0.0f;
	float mCurrWalkDist = 0.0f;
	float mPrevSmoothBob = 0.0f;
	float mCurrSmoothBob = 0.0f;
	float mTilt = 0.0f;
	float mPrevTilt = 0.0f;
	float mCurrTilt = 0.0f;
	int mHurtTime = 0;
	int mHurtDuration = 10;
	float mHurtDir = 0.0f;
	int mDeathTime = 0;
	float mAlpha = 0.0f;
	float mWalkAnimPos = 0.0f;
	float mPrevWalkAnimPos = 0.0f;
	float mCurrWalkAnimPos = 0.0f;
	float mWalkAnimSpeed = 0.0f;
	float mPrevWalkAnimSpeed = 0.0f;
	float mCurrWalkAnimSpeed = 0.0f;
	float mBodyYaw = 0.0f;
	float mPrevBodyYaw = 0.0f;
	float mCurrBodyYaw = 0.0f;
	int mNextStepSound = 1;
	bool mWasInWater = false;
	bool mWasJumpHeld = false;
	int mFlyToggleTicks = 0;

	void triggerSwing() {
		mSwingProgress = 0.001f;
	}
	//AABB* mLastHitBB;
	AABB mBB;
	std::vector<AABB> mSubBBs;
	//Vec2 mBBDim = { 0.6f, 1.8f }; // *** Def ***
	float mHeightOffset = 1.62f;
	Vec3 mSlideOffset = Vec3(0,0,0);
	bool mNoPhysics = false;

	BlockSource& getRegion() { return *mRegion; };
	Level& getLevel() { return *mLevel; };
	bool isStackable() { return false; };
	void moveBBs(const Vec3& pos);

	void move(const Vec3& posIn);

	Camera() : mRegion(nullptr), mLevel(nullptr), mPosDelta(0, 0, 0)
	{
		mBB.set(-0.3f, 98.0f, -0.3f, 0.3f, 99.8f, 0.3f);
		mPos = glm::vec3(0.0f, 98.0f + mHeightOffset, 0.0f);
		mPrevPos = mCurrPos = mPos;
		mPrevFeetY = mCurrFeetY = mRenderFeetY = 98.0f;
	}

	Camera(Level* level, Dimension* dim) : mPosDelta(0, 0, 0)
	{
		setWorld(level, dim);
		mBB.set(-0.3f, 98.0f, -0.3f, 0.3f, 99.8f, 0.3f);
		mPos = glm::vec3(0.0f, 98.0f + mHeightOffset, 0.0f);
		mPrevPos = mCurrPos = mPos;
		mPrevFeetY = mCurrFeetY = mRenderFeetY = 98.0f;
	}

	void setWorld(Level* level, Dimension* dim)
	{
		mLevel = level;
		mRegion = dim ? &dim->getBlockSourceDEPRECATEDUSEPLAYERREGIONINSTEAD() : nullptr;
	}

	void setPositionAndLook(float x, float feetY, float z, double yawIn, double pitchIn)
	{
		mBB.set(x - 0.3f, feetY, z - 0.3f, x + 0.3f, feetY + 1.8f, z + 0.3f);
		mPos = glm::vec3(x, feetY + mHeightOffset, z);
		mPrevPos = mCurrPos = mPos;
		mPrevFeetY = mCurrFeetY = mRenderFeetY = feetY;
		mPrevWalkDist = mCurrWalkDist = mWalkDist = 0.0f;
		mPrevSmoothBob = mCurrSmoothBob = mSmoothBob = 0.0f;
		mPrevWalkAnimPos = mCurrWalkAnimPos = mWalkAnimPos = 0.0f;
		mPrevWalkAnimSpeed = mCurrWalkAnimSpeed = mWalkAnimSpeed = 0.0f;
		mPrevBodyYaw = mCurrBodyYaw = mBodyYaw = static_cast<float>(yawIn);
		mNextStepSound = 1;
		mWasInWater = false;
		mWasJumpHeld = false;
		mFlyToggleTicks = 0;
		mTickAccum = 0.0f;
		mPosDelta = Vec3(0.0f, 0.0f, 0.0f);
		mFallDistance = 0.0f;
		mOnGround = false;
		mIsFlying = false;
		mSlideOffset = Vec3(0.0f, 0.0f, 0.0f);
		yaw = yawIn;
		pitch = pitchIn;
		if (pitch > 89.0) pitch = 89.0;
		if (pitch < -89.0) pitch = -89.0;
		glm::vec3 direction;
		direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		direction.y = sin(glm::radians(pitch));
		direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		cameraFront = glm::normalize(direction);
		direction.x = cos(glm::radians(yaw));
		direction.y = 0;
		direction.z = sin(glm::radians(yaw));
		cameraMoveFront = glm::normalize(direction);
		firstMouse = true;
	}

	void spawnAtSurface()
	{
		Height topY = getRegion().getAboveTopSolidBlock(0, 0, true, false);
		if (topY < 1 || topY >= 126) topY = 80;
		float feetY = static_cast<float>(topY) + 0.05f;
		mBB.set(0.5f - 0.3f, feetY, 0.5f - 0.3f,
		        0.5f + 0.3f, feetY + 1.8f, 0.5f + 0.3f);
		mPos = glm::vec3(0.5f, feetY + mHeightOffset, 0.5f);
		mPrevPos = mCurrPos = mPos;
		mPrevFeetY = mCurrFeetY = mRenderFeetY = feetY;
		mPrevWalkDist = mCurrWalkDist = mWalkDist = 0.0f;
		mPrevSmoothBob = mCurrSmoothBob = mSmoothBob = 0.0f;
		mPrevWalkAnimPos = mCurrWalkAnimPos = mWalkAnimPos = 0.0f;
		mPrevWalkAnimSpeed = mCurrWalkAnimSpeed = mWalkAnimSpeed = 0.0f;
		mPrevBodyYaw = mCurrBodyYaw = mBodyYaw = static_cast<float>(yaw);
		mNextStepSound = 1;
		mWasInWater = false;
		mWasJumpHeld = false;
		mFlyToggleTicks = 0;
		mTickAccum = 0.0f;
		mPosDelta = Vec3(0.0f, 0.0f, 0.0f);
		mFallDistance = 0.0f;
		mOnGround = false;
		mIsFlying = false;
		mSlideOffset = Vec3(0.0f, 0.0f, 0.0f);
	}

	void teleportTo(float x, float eyeY, float z)
	{
		float feetY = eyeY - mHeightOffset;
		mBB.set(x - 0.3f, feetY, z - 0.3f, x + 0.3f, feetY + 1.8f, z + 0.3f);
		mPos = glm::vec3(x, eyeY, z);
		mPrevPos = mCurrPos = mPos;
		mPrevFeetY = mCurrFeetY = mRenderFeetY = feetY;
		mSlideOffset = Vec3(0.0f, 0.0f, 0.0f);
		mTickAccum = 0.0f;
		mPosDelta = Vec3(0.0f, 0.0f, 0.0f);
		mFallDistance = 0.0f;
	}

	// Exact 1:1 MCPE 0.8.1 Entity::isInWater() + Level::checkAndHandleWater() (Entity.cpp L1167-1189, Level.cpp L173-234)
	bool isInWater(bool applyFlow = true)
	{
		AABB v10(mBB.min.x, mBB.min.y + 0.4f, mBB.min.z,
		         mBB.max.x, mBB.max.y - 0.4f, mBB.max.z);
		BlockSource& reg = getRegion();
		int v7 = Math::floor(v10.min.x);
		int v21 = Math::floor(v10.max.x + 1.0f);
		int v8 = Math::floor(v10.min.y);
		int v20 = Math::floor(v10.max.y + 1.0f);
		int v22 = Math::floor(v10.min.z);
		int v23 = Math::floor(v10.max.z + 1.0f);
		bool v9 = false;
		Vec3 v25(0.0f, 0.0f, 0.0f);
		for (int x = v7; x < v21; ++x) {
			for (int i = v8; i < v20; ++i) {
				for (int j = v22; j < v23; ++j) {
					BlockID bid = reg.getBlockID(x, i, j);
					const Block* blk = (bid > 0) ? Block::mBlocks[bid] : nullptr;
					if (blk && blk->getMaterial().isType(MaterialType::Water)) {
						int v11 = reg.getData(x, i, j);
						if (v11 > 7) v11 = 0;
						if (static_cast<float>(v20) >= (static_cast<float>(i + 1) - (static_cast<float>(v11 + 1) / 9.0f))) {
							if (applyFlow) {
								blk->handleEntityInside(reg, BlockPos(x, i, j), nullptr, v25);
							}
							v9 = true;
						}
					}
				}
			}
		}
		if (applyFlow) {
			float v14 = std::sqrt(v25.x * v25.x + v25.y * v25.y + v25.z * v25.z);
			if (v14 > 0.0f) {
				float v15 = 0.004f / v14;
				mPosDelta.x += v15 * v25.x;
				mPosDelta.y += v15 * v25.y;
				mPosDelta.z += v15 * v25.z;
			}
		}
		return v9;
	}

	// Exact 1:1 MCPE 0.8.1 Entity::isInLava() + Level::containsMaterial() (Entity.cpp L1190-1214, Level.cpp L597-638)
	bool isInLava()
	{
		AABB v11(mBB.min.x + 0.1f, mBB.min.y + 0.4f, mBB.min.z + 0.1f,
		         mBB.max.x - 0.1f, mBB.max.y - 0.4f, mBB.max.z - 0.1f);
		return getRegion().containsMaterial(v11, MaterialType::Lava);
	}

	// Exact 1:1 MCPE 0.8.1 Mob::onLadder() (Mob.cpp L835-844)
	bool onLadder()
	{
		int v2 = Math::floor(mCurrPos.x);
		int v3 = Math::floor(mBB.min.y);
		int v4 = Math::floor(mCurrPos.z);
		BlockSource& reg = getRegion();
		BlockID ladderId = Block::mLadder ? Block::mLadder->mID : BlockID(65);
		return reg.getBlockID(v2, v3, v4) == ladderId || reg.getBlockID(v2, v3 + 1, v4) == ladderId;
	}

	// Exact 1:1 MCPE 0.8.1 Entity::isFree(x, y, z) (Entity.cpp L1122-1150)
	bool isFree(float dx, float dy, float dz)
	{
		AABB v17 = mBB.cloneMove(Vec3(dx, dy, dz));
		BlockSource& reg = getRegion();
		return reg.fetchCollisionShapes(reg, v17, nullptr, false, nullptr).empty() && !reg.containsAnyLiquid(v17);
	}

	// Exact 1:1 MCPE 0.8.1 Entity::checkTileCollisions() (Entity.cpp L111-146)
	void checkTileCollisions()
	{
		int minX = Math::floor(mBB.min.x);
		int minY = Math::floor(mBB.min.y);
		int minZ = Math::floor(mBB.min.z);
		int maxX = Math::floor(mBB.max.x);
		int maxY = Math::floor(mBB.max.y);
		int maxZ = Math::floor(mBB.max.z);
		BlockSource& reg = getRegion();
		BlockID webId = Block::mWeb ? Block::mWeb->mID : BlockID(30);
		for (int x = minX; x <= maxX; ++x) {
			for (int y = minY; y <= maxY; ++y) {
				for (int z = minZ; z <= maxZ; ++z) {
					BlockID bid = reg.getBlockID(x, y, z);
					if (bid == webId) {
						// WebTile::entityInside -> Entity::makeStuckInWeb() (Entity.cpp L1252-1255)
						mIsStuckInWeb = true;
						mFallDistance = 0.0f;
					}
				}
			}
		}
	}

	// Exact 1:1 MCPE 0.8.1 Entity::checkFallDamage() + Mob::causeFallDamage() (Entity.cpp L1600-1634, Mob.cpp L749-776)
	void checkFallDamage(float fdChange, bool onGroundNow)
	{
		if (onGroundNow) {
			if (mFallDistance <= 0.0f) {
				return;
			}
			BlockSource& reg = getRegion();
			int x = Math::floor(mCurrPos.x);
			int y = Math::floor(mCurrPos.y - 0.2f - mHeightOffset);
			int z = Math::floor(mCurrPos.z);
			BlockID tileID = reg.getBlockID(x, y, z);
			BlockID fenceId = Block::mFence ? Block::mFence->mID : BlockID(85);
			if (tileID == 0) {
				if (reg.getBlockID(x, y - 1, z) == fenceId) {
					tileID = reg.getBlockID(x, y - 1, z);
				}
			}
			// FarmTile::fallOn(level, x, y, z, this, fallDistance) (FarmTile.cpp L86-90)
			if (tileID > 0 && Block::mFarmland && tileID == Block::mFarmland->mID) {
				if (reg.getLevel().getRandom().nextFloat() < (mFallDistance - 0.5f)) {
					BlockID dirtId = Block::mDirt ? Block::mDirt->mID : BlockID(3);
					reg.setBlock(x, y, z, dirtId, Block::UPDATE_ALL);
				}
			}
			// Mob::causeFallDamage(fallDistance) (Mob.cpp L760-775)
			int v3 = static_cast<int>(std::ceil(mFallDistance - 3.0f));
			if (v3 > 0 && !gGuiContext.isCreative && !mIsFlying) {
				SoundEngine::play(SoundEngine::SND_HURT, 0.75f, 1.0f);
				int dealt = gGuiContext.applyDamage(v3, false);
				if (dealt > 0) {
					mHurtTime = 10;
					mHurtDuration = 10;
					mHurtDir = static_cast<float>((std::rand() % 2) * 180);
				}
				if (tileID > 0) {
					SoundEngine::play(SoundEngine::SND_STEP, 0.5f, 0.75f);
				}
			}
			mFallDistance = 0.0f;
		} else if (fdChange < 0.0f) {
			mFallDistance -= fdChange;
		}
	}

	bool isSneaking()
	{
		return gGuiContext.currentScreen == nullptr && (glinput.KeyDown(GLFW_KEY_LEFT_SHIFT) || glinput.KeyDown(GLFW_KEY_RIGHT_SHIFT));
	}

	// Exact 1:1 MCPE 0.8.1 Entity::moveRelative(fw, sf, speed) (Entity.cpp L791-814)
	void moveRelative(float strafe, float forward, float speed)
	{
		float dist = std::sqrt(strafe * strafe + forward * forward);
		if (dist < 0.01f) return;
		if (dist < 1.0f) dist = 1.0f;
		float scale = speed / dist;
		float s = strafe * scale;
		float f = forward * scale;
		glm::vec3 right = glm::normalize(glm::cross(cameraMoveFront, cameraUp));
		mPosDelta.x += f * cameraMoveFront.x + s * right.x;
		mPosDelta.z += f * cameraMoveFront.z + s * right.z;
	}

	void stepPhysicsTick()
	{
		mPrevPos = mCurrPos;
		mPrevFeetY = mCurrFeetY;
		mPrevWalkDist = mCurrWalkDist;
		mPrevSmoothBob = mCurrSmoothBob;
		mPrevTilt = mCurrTilt;
		if (mHurtTime > 0) --mHurtTime;
		if (gGuiContext.health <= 0) ++mDeathTime;
		else mDeathTime = 0;
		mPrevWalkAnimPos = mCurrWalkAnimPos;
		mPrevWalkAnimSpeed = mCurrWalkAnimSpeed;
		mPrevBodyYaw = mCurrBodyYaw;

		// Entity::baseTick() (Entity.cpp L893-929): check water, reset fallDistance
		if (isInWater(true)) {
			mFallDistance = 0.0f;
			mWasInWater = true;
		} else {
			mWasInWater = false;
		}

		// LocalPlayer::aiStep() (LocalPlayer.cpp L400-458)
		if (mFlyToggleTicks > 0) --mFlyToggleTicks;
		bool prevJumpHeld = mWasJumpHeld;
		bool jumpHeld = (gGuiContext.currentScreen == nullptr) && glinput.KeyDown(GLFW_KEY_SPACE);
		mWasJumpHeld = jumpHeld;

		bool sneaking = isSneaking();
		if (sneaking && mSlideOffset.y < 0.2f) {
			mSlideOffset.y = 0.2f;
		}

		float moveStrafe = 0.0f;
		float moveForward = 0.0f;
		if (gGuiContext.currentScreen == nullptr) {
			if (glinput.KeyDown(GLFW_KEY_W)) moveForward += 1.0f;
			if (glinput.KeyDown(GLFW_KEY_S)) moveForward -= 1.0f;
			if (glinput.KeyDown(GLFW_KEY_A)) moveStrafe -= 1.0f;
			if (glinput.KeyDown(GLFW_KEY_D)) moveStrafe += 1.0f;
		}

		// KeyboardInput::tick() (KeyboardInput.cpp L38-41): sneaking multiplies strafeInput & forwardInput by 0.3f
		if (sneaking) {
			moveStrafe *= 0.3f;
			moveForward *= 0.3f;
		}

		// LocalPlayer::aiStep() L413-447: fly toggle & flying vertical velocity
		float flyIdleMod = 1.0f;
		if (gGuiContext.isCreative) {
			if (!prevJumpHeld && jumpHeld) {
				if (mFlyToggleTicks > 0) {
					mIsFlying = !mIsFlying;
					mFlyToggleTicks = 0;
				} else {
					mFlyToggleTicks = 7;
				}
			}
			if (mIsFlying) {
				float maxIn = std::max(std::abs(moveStrafe), std::abs(moveForward));
				if (maxIn < 0.01f) {
					flyIdleMod = 0.75f;
				}
				if (jumpHeld) {
					mPosDelta.y += 0.15f;
				}
				if (sneaking) {
					mPosDelta.y -= 0.15f;
				}
			}
		} else {
			mIsFlying = false;
		}
		mNoGravity = mIsFlying;

		// LocalPlayer::aiStep() L448-451: using item multiplies strafeInput & forwardInput by 0.35f
		bool usingItemSlow = gBowPulling || (gItemUseSlowTimer > 0.0f);
		if (usingItemSlow) {
			moveStrafe *= 0.35f;
			moveForward *= 0.35f;
		}

		// Mob::aiStep() (Mob.cpp L1117-1133)
		bool v7 = isInWater(true);
		bool v8 = isInLava();
		if (jumpHeld) {
			if (v7 || v8) {
				mPosDelta.y += 0.04f;
			} else if (mOnGround) {
				// Mob::jumpFromGround() (Mob.cpp L1355-1357)
				mPosDelta.y = 0.42f;
				if (!gGuiContext.isCreative) {
					gGuiContext.exhaustion += 0.2f;
				}
			}
		}
		moveStrafe *= 0.98f;
		moveForward *= 0.98f;

		float prevX = mCurrPos.x;
		float prevZ = mCurrPos.z;

		// Player::travel / Mob::travel (Player.cpp L789-800, Mob.cpp L930-1046)
		auto runMobTravel = [&](float jumpMovFactor) {
			if (isInWater(true)) {
				float startPosY = mCurrPos.y;
				moveRelative(moveStrafe, moveForward, 0.02f);
				move(mPosDelta);
				mPosDelta.y = (mPosDelta.y * 0.8f) - 0.02f;
				mPosDelta.x *= 0.8f;
				mPosDelta.z *= 0.8f;
				if (mHorizontalCollision) {
					if (isFree(mPosDelta.x, ((mPosDelta.y + 0.6f) - mCurrPos.y) + startPosY, mPosDelta.z)) {
						mPosDelta.y = 0.3f;
					}
				}
				return;
			}
			if (isInLava()) {
				float startPosY = mCurrPos.y;
				moveRelative(moveStrafe, moveForward, 0.02f);
				move(mPosDelta);
				mPosDelta.y = (mPosDelta.y * 0.5f) - 0.02f;
				mPosDelta.x *= 0.5f;
				mPosDelta.z *= 0.5f;
				if (mHorizontalCollision) {
					if (isFree(mPosDelta.x, ((mPosDelta.y + 0.6f) - mCurrPos.y) + startPosY, mPosDelta.z)) {
						mPosDelta.y = 0.3f;
					}
				}
				return;
			}

			float friction = 0.91f;
			if (mOnGround) {
				BlockID v21 = getRegion().getBlockID(
					Math::floor(mCurrPos.x),
					Math::floor(mBB.min.y - 0.5f),
					Math::floor(mCurrPos.z)
				);
				if (v21 <= 0 || !Block::mBlocks[v21]) {
					friction = 0.546f;
				} else {
					friction = Block::mBlocks[v21]->getFriction() * 0.91f;
				}
			}

			// Player::getBaseSpeed() = 0.1f, Player::getWalkingSpeedModifier() = 1.0f
			float speed = 0.1f;
			float modifier = 1.0f;
			float acc = mOnGround
				? ((speed * modifier) * (0.16277f / (friction * friction * friction)))
				: jumpMovFactor;
			moveRelative(moveStrafe, moveForward, acc);

			float v32 = 0.91f;
			if (mOnGround) {
				BlockID v31 = getRegion().getBlockID(
					Math::floor(mCurrPos.x),
					Math::floor(mBB.min.y - 0.5f),
					Math::floor(mCurrPos.z)
				);
				if (v31 <= 0 || !Block::mBlocks[v31]) {
					v32 = 0.546f;
				} else {
					v32 = Block::mBlocks[v31]->getFriction() * 0.91f;
				}
			}

			if (onLadder()) {
				mFallDistance = 0.0f;
				if (mPosDelta.y < -0.15f) {
					mPosDelta.y = -0.15f;
				}
				if (isSneaking() && mPosDelta.y < 0.0f) {
					mPosDelta.y = 0.0f;
				}
			}

			move(mPosDelta);

			if (mHorizontalCollision && onLadder()) {
				mPosDelta.y = 0.2f;
			}

			mPosDelta.y = (mPosDelta.y - 0.08f) * 0.98f;
			float v34 = v32 * flyIdleMod;
			mPosDelta.x *= v34;
			mPosDelta.z *= v34;
		};

		if (mIsFlying) {
			float savedMotionY = mPosDelta.y;
			runMobTravel(0.05f);
			mPosDelta.y = savedMotionY * 0.6f;
		} else {
			runMobTravel(0.02f);
		}

		float stepDx = mCurrPos.x - prevX;
		float stepDz = mCurrPos.z - prevZ;
		float horizStep = std::sqrt(stepDx * stepDx + stepDz * stepDz);
		if (!gGuiContext.isCreative && !mIsFlying && horizStep > 0.0005f) {
			float moveRate = v7 ? 0.09f : 0.07f;
			gGuiContext.exhaustion += horizStep * moveRate;
		}
		// Player::aiStep() (Player.cpp L821-848)
		float v2 = std::sqrt(mPosDelta.x * mPosDelta.x + mPosDelta.z * mPosDelta.z);
		if (v2 > 0.1f) v2 = 0.1f;
		if (!mOnGround || gGuiContext.health <= 0) v2 = 0.0f;
		mCurrSmoothBob += (v2 - mCurrSmoothBob) * 0.4f;
		float v3 = std::atan(-(mPosDelta.y * 0.2f));
		float v6 = (!mOnGround && gGuiContext.health > 0) ? (v3 * 15.0f) : 0.0f;
		mCurrTilt += (v6 - mCurrTilt) * 0.8f;

		// MCPE 0.8.1 Mob::tick (L358-406) body yaw tracking + Mob::updateWalkAnim (L1047-1088)
		auto wrapDeg = [](float d) {
			while (d < -180.0f) d += 360.0f;
			while (d >= 180.0f) d -= 360.0f;
			return d;
		};
		float headYawDeg = static_cast<float>(yaw);
		float targetBodyYaw = mCurrBodyYaw;
		if (horizStep > 0.05f) {
			targetBodyYaw = std::atan2(stepDz, stepDx) * (180.0f / 3.14159265f);
		}
		if (mSwingProgress > 0.0f || gBowPulling) {
			targetBodyYaw = headYawDeg;
		}
		float j = wrapDeg(targetBodyYaw - mCurrBodyYaw);
		float newBodyYaw = mCurrBodyYaw + j * 0.3f;
		float k = wrapDeg(headYawDeg - newBodyYaw);
		if (k < -75.0f) k = -75.0f;
		if (k >= 75.0f) k = 75.0f;
		newBodyYaw = headYawDeg - k;
		if (k * k > 2500.0f) {
			newBodyYaw += k * 0.2f;
		}
		mCurrBodyYaw = newBodyYaw;
		while (mCurrBodyYaw - mPrevBodyYaw < -180.0f) mPrevBodyYaw -= 360.0f;
		while (mCurrBodyYaw - mPrevBodyYaw >= 180.0f) mPrevBodyYaw += 360.0f;

		float walkMult = (mHurtTime > 0) ? 1.5f : 1.0f;
		if (!mOnGround && mPosDelta.y > 0.0f) {
			walkMult *= 0.35f;
		}
		float v9 = horizStep * 4.0f;
		if (v9 == 0.0f) {
			v9 = std::abs(wrapDeg(mCurrBodyYaw - mPrevBodyYaw)) * 0.15f;
			if (v9 > 0.5f) v9 = 0.5f;
		}
		if (v9 > 1.0f) v9 = 1.0f;
		mCurrWalkAnimSpeed += (v9 * walkMult - mCurrWalkAnimSpeed) * 0.4f;
		mCurrWalkAnimPos += mCurrWalkAnimSpeed;

		if (mCurrPos.y < 0.0f && !gGuiContext.isCreative && gGuiContext.health > 0) {
			// Mob::outOfWorld() (Entity.cpp L956-958, Mob.cpp L742-748): 4 damage per tick below Y=0
			gGuiContext.applyDamage(4, true);
		}
	}

	void UpdateCam(float deltaTime)
	{
		float dt = std::min(std::max(deltaTime, 0.0001f), 0.1f);
		double xposIn = glinput.mouseXPos;
		double yposIn = glinput.mouseYPos;

		if (!gCursorLocked) {
			firstMouse = true;
		} else if (mIgnoreMouseFrames > 0) {
			lastX = xposIn;
			lastY = yposIn;
			firstMouse = false;
			--mIgnoreMouseFrames;
		} else {
			if (firstMouse)
			{
				lastX = xposIn;
				lastY = yposIn;
				firstMouse = false;
			}

			float xoffset = static_cast<float>(xposIn - lastX);
			float yoffset = static_cast<float>(lastY - yposIn);
			lastX = xposIn;
			lastY = yposIn;

			float sensitivity = 0.02f + gGuiOptions.sensitivity * 0.16f;
			xoffset *= sensitivity;
			yoffset *= sensitivity * (gGuiOptions.invertMouse ? -1.0f : 1.0f);

			yaw += xoffset;
			pitch += yoffset;

			if (pitch > 89.0)
				pitch = 89.0;
			if (pitch < -89.0)
				pitch = -89.0;
		}

		glm::vec3 direction;
		direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		direction.y = sin(glm::radians(pitch));
		direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		cameraFront = glm::normalize(direction);

		direction.x = cos(glm::radians(yaw));
		direction.y = 0;
		direction.z = sin(glm::radians(yaw));
		cameraMoveFront = glm::normalize(direction);

		if (!gGuiContext.isCreative) {
			mIsFlying = false;
		}

		if (mSwingProgress > 0.0f) {
			mSwingProgress += dt * 4.2f;
			if (mSwingProgress >= 1.0f) {
				mSwingProgress = 0.0f;
			}
		}

		constexpr float TICK_DT = 0.05f;
		mTickAccum += dt;
		if (mTickAccum > 0.25f) mTickAccum = 0.25f;
		while (mTickAccum >= TICK_DT) {
			mTickAccum -= TICK_DT;
			stepPhysicsTick();
		}

		float alpha = mTickAccum / TICK_DT;
		mAlpha = alpha;
		mPos = glm::mix(mPrevPos, mCurrPos, alpha);
		mRenderFeetY = mPrevFeetY + (mCurrFeetY - mPrevFeetY) * alpha;
		mWalkDist = mPrevWalkDist + (mCurrWalkDist - mPrevWalkDist) * alpha;
		mSmoothBob = mPrevSmoothBob + (mCurrSmoothBob - mPrevSmoothBob) * alpha;
		mTilt = mPrevTilt + (mCurrTilt - mPrevTilt) * alpha;
		mWalkAnimSpeed = std::min(1.0f, mPrevWalkAnimSpeed + (mCurrWalkAnimSpeed - mPrevWalkAnimSpeed) * alpha);
		mWalkAnimPos = mCurrWalkAnimPos - (1.0f - alpha) * mCurrWalkAnimSpeed;
		float bodyDiff = mCurrBodyYaw - mPrevBodyYaw;
		while (bodyDiff < -180.0f) bodyDiff += 360.0f;
		while (bodyDiff >= 180.0f) bodyDiff -= 360.0f;
		mBodyYaw = mPrevBodyYaw + bodyDiff * alpha;
	}
};




void Camera::moveBBs(const Vec3& pos) {
	mBB.move(pos);
	for (auto& bb : mSubBBs) {
		bb.move(pos);
	}
}

// Exact 1:1 port of MCPE 0.8.1 Entity::move(dx, dy, dz) (Entity.cpp L425-771)
void Camera::move(const Vec3& posIn) {
	float movX = posIn.x;
	float movY = posIn.y;
	float movZ = posIn.z;

	if (std::abs(movX) > 500.0f || std::abs(movY) > 500.0f || std::abs(movZ) > 500.0f) {
		movX = movY = movZ = 0.0f;
	}

	if (mNoPhysics) {
		mBB.move(movX, movY, movZ);
		mCurrFeetY = mBB.min.y;
		mCurrPos.x = (mBB.min.x + mBB.max.x) * 0.5f;
		mCurrPos.y = (mBB.min.y + mHeightOffset) - mSlideOffset.y;
		mCurrPos.z = (mBB.min.z + mBB.max.z) * 0.5f;
		mPos = mCurrPos;
		mRenderFeetY = mCurrFeetY;
		return;
	}

	float startPosX = mCurrPos.x;
	float startPosZ = mCurrPos.z;

	// Entity.cpp L518-526: web slow-down
	if (mIsStuckInWeb) {
		mIsStuckInWeb = false;
		mPosDelta.x = 0.0f;
		mPosDelta.y = 0.0f;
		mPosDelta.z = 0.0f;
		movX *= 0.25f;
		movY *= 0.05f;
		movZ *= 0.25f;
	}
	if (mRegion && Block::mSoulSand) {
		BlockPos soulPos(Math::floor(mCurrPos.x), Math::floor(mBB.min.y + 0.05f), Math::floor(mCurrPos.z));
		if (mRegion->getBlockID(soulPos) == Block::mSoulSand->mID) {
			movX *= 0.4f;
			movZ *= 0.4f;
		}
	}

	AABB boundingBox = mBB;
	BlockSource& reg = getRegion();

	// Entity.cpp L528-603: sneaking edge clamp (minY - 1.0f, maxY - 1.0f, step 0.05f)
	bool v34 = false;
	if (mOnGround && isSneaking() && !mIsFlying) {
		while (movX != 0.0f) {
			AABB testX(mBB.min.x + movX, mBB.min.y - 1.0f, mBB.min.z,
			           mBB.max.x + movX, mBB.max.y - 1.0f, mBB.max.z);
			if (!reg.fetchCollisionShapes(reg, testX, nullptr, false, nullptr).empty()) {
				break;
			}
			if (movX < 0.05f && movX >= -0.05f) {
				movX = 0.0f;
				break;
			}
			if (movX <= 0.0f) movX += 0.05f;
			else movX -= 0.05f;
		}
		while (movZ != 0.0f) {
			AABB testZ(mBB.min.x, mBB.min.y - 1.0f, mBB.min.z + movZ,
			           mBB.max.x, mBB.max.y - 1.0f, mBB.max.z + movZ);
			if (!reg.fetchCollisionShapes(reg, testZ, nullptr, false, nullptr).empty()) {
				break;
			}
			if (movZ < 0.05f && movZ >= -0.05f) {
				movZ = 0.0f;
				break;
			}
			if (movZ <= 0.0f) movZ += 0.05f;
			else movZ -= 0.05f;
		}
		while (movX != 0.0f && movZ != 0.0f) {
			AABB testXZ(mBB.min.x + movX, mBB.min.y - 1.0f, mBB.min.z + movZ,
			            mBB.max.x + movX, mBB.max.y - 1.0f, mBB.max.z + movZ);
			if (!reg.fetchCollisionShapes(reg, testXZ, nullptr, false, nullptr).empty()) {
				break;
			}
			if (movX < 0.05f && movX >= -0.05f) movX = 0.0f;
			else if (movX <= 0.0f) movX += 0.05f;
			else movX -= 0.05f;

			if (movZ < 0.05f && movZ >= -0.05f) movZ = 0.0f;
			else if (movZ <= 0.0f) movZ += 0.05f;
			else movZ -= 0.05f;
		}
		v34 = true;
	}

	// Exact 1:1 MCPE 0.8.1 AABB::clipYCollide / clipXCollide / clipZCollide (AABB.cpp L189-265)
	auto clipYCollide081 = [](const AABB& cube, const AABB& ent, float dy) -> float {
		if (ent.max.x > cube.min.x && ent.min.x < cube.max.x && ent.max.z > cube.min.z && ent.min.z < cube.max.z) {
			float v4 = dy;
			if (dy > 0.0f && ent.max.y <= cube.min.y) {
				float d = cube.min.y - ent.max.y;
				if (d < dy) v4 = d;
			}
			if (v4 < 0.0f && ent.min.y >= cube.max.y) {
				float d = cube.max.y - ent.min.y;
				if (d > v4) return d;
			}
			return v4;
		}
		return dy;
	};
	auto clipXCollide081 = [](const AABB& cube, const AABB& ent, float dx) -> float {
		if (ent.max.y > cube.min.y && ent.min.y < cube.max.y && ent.max.z > cube.min.z && ent.min.z < cube.max.z) {
			float v4 = dx;
			if (dx > 0.0f && ent.max.x <= cube.min.x) {
				float d = cube.min.x - ent.max.x;
				if (d < dx) v4 = d;
			}
			if (v4 < 0.0f && ent.min.x >= cube.max.x) {
				float d = cube.max.x - ent.min.x;
				if (d > v4) return d;
			}
			return v4;
		}
		return dx;
	};
	auto clipZCollide081 = [](const AABB& cube, const AABB& ent, float dz) -> float {
		if (ent.max.x > cube.min.x && ent.min.x < cube.max.x && ent.max.y > cube.min.y && ent.min.y < cube.max.y) {
			float v4 = dz;
			if (dz > 0.0f && ent.max.z <= cube.min.z) {
				float d = cube.min.z - ent.max.z;
				if (d < dz) v4 = d;
			}
			if (v4 < 0.0f && ent.min.z >= cube.max.z) {
				float d = cube.max.z - ent.min.z;
				if (d > v4) return d;
			}
			return v4;
		}
		return dz;
	};

	// Entity.cpp L604-653: resolve Y, then X, then Z
	float v36 = movY;
	float v43 = movX;
	float v42 = movZ;
	std::vector<AABB>& v38 = reg.fetchCollisionShapes(reg, mBB.expanded(Vec3(movX, movY, movZ)), nullptr, false, nullptr);
	for (const auto& cube : v38) {
		v36 = clipYCollide081(cube, mBB, v36);
	}
	mBB.move(0.0f, v36, 0.0f);

	bool fallingFlag = mOnGround || (movY != v36 && movY < 0.0f);

	for (const auto& cube : v38) {
		v43 = clipXCollide081(cube, mBB, v43);
	}
	mBB.move(v43, 0.0f, 0.0f);

	for (const auto& cube : v38) {
		v42 = clipZCollide081(cube, mBB, v42);
	}
	mBB.move(0.0f, 0.0f, v42);

	// Entity.cpp L654-717: stepHeight (0.5f) when fallingFlag && ySize < 0.05f && (movX != v43 || movZ != v42)
	float stepHeight = mMaxAutoStep;
	if (stepHeight > 0.0f && fallingFlag && mSlideOffset.y < 0.05f && (movX != v43 || movZ != v42)) {
		AABB v96 = mBB;
		mBB = boundingBox;
		std::vector<AABB>& stepCubes = reg.fetchCollisionShapes(reg, mBB.expanded(Vec3(movX, stepHeight, movZ)), nullptr, false, nullptr);
		for (const auto& cube : stepCubes) {
			stepHeight = clipYCollide081(cube, mBB, stepHeight);
		}
		mBB.move(0.0f, stepHeight, 0.0f);

		float v64 = movX;
		for (const auto& cube : stepCubes) {
			v64 = clipXCollide081(cube, mBB, v64);
		}
		mBB.move(v64, 0.0f, 0.0f);

		float v63 = movZ;
		for (const auto& cube : stepCubes) {
			v63 = clipZCollide081(cube, mBB, v63);
		}
		mBB.move(0.0f, 0.0f, v63);

		if ((v42 * v42 + v43 * v43) < (v63 * v63 + v64 * v64)) {
			v42 = v63;
			v43 = v64;
			v36 = stepHeight;
			mSlideOffset.y += 0.5f;
		} else {
			mBB = v96;
		}
	}

	// Entity.cpp L718-770: update position, collision flags, fall damage, step sounds, ySize decay, tile collisions
	mCurrFeetY = mBB.min.y;
	mCurrPos.x = (mBB.min.x + mBB.max.x) * 0.5f;
	mCurrPos.y = (mBB.min.y + mHeightOffset) - mSlideOffset.y;
	mCurrPos.z = (mBB.min.z + mBB.max.z) * 0.5f;
	mPos = mCurrPos;
	mRenderFeetY = mCurrFeetY;

	mHorizontalCollision = (movX != v43) || (movZ != v42);
	mVerticalCollision = (movY != v36);
	mOnGround = (movY != v36 && movY < 0.0f);
	mCollision = mHorizontalCollision || mVerticalCollision;

	checkFallDamage(v36, mOnGround);

	if (movX != v43) {
		mPosDelta.x = 0.0f;
	}
	if (movY != v36) {
		mPosDelta.y = 0.0f;
	}
	if (movZ != v42) {
		mPosDelta.z = 0.0f;
	}

	// Entity.cpp L744-767: walkDist (field_BC) accumulation & step sounds when !v34 (not sneaking on ground)
	if (!v34 && !mIsFlying) {
		float dxMoved = mCurrPos.x - startPosX;
		float dzMoved = mCurrPos.z - startPosZ;
		float v86 = std::sqrt(dxMoved * dxMoved + dzMoved * dzMoved);
		mCurrWalkDist += v86 * 0.6f;
		int v87 = Math::floor(mCurrPos.x);
		int v88 = Math::floor((mCurrPos.y - 0.2f) - mHeightOffset);
		int v89 = Math::floor(mCurrPos.z);
		BlockID v90 = reg.getBlockID(v87, v88, v89);
		if (v90 == 0) {
			BlockID belowId = reg.getBlockID(v87, v88 - 1, v89);
			BlockID fenceId = Block::mFence ? Block::mFence->mID : BlockID(85);
			BlockID gateId = Block::mFenceGateOak ? Block::mFenceGateOak->mID : BlockID(107);
			BlockID wallId = Block::mCobblestoneWall ? Block::mCobblestoneWall->mID : BlockID(139);
			if (belowId == fenceId || belowId == gateId || belowId == wallId) {
				v90 = belowId;
			}
		}
		if (mCurrWalkDist > static_cast<float>(mNextStepSound) && v90 > 0) {
			mNextStepSound = static_cast<int>(mCurrWalkDist) + 1;
			const Block* stepBlk = Block::mBlocks[v90];
			if (stepBlk && !stepBlk->getMaterial().isLiquid()) {
				SoundEngine::play(SoundEngine::SND_STEP, 0.25f, 1.0f);
			}
		}
	}

	mSlideOffset.y *= 0.4f;
	checkTileCollisions();
}


















void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
	glinput.mouseXPos = xposIn;
	glinput.mouseYPos = yposIn;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (gGuiContext.currentScreen) {
		int guiX = static_cast<int>(glinput.mouseXPos) * gGuiContext.guiWidth / std::max(1, screenx);
		int guiY = static_cast<int>(glinput.mouseYPos) * gGuiContext.guiHeight / std::max(1, screeny);
		if (action == GLFW_PRESS) {
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				SoundEngine::play(SoundEngine::SND_CLICK, 0.5f);
			}
			gGuiContext.currentScreen->mouseClicked(guiX, guiY, button);
		} else if (action == GLFW_RELEASE) {
			gGuiContext.currentScreen->mouseReleased(guiX, guiY, button);
		}
	} else if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT && gGuiContext.state == GameState::InGame && !gCursorLocked) {
		int guiX = static_cast<int>(glinput.mouseXPos) * gGuiContext.guiWidth / std::max(1, screenx);
		int guiY = static_cast<int>(glinput.mouseYPos) * gGuiContext.guiHeight / std::max(1, screeny);
		if (gGuiHud.handleHudClick(guiX, guiY, gGuiContext.guiWidth, gGuiContext.guiHeight, &gGuiContext)) {
			return;
		}
	}

	bool pressed = (action != GLFW_RELEASE);

	if (pressed != glinput.mlookup_table[button])
	{
		glinput.mlast_table[button] = pressed ? 1 : -1;
		glinput.mlookup_table[button] = pressed;
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	if (gGuiContext.currentScreen && yoffset != 0.0) {
		gGuiContext.currentScreen->mouseScrolled(yoffset);
		return;
	}
	if (gGuiContext.state == GameState::InGame && !gGuiContext.currentScreen && yoffset != 0.0) {
		int step = (yoffset > 0.0) ? -1 : 1;
		int next = (gGuiContext.selectedHotbarSlot + step) % 9;
		if (next < 0) next += 9;
		gGuiContext.selectedHotbarSlot = next;
		gGuiHud.selectedSlot = next;
	}
}
//todo move more stuff to renderer init
GLFWwindow* renderThreadInit()
{

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);//3.3 minimum version (thats what this is linked with)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);//We need this for rendering
	glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
	if (mode && mode->width > 0 && mode->height > 0) {
		screenx = static_cast<int>(mode->width * 0.85f);
		screeny = static_cast<int>(mode->height * 0.85f);
	} else {
		screenx = 1280;
		screeny = 720;
	}

	GLFWwindow* window = glfwCreateWindow(screenx, screeny, "Minecraft PE 1.0.0 Restoration", NULL, NULL);
	if (window && mode) {
		glfwSetWindowPos(window, (mode->width - screenx) / 2, (mode->height - screeny) / 2);
	}
	if (window == NULL)
	{
		DEBUG_FAIL("Failed to make GLFW window");
		return nullptr;
	}
	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		DEBUG_FAIL("Failed to init GLAD");
		glfwTerminate();
		return nullptr;
	}

	glfwGetFramebufferSize(window, &screenx, &screeny);
	glViewport(0, 0, screenx, screeny);

	glViewport(0, 0, screenx, screeny); //this sets the screen size

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);//Oh no the user was annoying and changed the window size. FIX IT

	glEnable(GL_DEPTH_TEST);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // hide + lock cursor
	//glLineWidth(5.0f);

	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_BACK);
	//glFrontFace(GL_CW);
	//glEnable(GL_FRAMEBUFFER_SRGB);

	//glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);


	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCharCallback(window, char_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, scroll_callback);

	gGuiFont.init("font/default8.png");
	gGuiContext.font = &gGuiFont;
	gGuiOptions.load("options.txt");
	gGuiContext.options = &gGuiOptions;
	SoundEngine::init();
	updateGuiScale();
	gGuiContext.setScreen(new StartMenuScreen());
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	return window;
}



//decompiled
void /*MinecraftClient::*/updateFoliageColors()
{
	mce::TextureGroup* textureGroup = &texgroup;//mTextures;

	auto foliage = textureGroup->getTexturePair(
		ResourceLocation("textures/colormap/foliage")
	);

	auto evergreen = textureGroup->getTexturePair(
		ResourceLocation("textures/colormap/evergreen")
	);

	auto birch = textureGroup->getTexturePair(
		ResourceLocation("textures/colormap/birch")
	);

	auto grass = textureGroup->getTexturePair(
		ResourceLocation("textures/colormap/grass")
	);

	FoliageColor::setFoliageColorPalette(*foliage.unwrap().getTextureData());

	FoliageColor::setFoliageEvergreenColorPalette(*evergreen.unwrap().getTextureData());

	FoliageColor::setFoliageBirchColorPalette(*birch.unwrap().getTextureData());

	FoliageColor::setGrassColorPalette(*grass.unwrap().getTextureData());
}

namespace {

struct SpiralOffset {
	int dx, dz, d2;
};

const std::vector<SpiralOffset>& getCachedSpiralOffsets(int radius) {
	static std::unordered_map<int, std::vector<SpiralOffset>> sCache;
	auto it = sCache.find(radius);
	if (it != sCache.end()) {
		return it->second;
	}
	std::vector<SpiralOffset> out;
	out.reserve(static_cast<size_t>((2 * radius + 1) * (2 * radius + 1)));
	for (int x = -radius; x <= radius; ++x) {
		for (int z = -radius; z <= radius; ++z) {
			out.push_back({x, z, x * x + z * z});
		}
	}
	std::sort(out.begin(), out.end(), [](const SpiralOffset& a, const SpiralOffset& b) {
		return a.d2 < b.d2;
	});
	return sCache.emplace(radius, std::move(out)).first->second;
}

struct FrustumPlanes {
	glm::vec4 planes[6]{};

	void extract(const glm::mat4& clip) {
		const glm::vec4 row0(clip[0][0], clip[1][0], clip[2][0], clip[3][0]);
		const glm::vec4 row1(clip[0][1], clip[1][1], clip[2][1], clip[3][1]);
		const glm::vec4 row2(clip[0][2], clip[1][2], clip[2][2], clip[3][2]);
		const glm::vec4 row3(clip[0][3], clip[1][3], clip[2][3], clip[3][3]);
		planes[0] = row3 + row0; // Left
		planes[1] = row3 - row0; // Right
		planes[2] = row3 + row1; // Bottom
		planes[3] = row3 - row1; // Top
		planes[4] = row3 + row2; // Near
		planes[5] = row3 - row2; // Far
		for (int i = 0; i < 6; ++i) {
			const float len = glm::length(glm::vec3(planes[i]));
			if (len > 1e-6f) {
				planes[i] /= len;
			}
		}
	}

	bool isBoxVisible(const BlockPos& minPos, float pad, const glm::vec3& camPos, float extentX = 16.0f, float extentZ = 16.0f) const {
		const float minX = static_cast<float>(minPos.x) - pad - camPos.x;
		const float minY = static_cast<float>(minPos.y) - pad - camPos.y;
		const float minZ = static_cast<float>(minPos.z) - pad - camPos.z;
		const float maxX = static_cast<float>(minPos.x) + extentX + pad - camPos.x;
		const float maxY = static_cast<float>(minPos.y + 16) + pad - camPos.y;
		const float maxZ = static_cast<float>(minPos.z) + extentZ + pad - camPos.z;
		for (int i = 0; i < 6; ++i) {
			const glm::vec4& p = planes[i];
			const float px = (p.x >= 0.0f) ? maxX : minX;
			const float py = (p.y >= 0.0f) ? maxY : minY;
			const float pz = (p.z >= 0.0f) ? maxZ : minZ;
			if (p.x * px + p.y * py + p.z * pz + p.w < 0.0f) {
				return false;
			}
		}
		return true;
	}
};

std::unordered_set<ChunkPos>* gActiveDirtyMeshChunks = nullptr;

void rebuildSubChunkAt(const ChunkPos& cp, int subY,
                       std::unordered_map<ChunkPos, RenderChunk*[8]>& rc,
                       std::unique_ptr<RenderChunkBuilder>& chunkBuilder,
                       const Vec3& camPos) {
	if (subY < 0 || subY >= 8) return;
	auto it = rc.find(cp);
	if (it == rc.end() || it->second[subY] == nullptr) return;
	RenderChunk* chunkPart = it->second[subY];
	chunkPart->setDirty(Tick(0), true);
	if (!chunkPart->isBuildState(RenderChunk::BuildState::Dirty)) {
		if (gActiveDirtyMeshChunks) {
			gActiveDirtyMeshChunks->insert(cp);
		}
		return;
	}
	chunkPart->startRebuild(std::move(chunkBuilder), false);
	chunkPart->rebuild(gGuiOptions.fancyGraphics, true, 0, gGuiOptions.greedyMeshing, false);
	chunkBuilder = chunkPart->endRebuild(camPos);
	ChunkPerfMetrics::instance.meshRebuilds.fetch_add(1, std::memory_order_relaxed);
}

void rebuildChunksForBlock(const BlockPos& pos,
                           std::unordered_map<ChunkPos, RenderChunk*[8]>& rc,
                           std::unique_ptr<RenderChunkBuilder>& chunkBuilder,
                           const Vec3& camPos) {
	ChunkPos baseCp(pos);
	int subY = pos.y >> 4;
	int lx = pos.x & 15;
	int ly = pos.y & 15;
	int lz = pos.z & 15;

	rebuildSubChunkAt(baseCp, subY, rc, chunkBuilder, camPos);
	if (lx == 0) rebuildSubChunkAt(ChunkPos(baseCp.x - 1, baseCp.z), subY, rc, chunkBuilder, camPos);
	if (lx == 15) rebuildSubChunkAt(ChunkPos(baseCp.x + 1, baseCp.z), subY, rc, chunkBuilder, camPos);
	if (lz == 0) rebuildSubChunkAt(ChunkPos(baseCp.x, baseCp.z - 1), subY, rc, chunkBuilder, camPos);
	if (lz == 15) rebuildSubChunkAt(ChunkPos(baseCp.x, baseCp.z + 1), subY, rc, chunkBuilder, camPos);
	if (ly == 0 && subY > 0) rebuildSubChunkAt(baseCp, subY - 1, rc, chunkBuilder, camPos);
	if (ly == 15 && subY < 7) rebuildSubChunkAt(baseCp, subY + 1, rc, chunkBuilder, camPos);
}

void updateLightingAndRebuildChunks(BlockSource& region,
                                    const BlockPos& pos,
                                    std::unordered_map<ChunkPos, RenderChunk*[8]>& rc,
                                    std::unique_ptr<RenderChunkBuilder>& chunkBuilder,
                                    const Vec3& camPos) {
	WorldLighting::TouchedSubChunkMap touchedSubChunks;
	WorldLighting::updateLocalLighting(region, pos, 15, nullptr, &touchedSubChunks);

	// Rebuild the edited block's subchunk & immediate touching seams immediately;
	// wider lighting spill updates are queued into DirtyChunkSorter via setDirty().
	rebuildChunksForBlock(pos, rc, chunkBuilder, camPos);

	ChunkPos baseCp(pos);
	int baseSubY = pos.y >> 4;
	int lx = pos.x & 15;
	int ly = pos.y & 15;
	int lz = pos.z & 15;
	for (const auto& kv : touchedSubChunks) {
		auto it = rc.find(kv.first);
		if (it == rc.end()) continue;
		for (int sy = 0; sy < 8; ++sy) {
			if ((kv.second & (1u << sy)) == 0) continue;
			const bool alreadyRebuilt =
				(kv.first == baseCp && (sy == baseSubY || (ly == 0 && sy == baseSubY - 1) || (ly == 15 && sy == baseSubY + 1))) ||
				(sy == baseSubY && ((lx == 0 && kv.first == ChunkPos(baseCp.x - 1, baseCp.z)) ||
				                    (lx == 15 && kv.first == ChunkPos(baseCp.x + 1, baseCp.z)) ||
				                    (lz == 0 && kv.first == ChunkPos(baseCp.x, baseCp.z - 1)) ||
				                    (lz == 15 && kv.first == ChunkPos(baseCp.x, baseCp.z + 1))));
			if (alreadyRebuilt) continue;
			if (it->second[sy]) {
				it->second[sy]->setDirty(Tick(0), false);
				if (gActiveDirtyMeshChunks) {
					gActiveDirtyMeshChunks->insert(kv.first);
				}
			}
		}
	}
}

void drawBlockSelectionOutline(const BlockPos& pos, const AABB& localShape,
                               const glm::mat4& projection, const glm::mat4& view) {
	const float eps = 0.0025f;
	float minX = pos.x + localShape.min.x - eps;
	float minY = pos.y + localShape.min.y - eps;
	float minZ = pos.z + localShape.min.z - eps;
	float maxX = pos.x + localShape.max.x + eps;
	float maxY = pos.y + localShape.max.y + eps;
	float maxZ = pos.z + localShape.max.z + eps;

	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(projection));
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(view));

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(2.0f);
	glColor4f(0.0f, 0.0f, 0.0f, 0.45f);

	glBegin(GL_LINES);
	// Bottom face
	glVertex3f(minX, minY, minZ); glVertex3f(maxX, minY, minZ);
	glVertex3f(maxX, minY, minZ); glVertex3f(maxX, minY, maxZ);
	glVertex3f(maxX, minY, maxZ); glVertex3f(minX, minY, maxZ);
	glVertex3f(minX, minY, maxZ); glVertex3f(minX, minY, minZ);
	// Top face
	glVertex3f(minX, maxY, minZ); glVertex3f(maxX, maxY, minZ);
	glVertex3f(maxX, maxY, minZ); glVertex3f(maxX, maxY, maxZ);
	glVertex3f(maxX, maxY, maxZ); glVertex3f(minX, maxY, maxZ);
	glVertex3f(minX, maxY, maxZ); glVertex3f(minX, maxY, minZ);
	// Vertical edges
	glVertex3f(minX, minY, minZ); glVertex3f(minX, maxY, minZ);
	glVertex3f(maxX, minY, minZ); glVertex3f(maxX, maxY, minZ);
	glVertex3f(maxX, minY, maxZ); glVertex3f(maxX, maxY, maxZ);
	glVertex3f(minX, minY, maxZ); glVertex3f(minX, maxY, maxZ);
	glEnd();

	glDisable(GL_BLEND);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

void drawBlockBreakOverlay(const BlockPos& pos, const AABB& localShape, float progress,
                           const glm::mat4& projection, const glm::mat4& view) {
	if (progress <= 0.0f) return;
	float p = std::max(0.0f, std::min(1.0f, progress));
	int stage = std::max(0, std::min(9, static_cast<int>(p * 10.0f)));
	float minX = pos.x + localShape.min.x;
	float minY = pos.y + localShape.min.y;
	float minZ = pos.z + localShape.min.z;
	float maxX = pos.x + localShape.max.x;
	float maxY = pos.y + localShape.max.y;
	float maxZ = pos.z + localShape.max.z;

	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(projection));
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(view));

	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	GuiComponent::bindTexture("textures/environment/destroy_stage_" + std::to_string(stage) + ".png");
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_ZERO);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.0f, -1.0f);
	glDepthMask(GL_FALSE);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glBegin(GL_QUADS);
	// Bottom (-Y) & Top (+Y)
	glTexCoord2f(0.0f, 0.0f); glVertex3f(minX, minY, minZ);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(maxX, minY, minZ);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(maxX, minY, maxZ);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(minX, minY, maxZ);

	glTexCoord2f(0.0f, 0.0f); glVertex3f(minX, maxY, minZ);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(minX, maxY, maxZ);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(maxX, maxY, maxZ);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(maxX, maxY, minZ);

	// North (-Z) & South (+Z)
	glTexCoord2f(1.0f, 1.0f); glVertex3f(minX, minY, minZ);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(minX, maxY, minZ);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(maxX, maxY, minZ);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(maxX, minY, minZ);

	glTexCoord2f(0.0f, 1.0f); glVertex3f(minX, minY, maxZ);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(maxX, minY, maxZ);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(maxX, maxY, maxZ);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(minX, maxY, maxZ);

	// West (-X) & East (+X)
	glTexCoord2f(0.0f, 1.0f); glVertex3f(minX, minY, minZ);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(minX, minY, maxZ);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(minX, maxY, maxZ);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(minX, maxY, minZ);

	glTexCoord2f(1.0f, 1.0f); glVertex3f(maxX, minY, minZ);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(maxX, maxY, minZ);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(maxX, maxY, maxZ);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(maxX, minY, maxZ);
	glEnd();

	glDisable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(0.0f, 0.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

struct MiningTargetInfo {
	bool canBreak = false;
	bool canHarvest = false;
	float breakTime = 0.25f;
	bool effectiveTool = false;
};

MiningTargetInfo evaluateBlockMining(const Block* block, const ItemInstance& held) {
	MiningTargetInfo out;
	if (!block) return out;
	float hardness = block->getDestroySpeed();
	if (hardness < 0.0f) return out;
	out.canBreak = true;

	ToolInfo tool = getToolInfo(held);
	const Material& mat = block->getMaterial();

	auto isBlockOneOf = [&](std::initializer_list<const Block*> list) {
		for (const Block* b : list) {
			if (b && block->mID == b->mID) return true;
		}
		return false;
	};

	bool isPickaxeBlock = mat.isType(MaterialType::Stone) || mat.isType(MaterialType::Metal) ||
		mat.isType(MaterialType::Ice) || mat.isType(MaterialType::PackedIce) || mat.isType(MaterialType::Piston) ||
		isBlockOneOf({ Block::mFurnace, Block::mLitFurnace, Block::mStonecutterBench, Block::mCobblestoneWall,
		               Block::mStoneSlab, Block::mObsidian, Block::mCoalOre, Block::mIronOre, Block::mGoldOre,
		               Block::mDiamondOre, Block::mLapisOre, Block::mRedStoneOre, Block::mLitRedStoneOre,
		               Block::mEmeraldOre, Block::mQuartzOre, Block::mRail, Block::mGoldenRail, Block::mDetectorRail });
	bool isAxeBlock = mat.isType(MaterialType::Wood) ||
		isBlockOneOf({ Block::mWorkBench, Block::mChest, Block::mTrappedChest, Block::mWoodPlanks, Block::mLog,
		               Block::mLog2, Block::mWoodenSlab, Block::mBookshelf, Block::mFence, Block::mFenceGateOak,
		               Block::mPumpkin, Block::mLitPumpkin, Block::mMelon });
	bool isShovelBlock = mat.isType(MaterialType::Dirt) || mat.isType(MaterialType::Sand) ||
		mat.isType(MaterialType::TopSnow) || mat.isType(MaterialType::Snow) || mat.isType(MaterialType::Clay) ||
		isBlockOneOf({ Block::mGrass, Block::mDirt, Block::mSand, Block::mGravel, Block::mClay,
		               Block::mSoulSand, Block::mFarmland, Block::mGrassPathBlock, Block::mMycelium });

	if (tool.type == ToolType::Pickaxe && isPickaxeBlock) out.effectiveTool = true;
	else if (tool.type == ToolType::Axe && isAxeBlock) out.effectiveTool = true;
	else if (tool.type == ToolType::Shovel && isShovelBlock) out.effectiveTool = true;
	else if (tool.type == ToolType::Sword && (block == Block::mWeb || mat.isType(MaterialType::Plant) || mat.isType(MaterialType::Vegetable) || mat.isType(MaterialType::Leaves))) out.effectiveTool = true;
	else if (tool.type == ToolType::Shears && isBlockOneOf({ Block::mWool, Block::mLeaves, Block::mLeaves2, Block::mWeb, Block::mTallgrass })) out.effectiveTool = true;

	int reqLevel = -1; // -1 = hand/any
	if (block == Block::mObsidian || block == Block::mGlowingObsidian) {
		reqLevel = 3;
	} else if (isBlockOneOf({ Block::mGoldOre, Block::mGoldBlock, Block::mDiamondOre, Block::mDiamondBlock, Block::mEmeraldOre, Block::mRedStoneOre, Block::mLitRedStoneOre })) {
		reqLevel = 2;
	} else if (isBlockOneOf({ Block::mIronOre, Block::mIronBlock, Block::mLapisOre, Block::mLapisBlock })) {
		reqLevel = 1;
	} else if (!mat.isAlwaysDestroyable() || mat.isType(MaterialType::Stone) || mat.isType(MaterialType::Metal)) {
		reqLevel = 0;
	}

	if (reqLevel < 0) {
		out.canHarvest = true;
	} else {
		out.canHarvest = (tool.type == ToolType::Pickaxe && tool.harvestLevel >= reqLevel);
	}
	if (block == Block::mWeb) {
		out.canHarvest = (tool.type == ToolType::Sword || tool.type == ToolType::Shears);
	} else if (block == Block::mTopSnow || block == Block::mSnow) {
		out.canHarvest = (tool.type == ToolType::Shovel);
	}

	if (hardness <= 0.0001f) {
		out.breakTime = 0.0f;
	} else {
		float speedMult = out.effectiveTool ? tool.speedMultiplier : 1.0f;
		if (block == Block::mWeb && (tool.type == ToolType::Sword || tool.type == ToolType::Shears)) {
			speedMult = 15.0f;
		} else if ((block == Block::mLeaves || block == Block::mLeaves2) && tool.type == ToolType::Shears) {
			speedMult = 15.0f;
		}
		float divisor = out.canHarvest ? 1.5f : 5.0f;
		out.breakTime = std::max(0.05f, hardness * divisor / speedMult);
	}
	return out;
}

bool areChunkNeighborsLoaded(ChunkSource& cs, const ChunkPos& cp) {
	for (int dx = -1; dx <= 1; ++dx) {
		for (int dz = -1; dz <= 1; ++dz) {
			auto* lc = cs.getExistingChunk(ChunkPos(cp.x + dx, cp.z + dz));
			if (!lc || lc->getState() != ChunkState::Loaded) {
				return false;
			}
		}
	}
	return true;
}

void syncFurnaceBlockStates(BlockSource& region, GuiContext& ctx,
                            std::unordered_map<ChunkPos, RenderChunk*[8]>& rc,
                            std::unique_ptr<RenderChunkBuilder>& chunkBuilder,
                            const Vec3& camPos) {
	if (!Block::mFurnace || !Block::mLitFurnace) return;
	for (const auto& kv : ctx.furnaces) {
		BlockPos pos(kv.first.x, kv.first.y, kv.first.z);
		if (!region.hasBlock(pos)) continue;
		FullBlock fb = region.getBlockAndData(pos);
		bool isBurning = kv.second.burnTime > 0.0f;
		if (fb.id == Block::mFurnace->mID && isBurning) {
			region.setBlockAndData(pos, FullBlock(Block::mLitFurnace->mID, fb.data), Block::UPDATE_ALL);
			updateLightingAndRebuildChunks(region, pos, rc, chunkBuilder, camPos);
		} else if (fb.id == Block::mLitFurnace->mID && !isBurning) {
			region.setBlockAndData(pos, FullBlock(Block::mFurnace->mID, fb.data), Block::UPDATE_ALL);
			updateLightingAndRebuildChunks(region, pos, rc, chunkBuilder, camPos);
		}
	}
}

bool openInteractiveBlockScreen(BlockSource& region, const BlockPos& pos, BlockID id, GuiContext& guiCtx) {
	const Block* block = Block::mBlocks[id];
	if (!block) return false;
	if (block == Block::mWorkBench) {
		guiCtx.setScreen(new WorkbenchScreen(false));
		return true;
	}
	if (block == Block::mStonecutterBench) {
		guiCtx.setScreen(new StonecutterScreen());
		return true;
	}
	if (block == Block::mFurnace || block == Block::mLitFurnace) {
		guiCtx.setScreen(new FurnaceScreen(BlockCoordKey{ pos.x, pos.y, pos.z }));
		return true;
	}
	if (block == Block::mChest || block == Block::mTrappedChest) {
		bool isDouble = false;
		BlockCoordKey primary{ pos.x, pos.y, pos.z };
		BlockCoordKey secondary{};
		const FacingID horiz[] = { Facing::NORTH, Facing::SOUTH, Facing::WEST, Facing::EAST };
		for (FacingID f : horiz) {
			BlockPos np = pos + Facing::DIRECTION[f];
			if (region.getBlockID(np) == id) {
				isDouble = true;
				BlockCoordKey neighbor{ np.x, np.y, np.z };
				if (neighbor.x < primary.x || (neighbor.x == primary.x && neighbor.z < primary.z)) {
					secondary = primary;
					primary = neighbor;
				} else {
					secondary = neighbor;
				}
				break;
			}
		}
		guiCtx.setScreen(new ChestScreen(primary, isDouble, secondary));
		return true;
	}
	if (block == Block::mEnderChest) {
		guiCtx.setScreen(new ChestScreen(BlockCoordKey{ pos.x, pos.y, pos.z }, false));
		return true;
	}
	if (block == Block::mBed) {
		BlockPos headPos = pos;
		FullBlock fb = region.getBlockAndData(pos);
		if (!BedBlock::isHeadPiece(fb.data)) {
			int dir = Block::mBed->getBlockState(BlockState::Direction).get<int>(fb.data);
			BlockPos candidate(pos.x + BedBlock::HEAD_DIRECTION_OFFSETS[dir][0],
			                   pos.y,
			                   pos.z + BedBlock::HEAD_DIRECTION_OFFSETS[dir][1]);
			if (region.getBlockID(candidate) == Block::mBed->mID) {
				headPos = candidate;
			}
		}
		BlockPos standPos = headPos.above();
		BlockPos foundStand;
		if (BedBlock::findStandUpPosition(region, headPos, 0, foundStand)) {
			standPos = foundStand;
		}
		guiCtx.hasBedSpawn = true;
		guiCtx.bedSpawnX = standPos.x;
		guiCtx.bedSpawnY = standPos.y;
		guiCtx.bedSpawnZ = standPos.z;

		int64_t dayTime = guiCtx.worldTime % 24000;
		if (dayTime >= 12541 && dayTime < 23458) {
			guiCtx.setScreen(new InBedScreen());
		} else {
			guiCtx.statusBannerText = "You can only sleep at night";
			guiCtx.statusBannerTimer = 2.5f;
		}
		return true;
	}
	return false;
}

struct WorldItemEntity {
	ItemInstance item;
	Vec3 pos;
	Vec3 prevPos;
	Vec3 vel;
	int age = 0;
	int delayBeforePickup = 0;
	float hoverStart = 0.0f;
	bool onGround = false;
	int lifeTime = 6000;
};

static void serializeChunkWorldItems(const ChunkPos& cp,
                                     std::vector<WorldItemEntity>& items,
                                     LevelChunk& lc,
                                     bool removeFromList) {
	std::string encoded;
	StringByteOutput stream(encoded);
	bool anyWritten = false;

	for (size_t i = 0; i < items.size(); ) {
		const WorldItemEntity& e = items[i];
		ChunkPos itemCp(Math::floor(e.pos.x) >> 4, Math::floor(e.pos.z) >> 4);
		if (itemCp == cp) {
			if (!e.item.isNull() && e.item.getStackSize() > 0) {
				CompoundTag tag;
				tag.putInt("id", 64);
				tag.put("Pos", ListTagFloatAdder(e.pos.x)(e.pos.y)(e.pos.z).done());
				tag.put("Motion", ListTagFloatAdder(e.vel.x)(e.vel.y)(e.vel.z).done());
				tag.putShort("Age", static_cast<short>(std::max(0, std::min(e.age, 32767))));
				tag.putShort("PickupDelay", static_cast<short>(std::max(0, std::min(e.delayBeforePickup, 32767))));
				tag.putInt("LifeTime", e.lifeTime);

				auto itemTag = make_unique<CompoundTag>();
				itemTag->putShort("id", static_cast<short>(e.item.getId()));
				itemTag->putShort("Damage", static_cast<short>(e.item.getAuxValue()));
				itemTag->putByte("Count", static_cast<char>(e.item.getStackSize()));
				tag.putCompound("Item", std::move(itemTag));

				NbtIo::write(&tag, stream);
				anyWritten = true;
			}
			if (removeFromList) {
				items[i] = std::move(items.back());
				items.pop_back();
				continue;
			}
		}
		++i;
	}

	auto& paintings = PaintingSystem::getPaintings();
	for (size_t i = 0; i < paintings.size(); ) {
		ChunkPos pCp(paintings[i].tileX >> 4, paintings[i].tileZ >> 4);
		if (pCp == cp) {
			if (auto pTag = PaintingSystem::savePaintingTag(paintings[i])) {
				NbtIo::write(pTag.get(), stream);
				anyWritten = true;
			}
			if (removeFromList) {
				paintings[i] = paintings.back();
				paintings.pop_back();
				continue;
			}
		}
		++i;
	}

	if (anyWritten) {
		lc.setSerializedEntitiesBuffer(std::move(encoded), true);
		lc.setHadSerializedEntities(true);
	} else if (lc.hadSerializedEntities()) {
		// Entities previously on disk were picked up/despawned; mark Entity field dirty so DB removes stale Tag::Entity
		lc.setSerializedEntitiesBuffer(std::string(), true);
		lc.setHadSerializedEntities(false);
	}
}

static void deserializeChunkWorldItems(LevelChunk& lc, std::vector<WorldItemEntity>& items) {
	if (!lc.hasSerializedEntities()) {
		return;
	}
	std::string encoded = lc.takeSerializedEntitiesBuffer();
	StringByteInput input(encoded);
	static Random loadItemRng;

	while (input.numBytesLeft() > 0) {
		auto tag = NbtIo::read(input);
		if (!tag) {
			break;
		}
		if (tag->getInt("id") == 83) {
			PaintingSystem::loadPaintingTag(*tag);
			continue;
		}
		if (tag->getInt("id") != 64) {
			continue;
		}
		const ListTag* posList = tag->getList("Pos");
		const CompoundTag* itemTag = tag->getCompound("Item");
		if (!posList || posList->size() < 3 || !itemTag) {
			continue;
		}
		int itemId = static_cast<int>(itemTag->getShort("id"));
		int aux = static_cast<int>(itemTag->getShort("Damage"));
		int count = static_cast<int>(static_cast<uint8_t>(itemTag->getByte("Count")));
		if (itemId <= 0 || count <= 0) {
			continue;
		}
		ItemInstance inst(itemId, count, aux);
		if (inst.isNull() || !inst.isValid()) {
			continue;
		}

		WorldItemEntity ent;
		ent.item = inst;
		ent.pos = Vec3(posList->getFloat(0), posList->getFloat(1), posList->getFloat(2));
		ent.prevPos = ent.pos;
		const ListTag* velList = tag->getList("Motion");
		if (velList && velList->size() >= 3) {
			ent.vel = Vec3(velList->getFloat(0), velList->getFloat(1), velList->getFloat(2));
		}
		ent.age = static_cast<int>(tag->getShort("Age"));
		ent.delayBeforePickup = static_cast<int>(tag->getShort("PickupDelay"));
		int lt = tag->getInt("LifeTime");
		if (lt > 0) {
			ent.lifeTime = lt;
		}
		ent.hoverStart = loadItemRng.nextFloat() * 3.1416f * 2.0f;
		items.push_back(std::move(ent));
	}
}

void spawnWorldItem(std::vector<WorldItemEntity>& items,
                    const ItemInstance& item,
                    const Vec3& pos,
                    const Vec3& vel,
                    float pickupDelay) {
	if (item.isNull() || item.getStackSize() <= 0) return;
	static Random itemRng;
	WorldItemEntity ent;
	ent.item = item;
	ent.pos = pos;
	ent.prevPos = pos;
	ent.vel = vel * 0.05f;
	ent.delayBeforePickup = static_cast<int>(std::round(pickupDelay * 20.0f));
	ent.hoverStart = itemRng.nextFloat() * 3.1416f * 2.0f;
	items.push_back(std::move(ent));
}

// MCPE 0.8.1 ItemEntity::checkInTile
static void checkWorldItemInTile(WorldItemEntity& e, BlockSource& region, Random& rng) {
	int xFloor = Math::floor(e.pos.x);
	int yFloor = Math::floor(e.pos.y);
	int zFloor = Math::floor(e.pos.z);
	if (region.getBlock(xFloor, yFloor, zFloor).isSolid()) {
		float xDiff = e.pos.x - static_cast<float>(xFloor);
		float yDiff = e.pos.y - static_cast<float>(yFloor);
		float zDiff = e.pos.z - static_cast<float>(zFloor);
		bool xNeg = region.getBlock(xFloor - 1, yFloor, zFloor).isSolid();
		bool xPos = region.getBlock(xFloor + 1, yFloor, zFloor).isSolid();
		bool yNeg = region.getBlock(xFloor, yFloor - 1, zFloor).isSolid();
		bool yPos = region.getBlock(xFloor, yFloor + 1, zFloor).isSolid();
		bool zNeg = region.getBlock(xFloor, yFloor, zFloor - 1).isSolid();
		bool zPos = region.getBlock(xFloor, yFloor, zFloor + 1).isSolid();

		float bestDist = 9999.0f;
		int bestDir = -1;
		if (!xNeg && xDiff < bestDist) {
			bestDist = xDiff;
			bestDir = 0;
		}
		if (!xPos && (1.0f - xDiff) < bestDist) {
			bestDist = 1.0f - xDiff;
			bestDir = 1;
		}
		if (!yNeg && yDiff < bestDist) {
			bestDist = yDiff;
			bestDir = 2;
		}
		if (!yPos && (1.0f - yDiff) < bestDist) {
			bestDist = 1.0f - yDiff;
			bestDir = 3;
		}
		if (!zNeg && zDiff < bestDist) {
			bestDist = zDiff;
			bestDir = 4;
		}
		if (!zPos && (1.0f - zDiff) < bestDist) {
			bestDir = 5;
		}

		float push = rng.nextFloat() * 0.2f + 0.1f;
		if (bestDir == 0) e.vel.x = -push;
		else if (bestDir == 1) e.vel.x = push;
		else if (bestDir == 2) e.vel.y = -push;
		else if (bestDir == 3) e.vel.y = push;
		else if (bestDir == 4) e.vel.z = -push;
		else if (bestDir == 5) e.vel.z = push;
	}
}

void updateWorldItemEntities(std::vector<WorldItemEntity>& items,
                             float& tickAccum,
                             float dt,
                             BlockSource& region,
                             const AABB& playerBB,
                             GuiContext& guiCtx,
                             bool allowPickup) {
	constexpr float TICK_DT = 0.05f;
	constexpr float HALF_W = 0.125f;
	constexpr float HEIGHT = 0.25f;
	constexpr float RIDING_HEIGHT = HEIGHT * 0.5f; // MCPE 0.8.1 ItemEntity: 0.125f

	static Random itemTickRng;
	dt = std::min(std::max(dt, 0.0001f), 0.1f);
	tickAccum += dt;
	if (tickAccum > 0.25f) tickAccum = 0.25f;

	while (tickAccum >= TICK_DT) {
		tickAccum -= TICK_DT;
		for (size_t i = 0; i < items.size(); ) {
			WorldItemEntity& e = items[i];
			LevelChunk* itemLc = region.getChunk(ChunkPos(Math::floor(e.pos.x) >> 4, Math::floor(e.pos.z) >> 4));
			if (!itemLc || itemLc->getState() != ChunkState::Loaded) {
				++i;
				continue;
			}
			e.prevPos = e.pos;
			if (e.delayBeforePickup > 0) {
				--e.delayBeforePickup;
			}

			// MCPE 0.8.1 ItemEntity::tick()
			e.vel.y -= 0.04f;
			const Block& curBlock = region.getBlock(Math::floor(e.pos.x), Math::floor(e.pos.y), Math::floor(e.pos.z));
			if (curBlock.getMaterial().isType(MaterialType::Lava)) {
				e.vel.y = 0.2f;
				e.vel.x = (itemTickRng.nextFloat() - itemTickRng.nextFloat()) * 0.2f;
				e.vel.z = (itemTickRng.nextFloat() - itemTickRng.nextFloat()) * 0.2f;
			}

			checkWorldItemInTile(e, region, itemTickRng);

			AABB itemBB(e.pos.x - HALF_W, e.pos.y - RIDING_HEIGHT, e.pos.z - HALF_W,
			            e.pos.x + HALF_W, e.pos.y - RIDING_HEIGHT + HEIGHT, e.pos.z + HALF_W);
			std::vector<AABB>& aABBs = region.fetchCollisionShapes(region, itemBB.expanded(e.vel), nullptr, false, nullptr);

			float xaOrg = e.vel.x;
			float yaOrg = e.vel.y;
			float zaOrg = e.vel.z;

			Vec3 steps[3] = { Vec3(0.0f, e.vel.y, 0.0f), Vec3(e.vel.x, 0.0f, 0.0f), Vec3(0.0f, 0.0f, e.vel.z) };
			for (int axis = 0; axis < 3; ++axis) {
				for (const auto& bb : aABBs) {
					steps[axis] = bb.clipCollide(itemBB, steps[axis], false);
				}
				itemBB.move(steps[axis]);
			}

			e.pos.x = (itemBB.min.x + itemBB.max.x) * 0.5f;
			e.pos.y = itemBB.min.y + RIDING_HEIGHT;
			e.pos.z = (itemBB.min.z + itemBB.max.z) * 0.5f;

			if (steps[1].x != xaOrg) e.vel.x = 0.0f;
			if (steps[2].z != zaOrg) e.vel.z = 0.0f;
			if (steps[0].y != yaOrg) {
				e.onGround = (yaOrg < 0.0f);
				e.vel.y = 0.0f;
			} else {
				e.onGround = false;
			}

			float friction = 0.98f;
			if (e.onGround) {
				friction = 0.588f;
				const Block& below = region.getBlock(Math::floor(e.pos.x), Math::floor(itemBB.min.y) - 1, Math::floor(e.pos.z));
				if (below.getId() > 0) {
					friction = below.getFriction() * 0.98f;
				}
			}

			e.vel.x *= friction;
			e.vel.y *= 0.98f;
			e.vel.z *= friction;
			if (e.onGround) {
				e.vel.y *= -0.5f;
			}

			++e.age;
			if (e.age >= e.lifeTime || e.pos.y < -32.0f || e.item.isNull() || e.item.getStackSize() <= 0) {
				items[i] = std::move(items.back());
				items.pop_back();
				continue;
			}
			++i;
		}
	}

	if (allowPickup) {
		AABB pickupBox = playerBB.expanded(Vec3(0.85f, 0.5f, 0.85f));
		for (size_t i = 0; i < items.size(); ) {
			WorldItemEntity& e = items[i];
			if (e.delayBeforePickup <= 0) {
				AABB itemBB(e.pos.x - HALF_W, e.pos.y - RIDING_HEIGHT, e.pos.z - HALF_W,
				            e.pos.x + HALF_W, e.pos.y - RIDING_HEIGHT + HEIGHT, e.pos.z + HALF_W);
				if (itemBB.intersects(pickupBox)) {
					ItemInstance rem = guiCtx.addItemToInventoryRemainder(e.item);
					if (rem.isNull() || rem.getStackSize() <= 0) {
						items[i] = std::move(items.back());
						items.pop_back();
						continue;
					}
					e.item = rem;
				}
			}
			++i;
		}
	}
}

void renderWorldItemEntities(const std::vector<WorldItemEntity>& items,
                             BlockSource& region,
                             float sunFactor,
                             float mcpeCameraYaw,
                             float partialTicks,
                             const glm::mat4& projection,
                             const glm::mat4& view) {
	if (items.empty()) return;

	// MCPE 0.8.1 ItemRenderer::rndFloats[16]
	static float sRndFloats[16];
	static bool sRndInited = false;
	if (!sRndInited) {
		Random rnd;
		for (int i = 0; i < 16; ++i) {
			float ff = rnd.nextFloat();
			sRndFloats[i] = (ff + ff) - 1.0f;
		}
		sRndInited = true;
	}

	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(projection));
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(view));

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.1f);

	// Extract frustum planes from projection * view for world-space item culling
	const glm::mat4 vp = projection * view;
	glm::vec4 fPlanes[6];
	for (int i = 0; i < 4; ++i) {
		fPlanes[0][i] = vp[i][3] + vp[i][0];
		fPlanes[1][i] = vp[i][3] - vp[i][0];
		fPlanes[2][i] = vp[i][3] + vp[i][1];
		fPlanes[3][i] = vp[i][3] - vp[i][1];
		fPlanes[4][i] = vp[i][3] + vp[i][2];
		fPlanes[5][i] = vp[i][3] - vp[i][2];
	}

	for (const auto& e : items) {
		if (e.item.isNull()) continue;

		float rx = e.prevPos.x + (e.pos.x - e.prevPos.x) * partialTicks;
		float ry = e.prevPos.y + (e.pos.y - e.prevPos.y) * partialTicks;
		float rz = e.prevPos.z + (e.pos.z - e.prevPos.z) * partialTicks;

		bool vis = true;
		for (int p = 0; p < 6; ++p) {
			const float len = std::sqrt(fPlanes[p].x * fPlanes[p].x + fPlanes[p].y * fPlanes[p].y + fPlanes[p].z * fPlanes[p].z);
			if (fPlanes[p].x * rx + fPlanes[p].y * ry + fPlanes[p].z * rz + fPlanes[p].w < -1.0f * len) {
				vis = false;
				break;
			}
		}
		if (!vis) continue;

		// MCPE 0.8.1 Entity::getBrightness & Entity::setupLighting (clamp [0.35, 1.0])
		int lightY = Math::floor((ry - 0.125f) + 0.25f * 0.66f);
		float itemLight = WorldLighting::sampleWorldBrightness(
			region, BlockPos(Math::floor(rx), lightY, Math::floor(rz)), sunFactor);
		if (itemLight > 1.0f) itemLight = 1.0f;
		else if (itemLight <= 0.35f) itemLight = 0.35f;

		// MCPE 0.8.1 ItemRenderer::render
		float bobAge = static_cast<float>(e.age) + partialTicks;
		float hoverStart = e.hoverStart;
		int count = e.item.getStackSize();
		int renderCount = 2;
		if (count > 20) renderCount = 4;
		else if (count > 5) renderCount = 3;
		else if (count <= 1) renderCount = 1;

		glPushMatrix();
		glTranslatef(rx, ry + (std::sin((bobAge / 10.0f) + hoverStart) * 0.1f + 0.1f), rz);

		if (GuiComponent::isBlock3D(e.item)) {
			glRotatef(((bobAge / 20.0f) + hoverStart) * 57.296f, 0.0f, 1.0f, 0.0f);
			float scale = 0.25f;
			glScalef(scale, scale, scale);
			glDisable(GL_BLEND);
			float offsetScale = 0.2f / scale;
			for (int layer = 0; layer < renderCount; ++layer) {
				glPushMatrix();
				if (layer > 0) {
					glTranslatef(offsetScale * sRndFloats[layer * 3 + 0],
					             offsetScale * sRndFloats[layer * 3 + 1],
					             offsetScale * sRndFloats[layer * 3 + 2]);
				}
				GuiComponent::drawBlock3DModel(e.item, itemLight, false);
				glPopMatrix();
			}
		} else {
			std::string tex;
			uint32_t tint = 0xFFFFFFu;
			GuiComponent::getItemSpritePathAndTint(e.item, tex, tint);
			glDisable(GL_BLEND);
			glScalef(0.5f, 0.5f, 0.5f);
			glRotatef(180.0f - mcpeCameraYaw, 0.0f, 1.0f, 0.0f);
			GuiTextureInfo info = !tex.empty() ? GuiComponent::loadTexture(tex) : GuiTextureInfo{};
			if (info.id != 0) {
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, info.id);
				float vMax = (info.width > 0 && info.height > info.width)
					? (static_cast<float>(info.width) / static_cast<float>(info.height))
					: 1.0f;
				float tr = static_cast<float>((tint >> 16) & 0xFF) / 255.0f;
				float tg = static_cast<float>((tint >> 8) & 0xFF) / 255.0f;
				float tb = static_cast<float>(tint & 0xFF) / 255.0f;
				glColor4f(itemLight * tr, itemLight * tg, itemLight * tb, 1.0f);
				glBegin(GL_QUADS);
				for (int layer = 0; layer < renderCount; ++layer) {
					float ox = (layer > 0) ? (sRndFloats[layer * 3 + 0] * 0.3f) : 0.0f;
					float oy = (layer > 0) ? (sRndFloats[layer * 3 + 1] * 0.3f) : 0.0f;
					float oz = (layer > 0) ? (sRndFloats[layer * 3 + 2] * 0.3f) : 0.0f;
					glTexCoord2f(0.0f, vMax); glVertex3f(-0.5f + ox, -0.25f + oy, oz);
					glTexCoord2f(1.0f, vMax); glVertex3f( 0.5f + ox, -0.25f + oy, oz);
					glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.5f + ox,  0.75f + oy, oz);
					glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f + ox,  0.75f + oy, oz);
				}
				glEnd();
			}
		}

		glPopMatrix();
	}

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

struct CelestialInfo {
	float timeOfDay = 0.0f;   // 0=noon, 0.25=sunset, 0.5=midnight, 0.75=sunrise
	float sunHeight = 1.0f;   // cos(timeOfDay * 2pi)
	float sunFactor = 1.0f;   // 0.16..1.0 used for lightmap & entity shading
	float dayBright = 1.0f;   // 0..1 sky brightness
	float sunriseAlpha = 0.0f;
	glm::vec3 sunriseRgb = glm::vec3(0.95f, 0.42f, 0.15f);
	glm::vec3 skyColor = glm::vec3(0.48f, 0.68f, 1.0f);
	glm::vec3 fogColor = glm::vec3(0.67f, 0.82f, 1.0f);
};

CelestialInfo computeCelestialInfo(int64_t worldTime, float subTick) {
	CelestialInfo info;
	int dayStep = static_cast<int>(worldTime % 24000);
	float td = (static_cast<float>(dayStep) + subTick) / 24000.0f - 0.25f;
	if (td < 0.0f) td += 1.0f;
	if (td > 1.0f) td -= 1.0f;
	float tdo = td;
	td = 1.0f - (std::cos(td * 3.14159265f) + 1.0f) * 0.5f;
	td = tdo + (td - tdo) / 3.0f;

	info.timeOfDay = td;
	info.sunHeight = std::cos(td * 6.2831853f);
	info.dayBright = std::max(0.0f, std::min(1.0f, info.sunHeight * 2.0f + 0.5f));
	float skyScale = std::max(0.0f, std::min(1.0f, info.sunHeight * 2.0f + 0.2f));
	info.sunFactor = 0.16f + 0.84f * skyScale;

	glm::vec3 daySky(0.47f, 0.67f, 1.00f);
	glm::vec3 nightSky(0.015f, 0.020f, 0.055f);
	info.skyColor = glm::mix(nightSky, daySky, info.dayBright);

	glm::vec3 dayFog(0.67f, 0.82f, 1.00f);
	glm::vec3 nightFog(0.025f, 0.032f, 0.080f);
	info.fogColor = glm::mix(nightFog, dayFog, info.dayBright);

	const float span = 0.4f;
	if (info.sunHeight >= -span && info.sunHeight <= span) {
		float aa = (info.sunHeight / span) * 0.5f + 0.5f;
		float mixVal = 1.0f - (1.0f - std::sin(aa * 3.14159265f)) * 0.99f;
		mixVal = mixVal * mixVal;
		info.sunriseAlpha = mixVal;
		info.sunriseRgb = glm::vec3(aa * 0.3f + 0.7f, aa * aa * 0.7f + 0.2f, aa * aa * 0.05f + 0.15f);
		info.fogColor = glm::mix(info.fogColor, info.sunriseRgb, mixVal * 0.55f);
		info.skyColor = glm::mix(info.skyColor, info.sunriseRgb, mixVal * 0.25f);
	}
	return info;
}

void updateLightmapTexture(GLuint tex, float sunFactor) {
	unsigned char pixels[16 * 16 * 4];
	for (int sky = 0; sky < 16; ++sky) {
		float skyEff = (sky == 0 ? 0.0f : std::pow(0.82f, static_cast<float>(15 - sky))) * sunFactor;
		float skyR = skyEff * (0.72f + 0.28f * sunFactor);
		float skyG = skyEff * (0.78f + 0.22f * sunFactor);
		float skyB = skyEff;
		for (int blk = 0; blk < 16; ++blk) {
			float blkEff = (blk == 0 ? 0.0f : std::pow(0.80f, static_cast<float>(15 - blk)));
			float blkR = blkEff * 1.00f;
			float blkG = blkEff * 0.88f;
			float blkB = blkEff * 0.70f;
			const float amb = 0.06f;
			float r = std::min(1.0f, std::max({ amb, skyR, blkR }) + std::min(skyR, blkR) * 0.15f);
			float g = std::min(1.0f, std::max({ amb, skyG, blkG }) + std::min(skyG, blkG) * 0.12f);
			float b = std::min(1.0f, std::max({ amb, skyB, blkB }) + std::min(skyB, blkB) * 0.10f);
			int idx = (sky * 16 + blk) * 4;
			pixels[idx + 0] = static_cast<unsigned char>(r * 255.0f);
			pixels[idx + 1] = static_cast<unsigned char>(g * 255.0f);
			pixels[idx + 2] = static_cast<unsigned char>(b * 255.0f);
			pixels[idx + 3] = 255;
		}
	}
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint createLightmapTexture() {
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	unsigned char blank[16 * 16 * 4] = {};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, blank);
	glBindTexture(GL_TEXTURE_2D, 0);
	updateLightmapTexture(tex, 1.0f);
	return tex;
}

void renderSkyAndCelestials(const glm::mat4& projection,
                            const glm::mat4& camRelView,
                            const CelestialInfo& sky,
                            int64_t worldTime) {
	glUseProgram(0);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glDisable(GL_ALPHA_TEST);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(projection));
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(camRelView));

	// Precomputed unit-circle trig tables for sky dome (16) and horizon band (24)
	static float sDomeX[17], sDomeZ[17];
	static float sBandX[25], sBandZ[25];
	static bool sSkyTrigInited = false;
	if (!sSkyTrigInited) {
		for (int i = 0; i <= 16; ++i) {
			float ang = static_cast<float>(i) * (6.2831853f / 16.0f);
			sDomeX[i] = std::cos(ang) * 200.0f;
			sDomeZ[i] = std::sin(ang) * 200.0f;
		}
		for (int i = 0; i <= 24; ++i) {
			float ang = static_cast<float>(i) * (6.2831853f / 24.0f);
			sBandX[i] = std::cos(ang) * 180.0f;
			sBandZ[i] = std::sin(ang) * 180.0f;
		}
		sSkyTrigInited = true;
	}

	// Sky dome disc above horizon
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glBegin(GL_TRIANGLE_FAN);
	glColor3f(sky.skyColor.r, sky.skyColor.g, sky.skyColor.b);
	glVertex3f(0.0f, 16.0f, 0.0f);
	for (int i = 0; i <= 16; ++i) {
		glVertex3f(sDomeX[i], 16.0f, sDomeZ[i]);
	}
	glEnd();

	// Horizon mist gradient band (fog color at horizon -> sky color up high)
	glBegin(GL_QUAD_STRIP);
	for (int i = 0; i <= 24; ++i) {
		glColor3f(sky.fogColor.r, sky.fogColor.g, sky.fogColor.b);
		glVertex3f(sBandX[i], -12.0f, sBandZ[i]);
		glColor3f(sky.skyColor.r, sky.skyColor.g, sky.skyColor.b);
		glVertex3f(sBandX[i], 28.0f, sBandZ[i]);
	}
	glEnd();

	// Sunrise / Sunset directional glow
	if (sky.sunriseAlpha > 0.01f) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glPushMatrix();
		// Sunset (td ~ 0.25) is -X, Sunrise (td ~ 0.75) is +X
		bool isSunrise = (sky.timeOfDay > 0.5f);
		glRotatef(isSunrise ? -90.0f : 90.0f, 0.0f, 0.0f, 1.0f);
		glBegin(GL_TRIANGLE_FAN);
		glColor4f(sky.sunriseRgb.r, sky.sunriseRgb.g, sky.sunriseRgb.b, sky.sunriseAlpha * 0.85f);
		glVertex3f(0.0f, 100.0f, 0.0f);
		for (int i = 0; i <= 16; ++i) {
			float a = static_cast<float>(i) * (6.2831853f / 16.0f);
			glColor4f(sky.sunriseRgb.r, sky.sunriseRgb.g, sky.sunriseRgb.b, 0.0f);
			glVertex3f(std::sin(a) * 120.0f, std::cos(a) * 120.0f, 0.0f);
		}
		glEnd();
		glPopMatrix();
	}

	// Celestial rotation: rotates +Y (Sun) from Noon -> West (-X) -> Midnight (-Y) -> East (+X)
	glPushMatrix();
	glRotatef(sky.timeOfDay * 360.0f, 0.0f, 0.0f, 1.0f);

	// Twinkling stars at night
	if (sky.dayBright < 0.45f) {
		float starAlpha = std::min(1.0f, (0.45f - sky.dayBright) / 0.40f) * 0.85f;
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		static std::vector<Vec3> starPts;
		if (starPts.empty()) {
			Random starRng(10842);
			for (int i = 0; i < 320; ++i) {
				float sx = starRng.nextFloat() * 2.0f - 1.0f;
				float sy = starRng.nextFloat() * 2.0f - 1.0f;
				float sz = starRng.nextFloat() * 2.0f - 1.0f;
				float len2 = sx * sx + sy * sy + sz * sz;
				if (len2 > 0.01f && len2 < 1.0f) {
					float inv = 100.0f / std::sqrt(len2);
					starPts.emplace_back(sx * inv, sy * inv, sz * inv);
				}
			}
		}
		glPointSize(2.0f);
		glColor4f(0.95f, 0.97f, 1.0f, starAlpha);
		glBegin(GL_POINTS);
		for (const auto& sp : starPts) {
			glVertex3f(sp.x, sp.y, sp.z);
		}
		glEnd();
	}

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	// Sun (+Y)
	if (GuiComponent::bindTexture("textures/environment/sun.png")) {
		const float s = 26.0f;
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f); glVertex3f(-s, 100.0f, -s);
		glTexCoord2f(1.0f, 0.0f); glVertex3f( s, 100.0f, -s);
		glTexCoord2f(1.0f, 1.0f); glVertex3f( s, 100.0f,  s);
		glTexCoord2f(0.0f, 1.0f); glVertex3f(-s, 100.0f,  s);
		glEnd();
	}

	// Moon (-Y) with 8 phases from 4x2 atlas
	if (GuiComponent::bindTexture("textures/environment/moon_phases.png")) {
		int phase = static_cast<int>((worldTime / 24000) % 8);
		int col = phase % 4;
		int row = phase / 4;
		float u0 = col * 0.25f;
		float u1 = (col + 1) * 0.25f;
		float v0 = row * 0.5f;
		float v1 = (row + 1) * 0.5f;
		const float m = 18.0f;
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBegin(GL_QUADS);
		glTexCoord2f(u0, v0); glVertex3f(-m, -100.0f,  m);
		glTexCoord2f(u1, v0); glVertex3f( m, -100.0f,  m);
		glTexCoord2f(u1, v1); glVertex3f( m, -100.0f, -m);
		glTexCoord2f(u0, v1); glVertex3f(-m, -100.0f, -m);
		glEnd();
	}

	glPopMatrix(); // end celestial rotation

	glDisable(GL_BLEND);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}

void renderClouds(const glm::mat4& projection,
                  const glm::mat4& camRelView,
                  const glm::vec3& renderCamPos,
                  const CelestialInfo& sky,
                  int64_t worldTime,
                  float fracTick,
                  bool fancy) {
	if (!GuiComponent::bindTexture("textures/environment/clouds.png")) {
		return;
	}

	glUseProgram(0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(projection));
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(camRelView));

	const float cloudHeight = 128.0f;
	const float v7 = (cloudHeight - renderCamPos.y) + 0.33f;
	const float cellSize = 16.0f;
	const float invCellSize = 0.0625f;
	const float cloudThick = 7.0f;

	static bool sCloudMaskLoaded = false;
	static int sCloudW = 0;
	static int sCloudH = 0;
	static std::vector<uint8_t> sCloudRGBA;
	static std::vector<uint8_t> sCloudMask;
	if (!sCloudMaskLoaded) {
		GLint tw = 0, th = 0;
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tw);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &th);
		if (tw > 0 && th > 0) {
			sCloudW = tw;
			sCloudH = th;
			sCloudRGBA.resize(static_cast<size_t>(tw) * th * 4);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, sCloudRGBA.data());
			sCloudMask.resize(static_cast<size_t>(tw) * th);
			for (size_t i = 0; i < sCloudMask.size(); ++i) {
				sCloudMask[i] = (sCloudRGBA[i * 4 + 3] > 9) ? 1 : 0;
			}
			sCloudMaskLoaded = true;
		}
	}

	const float scaleW = (sCloudW > 0 ? static_cast<float>(sCloudW) : 256.0f) * cellSize;
	const float scaleH = (sCloudH > 0 ? static_cast<float>(sCloudH) : 256.0f) * cellSize;

	// Level::getCloudColor (MCPE 0.8.1 Level.cpp:793-809)
	const float v6 = std::max(0.0f, std::min(1.0f, 2.0f * std::cos(sky.timeOfDay * 6.2831853f) + 0.5f));
	const float v7c = v6 * 0.9f + 0.1f;
	const float cloudR = v7c;
	const float cloudG = v7c;
	const float cloudB = v6 * 0.85f + 0.15f;
	const float cloudA = 0.7f;

	const double scrollX = static_cast<double>(renderCamPos.x) + (static_cast<double>(worldTime) + static_cast<double>(fracTick)) * 0.03;
	const double scrollZ = static_cast<double>(renderCamPos.z);

	const float v17 = std::sqrt(v7 * v7 + 262140.0f);
	const GLfloat fogColor[4] = { sky.fogColor.r, sky.fogColor.g, sky.fogColor.b, 1.0f };
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogfv(GL_FOG_COLOR, fogColor);
	glFogf(GL_FOG_START, 0.0f);
	glFogf(GL_FOG_END, v17);

	if (!fancy || !sCloudMaskLoaded) {
		glEnable(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glDisable(GL_CULL_FACE);
		glDepthMask(GL_FALSE);

		const float extent = 512.0f;
		const int gridN = 8;
		const float step = (extent * 2.0f) / static_cast<float>(gridN);
		glBegin(GL_QUADS);
		for (int ix = 0; ix < gridN; ++ix) {
			for (int iz = 0; iz < gridN; ++iz) {
				float x0 = -extent + static_cast<float>(ix) * step;
				float x1 = x0 + step;
				float z0 = -extent + static_cast<float>(iz) * step;
				float z1 = z0 + step;
				float u0 = static_cast<float>((scrollX + x0) / scaleW);
				float u1 = static_cast<float>((scrollX + x1) / scaleW);
				float v0 = static_cast<float>((scrollZ + z0) / scaleH);
				float v1 = static_cast<float>((scrollZ + z1) / scaleH);
				auto edgeAlpha = [&](float px, float pz) -> float {
					float d = std::sqrt(px * px + pz * pz);
					float f = std::max(0.0f, std::min(1.0f, (extent - d) / (extent * 0.5f)));
					return cloudA * f;
				};
				glColor4f(cloudR, cloudG, cloudB, edgeAlpha(x0, z0)); glTexCoord2f(u0, v0); glVertex3f(x0, v7, z0);
				glColor4f(cloudR, cloudG, cloudB, edgeAlpha(x1, z0)); glTexCoord2f(u1, v0); glVertex3f(x1, v7, z0);
				glColor4f(cloudR, cloudG, cloudB, edgeAlpha(x1, z1)); glTexCoord2f(u1, v1); glVertex3f(x1, v7, z1);
				glColor4f(cloudR, cloudG, cloudB, edgeAlpha(x0, z1)); glTexCoord2f(u0, v1); glVertex3f(x0, v7, z1);
			}
		}
		glEnd();
	} else {
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glDepthMask(GL_TRUE);

		const double scaledX = scrollX * static_cast<double>(invCellSize);
		const double scaledZ = scrollZ * static_cast<double>(invCellSize);
		const int baseCellX = static_cast<int>(std::floor(scaledX));
		const int baseCellZ = static_cast<int>(std::floor(scaledZ));
		const float fracX = static_cast<float>((scaledX - static_cast<double>(baseCellX)) * static_cast<double>(cellSize));
		const float fracZ = static_cast<float>((scaledZ - static_cast<double>(baseCellZ)) * static_cast<double>(cellSize));

		struct CloudVertex {
			float x, y, z;
			float r, g, b, a;
		};
		static bool sCloudMeshValid = false;
		static int sCachedBaseCellX = 0;
		static int sCachedBaseCellZ = 0;
		static float sCachedCloudR = 0.0f;
		static float sCachedCloudG = 0.0f;
		static float sCachedCloudB = 0.0f;
		static std::vector<CloudVertex> sCloudMesh;
		static GLuint sCloudVbo = 0;
		static GLsizei sCloudVboVertexCount = 0;

		const float colorDiff = std::fabs(cloudR - sCachedCloudR) + std::fabs(cloudG - sCachedCloudG) + std::fabs(cloudB - sCachedCloudB);
		if (!sCloudMeshValid || baseCellX != sCachedBaseCellX || baseCellZ != sCachedBaseCellZ || colorDiff > 0.06f) {
			sCloudMesh.clear();
			auto wrapX = [&](int cx) -> int { return ((cx % sCloudW) + sCloudW) % sCloudW; };
			auto wrapZ = [&](int cz) -> int { return ((cz % sCloudH) + sCloudH) % sCloudH; };
			auto isCloud = [&](int cx, int cz) -> bool {
				return sCloudMask[static_cast<size_t>(wrapZ(cz)) * sCloudW + wrapX(cx)] != 0;
			};

			auto pushQuad = [&](float x0, float y0, float z0,
			                    float x1, float y1, float z1,
			                    float x2, float y2, float z2,
			                    float x3, float y3, float z3,
			                    float r, float g, float b, float a) {
				auto fadeA = [&](float px, float pz) -> float {
					float d = std::sqrt(px * px + pz * pz);
					float f = std::max(0.0f, std::min(1.0f, (512.0f - d) / 256.0f));
					return a * f;
				};
				sCloudMesh.push_back({x0, y0, z0, r, g, b, fadeA(x0, z0)});
				sCloudMesh.push_back({x1, y1, z1, r, g, b, fadeA(x1, z1)});
				sCloudMesh.push_back({x2, y2, z2, r, g, b, fadeA(x2, z2)});
				sCloudMesh.push_back({x3, y3, z3, r, g, b, fadeA(x3, z3)});
			};

			const float y0 = 0.0f;
			const float y1 = cloudThick;

			for (int gx = -32; gx < 32; ++gx) {
				for (int gz = -32; gz < 32; ++gz) {
					const int cx = baseCellX + gx;
					const int cz = baseCellZ + gz;
					if (!isCloud(cx, cz)) {
						continue;
					}

					const size_t tidx = (static_cast<size_t>(wrapZ(cz)) * sCloudW + wrapX(cx)) * 4;
					const float tr = static_cast<float>(sCloudRGBA[tidx + 0]) / 255.0f * cloudR;
					const float tg = static_cast<float>(sCloudRGBA[tidx + 1]) / 255.0f * cloudG;
					const float tb = static_cast<float>(sCloudRGBA[tidx + 2]) / 255.0f * cloudB;
					const float ta = static_cast<float>(sCloudRGBA[tidx + 3]) / 255.0f * cloudA;

					const float x0 = static_cast<float>(gx) * cellSize;
					const float x1 = x0 + cellSize;
					const float z0 = static_cast<float>(gz) * cellSize;
					const float z1 = z0 + cellSize;

					// TextureTesselator::_addLighting (f10 = (0, 0.70711, 0.70711), ambient = 0.6):
					// +Y: 1.0f, -Y: 0.746445f, +/-X: 1.0f, +Z: 1.0f, -Z: 0.746445f
					// Top face (+Y)
					pushQuad(x0, y1, z0, x0, y1, z1, x1, y1, z1, x1, y1, z0,
					         tr * 1.0f, tg * 1.0f, tb * 1.0f, ta);

					// Bottom face (-Y)
					pushQuad(x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1,
					         tr * 0.746445f, tg * 0.746445f, tb * 0.746445f, ta);

					// West face (-X)
					if (!isCloud(cx - 1, cz)) {
						pushQuad(x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0,
						         tr * 1.0f, tg * 1.0f, tb * 1.0f, ta);
					}
					// East face (+X)
					if (!isCloud(cx + 1, cz)) {
						pushQuad(x1, y0, z0, x1, y1, z0, x1, y1, z1, x1, y0, z1,
						         tr * 1.0f, tg * 1.0f, tb * 1.0f, ta);
					}
					// North face (-Z)
					if (!isCloud(cx, cz - 1)) {
						pushQuad(x0, y0, z0, x0, y1, z0, x1, y1, z0, x1, y0, z0,
						         tr * 0.746445f, tg * 0.746445f, tb * 0.746445f, ta);
					}
					// South face (+Z)
					if (!isCloud(cx, cz + 1)) {
						pushQuad(x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1,
						         tr * 1.0f, tg * 1.0f, tb * 1.0f, ta);
					}
				}
			}

			sCachedBaseCellX = baseCellX;
			sCachedBaseCellZ = baseCellZ;
			sCachedCloudR = cloudR;
			sCachedCloudG = cloudG;
			sCachedCloudB = cloudB;
			sCloudMeshValid = true;
			if (sCloudVbo == 0) {
				glGenBuffers(1, &sCloudVbo);
			}
			sCloudVboVertexCount = static_cast<GLsizei>(sCloudMesh.size());
			glBindBuffer(GL_ARRAY_BUFFER, sCloudVbo);
			glBufferData(GL_ARRAY_BUFFER,
			             static_cast<GLsizeiptr>(sCloudMesh.size() * sizeof(CloudVertex)),
			             sCloudMesh.empty() ? nullptr : sCloudMesh.data(),
			             GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		glPushMatrix();
		glTranslatef(-fracX, v7, -fracZ);
		if (sCloudVbo != 0 && sCloudVboVertexCount > 0) {
			glBindBuffer(GL_ARRAY_BUFFER, sCloudVbo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glVertexPointer(3, GL_FLOAT, sizeof(CloudVertex), reinterpret_cast<const void*>(0));
			glColorPointer(4, GL_FLOAT, sizeof(CloudVertex), reinterpret_cast<const void*>(offsetof(CloudVertex, r)));
			glDrawArrays(GL_QUADS, 0, sCloudVboVertexCount);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_VERTEX_ARRAY);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		glPopMatrix();
	}

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glDisable(GL_FOG);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}

GLuint createWhiteTexture() {
	unsigned char px[4] = { 255, 255, 255, 255 };
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

void drawPlayerBox3D(float x0, float y0, float z0,
                     float w, float h, float d,
                     int u, int v, int iw, int ih, int id,
                     float texW, float texH, float shade = 1.0f) {
	float x1 = x0 + w;
	float y1 = y0 + h;
	float z1 = z0 + d;

	auto quad = [&](float ax, float ay, float az,
	                float bx, float by, float bz,
	                float cx, float cy, float cz,
	                float dx, float dy, float dz,
	                int su, int sv, int sw, int sh, float faceShade) {
		float u0 = static_cast<float>(su) / texW;
		float v0 = static_cast<float>(sv) / texH;
		float u1 = static_cast<float>(su + sw) / texW;
		float v1 = static_cast<float>(sv + sh) / texH;
		float c = shade * faceShade;
		glColor4f(c, c, c, 1.0f);
		glTexCoord2f(u0, v1); glVertex3f(ax, ay, az);
		glTexCoord2f(u1, v1); glVertex3f(bx, by, bz);
		glTexCoord2f(u1, v0); glVertex3f(cx, cy, cz);
		glTexCoord2f(u0, v0); glVertex3f(dx, dy, dz);
	};

	glBegin(GL_QUADS);
	// Front (+Z)
	quad(x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1, u + id, v + id, iw, ih, 0.95f);
	// Back (-Z)
	quad(x1, y0, z0, x0, y0, z0, x0, y1, z0, x1, y1, z0, u + id + iw + id, v + id, iw, ih, 0.85f);
	// Right (-X)
	quad(x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0, u, v + id, id, ih, 0.80f);
	// Left (+X)
	quad(x1, y0, z1, x1, y0, z0, x1, y1, z0, x1, y1, z1, u + id + iw, v + id, id, ih, 0.80f);
	// Top (+Y)
	quad(x0, y1, z1, x1, y1, z1, x1, y1, z0, x0, y1, z0, u + id, v, iw, id, 1.0f);
	// Bottom (-Y)
	quad(x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, u + id + iw, v, iw, id, 0.65f);
	glEnd();
}

void drawTexturedCube3D(const BlockRenderInfo& blockInfo, float shade = 1.0f) {
	float y0 = -0.5f;
	float y1 = blockInfo.isSlab ? 0.0f : 0.5f;
	float vTop = blockInfo.isSlab ? 0.5f : 0.0f;

	auto drawQuad = [&](const std::string& tex, float faceShade,
	                    float ax, float ay, float az,
	                    float bx, float by, float bz,
	                    float cx, float cy, float cz,
	                    float dx, float dy, float dz,
	                    float v0, float v1) {
		if (tex.empty() || !GuiComponent::bindTexture(tex)) return;
		float c = shade * faceShade;
		glColor4f(c, c, c, 1.0f);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, v1); glVertex3f(ax, ay, az);
		glTexCoord2f(1.0f, v1); glVertex3f(bx, by, bz);
		glTexCoord2f(1.0f, v0); glVertex3f(cx, cy, cz);
		glTexCoord2f(0.0f, v0); glVertex3f(dx, dy, dz);
		glEnd();
	};

	if (blockInfo.isTransparent) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	drawQuad(blockInfo.topTex, 1.0f, -0.5f, y1, 0.5f, 0.5f, y1, 0.5f, 0.5f, y1, -0.5f, -0.5f, y1, -0.5f, 0.0f, 1.0f);
	drawQuad(blockInfo.topTex, 0.55f, -0.5f, y0, -0.5f, 0.5f, y0, -0.5f, 0.5f, y0, 0.5f, -0.5f, y0, 0.5f, 0.0f, 1.0f);
	drawQuad(blockInfo.frontTex, 0.85f, -0.5f, y0, 0.5f, 0.5f, y0, 0.5f, 0.5f, y1, 0.5f, -0.5f, y1, 0.5f, vTop, 1.0f);
	drawQuad(blockInfo.frontTex, 0.85f, 0.5f, y0, -0.5f, -0.5f, y0, -0.5f, -0.5f, y1, -0.5f, 0.5f, y1, -0.5f, vTop, 1.0f);
	drawQuad(blockInfo.sideTex, 0.70f, 0.5f, y0, 0.5f, 0.5f, y0, -0.5f, 0.5f, y1, -0.5f, 0.5f, y1, 0.5f, vTop, 1.0f);
	drawQuad(blockInfo.sideTex, 0.70f, -0.5f, y0, -0.5f, -0.5f, y0, 0.5f, -0.5f, y1, 0.5f, -0.5f, y1, -0.5f, vTop, 1.0f);
	if (blockInfo.isTransparent) {
		glDisable(GL_BLEND);
	}
}

struct ExtrudedEdgeQuad {
	float x0, y0, x1, y1, z0, z1;
	float u, v, shade;
	int axis; // 0 = X-facing (left/right), 1 = Y-facing (top/bottom)
};

const std::vector<ExtrudedEdgeQuad>& getExtrudedEdges(const std::string& texPath) {
	static std::unordered_map<std::string, std::vector<ExtrudedEdgeQuad>> cache;
	auto it = cache.find(texPath);
	if (it != cache.end()) return it->second;

	std::vector<ExtrudedEdgeQuad> edges;
	std::string fullPath = GuiComponent::resolveAssetPath(texPath);
	int w = 0, fullH = 0, ch = 0;
	stbi_set_flip_vertically_on_load(0);
	unsigned char* data = stbi_load(fullPath.c_str(), &w, &fullH, &ch, 4);
	int h = (w > 0 && fullH > w) ? w : fullH;
	if (data && w > 0 && h > 0 && w <= 64 && h <= 64) {
		auto isOpaque = [&](int px, int py) -> bool {
			if (px < 0 || py < 0 || px >= w || py >= h) return false;
			return data[(py * w + px) * 4 + 3] >= 128;
		};
		const float halfZ = 0.03125f;
		const float fw = static_cast<float>(w);
		const float fh = static_cast<float>(h);
		const float fFullH = static_cast<float>(fullH);
		for (int py = 0; py < h; ++py) {
			float y1 = 0.5f - static_cast<float>(py) / fh;
			float y0 = 0.5f - static_cast<float>(py + 1) / fh;
			float v = (static_cast<float>(py) + 0.5f) / fFullH;
			for (int px = 0; px < w; ++px) {
				if (!isOpaque(px, py)) continue;
				float x0 = -0.5f + static_cast<float>(px) / fw;
				float x1 = -0.5f + static_cast<float>(px + 1) / fw;
				float u = (static_cast<float>(px) + 0.5f) / fw;

				if (!isOpaque(px - 1, py)) {
					edges.push_back({ x0, y0, x0, y1, -halfZ, halfZ, u, v, 0.72f, 0 });
				}
				if (!isOpaque(px + 1, py)) {
					edges.push_back({ x1, y0, x1, y1, -halfZ, halfZ, u, v, 0.72f, 0 });
				}
				if (!isOpaque(px, py - 1)) {
					edges.push_back({ x0, y1, x1, y1, -halfZ, halfZ, u, v, 0.92f, 1 });
				}
				if (!isOpaque(px, py + 1)) {
					edges.push_back({ x0, y0, x1, y0, -halfZ, halfZ, u, v, 0.62f, 1 });
				}
			}
		}
	}
	if (data) stbi_image_free(data);
	auto res = cache.emplace(texPath, std::move(edges));
	return res.first->second;
}

void drawTexturedSprite3D(const std::string& tex, float shade = 1.0f, uint32_t rgbColor = 0xFFFFFFu) {
	if (tex.empty()) return;
	std::string useTex = tex;
	int frameIdx = 0;
	int frameCount = 1;
	if (tex == "textures/items/compass_item.png" || tex == "textures/items/compass_atlas.png") {
		useTex = "textures/items/compass_atlas.png";
		frameIdx = GuiComponent::getCompassFrame();
		frameCount = 32;
	} else if (tex == "textures/items/clock_item.png" || tex == "textures/items/watch_atlas.png") {
		useTex = "textures/items/watch_atlas.png";
		frameIdx = GuiComponent::getClockFrame();
		frameCount = 64;
	}
	GuiTextureInfo info = GuiComponent::loadTexture(useTex);
	if (info.id == 0) return;
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, info.id);

	float frameFrac = (info.width > 0 && info.height > info.width)
		? (static_cast<float>(info.width) / static_cast<float>(info.height))
		: 1.0f;
	float vMin = (frameCount > 1) ? (static_cast<float>(frameIdx) * frameFrac) : 0.0f;
	float vMax = (frameCount > 1) ? (static_cast<float>(frameIdx + 1) * frameFrac) : frameFrac;
	float tr = static_cast<float>((rgbColor >> 16) & 0xFF) / 255.0f;
	float tg = static_cast<float>((rgbColor >> 8) & 0xFF) / 255.0f;
	float tb = static_cast<float>(rgbColor & 0xFF) / 255.0f;
	const float halfZ = 0.03125f;

	glBegin(GL_QUADS);
	// Front face (+Z)
	glColor4f(shade * tr, shade * tg, shade * tb, 1.0f);
	glTexCoord2f(0.0f, vMax); glVertex3f(-0.5f, -0.5f, halfZ);
	glTexCoord2f(1.0f, vMax); glVertex3f(0.5f, -0.5f, halfZ);
	glTexCoord2f(1.0f, vMin); glVertex3f(0.5f, 0.5f, halfZ);
	glTexCoord2f(0.0f, vMin); glVertex3f(-0.5f, 0.5f, halfZ);

	// Back face (-Z)
	glColor4f(shade * 0.85f * tr, shade * 0.85f * tg, shade * 0.85f * tb, 1.0f);
	glTexCoord2f(0.0f, vMax); glVertex3f(-0.5f, -0.5f, -halfZ);
	glTexCoord2f(0.0f, vMin); glVertex3f(-0.5f, 0.5f, -halfZ);
	glTexCoord2f(1.0f, vMin); glVertex3f(0.5f, 0.5f, -halfZ);
	glTexCoord2f(1.0f, vMax); glVertex3f(0.5f, -0.5f, -halfZ);

	// Extruded pixel edges
	const auto& edges = getExtrudedEdges(tex);
	for (const auto& q : edges) {
		float c = shade * q.shade;
		glColor4f(c * tr, c * tg, c * tb, 1.0f);
		glTexCoord2f(q.u, q.v);
		if (q.axis == 0) {
			glVertex3f(q.x0, q.y0, q.z0);
			glVertex3f(q.x0, q.y0, q.z1);
			glVertex3f(q.x0, q.y1, q.z1);
			glVertex3f(q.x0, q.y1, q.z0);
		} else {
			glVertex3f(q.x0, q.y0, q.z0);
			glVertex3f(q.x1, q.y0, q.z0);
			glVertex3f(q.x1, q.y0, q.z1);
			glVertex3f(q.x0, q.y0, q.z1);
		}
	}
	glEnd();
}

std::string getArmorTexturePath(const ItemInstance& item, bool layer2) {
	if (item.isNull()) return "";
	int id = item.getId();
	auto eq = [&](const Item* it) { return it && id == it->getId(); };
	if (eq(Item::mHelmet_cloth) || eq(Item::mChestplate_cloth) || eq(Item::mLeggings_cloth) || eq(Item::mBoots_cloth)) {
		return layer2 ? "textures/models/armor/cloth_2.png" : "textures/models/armor/cloth_1.png";
	}
	if (eq(Item::mHelmet_chain) || eq(Item::mChestplate_chain) || eq(Item::mLeggings_chain) || eq(Item::mBoots_chain)) {
		return layer2 ? "textures/models/armor/chain_2.png" : "textures/models/armor/chain_1.png";
	}
	if (eq(Item::mHelmet_iron) || eq(Item::mChestplate_iron) || eq(Item::mLeggings_iron) || eq(Item::mBoots_iron)) {
		return layer2 ? "textures/models/armor/iron_2.png" : "textures/models/armor/iron_1.png";
	}
	if (eq(Item::mHelmet_gold) || eq(Item::mChestplate_gold) || eq(Item::mLeggings_gold) || eq(Item::mBoots_gold)) {
		return layer2 ? "textures/models/armor/gold_2.png" : "textures/models/armor/gold_1.png";
	}
	if (eq(Item::mHelmet_diamond) || eq(Item::mChestplate_diamond) || eq(Item::mLeggings_diamond) || eq(Item::mBoots_diamond)) {
		return layer2 ? "textures/models/armor/diamond_2.png" : "textures/models/armor/diamond_1.png";
	}
	return "";
}

static void bobHurt(glm::mat4& mat, const Camera& c) {
	if (gGuiContext.health <= 0) {
		float f = 40.0f - (8000.0f / (static_cast<float>(c.mDeathTime) + c.mAlpha + 200.0f));
		mat = glm::rotate(mat, glm::radians(f), glm::vec3(0.0f, 0.0f, 1.0f));
	}
	if (c.mHurtTime > 0) {
		float p = (static_cast<float>(c.mHurtTime) - c.mAlpha) / static_cast<float>(c.mHurtDuration);
		float v5 = std::sin((p * p * p * p) * 3.14159265f);
		mat = glm::rotate(mat, glm::radians(-c.mHurtDir), glm::vec3(0.0f, 1.0f, 0.0f));
		mat = glm::rotate(mat, glm::radians(-(v5 * 14.0f)), glm::vec3(0.0f, 0.0f, 1.0f));
		mat = glm::rotate(mat, glm::radians(c.mHurtDir), glm::vec3(0.0f, 1.0f, 0.0f));
	}
}

static void bobView(glm::mat4& mat, const Camera& c) {
	float v5 = c.mSmoothBob;
	float v6 = -((c.mCurrWalkDist + (c.mCurrWalkDist - c.mPrevWalkDist) * c.mAlpha) * 3.14159265f);
	float v7 = c.mTilt;
	float v8 = std::sin(v6);
	float v9 = std::cos(v6);
	mat = glm::translate(mat, glm::vec3((v8 * v5) * 0.5f, -std::fabs(v9 * v5), 0.0f));
	mat = glm::rotate(mat, glm::radians((std::sin(v6) * v5) * 3.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	mat = glm::rotate(mat, glm::radians(std::fabs(std::cos(v6 - 0.2f) * v5) * 5.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	mat = glm::rotate(mat, glm::radians(v7), glm::vec3(1.0f, 0.0f, 0.0f));
}

void renderFirstPersonHand(const Camera& c, const ItemInstance& held, float aspect, bool viewBobbing, float lightShade) {
	glUseProgram(0);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClear(GL_DEPTH_BUFFER_BIT);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.1f);

	glm::mat4 handProj = glm::perspective(glm::radians(70.0f), aspect, 0.05f, 20.0f);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(handProj));
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glm::mat4 handBob(1.0f);
	bobHurt(handBob, c);
	if (viewBobbing) {
		bobView(handBob, c);
	}
	glLoadMatrixf(glm::value_ptr(handBob));

	float sw = c.mSwingProgress;
	float swSqSin = std::sin(sw * sw * 3.14159265f);
	float swSin = std::sin(sw * 3.14159265f);
	float swSqrtSin = std::sin(std::sqrt(sw) * 3.14159265f);
	float swDoubleSin = std::sin(std::sqrt(sw) * 2.0f * 3.14159265f);

	if (held.isNull() || held.getStackSize() <= 0) {
		if (GuiComponent::bindTexture("textures/entity/steve.png")) {
			glTranslatef(-swSqrtSin * 0.30f, swDoubleSin * 0.40f, -swSin * 0.40f);
			glTranslatef(0.64f, -0.60f, -0.72f);
			glRotatef(45.0f + swSqrtSin * 70.0f, 0.0f, 1.0f, 0.0f);
			glRotatef(-swSqSin * 20.0f, 0.0f, 0.0f, 1.0f);
			// Match MCPE 0.8.1 ItemInHandRenderer::render() empty arm pose
			glTranslatef(-1.0f, 3.6f, 3.5f);
			glRotatef(120.0f, 0.0f, 0.0f, 1.0f);
			glRotatef(200.0f, 1.0f, 0.0f, 0.0f);
			glRotatef(-135.0f, 0.0f, 1.0f, 0.0f);
			glTranslatef(5.6f, 0.0f, 0.0f);
			glTranslatef(-5.0f * 0.0625f, 2.0f * 0.0625f, 0.0f);
			glScalef(0.0625f, 0.0625f, 0.0625f);
			glTranslatef(-1.0f, 4.0f, 0.0f);
			glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
			drawPlayerBox3D(-2.0f, -6.0f, -2.0f, 4.0f, 12.0f, 4.0f, 40, 16, 4, 12, 4, 64.0f, 64.0f, lightShade);
		}
	} else {
		bool isPullingBow = (Item::mBow && held.getItem() == Item::mBow && gBowPulling && gBowPullTime > 0.0f);
		if (!isPullingBow) {
			glTranslatef(-swSqrtSin * 0.40f, swDoubleSin * 0.20f, -swSin * 0.20f);
		}
		glTranslatef(0.56f, -0.52f, -0.72f);
		glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
		if (!isPullingBow) {
			glRotatef(-swSqSin * 20.0f, 0.0f, 1.0f, 0.0f);
			glRotatef(-swSqrtSin * 20.0f, 0.0f, 0.0f, 1.0f);
			glRotatef(-swSqrtSin * 80.0f, 1.0f, 0.0f, 0.0f);
		}
		glScalef(0.40f, 0.40f, 0.40f);

		if (isPullingBow) {
			// Exact MCPE 0.8.1 ItemInHandRenderer::render() bow pulling transform (L226-251)
			glRotatef(-18.0f, 0.0f, 0.0f, 1.0f);
			glRotatef(-12.0f, 0.0f, 1.0f, 0.0f);
			glRotatef(-8.0f, 1.0f, 0.0f, 0.0f);
			glTranslatef(-0.9f, 0.2f, 0.0f);

			float pullTicks = gBowPullTime * 20.0f;
			float v24 = ((pullTicks / 20.0f) * 2.0f + (pullTicks / 20.0f) * (pullTicks / 20.0f)) / 3.0f;
			if (v24 > 1.0f) v24 = 1.0f;
			if (v24 > 0.1f) {
				glTranslatef(0.0f, std::sin((pullTicks - 0.1f) * 1.3f) * 0.01f * (v24 - 0.1f), 0.0f);
			}
			glTranslatef(0.0f, 0.0f, v24 * 0.1f);
			glRotatef(-335.0f, 0.0f, 0.0f, 1.0f);
			glRotatef(-50.0f, 0.0f, 1.0f, 0.0f);
			glTranslatef(0.0f, 0.5f, 0.0f);
			glScalef(1.0f, 1.0f, (v24 * 0.2f) + 1.0f);
			glTranslatef(0.0f, -0.5f, 0.0f);
			glRotatef(50.0f, 0.0f, 1.0f, 0.0f);
			glRotatef(335.0f, 0.0f, 0.0f, 1.0f);
		}

		if (GuiComponent::isBlock3D(held)) {
			GuiComponent::drawBlock3DModel(held, lightShade, false);
		} else {
			std::string tex;
			uint32_t tint = 0xFFFFFFu;
			GuiComponent::getItemSpritePathAndTint(held, tex, tint);
			if (isPullingBow) {
				if (gBowPullTime >= 0.85f) tex = "textures/items/bow_pulling_2.png";
				else if (gBowPullTime >= 0.45f) tex = "textures/items/bow_pulling_1.png";
				else tex = "textures/items/bow_pulling_0.png";
			}
			if (held.getItem() && held.getItem()->isMirroredArt()) {
				glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
			}
			// Exact MCPE 0.8.1 ItemInHandRenderer::renderItem() 2D sprite chain
			glScalef(1.5f, 1.5f, 1.5f);
			glRotatef(50.0f, 0.0f, 1.0f, 0.0f);
			glRotatef(335.0f, 0.0f, 0.0f, 1.0f);
			glTranslatef(0.075f, -0.245f, -0.1f);
			glScalef(0.0625f, 0.0625f, 0.0625f);
			glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
			glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
			glRotatef(-180.0f, 1.0f, 0.0f, 0.0f);
			glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
			glTranslatef(0.0f, 0.0f, -16.0f);
			// Map drawTexturedSprite3D [-0.5..0.5, -0.5..0.5, -0.03125..0.03125] -> TextureTesselator [0..16, 0..1, 0..16]
			glTranslatef(8.0f, 0.5f, 8.0f);
			glScalef(16.0f, 16.0f, -16.0f);
			glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
			drawTexturedSprite3D(tex, lightShade, tint);
		}
	}

	glDisable(GL_ALPHA_TEST);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

struct SteveModelQuad3D {
	Vec3 v[4];
	float u[4];
	float tv[4];
	Vec3 normal;

	void init(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3,
	          int a, int b, int c, int d, float texW, float texH, bool mirror) {
		v[0] = v0; v[1] = v1; v[2] = v2; v[3] = v3;
		float u0 = static_cast<float>(a) / texW;
		float vTop = static_cast<float>(b) / texH;
		float u1 = static_cast<float>(c) / texW;
		float vBot = static_cast<float>(d) / texH;
		u[0] = u1; tv[0] = vTop;
		u[1] = u0; tv[1] = vTop;
		u[2] = u0; tv[2] = vBot;
		u[3] = u1; tv[3] = vBot;

		if (mirror) {
			std::swap(v[0], v[3]);
			std::swap(v[1], v[2]);
			std::swap(u[0], u[3]);
			std::swap(u[1], u[2]);
			std::swap(tv[0], tv[3]);
			std::swap(tv[1], tv[2]);
		}

		Vec3 e1 = v[0] - v[1];
		Vec3 e2 = v[0] - v[3];
		Vec3 n(e1.y * e2.z - e1.z * e2.y,
		       e1.z * e2.x - e1.x * e2.z,
		       e1.x * e2.y - e1.y * e2.x);
		float len = n.length();
		normal = (len > 1e-5f) ? (n / len) : Vec3(0.0f, -1.0f, 0.0f);
	}
};

struct SteveModelCube3D {
	SteveModelQuad3D quads[6];

	SteveModelCube3D(int u, int v, float x, float y, float z, int w, int h, int d,
	                 float inflate, float texW, float texH, bool mirror) {
		float x1 = x + static_cast<float>(w);
		float y1 = y + static_cast<float>(h);
		float z1 = z + static_cast<float>(d);

		float v20 = x - inflate;
		float v21 = y - inflate;
		float v22 = z - inflate;
		float v23 = x1 + inflate;
		float v24 = y1 + inflate;
		float v25 = z1 + inflate;
		if (!mirror) {
			std::swap(v20, v23);
		}

		Vec3 p[8] = {
			Vec3(v23, v21, v22),
			Vec3(v20, v21, v22),
			Vec3(v20, v24, v22),
			Vec3(v23, v24, v22),
			Vec3(v23, v21, v25),
			Vec3(v20, v21, v25),
			Vec3(v20, v24, v25),
			Vec3(v23, v24, v25),
		};

		int uMid = u + d + w;
		int vMid = v + d;
		quads[0].init(p[5], p[1], p[2], p[6], uMid, vMid, uMid + d, vMid + h, texW, texH, mirror);
		quads[1].init(p[0], p[4], p[7], p[3], u, vMid, u + d, vMid + h, texW, texH, mirror);
		quads[2].init(p[5], p[4], p[0], p[1], u + d, v, uMid, v + d, texW, texH, mirror);
		quads[3].init(p[2], p[3], p[7], p[6], uMid, v + d, uMid + w, v, texW, texH, mirror);
		quads[4].init(p[1], p[0], p[3], p[2], u + d, vMid, uMid, vMid + h, texW, texH, mirror);
		quads[5].init(p[4], p[5], p[6], p[7], uMid + d, vMid, uMid + d + w, vMid + h, texW, texH, mirror);
	}
};

struct SteveModelPart3D {
	float xOffset = 0.0f;
	float yOffset = 0.0f;
	float zOffset = 0.0f;
	float xRotAngle = 0.0f;
	float yRotAngle = 0.0f;
	float zRotAngle = 0.0f;
	bool visible = true;
	bool mirror = false;
	float texW = 64.0f;
	float texH = 32.0f;
	int texOffsetX = 0;
	int texOffsetY = 0;
	std::vector<SteveModelCube3D> cubes;

	SteveModelPart3D() = default;
	SteveModelPart3D(int u, int v, int tw, int th, bool mir = false)
		: visible(true), mirror(mir), texW(static_cast<float>(tw)), texH(static_cast<float>(th)),
		  texOffsetX(u), texOffsetY(v) {}

	void setPos(float x, float y, float z) {
		xOffset = x;
		yOffset = y;
		zOffset = z;
	}

	void addBox(float x, float y, float z, int w, int h, int d, float inflate = 0.0f) {
		cubes.emplace_back(texOffsetX, texOffsetY, x, y, z, w, h, d, inflate, texW, texH, mirror);
	}

	void translateTo(float scale) const {
		if (xOffset != 0.0f || yOffset != 0.0f || zOffset != 0.0f) {
			glTranslatef(scale * xOffset, scale * yOffset, scale * zOffset);
		}
		constexpr float kRadToDeg = 180.0f / 3.14159265f;
		if (zRotAngle != 0.0f) glRotatef(zRotAngle * kRadToDeg, 0.0f, 0.0f, 1.0f);
		if (yRotAngle != 0.0f) glRotatef(yRotAngle * kRadToDeg, 0.0f, 1.0f, 0.0f);
		if (xRotAngle != 0.0f) glRotatef(xRotAngle * kRadToDeg, 1.0f, 0.0f, 0.0f);
	}

	void render(float scale, float shade = 1.0f) const {
		if (!visible) return;
		glPushMatrix();
		translateTo(scale);

		glm::mat3 rot(1.0f);
		if (zRotAngle != 0.0f) {
			float c = std::cos(zRotAngle), s = std::sin(zRotAngle);
			rot = rot * glm::mat3(c, s, 0.0f, -s, c, 0.0f, 0.0f, 0.0f, 1.0f);
		}
		if (yRotAngle != 0.0f) {
			float c = std::cos(yRotAngle), s = std::sin(yRotAngle);
			rot = rot * glm::mat3(c, 0.0f, -s, 0.0f, 1.0f, 0.0f, s, 0.0f, c);
		}
		if (xRotAngle != 0.0f) {
			float c = std::cos(xRotAngle), s = std::sin(xRotAngle);
			rot = rot * glm::mat3(1.0f, 0.0f, 0.0f, 0.0f, c, s, 0.0f, -s, c);
		}

		glBegin(GL_QUADS);
		for (const auto& cube : cubes) {
			for (int qi = 0; qi < 6; ++qi) {
				const SteveModelQuad3D& q = cube.quads[qi];
				glm::vec3 wn = rot * glm::vec3(q.normal.x, q.normal.y, q.normal.z);
				float up = -wn.y;
				float faceShade = 0.88f;
				if (up > 0.55f) faceShade = 1.0f;
				else if (up < -0.55f) faceShade = 0.68f;
				else if (std::abs(wn.z) > 0.55f) faceShade = 0.94f;
				else faceShade = 0.82f;

				float c = std::min(1.0f, shade * faceShade);
				glColor4f(c, c, c, 1.0f);
				for (int vi = 0; vi < 4; ++vi) {
					glTexCoord2f(q.u[vi], q.tv[vi]);
					glVertex3f(q.v[vi].x * scale, q.v[vi].y * scale, q.v[vi].z * scale);
				}
			}
		}
		glEnd();
		glPopMatrix();
	}
};

struct SteveHumanoidModel3D {
	SteveModelPart3D headModel;
	SteveModelPart3D bodyModel;
	SteveModelPart3D rightArmModel;
	SteveModelPart3D leftArmModel;
	SteveModelPart3D rightLegModel;
	SteveModelPart3D leftLegModel;

	float attackTime = 0.0f;   // field_0
	bool isRiding = false;     // field_4
	bool holdingLeft = false;  // field_318
	bool holdingRight = false; // field_319
	bool sneaking = false;     // field_31A
	bool isUsingBow = false;   // isUsingBow

	// Matches MCPE 0.8.1 HumanoidModel::HumanoidModel(inflate, yOffset)
	// Supports both 64x64 modern Steve skin and 64x32 armor textures
	SteveHumanoidModel3D(float inflate, float yOff, int texH)
		: headModel(0, 0, 64, texH, false)
		, bodyModel(16, 16, 64, texH, false)
		, rightArmModel(40, 16, 64, texH, false)
		, leftArmModel(texH == 64 ? 32 : 40, texH == 64 ? 48 : 16, 64, texH, texH != 64)
		, rightLegModel(0, 16, 64, texH, false)
		, leftLegModel(texH == 64 ? 16 : 0, texH == 64 ? 48 : 16, 64, texH, texH != 64) {
		headModel.addBox(-4.0f, -8.0f, -4.0f, 8, 8, 8, inflate);
		headModel.setPos(0.0f, yOff + 0.0f, 0.0f);
		bodyModel.addBox(-4.0f, 0.0f, -2.0f, 8, 12, 4, inflate);
		bodyModel.setPos(0.0f, yOff + 0.0f, 0.0f);
		rightArmModel.addBox(-3.0f, -2.0f, -2.0f, 4, 12, 4, inflate);
		rightArmModel.setPos(-5.0f, yOff + 2.0f, 0.0f);
		leftArmModel.addBox(-1.0f, -2.0f, -2.0f, 4, 12, 4, inflate);
		leftArmModel.setPos(5.0f, yOff + 2.0f, 0.0f);
		rightLegModel.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4, inflate);
		rightLegModel.setPos(-2.0f, yOff + 12.0f, 0.0f);
		leftLegModel.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4, inflate);
		leftLegModel.setPos(2.0f, yOff + 12.0f, 0.0f);
	}

	// Exact 1:1 port of MCPE 0.8.1 HumanoidModel::setupAnim(a2, a3, a4, a5, a6, a7)
	void setupAnim(float a2, float a3, float a4, float a5, float a6, float /*a7*/) {
		constexpr float kPi = 3.1416f;
		headModel.yRotAngle = a5 / (180.0f / kPi);
		float v10 = a2 * 0.6662f;
		headModel.xRotAngle = a6 / (180.0f / kPi);
		float v11 = std::cos(a2 * 0.6662f) * a3;
		float v12 = std::cos(v10 + kPi);
		leftArmModel.xRotAngle = v11;
		rightArmModel.zRotAngle = 0.0f;
		leftArmModel.zRotAngle = 0.0f;
		rightLegModel.yRotAngle = 0.0f;
		leftLegModel.yRotAngle = 0.0f;
		rightArmModel.xRotAngle = v12 * a3;
		rightLegModel.xRotAngle = v11 * 1.4f;
		leftLegModel.xRotAngle = (v12 * a3) * 1.4f;

		if (isRiding) {
			float v14 = -(kPi * 0.5f);
			rightArmModel.xRotAngle += v14 * 0.4f;
			rightLegModel.xRotAngle = v14 * 0.8f;
			leftLegModel.xRotAngle = v14 * 0.8f;
			rightLegModel.yRotAngle = (kPi * 0.5f) * 0.2f;
			leftLegModel.yRotAngle = v14 * 0.2f;
			leftArmModel.xRotAngle += v14 * 0.4f;
		}
		if (holdingLeft) {
			leftArmModel.xRotAngle = (leftArmModel.xRotAngle * 0.5f) - ((kPi * 0.5f) * 0.2f);
		}
		if (holdingRight) {
			rightArmModel.xRotAngle = (rightArmModel.xRotAngle * 0.5f) - ((kPi * 0.5f) * 0.2f);
		}

		rightArmModel.yRotAngle = 0.0f;
		leftArmModel.yRotAngle = 0.0f;
		bodyModel.yRotAngle = 0.0f;
		rightArmModel.zOffset = 0.0f;
		rightArmModel.xOffset = -5.0f;
		leftArmModel.zOffset = 0.0f;
		leftArmModel.xOffset = 5.0f;

		float v15 = attackTime;
		if (v15 > 0.0f) {
			float v16 = std::sqrt(v15);
			float v17 = std::sin((v16 * kPi) + (v16 * kPi)) * 0.2f;
			bodyModel.yRotAngle = v17;
			rightArmModel.zOffset = std::sin(v17) * 5.0f;
			rightArmModel.xOffset = -(std::cos(v17) * 5.0f);
			leftArmModel.zOffset = -(std::sin(v17) * 5.0f);
			float v18 = std::cos(v17);
			float v19 = attackTime;
			leftArmModel.yRotAngle += v17;
			leftArmModel.xRotAngle += v17;
			leftArmModel.xOffset = v18 * 5.0f;
			float v20 = v17 + rightArmModel.yRotAngle;
			rightArmModel.yRotAngle = v20;
			float oneMinus = 1.0f - v19;
			float v21 = std::sin((1.0f - (oneMinus * oneMinus * oneMinus * oneMinus)) * kPi);
			float v22 = std::sin(v19 * kPi);
			float v23 = headModel.xRotAngle - 0.7f;
			rightArmModel.yRotAngle = v20 + (v17 + v17);
			rightArmModel.xRotAngle -= ((-(v23 * v22) * 0.75f) + (v21 * 1.2f));
			rightArmModel.zRotAngle = std::sin(v19 * kPi) * -0.4f;
		}

		if (sneaking) {
			bodyModel.xRotAngle = 0.5f;
			rightLegModel.zOffset = 4.0f;
			leftLegModel.zOffset = 4.0f;
			rightLegModel.yOffset = 9.0f;
			leftLegModel.yOffset = 9.0f;
			headModel.yOffset = 1.0f;
			rightArmModel.xRotAngle += 0.4f;
			leftArmModel.xRotAngle += 0.4f;
		} else {
			bodyModel.xRotAngle = 0.0f;
			rightLegModel.zOffset = 0.0f;
			leftLegModel.zOffset = 0.0f;
			headModel.yOffset = 0.0f;
			rightLegModel.yOffset = 12.0f;
			leftLegModel.yOffset = 12.0f;
		}

		float v25 = (std::cos(a4 * 0.09f) * 0.05f) + 0.05f;
		float v26 = std::sin(a4 * 0.067f);
		rightArmModel.zRotAngle += v25;
		float v28 = v26 * 0.05f;
		leftArmModel.zRotAngle -= v25;
		rightArmModel.xRotAngle += v28;
		leftArmModel.xRotAngle -= v28;

		if (isUsingBow) {
			float yRotAngle = headModel.yRotAngle;
			rightArmModel.yRotAngle = yRotAngle - 0.1f;
			leftArmModel.yRotAngle = (yRotAngle + 0.1f) + 0.4f;
			float v30 = headModel.xRotAngle - (kPi * 0.5f);
			rightArmModel.zRotAngle = v25;
			leftArmModel.zRotAngle = -v25;
			rightArmModel.xRotAngle = v30 + v28;
			leftArmModel.xRotAngle = v30 - v28;
		}
	}

	void render(float a2, float a3, float a4, float a5, float a6, float a7, float shade = 1.0f) {
		setupAnim(a2, a3, a4, a5, a6, a7);
		headModel.render(a7, shade);
		bodyModel.render(a7, shade);
		rightArmModel.render(a7, shade);
		leftArmModel.render(a7, shade);
		rightLegModel.render(a7, shade);
		leftLegModel.render(a7, shade);
	}
};

void renderWorldSteve3D(const Camera& c,
                        const GuiContext& guiCtx,
                        const glm::mat4& projection,
                        const glm::mat4& view,
                        float lightShade = 1.0f) {
	glActiveTexture(GL_TEXTURE0);
	if (!GuiComponent::bindTexture("textures/entity/steve.png")) return;

	static SteveHumanoidModel3D steveModel(0.0f, 0.0f, 64);
	static SteveHumanoidModel3D armorModel1(1.0f, 0.0f, 32);
	static SteveHumanoidModel3D armorModel2(0.5f, 0.0f, 32);

	glUseProgram(0);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.1f);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(projection));
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(view));

	const float kPxScale = 0.0625f;
	int slotIdx = std::max(0, std::min(8, guiCtx.selectedHotbarSlot));
	const ItemInstance& held = guiCtx.inventorySlots[static_cast<size_t>(slotIdx)];
	bool hasHeld = !held.isNull() && held.getStackSize() > 0;
	bool isUsingBow = hasHeld && Item::mBow && held.getItem() == Item::mBow && gBowPulling;
	bool sneaking = !c.mIsFlying && (glinput.KeyDown(GLFW_KEY_LEFT_SHIFT) || glinput.KeyDown(GLFW_KEY_RIGHT_SHIFT));

	// Convert renderer yaw (0=+X, 90=+Z) to MCPE 0.8.1 yaw (yaw_mcpe = yaw - 90, 180 - yaw_mcpe = 270 - yaw)
	float bodyYawMcpe = c.mBodyYaw - 90.0f;
	float headYawMcpe = static_cast<float>(c.yaw) - 90.0f;
	float netHeadYaw = headYawMcpe - bodyYawMcpe;
	while (netHeadYaw < -180.0f) netHeadYaw += 360.0f;
	while (netHeadYaw >= 180.0f) netHeadYaw -= 360.0f;
	float headPitch = static_cast<float>(-c.pitch);
	float bobTicks = static_cast<float>(glfwGetTime()) * 20.0f;

	// Exact MCPE 0.8.1 MobRenderer::render transform chain (L116-124):
	// setupPosition(a3, a4 - ridingHeight, a5) -> setupRotations(180 - bodyYaw) -> glScalef(-1, -1, 1) -> glTranslatef(0, -24*0.0625 - 0.0078125, 0)
	glPushMatrix();
	glTranslatef(c.mPos.x, c.mPos.y - c.mHeightOffset, c.mPos.z);
	glRotatef(180.0f - bodyYawMcpe, 0.0f, 1.0f, 0.0f);
	glScalef(-1.0f, -1.0f, 1.0f);
	glTranslatef(0.0f, (kPxScale * -24.0f) - 0.0078125f, 0.0f);

	// 1. Base Steve model (HumanoidMobRenderer::render + HumanoidModel::render)
	steveModel.attackTime = c.mSwingProgress;
	steveModel.holdingRight = hasHeld;
	steveModel.sneaking = sneaking;
	steveModel.isUsingBow = isUsingBow;
	steveModel.render(c.mWalkAnimPos, c.mWalkAnimSpeed, bobTicks, netHeadYaw, headPitch, kPxScale, lightShade);

	// 2. Equipped Armor passes (PlayerRenderer::prepareArmor L59-86, 4 passes: 0=helmet, 1=chestplate, 2=leggings, 3=boots)
	for (int armour = 0; armour < 4; ++armour) {
		const ItemInstance& armorItem = guiCtx.armorSlots[armour];
		if (armorItem.isNull() || armorItem.getStackSize() <= 0) continue;
		std::string armorTex = getArmorTexturePath(armorItem, armour == 2);
		if (armorTex.empty() || !GuiComponent::bindTexture(armorTex)) continue;

		SteveHumanoidModel3D& am = (armour == 2) ? armorModel2 : armorModel1;
		am.headModel.visible = (armour == 0);
		am.bodyModel.visible = (armour == 1 || armour == 2);
		am.rightArmModel.visible = (armour == 1);
		am.leftArmModel.visible = (armour == 1);
		am.rightLegModel.visible = (armour == 2 || armour == 3);
		am.leftLegModel.visible = (armour == 2 || armour == 3);

		am.attackTime = steveModel.attackTime;
		am.holdingRight = steveModel.holdingRight;
		am.sneaking = steveModel.sneaking;
		am.isUsingBow = steveModel.isUsingBow;
		am.render(c.mWalkAnimPos, c.mWalkAnimSpeed, bobTicks, netHeadYaw, headPitch, kPxScale, lightShade);
	}

	// 3. Held Item in 3rd person (Exact MCPE 0.8.1 HumanoidMobRenderer::additionalRendering L33-90)
	if (hasHeld) {
		glPushMatrix();
		steveModel.rightArmModel.translateTo(kPxScale);
		glTranslatef(-0.0625f, 0.4375f, 0.0625f);

		if (GuiComponent::isBlock3D(held)) {
			// HumanoidMobRenderer.cpp L48-56: 3D Tile / Block
			glTranslatef(0.0f, 0.1875f, -0.3125f);
			glRotatef(200.0f, 1.0f, 0.0f, 0.0f);
			glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
			glScalef(0.375f, 0.375f, 0.375f);
			GuiComponent::drawBlock3DModel(held, lightShade, false);
		} else {
			if (Item::mBow && held.getItem() == Item::mBow) {
				// HumanoidMobRenderer.cpp L58-63 + L80-85: Item::bow
				glTranslatef(0.2f, 0.125f, -0.3875f);
				glRotatef(-180.0f, 1.0f, 0.0f, 0.0f);
				glRotatef(10.0f, 0.0f, 1.0f, 0.0f);
				glScalef(0.625f, 0.625f, 0.625f);
				glRotatef(-100.0f, 1.0f, 0.0f, 0.0f);
				glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
			} else if (held.getItem() && held.getItem()->isHandEquipped()) {
				// HumanoidMobRenderer.cpp L75-85: Tools / Swords (isHandEquipped)
				glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
				glTranslatef(0.1f, 0.265f, 0.0f);
				glScalef(0.625f, 0.625f, 0.625f);
				glRotatef(80.0f, 1.0f, 0.0f, 0.0f);
				glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
			} else {
				// HumanoidMobRenderer.cpp L65-73 + L85: Regular flat items
				glTranslatef(0.25f, 0.1875f, -0.1875f);
				glScalef(0.375f, 0.375f, 0.375f);
				glRotatef(60.0f, 0.0f, 0.0f, 1.0f);
				glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
				glRotatef(20.0f, 0.0f, 0.0f, 1.0f);
			}

			std::string tex;
			uint32_t tint = 0xFFFFFFu;
			GuiComponent::getItemSpritePathAndTint(held, tex, tint);
			if (isUsingBow && gBowPullTime > 0.0f) {
				if (gBowPullTime >= 0.85f) tex = "textures/items/bow_pulling_2.png";
				else if (gBowPullTime >= 0.45f) tex = "textures/items/bow_pulling_1.png";
				else tex = "textures/items/bow_pulling_0.png";
			}

			// Exact MCPE 0.8.1 ItemInHandRenderer::renderItem() 2D sprite chain (L320-331)
			glScalef(1.5f, 1.5f, 1.5f);
			glRotatef(50.0f, 0.0f, 1.0f, 0.0f);
			glRotatef(335.0f, 0.0f, 0.0f, 1.0f);
			glTranslatef(0.075f, -0.245f, -0.1f);
			glScalef(0.0625f, 0.0625f, 0.0625f);
			glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
			glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
			glRotatef(-180.0f, 1.0f, 0.0f, 0.0f);
			glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
			glTranslatef(0.0f, 0.0f, -16.0f);
			glTranslatef(8.0f, 0.5f, 8.0f);
			glScalef(16.0f, 16.0f, -16.0f);
			glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
			drawTexturedSprite3D(tex, lightShade, tint);
		}
		glPopMatrix();
	}

	glPopMatrix();

	glDisable(GL_ALPHA_TEST);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

} // namespace

void renderThreadStuff()
{
	GLFWwindow* window = renderThreadInit();

	if (!window)
		return;

	gCursorLocked = true;






	//RenderChunk rc[8]; (BlockPos(0, 0, 0));

	//RenderChunkRenderParameters renderParams;
	//renderParams.layer = TerrainLayer::Opaque;//?
	//renderParams.forceUnsorted = false;
	//renderParams.forceFog = false;

	mce::RenderMaterialGroup::switchable.InitRenderMaterials();

	//ShaderComponent mainVertex("./restore/shaders/test_textured.vs", VERTEX_SHADER);
	//ShaderComponent mainFragment("./restore/shaders/test_textured.fs", FRAGMENT_SHADER);

	//Shader shader(mainVertex, mainFragment);
	//shader.activate();
	//shader.setInt("atlas", 0);
	//glUniform1i(glGetUniformLocation(shader, "atlas"), 0);//set the atlas to be texture 0

	Camera c;

	//sptr = &shader;



	ResourceLocation metaFile("textures/terrain_texture.json");

	ResourcePackManager resourcePackManager;

	auto atlas = make_shared<TextureAtlas>(metaFile, resourcePackManager, &texgroup);

	BlockGraphics::setTextureAtlas(atlas);

	atlas->loadMetaFile();

	BlockGraphics::initBlocks();
	RenderChunk::initMaterials(texgroup);

	updateFoliageColors();

	/*Tessellator::instance.begin(4);


	Tessellator::instance.vertexUV(0, 0, 0, 0.0f, 0.0f);
	Tessellator::instance.vertexUV(1, 0, 0, 1.0f, 0.0f);
	Tessellator::instance.vertexUV(1, 1, 0, 1.0f, 1.0f);
	Tessellator::instance.vertexUV(0, 1, 0, 0.0f, 1.0f);

	mce::Mesh m = Tessellator::instance.end();*/

	float currentFrame = 0.0f;
	float deltaTime;
	float lastFrame;


	//needed for post-processing
	static const ChunkPos offsets[] = {
	{ 0, 0 },
	{ -1, -1 },
	{ 0, -1 },
	{ 1, -1 },
	{ -1, 0 },
	{ 1, 0 },
	{ -1, 1 },
	{ 0, 1 },
	{ 1, 1 }
	};

	GLuint gLightmapTex = createLightmapTexture();
	GLuint gWhiteTex = createWhiteTexture();

	mce::MaterialPtr chunkMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_opaque");
	mce::MaterialPtr chunkSeasonsMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_opaque_seasons");
	mce::MaterialPtr chunkAlphaMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_alpha");
	mce::MaterialPtr chunkAlphaSingleMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_alpha_single_side");
	mce::MaterialPtr chunkAlphaSeasonsMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_alpha_seasons");
	mce::MaterialPtr chunkDoubleSideMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_doubleside");
	mce::MaterialPtr chunk2Material = mce::RenderMaterialGroup::switchable.getMaterial("terrain_water");
	mce::MaterialPtr chunk2BelowMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_water_below");
	mce::MaterialPtr chunk3Material = mce::RenderMaterialGroup::switchable.getMaterial("terrain_blend");
	mce::MaterialPtr chunk3BelowMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_blend_below");

	mce::MaterialPtr chunkFogMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_opaque_fog");
	mce::MaterialPtr chunkFogNoCullMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_opaque_fog_no_cull");
	mce::MaterialPtr chunkAlphaFogMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_alpha_fog");
	mce::MaterialPtr chunkAlphaSingleFogMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_alpha_single_side_fog");
	mce::MaterialPtr chunkDoubleSideFogMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_doubleside_fog");
	mce::MaterialPtr chunkWaterFogMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_water_fog");
	mce::MaterialPtr chunkWaterBelowFogMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_water_below_fog");
	mce::MaterialPtr chunkBlendFogMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_blend_fog");
	mce::MaterialPtr chunkBlendBelowFogMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_blend_below_fog");
	
	std::vector<mce::MaterialPtr*> mptrs = {
		&chunkMaterial, &chunkSeasonsMaterial,
		&chunkAlphaMaterial, &chunkAlphaSingleMaterial, &chunkAlphaSeasonsMaterial,
		&chunkDoubleSideMaterial,
		&chunk2Material, &chunk2BelowMaterial,
		&chunk3Material, &chunk3BelowMaterial,
		&chunkFogMaterial, &chunkFogNoCullMaterial,
		&chunkAlphaFogMaterial, &chunkAlphaSingleFogMaterial,
		&chunkDoubleSideFogMaterial,
		&chunkWaterFogMaterial, &chunkWaterBelowFogMaterial,
		&chunkBlendFogMaterial, &chunkBlendBelowFogMaterial
	};

	std::vector<mce::Shader*> uniqueTerrainShaders;
	uniqueTerrainShaders.reserve(mptrs.size());
	for (auto sl : mptrs) {
		if (!sl || !(*sl).operator->() || !(*sl)->myShader) continue;
		mce::Shader* sh = (*sl)->myShader;
		if (std::find(uniqueTerrainShaders.begin(), uniqueTerrainShaders.end(), sh) == uniqueTerrainShaders.end()) {
			uniqueTerrainShaders.push_back(sh);
		}
	}
	int lastProjW = -1, lastProjH = -1;
	int lastUploadedRenderDist = -1;
	glm::vec4 lastUploadedFogCol(-1.0f);
	glm::vec2 lastUploadedFogCtrl(-1.0f);
	glm::mat4 lastUploadedWorldView(0.0f);
	bool hasUploadedWorldView = false;

	glm::mat4 projection = glm::perspective(glm::radians(85.0f),
		(float)screenx / (float)screeny,
		0.1f, 1000.0f);

	float tileStepU = 1.0f / 512.0f;
	float tileStepV = 1.0f / 256.0f;
	if (BlockGraphics::mBlocks[1]) {
		const TextureUVCoordinateSet& stoneTex = BlockGraphics::mBlocks[1]->getTexture(Facing::UP);
		float su = std::abs(stoneTex._u1 - stoneTex._u0) / 16.0f;
		float sv = std::abs(stoneTex._v1 - stoneTex._v0) / 16.0f;
		if (su > 0.0f && sv > 0.0f) {
			tileStepU = su;
			tileStepV = sv;
		}
	}

	for (mce::Shader* sh : uniqueTerrainShaders)
	{
		sh->setMatrix4("PROJ", glm::value_ptr(projection));
		sh->setInt("TEXTURE_0", 0);
		sh->setInt("TEXTURE_1", 1);
		sh->setInt("TEXTURE_2", 2);
		sh->setInt("TEXTURE_3", 3);
		sh->setFloat("FAR_CHUNKS_DISTANCE", 48.0f);
		sh->setFloat("RENDER_DISTANCE", 48.0f);
		sh->setVector2("TILE_TEXEL_STEP", tileStepU, tileStepV);
	}
	lastProjW = screenx;
	lastProjH = screeny;

	std::unordered_map<ChunkPos,RenderChunk*[8]> rc;
	std::vector<RenderChunk*> renderChunkPool;
	std::unordered_map<ChunkPos, LevelChunk*> heldChunks;

	std::unordered_set<ChunkPos> needsReRendering;
	std::unordered_set<ChunkPos> needsReRendering2;
	gActiveDirtyMeshChunks = &needsReRendering2;
	std::unordered_set<ChunkPos> loadedChunkSet;
	std::unordered_set<ChunkPos> litChunks;
	ChunkPos lastUnloadCamChunkPos(INT_MAX, INT_MAX);
	int lastUnloadRenderDist = -1;

	constexpr size_t kAsyncBuilderPoolSize = 12;
	std::unique_ptr<RenderChunkBuilder> chunkBuilder;
	std::vector<std::unique_ptr<RenderChunkBuilder>> asyncBuilderPool;
	std::unordered_set<RenderChunk*> inFlightRenderChunks;
	std::unordered_set<RenderChunk*> retiredInFlightChunks;
	glm::vec3 latestRenderCamPos(0.0f);
	uint64_t worldEpoch = 0;
	size_t maxRenderPoolCache = 512;

	auto acquireRenderChunk = [&](const BlockPos& pos) -> RenderChunk* {
		if (!renderChunkPool.empty()) {
			RenderChunk* recycled = renderChunkPool.back();
			renderChunkPool.pop_back();
			recycled->setPos(pos);
			return recycled;
		}
		return new RenderChunk(pos);
	};

	auto releaseRenderChunk = [&](RenderChunk*& part, size_t maxPoolSize) {
		if (!part) return;
		if (inFlightRenderChunks.find(part) != inFlightRenderChunks.end()) {
			retiredInFlightChunks.insert(part);
			part = nullptr;
			return;
		}
		part->reset();
		if (renderChunkPool.size() < maxPoolSize) {
			renderChunkPool.push_back(part);
		} else {
			delete part;
		}
		part = nullptr;
	};

	auto acquireHeldChunk = [&](ChunkSource& chunkSrc, const ChunkPos& cp) -> LevelChunk* {
		auto it = heldChunks.find(cp);
		if (it != heldChunks.end()) {
			return it->second;
		}
		LevelChunk* lc = chunkSrc.getOrLoadChunk(cp, ChunkSource::LoadMode::Deferred);
		if (lc) {
			heldChunks[cp] = lc;
		}
		return lc;
	};

	WorkerPool::setMainThreadPumpActive(true);

	Recipes::getInstance()->init();

	std::vector<WorldItemEntity> worldItems;
	float worldItemTickAccum = 0.0f;
	MobManager mobManager;
	float lastLightmapSun = -1.0f;
	bool hasInitialSpawn = false;
	bool isMining = false;
	BlockPos miningPos(0, -1, 0);
	float miningProgress = 0.0f;
	float hazardDamageCooldown = 0.0f;
	float eatCooldown = 0.0f;
	currentFrame = static_cast<float>(glfwGetTime());

	auto applyFancyGraphicsMode = [&](bool fancy) {
		Block::DEPRECATEDcallOnGraphicsModeChanged(fancy, false, fancy);
		for (size_t i = 0; i < sizeof(BlockGraphics::mBlocks) / sizeof(BlockGraphics::mBlocks[0]); ++i) {
			if (BlockGraphics::mBlocks[i]) {
				BlockGraphics::mBlocks[i]->onGraphicsModeChanged(fancy, false);
			}
		}
		for (auto& kv : rc) {
			for (int i = 0; i < 8; ++i) {
				if (kv.second[i]) {
					kv.second[i]->setDirty(Tick(0), true);
				}
			}
			needsReRendering2.insert(kv.first);
		}
	};
	applyFancyGraphicsMode(gGuiOptions.fancyGraphics);
	if (const char* fgEnv = std::getenv("MCPE_FORCE_GREEDY")) {
		gGuiOptions.greedyMeshing = (std::atoi(fgEnv) != 0);
	}
	bool lastFancyGraphics = gGuiOptions.fancyGraphics;
	bool lastGreedyMeshing = gGuiOptions.greedyMeshing;
	if (const char* autoWorld = std::getenv("MCPE_AUTOLOAD_WORLD")) {
		if (autoWorld[0] != '\0') {
			std::string wname(autoWorld);
			gGuiContext.activeWorldDir = "worlds/" + wname;
			gGuiContext.activeWorldName = wname;
			gGuiContext.worldOpenRequested = true;
			gGuiContext.setScreen(new ProgressScreen());
		}
	}

	auto saveActiveWorld = [&]() {
		if (!gGuiContext.hasWorldLoaded) return;
		gGuiContext.hasPlayerTransform = true;
		gGuiContext.playerX = c.mPos.x;
		gGuiContext.playerY = c.mBB.min.y;
		gGuiContext.playerZ = c.mPos.z;
		gGuiContext.playerYaw = c.yaw;
		gGuiContext.playerPitch = c.pitch;

		WorldMetadata meta;
		captureMetadataFromContext(gGuiContext, meta);
		saveWorldMetadata(gGuiContext.activeWorldDir, meta);

		if (commandContext.level) {
			for (auto& kv : heldChunks) {
				LevelChunk* lc = kv.second;
				if (lc && lc->getState() == ChunkState::Loaded) {
					serializeChunkWorldItems(kv.first, worldItems, *lc, false);
				}
			}
			commandContext.level->saveDirtyChunks();
			WorkerPool::drainWorkerPool(WorkerRole::Disk);
		}
	};

	auto closeActiveWorld = [&]() {
		++worldEpoch;
		WorkerPool::drainWorkerPool(WorkerRole::MainThread);
		WorkerPool::drainWorkerPool(WorkerRole::Streaming);
		for (RenderChunk* orphan : retiredInFlightChunks) {
			delete orphan;
		}
		retiredInFlightChunks.clear();
		inFlightRenderChunks.clear();
		asyncBuilderPool.clear();
		if (commandContext.dim != nullptr && !heldChunks.empty()) {
			ChunkSource& chunkSrc = commandContext.dim->getChunkSource();
			chunkSrc.hintDiscardBatchBegin();
			for (auto& kv : heldChunks) {
				LevelChunk* lc = kv.second;
				if (lc) {
					serializeChunkWorldItems(kv.first, worldItems, *lc, true);
					chunkSrc.releaseChunk(*lc);
				}
			}
			chunkSrc.hintDiscardBatchEnd();
		}
		heldChunks.clear();
		WorkerPool::drainWorkerPool(WorkerRole::Disk);
		for (auto& kv : rc) {
			for (int i = 0; i < 8; ++i) {
				delete kv.second[i];
				kv.second[i] = nullptr;
			}
		}
		rc.clear();
		for (RenderChunk* pooled : renderChunkPool) {
			delete pooled;
		}
		renderChunkPool.clear();
		needsReRendering.clear();
		needsReRendering2.clear();
		loadedChunkSet.clear();
		litChunks.clear();
		lastUnloadCamChunkPos = ChunkPos(INT_MAX, INT_MAX);
		lastUnloadRenderDist = -1;
		worldItems.clear();
		worldItemTickAccum = 0.0f;
		mobManager.clear();
		PaintingSystem::clear();
		sFallingBlocks.clear();
		gPlayerFireTicks = 0;
		lastLightmapSun = -1.0f;
		isMining = false;
		miningProgress = 0.0f;
		hazardDamageCooldown = 0.0f;
		eatCooldown = 0.0f;
		chunkBuilder.reset();
		c.setWorld(nullptr, nullptr);
		commandContext.dim = nullptr;
		delete commandContext.level;
		commandContext.level = nullptr;
		LevelChunk::trimMemoryPool();
		LevelChunk::SubChunk::trimMemoryPool();
		gGuiContext.hasWorldLoaded = false;
	};

	auto openActiveWorld = [&]() {
		if (gGuiContext.hasWorldLoaded || commandContext.level) {
			saveActiveWorld();
			closeActiveWorld();
		}

		WorldMetadata meta;
		meta.name = gGuiContext.activeWorldName;
		meta.seed = gGuiContext.activeWorldSeed;
		meta.isCreative = gGuiContext.isCreative;
		loadWorldMetadata(gGuiContext.activeWorldDir, meta);
		applyMetadataToContext(meta, gGuiContext);
		gGuiHud.selectedSlot = gGuiContext.selectedHotbarSlot;

		std::string dbPath = getWorldDbPath(gGuiContext.activeWorldDir);
		Level* level = new Level(std::make_unique<DBStorage>(meta.name, dbPath));
		level->getLevelData().setLevelName(meta.name);
		level->getLevelData().setSeed(static_cast<RandomSeed>(meta.seed));
		level->getLevelData().setGameType(meta.isCreative ? GameType::Creative : GameType::Survival);
		level->setTime(static_cast<int>(gGuiContext.worldTime));
		commandContext.level = level;
		commandContext.dim = &level->createDimension(Overworld);
		c.setWorld(commandContext.level, commandContext.dim);
		chunkBuilder = make_unique<RenderChunkBuilder>(commandContext.dim->getChunkSource());
		asyncBuilderPool.clear();
		asyncBuilderPool.reserve(kAsyncBuilderPoolSize);
		for (size_t i = 0; i < kAsyncBuilderPoolSize; ++i) {
			asyncBuilderPool.push_back(make_unique<RenderChunkBuilder>(commandContext.dim->getChunkSource()));
		}

		if (meta.hasPlayerPos) {
			c.setPositionAndLook(meta.playerX, meta.playerY, meta.playerZ, meta.playerYaw, meta.playerPitch);
			hasInitialSpawn = true;
			gGuiContext.respawnRequested = false;
		} else if (meta.hasBedSpawn) {
			c.setPositionAndLook(meta.bedSpawnX + 0.5f,
			                     static_cast<float>(meta.bedSpawnY) + 0.05f,
			                     meta.bedSpawnZ + 0.5f, 0.0, 0.0);
			hasInitialSpawn = false;
			gGuiContext.respawnRequested = true;
		} else {
			c.setPositionAndLook(0.5f, 98.0f, 0.5f, 0.0, 0.0);
			hasInitialSpawn = false;
			gGuiContext.respawnRequested = true;
		}
		gGuiContext.hasWorldLoaded = true;
		gGuiContext.worldOpenRequested = false;
	};



	std::string lastSavedOptions = gGuiOptions.serialize();

	// Benchmark state (--bench-chunks)
	bool benchStarted = false;
	int benchPhase = 0; // 0 = waiting init load, 1 = streaming walk + thrash + block edit, 2 = done
	int benchInitFrames = 0;
	int benchWalkStep = 0;
	double benchInitWallMs = 0.0;
	std::chrono::high_resolution_clock::time_point benchInitStart{};
	double benchWalkWallMs = 0.0;
	std::chrono::high_resolution_clock::time_point benchWalkStart{};
	float benchStartX = 0.0f, benchStartY = 85.0f, benchStartZ = 0.0f;
	std::vector<double> benchCpuFrameMs;
	std::vector<double> benchWallFrameMs;
	std::vector<double> benchEditFrameMs;
	auto printBenchMetrics = [&](const char* tag, int frames, double wallTotalMs, const std::vector<double>& cpuMs, const std::vector<double>& wallMs) {
		auto& m = ChunkPerfMetrics::instance;
		double avgCpu = cpuMs.empty() ? 0.0 : std::accumulate(cpuMs.begin(), cpuMs.end(), 0.0) / cpuMs.size();
		double avgWall = wallMs.empty() ? 0.0 : std::accumulate(wallMs.begin(), wallMs.end(), 0.0) / wallMs.size();
		double maxCpu = 0.0, p95Cpu = 0.0;
		if (!cpuMs.empty()) {
			std::vector<double> sorted = cpuMs;
			std::sort(sorted.begin(), sorted.end());
			maxCpu = sorted.back();
			p95Cpu = sorted[std::min(sorted.size() - 1, (sorted.size() * 95) / 100)];
		}
		double maxWall = 0.0, p95Wall = 0.0;
		if (!wallMs.empty()) {
			std::vector<double> sorted = wallMs;
			std::sort(sorted.begin(), sorted.end());
			maxWall = sorted.back();
			p95Wall = sorted[std::min(sorted.size() - 1, (sorted.size() * 95) / 100)];
		}
		double avgFpsCpu = (avgCpu > 0.0001) ? (1000.0 / avgCpu) : 0.0;
		double avgFpsWall = (avgWall > 0.0001) ? (1000.0 / avgWall) : 0.0;
		printf("[BENCH_%s] frames=%d wallTotalMs=%.2f cpuAvgMs=%.2f cpuP95Ms=%.2f cpuMaxMs=%.2f cpuFps=%.1f wallAvgMs=%.2f wallP95Ms=%.2f wallMaxMs=%.2f wallFps=%.1f\n",
		       tag, frames, wallTotalMs, avgCpu, p95Cpu, maxCpu, avgFpsCpu, avgWall, p95Wall, maxWall, avgFpsWall);
		printf("[BENCH_%s_PHASES] unloadMs=%.2f loadPhaseMs=%.2f dbIoMs=%.2f lightMs=%.2f meshMs=%.2f (buildMs=%.2f uploadMs=%.2f) faceSortMs=%.2f renderMs=%.2f\n",
		       tag,
		       m.unloadUs.load() / 1000.0,
		       m.loadPhaseUs.load() / 1000.0,
		       m.dbUs.load() / 1000.0,
		       m.lightUs.load() / 1000.0,
		       m.meshUs.load() / 1000.0,
		       m.meshBuildUs.load() / 1000.0,
		       m.meshUploadUs.load() / 1000.0,
		       m.faceSortUs.load() / 1000.0,
		       m.renderUs.load() / 1000.0);
		printf("[BENCH_%s_COUNTS] dbHasKey=%llu dbRead=%llu dbHits=%llu lightCalls=%llu meshRebuilds=%llu emptySkipped=%llu faceSorts=%llu chunkNew=%llu chunkDel=%llu subNew=%llu subDel=%llu poolMalloc=%llu poolFree=%llu glGen=%llu glDel=%llu glPoolHits=%llu\n",
		       tag,
		       (unsigned long long)m.dbHasKeyCalls.load(),
		       (unsigned long long)m.dbReadCalls.load(),
		       (unsigned long long)m.dbChunkReadHits.load(),
		       (unsigned long long)m.lightPropagateCalls.load(),
		       (unsigned long long)m.meshRebuilds.load(),
		       (unsigned long long)m.emptySubChunksSkipped.load(),
		       (unsigned long long)m.faceSorts.load(),
		       (unsigned long long)m.chunkNewCalls.load(),
		       (unsigned long long)m.chunkDeleteCalls.load(),
		       (unsigned long long)m.subChunkNewCalls.load(),
		       (unsigned long long)m.subChunkDeleteCalls.load(),
		       (unsigned long long)m.poolMallocCalls.load(),
		       (unsigned long long)m.poolFreeCalls.load(),
		       (unsigned long long)mce::Mesh::sGlGenCalls,
		       (unsigned long long)mce::Mesh::sGlDeleteCalls,
		       (unsigned long long)mce::Mesh::sGlPoolHits);
		fflush(stdout);
	};

	if (gBenchChunksMode && !benchStarted) {
		benchStarted = true;
		gGuiContext.activeWorldDir = gBenchWorldDir;
		gGuiContext.activeWorldName = "BenchWorld";
		gGuiContext.activeWorldSeed = gBenchWorldSeed;
		gGuiContext.isCreative = true;
		gGuiContext.worldOpenRequested = true;
		gGuiContext.setScreen(new ProgressScreen("Benchmarking chunks"));
		ChunkPerfMetrics::instance.reset();
		mce::Mesh::resetStats();
		benchInitStart = std::chrono::high_resolution_clock::now();
	}

	while (!glfwWindowShouldClose(window)) 
	{
		auto frameStartTP = std::chrono::high_resolution_clock::now();
		WorkerPool::pumpMainThreadCallbacks();
		lastFrame = currentFrame;
		currentFrame = glfwGetTime();

		deltaTime = currentFrame - lastFrame;
		gGuiContext.deltaTime = deltaTime;
		static bool lastUseTouchUI = gGuiOptions.useTouchUI;
		if (gGuiOptions.guiScale != gGuiContext.guiScale || gGuiOptions.useTouchUI != lastUseTouchUI) {
			lastUseTouchUI = gGuiOptions.useTouchUI;
			updateGuiScale();
			if (gGuiContext.currentScreen) {
				gGuiContext.currentScreen->init(&gGuiContext, gGuiContext.guiWidth, gGuiContext.guiHeight);
			}
		}
		if (gGuiOptions.fancyGraphics != lastFancyGraphics) {
			lastFancyGraphics = gGuiOptions.fancyGraphics;
			applyFancyGraphicsMode(gGuiOptions.fancyGraphics);
		}
		if (gGuiOptions.greedyMeshing != lastGreedyMeshing) {
			lastGreedyMeshing = gGuiOptions.greedyMeshing;
			for (auto& kv : rc) {
				for (int i = 0; i < 8; ++i) {
					if (kv.second[i]) {
						kv.second[i]->setDirty(Tick(0), true);
					}
				}
				needsReRendering2.insert(kv.first);
			}
		}
		if (gGuiContext.worldSaveAndCloseRequested) {
			saveActiveWorld();
			closeActiveWorld();
			gGuiContext.worldSaveAndCloseRequested = false;
		}
		if (gGuiContext.worldOpenRequested) {
			openActiveWorld();
		}
		if (gGuiContext.shouldQuit) {
			saveActiveWorld();
			closeActiveWorld();
			glfwSetWindowShouldClose(window, true);
			continue;
		}

		float clampedFrameDt = std::min(std::max(deltaTime, 0.0001f), 0.1f);
		if (gGuiContext.hurtFlashTimer > 0.0f) {
			gGuiContext.hurtFlashTimer = std::max(0.0f, gGuiContext.hurtFlashTimer - clampedFrameDt);
		}
		if (gGuiContext.statusBannerTimer > 0.0f) {
			gGuiContext.statusBannerTimer = std::max(0.0f, gGuiContext.statusBannerTimer - clampedFrameDt);
		}

		static bool gManualCursorUnlock = false;
		if (gGuiContext.state != GameState::InGame || gGuiContext.currentScreen != nullptr) {
			gManualCursorUnlock = false;
		}
		auto syncCursorLock = [&]() {
			bool wantLock = (gGuiContext.state == GameState::InGame && gGuiContext.currentScreen == nullptr && !gManualCursorUnlock);
			if (wantLock != gCursorLocked) {
				gCursorLocked = wantLock;
				glfwSetInputMode(window, GLFW_CURSOR, gCursorLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
				if (gCursorLocked) {
					double cx = 0.0, cy = 0.0;
					glfwGetCursorPos(window, &cx, &cy);
					glinput.mouseXPos = cx;
					glinput.mouseYPos = cy;
					c.lastX = cx;
					c.lastY = cy;
					c.firstMouse = true;
					c.mIgnoreMouseFrames = 2;
				}
			}
		};

		bool renderWorld = ((gGuiContext.state == GameState::InGame) ||
		                    (gGuiContext.currentScreen && gGuiContext.currentScreen->renderGameBehind())) &&
		                   commandContext.level != nullptr && commandContext.dim != nullptr;

		if (gGuiContext.sleepCompleteRequested && commandContext.level) {
			gGuiContext.sleepCompleteRequested = false;
			int64_t nextDay = (gGuiContext.worldTime / 24000) + 1;
			gGuiContext.worldTime = nextDay * 24000;
			gGuiContext.worldTimeSubTick = 0.0f;
			commandContext.level->setTime(static_cast<int>(gGuiContext.worldTime));
			gGuiContext.statusBannerText = "Good morning!";
			gGuiContext.statusBannerTimer = 2.0f;
		}

		bool isPausedByScreen = (gGuiContext.currentScreen && gGuiContext.currentScreen->isPauseScreen());
		if (gGuiContext.state == GameState::InGame && !gGuiContext.loadingChunks &&
		    !isPausedByScreen && commandContext.level) {
			gGuiContext.worldTimeSubTick += clampedFrameDt * 20.0f;
			if (gGuiContext.worldTimeSubTick >= 1.0f) {
				int64_t addTicks = static_cast<int64_t>(gGuiContext.worldTimeSubTick);
				gGuiContext.worldTime += addTicks;
				gGuiContext.worldTimeSubTick -= static_cast<float>(addTicks);
				commandContext.level->setTime(static_cast<int>(gGuiContext.worldTime));
			}
		}

		CelestialInfo skyInfo = computeCelestialInfo(gGuiContext.worldTime, gGuiContext.worldTimeSubTick);
		if (std::abs(skyInfo.sunFactor - lastLightmapSun) > 0.005f) {
			updateLightmapTexture(gLightmapTex, skyInfo.sunFactor);
			lastLightmapSun = skyInfo.sunFactor;
		}

		glDepthMask(GL_TRUE);
		if (renderWorld) {
			glClearColor(skyInfo.fogColor.r, skyInfo.fogColor.g, skyInfo.fogColor.b, 1.0f);
		} else {
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		}
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if ((renderWorld || gGuiContext.loadingChunks) && commandContext.level != nullptr && commandContext.dim != nullptr) {
			if (gGuiContext.state == GameState::InGame && !gGuiContext.loadingChunks &&
			    (!hasInitialSpawn || gGuiContext.respawnRequested)) {
				if (gGuiContext.respawnRequested && gGuiContext.hasBedSpawn) {
					ChunkPos bedCp(gGuiContext.bedSpawnX >> 4, gGuiContext.bedSpawnZ >> 4);
					auto* bedChunk = acquireHeldChunk(commandContext.dim->getChunkSource(), bedCp);
					if (bedChunk && bedChunk->getState() == ChunkState::Loaded) {
						bool bedFound = false;
						BlockSource& reg = c.getRegion();
						for (int ox = -2; ox <= 2 && !bedFound; ++ox) {
							for (int oz = -2; oz <= 2 && !bedFound; ++oz) {
								for (int oy = -1; oy <= 1 && !bedFound; ++oy) {
									if (Block::mBed && reg.getBlockID(gGuiContext.bedSpawnX + ox, gGuiContext.bedSpawnY + oy, gGuiContext.bedSpawnZ + oz) == Block::mBed->mID) {
										bedFound = true;
									}
								}
							}
						}
						if (bedFound) {
							c.setPositionAndLook(gGuiContext.bedSpawnX + 0.5f,
							                     static_cast<float>(gGuiContext.bedSpawnY) + 0.05f,
							                     gGuiContext.bedSpawnZ + 0.5f, c.yaw, 0.0);
						} else {
							gGuiContext.hasBedSpawn = false;
							c.spawnAtSurface();
						}
						hasInitialSpawn = true;
						gGuiContext.respawnRequested = false;
					}
				} else {
					auto* spawnChunk = acquireHeldChunk(commandContext.dim->getChunkSource(), ChunkPos(0, 0));
					if (spawnChunk && spawnChunk->getState() == ChunkState::Loaded) {
						c.spawnAtSurface();
						hasInitialSpawn = true;
						gGuiContext.respawnRequested = false;
					}
				}
			}

			auto triggerPlayerDeath = [&]() {
				if (gGuiContext.currentScreen) return;
				if (!gGuiContext.isCreative) {
					static Random deathDropRng;
					Vec3 deathPos(c.mPos.x, c.mBB.min.y + 0.5f, c.mPos.z);
					auto dropOnDeath = [&](ItemInstance& slot) {
						if (slot.isNull() || slot.getStackSize() <= 0) return;
						Vec3 vel((deathDropRng.nextFloat() - 0.5f) * 4.5f,
						         2.8f + deathDropRng.nextFloat() * 1.5f,
						         (deathDropRng.nextFloat() - 0.5f) * 4.5f);
						spawnWorldItem(worldItems, slot, deathPos, vel, 0.5f);
						slot.setNull();
					};
					for (auto& s : gGuiContext.inventorySlots) dropOnDeath(s);
					for (auto& a : gGuiContext.armorSlots) dropOnDeath(a);
				}
				gGuiContext.setScreen(new DeathScreen());
			};

			auto damagePlayer = [&](int rawDmg, const Vec3& knockDir) {
				if (gGuiContext.isCreative || gGuiContext.health <= 0) return;
				int dealt = gGuiContext.applyDamage(rawDmg, false);
				if (dealt > 0) {
					SoundEngine::play(SoundEngine::SND_HURT, 0.9f);
					c.mHurtTime = 10;
					c.mHurtDuration = 10;
					float kLen = std::sqrt(knockDir.x * knockDir.x + knockDir.z * knockDir.z);
					if (kLen > 0.0001f) {
						// Exact MCPE 0.8.1 Mob::knockback (Mob.cpp L792-814):
						// knockDir is toward player (posX - a2->posX), so +0.4f along knockDir/kLen
						c.mPosDelta.x = (c.mPosDelta.x * 0.5f) + (knockDir.x / kLen) * 0.4f;
						c.mPosDelta.y = std::min(0.4f, (c.mPosDelta.y * 0.5f) + 0.4f);
						c.mPosDelta.z = (c.mPosDelta.z * 0.5f) + (knockDir.z / kLen) * 0.4f;
						c.mHurtDir = (std::atan2(-knockDir.z, -knockDir.x) * (180.0f / 3.14159265f)) - static_cast<float>(c.yaw);
					} else {
						c.mHurtDir = static_cast<float>((std::rand() % 2) * 180);
					}
				}
				if (gGuiContext.health <= 0) {
					triggerPlayerDeath();
				}
			};

			// Drain drops created outside inventory UI
			if (!gGuiContext.pendingWorldDrops.empty()) {
				for (const auto& dropItem : gGuiContext.pendingWorldDrops) {
					Vec3 spawnPos(c.mPos.x + c.cameraFront.x * 0.55f,
					              c.mPos.y - 0.3f + c.cameraFront.y * 0.55f,
					              c.mPos.z + c.cameraFront.z * 0.55f);
					Vec3 spawnVel(c.cameraFront.x * 5.5f,
					              c.cameraFront.y * 5.5f + 1.5f,
					              c.cameraFront.z * 5.5f);
					spawnWorldItem(worldItems, dropItem, spawnPos, spawnVel, 1.2f);
				}
				gGuiContext.pendingWorldDrops.clear();
			}

			HitResult targetHit;
			bool hasTargetHit = false;
			bool mobInFront = false;
			syncCursorLock();
			if (gGuiContext.state == GameState::InGame && !isPausedByScreen) {
				c.UpdateCam(deltaTime);

				float survDt = std::min(std::max(deltaTime, 0.0001f), 0.1f);
				if (eatCooldown > 0.0f) eatCooldown = std::max(0.0f, eatCooldown - survDt);
				if (gItemUseSlowTimer > 0.0f) gItemUseSlowTimer = std::max(0.0f, gItemUseSlowTimer - survDt);
				if (hazardDamageCooldown > 0.0f) hazardDamageCooldown = std::max(0.0f, hazardDamageCooldown - survDt);

				if (!gGuiContext.isCreative && gGuiContext.health > 0) {
					while (gGuiContext.exhaustion >= 4.0f) {
						gGuiContext.exhaustion -= 4.0f;
						if (gGuiContext.saturation > 0.0f) {
							gGuiContext.saturation = std::max(0.0f, gGuiContext.saturation - 1.0f);
						} else if (gGuiContext.hunger > 0) {
							gGuiContext.hunger = std::max(0, gGuiContext.hunger - 1);
						}
					}
					if (gGuiContext.hunger >= 18 && gGuiContext.health < 20) {
						gGuiContext.healTimer += survDt;
						if (gGuiContext.healTimer >= 3.5f) {
							gGuiContext.healTimer = 0.0f;
							gGuiContext.health = std::min(20, gGuiContext.health + 1);
							gGuiContext.exhaustion += 3.0f;
						}
					} else {
						gGuiContext.healTimer = 0.0f;
					}
					if (gGuiContext.hunger <= 0 && gGuiContext.health > 1) {
						gGuiContext.starveTimer += survDt;
						if (gGuiContext.starveTimer >= 4.0f) {
							gGuiContext.starveTimer = 0.0f;
							gGuiContext.applyDamage(1, true);
						}
					} else {
						gGuiContext.starveTimer = 0.0f;
					}

					{
						BlockSource& region = c.getRegion();
						int fx = Math::floor(c.mPos.x);
						int fy0 = Math::floor(c.mBB.min.y + 0.1f);
						int fy1 = Math::floor(c.mBB.min.y + 1.0f);
						int fz = Math::floor(c.mPos.z);
						int hazDmg = 0;
						bool touchingFire = false;
						bool touchingLava = false;
						bool touchingWater = false;
						for (int fy = fy0; fy <= fy1; ++fy) {
							for (int ox = -1; ox <= 1; ++ox) {
								for (int oz = -1; oz <= 1; ++oz) {
									BlockPos bp(fx + ox, fy, fz + oz);
									BlockID bid = region.getBlockID(bp);
									const Block* b = bid > 0 ? Block::mBlocks[bid] : nullptr;
									if (!b) continue;
									AABB blkBB(static_cast<float>(bp.x), static_cast<float>(bp.y), static_cast<float>(bp.z),
									           static_cast<float>(bp.x + 1), static_cast<float>(bp.y + 1), static_cast<float>(bp.z + 1));
									if (b->getMaterial().isType(MaterialType::Water) && blkBB.intersects(c.mBB)) {
										touchingWater = true;
									} else if (b->getMaterial().isType(MaterialType::Lava) && blkBB.intersects(c.mBB)) {
										touchingLava = true;
										hazDmg = std::max(hazDmg, 4);
									} else if (b == Block::mFire && blkBB.expanded(Vec3(0.05f, 0.05f, 0.05f)).intersects(c.mBB)) {
										touchingFire = true;
										hazDmg = std::max(hazDmg, 1);
									} else if (b == Block::mCactus && blkBB.expanded(Vec3(0.05f, 0.05f, 0.05f)).intersects(c.mBB)) {
										hazDmg = std::max(hazDmg, 1);
									}
								}
							}
						}
						if (touchingFire) gPlayerFireTicks = std::max(gPlayerFireTicks, 300);
						if (touchingLava) gPlayerFireTicks = std::max(gPlayerFireTicks, 600);
						if (touchingWater && gPlayerFireTicks > 0) {
							gPlayerFireTicks = 0;
							SoundEngine::play(SoundEngine::SND_EXPLODE, 0.25f, 1.8f);
							ParticleSystem::spawnSmoke(c.mPos.x, c.mBB.min.y + 0.8f, c.mPos.z, 0.0f, 0.05f, 0.0f, 100, &region);
						}
						if (hazardDamageCooldown <= 0.0f && hazDmg > 0) {
							damagePlayer(hazDmg, Vec3::ZERO);
							hazardDamageCooldown = 0.5f;
						}
					}
				}
				if (gGuiContext.pendingTeleport) {
					c.teleportTo(gGuiContext.pendingTpX, gGuiContext.pendingTpY, gGuiContext.pendingTpZ);
					gGuiContext.pendingTeleport = false;
				}
				if (!gGuiContext.isCreative && gGuiContext.health <= 0 && !gGuiContext.currentScreen) {
					triggerPlayerDeath();
				}

				if (glinput.NewKeyDown(GLFW_KEY_Q)) {
					gGuiContext.ensureInventoryInitialized();
					int slotIdx = std::max(0, std::min(8, gGuiContext.selectedHotbarSlot));
					ItemInstance& held = gGuiContext.inventorySlots[slotIdx];
					if (!held.isNull() && held.getStackSize() > 0) {
						bool dropAll = glinput.KeyDown(GLFW_KEY_LEFT_CONTROL) || glinput.KeyDown(GLFW_KEY_RIGHT_CONTROL);
						int dropAmt = dropAll ? static_cast<int>(held.getStackSize()) : 1;
						ItemInstance dropStack(held);
						dropStack.set(dropAmt);
						Vec3 spawnPos(c.mPos.x + c.cameraFront.x * 0.55f,
						              c.mPos.y - 0.3f + c.cameraFront.y * 0.55f,
						              c.mPos.z + c.cameraFront.z * 0.55f);
						Vec3 spawnVel(c.cameraFront.x * 6.0f,
						              c.cameraFront.y * 6.0f + 1.5f,
						              c.cameraFront.z * 6.0f);
						spawnWorldItem(worldItems, dropStack, spawnPos, spawnVel, 1.2f);
						held.remove(dropAmt);
						if (held.getStackSize() <= 0) {
							held.setNull();
						}
					}
				}

				const float reach = 5.0f;
				Vec3 rayStart(c.mPos.x, c.mPos.y, c.mPos.z);
				Vec3 rayEnd(c.mPos.x + c.cameraFront.x * reach,
				            c.mPos.y + c.cameraFront.y * reach,
				            c.mPos.z + c.cameraFront.z * reach);
				BlockSource& region = c.getRegion();
				static Vec3 sLastRayStart(1e9f, 1e9f, 1e9f);
				static Vec3 sLastRayEnd(1e9f, 1e9f, 1e9f);
				static HitResult sCachedTargetHit;
				if (rayStart.x != sLastRayStart.x || rayStart.y != sLastRayStart.y || rayStart.z != sLastRayStart.z ||
				    rayEnd.x != sLastRayEnd.x || rayEnd.y != sLastRayEnd.y || rayEnd.z != sLastRayEnd.z ||
				    glinput.MouseDown(GLFW_MOUSE_BUTTON_LEFT) || glinput.MouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
					sCachedTargetHit = region.clip(rayStart, rayEnd, false, false, 32, false);
					sLastRayStart = rayStart;
					sLastRayEnd = rayEnd;
				}
				targetHit = sCachedTargetHit;
				hasTargetHit = targetHit.isHit() && targetHit.isTile();

				gGuiContext.ensureInventoryInitialized();
				int heldSlotIdx = std::max(0, std::min(8, gGuiContext.selectedHotbarSlot));
				ItemInstance& held = gGuiContext.inventorySlots[heldSlotIdx];

				float mobHitDist = 999.0f;
				int targetMobIdx = mobManager.raycastMob(
					rayStart,
					Vec3(c.cameraFront.x, c.cameraFront.y, c.cameraFront.z),
					3.8f,
					mobHitDist);
				float blockHitDist = hasTargetHit ? (targetHit.getPos() - rayStart).length() : 999.0f;
				mobInFront = (targetMobIdx >= 0 && (!hasTargetHit || mobHitDist <= blockHitDist));

				bool attackedMobThisClick = false;
				if (glinput.NewMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
					c.triggerSwing();
					if (mobInFront) {
						ToolInfo ti = getToolInfo(held);
						int atkDmg = ti.attackDamage;
						if (!c.mOnGround && c.mPosDelta.y < -0.04f) {
							atkDmg = static_cast<int>(std::ceil(static_cast<float>(atkDmg) * 1.5f));
						}
						static Random mobDropRng;
						bool landed = mobManager.damageMob(
							targetMobIdx,
							atkDmg,
							Vec3(c.cameraFront.x, 0.0f, c.cameraFront.z),
							[&](const ItemInstance& dropItem, const Vec3& dropPos) {
								Vec3 vel((mobDropRng.nextFloat() - 0.5f) * 2.4f,
								         3.2f + mobDropRng.nextFloat() * 1.0f,
								         (mobDropRng.nextFloat() - 0.5f) * 2.4f);
								spawnWorldItem(worldItems, dropItem, dropPos, vel, 0.25f);
							});
						if (landed) {
							SoundEngine::play(SoundEngine::SND_HURT, 0.8f, 0.9f + (std::rand() % 20) * 0.01f);
						}
						if (landed && !gGuiContext.isCreative) {
							gGuiContext.exhaustion += 0.3f;
							int maxDur = getItemMaxDurability(held);
							if (maxDur > 0 && !held.isNull()) {
								int wear = (ti.type == ToolType::Sword) ? 1 : 2;
								int nextAux = static_cast<int>(held.getAuxValue()) + wear;
								if (nextAux > maxDur) {
									held.setNull();
								} else {
									held.setAuxValue(static_cast<short>(nextAux));
								}
							}
						}
						attackedMobThisClick = true;
						isMining = false;
						miningProgress = 0.0f;
					}
				}

				auto performBreakBlock = [&](const BlockPos& hitPos, const FullBlock& fb, const Block* block, bool canHarvest) {
					BlockPos bedSecondPos;
					bool hasBedSecond = (Block::mBed && block == Block::mBed &&
					                     Block::mBed->getSecondPart(region, hitPos, bedSecondPos) &&
					                     region.getBlockID(bedSecondPos) == Block::mBed->mID);
					BlockPos doorSecondPos;
					bool hasDoorSecond = false;
					if (block->hasProperty(BlockProperty::Door)) {
						bool isUpper = block->getBlockState(BlockState::UpperBlockBit).getBool(fb.data);
						doorSecondPos = isUpper ? hitPos.below() : hitPos.above();
						hasDoorSecond = (region.getBlockID(doorSecondPos) == fb.id);
					}

					int blockTintCol = (block == Block::mGrass) ? 0xFFFFFF : block->getColor(region, hitPos, fb.data);
					if (region.removeBlock(hitPos)) {
						SoundEngine::play(SoundEngine::SND_DIG, 0.85f, 0.9f + (std::rand() % 20) * 0.01f);
						float pr = ((blockTintCol >> 16) & 0xFF) / 255.0f;
						float pg = ((blockTintCol >> 8) & 0xFF) / 255.0f;
						float pb = (blockTintCol & 0xFF) / 255.0f;
						float tu0 = 0.0f, tv0 = 0.0f, tu1 = 0.0f, tv1 = 0.0f;
						bool hasTex = false;
						if (const BlockGraphics* bg = (fb.id > 0 && fb.id < 256) ? BlockGraphics::mBlocks[fb.id] : nullptr) {
							const TextureUVCoordinateSet& uv = bg->getTexture(2, fb.data);
							tu0 = uv._u0; tv0 = uv._v0; tu1 = uv._u1; tv1 = uv._v1;
							hasTex = true;
						}
						if (!hasTex && blockTintCol == 0xFFFFFF) {
							if (block == Block::mDirt || block == Block::mLog || block == Block::mWoodPlanks) {
								pr = 0.55f; pg = 0.38f; pb = 0.22f;
							} else if (block == Block::mSand || block == Block::mSandStone) {
								pr = 0.85f; pg = 0.80f; pb = 0.55f;
							} else if (block == Block::mTNT) {
								pr = 0.80f; pg = 0.20f; pb = 0.15f;
							} else {
								pr = 0.55f; pg = 0.55f; pb = 0.55f;
							}
						}
						ParticleSystem::spawnBlockBreak(static_cast<float>(hitPos.x), static_cast<float>(hitPos.y), static_cast<float>(hitPos.z), pr, pg, pb, &region, tu0, tv0, tu1, tv1, hasTex);
						if (hasBedSecond) {
							region.removeBlock(bedSecondPos);
						}
						if (hasDoorSecond) {
							region.removeBlock(doorSecondPos);
						}
						if (block == Block::mIce && (region.getMaterial(hitPos.below()).getBlocksMotion() || region.getMaterial(hitPos.below()).isLiquid())) {
							region.setBlockAndData(hitPos, FullBlock(Block::mFlowingWater->mID, 0), Block::UPDATE_ALL);
						}
						BlockCoordKey bkey{ hitPos.x, hitPos.y, hitPos.z };
						auto dropContainerItem = [&](const ItemInstance& ci) {
							if (ci.isNull() || ci.getStackSize() <= 0) return;
							block->popResource(region, hitPos, ci);
						};
						auto chestIt = gGuiContext.chests.find(bkey);
						if (chestIt != gGuiContext.chests.end()) {
							for (const auto& s : chestIt->second.slots) dropContainerItem(s);
							gGuiContext.chests.erase(chestIt);
						}
						auto furnIt = gGuiContext.furnaces.find(bkey);
						if (furnIt != gGuiContext.furnaces.end()) {
							dropContainerItem(furnIt->second.input);
							dropContainerItem(furnIt->second.fuel);
							dropContainerItem(furnIt->second.output);
							gGuiContext.furnaces.erase(furnIt);
						}
						bool holdingShears = (Item::mShears && !held.isNull() && held.getItem() == Item::mShears);
						if (!gGuiContext.isCreative && holdingShears &&
						    (block == Block::mLeaves || block == Block::mLeaves2 || block == Block::mTallgrass ||
						     block == Block::mDeadBush || block == Block::mWeb)) {
							DataID shearAux = (block == Block::mLeaves || block == Block::mLeaves2) ? (fb.data & 3) : fb.data;
							block->popResource(region, hitPos, ItemInstance(block, 1, shearAux));
						} else if (!gGuiContext.isCreative && block == Block::mTopSnow && canHarvest && Item::mSnowBall) {
							block->popResource(region, hitPos, ItemInstance(Item::mSnowBall, 1 + (fb.data & 7) / 2, 0));
						} else if (!gGuiContext.isCreative && canHarvest) {
							block->spawnResources(region, hitPos, fb.data, 1.0f, 0);
						}
						if (!gGuiContext.isCreative && block->getDestroySpeed() > 0.0f) {
							gGuiContext.exhaustion += 0.025f;
							int maxDur = getItemMaxDurability(held);
							if (maxDur > 0 && !held.isNull()) {
								ToolInfo ti = getToolInfo(held);
								int wear = (ti.type == ToolType::Sword) ? 2 : 1;
								int nextAux = static_cast<int>(held.getAuxValue()) + wear;
								if (nextAux > maxDur) {
									held.setNull();
								} else {
									held.setAuxValue(static_cast<short>(nextAux));
								}
							}
						}
						updateLightingAndRebuildChunks(region, hitPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
						if (hasBedSecond) {
							rebuildChunksForBlock(bedSecondPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
						}
						targetHit = region.clip(rayStart, rayEnd, false, false, 200, false);
						hasTargetHit = targetHit.isHit() && targetHit.isTile();
					}
				};

				if (!attackedMobThisClick && glinput.NewMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
					int pIdx = PaintingSystem::raycastPainting(
						Vec3(c.mPos.x, c.mPos.y, c.mPos.z),
						Vec3(c.cameraFront.x, c.cameraFront.y, c.cameraFront.z),
						hasTargetHit ? blockHitDist : 5.0f);
					if (pIdx >= 0) {
						auto& paintings = PaintingSystem::getPaintings();
						if (!gGuiContext.isCreative && Item::mPainting) {
							const auto& pe = paintings[static_cast<size_t>(pIdx)];
							spawnWorldItem(worldItems, ItemInstance(Item::mPainting, 1, 0), Vec3(pe.x, pe.y, pe.z), Vec3(0, 2.0f, 0), 0.25f);
						}
						paintings.erase(paintings.begin() + pIdx);
						c.triggerSwing();
						attackedMobThisClick = true;
					}
				}

				if (!attackedMobThisClick && !mobInFront && hasTargetHit && glinput.MouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
					BlockPos hitPos = targetHit.getBlock();
					FullBlock fb = region.getBlockAndData(hitPos);
					const Block* block = fb.id > 0 ? Block::mBlocks[fb.id] : nullptr;
					if (gGuiContext.isCreative) {
						if (glinput.NewMouseDown(GLFW_MOUSE_BUTTON_LEFT) && block) {
							performBreakBlock(hitPos, fb, block, false);
						}
						isMining = false;
						miningProgress = 0.0f;
					} else {
						MiningTargetInfo mineInfo = evaluateBlockMining(block, held);
						if (block && mineInfo.canBreak) {
							if (!isMining || !(miningPos == hitPos)) {
								isMining = true;
								miningPos = hitPos;
								miningProgress = 0.0f;
							}
							if (c.mSwingProgress <= 0.0f) {
								c.triggerSwing();
							}
							if (mineInfo.breakTime <= 0.0f) {
								if (glinput.NewMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
									performBreakBlock(hitPos, fb, block, mineInfo.canHarvest);
									miningProgress = 0.0f;
								}
							} else {
								static float crackParticleAccum = 0.0f;
								crackParticleAccum += survDt;
								if (crackParticleAccum >= 0.09f) {
									crackParticleAccum = 0.0f;
									int blockTintCol = (block == Block::mGrass && targetHit.getFacing() != 1) ? 0xFFFFFF : block->getColor(region, hitPos, fb.data);
									float pr = ((blockTintCol >> 16) & 0xFF) / 255.0f;
									float pg = ((blockTintCol >> 8) & 0xFF) / 255.0f;
									float pb = (blockTintCol & 0xFF) / 255.0f;
									float tu0 = 0.0f, tv0 = 0.0f, tu1 = 0.0f, tv1 = 0.0f;
									bool hasTex = false;
									if (const BlockGraphics* bg = (fb.id > 0 && fb.id < 256) ? BlockGraphics::mBlocks[fb.id] : nullptr) {
										const TextureUVCoordinateSet& uv = bg->getTexture(static_cast<FacingID>(targetHit.getFacing()), fb.data);
										tu0 = uv._u0; tv0 = uv._v0; tu1 = uv._u1; tv1 = uv._v1;
										hasTex = true;
									}
									if (!hasTex && blockTintCol == 0xFFFFFF) {
										if (block == Block::mDirt || block == Block::mLog || block == Block::mWoodPlanks) {
											pr = 0.55f; pg = 0.38f; pb = 0.22f;
										} else if (block == Block::mSand || block == Block::mSandStone) {
											pr = 0.85f; pg = 0.80f; pb = 0.55f;
										} else if (block == Block::mTNT) {
											pr = 0.80f; pg = 0.20f; pb = 0.15f;
										} else {
											pr = 0.55f; pg = 0.55f; pb = 0.55f;
										}
									}
									ParticleSystem::spawnBlockCrack(static_cast<float>(hitPos.x), static_cast<float>(hitPos.y), static_cast<float>(hitPos.z), targetHit.getFacing(), pr, pg, pb, &region, tu0, tv0, tu1, tv1, hasTex);
								}
								miningProgress += survDt / mineInfo.breakTime;
								if (miningProgress >= 1.0f) {
									performBreakBlock(hitPos, fb, block, mineInfo.canHarvest);
									isMining = false;
									miningProgress = 0.0f;
								}
							}
						} else {
							isMining = false;
							miningProgress = 0.0f;
						}
					}
				} else {
					isMining = false;
					miningProgress = 0.0f;
				}

				// Bow pull & release
				{
					bool holdingBow = (Item::mBow && !held.isNull() && held.getItem() == Item::mBow);
					bool rightHeld = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
					auto findArrowSlot = [&]() -> int {
						if (!Item::mArrow) return -1;
						for (size_t si = 0; si < gGuiContext.inventorySlots.size(); ++si) {
							const ItemInstance& sItem = gGuiContext.inventorySlots[si];
							if (!sItem.isNull() && sItem.getItem() == Item::mArrow && sItem.getStackSize() > 0) {
								return static_cast<int>(si);
							}
						}
						return -1;
					};
					if (holdingBow && rightHeld && (gGuiContext.isCreative || findArrowSlot() >= 0)) {
						gBowPulling = true;
						gBowPullTime += deltaTime;
					} else if (gBowPulling) {
						float t = gBowPullTime;
						float launchPower = std::min(1.0f, (t * t + t * 2.0f) / 3.0f);
						if (holdingBow && launchPower >= 0.1f) {
							int arrowSlot = findArrowSlot();
							if (gGuiContext.isCreative || arrowSlot >= 0) {
								float speedPerTick = launchPower * 3.0f; // MCPE 0.8.1: a4 * 1.5 where a4 = LaunchPower * 2.0
								float speed = speedPerTick * 20.0f;
								int dmg = static_cast<int>(std::ceil(speedPerTick * 2.0f));
								if (launchPower == 1.0f) {
									dmg += (std::rand() % (dmg / 2 + 2));
								}
								// Match MCPE 0.8.1 Arrow(Level*, Mob*, float) spawn at eye height - 0.10, right hand 0.16
								glm::vec3 rightVec = glm::normalize(glm::cross(c.cameraFront, c.cameraUp));
								Vec3 spawnPos(c.mPos.x + rightVec.x * 0.16f + c.cameraFront.x * 0.20f,
								              c.mPos.y - 0.10f + c.cameraFront.y * 0.20f,
								              c.mPos.z + rightVec.z * 0.16f + c.cameraFront.z * 0.20f);
								ProjectileTntSystem::spawnArrow(spawnPos, Vec3(c.cameraFront.x, c.cameraFront.y, c.cameraFront.z), speed, dmg, true);
								SoundEngine::play(SoundEngine::SND_BOW, 1.0f, (1.0f / (((std::rand() % 100) / 100.0f) * 0.4f + 1.2f)) + launchPower * 0.5f);
								if (!gGuiContext.isCreative && arrowSlot >= 0) {
									ItemInstance& aSlot = gGuiContext.inventorySlots[static_cast<size_t>(arrowSlot)];
									aSlot.remove(1);
									if (aSlot.getStackSize() <= 0) aSlot.setNull();
									int nextAux = static_cast<int>(held.getAuxValue()) + 1;
									if (nextAux > 384) held.setNull();
									else held.setAuxValue(static_cast<short>(nextAux));
								}
							}
						}
						gBowPulling = false;
						gBowPullTime = 0.0f;
					}
				}

				if (glinput.NewMouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
					bool handledRightClick = false;
					if (mobInFront && !held.isNull() && held.getStackSize() > 0) {
						if (Item::mShears && held.getItem() == Item::mShears) {
							static Random shearRng;
							if (mobManager.shearSheep(targetMobIdx, [&](const ItemInstance& dropItem, const Vec3& dropPos) {
								    Vec3 dvel((shearRng.nextFloat() - 0.5f) * 2.4f, 2.8f + shearRng.nextFloat() * 0.8f, (shearRng.nextFloat() - 0.5f) * 2.4f);
								    spawnWorldItem(worldItems, dropItem, dropPos, dvel, 0.25f);
							    })) {
								SoundEngine::play(SoundEngine::SND_DIG, 0.8f, 1.2f);
								c.triggerSwing();
								if (!gGuiContext.isCreative) {
									int maxDur = getItemMaxDurability(held);
									if (maxDur > 0) {
										int nextAux = static_cast<int>(held.getAuxValue()) + 1;
										if (nextAux > maxDur) held.setNull();
										else held.setAuxValue(static_cast<short>(nextAux));
									}
								}
								handledRightClick = true;
							}
						} else if (Item::mDye_powder && held.getItem() == Item::mDye_powder) {
							if (mobManager.dyeSheep(targetMobIdx, (~held.getAuxValue()) & 0xF)) {
								c.triggerSwing();
								if (!gGuiContext.isCreative) {
									held.remove(1);
									if (held.getStackSize() <= 0) held.setNull();
								}
								handledRightClick = true;
							}
						} else if (Item::mBucket && held.getItem() == Item::mBucket && held.getAuxValue() == 0 && !gGuiContext.isCreative) {
							// MCPE 0.8.1 Cow::interactWithPlayer: fill empty bucket with milk (aux = 1)
							if (mobManager.milkCow(targetMobIdx)) {
								SoundEngine::play(SoundEngine::SND_EAT, 0.7f, 1.1f);
								c.triggerSwing();
								ItemInstance milkBucket(Item::mBucket, 1, 1);
								if (held.getStackSize() <= 1) {
									held = milkBucket;
								} else {
									held.remove(1);
									gGuiContext.addItemToInventory(milkBucket);
								}
								handledRightClick = true;
							}
						}
						if (!handledRightClick) {
							// MCPE 0.8.1 Animal::interactWithPlayer: feed animal -> 7 PT_HEART particles & love mode
							if (mobManager.feedAnimal(targetMobIdx, held, [&](const Vec3& pos, float hw, float h, int count) {
								    for (int k = 0; k < count; ++k) {
									    float hx = pos.x + (ParticleSystem::rand01() * 2.0f - 1.0f) * hw;
									    float hy = pos.y + 0.5f + ParticleSystem::rand01() * h;
									    float hz = pos.z + (ParticleSystem::rand01() * 2.0f - 1.0f) * hw;
									    ParticleSystem::spawnHeart(hx, hy, hz,
									                               ParticleSystem::randSigned() * 0.02f,
									                               ParticleSystem::randSigned() * 0.02f,
									                               ParticleSystem::randSigned() * 0.02f,
									                               &c.getRegion());
								    }
							    })) {
								SoundEngine::play(SoundEngine::SND_EAT, 0.65f, 1.15f);
								c.triggerSwing();
								if (!gGuiContext.isCreative) {
									held.remove(1);
									if (held.getStackSize() <= 0) held.setNull();
								}
								handledRightClick = true;
							}
						}
					}
					if (!handledRightClick && hasTargetHit && !c.isSneaking()) {
						BlockPos hitPos = targetHit.getBlock();
						FullBlock fb = region.getBlockAndData(hitPos);
						const Block* hitBlk = fb.id > 0 ? Block::mBlocks[fb.id] : nullptr;
						if (Block::mTNT && fb.id == Block::mTNT->mID &&
						    Item::mFlintAndSteel && !held.isNull() && held.getItem() == Item::mFlintAndSteel) {
							region.removeBlock(hitPos);
							rebuildChunksForBlock(hitPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
							ProjectileTntSystem::spawnPrimedTnt(hitPos.x + 0.5f, static_cast<float>(hitPos.y), hitPos.z + 0.5f);
							if (!gGuiContext.isCreative) {
								int nextAux = static_cast<int>(held.getAuxValue()) + 1;
								if (nextAux > 64) held.setNull();
								else held.setAuxValue(static_cast<short>(nextAux));
							}
							c.triggerSwing();
							handledRightClick = true;
						} else if (Block::mCake && fb.id == Block::mCake->mID) {
							if (!gGuiContext.isCreative && (gGuiContext.health < 20 || gGuiContext.hunger < 20)) {
								gGuiContext.health = std::min(20, gGuiContext.health + 2);
								gGuiContext.hunger = std::min(20, gGuiContext.hunger + 2);
								int bites = Block::mCake->getBlockState(BlockState::BiteCounter).get<int>(fb.data) + 1;
								if (bites > 5) {
									region.removeBlock(hitPos);
								} else {
									DataID nd = fb.data;
									Block::mCake->getBlockState(BlockState::BiteCounter).set(nd, bites);
									region.setBlockAndData(hitPos, FullBlock(fb.id, nd), Block::UPDATE_ALL);
								}
								rebuildChunksForBlock(hitPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
								SoundEngine::play(SoundEngine::SND_EAT, 0.8f);
								c.triggerSwing();
								handledRightClick = true;
							}
						} else if (Block::mNetherReactor && fb.id == Block::mNetherReactor->mID) {
							std::string reactorMsg;
							if (NetherReactorBlock::activateReactor(region, hitPos, Vec3(c.mPos.x, c.mPos.y - c.mHeightOffset, c.mPos.z), gGuiContext.isCreative, reactorMsg)) {
								updateLightingAndRebuildChunks(region, hitPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
							}
							if (!reactorMsg.empty()) {
								gGuiContext.addChatMessage(reactorMsg);
							}
							c.triggerSwing();
							handledRightClick = true;
						} else if (Block::mRedStoneOre && fb.id == Block::mRedStoneOre->mID) {
							region.setBlockAndData(hitPos, FullBlock(Block::mLitRedStoneOre->mID, 0), Block::UPDATE_ALL);
							if (auto* tq = region.getTickQueue(hitPos)) {
								tq->add(region, hitPos, Block::mLitRedStoneOre->mID, 30);
							}
							updateLightingAndRebuildChunks(region, hitPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
						} else if (hitBlk && hitBlk->hasProperty(BlockProperty::Door) && !hitBlk->getMaterial().isType(MaterialType::Metal)) {
							const DoorBlock* door = static_cast<const DoorBlock*>(hitBlk);
							bool wasOpen = door->isOpen(region, hitPos);
							door->setOpen(region, hitPos, !wasOpen, nullptr);
							BlockPos secondPos;
							if (door->getSecondPart(region, hitPos, secondPos)) {
								rebuildChunksForBlock(secondPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
							}
							rebuildChunksForBlock(hitPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
							SoundEngine::play(SoundEngine::SND_DOOR, 0.75f);
							c.triggerSwing();
							handledRightClick = true;
						} else if (hitBlk && hitBlk->hasProperty(BlockProperty::Trap) && !hitBlk->getMaterial().isType(MaterialType::Metal)) {
							const TrapDoorBlock* trap = static_cast<const TrapDoorBlock*>(hitBlk);
							bool wasOpen = hitBlk->getBlockState(BlockState::OpenBit).getBool(fb.data);
							trap->setOpen(region, hitPos, !wasOpen);
							rebuildChunksForBlock(hitPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
							SoundEngine::play(SoundEngine::SND_DOOR, 0.75f);
							c.triggerSwing();
							handledRightClick = true;
						} else if (hitBlk && hitBlk->hasProperty(BlockProperty::FenceGate)) {
							DataID newData = fb.data;
							bool wasOpen = hitBlk->getBlockState(BlockState::OpenBit).getBool(newData);
							if (wasOpen) {
								hitBlk->getBlockState(BlockState::OpenBit).set(newData, false);
							} else {
								int dir = 0;
								if (std::abs(c.cameraMoveFront.z) >= std::abs(c.cameraMoveFront.x)) {
									dir = (c.cameraMoveFront.z >= 0.0f) ? 0 : 2;
								} else {
									dir = (c.cameraMoveFront.x >= 0.0f) ? 3 : 1;
								}
								int curDir = hitBlk->getBlockState(BlockState::Direction).get<int>(newData);
								if (curDir == ((dir + 2) % 4)) {
									hitBlk->getBlockState(BlockState::Direction).set(newData, dir);
								}
								hitBlk->getBlockState(BlockState::OpenBit).set(newData, true);
							}
							region.setBlockAndData(hitPos, FullBlock(fb.id, newData), Block::UPDATE_ALL);
							rebuildChunksForBlock(hitPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
							SoundEngine::play(SoundEngine::SND_DOOR, 0.75f);
							c.triggerSwing();
							handledRightClick = true;
						} else if (openInteractiveBlockScreen(region, hitPos, fb.id, gGuiContext)) {
							c.triggerSwing();
							handledRightClick = true;
						}
					}

					if (!handledRightClick && !held.isNull() && held.getStackSize() > 0 &&
					    Item::mBucket && held.getItem() == Item::mBucket && held.getAuxValue() == 1 &&
					    !gGuiContext.isCreative && eatCooldown <= 0.0f) {
						c.triggerSwing();
						held = ItemInstance(Item::mBucket, 1, 0);
						SoundEngine::play(SoundEngine::SND_EAT, 0.8f);
						eatCooldown = 0.25f;
						handledRightClick = true;
					}

					if (!handledRightClick && !held.isNull() && held.getStackSize() > 0) {
						FoodInfo food = getFoodInfo(held);
						if (food.isFood && !gGuiContext.isCreative && eatCooldown <= 0.0f &&
						    (gGuiContext.hunger < 20 || food.alwaysEdible)) {
							c.triggerSwing();
							gGuiContext.hunger = std::min(20, gGuiContext.hunger + food.nutrition);
							gGuiContext.saturation = std::min(static_cast<float>(gGuiContext.hunger),
							                                  gGuiContext.saturation + food.saturation);
							if (food.healHealth > 0) {
								gGuiContext.health = std::min(20, gGuiContext.health + food.healHealth);
							}
							held.remove(1);
							if (held.getStackSize() <= 0) {
								held.setNull();
							}
							if (food.returnsBowl && Item::mBowl) {
								ItemInstance bowl(Item::mBowl, 1, 0);
								ItemInstance rem = gGuiContext.addItemToInventoryRemainder(bowl);
								if (!rem.isNull() && rem.getStackSize() > 0) {
									gGuiContext.pendingWorldDrops.push_back(rem);
								}
							}
							SoundEngine::play(SoundEngine::SND_EAT, 0.8f);
							eatCooldown = 0.25f;
							gItemUseSlowTimer = 0.35f;
							handledRightClick = true;
						}
					}

					if (!handledRightClick && hasTargetHit && !held.isNull() && held.getStackSize() > 0) {
						const Block* doorBlockToPlace = nullptr;
						int hid = held.getId();
						if (Item::mDoor_wood && hid == Item::mDoor_wood->getId()) doorBlockToPlace = Block::mWoodenDoor;
						else if (Item::mDoor_spruce && hid == Item::mDoor_spruce->getId()) doorBlockToPlace = Block::mWoodenDoorSpruce;
						else if (Item::mDoor_birch && hid == Item::mDoor_birch->getId()) doorBlockToPlace = Block::mWoodenDoorBirch;
						else if (Item::mDoor_jungle && hid == Item::mDoor_jungle->getId()) doorBlockToPlace = Block::mWoodenDoorJungle;
						else if (Item::mDoor_acacia && hid == Item::mDoor_acacia->getId()) doorBlockToPlace = Block::mWoodenDoorAcacia;
						else if (Item::mDoor_darkoak && hid == Item::mDoor_darkoak->getId()) doorBlockToPlace = Block::mWoodenDoorDarkOak;
						else if (Item::mDoor_iron && hid == Item::mDoor_iron->getId()) doorBlockToPlace = Block::mIronDoor;

						if (doorBlockToPlace) {
							BlockPos hitPos = targetHit.getBlock();
							FacingID face = targetHit.getFacing();
							if (face == Facing::UP) {
								BlockPos lowerPos = hitPos.above();
								BlockPos upperPos = lowerPos.above();
								auto isRepl = [&](const BlockPos& bp) {
									if (bp.y < 1 || bp.y >= 127) return false;
									BlockID eid = region.getBlockID(bp);
									const Block* eb = Block::mBlocks[eid];
									return eid == 0 || (eb && eb->getMaterial().isReplaceable());
								};
								if (isRepl(lowerPos) && isRepl(upperPos) && region.isSolidBlockingBlock(hitPos)) {
									int dir = 0;
									if (std::abs(c.cameraMoveFront.z) >= std::abs(c.cameraMoveFront.x)) {
										dir = (c.cameraMoveFront.z >= 0.0f) ? 1 : 3;
									} else {
										dir = (c.cameraMoveFront.x >= 0.0f) ? 0 : 2;
									}
									DataID lowerData = 0;
									doorBlockToPlace->getBlockState(BlockState::Direction).set(lowerData, dir);
									DataID upperData = 0;
									doorBlockToPlace->getBlockState(BlockState::UpperBlockBit).set(upperData, 1);
									if (region.setBlockAndData(lowerPos, FullBlock(doorBlockToPlace->mID, lowerData), Block::UPDATE_ALL) &&
									    region.setBlockAndData(upperPos, FullBlock(doorBlockToPlace->mID, upperData), Block::UPDATE_ALL)) {
										SoundEngine::play(SoundEngine::SND_PLACE, 0.75f, 0.95f);
										c.triggerSwing();
										if (!gGuiContext.isCreative) {
											held.remove(1);
											if (held.getStackSize() <= 0) held.setNull();
										}
										updateLightingAndRebuildChunks(region, lowerPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
										rebuildChunksForBlock(upperPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
										handledRightClick = true;
									}
								}
							}
						}
					}

					if (!handledRightClick && hasTargetHit && !held.isNull() && held.getStackSize() > 0 &&
					    Item::mBed && Block::mBed && held.getId() == Item::mBed->getId()) {
						BlockPos hitPos = targetHit.getBlock();
						FacingID face = targetHit.getFacing();
						if (face == Facing::UP) {
							BlockPos footPos = hitPos + Facing::DIRECTION[face];
							int dir = 0;
							if (std::abs(c.cameraMoveFront.z) >= std::abs(c.cameraMoveFront.x)) {
								dir = (c.cameraMoveFront.z >= 0.0f) ? 0 : 2;
							} else {
								dir = (c.cameraMoveFront.x >= 0.0f) ? 3 : 1;
							}
							BlockPos headPos(footPos.x + BedBlock::HEAD_DIRECTION_OFFSETS[dir][0],
							                 footPos.y,
							                 footPos.z + BedBlock::HEAD_DIRECTION_OFFSETS[dir][1]);
							auto isRepl = [&](const BlockPos& bp) {
								if (bp.y < 0 || bp.y >= 128) return false;
								BlockID eid = region.getBlockID(bp);
								const Block* eb = Block::mBlocks[eid];
								return eid == 0 || (eb && eb->getMaterial().isReplaceable());
							};
							if (isRepl(footPos) && isRepl(headPos) &&
							    region.isSolidBlockingBlock(footPos.below()) && region.isSolidBlockingBlock(headPos.below())) {
								AABB footBB(footPos.x, footPos.y, footPos.z, footPos.x + 1.0f, footPos.y + 0.5625f, footPos.z + 1.0f);
								AABB headBB(headPos.x, headPos.y, headPos.z, headPos.x + 1.0f, headPos.y + 0.5625f, headPos.z + 1.0f);
								if (!footBB.intersects(c.mBB) && !headBB.intersects(c.mBB)) {
									DataID footData = 0;
									Block::mBed->getBlockState(BlockState::Direction).set(footData, dir);
									DataID headData = footData;
									Block::mBed->getBlockState(BlockState::HeadPieceBit).set(headData, 1);
									if (region.setBlockAndData(footPos, FullBlock(Block::mBed->mID, footData), Block::UPDATE_ALL) &&
									    region.setBlockAndData(headPos, FullBlock(Block::mBed->mID, headData), Block::UPDATE_ALL)) {
										c.triggerSwing();
										if (!gGuiContext.isCreative) {
											held.remove(1);
											if (held.getStackSize() <= 0) held.setNull();
										}
										updateLightingAndRebuildChunks(region, footPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
										rebuildChunksForBlock(headPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
										handledRightClick = true;
									}
								}
							}
						}
					}

					if (!handledRightClick && hasTargetHit && !held.isNull() && held.getStackSize() > 0 &&
					    Item::mSign && held.getId() == Item::mSign->getId()) {
						BlockPos hitPos = targetHit.getBlock();
						FacingID face = targetHit.getFacing();
						if (face != Facing::DOWN && face < Facing::MAX) {
							BlockPos placePos = hitPos + Facing::DIRECTION[face];
							if (placePos.y >= 0 && placePos.y < 128) {
								BlockID existingId = region.getBlockID(placePos);
								const Block* existingBlock = Block::mBlocks[existingId];
								bool canReplace = (existingId == 0) || (existingBlock && existingBlock->getMaterial().isReplaceable());
								if (canReplace && region.getMaterial(hitPos).isSolid()) {
									const Block* signBlock = (face == Facing::UP) ? Block::mSign : Block::mWallSign;
									if (signBlock) {
										DataID signData = 0;
										if (face == Facing::UP) {
											float deg = static_cast<float>(c.yaw) - 90.0f;
											int rot16 = Math::floor(((deg + 180.0f) * 16.0f / 360.0f) + 0.5f) & 15;
											signData = static_cast<DataID>(rot16);
										} else {
											signBlock->getBlockState(BlockState::FacingDirection).set(signData, face);
										}
										if (region.setBlockAndData(placePos, FullBlock(signBlock->mID, signData), Block::UPDATE_ALL)) {
											SoundEngine::play(SoundEngine::SND_PLACE, 0.75f, 0.95f);
											c.triggerSwing();
											if (!gGuiContext.isCreative) {
												held.remove(1);
												if (held.getStackSize() <= 0) held.setNull();
											}
											updateLightingAndRebuildChunks(region, placePos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
											if (BlockEntity* be = region.getBlockEntity(placePos); be && be->isType(BlockEntityType::Sign)) {
												gGuiContext.setScreen(new TextEditScreen(static_cast<SignBlockEntity*>(be)));
											}
											handledRightClick = true;
										}
									}
								}
							}
						}
					}

					if (!handledRightClick && hasTargetHit && Item::mMobPlacer && !held.isNull() && held.getStackSize() > 0 && held.getItem() == Item::mMobPlacer) {
						BlockPos hitPos = targetHit.getBlock();
						FacingID face = targetHit.getFacing();
						if (face < Facing::MAX) {
							BlockPos spawnBlock = hitPos + Facing::DIRECTION[face];
							float yOff = (face == Facing::UP && Block::mFence && region.getBlockID(hitPos) == Block::mFence->mID) ? 0.5f : 0.0f;
							MobType eggType = MobType::Pig;
							bool validEgg = true;
							switch (held.getAuxValue()) {
								case 10: eggType = MobType::Chicken; break;
								case 11: eggType = MobType::Cow; break;
								case 12: eggType = MobType::Pig; break;
								case 13: eggType = MobType::Sheep; break;
								case 32: eggType = MobType::Zombie; break;
								case 33: eggType = MobType::Creeper; break;
								case 34: eggType = MobType::Skeleton; break;
								case 35: eggType = MobType::Spider; break;
								case 36: eggType = MobType::PigZombie; break;
								default: validEgg = false; break;
							}
							if (validEgg && mobManager.spawnMobAt(eggType, spawnBlock.x + 0.5f, spawnBlock.y + yOff, spawnBlock.z + 0.5f)) {
								if (!gGuiContext.isCreative) {
									held.remove(1);
									if (held.getStackSize() <= 0) held.setNull();
								}
								handledRightClick = true;
							}
						}
					}

					// 0.8.1 Item useOn: Flint & Steel, Bucket, Bonemeal, Hoes, Seeds, Carrot, Potato, Reeds, Cake
					if (!handledRightClick && hasTargetHit && !held.isNull() && held.getStackSize() > 0) {
						BlockPos hitPos = targetHit.getBlock();
						FacingID face = targetHit.getFacing();
						BlockPos placePos = (face < Facing::MAX) ? (hitPos + Facing::DIRECTION[face]) : hitPos;
						int hid = held.getId();
						if (Item::mFlintAndSteel && hid == Item::mFlintAndSteel->getId() && face < Facing::MAX) {
							if (placePos.y >= 1 && placePos.y < 127 && region.getBlockID(placePos) == BlockID::AIR && Block::mFire) {
								region.setBlockAndData(placePos, FullBlock(Block::mFire->mID, 0), Block::UPDATE_ALL);
								SoundEngine::play(SoundEngine::SND_EXPLODE, 0.3f, 1.5f);
								c.triggerSwing();
								if (!gGuiContext.isCreative) {
									int nextAux = static_cast<int>(held.getAuxValue()) + 1;
									if (nextAux > 64) held.setNull();
									else held.setAuxValue(static_cast<short>(nextAux));
								}
								updateLightingAndRebuildChunks(region, placePos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
								handledRightClick = true;
							}
						} else if (Item::mBucket && hid == Item::mBucket->getId()) {
							int baux = held.getAuxValue();
							if (baux == 0 && face < Facing::MAX) {
								BlockID lid = region.getBlockID(placePos);
								int newAux = 0;
								if ((Block::mFlowingWater && lid == Block::mFlowingWater->mID) || (Block::mStillWater && lid == Block::mStillWater->mID)) {
									if (region.getData(placePos) == 0) newAux = 8;
								} else if ((Block::mFlowingLava && lid == Block::mFlowingLava->mID) || (Block::mStillLava && lid == Block::mStillLava->mID)) {
									if (region.getData(placePos) == 0) newAux = 10;
								}
								if (newAux > 0) {
									region.removeBlock(placePos);
									c.triggerSwing();
									if (!gGuiContext.isCreative) {
										ItemInstance filled(Item::mBucket, 1, newAux);
										if (held.getStackSize() <= 1) held = filled;
										else { held.remove(1); gGuiContext.addItemToInventory(filled); }
									}
									updateLightingAndRebuildChunks(region, placePos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
									handledRightClick = true;
								}
							} else if ((baux == 8 || baux == 10) && face < Facing::MAX) {
								BlockID eid = region.getBlockID(placePos);
								const Block* eb = Block::mBlocks[eid];
								if (eid == BlockID::AIR || (eb && eb->getMaterial().isReplaceable())) {
									BlockID placeLiq = (baux == 10) ? Block::mFlowingLava->mID : Block::mFlowingWater->mID;
									region.setBlockAndData(placePos, FullBlock(placeLiq, 0), Block::UPDATE_ALL);
									c.triggerSwing();
									if (!gGuiContext.isCreative) {
										held = ItemInstance(Item::mBucket, 1, 0);
									}
									updateLightingAndRebuildChunks(region, placePos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
									handledRightClick = true;
								}
							}
						} else if (Item::mDye_powder && hid == Item::mDye_powder->getId() && held.getAuxValue() == 15) {
							const Block* hb = Block::mBlocks[region.getBlockID(hitPos)];
							if (hb && hb->onFertilized(region, hitPos, nullptr)) {
								c.triggerSwing();
								if (!gGuiContext.isCreative) {
									held.remove(1);
									if (held.getStackSize() <= 0) held.setNull();
								}
								updateLightingAndRebuildChunks(region, hitPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
								handledRightClick = true;
							}
						} else if (Item::mPainting && hid == Item::mPainting->getId() && face < Facing::MAX) {
							if (PaintingSystem::tryPlacePainting(region, hitPos, face)) {
								SoundEngine::play(SoundEngine::SND_PLACE, 0.75f, 0.95f);
								c.triggerSwing();
								if (!gGuiContext.isCreative) {
									held.remove(1);
									if (held.getStackSize() <= 0) held.setNull();
								}
								handledRightClick = true;
							}
						} else if ((Item::mHoe_wood && hid == Item::mHoe_wood->getId()) ||
						           (Item::mHoe_stone && hid == Item::mHoe_stone->getId()) ||
						           (Item::mHoe_iron && hid == Item::mHoe_iron->getId()) ||
						           (Item::mHoe_diamond && hid == Item::mHoe_diamond->getId()) ||
						           (Item::mHoe_gold && hid == Item::mHoe_gold->getId())) {
							BlockID hbId = region.getBlockID(hitPos);
							if ((hbId == Block::mGrass->mID || hbId == Block::mDirt->mID) &&
							    region.getBlockID(hitPos.above()) == BlockID::AIR && face != Facing::DOWN) {
								region.setBlockAndData(hitPos, FullBlock(Block::mFarmland->mID, 0), Block::UPDATE_ALL);
								SoundEngine::play(SoundEngine::SND_PLACE, 0.75f, 0.9f);
								c.triggerSwing();
								if (hbId == Block::mGrass->mID && (std::rand() % 8) == 0 && !gGuiContext.isCreative) {
									const Item* seed = ((std::rand() % 15) == 0 && Item::mSeeds_beetroot) ? Item::mSeeds_beetroot : Item::mSeeds_wheat;
									if (seed) {
										spawnWorldItem(worldItems, ItemInstance(seed, 1, 0), Vec3(hitPos.x + 0.5f, hitPos.y + 1.1f, hitPos.z + 0.5f), Vec3(0, 2.0f, 0), 0.25f);
									}
								}
								if (!gGuiContext.isCreative) {
									int maxD = getItemMaxDurability(held);
									int nextAux = static_cast<int>(held.getAuxValue()) + 1;
									if (maxD > 0 && nextAux > maxD) held.setNull();
									else held.setAuxValue(static_cast<short>(nextAux));
								}
								rebuildChunksForBlock(hitPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
								handledRightClick = true;
							}
						} else {
							const Block* cropBlock = nullptr;
							bool needsFarmland = true;
							if (Item::mSeeds_wheat && hid == Item::mSeeds_wheat->getId()) cropBlock = Block::mWheatCrop;
							else if (Item::mSeeds_pumpkin && hid == Item::mSeeds_pumpkin->getId()) cropBlock = Block::mPumpkinStem;
							else if (Item::mSeeds_melon && hid == Item::mSeeds_melon->getId()) cropBlock = Block::mMelonStem;
							else if (Item::mSeeds_beetroot && hid == Item::mSeeds_beetroot->getId()) cropBlock = Block::mBeetrootCrop;
							else if (Item::mCarrot && hid == Item::mCarrot->getId()) cropBlock = Block::mCarrotCrop;
							else if (Item::mPotato && hid == Item::mPotato->getId()) cropBlock = Block::mPotatoCrop;
							else if (Item::mReeds && hid == Item::mReeds->getId()) { cropBlock = Block::mReeds; needsFarmland = false; }
							else if (Item::mCake && hid == Item::mCake->getId()) { cropBlock = Block::mCake; needsFarmland = false; }

							if (cropBlock && face < Facing::MAX && region.getBlockID(placePos) == BlockID::AIR) {
								bool ok = needsFarmland ? (face == Facing::UP && region.getBlockID(hitPos) == Block::mFarmland->mID)
								                        : cropBlock->mayPlace(region, placePos, face);
								if (ok && region.setBlockAndData(placePos, FullBlock(cropBlock->mID, 0), Block::UPDATE_ALL)) {
									SoundEngine::play(SoundEngine::SND_PLACE, 0.75f, 0.95f);
									c.triggerSwing();
									if (!gGuiContext.isCreative) {
										held.remove(1);
										if (held.getStackSize() <= 0) held.setNull();
									}
									updateLightingAndRebuildChunks(region, placePos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
									handledRightClick = true;
								}
							}
						}
					}

					// Snowball & Egg throw
					if (!handledRightClick && !held.isNull() && held.getStackSize() > 0 &&
					    ((Item::mSnowBall && held.getItem() == Item::mSnowBall) ||
					     (Item::mEgg && held.getItem() == Item::mEgg))) {
						glm::vec3 rightVec = glm::normalize(glm::cross(c.cameraFront, c.cameraUp));
						Vec3 spawnPos(c.mPos.x + rightVec.x * 0.16f + c.cameraFront.x * 0.25f,
						              c.mPos.y - 0.10f + c.cameraFront.y * 0.25f,
						              c.mPos.z + rightVec.z * 0.16f + c.cameraFront.z * 0.25f);
						ProjectileTntSystem::spawnArrow(spawnPos, Vec3(c.cameraFront.x, c.cameraFront.y, c.cameraFront.z), 30.0f, 0, true);
						SoundEngine::play(SoundEngine::SND_BOW, 0.5f, 1.4f);
						c.triggerSwing();
						if (!gGuiContext.isCreative) {
							held.remove(1);
							if (held.getStackSize() <= 0) held.setNull();
						}
						handledRightClick = true;
					}

					if (!handledRightClick && hasTargetHit && !held.isNull() && held.getStackSize() > 0 && held.isBlock()) {
						BlockPos hitPos = targetHit.getBlock();
						FacingID face = targetHit.getFacing();
						if (face < Facing::MAX) {
							const Block* placeBlock = held.getBlock();
							BlockPos placePos = hitPos + Facing::DIRECTION[face];

							// Merge matching half-slabs into a double slab when clicking the inner face
							if (placeBlock && (placeBlock == Block::mStoneSlab || placeBlock == Block::mWoodenSlab || placeBlock == Block::mStoneSlab2)) {
								FullBlock hitFB = region.getBlockAndData(hitPos);
								if (hitFB.id == placeBlock->mID) {
									int heldVariant = held.getAuxValue() & 0x7;
									int hitVariant = hitFB.data & 0x7;
									bool hitIsBottom = SlabBlock::isBottomSlab(hitFB.data);
									bool canMerge = (heldVariant == hitVariant) &&
										((face == Facing::UP && hitIsBottom) || (face == Facing::DOWN && !hitIsBottom));
									const Block* doubleSlab =
										(placeBlock == Block::mStoneSlab) ? Block::mDoubleStoneSlab :
										(placeBlock == Block::mWoodenSlab) ? Block::mDoubleWoodenSlab :
										Block::mDoubleStoneSlab2;
									if (canMerge && doubleSlab) {
										AABB fullBB(static_cast<float>(hitPos.x), static_cast<float>(hitPos.y), static_cast<float>(hitPos.z),
										            static_cast<float>(hitPos.x + 1), static_cast<float>(hitPos.y + 1), static_cast<float>(hitPos.z + 1));
										if (!fullBB.intersects(c.mBB) &&
										    region.setBlockAndData(hitPos, FullBlock(doubleSlab->mID, static_cast<DataID>(hitVariant)), Block::UPDATE_ALL)) {
											SoundEngine::play(SoundEngine::SND_PLACE, 0.75f, 0.95f + (std::rand() % 15) * 0.01f);
											c.triggerSwing();
											if (!gGuiContext.isCreative) {
												held.remove(1);
												if (held.getStackSize() <= 0) held.setNull();
											}
											updateLightingAndRebuildChunks(region, hitPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
											handledRightClick = true;
										}
									}
								}
							}

							if (!handledRightClick && placeBlock && placePos.y >= 0 && placePos.y < 128) {
								BlockID existingId = region.getBlockID(placePos);
								const Block* existingBlock = Block::mBlocks[existingId];
								bool canReplace = (existingId == 0) || (existingBlock && existingBlock->getMaterial().isReplaceable());
								if (canReplace) {
									AABB tmpBB;
									bool overlapsPlayer = false;
									if (placeBlock->isSolid() || !placeBlock->getAABB(region, placePos, tmpBB).isEmpty()) {
										AABB placeBB(static_cast<float>(placePos.x), static_cast<float>(placePos.y), static_cast<float>(placePos.z),
										             static_cast<float>(placePos.x + 1), static_cast<float>(placePos.y + 1), static_cast<float>(placePos.z + 1));
										overlapsPlayer = placeBB.intersects(c.mBB);
									}
									if (!overlapsPlayer) {
										DataID placeData = static_cast<DataID>(held.getAuxValue() & 0xF);
										bool allowPlace = true;

										// Matches Minecraft Entity::getRotation().y quadrant: 0=+Z, 1=-X, 2=-Z, 3=+X
										int mcDir = 0;
										if (std::abs(c.cameraMoveFront.z) >= std::abs(c.cameraMoveFront.x)) {
											mcDir = (c.cameraMoveFront.z >= 0.0f) ? 0 : 2;
										} else {
											mcDir = (c.cameraMoveFront.x >= 0.0f) ? 3 : 1;
										}
										static const FacingID mcDirToFacing[4] = { Facing::NORTH, Facing::EAST, Facing::SOUTH, Facing::WEST };
										static const FacingID mcDirToOppFacing[4] = { Facing::SOUTH, Facing::WEST, Facing::NORTH, Facing::EAST };
										auto computeFacingAll = [&](bool invert) -> FacingID {
											if (std::abs(static_cast<int>(c.mPos.x) - placePos.x) < 2 &&
											    std::abs(static_cast<int>(c.mPos.z) - placePos.z) < 2) {
												if (c.mBB.min.y > placePos.y) return invert ? Facing::DOWN : Facing::UP;
												if (placePos.y > c.mBB.max.y) return invert ? Facing::UP : Facing::DOWN;
											}
											return invert ? mcDirToOppFacing[mcDir] : mcDirToFacing[mcDir];
										};

										if (placeBlock == Block::mTorch || placeBlock == Block::mLitRedStoneTorch || placeBlock == Block::mUnlitRedStoneTorch) {
											static const DataID torchDataFromFacing[6] = {
												TorchBlock::TORCH_TOP, TorchBlock::TORCH_TOP,
												TorchBlock::TORCH_SOUTH, TorchBlock::TORCH_NORTH,
												TorchBlock::TORCH_EAST, TorchBlock::TORCH_WEST
											};
											if (face != Facing::DOWN && region.canProvideSupport(hitPos, face, BlockSupportType::Center)) {
												placeData = torchDataFromFacing[face];
											} else if (region.canProvideSupport(placePos.below(), Facing::UP, BlockSupportType::Center)) {
												placeData = TorchBlock::TORCH_TOP;
											} else {
												bool foundWall = false;
												for (FacingID f = Facing::NORTH; f <= Facing::EAST; ++f) {
													if (region.canProvideSupport(placePos.neighbor(Facing::OPPOSITE_FACING[f]), f, BlockSupportType::Center)) {
														placeData = torchDataFromFacing[f];
														foundWall = true;
														break;
													}
												}
												if (!foundWall) allowPlace = false;
											}
										} else if (placeBlock == Block::mLadder) {
											placeData = 0;
											if (face >= Facing::NORTH && face <= Facing::EAST && region.isSolidBlockingBlock(hitPos)) {
												placeBlock->getBlockState(BlockState::FacingDirection).set(placeData, face);
											} else {
												bool foundWall = false;
												for (FacingID f = Facing::NORTH; f <= Facing::EAST; ++f) {
													if (region.isSolidBlockingBlock(placePos.neighbor(Facing::OPPOSITE_FACING[f]))) {
														placeBlock->getBlockState(BlockState::FacingDirection).set(placeData, f);
														foundWall = true;
														break;
													}
												}
												if (!foundWall) allowPlace = false;
											}
										} else if (placeBlock == Block::mVine) {
											int vineFacing = 0;
											switch (face) {
											case Facing::NORTH: vineFacing = VineBlock::VINE_SOUTH; break;
											case Facing::SOUTH: vineFacing = VineBlock::VINE_NORTH; break;
											case Facing::WEST:  vineFacing = VineBlock::VINE_EAST; break;
											case Facing::EAST:  vineFacing = VineBlock::VINE_WEST; break;
											default: break;
											}
											placeData = 0;
											placeBlock->getBlockState(BlockState::VineGrowth).set(placeData, vineFacing);
											if (placeData == 0 && !region.isSolidBlockingBlock(placePos.above())) {
												allowPlace = false;
											}
										} else if (placeBlock == Block::mStoneButton || placeBlock == Block::mWoodButton) {
											placeData = 0;
											placeBlock->getBlockState(BlockState::FacingDirection).set(placeData, face);
										} else if (placeBlock == Block::mLever) {
											int ldir = LeverBlock::getLeverFacing(face);
											if (face == Facing::UP) {
												ldir = ((mcDir & 1) == 0) ? 5 : 6;
											} else if (face == Facing::DOWN) {
												ldir = ((mcDir & 1) == 0) ? 7 : 0;
											}
											placeData = 0;
											placeBlock->getBlockState(BlockState::FacingDirection).set(placeData, ldir);
										} else if (placeBlock->hasProperty(BlockProperty::Trap)) {
											static const int trapDirFromMc[4] = { 3, 0, 2, 1 };
											placeData = 0;
											placeBlock->getBlockState(BlockState::Direction).set(placeData, trapDirFromMc[mcDir]);
											bool upsideDown = (face == Facing::DOWN) || (face != Facing::UP && targetHit.getPos().y - hitPos.y > 0.5f);
											placeBlock->getBlockState(BlockState::UpsideDownBit).set(placeData, upsideDown ? 1 : 0);
										} else if (placeBlock->hasProperty(BlockProperty::Stair)) {
											static const int stairDirFromMc[4] = { 2, 1, 3, 0 };
											placeData = 0;
											placeBlock->getBlockState(BlockState::Direction).set(placeData, stairDirFromMc[mcDir]);
											bool upsideDown = (face == Facing::DOWN) || (face != Facing::UP && targetHit.getPos().y - hitPos.y > 0.5f);
											placeBlock->getBlockState(BlockState::UpsideDownBit).set(placeData, upsideDown ? 1 : 0);
										} else if (placeBlock->hasProperty(BlockProperty::FenceGate)) {
											placeData = 0;
											placeBlock->getBlockState(BlockState::Direction).set(placeData, mcDir);
										} else if (placeBlock == Block::mStoneSlab || placeBlock == Block::mWoodenSlab || placeBlock == Block::mStoneSlab2) {
											placeData = static_cast<DataID>(held.getAuxValue() & 0x7);
											bool topSlab = (face == Facing::DOWN) || (face != Facing::UP && targetHit.getPos().y - hitPos.y > 0.5f);
											placeBlock->getBlockState(BlockState::TopSlotBit).set(placeData, topSlab ? 1 : 0);
										} else if (placeBlock == Block::mChest || placeBlock == Block::mTrappedChest || placeBlock == Block::mEnderChest ||
										           placeBlock == Block::mFurnace || placeBlock == Block::mLitFurnace) {
											placeData = 0;
											placeBlock->getBlockState(BlockState::FacingDirection).set(placeData, mcDirToFacing[mcDir]);
										} else if (placeBlock == Block::mPumpkin || placeBlock == Block::mLitPumpkin) {
											placeData = 0;
											placeBlock->getBlockState(BlockState::Direction).set(placeData, (mcDir + 2) & 3);
										} else if (placeBlock == Block::mAnvil) {
											static const int anvilDirFromMc[4] = {
												Direction::WEST, Direction::NORTH, Direction::EAST, Direction::SOUTH
											};
											placeData = 0;
											placeBlock->getBlockState(BlockState::Damage).set(placeData, held.getAuxValue() & 0x3);
											placeBlock->getBlockState(BlockState::Direction).set(placeData, anvilDirFromMc[mcDir]);
										} else if (placeBlock == Block::mLog || placeBlock == Block::mLog2 || placeBlock == Block::mHayBlock ||
										           (placeBlock == Block::mQuartzBlock && (held.getAuxValue() & 0x3) != 0)) {
											int axis = RotatedPillarBlock::FACING_Y;
											if (face == Facing::NORTH || face == Facing::SOUTH) axis = RotatedPillarBlock::FACING_Z;
											else if (face == Facing::EAST || face == Facing::WEST) axis = RotatedPillarBlock::FACING_X;
											placeBlock->getBlockState(BlockState::Direction).set(placeData, axis);
										} else if (placeBlock == Block::mUnpoweredRepeater || placeBlock == Block::mPoweredRepeater ||
										           placeBlock == Block::mUnpoweredComparator || placeBlock == Block::mPoweredComparator) {
											placeData = 0;
											placeBlock->getBlockState(BlockState::Direction).set(placeData, (mcDir + 2) & 3);
										} else if (placeBlock == Block::mHopper) {
											FacingID attached = Facing::OPPOSITE_FACING[face];
											if (attached == Facing::UP) attached = Facing::DOWN;
											placeData = 0;
											placeBlock->getBlockState(BlockState::FacingDirection).set(placeData, attached);
										} else if (placeBlock == Block::mEndRod) {
											placeData = 0;
											placeBlock->getBlockState(BlockState::FacingDirection).set(placeData, face);
										} else if (placeBlock == Block::mDispenser || placeBlock == Block::mDropper) {
											placeData = 0;
											placeBlock->getBlockState(BlockState::FacingDirection).set(placeData, computeFacingAll(false));
										} else if (placeBlock == Block::mPiston || placeBlock == Block::mStickyPiston || placeBlock == Block::mObserver) {
											placeData = 0;
											placeBlock->getBlockState(BlockState::FacingDirection).set(placeData, computeFacingAll(true));
										}
										if (allowPlace && !placeBlock->mayPlace(region, placePos, face)) {
											allowPlace = false;
										}
										if (allowPlace && region.setBlockAndData(placePos, FullBlock(placeBlock->mID, placeData), Block::UPDATE_ALL)) {
											SoundEngine::play(SoundEngine::SND_PLACE, 0.75f, 0.95f + (std::rand() % 15) * 0.01f);
											c.triggerSwing();
											if (!gGuiContext.isCreative) {
												held.remove(1);
												if (held.getStackSize() <= 0) {
													held.setNull();
												}
											}
											updateLightingAndRebuildChunks(region, placePos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
										}
									}
								}
							}
						}
					}
				}
			} else {
				isMining = false;
				miningProgress = 0.0f;
				gBowPulling = false;
				gBowPullTime = 0.0f;
			}

			syncFurnaceBlockStates(c.getRegion(), gGuiContext, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));

			if (!gGuiContext.loadingChunks) {
				float clampedDt = std::min(std::max(deltaTime, 0.0001f), 0.1f);
				tickFurnaces(gGuiContext, clampedDt);
				bool allowPickup = (gGuiContext.state == GameState::InGame && !gGuiContext.currentScreen);
				updateWorldItemEntities(worldItems, worldItemTickAccum, deltaTime, c.getRegion(), c.mBB, gGuiContext, allowPickup);
				SoundEngine::setMasterVolume(gGuiOptions.soundVolume);
				ParticleSystem::tick(clampedDt, &c.getRegion());

				// Wire 1:1 0.8.1 world callbacks (popResource, FallingBlock, NetherReactor, FireSmoke, BlockSourceListener)
				static std::vector<BlockPos> sWorldTickChangedBlocks;
				struct WorldTickBlockListener : public BlockSourceListener {
					void onBlockChanged(BlockSource& source, const BlockPos& pos, FullBlock block, FullBlock oldBlock, int updateFlags, Entity* changer) override {
						if (!gGuiContext.loadingChunks && (updateFlags & Block::UPDATE_CLIENTS)) {
							sWorldTickChangedBlocks.push_back(pos);
						}
					}
				};
				static WorldTickBlockListener sWorldTickListener;
				static BlockSource* sListenerRegion = nullptr;
				if (sListenerRegion != &c.getRegion()) {
					sListenerRegion = &c.getRegion();
					sListenerRegion->addListener(sWorldTickListener);
				}

				static bool sCallbacksWired = false;
				if (!sCallbacksWired) {
					sCallbacksWired = true;
					Block::setPopResourceCallback([&](BlockSource&, const BlockPos& pos, const ItemInstance& item) {
						static Random popRng;
						Vec3 dpos(pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f);
						Vec3 dvel((popRng.nextFloat() - 0.5f) * 2.4f, 3.0f, (popRng.nextFloat() - 0.5f) * 2.4f);
						spawnWorldItem(worldItems, item, dpos, dvel, 0.35f);
					});
					HeavyBlock::setStartFallingCallback([](BlockSource&, const BlockPos& pos, const Block* blk, DataID data, bool creative) {
						sFallingBlocks.push_back({ pos.x + 0.5f, static_cast<float>(pos.y), pos.z + 0.5f, 0.0f, static_cast<float>(pos.y), blk->mID, data, creative });
					});
					FireBlock::setSmokeCallback([&](const Vec3& p) {
						ParticleSystem::spawnSmoke(p.x, p.y, p.z, 0.0f, 0.03f, 0.0f, 250, &c.getRegion());
					});
					NetherReactorBlockEntity::setCallbacks(
						[&](const Vec3& sp) { mobManager.spawnMobAt(MobType::PigZombie, sp.x, sp.y, sp.z); },
						[&](const BlockPos& pos, float rad) { mobManager.removePigZombiesNear(pos, rad); },
						[&](bool night) {
							if (night) gGuiContext.worldTime = 18000.0;
						});
				}

				{
					BlockPos sp = c.getLevel().getSharedSpawnPos();
					GuiComponent::setCompassClockState(c.mPos.x, c.mPos.z, static_cast<float>(c.yaw),
					                                   static_cast<float>(sp.x), static_cast<float>(sp.z), gGuiContext.worldTime);
				}

				// 20 TPS world tick: random ticks, scheduled ticks, BlockEntities, FallingBlocks, Paintings, Fire, animateTick
				static float worldTickAccum = 0.0f;
				static uint64_t worldTickCounter = 0;
				worldTickAccum += clampedDt;
				while (worldTickAccum >= 0.05f) {
					worldTickAccum -= 0.05f;
					++worldTickCounter;
					BlockSource& reg = c.getRegion();
					Level& lvl = c.getLevel();
					sWorldTickChangedBlocks.clear();
					int pcx = Math::floor(c.mPos.x) >> 4;
					int pcz = Math::floor(c.mPos.z) >> 4;
					Tick curTick(worldTickCounter);
					for (int dx = -3; dx <= 3; ++dx) {
						for (int dz = -3; dz <= 3; ++dz) {
							LevelChunk* lc = reg.getChunk(ChunkPos(pcx + dx, pcz + dz));
							if (!lc || lc->getState() != ChunkState::Loaded) continue;
							lc->tickBlocks(reg, lvl);
							lc->getTickQueue().tickPendingTicks(reg, curTick, 100, false);
							for (auto& bePair : lc->getBlockEntities()) {
								if (bePair.second) {
									bePair.second->tick(reg);
								}
							}
						}
					}
					if (!sWorldTickChangedBlocks.empty()) {
						std::vector<ChunkPos> rebuiltChunks;
						for (const BlockPos& bp : sWorldTickChangedBlocks) {
							ChunkPos cp(bp.x >> 4, bp.z >> 4);
							bool already = false;
							for (const ChunkPos& r : rebuiltChunks) {
								if (r == cp) { already = true; break; }
							}
							if (!already) {
								rebuiltChunks.push_back(cp);
								updateLightingAndRebuildChunks(reg, bp, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
							}
						}
						sWorldTickChangedBlocks.clear();
					}

					// Animated flipbook textures (fire_0, fire_1, still_water, flowing_water, still_lava, flowing_lava, portal)
					{
						struct FlipbookEntry {
							int dstX = 0, dstY = 0, tileW = 16, tileH = 16;
							int frameW = 16, numStripFrames = 1;
							int ticksPerFrame = 1;
							int tickAcc = 0;
							size_t seqIdx = 0;
							std::vector<int> frameSeq;
							std::vector<uint8_t> rgba;
						};
						static std::vector<FlipbookEntry> sFlipbooks;
						static bool sFlipbooksLoaded = false;
						if (!sFlipbooksLoaded) {
							sFlipbooksLoaded = true;
							mce::TexturePtr atTex = texgroup.getTexture(ResourceLocation("atlas.terrain"));
							int atlasW = atTex.get() ? static_cast<int>(atTex->getDescription().mWidth) : 0;
							int atlasH = atTex.get() ? static_cast<int>(atTex->getDescription().mHeight) : 0;
							if (atlas && atlasW > 0 && atlasH > 0) {
								std::string jsonPath = GuiComponent::resolveAssetPath("textures/flipbook_textures.json");
								std::ifstream ifs(jsonPath);
								if (ifs.good()) {
									std::string jsonStr((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
									Json::Value root;
									Json::Reader reader;
									if (reader.parse(jsonStr, root) && root.isArray()) {
										for (Json::ArrayIndex idx = 0; idx < root.size(); ++idx) {
											const Json::Value& item = root[idx];
											std::string texRel = item["flipbook_texture"].asString();
											std::string tileName = item["atlas_tile"].asString();
											if (texRel.empty() || tileName.empty()) continue;
											const TextureAtlasItem& ti = atlas->getTextureItem(tileName);
											if (ti.uvCount() <= 0) continue;
											const TextureUVCoordinateSet& uv = ti[0];
											int dstX = static_cast<int>(std::round(uv._u0 * atlasW));
											int dstY = static_cast<int>(std::round(uv._v0 * atlasH));
											int tileW = std::max(1, static_cast<int>(std::round((uv._u1 - uv._u0) * atlasW)));
											int tileH = std::max(1, static_cast<int>(std::round((uv._v1 - uv._v0) * atlasH)));
											std::string pngPath = GuiComponent::resolveAssetPath(texRel + ".png");
											int pw = 0, ph = 0, comp = 0;
											unsigned char* px = stbi_load(pngPath.c_str(), &pw, &ph, &comp, 4);
											if (!px || pw <= 0 || ph < pw) {
												if (px) stbi_image_free(px);
												continue;
											}
											FlipbookEntry fe;
											fe.dstX = dstX;
											fe.dstY = dstY;
											fe.tileW = tileW;
											fe.tileH = tileH;
											fe.frameW = pw;
											fe.numStripFrames = std::max(1, ph / pw);
											fe.ticksPerFrame = std::max(1, item["ticks_per_frame"].asInt(1));
											if (item.isMember("frames") && item["frames"].isArray()) {
												for (Json::ArrayIndex fi = 0; fi < item["frames"].size(); ++fi) {
													fe.frameSeq.push_back(item["frames"][fi].asInt() % fe.numStripFrames);
												}
											} else {
												for (int fi = 0; fi < fe.numStripFrames; ++fi) {
													fe.frameSeq.push_back(fi);
												}
											}
											fe.rgba.assign(px, px + static_cast<size_t>(pw * ph * 4));
											stbi_image_free(px);
											if (!fe.frameSeq.empty()) {
												sFlipbooks.push_back(std::move(fe));
											}
										}
									}
								}
							}
						}
						mce::TexturePtr atTex = texgroup.getTexture(ResourceLocation("atlas.terrain"));
						if (atTex.get() && !sFlipbooks.empty()) {
							int mipCount = std::max(1, static_cast<int>(atTex->getDescription().mMipMapCount));
							auto& rctx = mce::RenderContextImmediate::get();
							for (auto& fe : sFlipbooks) {
								if (++fe.tickAcc < fe.ticksPerFrame) continue;
								fe.tickAcc = 0;
								fe.seqIdx = (fe.seqIdx + 1) % fe.frameSeq.size();
								int f = fe.frameSeq[fe.seqIdx];
								const uint8_t* srcFrame = fe.rgba.data() + static_cast<size_t>(f * fe.frameW * fe.frameW * 4);
								std::vector<uint8_t> scaled(static_cast<size_t>(fe.tileW * fe.tileH * 4));
								for (int y = 0; y < fe.tileH; ++y) {
									int sy = (y * fe.frameW) / fe.tileH;
									for (int x = 0; x < fe.tileW; ++x) {
										int sx = (x * fe.frameW) / fe.tileW;
										const uint8_t* sp = srcFrame + (sy * fe.frameW + sx) * 4;
										uint8_t* dp = scaled.data() + (y * fe.tileW + x) * 4;
										dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = sp[3];
									}
								}
								for (int mip = 0; mip < mipCount; ++mip) {
									int mw = std::max(1, fe.tileW >> mip);
									int mh = std::max(1, fe.tileH >> mip);
									int mx = fe.dstX >> mip;
									int my = fe.dstY >> mip;
									if (mip == 0) {
										atTex->subBuffer(rctx, scaled.data(), mx, my, mw, mh, 0);
									} else {
										std::vector<uint8_t> mscaled(static_cast<size_t>(mw * mh * 4));
										for (int y = 0; y < mh; ++y) {
											int sy = (y * fe.tileH) / mh;
											for (int x = 0; x < mw; ++x) {
												int sx = (x * fe.tileW) / mw;
												const uint8_t* sp = scaled.data() + (sy * fe.tileW + sx) * 4;
												uint8_t* dp = mscaled.data() + (y * mw + x) * 4;
												dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = sp[3];
											}
										}
										atTex->subBuffer(rctx, mscaled.data(), mx, my, mw, mh, mip);
									}
								}
							}
						}
					}

					// Player persistent fire ticks (1 dmg every 20 ticks)
					if (gPlayerFireTicks > 0) {
						--gPlayerFireTicks;
						if (!gGuiContext.isCreative && (gPlayerFireTicks % 20) == 0) {
							damagePlayer(1, Vec3::ZERO);
						}
					}

					// Paintings 100-tick survives() check
					{
						auto& paintings = PaintingSystem::getPaintings();
						for (size_t pi = 0; pi < paintings.size(); ) {
							if (++paintings[pi].tickCounter >= 100) {
								paintings[pi].tickCounter = 0;
								if (!paintings[pi].survives(reg, paintings, static_cast<int>(pi))) {
									if (!gGuiContext.isCreative && Item::mPainting) {
										spawnWorldItem(worldItems, ItemInstance(Item::mPainting, 1, 0),
										               Vec3(paintings[pi].x, paintings[pi].y, paintings[pi].z), Vec3(0, 2.0f, 0), 0.25f);
									}
									paintings.erase(paintings.begin() + pi);
									continue;
								}
							}
							++pi;
						}
					}

					// Falling blocks tick (gravity -0.04/tick, drag 0.98) + Anvil fall damage & degrade
					for (size_t fi = 0; fi < sFallingBlocks.size();) {
						FallingBlockEnt& fb = sFallingBlocks[fi];
						fb.vy = (fb.vy - 0.04f) * 0.98f;
						fb.y += fb.vy;
						BlockPos belowPos(Math::floor(fb.x), Math::floor(fb.y - 0.01f), Math::floor(fb.z));
						if (fb.y <= 0.0f) {
							sFallingBlocks.erase(sFallingBlocks.begin() + fi);
							continue;
						}
						if (Block::mSand && !static_cast<const HeavyBlock*>(Block::mSand)->isFree(reg, belowPos)) {
							BlockPos landPos = belowPos.above();
							while (landPos.y < 127 && !static_cast<const HeavyBlock*>(Block::mSand)->isFree(reg, landPos)) {
								++landPos.y;
							}
							float fallDist = fb.startY - fb.y;
							bool isAnvil = (Block::mAnvil && fb.id == Block::mAnvil->mID);
							bool brokenAnvil = false;
							DataID placeData = fb.data;
							if (isAnvil) {
								SoundEngine::play(SoundEngine::SND_DIG, 0.9f, 0.6f);
								if (fallDist > 1.0f) {
									int dmg = std::min(40, std::max(0, static_cast<int>(std::floor(fallDist - 1.0f)) * 2));
									if (dmg > 0) {
										AABB landBB(static_cast<float>(landPos.x), static_cast<float>(landPos.y), static_cast<float>(landPos.z),
										            static_cast<float>(landPos.x + 1), static_cast<float>(landPos.y + 1), static_cast<float>(landPos.z + 1));
										if (!gGuiContext.isCreative && landBB.intersects(c.mBB)) {
											damagePlayer(dmg, Vec3::ZERO);
										}
									}
									int curDmg = Block::mAnvil->getBlockState(BlockState::Damage).get<int>(placeData);
									++curDmg;
									if (curDmg > 2) {
										brokenAnvil = true;
									} else {
										Block::mAnvil->getBlockState(BlockState::Damage).set(placeData, curDmg);
									}
								}
							}
							if (!brokenAnvil) {
								if (reg.getBlockID(landPos) == BlockID::AIR || reg.getMaterial(landPos).isReplaceable()) {
									reg.setBlockAndData(landPos, FullBlock(fb.id, placeData), Block::UPDATE_ALL);
									updateLightingAndRebuildChunks(reg, landPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
								} else if (!fb.creative && Block::mBlocks[fb.id]) {
									Block::mBlocks[fb.id]->popResource(reg, landPos, ItemInstance(fb.id, 1, placeData));
								}
							}
							sFallingBlocks.erase(sFallingBlocks.begin() + fi);
						} else {
							++fi;
						}
					}

					// 1:1 Level::animateTick (100 random blocks in r=16)
					BlockPos basePos(Math::floor(c.mPos.x), Math::floor(c.mPos.y), Math::floor(c.mPos.z));
					Random& animRng = lvl.getRandom();
					for (int ai = 0; ai < 100; ++ai) {
						BlockPos ap(basePos.x + animRng.nextGaussianInt(16),
						            basePos.y + animRng.nextGaussianInt(16),
						            basePos.z + animRng.nextGaussianInt(16));
						if (ap.y >= 1 && ap.y < 127) {
							reg.getBlock(ap).animateTick(reg, ap, animRng);
						}
					}
				}

				// Ambient torch / lit furnace particles near player (1:1 MCPE 0.8.1 TorchTile/FurnaceTile::animateTick)
				static float ambientParticleAccum = 0.0f;
				ambientParticleAccum += clampedDt;
				if (ambientParticleAccum >= 0.18f) {
					ambientParticleAccum = 0.0f;
					BlockSource& reg = c.getRegion();
					int px = Math::floor(c.mPos.x);
					int py = Math::floor(c.mPos.y);
					int pz = Math::floor(c.mPos.z);
					for (int i = 0; i < 6; ++i) {
						int rx = px + (std::rand() % 13) - 6;
						int ry = py + (std::rand() % 9) - 4;
						int rz = pz + (std::rand() % 13) - 6;
						if (ry < 1 || ry > 126) continue;
						BlockPos bpos(rx, ry, rz);
						FullBlock fb = reg.getBlockAndData(bpos);
						if (Block::mTorch && fb.id == Block::mTorch->mID) {
							float tx = rx + 0.5f;
							float ty = ry + 0.7f;
							float tz = rz + 0.5f;
							switch (fb.data & 7) {
								case TorchBlock::TORCH_WEST:  tx -= 0.27f; ty += 0.22f; break;
								case TorchBlock::TORCH_EAST:  tx += 0.27f; ty += 0.22f; break;
								case TorchBlock::TORCH_NORTH: tz -= 0.27f; ty += 0.22f; break;
								case TorchBlock::TORCH_SOUTH: tz += 0.27f; ty += 0.22f; break;
								default: break;
							}
							ParticleSystem::spawnTorchSmoke(tx, ty, tz, &reg);
						} else if (Block::mLitFurnace && fb.id == Block::mLitFurnace->mID) {
							int dir = Block::mLitFurnace->getBlockState(BlockState::FacingDirection).get<int>(fb.data);
							float fx = rx + 0.5f;
							float fy = ry + (ParticleSystem::rand01() * 6.0f) * 0.0625f;
							float fz = rz + 0.5f;
							float ss = ParticleSystem::rand01() * 0.6f - 0.3f;
							if (dir == Facing::WEST) {
								fx -= 0.52f; fz += ss;
							} else if (dir == Facing::EAST) {
								fx += 0.52f; fz += ss;
							} else if (dir == Facing::NORTH) {
								fx += ss; fz -= 0.52f;
							} else {
								fx += ss; fz += 0.52f;
							}
							ParticleSystem::spawnFurnaceSmoke(fx, fy, fz, &reg);
						}
					}
				}

				mobManager.peaceful = (gGuiOptions.difficulty == 0);
				mobManager.update(
					clampedDt,
					c.getRegion(),
					Vec3(c.mPos.x, c.mPos.y, c.mPos.z),
					c.mBB,
					skyInfo.sunFactor,
					gGuiContext.isCreative,
					allowPickup,
					damagePlayer,
					[&](const Vec3& expPos, float radius) {
						static Random expRng;
						SoundEngine::play(SoundEngine::SND_EXPLODE, 1.0f);
						ParticleSystem::spawnExplosion(expPos.x, expPos.y, expPos.z, radius, &c.getRegion());
						BlockSource& reg = c.getRegion();
						int r = static_cast<int>(std::ceil(radius));
						int cx = Math::floor(expPos.x);
						int cy = Math::floor(expPos.y);
						int cz = Math::floor(expPos.z);
						for (int dx = -r; dx <= r; ++dx) {
							for (int dy = -r; dy <= r; ++dy) {
								for (int dz = -r; dz <= r; ++dz) {
									if (dx * dx + dy * dy + dz * dz > radius * radius) continue;
									BlockPos bp(cx + dx, cy + dy, cz + dz);
									if (bp.y <= 0 || bp.y >= 127) continue;
									FullBlock fb = reg.getBlockAndData(bp);
									if (fb.id == 0) continue;
									const Block* blk = Block::mBlocks[fb.id];
									if (!blk || blk == Block::mBedrock || blk == Block::mObsidian || blk->getMaterial().isLiquid()) continue;
									if (reg.removeBlock(bp) && !gGuiContext.isCreative && expRng.nextFloat() < 0.38f) {
										int dropId = blk->getResource(expRng, fb.data, 0);
										if (dropId > 0) {
											ItemInstance dropItem(dropId, 1, 0);
											Vec3 dpos(bp.x + 0.5f, bp.y + 0.5f, bp.z + 0.5f);
											Vec3 dvel((expRng.nextFloat() - 0.5f) * 3.0f, 3.5f, (expRng.nextFloat() - 0.5f) * 3.0f);
											spawnWorldItem(worldItems, dropItem, dpos, dvel, 0.35f);
										}
									}
								}
							}
						}
						updateLightingAndRebuildChunks(reg, BlockPos(cx, cy, cz), rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
					},
					[&](const ItemInstance& dropItem, const Vec3& dropPos) {
						static Random mobDropRng2;
						Vec3 dvel((mobDropRng2.nextFloat() - 0.5f) * 2.4f, 3.0f, (mobDropRng2.nextFloat() - 0.5f) * 2.4f);
						spawnWorldItem(worldItems, dropItem, dropPos, dvel, 0.25f);
					},
					&gGuiContext.inventorySlots[std::max(0, std::min(8, gGuiContext.selectedHotbarSlot))],
					[&](const Vec3& pos, float hw, float h) {
						ParticleSystem::spawnDeathPoof(pos.x, pos.y, pos.z, hw, h, &c.getRegion());
					},
					[&](const Vec3& pos, float hw, float h, int count) {
						for (int k = 0; k < count; ++k) {
							float hx = pos.x + (ParticleSystem::rand01() * 2.0f - 1.0f) * hw;
							float hy = pos.y + 0.5f + ParticleSystem::rand01() * h;
							float hz = pos.z + (ParticleSystem::rand01() * 2.0f - 1.0f) * hw;
							ParticleSystem::spawnHeart(hx, hy, hz,
							                           ParticleSystem::randSigned() * 0.02f,
							                           ParticleSystem::randSigned() * 0.02f,
							                           ParticleSystem::randSigned() * 0.02f,
							                           &c.getRegion());
						}
					});

				// Update arrows
				{
					auto& arrows = ProjectileTntSystem::getArrows();
					BlockSource& reg = c.getRegion();
					for (size_t ai = 0; ai < arrows.size();) {
						auto& a = arrows[ai];
						a.age += clampedDt;
						if (a.age > 20.0f) {
							arrows[ai] = arrows.back();
							arrows.pop_back();
							continue;
						}
						if (!a.inGround) {
							BlockID curBid = reg.getBlockID(BlockPos(Math::floor(a.x), Math::floor(a.y), Math::floor(a.z)));
							bool inWater = (curBid == 8 || curBid == 9);
							float dragPerTick = inWater ? 0.80f : 0.99f;
							float drag = std::pow(dragPerTick, clampedDt * 20.0f);
							a.vx *= drag;
							a.vz *= drag;
							a.vy = a.vy * drag - 20.0f * clampedDt;
							ProjectileTntSystem::updateArrowOrientation(a);
							float nx = a.x + a.vx * clampedDt;
							float ny = a.y + a.vy * clampedDt;
							float nz = a.z + a.vz * clampedDt;
							float stepLen = std::sqrt((nx - a.x) * (nx - a.x) + (ny - a.y) * (ny - a.y) + (nz - a.z) * (nz - a.z));
							bool hitEntity = false;
							if (a.fromPlayer && stepLen > 0.001f) {
								float mobHitDist = 0.0f;
								int hitMob = mobManager.raycastMob(
									Vec3(a.x, a.y, a.z),
									Vec3((nx - a.x) / stepLen, (ny - a.y) / stepLen, (nz - a.z) / stepLen),
									stepLen + 0.35f, mobHitDist);
								if (hitMob >= 0) {
									static Random arrowDropRng;
									bool landed = mobManager.damageMob(
										hitMob, a.damage, Vec3(a.vx, 0.0f, a.vz),
										[&](const ItemInstance& dropItem, const Vec3& dropPos) {
											Vec3 dvel((arrowDropRng.nextFloat() - 0.5f) * 2.4f, 3.0f, (arrowDropRng.nextFloat() - 0.5f) * 2.4f);
											spawnWorldItem(worldItems, dropItem, dropPos, dvel, 0.25f);
										});
									if (landed) {
										SoundEngine::play(SoundEngine::SND_HURT, 0.8f);
									}
									hitEntity = true;
								}
							} else if (!a.fromPlayer) {
								if (nx >= c.mBB.min.x && nx <= c.mBB.max.x &&
								    ny >= c.mBB.min.y && ny <= c.mBB.max.y &&
								    nz >= c.mBB.min.z && nz <= c.mBB.max.z) {
									damagePlayer(a.damage, Vec3(a.vx, 0.0f, a.vz));
									hitEntity = true;
								}
							}
							if (hitEntity) {
								arrows[ai] = arrows.back();
								arrows.pop_back();
								continue;
							}
							HitResult blockHit = reg.clip(Vec3(a.x, a.y, a.z), Vec3(nx, ny, nz), false, true, 50, false);
							if ((blockHit.isHit() && blockHit.isTile()) ||
							    reg.isSolidBlockingBlock(BlockPos(Math::floor(nx), Math::floor(ny), Math::floor(nz)))) {
								ProjectileTntSystem::updateArrowOrientation(a);
								if (blockHit.isHit() && blockHit.isTile() && stepLen > 0.0001f) {
									const Vec3& hp = blockHit.getPos();
									float dirX = (nx - a.x) / stepLen;
									float dirY = (ny - a.y) / stepLen;
									float dirZ = (nz - a.z) / stepLen;
									a.x = hp.x - dirX * 0.05f;
									a.y = hp.y - dirY * 0.05f;
									a.z = hp.z - dirZ * 0.05f;
								}
								a.inGround = true;
								a.vx = a.vy = a.vz = 0.0f;
							} else {
								a.x = nx; a.y = ny; a.z = nz;
							}
						}
						++ai;
					}
				}

				// Update primed TNT
				{
					auto& tnts = ProjectileTntSystem::getTnts();
					BlockSource& reg = c.getRegion();
					for (size_t ti = 0; ti < tnts.size();) {
						auto& t = tnts[ti];
						t.fuse -= clampedDt;
						if (t.fuse <= 0.0f) {
							static Random tntRng;
							SoundEngine::play(SoundEngine::SND_EXPLODE, 1.0f);
							ParticleSystem::spawnExplosion(t.x, t.y + 0.5f, t.z, 3.8f, &reg);
							int cx = Math::floor(t.x), cy = Math::floor(t.y + 0.5f), cz = Math::floor(t.z);
							float radius = 3.8f;
							int r = 4;
							for (int dx = -r; dx <= r; ++dx) {
								for (int dy = -r; dy <= r; ++dy) {
									for (int dz = -r; dz <= r; ++dz) {
										if (dx * dx + dy * dy + dz * dz > radius * radius) continue;
										BlockPos bp(cx + dx, cy + dy, cz + dz);
										if (bp.y <= 0 || bp.y >= 127) continue;
										FullBlock fb = reg.getBlockAndData(bp);
										if (fb.id == 0) continue;
										const Block* blk = Block::mBlocks[fb.id];
										if (!blk || blk == Block::mBedrock || blk == Block::mObsidian || blk->getMaterial().isLiquid()) continue;
										if (reg.removeBlock(bp) && !gGuiContext.isCreative && tntRng.nextFloat() < 0.35f) {
											int dropId = blk->getResource(tntRng, fb.data, 0);
											if (dropId > 0) {
												spawnWorldItem(worldItems, ItemInstance(dropId, 1, 0),
												               Vec3(bp.x + 0.5f, bp.y + 0.5f, bp.z + 0.5f),
												               Vec3((tntRng.nextFloat() - 0.5f) * 3.0f, 3.5f, (tntRng.nextFloat() - 0.5f) * 3.0f), 0.35f);
											}
										}
									}
								}
							}
							float pdx = c.mPos.x - t.x, pdy = c.mPos.y - t.y, pdz = c.mPos.z - t.z;
							float pdist = std::sqrt(pdx * pdx + pdy * pdy + pdz * pdz);
							if (pdist < 5.0f) {
								int dmg = std::max(1, static_cast<int>((1.0f - pdist / 5.0f) * 14.0f));
								damagePlayer(dmg, Vec3(pdx, 0.0f, pdz));
							}
							updateLightingAndRebuildChunks(reg, BlockPos(cx, cy, cz), rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
							tnts[ti] = tnts.back();
							tnts.pop_back();
							continue;
						}
						t.vy -= 18.0f * clampedDt;
						float ny = t.y + t.vy * clampedDt;
						if (reg.isSolidBlockingBlock(BlockPos(Math::floor(t.x), Math::floor(ny), Math::floor(t.z)))) {
							t.vy = 0.0f;
							t.vx *= 0.7f;
							t.vz *= 0.7f;
						} else {
							t.x += t.vx * clampedDt;
							t.y = ny;
							t.z += t.vz * clampedDt;
						}
						++ti;
					}
				}
			}
			float aspect = static_cast<float>(screenx) / static_cast<float>(std::max(1, screeny));
			if (screenx != lastProjW || screeny != lastProjH) {
				lastProjW = screenx;
				lastProjH = screeny;
				projection = glm::perspective(glm::radians(85.0f), aspect, 0.1f, 1000.0f);
				for (mce::Shader* sh : uniqueTerrainShaders) {
					sh->setMatrix4("PROJ", glm::value_ptr(projection));
				}
			}

			// Compute camera position & view (first-person with viewBobbing vs third-person with collision)
			glm::vec3 eyePos = c.mPos;
			glm::vec3 renderCamPos = eyePos;
			glm::vec3 lookTarget = eyePos + c.cameraFront;

			if (gGuiOptions.thirdPerson) {
				const float thirdPersonDist = 4.0f;
				glm::vec3 desiredCam = eyePos - c.cameraFront * thirdPersonDist;
				HitResult camHit = c.getRegion().clip(
					Vec3(eyePos.x, eyePos.y, eyePos.z),
					Vec3(desiredCam.x, desiredCam.y, desiredCam.z),
					false, true, 50, false);
				if (camHit.isHit() && camHit.isTile()) {
					const Vec3& hp = camHit.getPos();
					glm::vec3 hitVec(hp.x, hp.y, hp.z);
					float hitDist = glm::length(hitVec - eyePos);
					float clampedDist = std::max(0.45f, hitDist - 0.25f);
					renderCamPos = eyePos - c.cameraFront * clampedDist;
				} else {
					renderCamPos = desiredCam;
				}
				lookTarget = eyePos;
			}

			glm::mat4 view = glm::lookAt(renderCamPos, lookTarget, c.cameraUp);
			latestRenderCamPos = renderCamPos;
			glm::mat4 camRelView = glm::lookAt(glm::vec3(0.0f), lookTarget - renderCamPos, c.cameraUp);
			if (!gGuiOptions.thirdPerson) {
				glm::mat4 bobMat(1.0f);
				bobHurt(bobMat, c);
				if (gGuiOptions.viewBobbing) {
					bobView(bobMat, c);
				}
				view = bobMat * view;
				camRelView = bobMat * camRelView;
			}
			FrustumPlanes frustum;
			frustum.extract(projection * camRelView);

			if (!hasUploadedWorldView || std::memcmp(&camRelView, &lastUploadedWorldView, sizeof(glm::mat4)) != 0) {
				lastUploadedWorldView = camRelView;
				hasUploadedWorldView = true;
				for (mce::Shader* sh : uniqueTerrainShaders) {
					sh->setMatrix4("WORLDVIEW", glm::value_ptr(camRelView));
				}
			}

			ChunkPos camChunkPos(Math::floor(c.mPos.x / 16.0f), Math::floor(c.mPos.z / 16.0f));
			int renderDistIdx = std::max(0, std::min(3, gGuiOptions.renderDistance));
			// Tiny=2, Short=4, Normal=6, Far=8 chunks
			int renderDistance = 2 * (1 + renderDistIdx);
			int activeRenderDist = gGuiContext.loadingChunks ? std::min(renderDistance, 2) : renderDistance;
			if (activeRenderDist != lastUploadedRenderDist) {
				lastUploadedRenderDist = activeRenderDist;
				const float fogDistBlocks = static_cast<float>(activeRenderDist * 16);
				for (mce::Shader* sh : uniqueTerrainShaders) {
					sh->setFloat("FAR_CHUNKS_DISTANCE", fogDistBlocks);
					sh->setFloat("RENDER_DISTANCE", fogDistBlocks);
				}
			}
			int lightDistance = activeRenderDist + 1;
			int loadDistance = lightDistance + 2;           // +2 beyond lightDistance ensures lightDistance's 3x3 neighbors finish postProcess (Loaded)
			int renderUnloadDist = activeRenderDist + 2;    // +2 hysteresis buffer prevents RenderChunk/VBO thrashing on chunk borders
			int chunkUnloadDist = loadDistance + 2;         // +2 hysteresis buffer prevents LevelChunk unload/reload thrashing on borders
			size_t maxRenderPool = static_cast<size_t>((2 * renderUnloadDist + 1) * (2 * renderUnloadDist + 1) * 8);
			maxRenderPoolCache = maxRenderPool;
			ChunkSource& chunkSrc = commandContext.dim->getChunkSource();
			const auto& renderOffsets = getCachedSpiralOffsets(activeRenderDist);

			auto computeSubChunkLod = [&](const BlockPos& center) -> uint8_t {
				if (gGuiContext.loadingChunks) {
					return 0;
				}
				const float dx = static_cast<float>(center.x) - renderCamPos.x;
				const float dz = static_cast<float>(center.z) - renderCamPos.z;
				const float distSq = dx * dx + dz * dz;
				if (distSq >= 176.0f * 176.0f) return 3;
				if (distSq >= 128.0f * 128.0f) return 2;
				if (distSq >= 64.0f * 64.0f) return 1;
				return 0;
			};

			static bool allLoadOffsetsSettled = false;
			static bool allLightOffsetsSettled = false;
			static ChunkPos lastSettledCamChunkPos(INT_MAX, INT_MAX);
			static int lastSettledRenderDist = -1;
			static uint64_t rcGeneration = 1;
			if (camChunkPos != lastSettledCamChunkPos || activeRenderDist != lastSettledRenderDist) {
				allLoadOffsetsSettled = false;
				allLightOffsetsSettled = false;
				lastSettledCamChunkPos = camChunkPos;
				lastSettledRenderDist = activeRenderDist;
			}

			auto tUnload0 = std::chrono::high_resolution_clock::now();
			// Only scan unload maps when the camera enters a different chunk or render distance changes
			if (camChunkPos != lastUnloadCamChunkPos || activeRenderDist != lastUnloadRenderDist) {
				lastUnloadCamChunkPos = camChunkPos;
				lastUnloadRenderDist = activeRenderDist;

				for (auto it = rc.begin(); it != rc.end(); ) {
					const ChunkPos pos = it->first;
					if (std::abs(pos.x - camChunkPos.x) > renderUnloadDist ||
					    std::abs(pos.z - camChunkPos.z) > renderUnloadDist) {
						for (int i = 0; i < 8; ++i) {
							releaseRenderChunk(it->second[i], maxRenderPool);
						}
						needsReRendering2.erase(pos);
						it = rc.erase(it);
						++rcGeneration;
					} else {
						++it;
					}
				}
				while (renderChunkPool.size() > maxRenderPool) {
					delete renderChunkPool.back();
					renderChunkPool.pop_back();
				}

				chunkSrc.hintDiscardBatchBegin();
				for (auto it = heldChunks.begin(); it != heldChunks.end(); ) {
					const ChunkPos& cp = it->first;
					if (std::abs(cp.x - camChunkPos.x) > chunkUnloadDist ||
					    std::abs(cp.z - camChunkPos.z) > chunkUnloadDist) {
						LevelChunk* lc = it->second;
						if (lc) {
							if (lc->isLoaded() || lc->getState() == ChunkState::Loaded) {
								serializeChunkWorldItems(cp, worldItems, *lc, true);
							}
							chunkSrc.releaseChunk(*lc);
						}
						loadedChunkSet.erase(cp);
						litChunks.erase(cp);
						needsReRendering.erase(cp);
						needsReRendering2.erase(cp);
						auto rcIt = rc.find(cp);
						if (rcIt != rc.end()) {
							for (int i = 0; i < 8; ++i) {
								releaseRenderChunk(rcIt->second[i], maxRenderPool);
							}
							rc.erase(rcIt);
						}
						it = heldChunks.erase(it);
					} else {
						++it;
					}
				}
				chunkSrc.hintDiscardBatchEnd();

				for (auto it = needsReRendering.begin(); it != needsReRendering.end(); ) {
					if (std::abs(it->x - camChunkPos.x) > chunkUnloadDist ||
					    std::abs(it->z - camChunkPos.z) > chunkUnloadDist) {
						it = needsReRendering.erase(it);
					} else {
						++it;
					}
				}
				for (auto it = needsReRendering2.begin(); it != needsReRendering2.end(); ) {
					if (std::abs(it->x - camChunkPos.x) > activeRenderDist ||
					    std::abs(it->z - camChunkPos.z) > activeRenderDist) {
						it = needsReRendering2.erase(it);
					} else {
						++it;
					}
				}
				// Ensure any chunk newly inside activeRenderDist or needing an LOD upgrade/downgrade is inspected by Phase 3
				for (const auto& off : renderOffsets) {
					ChunkPos rcp(camChunkPos.x + off.dx, camChunkPos.z + off.dz);
					auto rcIt = rc.find(rcp);
					if (rcIt == rc.end()) {
						needsReRendering2.insert(rcp);
					} else {
						for (int sy = 0; sy < 8; ++sy) {
							RenderChunk* part = rcIt->second[sy];
							if (!part || part->isBuildState(RenderChunk::BuildState::Dirty)) {
								needsReRendering2.insert(rcp);
								break;
							}
							if (part->isBuildState(RenderChunk::BuildState::Ready) && !part->isEmpty()) {
								const uint8_t neededLod = computeSubChunkLod(part->getCenter());
								const uint8_t builtLod = part->getLodLevel();
								if (neededLod < builtLod || neededLod > static_cast<uint8_t>(builtLod + 1)) {
									part->setDirty(Tick(0), false);
									needsReRendering2.insert(rcp);
								}
							}
						}
					}
				}
			}

			auto queueChunkAndNeighbors = [&](const ChunkPos& cp) {
				if (std::abs(cp.x - camChunkPos.x) <= activeRenderDist &&
				    std::abs(cp.z - camChunkPos.z) <= activeRenderDist) {
					needsReRendering2.insert(cp);
				}
				for (int i = 1; i < 9; i++) {
					ChunkPos np = cp + offsets[i];
					if (rc.find(np) != rc.end() ||
					    (std::abs(np.x - camChunkPos.x) <= activeRenderDist &&
					     std::abs(np.z - camChunkPos.z) <= activeRenderDist)) {
						needsReRendering2.insert(np);
					}
				}
			};

			auto tChunkPhase0 = std::chrono::high_resolution_clock::now();
			ChunkPerfMetrics::instance.unloadUs.fetch_add(
				static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(tChunkPhase0 - tUnload0).count()),
				std::memory_order_relaxed);
			auto phaseElapsedMs = [](const std::chrono::high_resolution_clock::time_point& startTP) {
				return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - startTP).count();
			};

			// Phase 1: Chunk Loading (_ChunkSorter cached nearest-first spiral order + async Streaming / bounded fallback)
#ifdef UNSTABLE_THREADING
			const double loadBudgetMs = gGuiContext.loadingChunks ? 10.0 : 2.0;
			const int maxLoadsPerFrame = gGuiContext.loadingChunks ? 32 : 8;
#else
			const double loadBudgetMs = gGuiContext.loadingChunks ? 20.0 : 6.0;
			const int maxLoadsPerFrame = gGuiContext.loadingChunks ? 16 : 3;
#endif
			chunkSrc.setDeferPostProcessing(!gGuiContext.loadingChunks);
			int loadedThisFrame = chunkSrc.processDeferredPostProcesses(
				camChunkPos,
				gGuiContext.loadingChunks ? 16 : 1,
				loadBudgetMs);
			const auto& loadOffsets = getCachedSpiralOffsets(loadDistance);
			if (!allLoadOffsetsSettled) {
				bool anyPendingLoad = false;
				bool brokeEarly = false;
				for (const auto& off : loadOffsets) {
					ChunkPos cp(camChunkPos.x + off.dx, camChunkPos.z + off.dz);
					if (loadedChunkSet.find(cp) != loadedChunkSet.end()) {
						continue;
					}
					LevelChunk* lc = nullptr;
					auto heldIt = heldChunks.find(cp);
					if (heldIt == heldChunks.end()) {
						if (loadedThisFrame >= maxLoadsPerFrame ||
						    (loadedThisFrame > 0 && phaseElapsedMs(tChunkPhase0) >= loadBudgetMs)) {
							brokeEarly = true;
							break;
						}
						lc = acquireHeldChunk(chunkSrc, cp);
						++loadedThisFrame;
					} else {
						lc = heldIt->second;
					}

					if (lc && lc->getState() == ChunkState::Loaded) {
						if (loadedChunkSet.insert(cp).second) {
							deserializeChunkWorldItems(*lc, worldItems);
							queueChunkAndNeighbors(cp);
						}
					} else {
						anyPendingLoad = true;
						if (std::abs(off.dx) <= lightDistance + 1 && std::abs(off.dz) <= lightDistance + 1) {
							needsReRendering.insert(cp);
						}
					}
				}
				if (!brokeEarly && !anyPendingLoad) {
					allLoadOffsetsSettled = true;
				}
			}

			WorkerPool::pumpMainThreadCallbacks();
			for (auto it = needsReRendering.begin(); it != needsReRendering.end(); ) {
				ChunkPos pos = *it;
				auto* lc = chunkSrc.getExistingChunk(pos);
				if (lc && lc->getState() == ChunkState::Loaded) {
					if (loadedChunkSet.insert(pos).second) {
						deserializeChunkWorldItems(*lc, worldItems);
					}
					queueChunkAndNeighbors(pos);
					it = needsReRendering.erase(it);
				} else {
					++it;
				}
			}
			auto tLightPhase0 = std::chrono::high_resolution_clock::now();
			ChunkPerfMetrics::instance.loadPhaseUs.fetch_add(
				static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(tLightPhase0 - tChunkPhase0).count()),
				std::memory_order_relaxed);

			// Phase 2: Decoupled Local/Border Lighting (budgeted, ~3.5ms in gameplay so Light + Mesh stay within 60fps frame budget)
			const double lightBudgetMs = gGuiContext.loadingChunks ? 25.0 : 3.5;
			const int maxLitPerFrame = gGuiContext.loadingChunks ? 24 : 4;
			int litThisFrame = 0;
			const auto& lightOffsets = getCachedSpiralOffsets(lightDistance);
			if (!allLightOffsetsSettled) {
				bool anyPendingLight = false;
				bool brokeLightEarly = false;
				for (const auto& off : lightOffsets) {
					if (litThisFrame >= maxLitPerFrame ||
					    (litThisFrame > 0 && phaseElapsedMs(tLightPhase0) >= lightBudgetMs)) {
						brokeLightEarly = true;
						break;
					}
					ChunkPos pos(camChunkPos.x + off.dx, camChunkPos.z + off.dz);
					if (litChunks.find(pos) != litChunks.end()) continue;
					if (!areChunkNeighborsLoaded(chunkSrc, pos)) {
						anyPendingLight = true;
						continue;
					}

					litChunks.insert(pos);
					queueChunkAndNeighbors(pos);
					WorldLighting::TouchedSubChunkMap touchedSubChunks;
					WorldLighting::propagateChunkLighting(c.getRegion(), pos, nullptr, &touchedSubChunks);
					++litThisFrame;

					// Only dirty already-built neighbor subchunks whose lighting actually changed
					for (const auto& kv : touchedSubChunks) {
						const ChunkPos& np = kv.first;
						if (np == pos) continue;
						auto rcIt = rc.find(np);
						if (rcIt != rc.end()) {
							for (int s = 0; s < 8; ++s) {
								if ((kv.second & (1u << s)) != 0 && rcIt->second[s]) {
									rcIt->second[s]->setDirty(Tick(0), false);
								}
							}
							if (std::abs(np.x - camChunkPos.x) <= activeRenderDist &&
							    std::abs(np.z - camChunkPos.z) <= activeRenderDist) {
								needsReRendering2.insert(np);
							}
						}
					}
				}
				if (!brokeLightEarly && !anyPendingLight) {
					allLightOffsetsSettled = true;
				}
			}

			// Phase 3: Parallel Async SubChunk Meshing with MCPE 0.8.1 DirtyChunkSorter (Frustum first + 3D distance)
			auto tMesh0 = std::chrono::high_resolution_clock::now();
			struct DirtySubChunkCandidate {
				RenderChunk* part;
				bool immediate;
				bool inFrustum;
				float distSq;
			};
			static std::vector<DirtySubChunkCandidate> dirtySubChunks;
			static std::vector<ChunkPos> cleanMeshChunks;
			dirtySubChunks.clear();
			cleanMeshChunks.clear();

			for (const ChunkPos& pos : needsReRendering2) {
				if (std::abs(pos.x - camChunkPos.x) > activeRenderDist ||
				    std::abs(pos.z - camChunkPos.z) > activeRenderDist) {
					continue;
				}
				if (!areChunkNeighborsLoaded(chunkSrc, pos)) {
					continue;
				}
				bool readyToMesh = (litChunks.find(pos) != litChunks.end());
				for (int dx = -1; dx <= 1 && readyToMesh; ++dx) {
					for (int dz = -1; dz <= 1 && readyToMesh; ++dz) {
						ChunkPos ncp(pos.x + dx, pos.z + dz);
						if (std::abs(ncp.x - camChunkPos.x) <= lightDistance &&
						    std::abs(ncp.z - camChunkPos.z) <= lightDistance &&
						    litChunks.find(ncp) == litChunks.end()) {
							readyToMesh = false;
						}
					}
				}
				if (!readyToMesh) continue;

				LevelChunk* centerLc = chunkSrc.getExistingChunk(pos);
				if (!centerLc) continue;
				const auto subs = centerLc->getSubChunks();
				const size_t prevRcSize = rc.size();
				auto& subChunks = rc[pos];
				if (rc.size() != prevRcSize) {
					++rcGeneration;
				}
				bool allSettled = true;
				for (int sy = 0; sy < 8; ++sy) {
					const bool hasBlocks = (static_cast<size_t>(sy) < subs.size() && subs[sy]->hasBlocks());
					if (subChunks[sy] == nullptr) {
						subChunks[sy] = acquireRenderChunk(BlockPos(pos.x * 16, sy * 16, pos.z * 16));
						if (!hasBlocks) {
							subChunks[sy]->makeReadyAsEmpty(false);
							ChunkPerfMetrics::instance.emptySubChunksSkipped.fetch_add(1, std::memory_order_relaxed);
							continue;
						}
					} else if (subChunks[sy]->isBuildState(RenderChunk::BuildState::Dirty)) {
						if (!hasBlocks) {
							subChunks[sy]->makeReadyAsEmpty(false);
							ChunkPerfMetrics::instance.emptySubChunksSkipped.fetch_add(1, std::memory_order_relaxed);
							continue;
						}
					} else {
						continue;
					}

					allSettled = false;
					RenderChunk* part = subChunks[sy];
					const BlockPos& center = part->getCenter();
					const float dx = static_cast<float>(center.x) - renderCamPos.x;
					const float dy = static_cast<float>(center.y) - renderCamPos.y;
					const float dz = static_cast<float>(center.z) - renderCamPos.z;
					dirtySubChunks.push_back({
						part,
						part->hasImmediateChange(),
						frustum.isBoxVisible(part->getPosition(), 2.0f, renderCamPos),
						dx * dx + dy * dy + dz * dz
					});
				}
				if (allSettled) {
					cleanMeshChunks.push_back(pos);
				}
			}
			for (const ChunkPos& cleanPos : cleanMeshChunks) {
				needsReRendering2.erase(cleanPos);
			}

			// MCPE 0.8.1 DirtyChunkSorter: immediate edits > in-frustum subchunks > closest 3D distance
			std::sort(dirtySubChunks.begin(), dirtySubChunks.end(),
			          [](const DirtySubChunkCandidate& a, const DirtySubChunkCandidate& b) {
				          if (a.immediate != b.immediate) return a.immediate > b.immediate;
				          if (a.inFrustum != b.inFrustum) return a.inFrustum > b.inFrustum;
				          return a.distSq < b.distSq;
			          });

			const int maxDispatchPerFrame = gGuiContext.loadingChunks ? 12 : 4;
			const int maxSyncImmediatePerFrame = 2;
			int dispatchedAsync = 0;
			int rebuiltSync = 0;
			const bool fancyGraphics = gGuiOptions.fancyGraphics;
			const bool greedyMeshing = gGuiOptions.greedyMeshing;

			for (const auto& cand : dirtySubChunks) {
				RenderChunk* chunkPart = cand.part;
				if (!chunkPart->isBuildState(RenderChunk::BuildState::Dirty)) continue;

				if (cand.immediate && rebuiltSync < maxSyncImmediatePerFrame && chunkBuilder) {
					const uint8_t targetLod = greedyMeshing ? computeSubChunkLod(chunkPart->getCenter()) : 0;
					chunkPart->startRebuild(std::move(chunkBuilder), false);
					auto tBuild0 = std::chrono::high_resolution_clock::now();
					chunkPart->rebuild(fancyGraphics, true, targetLod, greedyMeshing, false);
					auto tBuild1 = std::chrono::high_resolution_clock::now();
					chunkBuilder = chunkPart->endRebuild(Vec3(renderCamPos.x, renderCamPos.y, renderCamPos.z));
					auto tBuild2 = std::chrono::high_resolution_clock::now();
					ChunkPerfMetrics::instance.meshBuildUs.fetch_add(
						static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(tBuild1 - tBuild0).count()), std::memory_order_relaxed);
					ChunkPerfMetrics::instance.meshUploadUs.fetch_add(
						static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(tBuild2 - tBuild1).count()), std::memory_order_relaxed);
					ChunkPerfMetrics::instance.meshRebuilds.fetch_add(1, std::memory_order_relaxed);
					++rebuiltSync;
					continue;
				}

				if (dispatchedAsync >= maxDispatchPerFrame || asyncBuilderPool.empty()) {
					if (asyncBuilderPool.empty() || dispatchedAsync >= maxDispatchPerFrame) {
						break;
					}
				}

				std::unique_ptr<RenderChunkBuilder> workerBuilder = std::move(asyncBuilderPool.back());
				asyncBuilderPool.pop_back();
				inFlightRenderChunks.insert(chunkPart);
				chunkPart->startRebuild(std::move(workerBuilder), false);
				const uint8_t targetLod = computeSubChunkLod(chunkPart->getCenter());
				const uint64_t taskEpoch = worldEpoch;
				const int priority = (cand.inFrustum ? 0 : 10000) + std::min(9000, static_cast<int>(cand.distSq));

				WorkerPool::getFor(WorkerRole::MainThread).queue(
					[chunkPart, fancyGraphics, targetLod, greedyMeshing]() -> bool {
						auto tBuild0 = std::chrono::high_resolution_clock::now();
						chunkPart->rebuild(fancyGraphics, true, targetLod, greedyMeshing, false);
						auto tBuild1 = std::chrono::high_resolution_clock::now();
						ChunkPerfMetrics::instance.meshBuildUs.fetch_add(
							static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(tBuild1 - tBuild0).count()),
							std::memory_order_relaxed);
						return true;
					},
					[&, chunkPart, taskEpoch]() {
						auto tUp0 = std::chrono::high_resolution_clock::now();
						inFlightRenderChunks.erase(chunkPart);
						auto returnedBuilder = chunkPart->endRebuild(Vec3(latestRenderCamPos.x, latestRenderCamPos.y, latestRenderCamPos.z));
						auto tUp1 = std::chrono::high_resolution_clock::now();
						ChunkPerfMetrics::instance.meshUploadUs.fetch_add(
							static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(tUp1 - tUp0).count()),
							std::memory_order_relaxed);
						ChunkPerfMetrics::instance.meshRebuilds.fetch_add(1, std::memory_order_relaxed);
						if (returnedBuilder) {
							if (taskEpoch == worldEpoch && asyncBuilderPool.size() < kAsyncBuilderPoolSize) {
								asyncBuilderPool.push_back(std::move(returnedBuilder));
							}
						}
						if (retiredInFlightChunks.erase(chunkPart) > 0) {
							RenderChunk* toRelease = chunkPart;
							releaseRenderChunk(toRelease, maxRenderPoolCache);
						} else if (taskEpoch == worldEpoch && chunkPart->isBuildState(RenderChunk::BuildState::Dirty)) {
							const BlockPos& bp = chunkPart->getPosition();
							needsReRendering2.insert(ChunkPos(bp.x >> 4, bp.z >> 4));
						}
					},
					priority
				);
				++dispatchedAsync;
			}
			ChunkPerfMetrics::instance.meshUs.fetch_add(
				static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - tMesh0).count()),
				std::memory_order_relaxed);

			// Bind lightmap (TEXTURE_1) & neutral foliage fallback (TEXTURE_2)
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, gLightmapTex);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, gWhiteTex);
			glActiveTexture(GL_TEXTURE0);

			BlockPos camBlockPos(Math::floor(renderCamPos.x), Math::floor(renderCamPos.y), Math::floor(renderCamPos.z));
			const Material& camMat = c.getRegion().getMaterial(camBlockPos);
			bool camUnderwater = camMat.isType(MaterialType::Water);
			bool camUnderlava = camMat.isType(MaterialType::Lava);
			RenderChunk::setMaterialsBasedOnCameraUnderLiquid(camUnderwater, camUnderlava);

			glm::vec4 fogCol(skyInfo.fogColor.r, skyInfo.fogColor.g, skyInfo.fogColor.b, 1.0f);
			glm::vec2 fogCtrl(0.55f, 1.0f);
			if (camUnderwater) {
				fogCol = glm::vec4(0.05f * skyInfo.sunFactor, 0.14f * skyInfo.sunFactor, 0.36f * skyInfo.sunFactor, 1.0f);
				fogCtrl = glm::vec2(0.05f, 0.48f);
			} else if (camUnderlava) {
				fogCol = glm::vec4(0.60f, 0.12f, 0.02f, 1.0f);
				fogCtrl = glm::vec2(0.02f, 0.15f);
			}
			if (std::abs(fogCol.r - lastUploadedFogCol.r) > 0.0005f ||
			    std::abs(fogCol.g - lastUploadedFogCol.g) > 0.0005f ||
			    std::abs(fogCol.b - lastUploadedFogCol.b) > 0.0005f ||
			    std::abs(fogCol.a - lastUploadedFogCol.a) > 0.0005f ||
			    fogCtrl != lastUploadedFogCtrl) {
				lastUploadedFogCol = fogCol;
				lastUploadedFogCtrl = fogCtrl;
				for (mce::Shader* sh : uniqueTerrainShaders) {
					sh->setVector4("FOG_COLOR", fogCol);
					sh->setVector2("FOG_CONTROL", fogCtrl);
				}
			}

			static glm::vec3 lastUploadedViewPos(1e9f, 1e9f, 1e9f);
			if (renderCamPos.x != lastUploadedViewPos.x ||
			    renderCamPos.y != lastUploadedViewPos.y ||
			    renderCamPos.z != lastUploadedViewPos.z) {
				lastUploadedViewPos = renderCamPos;
				for (mce::Shader* sh : uniqueTerrainShaders) {
					sh->setVector3("VIEW_POS", renderCamPos);
				}
			}

			if (!camUnderwater && !camUnderlava && gGuiOptions.fancySkies) {
				renderSkyAndCelestials(projection, camRelView, skyInfo, gGuiContext.worldTime);
				renderClouds(projection, camRelView, renderCamPos, skyInfo, gGuiContext.worldTime, gGuiContext.worldTimeSubTick, gGuiOptions.fancyGraphics);
			}

			// Rebind lightmap/atlas after sky rendering
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, gLightmapTex);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, gWhiteTex);
			glActiveTexture(GL_TEXTURE0);

			auto setChunkOrigin = [&](const BlockPos& position) {
				// Uploaded lazily by RenderChunk::render() to the one shader that draws (cached uniform location),
				// instead of glUseProgram + glGetUniformLocation on all 19 terrain shaders per subchunk.
				RenderChunk::setChunkOrigin(
					static_cast<float>(position.x) - renderCamPos.x,
					static_cast<float>(position.y) - renderCamPos.y,
					static_cast<float>(position.z) - renderCamPos.z,
					1.0f);
			};

			// Pass 1 & 2: Frustum-cull in front-to-back spiral order (maximizes GPU Early-Z),
			// batch Pass 1 by TerrainLayer to avoid per-subchunk shader/material/texture switches,
			// and keep Pass 2 (Blend & Water) sorted back-to-front.
			auto tRender0 = std::chrono::high_resolution_clock::now();
			static const TerrainLayer opaqueLayers[] = {
				TerrainLayer::Opaque,
				TerrainLayer::OpaqueSeasons,
				TerrainLayer::DoubleSide,
				TerrainLayer::Alpha,
				TerrainLayer::AlphaSingleSide,
				TerrainLayer::AlphaSeasons
			};
			constexpr int kNumOpaqueLayers = static_cast<int>(sizeof(opaqueLayers) / sizeof(opaqueLayers[0]));
			struct TransparentChunkRef {
				RenderChunk* chunk;
				float distSq;
			};
			static std::array<std::vector<RenderChunk*>, kNumOpaqueLayers> opaqueBuckets;
			static std::vector<TransparentChunkRef> transparentChunks;
			for (int l = 0; l < kNumOpaqueLayers; ++l) {
				opaqueBuckets[l].clear();
			}
			transparentChunks.clear();

			struct CachedRenderColumn {
				RenderChunk** subChunks;
				int d2;
			};
			static std::vector<CachedRenderColumn> sCachedRenderCols;
			static ChunkPos sCachedColsCamChunk(INT_MAX, INT_MAX);
			static int sCachedColsRenderDist = -1;
			static uint64_t sCachedColsRcGen = 0;
			if (camChunkPos != sCachedColsCamChunk ||
			    activeRenderDist != sCachedColsRenderDist ||
			    rcGeneration != sCachedColsRcGen) {
				sCachedRenderCols.clear();
				sCachedRenderCols.reserve(renderOffsets.size());
				for (const auto& off : renderOffsets) {
					ChunkPos pos(camChunkPos.x + off.dx, camChunkPos.z + off.dz);
					auto rcIt = rc.find(pos);
					if (rcIt != rc.end()) {
						sCachedRenderCols.push_back({ rcIt->second, off.d2 });
					}
				}
				sCachedColsCamChunk = camChunkPos;
				sCachedColsRenderDist = activeRenderDist;
				sCachedColsRcGen = rcGeneration;
			}

			const bool camOutdoors = (renderCamPos.y >= 62.0f) &&
				(c.getRegion().getBrightness(c.getRegion().getDimension().getSkyLightLayer(), camBlockPos) > 0);

			for (const auto& col : sCachedRenderCols) {
				RenderChunk** subChunks = col.subChunks;
				const bool farHoriz = (col.d2 > 4);
				for (int i = 0; i < 8; i++) {
					RenderChunk* chunkPart = subChunks[i];
					if (!chunkPart || chunkPart->isEmpty() || !chunkPart->isReady()) continue;
					if (camOutdoors && farHoriz && !chunkPart->isSkyLit()) continue;
					const float extent = chunkPart->isGreedyClusterOwner() ? 32.0f : 16.0f;
					if (!frustum.isBoxVisible(chunkPart->getPosition(), 2.0f, renderCamPos, extent, extent)) continue;

					if (chunkPart->hasLayer(TerrainLayer::Blend) || chunkPart->hasLayer(TerrainLayer::Water)) {
						BlockPos center = chunkPart->getCenter();
						float dx = static_cast<float>(center.x) - renderCamPos.x;
						float dy = static_cast<float>(center.y) - renderCamPos.y;
						float dz = static_cast<float>(center.z) - renderCamPos.z;
						transparentChunks.push_back({ chunkPart, dx * dx + dy * dy + dz * dz });
					}

					for (int l = 0; l < kNumOpaqueLayers; ++l) {
						if (chunkPart->hasLayer(opaqueLayers[l])) {
							opaqueBuckets[l].push_back(chunkPart);
						}
					}
				}
			}

			RenderChunk::beginTerrainPass();
			for (int l = 0; l < kNumOpaqueLayers; ++l) {
				const auto& bucket = opaqueBuckets[l];
				if (bucket.empty()) continue;
				RenderChunkRenderParameters renderParams;
				renderParams.layer = opaqueLayers[l];
				renderParams.forceUnsorted = false;
				renderParams.forceFog = true;
				for (RenderChunk* chunkPart : bucket) {
					setChunkOrigin(chunkPart->getPosition());
					chunkPart->render(renderParams, currentFrame);
				}
			}

			// Pass 2: Transparent layers (Blend & Water) sorted back-to-front
			std::sort(transparentChunks.begin(), transparentChunks.end(),
			          [](const TransparentChunkRef& a, const TransparentChunkRef& b) {
				          return a.distSq > b.distSq;
			          });

			for (const auto& entry : transparentChunks) {
				RenderChunk* chunkPart = entry.chunk;
				setChunkOrigin(chunkPart->getPosition());

				if (chunkPart->hasLayer(TerrainLayer::Blend)) {
					RenderChunkRenderParameters renderParams;
					renderParams.layer = TerrainLayer::Blend;
					renderParams.forceUnsorted = false;
					renderParams.forceFog = true;
					chunkPart->render(renderParams, currentFrame);
				}
				if (chunkPart->hasLayer(TerrainLayer::Water)) {
					RenderChunkRenderParameters renderParams;
					renderParams.layer = TerrainLayer::Water;
					renderParams.forceUnsorted = false;
					renderParams.forceFog = true;
					chunkPart->render(renderParams, currentFrame);
				}
			}
			RenderChunk::endTerrainPass();
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);

			ChunkPerfMetrics::instance.renderUs.fetch_add(
				static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - tRender0).count()),
				std::memory_order_relaxed);

			glActiveTexture(GL_TEXTURE0);

			renderWorldItemEntities(worldItems, c.getRegion(), skyInfo.sunFactor, static_cast<float>(c.yaw) - 90.0f, worldItemTickAccum / 0.05f, projection, view);

			mobManager.render(c.getRegion(), projection, view, skyInfo.sunFactor);

			// Render particles
			glUseProgram(0);
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glLoadMatrixf(&projection[0][0]);
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			glLoadMatrixf(&view[0][0]);
			ParticleSystem::render(c.yaw, c.pitch, &texgroup);
			ProjectileTntSystem::renderArrows();
			ProjectileTntSystem::renderTnts();
			PaintingSystem::renderPaintings(c.getRegion());
			for (const auto& fb : sFallingBlocks) {
				glPushMatrix();
				glTranslatef(fb.x, fb.y + 0.5f, fb.z);
				GuiComponent::drawBlock3DModel(ItemInstance(fb.id, 1, fb.data), 0.95f, false);
				glPopMatrix();
			}
			glPopMatrix();
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);

			if (gGuiOptions.thirdPerson && !gGuiContext.loadingChunks) {
				float steveLight = WorldLighting::sampleWorldBrightness(
					c.getRegion(),
					BlockPos(Math::floor(c.mPos.x), Math::floor(c.mPos.y), Math::floor(c.mPos.z)),
					skyInfo.sunFactor);
				renderWorldSteve3D(c, gGuiContext, projection, view, steveLight);
			}

			if (hasTargetHit && !mobInFront && gGuiContext.state == GameState::InGame && !gGuiContext.currentScreen) {
				BlockPos hitPos = targetHit.getBlock();
				BlockID hitId = c.getRegion().getBlockID(hitPos);
				const Block* hitBlock = hitId > 0 ? Block::mBlocks[hitId] : nullptr;
				if (hitBlock) {
					AABB shapeBuf;
					AABB localShape = hitBlock->getVisualShape(c.getRegion(), hitPos, shapeBuf);
					if (localShape.isEmpty()) {
						localShape.set(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
					}
					drawBlockSelectionOutline(hitPos, localShape, projection, view);
					if (isMining && miningPos == hitPos && miningProgress > 0.01f) {
						drawBlockBreakOverlay(hitPos, localShape, miningProgress, projection, view);
					}
				}
			}

			if (!gGuiOptions.thirdPerson && gGuiContext.state == GameState::InGame && !gGuiContext.currentScreen) {
				gGuiContext.ensureInventoryInitialized();
				int slotIdx = std::max(0, std::min(8, gGuiContext.selectedHotbarSlot));
				float handLight = WorldLighting::sampleWorldBrightness(
					c.getRegion(),
					BlockPos(Math::floor(c.mPos.x), Math::floor(c.mPos.y), Math::floor(c.mPos.z)),
					skyInfo.sunFactor);
				renderFirstPersonHand(c, gGuiContext.inventorySlots[static_cast<size_t>(slotIdx)], aspect, gGuiOptions.viewBobbing, handLight);
			}
			// 1:1 ItemInHandRenderer::renderFire (first-person fire overlay)
			if (!gGuiOptions.thirdPerson && gGuiContext.state == GameState::InGame && gPlayerFireTicks > 0) {
				GuiTextureInfo fireTex = GuiComponent::loadTexture("textures/blocks/fire_0.png");
				if (fireTex.id != 0) {
					glMatrixMode(GL_PROJECTION);
					glPushMatrix();
					glLoadIdentity();
					glm::mat4 fireProj = glm::perspective(glm::radians(70.0f), aspect, 0.05f, 10.0f);
					glLoadMatrixf(&fireProj[0][0]);
					glMatrixMode(GL_MODELVIEW);
					glPushMatrix();
					glLoadIdentity();
					glEnable(GL_TEXTURE_2D);
					glBindTexture(GL_TEXTURE_2D, fireTex.id);
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					glDisable(GL_DEPTH_TEST);
					glDisable(GL_CULL_FACE);
					glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
					int frame = (static_cast<int>(glfwGetTime() * 20.0) & 31);
					float v0 = static_cast<float>(frame) / 32.0f;
					float v1 = static_cast<float>(frame + 1) / 32.0f;
					for (int i = 0; i < 2; ++i) {
						float v = (i == 0) ? 1.0f : -1.0f;
						glPushMatrix();
						glTranslatef(v * 0.24f, -0.3f, 0.0f);
						glRotatef(-v * 10.0f, 0.0f, 1.0f, 0.0f);
						glBegin(GL_QUADS);
						glTexCoord2f(1.0f, v1); glVertex3f(-0.5f, -0.5f, -0.5f);
						glTexCoord2f(0.0f, v1); glVertex3f( 0.5f, -0.5f, -0.5f);
						glTexCoord2f(0.0f, v0); glVertex3f( 0.5f,  0.5f, -0.5f);
						glTexCoord2f(1.0f, v0); glVertex3f(-0.5f,  0.5f, -0.5f);
						glEnd();
						glPopMatrix();
					}
					glEnable(GL_CULL_FACE);
					glEnable(GL_DEPTH_TEST);
					glDisable(GL_BLEND);
					glPopMatrix();
					glMatrixMode(GL_PROJECTION);
					glPopMatrix();
					glMatrixMode(GL_MODELVIEW);
				}
			}
		} // end if (renderWorld || gGuiContext.loadingChunks)

		if (gGuiContext.loadingChunks) {
			if (commandContext.dim != nullptr) {
				ChunkPos camCp(Math::floor(c.mPos.x / 16.0f), Math::floor(c.mPos.z / 16.0f));
				ChunkSource& chunkSrc = commandContext.dim->getChunkSource();
				const int renderDistOpt = 2 * (1 + std::max(0, std::min(3, gGuiOptions.renderDistance)));
				const int rDist = std::min(renderDistOpt, 2);
				const int lDist = rDist + 1;
				int loadedCount = 0;
				int litCount = 0;
				int meshedCount = 0;
				int innerPending = 0;
				const int totalLightChunks = (2 * lDist + 1) * (2 * lDist + 1);
				const int totalRenderChunks = (2 * rDist + 1) * (2 * rDist + 1);

				for (int dx = -lDist; dx <= lDist; ++dx) {
					for (int dz = -lDist; dz <= lDist; ++dz) {
						ChunkPos cp(camCp.x + dx, camCp.z + dz);
						auto* lc = chunkSrc.getExistingChunk(cp);
						if (lc && lc->getState() == ChunkState::Loaded) {
							++loadedCount;
						}
						if (litChunks.find(cp) != litChunks.end()) {
							++litCount;
						}
						if (std::abs(dx) <= rDist && std::abs(dz) <= rDist) {
							auto rcIt = rc.find(cp);
							if (rcIt != rc.end()) {
								bool colReady = true;
								for (int s = 0; s < 8; ++s) {
									if (!rcIt->second[s] || rcIt->second[s]->isPending()) {
										colReady = false;
										break;
									}
								}
								if (colReady) {
									++meshedCount;
								} else {
									++innerPending;
								}
							} else {
								++innerPending;
							}
						}
					}
				}

				bool allLoadedAndLit = (loadedCount == totalLightChunks && litCount == totalLightChunks);
				bool allMeshed = (meshedCount == totalRenderChunks);
				if (allLoadedAndLit && allMeshed && innerPending == 0) {
					gGuiContext.loadingProgress = 1.0f;
				} else {
					float loadFrac = static_cast<float>(loadedCount) / static_cast<float>(totalLightChunks);
					float litFrac = static_cast<float>(litCount) / static_cast<float>(totalLightChunks);
					float meshFrac = static_cast<float>(meshedCount) / static_cast<float>(totalRenderChunks);
					float weighted = 0.30f * loadFrac + 0.30f * litFrac + 0.40f * meshFrac;
					gGuiContext.loadingProgress = std::min(0.99f, std::max(0.0f, weighted));
				}
			} else {
				gGuiContext.loadingProgress = 0.0f;
			}
		}

		// 2D GUI / HUD Pass
		glUseProgram(0);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0.0, static_cast<double>(gGuiContext.guiWidth), static_cast<double>(gGuiContext.guiHeight), 0.0, -1000.0, 1000.0);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		if (gGuiContext.state == GameState::InGame) {
			gGuiHud.render(0.0f, gGuiContext.guiWidth, gGuiContext.guiHeight, &gGuiOptions, &gGuiFont, &gGuiContext);
			static float fpsAccumTime = 0.0f;
			static int fpsFrameCount = 0;
			static int displayedFps = 0;
			fpsAccumTime += std::max(0.0f, deltaTime);
			++fpsFrameCount;
			if (fpsAccumTime >= 0.25f) {
				displayedFps = static_cast<int>(std::round(static_cast<float>(fpsFrameCount) / fpsAccumTime));
				fpsAccumTime = 0.0f;
				fpsFrameCount = 0;
			}
			if (gGuiOptions.showFps && !gGuiContext.loadingChunks) {
				char fpsBuf[32];
				snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %d", displayedFps);
				gGuiFont.drawShadow(fpsBuf, 4, 4, 0xFFFFFFFF);
			}
		}
		if (gGuiContext.currentScreen) {
			int guiMouseX = static_cast<int>(glinput.mouseXPos) * gGuiContext.guiWidth / std::max(1, screenx);
			int guiMouseY = static_cast<int>(glinput.mouseYPos) * gGuiContext.guiHeight / std::max(1, screeny);
			gGuiContext.currentScreen->tick();
			if (gGuiContext.currentScreen) {
				gGuiContext.currentScreen->render(guiMouseX, guiMouseY, 0.0f);
			}
		}

		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		if (glinput.NewKeyDown(GLFW_KEY_U) && gGuiContext.state == GameState::InGame && gGuiContext.currentScreen == nullptr)
		{
			gManualCursorUnlock = !gManualCursorUnlock;
		}
		syncCursorLock();
		glinput.Clear();

		auto frameCpuEndTP = std::chrono::high_resolution_clock::now();
		double frameCpuMs = std::chrono::duration<double, std::milli>(frameCpuEndTP - frameStartTP).count();

		static int lastFpsLimitMode = -1;
		if (gGuiOptions.fpsLimit != lastFpsLimitMode) {
			lastFpsLimitMode = gGuiOptions.fpsLimit;
			glfwSwapInterval(lastFpsLimitMode == 0 ? 1 : 0);
		}

		if (const char* fpsLog = std::getenv("MCPE_FPS_LOG")) {
			static int sFpsFrames = 0;
			static double sFpsStart = 0.0;
			if (fpsLog[0] != '\0' && gGuiContext.hasWorldLoaded && !gGuiContext.loadingChunks) {
				if (sFpsFrames == 0) sFpsStart = glfwGetTime();
				++sFpsFrames;
				double now = glfwGetTime();
				if (now - sFpsStart >= 5.0) {
					LOGI("AVG_FPS=%.1f (%d frames in %.2fs, greedy=%d)\n",
					     static_cast<double>(sFpsFrames) / (now - sFpsStart),
					     sFpsFrames, now - sFpsStart, gGuiOptions.greedyMeshing ? 1 : 0);
					sFpsFrames = 0;
					sFpsStart = now;
				}
			}
		}
		if (const char* testTime = std::getenv("MCPE_TEST_TIME")) {
			if (std::string(testTime) == "day") {
				gGuiContext.worldTime = 6000;
				if (commandContext.level) commandContext.level->setTime(6000);
			}
		}
		if (const char* testCam = std::getenv("MCPE_TEST_CAM")) {
			float tx = 0, ty = 80, tz = 0, tyaw = 0, tpitch = 20;
			if (std::sscanf(testCam, "%f,%f,%f,%f,%f", &tx, &ty, &tz, &tyaw, &tpitch) >= 3) {
				c.setPositionAndLook(tx, ty, tz, tyaw, tpitch);
			}
		}
		if (const char* ssPath = std::getenv("MCPE_SCREENSHOT")) {
			static int sInWorldFrames = 0;
			static bool sSavedScreenshot = false;
			if (!sSavedScreenshot && gGuiContext.hasWorldLoaded && !gGuiContext.loadingChunks) {
				if (++sInWorldFrames >= 180) {
					sSavedScreenshot = true;
					int fbw = 0, fbh = 0;
					glfwGetFramebufferSize(window, &fbw, &fbh);
					if (fbw > 0 && fbh > 0) {
						std::vector<unsigned char> px(static_cast<size_t>(fbw) * static_cast<size_t>(fbh) * 3);
						glPixelStorei(GL_PACK_ALIGNMENT, 1);
						glReadPixels(0, 0, fbw, fbh, GL_RGB, GL_UNSIGNED_BYTE, px.data());
						std::ofstream out(ssPath, std::ios::binary);
						if (out) {
							out << "P6\n" << fbw << " " << fbh << "\n255\n";
							for (int y = fbh - 1; y >= 0; --y) {
								out.write(reinterpret_cast<const char*>(px.data() + static_cast<size_t>(y) * fbw * 3), static_cast<std::streamsize>(fbw * 3));
							}
							LOGI("SCREENSHOT_SAVED=%s (%dx%d)\n", ssPath, fbw, fbh);
						}
					}
				}
			}
		}

		glfwSwapBuffers(window);
		glfwPollEvents();
		if (gGuiOptions.fpsLimit >= 1 && gGuiOptions.fpsLimit <= 4) {
			static const int kFpsTargets[] = { 0, 30, 60, 120, 240 };
			const auto targetDuration = std::chrono::microseconds(1000000 / kFpsTargets[gGuiOptions.fpsLimit]);
			const auto targetTP = frameStartTP + targetDuration;
			auto nowTP = std::chrono::high_resolution_clock::now();
			if (nowTP + std::chrono::milliseconds(1) < targetTP) {
				std::this_thread::sleep_until(targetTP - std::chrono::milliseconds(1));
			}
			while (std::chrono::high_resolution_clock::now() < targetTP) {
				std::this_thread::yield();
			}
		}
		auto frameWallEndTP = std::chrono::high_resolution_clock::now();
		double frameWallMs = std::chrono::duration<double, std::milli>(frameWallEndTP - frameStartTP).count();

		if (gBenchChunksMode) {
			if (benchPhase == 0) {
				++benchInitFrames;
				benchCpuFrameMs.push_back(frameCpuMs);
				benchWallFrameMs.push_back(frameWallMs);
				if (!gGuiContext.loadingChunks && gGuiContext.state == GameState::InGame && hasInitialSpawn) {
					benchInitWallMs = std::chrono::duration<double, std::milli>(frameWallEndTP - benchInitStart).count();
					printBenchMetrics("INIT", benchInitFrames, benchInitWallMs, benchCpuFrameMs, benchWallFrameMs);
					benchCpuFrameMs.clear();
					benchWallFrameMs.clear();
					ChunkPerfMetrics::instance.reset();
					mce::Mesh::resetStats();
					c.mIsFlying = true;
					c.mNoGravity = true;
					c.setPositionAndLook(c.mPos.x, 90.0f, c.mPos.z, 0.0, -15.0);
					benchStartX = c.mPos.x;
					benchStartY = c.mPos.y;
					benchStartZ = c.mPos.z;
					benchWalkStart = std::chrono::high_resolution_clock::now();
					benchPhase = 1;
					benchWalkStep = 0;
				}
			} else if (benchPhase == 1) {
				benchCpuFrameMs.push_back(frameCpuMs);
				benchWallFrameMs.push_back(frameWallMs);
				++benchWalkStep;
				if (benchWalkStep <= 120) {
					float nx = benchStartX + static_cast<float>(benchWalkStep) * 1.4f;
					c.teleportTo(nx, benchStartY, benchStartZ);
				} else if (benchWalkStep <= 160) {
					// Oscillate back and forth across a chunk border to test hysteresis & edge thrashing
					int cx = Math::floor((benchStartX + 120.0f * 1.4f) / 16.0f);
					float borderX = static_cast<float>(cx * 16);
					float offset = ((benchWalkStep % 4) < 2) ? -1.5f : 1.5f;
					c.teleportTo(borderX + offset, benchStartY, benchStartZ);
				} else if (benchWalkStep == 165 && commandContext.dim) {
					// Break and place a block to measure local update/lighting/rebuild cost
					auto tEdit0 = std::chrono::high_resolution_clock::now();
					BlockPos editPos(Math::floor(c.mPos.x) + 2, std::max(4, Math::floor(c.mPos.y) - 20), Math::floor(c.mPos.z));
					BlockSource& reg = c.getRegion();
					if (reg.hasBlock(editPos)) {
						reg.setBlockAndData(editPos, FullBlock(Block::mStone->mID, 0), Block::UPDATE_ALL);
						updateLightingAndRebuildChunks(reg, editPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
						reg.removeBlock(editPos);
						updateLightingAndRebuildChunks(reg, editPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
					}
					double editMs = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tEdit0).count();
					printf("[BENCH_EDIT] blockPlaceAndBreakMs=%.2f\n", editMs);
					fflush(stdout);
				} else if (benchWalkStep >= gBenchWalkFrames) {
					benchWalkWallMs = std::chrono::duration<double, std::milli>(frameWallEndTP - benchWalkStart).count();
					printBenchMetrics("WALK", benchWalkStep, benchWalkWallMs, benchCpuFrameMs, benchWallFrameMs);
					benchPhase = 2;
					gGuiContext.shouldQuit = true;
				}
			}
		}
		{
			std::string cur = gGuiOptions.serialize();
			if (cur != lastSavedOptions) { gGuiOptions.save("options.txt"); lastSavedOptions = cur; }
		}
	}
	gGuiOptions.save("options.txt");
	saveActiveWorld();
	closeActiveWorld();
	gActiveDirtyMeshChunks = nullptr;
	WorkerPool::setMainThreadPumpActive(false);
	SoundEngine::shutdown();
	commandContext.running = false;
	mce::Mesh::shutdownPool();
	glfwTerminate();
}