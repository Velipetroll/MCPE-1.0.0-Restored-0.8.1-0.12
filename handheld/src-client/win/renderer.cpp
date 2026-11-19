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
#include "world/level/block/Block.h"
#include "world/level/block/BedBlock.h"
#include "world/level/block/DoorBlock.h"
#include "world/level/block/TrapDoorBlock.h"
#include "world/level/block/FenceGateBlock.h"
#include "world/level/block/TorchBlock.h"
#include "world/level/GameType.h"
#include "world/level/material/Material.h"
#include "util/Random.h"
#include "stb_image.h"


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
		}
	}
	if (action == GLFW_PRESS && gGuiContext.currentScreen) {
		gGuiContext.currentScreen->keyPressed(key);
	}
	if (key == GLFW_KEY_0 && action == GLFW_PRESS)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	if (key == GLFW_KEY_1 && action == GLFW_PRESS && gGuiContext.state != GameState::InGame)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	bool pressed = (action != GLFW_RELEASE);

	if (pressed != glinput.lookup_table[key])
	{
		glinput.last_table[key] = pressed ? 1 : -1;
		glinput.lookup_table[key] = pressed;
	}
}

static float gBowPullTime = 0.0f;
static bool gBowPulling = false;
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
		mTickAccum = 0.0f;
		mPosDelta = Vec3(0.0f, 0.0f, 0.0f);
		mFallDistance = 0.0f;
	}

	bool isInWater()
	{
		int bx = Math::floor(mCurrPos.x);
		int by = Math::floor(mBB.min.y + 0.3f);
		int bz = Math::floor(mCurrPos.z);
		return getRegion().getMaterial(bx, by, bz).isType(MaterialType::Water);
	}

	void checkFallDamage(float dy, bool onGround)
	{
		if (mIsFlying || gGuiContext.isCreative || isInWater()) {
			mFallDistance = 0.0f;
			return;
		}
		if (onGround) {
			if (mFallDistance > 0.0f) {
				int damage = static_cast<int>(std::ceil(mFallDistance - 3.0f));
				if (damage > 0) {
					gGuiContext.applyDamage(damage, false);
				}
				mFallDistance = 0.0f;
			}
		} else if (dy < 0.0f) {
			mFallDistance -= dy;
		}
	}

	bool isSneaking()
	{
		return glinput.KeyDown(GLFW_KEY_LEFT_SHIFT) || glinput.KeyDown(GLFW_KEY_RIGHT_SHIFT);
	}

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

		if (gGuiContext.isCreative) {
			if (mOnGround && mIsFlying) {
				mIsFlying = false;
			}
		} else {
			mIsFlying = false;
		}
		mNoGravity = mIsFlying;

		bool inWater = !mIsFlying && isInWater();
		bool usingItemSlow = gBowPulling || (gItemUseSlowTimer > 0.0f);
		bool sprinting = !usingItemSlow && (glinput.KeyDown(GLFW_KEY_LEFT_CONTROL) || glinput.KeyDown(GLFW_KEY_RIGHT_CONTROL));
		bool sneaking = !mIsFlying && isSneaking();

		// LocalPlayer::aiStep + KeyboardInput::tick
		float moveStrafe = 0.0f;
		float moveForward = 0.0f;
		if (glinput.KeyDown(GLFW_KEY_W)) moveForward += 1.0f;
		if (glinput.KeyDown(GLFW_KEY_S)) moveForward -= 1.0f;
		if (glinput.KeyDown(GLFW_KEY_A)) moveStrafe -= 1.0f;
		if (glinput.KeyDown(GLFW_KEY_D)) moveStrafe += 1.0f;

		if (sneaking) {
			moveStrafe *= 0.3f;
			moveForward *= 0.3f;
		}
		if (usingItemSlow) {
			moveStrafe *= 0.35f;
			moveForward *= 0.35f;
		}

		float flyIdleMod = 1.0f;
		if (mIsFlying) {
			float maxIn = std::max(std::abs(moveStrafe), std::abs(moveForward));
			if (maxIn < 0.01f) {
				flyIdleMod = 0.75f;
			}
			if (glinput.KeyDown(GLFW_KEY_SPACE)) mPosDelta.y += 0.15f;
			if (isSneaking()) mPosDelta.y -= 0.15f;
		}

		// Mob::aiStep jump + input damping
		if (glinput.KeyDown(GLFW_KEY_SPACE) && !mIsFlying) {
			if (inWater) {
				mPosDelta.y += 0.04f;
			} else if (mOnGround) {
				mPosDelta.y = 0.42f;
				mOnGround = false;
				if (!gGuiContext.isCreative) {
					gGuiContext.exhaustion += sprinting ? 0.6f : 0.25f;
				}
			}
		}
		moveStrafe *= 0.98f;
		moveForward *= 0.98f;

		float prevX = mCurrPos.x;
		float prevZ = mCurrPos.z;

		// Player::travel / Mob::travel (1:1 MCPE 0.8.1)
		if (inWater) {
			float startY = mCurrFeetY;
			moveRelative(moveStrafe, moveForward, 0.02f);
			move(mPosDelta);
			mPosDelta.x *= 0.8f;
			mPosDelta.y = mPosDelta.y * 0.8f - 0.02f;
			mPosDelta.z *= 0.8f;
			mFallDistance = 0.0f;
			if (mHorizontalCollision &&
			    mRegion->fetchCollisionShapes(getRegion(),
			        mBB.cloneMove(Vec3(mPosDelta.x, mPosDelta.y + 0.6f - (mCurrFeetY - startY), mPosDelta.z)),
			        nullptr, false, nullptr).empty()) {
				mPosDelta.y = 0.3f;
			}
		} else if (mIsFlying) {
			float savedY = mPosDelta.y;
			float airAcc = sprinting ? 0.075f : 0.05f;
			moveRelative(moveStrafe, moveForward, airAcc);
			move(mPosDelta);
			mPosDelta.y = savedY * 0.6f;
			float drag = 0.91f * flyIdleMod;
			mPosDelta.x *= drag;
			mPosDelta.z *= drag;
		} else {
			float friction = 0.91f;
			if (mOnGround) {
				friction = 0.6f * 0.91f;
				BlockID t = getRegion().getBlockID(
					Math::floor(mCurrPos.x),
					Math::floor(mBB.min.y - 0.5f),
					Math::floor(mCurrPos.z)
				);
				if (t > 0 && Block::mBlocks[t]) {
					friction = Block::mBlocks[t]->getFriction() * 0.91f;
				}
			}
			float baseSpeed = 0.1f; // Player::getBaseSpeed() * Player::getWalkingSpeedModifier()
			if (sprinting) baseSpeed *= 1.3f;
			float acc = mOnGround
				? (baseSpeed * (0.16277f / (friction * friction * friction)))
				: (sprinting ? 0.026f : 0.02f);
			moveRelative(moveStrafe, moveForward, acc);

			float postFriction = 0.91f;
			if (mOnGround) {
				postFriction = 0.6f * 0.91f;
				BlockID t = getRegion().getBlockID(
					Math::floor(mCurrPos.x),
					Math::floor(mBB.min.y - 0.5f),
					Math::floor(mCurrPos.z)
				);
				if (t > 0 && Block::mBlocks[t]) {
					postFriction = Block::mBlocks[t]->getFriction() * 0.91f;
				}
			}
			move(mPosDelta);
			mPosDelta.y = (mPosDelta.y - 0.08f) * 0.98f;
			float horizDrag = postFriction * flyIdleMod;
			mPosDelta.x *= horizDrag;
			mPosDelta.z *= horizDrag;
		}

		float stepDx = mCurrPos.x - prevX;
		float stepDz = mCurrPos.z - prevZ;
		float horizStep = std::sqrt(stepDx * stepDx + stepDz * stepDz);
		if (mOnGround && !mIsFlying) {
			mCurrWalkDist += horizStep * 0.6f;
		}
		if (!gGuiContext.isCreative && !mIsFlying && horizStep > 0.0005f) {
			float moveRate = sprinting ? 0.14f : (inWater ? 0.09f : 0.07f);
			gGuiContext.exhaustion += horizStep * moveRate;
		}
		float targetBob = (mOnGround && !mIsFlying && horizStep > 0.001f) ? 1.0f : 0.0f;
		mCurrSmoothBob += (targetBob - mCurrSmoothBob) * 0.4f;

		if (mBB.min.y < -16.0f && !gGuiContext.isCreative && gGuiContext.health > 0) {
			gGuiContext.health = 0;
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

			float sensitivity = 0.1f;
			xoffset *= sensitivity;
			yoffset *= sensitivity;

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

		if (gGuiContext.isCreative) {
			if (glinput.NewKeyDown(GLFW_KEY_SPACE)) {
				double now = glfwGetTime();
				if (mLastSpacePressTime > 0.0 && (now - mLastSpacePressTime) < 0.32) {
					mIsFlying = !mIsFlying;
					mPosDelta.y = 0.0f;
					mLastSpacePressTime = -1.0;
				} else {
					mLastSpacePressTime = now;
				}
			}
		} else {
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
		mPos = glm::mix(mPrevPos, mCurrPos, alpha);
		mRenderFeetY = mPrevFeetY + (mCurrFeetY - mPrevFeetY) * alpha;
		mWalkDist = mPrevWalkDist + (mCurrWalkDist - mPrevWalkDist) * alpha;
		mSmoothBob = mPrevSmoothBob + (mCurrSmoothBob - mPrevSmoothBob) * alpha;
	}
};




void Camera::moveBBs(const Vec3& pos) {
	mBB.move(pos);
	for (auto& bb : mSubBBs) {
		bb.move(pos);
	}
}

void Camera::move(const Vec3& posIn) {
	Vec3 speed(posIn);
	// stupid check
	if (std::abs(speed.x) > 500 || std::abs(speed.y) > 500 || std::abs(speed.z) > 500) {
		ALOGE(LOG_AREA_ENTITY, "Entity moved WAAAY too fast!\n");
		speed.x = speed.y = speed.z = 0;
	}

	if (mNoPhysics) {
		moveBBs(speed);
		mCurrFeetY = mBB.min.y;
		mCurrPos.x = (mBB.min.x + mBB.max.x) / 2.0f;
		mCurrPos.y = mBB.min.y + mHeightOffset - mSlideOffset.y;
		mCurrPos.z = (mBB.min.z + mBB.max.z) / 2.0f;
		mPos = mCurrPos;
		mRenderFeetY = mCurrFeetY;
		return;
	}

	//ScopedProfile("move");

	// add all intersecting aabb's to one box
	std::vector<AABB>& preIntersectingAabbs = mRegion->fetchCollisionShapes(getRegion(), mBB, nullptr, false, nullptr);//&mTerrainSurfaceOffset
	AABB allIntersects = AABB::EMPTY;
	for(auto& it : preIntersectingAabbs) {
		if(allIntersects.isEmpty()) {
			allIntersects = it;
		}
		else {
			allIntersects.merge(it);
		}
	}

	if (mIsStuckInWeb) {
		mIsStuckInWeb = false;

		speed.x *= .25f;
		speed.y *= .05f;
		speed.z *= .25f;
		mPosDelta.x = .0f;
		mPosDelta.y = .0f;
		mPosDelta.z = .0f;
	}

	float xaOrg = speed.x;
	float yaOrg = speed.y;
	float zaOrg = speed.z;

	AABB bbOrg = mBB;

	bool sneaking = !mIsFlying && mOnGround && isSneaking();

	if (sneaking) {
		float d = 0.05f;
		
		//bias the step size a bit to make sure we reach the bottom of the step
		float maxUpStep = mMaxAutoStep * 1.01f;

		while (speed.x != 0 && mRegion->fetchCollisionShapes(getRegion(), mBB.cloneMove(Vec3(speed.x, -maxUpStep, 0)), nullptr, false, nullptr).empty()) {
			if (speed.x < d && speed.x >= -d) {
				speed.x = 0;
			}
			else if (speed.x > 0) {
				speed.x -= d;
			}
			else {
				speed.x += d;
			}

			xaOrg = speed.x;
		}

		while (speed.z != 0 && mRegion->fetchCollisionShapes(getRegion(), mBB.cloneMove(Vec3(0, -maxUpStep, speed.z)), nullptr, false, nullptr).empty()) {
			if (speed.z < d && speed.z >= -d) {
				speed.z = 0;
			}
			else if (speed.z > 0) {
				speed.z -= d;
			}
			else {
				speed.z += d;
			}

			zaOrg = speed.z;
		}

		while (speed.x != 0 && speed.z != 0 && mRegion->fetchCollisionShapes(getRegion(), mBB.cloneMove(Vec3(speed.x, -maxUpStep, speed.z)), nullptr, false, nullptr).empty()) {
			if (speed.x < d && speed.x >= -d) {
				speed.x = 0;
			}
			else if (speed.x > 0) {
				speed.x -= d;
			}
			else {
				speed.x += d;
			}

			if (speed.z < d && speed.z >= -d) {
				speed.z = 0;
			}
			else if (speed.z > 0) {
				speed.z -= d;
			}
			else {
				speed.z += d;
			}

			xaOrg = speed.x;
			zaOrg = speed.z;
		}

	}
	Vec3 orgSpeed = speed;

	AABB intersectTestBox = mBB.expanded(speed);
	std::vector<AABB>& aABBs = mRegion->fetchCollisionShapes(getRegion(), intersectTestBox, nullptr, false, nullptr);

	//LOGE(
	//	"MOVE pos=(%f,%f,%f) speed=(%f,%f,%f) BB=(%f,%f,%f)->(%f,%f,%f) boxes=%zu\n",
	//	mPos.x, mPos.y, mPos.z,
	//	speed.x, speed.y, speed.z,
	//	mBB.min.x, mBB.min.y, mBB.min.z,
	//	mBB.max.x, mBB.max.y, mBB.max.z,
	//	aABBs.size()
	//);

	bool collidableWasNear = mCollidableMobNear;

	//	Because some entities can collide and interact differently with other entities (I.E. minecarts stacking) we want to go ahead and
	//	have the entity do it's own test for collision so we're not unnecessarily testing for collisions.
	/*if (isStackable()) {
		testForEntityStacking(*mRegion, intersectTestBox, aABBs);
	}

	else if (mCollidableMobNear) {
		testForCollidableMobs(*mRegion, intersectTestBox, aABBs);
		mCollidableMobNear = false;
	}*/

	// Use oneway collision for all blocks if the entity is within any oneway blocks, or they take advantage of one way physics intentionally,
	// or if they're stuck, to prevent thrashing around trying to resolve
	bool onewayCollide = false;//!mOnewayPhysicsBlocks.empty() || mUsesOneWayCollision || mStuckInCollider;

	// This is broken into 3 passes for the sake of smooth movement. Neighboring blocks on a smooth surface align the movement vector with the surface if possible
	// If all done at once, entities can catch on corners vertical edges while trying to move horizontally along a surface, or similarly when brushing up against walls
	Vec3 steps[3] = { Vec3(0.0f, speed.y, 0.0f), Vec3(speed.x, 0.0f, 0.0f), Vec3(0.0f, 0.0f, speed.z) };
	Vec3 penetration(0.0f);

	for (int j = 0; j < 3; ++j) {
		Vec3& step = steps[j];
		float& pen = penetration[j];
		for (auto& bb : aABBs) {
			float tempPenetration;
			step = bb.clipCollide(mBB, step, onewayCollide, &tempPenetration);
			pen = std::max(pen, tempPenetration);
		}
		moveBBs(step);
	}

	float totalPenetration = penetration.x + penetration.y + penetration.z;
	float epsilon = FLT_EPSILON;
	// If there was no penetration, then we're not stuck anymore
	if(mStuckInCollider && totalPenetration <= epsilon) {
		mStuckInCollider = false;
	}
	// if there was penetration last frame we might have fixed an error, but if it's also this frame, then we're probably stuck between two objects
	else if(!mStuckInCollider && mLastPenetration > epsilon && totalPenetration > epsilon) {
		mStuckInCollider = true;
	}
	mLastPenetration = totalPenetration;

	// Combine the collective move we just did back into here for later use
	speed = steps[0] + steps[1] + steps[2];
	bool og = mOnGround || (yaOrg != speed.y && yaOrg < 0);

	// If we ran into a vertical wall and we're on the ground
	if (mMaxAutoStep > 0 && og && ((xaOrg != speed.x) || (zaOrg != speed.z))) {
		// Try stepping up onto what's in front of us using our foot size, storing old velocity in case we can't do it
		// This is done by first trying to move up vertically, then over horizontally, in the hopes that the first move cleared the obstacle
		float xaN = speed.x;
		float yaN = speed.y;
		float zaN = speed.z;

		// Use the unadjusted speed before collision as we're trying again
		Vec3 speedX(xaOrg, 0.0f, 0.0f);
		Vec3 speedY(0.0f, mMaxAutoStep, 0.0f);
		Vec3 speedZ(0.0f, 0.0f, zaOrg);

		// Store original bounding box in case we want to revert
		AABB normal = mBB;
		std::vector<AABB> norm;
		for (auto& bb : mSubBBs) {
			norm.push_back(bb);
		}
		mBB.set(bbOrg);
		// Fetch collision shapes using original speed, this will exclude colliders above head, we'll check those at the end
		aABBs = mRegion->fetchCollisionShapes(getRegion(), mBB.expanded(orgSpeed), nullptr, false, nullptr);

		if (collidableWasNear) {
			//testForCollidableMobs(*mRegion, intersectTestBox, aABBs);
		}

		// Try to step up over the obstacle
		// Even though the motion is only on y, we don't want to use clipYCollide because that will force penetration resolution along the y axis, which may not be the smallest penetrating axis
		for (auto& bb : aABBs) {
			speedY = bb.clipCollide(mBB, speedY, onewayCollide);
		}
		moveBBs(speedY);

		// Now that we've attempted the vertical step, do the horizontal ones, which will hopefully not be impeded by the same obstacle as before
		for (auto& bb : aABBs) {
			speedX = bb.clipCollide(mBB, speedX, onewayCollide);
		}
		moveBBs(speedX);

		for (auto& bb : aABBs) {
			speedZ = bb.clipCollide(mBB, speedZ, onewayCollide);
		}
		moveBBs(speedZ);

		// Undo as much y axis movement as possible, we're climping up, not jumping up
		Vec3 reverseSpeedY = -speedY;
		for (auto& bb : aABBs) {
			reverseSpeedY = bb.clipCollide(mBB, reverseSpeedY, onewayCollide);
		}
		moveBBs(reverseSpeedY);
		speedY += reverseSpeedY;

		// Store the movement we just did in here for later use if we decide to keep it
		speed = speedX + speedY + speedZ;
		// See if we hit our head on the ceiling, meaning we shouldn't do step up.
		// This makes getting head stuck in ceiling possible if moving into a block lower than footsize at a speed larger than you bounding box width. This shouldn't be achievable by standard means
		// Not expanding bb by anything because we just want to see if we ended up in a good spot
		bool hitCeiling = !mRegion->fetchCollisionShapes(getRegion(), mBB, nullptr, false, nullptr).empty();

		// If our original horizontal square velocity was higher without trying to step over the obstacle, or our head ended up in the ceiling, revert back to that for the sake of smoother movement
		if (hitCeiling || xaN * xaN + zaN * zaN >= speed.x * speed.x + speed.z * speed.z) {
			speed.x = xaN;
			speed.y = yaN;
			speed.z = zaN;
			mBB.set(normal);
			for (size_t i = 0; i < mSubBBs.size(); ++i) {
				mSubBBs[i] = norm[i];
			}
		}
		// Else our vertical step cleared the obstacle and have more horizontal velocity than before, so keep the changes
		// Don't do mSlideOffset on client side except for players because it can cause tunneling for entities
		else if(true) {
			// Add an artificial downward offset that will ease up over time to smooth out the sudden change in height caused by this step
			mSlideOffset.y += speedY.y;
		}
	}

	{
		//ScopedProfile("rest");

		mCurrFeetY = mBB.min.y;
		mCurrPos.x = (mBB.min.x + mBB.max.x) / 2.0f;
		mCurrPos.y = mBB.min.y + mHeightOffset - mSlideOffset.y - ((!mIsFlying && isSneaking()) ? 0.08f : 0.0f);
		mCurrPos.z = (mBB.min.z + mBB.max.z) / 2.0f;
		mPos = mCurrPos;
		mRenderFeetY = mCurrFeetY;

		mHorizontalCollision = (xaOrg != speed.x) || (zaOrg != speed.z);
		mVerticalCollision = (yaOrg != speed.y);

		mOnGround = (yaOrg != speed.y && yaOrg < 0) || (mOnGround && yaOrg == speed.y && yaOrg == 0);
		mCollision = mHorizontalCollision || mVerticalCollision;

		checkFallDamage(speed.y, mOnGround);

		if (xaOrg != speed.x) {
			mPosDelta.x = 0;
			//onBlockCollision(0);
		}

		if (yaOrg != speed.y) {
			mPosDelta.y = 0;
		}

		if (zaOrg != speed.z) {
			mPosDelta.z = 0;
			//onBlockCollision(2);
		}

		//_playMovementSound(mVerticalCollision);

		mSlideOffset.y *= 0.4f;

		//checkBlockCollisions();
	}
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
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, scroll_callback);

	gGuiFont.init("font/default8.png");
	gGuiContext.font = &gGuiFont;
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

void rebuildSubChunkAt(const ChunkPos& cp, int subY,
                       std::unordered_map<ChunkPos, RenderChunk*[8]>& rc,
                       std::unique_ptr<RenderChunkBuilder>& chunkBuilder,
                       const Vec3& camPos) {
	if (subY < 0 || subY >= 8) return;
	auto it = rc.find(cp);
	if (it == rc.end() || it->second[subY] == nullptr) return;
	RenderChunk* chunkPart = it->second[subY];
	chunkPart->setDirty(Tick(0), true);
	chunkPart->startRebuild(std::move(chunkBuilder));
	chunkPart->rebuild(false, true);
	chunkBuilder = chunkPart->endRebuild(camPos);
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
	std::unordered_set<ChunkPos> touchedChunks;
	WorldLighting::updateLocalLighting(region, pos, 15, &touchedChunks);

	// Always rebuild the block's own subchunk & immediate touching subchunks
	rebuildChunksForBlock(pos, rc, chunkBuilder, camPos);

	ChunkPos baseCp(pos);
	int baseSubY = pos.y >> 4;
	int minSubY = std::max(0, (pos.y - 15) >> 4);
	int maxSubY = std::min(7, (pos.y + 15) >> 4);
	for (const ChunkPos& cp : touchedChunks) {
		for (int sy = minSubY; sy <= maxSubY; ++sy) {
			if (cp == baseCp && sy == baseSubY) continue;
			rebuildSubChunkAt(cp, sy, rc, chunkBuilder, camPos);
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
	if (progress <= 0.01f) return;
	float p = std::max(0.0f, std::min(1.0f, progress));
	const float eps = 0.004f;
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
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glColor4f(0.05f, 0.05f, 0.05f, 0.12f + p * 0.55f);

	glBegin(GL_QUADS);
	// Bottom (-Y) & Top (+Y)
	glVertex3f(minX, minY, minZ); glVertex3f(maxX, minY, minZ); glVertex3f(maxX, minY, maxZ); glVertex3f(minX, minY, maxZ);
	glVertex3f(minX, maxY, minZ); glVertex3f(minX, maxY, maxZ); glVertex3f(maxX, maxY, maxZ); glVertex3f(maxX, maxY, minZ);
	// North (-Z) & South (+Z)
	glVertex3f(minX, minY, minZ); glVertex3f(minX, maxY, minZ); glVertex3f(maxX, maxY, minZ); glVertex3f(maxX, minY, minZ);
	glVertex3f(minX, minY, maxZ); glVertex3f(maxX, minY, maxZ); glVertex3f(maxX, maxY, maxZ); glVertex3f(minX, maxY, maxZ);
	// West (-X) & East (+X)
	glVertex3f(minX, minY, minZ); glVertex3f(minX, minY, maxZ); glVertex3f(minX, maxY, maxZ); glVertex3f(minX, maxY, minZ);
	glVertex3f(maxX, minY, minZ); glVertex3f(maxX, maxY, minZ); glVertex3f(maxX, maxY, maxZ); glVertex3f(maxX, minY, maxZ);
	glEnd();

	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
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
	if (block == Block::mObsidian) {
		reqLevel = 3;
	} else if (isBlockOneOf({ Block::mGoldOre, Block::mDiamondOre, Block::mEmeraldOre, Block::mRedStoneOre, Block::mLitRedStoneOre })) {
		reqLevel = 2;
	} else if (isBlockOneOf({ Block::mIronOre, Block::mLapisOre, Block::mLapisBlock })) {
		reqLevel = 1;
	} else if (!mat.isAlwaysDestroyable() || mat.isType(MaterialType::Stone) || mat.isType(MaterialType::Metal)) {
		reqLevel = 0;
	}

	if (reqLevel < 0) {
		out.canHarvest = true;
	} else {
		out.canHarvest = (tool.type == ToolType::Pickaxe && tool.harvestLevel >= reqLevel);
	}

	if (hardness <= 0.0001f) {
		out.breakTime = 0.0f;
	} else {
		float speedMult = out.effectiveTool ? tool.speedMultiplier : 1.0f;
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
	Vec3 vel;
	float age = 0.0f;
	float pickupDelay = 0.0f;
	float spinPhase = 0.0f;
	bool onGround = false;
};

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
	ent.vel = vel;
	ent.pickupDelay = pickupDelay;
	ent.spinPhase = itemRng.nextFloat() * 6.283185f;
	items.push_back(std::move(ent));
}

void updateWorldItemEntities(std::vector<WorldItemEntity>& items,
                             float dt,
                             BlockSource& region,
                             const AABB& playerBB,
                             GuiContext& guiCtx,
                             bool allowPickup) {
	dt = std::min(std::max(dt, 0.0001f), 0.05f);
	float tickScale = dt / 0.05f;
	AABB pickupBox = playerBB.expanded(Vec3(0.85f, 0.5f, 0.85f));
	const float halfW = 0.125f;
	const float height = 0.25f;

	for (size_t i = 0; i < items.size(); ) {
		WorldItemEntity& e = items[i];
		e.age += dt;
		if (e.pickupDelay > 0.0f) {
			e.pickupDelay = std::max(0.0f, e.pickupDelay - dt);
		}
		if (e.age > 300.0f || e.pos.y < -32.0f || e.item.isNull() || e.item.getStackSize() <= 0) {
			items[i] = std::move(items.back());
			items.pop_back();
			continue;
		}

		// Gravity
		e.vel.y -= 16.0f * dt;
		if (e.vel.y < -25.0f) e.vel.y = -25.0f;

		Vec3 moveStep = e.vel * dt;
		AABB itemBB(e.pos.x - halfW, e.pos.y, e.pos.z - halfW,
		            e.pos.x + halfW, e.pos.y + height, e.pos.z + halfW);
		std::vector<AABB>& aABBs = region.fetchCollisionShapes(region, itemBB.expanded(moveStep), nullptr, false, nullptr);

		float xaOrg = moveStep.x;
		float yaOrg = moveStep.y;
		float zaOrg = moveStep.z;

		Vec3 steps[3] = { Vec3(0.0f, moveStep.y, 0.0f), Vec3(moveStep.x, 0.0f, 0.0f), Vec3(0.0f, 0.0f, moveStep.z) };
		for (int axis = 0; axis < 3; ++axis) {
			for (const auto& bb : aABBs) {
				steps[axis] = bb.clipCollide(itemBB, steps[axis], false);
			}
			itemBB.move(steps[axis]);
		}

		e.pos.x = (itemBB.min.x + itemBB.max.x) * 0.5f;
		e.pos.y = itemBB.min.y;
		e.pos.z = (itemBB.min.z + itemBB.max.z) * 0.5f;

		if (steps[1].x != xaOrg) e.vel.x = 0.0f;
		if (steps[2].z != zaOrg) e.vel.z = 0.0f;
		if (steps[0].y != yaOrg) {
			e.onGround = (yaOrg < 0.0f);
			e.vel.y = 0.0f;
		} else {
			e.onGround = false;
		}

		float drag = std::pow(e.onGround ? 0.65f : 0.98f, tickScale);
		e.vel.x *= drag;
		e.vel.z *= drag;

		if (allowPickup && e.pickupDelay <= 0.0f && itemBB.intersects(pickupBox)) {
			ItemInstance rem = guiCtx.addItemToInventoryRemainder(e.item);
			if (rem.isNull() || rem.getStackSize() <= 0) {
				items[i] = std::move(items.back());
				items.pop_back();
				continue;
			}
			e.item = rem;
		}
		++i;
	}
}

void renderWorldItemEntities(const std::vector<WorldItemEntity>& items,
                             BlockSource& region,
                             float sunFactor,
                             const glm::mat4& projection,
                             const glm::mat4& view) {
	if (items.empty()) return;

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

	auto drawCubeQuad = [](const std::string& tex, float shade,
	                       float ax, float ay, float az,
	                       float bx, float by, float bz,
	                       float cx, float cy, float cz,
	                       float dx, float dy, float dz,
	                       float v0, float v1) {
		if (tex.empty() || !GuiComponent::bindTexture(tex)) return;
		glColor4f(shade, shade, shade, 1.0f);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, v1); glVertex3f(ax, ay, az);
		glTexCoord2f(1.0f, v1); glVertex3f(bx, by, bz);
		glTexCoord2f(1.0f, v0); glVertex3f(cx, cy, cz);
		glTexCoord2f(0.0f, v0); glVertex3f(dx, dy, dz);
		glEnd();
	};

	for (const auto& e : items) {
		if (e.item.isNull()) continue;
		float itemLight = WorldLighting::sampleWorldBrightness(
			region, BlockPos(Math::floor(e.pos.x), Math::floor(e.pos.y + 0.25f), Math::floor(e.pos.z)), sunFactor);
		float bob = std::sin(e.age * 3.0f + e.spinPhase) * 0.07f;
		float yawDeg = e.age * 70.0f + e.spinPhase * 57.29578f;

		glPushMatrix();
		glTranslatef(e.pos.x, e.pos.y + 0.18f + bob, e.pos.z);
		glRotatef(yawDeg, 0.0f, 1.0f, 0.0f);

		BlockRenderInfo blockInfo = getBlockRenderInfo(e.item);
		if (blockInfo.is3D) {
			float y0 = -0.5f;
			float y1 = blockInfo.isSlab ? 0.0f : 0.5f;
			float vTop = blockInfo.isSlab ? 0.5f : 0.0f;

			glScalef(0.25f, 0.25f, 0.25f);
			if (blockInfo.isTransparent) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			} else {
				glDisable(GL_BLEND);
			}

			// Top (+Y) & Bottom (-Y)
			drawCubeQuad(blockInfo.topTex, 1.0f * itemLight, -0.5f, y1, 0.5f, 0.5f, y1, 0.5f, 0.5f, y1, -0.5f, -0.5f, y1, -0.5f, 0.0f, 1.0f);
			drawCubeQuad(blockInfo.topTex, 0.5f * itemLight, -0.5f, y0, -0.5f, 0.5f, y0, -0.5f, 0.5f, y0, 0.5f, -0.5f, y0, 0.5f, 0.0f, 1.0f);
			// Front (+Z) & Back (-Z)
			drawCubeQuad(blockInfo.frontTex, 0.8f * itemLight, -0.5f, y0, 0.5f, 0.5f, y0, 0.5f, 0.5f, y1, 0.5f, -0.5f, y1, 0.5f, vTop, 1.0f);
			drawCubeQuad(blockInfo.frontTex, 0.8f * itemLight, 0.5f, y0, -0.5f, -0.5f, y0, -0.5f, -0.5f, y1, -0.5f, 0.5f, y1, -0.5f, vTop, 1.0f);
			// Right (+X) & Left (-X)
			drawCubeQuad(blockInfo.sideTex, 0.65f * itemLight, 0.5f, y0, 0.5f, 0.5f, y0, -0.5f, 0.5f, y1, -0.5f, 0.5f, y1, 0.5f, vTop, 1.0f);
			drawCubeQuad(blockInfo.sideTex, 0.65f * itemLight, -0.5f, y0, -0.5f, -0.5f, y0, 0.5f, -0.5f, y1, 0.5f, -0.5f, y1, -0.5f, vTop, 1.0f);
		} else {
			std::string tex = getItemTexturePath(e.item);
			glDisable(GL_BLEND);
			glScalef(0.36f, 0.36f, 0.36f);
			if (!tex.empty() && GuiComponent::bindTexture(tex)) {
				glColor4f(itemLight, itemLight, itemLight, 1.0f);
				glBegin(GL_QUADS);
				glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, -0.5f, 0.0f);
				glTexCoord2f(1.0f, 1.0f); glVertex3f(0.5f, -0.5f, 0.0f);
				glTexCoord2f(1.0f, 0.0f); glVertex3f(0.5f, 0.5f, 0.0f);
				glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, 0.5f, 0.0f);
				glEnd();
				if (e.item.getStackSize() > 1) {
					glBegin(GL_QUADS);
					glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.46f, -0.46f, -0.05f);
					glTexCoord2f(1.0f, 1.0f); glVertex3f(0.54f, -0.46f, -0.05f);
					glTexCoord2f(1.0f, 0.0f); glVertex3f(0.54f, 0.54f, -0.05f);
					glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.46f, 0.54f, -0.05f);
					glEnd();
				}
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

	// Sky dome disc above horizon
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glBegin(GL_TRIANGLE_FAN);
	glColor3f(sky.skyColor.r, sky.skyColor.g, sky.skyColor.b);
	glVertex3f(0.0f, 16.0f, 0.0f);
	for (int i = 0; i <= 16; ++i) {
		float ang = static_cast<float>(i) * (6.2831853f / 16.0f);
		glVertex3f(std::cos(ang) * 200.0f, 16.0f, std::sin(ang) * 200.0f);
	}
	glEnd();

	// Horizon mist gradient band (fog color at horizon -> sky color up high)
	glBegin(GL_QUAD_STRIP);
	for (int i = 0; i <= 24; ++i) {
		float ang = static_cast<float>(i) * (6.2831853f / 24.0f);
		float cx = std::cos(ang) * 180.0f;
		float cz = std::sin(ang) * 180.0f;
		glColor3f(sky.fogColor.r, sky.fogColor.g, sky.fogColor.b);
		glVertex3f(cx, -12.0f, cz);
		glColor3f(sky.skyColor.r, sky.skyColor.g, sky.skyColor.b);
		glVertex3f(cx, 28.0f, cz);
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
	int w = 0, h = 0, ch = 0;
	stbi_set_flip_vertically_on_load(0);
	unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &ch, 4);
	if (data && w > 0 && h > 0 && w <= 64 && h <= 64) {
		auto isOpaque = [&](int px, int py) -> bool {
			if (px < 0 || py < 0 || px >= w || py >= h) return false;
			return data[(py * w + px) * 4 + 3] >= 128;
		};
		const float halfZ = 0.03125f;
		const float fw = static_cast<float>(w);
		const float fh = static_cast<float>(h);
		for (int py = 0; py < h; ++py) {
			float y1 = 0.5f - static_cast<float>(py) / fh;
			float y0 = 0.5f - static_cast<float>(py + 1) / fh;
			float v = (static_cast<float>(py) + 0.5f) / fh;
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

void drawTexturedSprite3D(const std::string& tex, float shade = 1.0f) {
	if (tex.empty() || !GuiComponent::bindTexture(tex)) return;
	const float halfZ = 0.03125f;

	glBegin(GL_QUADS);
	// Front face (+Z)
	glColor4f(shade, shade, shade, 1.0f);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, -0.5f, halfZ);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(0.5f, -0.5f, halfZ);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(0.5f, 0.5f, halfZ);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, 0.5f, halfZ);

	// Back face (-Z)
	glColor4f(shade * 0.85f, shade * 0.85f, shade * 0.85f, 1.0f);
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f, -0.5f, -halfZ);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, 0.5f, -halfZ);
	glTexCoord2f(1.0f, 0.0f); glVertex3f(0.5f, 0.5f, -halfZ);
	glTexCoord2f(1.0f, 1.0f); glVertex3f(0.5f, -0.5f, -halfZ);

	// Extruded pixel edges
	const auto& edges = getExtrudedEdges(tex);
	for (const auto& q : edges) {
		float c = shade * q.shade;
		glColor4f(c, c, c, 1.0f);
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
	glLoadIdentity();

	float bobX = 0.0f;
	float bobY = 0.0f;
	if (viewBobbing) {
		float phase = c.mWalkDist * 3.14159265f * 1.15f;
		bobX = -std::cos(phase) * 0.045f * c.mSmoothBob;
		bobY = std::abs(std::sin(phase)) * 0.045f * c.mSmoothBob;
	}

	float sw = c.mSwingProgress;
	float swSqSin = std::sin(sw * sw * 3.14159265f);
	float swSin = std::sin(sw * 3.14159265f);
	float swSqrtSin = std::sin(std::sqrt(sw) * 3.14159265f);
	float swDoubleSin = std::sin(std::sqrt(sw) * 2.0f * 3.14159265f);

	if (held.isNull() || held.getStackSize() <= 0) {
		if (GuiComponent::bindTexture("textures/entity/steve.png")) {
			glTranslatef(-swSqrtSin * 0.30f, swDoubleSin * 0.40f, -swSin * 0.40f);
			glTranslatef(0.64f + bobX, -0.60f - bobY, -0.72f);
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
		glTranslatef(0.56f + bobX, -0.52f - bobY, -0.72f);
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

		BlockRenderInfo blockInfo = getBlockRenderInfo(held);
		if (blockInfo.is3D) {
			drawTexturedCube3D(blockInfo, lightShade);
		} else {
			std::string tex = getItemTexturePath(held);
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
			drawTexturedSprite3D(tex, lightShade);
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

void renderWorldSteve3D(const Camera& c,
                        const GuiContext& guiCtx,
                        const glm::mat4& projection,
                        const glm::mat4& view) {
	if (!GuiComponent::bindTexture("textures/entity/steve.png")) return;

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

	const float scale = 1.8f / 32.0f;
	float limbSwing = std::sin(c.mWalkDist * 3.14159265f * 1.15f) * 45.0f * c.mSmoothBob;
	float sw = c.mSwingProgress;
	float swingAngle = (sw > 0.0f) ? (-std::sin(std::sqrt(sw) * 3.14159265f) * 85.0f) : 0.0f;
	bool sneaking = !c.mIsFlying && (glinput.KeyDown(GLFW_KEY_LEFT_SHIFT) || glinput.KeyDown(GLFW_KEY_RIGHT_SHIFT));

	glTranslatef(c.mPos.x, c.mRenderFeetY, c.mPos.z);
	glRotatef(90.0f - static_cast<float>(c.yaw), 0.0f, 1.0f, 0.0f);
	glScalef(scale, scale, scale);
	if (sneaking) {
		glTranslatef(0.0f, -1.5f, 0.0f);
	}

	// Right Leg (x = -2, hip y = 12)
	glPushMatrix();
	glTranslatef(-2.0f, 12.0f, 0.0f);
	glRotatef(limbSwing, 1.0f, 0.0f, 0.0f);
	drawPlayerBox3D(-2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 0, 16, 4, 12, 4, 64.0f, 64.0f);
	glPopMatrix();

	// Left Leg (x = +2, hip y = 12)
	glPushMatrix();
	glTranslatef(2.0f, 12.0f, 0.0f);
	glRotatef(-limbSwing, 1.0f, 0.0f, 0.0f);
	drawPlayerBox3D(-2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 16, 48, 4, 12, 4, 64.0f, 64.0f);
	glPopMatrix();

	// Torso
	glPushMatrix();
	if (sneaking) glRotatef(18.0f, 1.0f, 0.0f, 0.0f);
	drawPlayerBox3D(-4.0f, 12.0f, -2.0f, 8.0f, 12.0f, 4.0f, 16, 16, 8, 12, 4, 64.0f, 64.0f);

	// Right Arm + Held Item
	int slotIdx = std::max(0, std::min(8, guiCtx.selectedHotbarSlot));
	const ItemInstance& held = guiCtx.inventorySlots[static_cast<size_t>(slotIdx)];
	bool hasHeld = !held.isNull() && held.getStackSize() > 0;
	glPushMatrix();
	glTranslatef(-6.0f, 22.0f, 0.0f);
	glRotatef((hasHeld ? (-limbSwing * 0.5f - 18.0f) : -limbSwing) + swingAngle, 1.0f, 0.0f, 0.0f);
	glRotatef(4.0f, 0.0f, 0.0f, -1.0f);
	drawPlayerBox3D(-2.0f, -10.0f, -2.0f, 4.0f, 12.0f, 4.0f, 40, 16, 4, 12, 4, 64.0f, 64.0f);

	if (hasHeld) {
		glPushMatrix();
		BlockRenderInfo blockInfo = getBlockRenderInfo(held);
		if (blockInfo.is3D) {
			glTranslatef(0.0f, -10.0f, 3.0f);
			glRotatef(20.0f, 1.0f, 0.0f, 0.0f);
			glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
			glScalef(6.0f, 6.0f, 6.0f);
			drawTexturedCube3D(blockInfo, 1.0f);
		} else {
			std::string tex = getItemTexturePath(held);
			bool isTool = held.getItem() && (held.getItem()->isHandEquipped() || held.getItem()->isMirroredArt());
			glTranslatef(0.0f, -9.5f, 2.5f);
			if (isTool) {
				glScalef(10.0f, 10.0f, 10.0f);
				glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
				glRotatef(-45.0f, 0.0f, 0.0f, 1.0f);
				glTranslatef(0.30f, 0.30f, 0.0f);
			} else {
				glScalef(6.0f, 6.0f, 6.0f);
				glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
			}
			drawTexturedSprite3D(tex, 1.0f);
		}
		glPopMatrix();
	}
	glPopMatrix();

	// Left Arm
	glPushMatrix();
	glTranslatef(6.0f, 22.0f, 0.0f);
	glRotatef(limbSwing, 1.0f, 0.0f, 0.0f);
	glRotatef(4.0f, 0.0f, 0.0f, 1.0f);
	drawPlayerBox3D(-2.0f, -10.0f, -2.0f, 4.0f, 12.0f, 4.0f, 32, 48, 4, 12, 4, 64.0f, 64.0f);
	glPopMatrix();

	// Head
	glPushMatrix();
	glTranslatef(0.0f, 24.0f, 0.0f);
	glRotatef(static_cast<float>(-c.pitch), 1.0f, 0.0f, 0.0f);
	drawPlayerBox3D(-4.0f, 0.0f, -4.0f, 8.0f, 8.0f, 8.0f, 0, 0, 8, 8, 8, 64.0f, 64.0f);

	// Armor slot 0: Helmet
	std::string helmTex = getArmorTexturePath(guiCtx.armorSlots[0], false);
	if (!helmTex.empty() && GuiComponent::bindTexture(helmTex)) {
		drawPlayerBox3D(-4.5f, -0.5f, -4.5f, 9.0f, 9.0f, 9.0f, 0, 0, 8, 8, 8, 64.0f, 32.0f);
	}
	glPopMatrix();

	// Armor slot 1: Chestplate
	std::string chestTex = getArmorTexturePath(guiCtx.armorSlots[1], false);
	if (!chestTex.empty() && GuiComponent::bindTexture(chestTex)) {
		drawPlayerBox3D(-4.5f, 11.5f, -2.5f, 9.0f, 13.0f, 5.0f, 16, 16, 8, 12, 4, 64.0f, 32.0f);

		glPushMatrix();
		glTranslatef(-6.0f, 22.0f, 0.0f);
		glRotatef(-limbSwing + swingAngle, 1.0f, 0.0f, 0.0f);
		glRotatef(4.0f, 0.0f, 0.0f, -1.0f);
		drawPlayerBox3D(-2.5f, -10.5f, -2.5f, 5.0f, 13.0f, 5.0f, 40, 16, 4, 12, 4, 64.0f, 32.0f);
		glPopMatrix();

		glPushMatrix();
		glTranslatef(6.0f, 22.0f, 0.0f);
		glRotatef(limbSwing, 1.0f, 0.0f, 0.0f);
		glRotatef(4.0f, 0.0f, 0.0f, 1.0f);
		drawPlayerBox3D(-2.5f, -10.5f, -2.5f, 5.0f, 13.0f, 5.0f, 40, 16, 4, 12, 4, 64.0f, 32.0f);
		glPopMatrix();
	}
	glPopMatrix(); // end torso tilt

	// Armor slot 2: Leggings
	std::string legsTex = getArmorTexturePath(guiCtx.armorSlots[2], true);
	if (!legsTex.empty() && GuiComponent::bindTexture(legsTex)) {
		drawPlayerBox3D(-4.25f, 11.75f, -2.25f, 8.5f, 12.5f, 4.5f, 16, 16, 8, 12, 4, 64.0f, 32.0f);

		glPushMatrix();
		glTranslatef(-2.0f, 12.0f, 0.0f);
		glRotatef(limbSwing, 1.0f, 0.0f, 0.0f);
		drawPlayerBox3D(-2.25f, -12.25f, -2.25f, 4.5f, 12.5f, 4.5f, 0, 16, 4, 12, 4, 64.0f, 32.0f);
		glPopMatrix();

		glPushMatrix();
		glTranslatef(2.0f, 12.0f, 0.0f);
		glRotatef(-limbSwing, 1.0f, 0.0f, 0.0f);
		drawPlayerBox3D(-2.25f, -12.25f, -2.25f, 4.5f, 12.5f, 4.5f, 0, 16, 4, 12, 4, 64.0f, 32.0f);
		glPopMatrix();
	}

	// Armor slot 3: Boots
	std::string bootsTex = getArmorTexturePath(guiCtx.armorSlots[3], false);
	if (!bootsTex.empty() && GuiComponent::bindTexture(bootsTex)) {
		glPushMatrix();
		glTranslatef(-2.0f, 12.0f, 0.0f);
		glRotatef(limbSwing, 1.0f, 0.0f, 0.0f);
		drawPlayerBox3D(-2.5f, -12.4f, -2.5f, 5.0f, 12.8f, 5.0f, 0, 16, 4, 12, 4, 64.0f, 32.0f);
		glPopMatrix();

		glPushMatrix();
		glTranslatef(2.0f, 12.0f, 0.0f);
		glRotatef(-limbSwing, 1.0f, 0.0f, 0.0f);
		drawPlayerBox3D(-2.5f, -12.4f, -2.5f, 5.0f, 12.8f, 5.0f, 0, 16, 4, 12, 4, 64.0f, 32.0f);
		glPopMatrix();
	}

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

	glm::mat4 projection = glm::perspective(glm::radians(85.0f),
		(float)screenx / (float)screeny,
		0.1f, 1000.0f);

	for (auto sl : mptrs)
	{
		if (!sl || !(*sl).operator->() || !(*sl)->myShader) continue;
		(*sl)->myShader->setMatrix4("PROJ", glm::value_ptr(projection));
		(*sl)->myShader->setInt("TEXTURE_0", 0);
		(*sl)->myShader->setInt("TEXTURE_1", 1);
		(*sl)->myShader->setInt("TEXTURE_2", 2);
		(*sl)->myShader->setFloat("FAR_CHUNKS_DISTANCE", 48.0f);
		(*sl)->myShader->setFloat("RENDER_DISTANCE", 48.0f);
	}

	std::unordered_map<ChunkPos,RenderChunk*[8]> rc;

	std::unordered_set<ChunkPos> needsReRendering;
	std::unordered_set<ChunkPos> needsReRendering2;
	std::unordered_set<ChunkPos> loadedChunkSet;
	std::unordered_set<ChunkPos> litChunks;

	std::unique_ptr<RenderChunkBuilder> chunkBuilder;

	Recipes::getInstance()->init();

	std::vector<WorldItemEntity> worldItems;
	MobManager mobManager;
	float lastLightmapSun = -1.0f;
	bool hasInitialSpawn = false;
	bool isMining = false;
	BlockPos miningPos(0, -1, 0);
	float miningProgress = 0.0f;
	float hazardDamageCooldown = 0.0f;
	float eatCooldown = 0.0f;
	currentFrame = static_cast<float>(glfwGetTime());

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
			commandContext.level->saveDirtyChunks();
			for (auto* w : WorkerPool::getWorkersFor(WorkerRole::Disk)) {
				w->sync();
			}
		}
	};

	auto closeActiveWorld = [&]() {
		for (auto* w : WorkerPool::getWorkersFor(WorkerRole::Streaming)) {
			w->sync();
		}
		for (auto* w : WorkerPool::getWorkersFor(WorkerRole::Disk)) {
			w->sync();
		}
		for (auto& kv : rc) {
			for (int i = 0; i < 8; ++i) {
				delete kv.second[i];
				kv.second[i] = nullptr;
			}
		}
		rc.clear();
		needsReRendering.clear();
		needsReRendering2.clear();
		loadedChunkSet.clear();
		litChunks.clear();
		worldItems.clear();
		mobManager.clear();
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

		if (meta.hasPlayerPos) {
			c.setPositionAndLook(meta.playerX, meta.playerY, meta.playerZ, meta.playerYaw, meta.playerPitch);
			hasInitialSpawn = true;
			gGuiContext.respawnRequested = false;
		} else {
			c.setPositionAndLook(0.5f, 98.0f, 0.5f, 0.0, 0.0);
			hasInitialSpawn = false;
			gGuiContext.respawnRequested = true;
		}
		gGuiContext.hasWorldLoaded = true;
		gGuiContext.worldOpenRequested = false;
	};



	while (!glfwWindowShouldClose(window)) 
	{
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

		if (gGuiContext.state == GameState::InGame && !gGuiContext.loadingChunks &&
		    !gGuiContext.currentScreen && commandContext.level) {
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
					auto* bedChunk = commandContext.dim->getChunkSource().getOrLoadChunk(bedCp, ChunkSource::LoadMode::None);
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
					auto* spawnChunk = commandContext.dim->getChunkSource().getOrLoadChunk(ChunkPos(0, 0), ChunkSource::LoadMode::None);
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
					float kLen = std::sqrt(knockDir.x * knockDir.x + knockDir.z * knockDir.z);
					if (kLen > 0.001f) {
						c.mPosDelta.x += (knockDir.x / kLen) * 0.34f;
						c.mPosDelta.z += (knockDir.z / kLen) * 0.34f;
					}
					c.mPosDelta.y = std::max(c.mPosDelta.y, 0.24f);
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
			if (gGuiContext.state == GameState::InGame && !gGuiContext.currentScreen) {
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

					if (hazardDamageCooldown <= 0.0f) {
						BlockSource& region = c.getRegion();
						int fx = Math::floor(c.mPos.x);
						int fy0 = Math::floor(c.mBB.min.y + 0.1f);
						int fy1 = Math::floor(c.mBB.min.y + 1.0f);
						int fz = Math::floor(c.mPos.z);
						int hazDmg = 0;
						for (int fy = fy0; fy <= fy1; ++fy) {
							for (int ox = -1; ox <= 1; ++ox) {
								for (int oz = -1; oz <= 1; ++oz) {
									BlockPos bp(fx + ox, fy, fz + oz);
									BlockID bid = region.getBlockID(bp);
									const Block* b = bid > 0 ? Block::mBlocks[bid] : nullptr;
									if (!b) continue;
									AABB blkBB(static_cast<float>(bp.x), static_cast<float>(bp.y), static_cast<float>(bp.z),
									           static_cast<float>(bp.x + 1), static_cast<float>(bp.y + 1), static_cast<float>(bp.z + 1));
									if (b->getMaterial().isType(MaterialType::Lava) && blkBB.intersects(c.mBB)) {
										hazDmg = std::max(hazDmg, 4);
									} else if ((b == Block::mFire || b == Block::mCactus) && blkBB.expanded(Vec3(0.05f, 0.05f, 0.05f)).intersects(c.mBB)) {
										hazDmg = std::max(hazDmg, 1);
									}
								}
							}
						}
						if (hazDmg > 0) {
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
				targetHit = region.clip(rayStart, rayEnd, false, false, 200, false);
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
					static Random dropRng;
					int dropId = block->getResource(dropRng, fb.data, 0);
					int dropCount = block->getResourceCount(dropRng, fb.data, 0);
					DataID dropData = (dropId == fb.id && block != Block::mFurnace && block != Block::mLitFurnace &&
					                   block != Block::mChest && block != Block::mTrappedChest) ? fb.data : 0;

					BlockPos bedSecondPos;
					bool hasBedSecond = (Block::mBed && block == Block::mBed &&
					                     Block::mBed->getSecondPart(region, hitPos, bedSecondPos) &&
					                     region.getBlockID(bedSecondPos) == Block::mBed->mID);

					if (region.removeBlock(hitPos)) {
						SoundEngine::play(SoundEngine::SND_DIG, 0.85f, 0.9f + (std::rand() % 20) * 0.01f);
						float pr = 0.55f, pg = 0.55f, pb = 0.55f;
						if (block == Block::mGrass || block == Block::mLeaves || block == Block::mLeaves2) {
							pr = 0.30f; pg = 0.65f; pb = 0.25f;
						} else if (block == Block::mDirt || block == Block::mLog || block == Block::mWoodPlanks) {
							pr = 0.55f; pg = 0.38f; pb = 0.22f;
						} else if (block == Block::mSand || block == Block::mSandStone) {
							pr = 0.85f; pg = 0.80f; pb = 0.55f;
						} else if (block == Block::mTNT) {
							pr = 0.80f; pg = 0.20f; pb = 0.15f;
						}
						ParticleSystem::spawnBlockBreak(static_cast<float>(hitPos.x), static_cast<float>(hitPos.y), static_cast<float>(hitPos.z), pr, pg, pb, &region);
						if (hasBedSecond) {
							region.removeBlock(bedSecondPos);
						}
						BlockCoordKey bkey{ hitPos.x, hitPos.y, hitPos.z };
						auto dropContainerItem = [&](const ItemInstance& ci) {
							if (ci.isNull() || ci.getStackSize() <= 0) return;
							Vec3 dpos(hitPos.x + 0.5f, hitPos.y + 0.35f, hitPos.z + 0.5f);
							Vec3 dvel((dropRng.nextFloat() - 0.5f) * 2.4f, 3.0f, (dropRng.nextFloat() - 0.5f) * 2.4f);
							spawnWorldItem(worldItems, ci, dpos, dvel, 0.35f);
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
						if (!gGuiContext.isCreative && canHarvest && dropId > 0 && dropCount > 0) {
							ItemInstance dropItem(dropId, dropCount, dropData);
							if (!dropItem.isNull()) {
								Vec3 dropPos(hitPos.x + 0.5f, hitPos.y + 0.35f, hitPos.z + 0.5f);
								Vec3 dropVel((dropRng.nextFloat() - 0.5f) * 2.4f,
								             3.2f + dropRng.nextFloat() * 1.0f,
								             (dropRng.nextFloat() - 0.5f) * 2.4f);
								spawnWorldItem(worldItems, dropItem, dropPos, dropVel, 0.35f);
							}
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
									float pr = 0.55f, pg = 0.55f, pb = 0.55f;
									if (block == Block::mGrass || block == Block::mLeaves || block == Block::mLeaves2) {
										pr = 0.30f; pg = 0.65f; pb = 0.25f;
									} else if (block == Block::mDirt || block == Block::mLog || block == Block::mWoodPlanks) {
										pr = 0.55f; pg = 0.38f; pb = 0.22f;
									} else if (block == Block::mSand || block == Block::mSandStone) {
										pr = 0.85f; pg = 0.80f; pb = 0.55f;
									} else if (block == Block::mTNT) {
										pr = 0.80f; pg = 0.20f; pb = 0.15f;
									}
									ParticleSystem::spawnBlockCrack(static_cast<float>(hitPos.x), static_cast<float>(hitPos.y), static_cast<float>(hitPos.z), targetHit.getFacing(), pr, pg, pb, &region);
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
						if (holdingBow && gBowPullTime >= 0.12f) {
							int arrowSlot = findArrowSlot();
							if (gGuiContext.isCreative || arrowSlot >= 0) {
								float power = std::min(1.0f, gBowPullTime / 1.0f);
								float speed = 12.0f + power * 26.0f;
								int dmg = 2 + static_cast<int>(power * 7.0f);
								// Match MCPE 0.8.1 Arrow(Level*, Mob*, float) spawn at eye height - 0.10, right hand 0.16
								glm::vec3 rightVec = glm::normalize(glm::cross(c.cameraFront, c.cameraUp));
								Vec3 spawnPos(c.mPos.x + rightVec.x * 0.16f + c.cameraFront.x * 0.20f,
								              c.mPos.y - 0.10f + c.cameraFront.y * 0.20f,
								              c.mPos.z + rightVec.z * 0.16f + c.cameraFront.z * 0.20f);
								ProjectileTntSystem::spawnArrow(spawnPos, Vec3(c.cameraFront.x, c.cameraFront.y, c.cameraFront.z), speed, dmg, true);
								SoundEngine::play(SoundEngine::SND_BOW, 0.9f, 0.9f + power * 0.2f);
								if (!gGuiContext.isCreative && arrowSlot >= 0) {
									ItemInstance& aSlot = gGuiContext.inventorySlots[static_cast<size_t>(arrowSlot)];
									aSlot.remove(1);
									if (aSlot.getStackSize() <= 0) aSlot.setNull();
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
							c.triggerSwing();
							handledRightClick = true;
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
						if (face < Facing::MAX) {
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
							if (isRepl(footPos) && isRepl(headPos)) {
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

					if (!handledRightClick && hasTargetHit && !held.isNull() && held.getStackSize() > 0 && held.isBlock()) {
						BlockPos hitPos = targetHit.getBlock();
						FacingID face = targetHit.getFacing();
						if (face < Facing::MAX) {
							const Block* placeBlock = held.getBlock();
							BlockPos placePos = hitPos + Facing::DIRECTION[face];
							if (placeBlock && placePos.y >= 0 && placePos.y < 128) {
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
										if (Block::mTorch && placeBlock == Block::mTorch) {
											static const DataID torchDataFromFacing[6] = {
												TorchBlock::TORCH_TOP, TorchBlock::TORCH_TOP,
												TorchBlock::TORCH_SOUTH, TorchBlock::TORCH_NORTH,
												TorchBlock::TORCH_EAST, TorchBlock::TORCH_WEST
											};
											if (face != Facing::DOWN && region.isSolidBlockingBlock(hitPos)) {
												placeData = torchDataFromFacing[face];
											} else if (region.isSolidBlockingBlock(placePos.below())) {
												placeData = TorchBlock::TORCH_TOP;
											} else {
												allowPlace = false;
											}
										} else if (placeBlock->hasProperty(BlockProperty::Trap)) {
											int tdir = 0;
											if (std::abs(c.cameraMoveFront.z) >= std::abs(c.cameraMoveFront.x)) {
												tdir = (c.cameraMoveFront.z >= 0.0f) ? 3 : 2;
											} else {
												tdir = (c.cameraMoveFront.x >= 0.0f) ? 1 : 0;
											}
											placeData = 0;
											placeBlock->getBlockState(BlockState::Direction).set(placeData, tdir);
											bool upsideDown = (face == Facing::DOWN) || (face != Facing::UP && targetHit.getPos().y - hitPos.y > 0.5f);
											placeBlock->getBlockState(BlockState::UpsideDownBit).set(placeData, upsideDown ? 1 : 0);
										} else if (placeBlock->hasProperty(BlockProperty::FenceGate)) {
											int gdir = 0;
											if (std::abs(c.cameraMoveFront.z) >= std::abs(c.cameraMoveFront.x)) {
												gdir = (c.cameraMoveFront.z >= 0.0f) ? 0 : 2;
											} else {
												gdir = (c.cameraMoveFront.x >= 0.0f) ? 3 : 1;
											}
											placeData = 0;
											placeBlock->getBlockState(BlockState::Direction).set(placeData, gdir);
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

			if (!gGuiContext.loadingChunks) {
				float clampedDt = std::min(std::max(deltaTime, 0.0001f), 0.1f);
				tickFurnaces(gGuiContext, clampedDt);
				syncFurnaceBlockStates(c.getRegion(), gGuiContext, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
				bool allowPickup = (gGuiContext.state == GameState::InGame && !gGuiContext.currentScreen);
				updateWorldItemEntities(worldItems, deltaTime, c.getRegion(), c.mBB, gGuiContext, allowPickup);
				SoundEngine::setMasterVolume(gGuiOptions.soundVolume);
				ParticleSystem::tick(clampedDt, &c.getRegion());
				{
					static float lastStepWalkDist = 0.0f;
					if (c.mWalkDist < lastStepWalkDist) {
						lastStepWalkDist = c.mWalkDist;
					}
					if (c.mOnGround && !c.mIsFlying && gGuiContext.state == GameState::InGame && !gGuiContext.currentScreen) {
						if (c.mWalkDist - lastStepWalkDist >= 1.1f) {
							lastStepWalkDist = c.mWalkDist;
							SoundEngine::play(SoundEngine::SND_STEP, 0.35f, 0.9f + (std::rand() % 20) * 0.01f);
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
							a.vy -= 18.0f * clampedDt;
							a.vx *= 0.995f;
							a.vz *= 0.995f;
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
			projection = glm::perspective(glm::radians(85.0f), aspect, 0.1f, 1000.0f);
			for (auto sl : mptrs) {
				if (!sl || !(*sl).operator->() || !(*sl)->myShader) continue;
				(*sl)->myShader->setMatrix4("PROJ", glm::value_ptr(projection));
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
			} else if (gGuiOptions.viewBobbing) {
				float phase = c.mWalkDist * 3.14159265f * 1.15f;
				float bobVert = -std::abs(std::cos(phase)) * 0.045f * c.mSmoothBob;
				float bobHoriz = std::sin(phase) * 0.035f * c.mSmoothBob;
				glm::vec3 rightVec = glm::normalize(glm::cross(c.cameraFront, c.cameraUp));
				renderCamPos += rightVec * bobHoriz + c.cameraUp * bobVert;
				lookTarget = renderCamPos + c.cameraFront;
			}

			glm::mat4 view = glm::lookAt(renderCamPos, lookTarget, c.cameraUp);
			glm::mat4 camRelView = glm::lookAt(glm::vec3(0.0f), lookTarget - renderCamPos, c.cameraUp);

			for (auto sl : mptrs) {
				if (!sl || !(*sl).operator->() || !(*sl)->myShader) continue;
				(*sl)->myShader->setMatrix4("WORLDVIEW", glm::value_ptr(camRelView));
			}

			ChunkPos camChunkPos(Math::floor(c.mPos.x / 16.0f), Math::floor(c.mPos.z / 16.0f));
			int renderDistance = 3;
			int loadDistance = renderDistance + 2;
			ChunkSource& chunkSrc = commandContext.dim->getChunkSource();

			auto queueChunkAndNeighbors = [&](const ChunkPos& cp) {
				if (std::abs(cp.x - camChunkPos.x) <= renderDistance &&
				    std::abs(cp.z - camChunkPos.z) <= renderDistance) {
					needsReRendering2.insert(cp);
				}
				for (int i = 1; i < 9; i++) {
					ChunkPos np = cp + offsets[i];
					if (rc.find(np) != rc.end() ||
					    (std::abs(np.x - camChunkPos.x) <= renderDistance &&
					     std::abs(np.z - camChunkPos.z) <= renderDistance)) {
						needsReRendering2.insert(np);
					}
				}
			};

			for (int x = -loadDistance; x <= loadDistance; x++) {
				for (int z = -loadDistance; z <= loadDistance; z++) {
					ChunkPos cp(camChunkPos.x + x, camChunkPos.z + z);
					auto* lc = chunkSrc.getExistingChunk(cp);
					if (!lc) {
						lc = chunkSrc.getOrLoadChunk(cp, ChunkSource::LoadMode::Deferred);
					}
					bool inRenderRange = (std::abs(x) <= renderDistance && std::abs(z) <= renderDistance);

					if (lc && lc->getState() == ChunkState::Loaded) {
						if (loadedChunkSet.insert(cp).second) {
							queueChunkAndNeighbors(cp);
						} else if (inRenderRange && rc.find(cp) == rc.end()) {
							needsReRendering2.insert(cp);
						}
					} else {
						needsReRendering.insert(cp);
					}
				}
			}

			for (auto it = needsReRendering.begin(); it != needsReRendering.end(); ) {
				ChunkPos pos = *it;
				auto* lc = chunkSrc.getExistingChunk(pos);
				if (lc && lc->getState() == ChunkState::Loaded) {
					loadedChunkSet.insert(pos);
					queueChunkAndNeighbors(pos);
					it = needsReRendering.erase(it);
				} else {
					++it;
				}
			}

			for (auto it = needsReRendering2.begin(); it != needsReRendering2.end(); ) {
				ChunkPos pos = *it;
				if (!areChunkNeighborsLoaded(chunkSrc, pos)) {
					needsReRendering.insert(pos);
					it = needsReRendering2.erase(it);
					continue;
				}
				if (litChunks.insert(pos).second) {
					WorldLighting::propagateChunkLighting(c.getRegion(), pos);
				}
				for (int i = 0; i < 8; i++) {
					BlockPos cp(pos.x * 16, i * 16, pos.z * 16);
					if (rc[pos][i] == nullptr)
						rc[pos][i] = new RenderChunk(cp);
					else
						rc[pos][i]->setDirty(Tick(0), true);

					RenderChunk* chunkPart = rc[pos][i];
					chunkPart->setDirty(Tick(0), true);
					chunkPart->startRebuild(std::move(chunkBuilder));
					chunkPart->rebuild(false, true);
					chunkBuilder = chunkPart->endRebuild(Vec3(renderCamPos.x, renderCamPos.y, renderCamPos.z));
				}
				it = needsReRendering2.erase(it);
			}

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
			for (auto sl : mptrs) {
				if (!sl || !(*sl).operator->() || !(*sl)->myShader) continue;
				(*sl)->myShader->setVector4("FOG_COLOR", fogCol);
				(*sl)->myShader->setVector2("FOG_CONTROL", fogCtrl);
			}

			if (!camUnderwater && !camUnderlava) {
				renderSkyAndCelestials(projection, camRelView, skyInfo, gGuiContext.worldTime);
			}

			// Rebind lightmap/atlas after sky rendering
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, gLightmapTex);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, gWhiteTex);
			glActiveTexture(GL_TEXTURE0);

			auto setChunkOrigin = [&](const BlockPos& position) {
				glm::vec4 chunkOrigin(
					static_cast<float>(position.x) - renderCamPos.x,
					static_cast<float>(position.y) - renderCamPos.y,
					static_cast<float>(position.z) - renderCamPos.z,
					1.0f);
				for (auto sl : mptrs) {
					if (!sl || !(*sl).operator->() || !(*sl)->myShader) continue;
					(*sl)->myShader->setVector4("CHUNK_ORIGIN_AND_SCALE", chunkOrigin);
				}
			};

			// Ensure every RenderChunk has a sorted index buffer before Pass 1 so Pass 1 never falls back to unindexed glDrawArrays
			for (auto& lc : rc) {
				for (int i = 0; i < 8; i++) {
					RenderChunk* chunkPart = lc.second[i];
					if (!chunkPart) continue;
					chunkPart->updateFaceSortState(Vec3(renderCamPos.x, renderCamPos.y, renderCamPos.z));
					if (chunkPart->isFaceSortDirty()) {
						chunkPart->startFaceSort(make_unique<RenderChunkSorter>());
						chunkPart->faceSort();
						chunkPart->endFaceSort();
					}
				}
			}

			// Pass 1: Opaque, double-sided, and alpha-tested layers
			static const TerrainLayer opaqueLayers[] = {
				TerrainLayer::Opaque,
				TerrainLayer::OpaqueSeasons,
				TerrainLayer::DoubleSide,
				TerrainLayer::Alpha,
				TerrainLayer::AlphaSingleSide,
				TerrainLayer::AlphaSeasons
			};
			struct TransparentChunkRef {
				RenderChunk* chunk;
				float distSq;
			};
			std::vector<TransparentChunkRef> transparentChunks;

			for (auto& lc : rc) {
				for (int i = 0; i < 8; i++) {
					RenderChunk* chunkPart = lc.second[i];
					if (!chunkPart) continue;
					BlockPos position = chunkPart->getPosition();

					bool hasTransp = chunkPart->hasLayer(TerrainLayer::Blend) || chunkPart->hasLayer(TerrainLayer::Water);
					if (hasTransp) {
						BlockPos center = chunkPart->getCenter();
						float dx = static_cast<float>(center.x) - renderCamPos.x;
						float dy = static_cast<float>(center.y) - renderCamPos.y;
						float dz = static_cast<float>(center.z) - renderCamPos.z;
						transparentChunks.push_back({ chunkPart, dx * dx + dy * dy + dz * dz });
					}

					bool anyOpaque = false;
					for (TerrainLayer layer : opaqueLayers) {
						if (chunkPart->hasLayer(layer)) {
							anyOpaque = true;
							break;
						}
					}
					if (!anyOpaque) continue;

					setChunkOrigin(position);
					for (TerrainLayer layer : opaqueLayers) {
						if (chunkPart->hasLayer(layer)) {
							RenderChunkRenderParameters renderParams;
							renderParams.layer = layer;
							renderParams.forceUnsorted = false;
							renderParams.forceFog = true;
							chunkPart->render(renderParams, currentFrame);
						}
					}
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
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);

			glActiveTexture(GL_TEXTURE0);

			renderWorldItemEntities(worldItems, c.getRegion(), skyInfo.sunFactor, projection, view);

			mobManager.render(c.getRegion(), projection, view, skyInfo.sunFactor);

			// Render particles
			glUseProgram(0);
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glLoadMatrixf(&projection[0][0]);
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			glLoadMatrixf(&view[0][0]);
			ParticleSystem::render(c.yaw, c.pitch);
			ProjectileTntSystem::renderArrows();
			ProjectileTntSystem::renderTnts();
			glPopMatrix();
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);

			if (gGuiOptions.thirdPerson && !gGuiContext.loadingChunks) {
				renderWorldSteve3D(c, gGuiContext, projection, view);
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
		} // end if (renderWorld || gGuiContext.loadingChunks)

		if (gGuiContext.loadingChunks) {
			const int targetChunks = 49; // (2 * renderDistance + 1)^2 for renderDistance = 3
			gGuiContext.loadingProgress = std::min(1.0f, static_cast<float>(rc.size()) / static_cast<float>(targetChunks));
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

		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	saveActiveWorld();
	closeActiveWorld();
	SoundEngine::shutdown();
	commandContext.running = false;
	glfwTerminate();
}