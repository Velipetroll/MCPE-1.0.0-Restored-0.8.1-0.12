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
#include "client/gui/GuiComponent.h"
#include "client/gui/WorldPersistence.h"
#include "world/crafting/CraftingRecipes.h"
#include "world/level/storage/DBStorage.h"
#include "platform/threading/WorkerPool.h"
#include "world/phys/HitResult.h"
#include "world/Facing.h"
#include "world/level/block/Block.h"
#include "world/level/material/Material.h"
#include "util/Random.h"


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



void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		if (gGuiContext.currentScreen) {
			gGuiContext.currentScreen->keyPressed(key);
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
			if (!gGuiContext.isCreative && gGuiContext.health <= 0) {
				gGuiContext.health = 20;
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
			gGuiContext.setScreen(new InventoryScreen());
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


struct CCTX
{
	std::atomic<bool> running;
	Level* level;
	Dimension* dim;
};

extern CCTX commandContext;

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
	double yaw = 0;
	double pitch = 0;

	BlockSource* mRegion;//commandContext.dim->getChunkSource()
	Level* mLevel;
	Vec3 mPosDelta;
	float mMaxAutoStep = 0.56f;
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
	double mLastSpacePressTime = -1.0;
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
	}

	Camera(Level* level, Dimension* dim) : mPosDelta(0, 0, 0)
	{
		setWorld(level, dim);
		mBB.set(-0.3f, 98.0f, -0.3f, 0.3f, 99.8f, 0.3f);
		mPos = glm::vec3(0.0f, 98.0f + mHeightOffset, 0.0f);
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
		mPosDelta = Vec3(0.0f, 0.0f, 0.0f);
		mFallDistance = 0.0f;
		mOnGround = false;
		mIsFlying = false;
		mSlideOffset = Vec3(0.0f, 0.0f, 0.0f);
	}

	bool isInWater()
	{
		int bx = Math::floor(mPos.x);
		int by = Math::floor(mBB.min.y + 0.3f);
		int bz = Math::floor(mPos.z);
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
					gGuiContext.health = std::max(0, gGuiContext.health - damage);
					if (gGuiContext.health <= 0 && !gGuiContext.currentScreen) {
						gGuiContext.setScreen(new DeathScreen());
					}
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

	void UpdateCam(float deltaTime)
	{
		float dt = std::min(std::max(deltaTime, 0.0001f), 0.05f);
		float xposIn = glinput.mouseXPos;
		float yposIn = glinput.mouseYPos;

		if (firstMouse)
		{
			lastX = xposIn;
			lastY = yposIn;
			firstMouse = false;
		}

		float xoffset = xposIn - lastX;
		float yoffset = lastY - yposIn;
		lastX = xposIn;
		lastY = yposIn;

		float sensitivity = 0.1f;
		xoffset *= sensitivity;
		yoffset *= sensitivity;

		yaw += xoffset;
		pitch += yoffset;

		if (pitch > 89.0f)
			pitch = 89.0f;
		if (pitch < -89.0f)
			pitch = -89.0f;

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
			if (mOnGround && mIsFlying) {
				mIsFlying = false;
			}
		} else {
			mIsFlying = false;
		}
		mNoGravity = mIsFlying;

		bool inWater = !mIsFlying && isInWater();
		float cameraSpeed = mOnGround ? 2.7f : (mIsFlying ? 1.15f : 0.46f);
		if (inWater) cameraSpeed = 0.85f;

		if (glinput.KeyDown(GLFW_KEY_LEFT_CONTROL) || glinput.KeyDown(GLFW_KEY_RIGHT_CONTROL)) {
			cameraSpeed *= mIsFlying ? 1.8f : 1.4f;
		}
		if (!mIsFlying && isSneaking()) {
			cameraSpeed *= 0.4f;
		}

		const float MAGIC_FRICTION_CONSTANT = 0.91f;
		const float MAGIC_FRICTION_MODIFIER = 0.6f;

		float friction = MAGIC_FRICTION_CONSTANT;
		if (inWater) {
			friction = 0.8f;
		} else if (mOnGround) {
			friction = MAGIC_FRICTION_MODIFIER * MAGIC_FRICTION_CONSTANT;

			BlockID t = getRegion().getBlockID(
				Math::floor(mPos.x),
				Math::floor(mBB.min.y - 0.1f),
				Math::floor(mPos.z)
			);

			if (t > 0 && Block::mBlocks[t]) {
				friction = Block::mBlocks[t]->getFriction() * MAGIC_FRICTION_CONSTANT;
			}
		}
		float tickScale = dt / 0.05f;
		friction = std::pow(friction, tickScale);

		if (mIsFlying) {
			if (glinput.KeyDown(GLFW_KEY_SPACE))
				mPosDelta.y += 4.5f * dt;
			if (isSneaking())
				mPosDelta.y -= 4.5f * dt;
			mPosDelta.y *= std::pow(0.6f, tickScale);
		} else if (inWater) {
			if (glinput.KeyDown(GLFW_KEY_SPACE))
				mPosDelta.y += 0.85f * dt;
			mFallDistance = 0.0f;
		} else {
			if (mOnGround && glinput.KeyDown(GLFW_KEY_SPACE)) {
				mPosDelta.y = 0.44f;
				mOnGround = false;
			}
		}

		glm::vec3 mmPos(0, 0, 0);
		if (glinput.KeyDown(GLFW_KEY_W))
			mmPos += cameraMoveFront;
		if (glinput.KeyDown(GLFW_KEY_S))
			mmPos -= cameraMoveFront;
		if (glinput.KeyDown(GLFW_KEY_A))
			mmPos -= glm::normalize(glm::cross(cameraMoveFront, cameraUp));
		if (glinput.KeyDown(GLFW_KEY_D))
			mmPos += glm::normalize(glm::cross(cameraMoveFront, cameraUp));
		if (glm::dot(mmPos, mmPos) > 0.0001f) {
			mmPos = glm::normalize(mmPos) * cameraSpeed;
			mPosDelta += Vec3(mmPos.x, mmPos.y, mmPos.z) * dt;
		}

		move(mPosDelta * dt * 20.0f);

		mPosDelta.x *= friction;
		mPosDelta.z *= friction;

		// Apply gravity after moving (matches Minecraft tick order so jumps clear 1-block ledges at any FPS)
		if (!mIsFlying) {
			if (inWater) {
				mPosDelta.y -= 0.02f * tickScale;
				mPosDelta.y *= std::pow(0.8f, tickScale);
			} else {
				mPosDelta.y -= 0.08f * tickScale;
				mPosDelta.y *= std::pow(0.98f, tickScale);
			}
		}

		if (mBB.min.y < -16.0f && !gGuiContext.isCreative && gGuiContext.health > 0 && !gGuiContext.currentScreen) {
			gGuiContext.health = 0;
			gGuiContext.setScreen(new DeathScreen());
		}
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
		mPos.x = (mBB.min.x + mBB.max.x) / 2.0f;
		mPos.y = mBB.min.y + mHeightOffset - mSlideOffset.y;
		mPos.z = (mBB.min.z + mBB.max.z) / 2.0f;
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

		mPos.x = (mBB.min.x + mBB.max.x) / 2.0f;
		mPos.y = mBB.min.y + mHeightOffset - mSlideOffset.y - ((!mIsFlying && isSneaking()) ? 0.08f : 0.0f);
		mPos.z = (mBB.min.z + mBB.max.z) / 2.0f;

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
			drawCubeQuad(blockInfo.topTex, 1.0f, -0.5f, y1, 0.5f, 0.5f, y1, 0.5f, 0.5f, y1, -0.5f, -0.5f, y1, -0.5f, 0.0f, 1.0f);
			drawCubeQuad(blockInfo.topTex, 0.5f, -0.5f, y0, -0.5f, 0.5f, y0, -0.5f, 0.5f, y0, 0.5f, -0.5f, y0, 0.5f, 0.0f, 1.0f);
			// Front (+Z) & Back (-Z)
			drawCubeQuad(blockInfo.frontTex, 0.8f, -0.5f, y0, 0.5f, 0.5f, y0, 0.5f, 0.5f, y1, 0.5f, -0.5f, y1, 0.5f, vTop, 1.0f);
			drawCubeQuad(blockInfo.frontTex, 0.8f, 0.5f, y0, -0.5f, -0.5f, y0, -0.5f, -0.5f, y1, -0.5f, 0.5f, y1, -0.5f, vTop, 1.0f);
			// Right (+X) & Left (-X)
			drawCubeQuad(blockInfo.sideTex, 0.65f, 0.5f, y0, 0.5f, 0.5f, y0, -0.5f, 0.5f, y1, -0.5f, 0.5f, y1, 0.5f, vTop, 1.0f);
			drawCubeQuad(blockInfo.sideTex, 0.65f, -0.5f, y0, -0.5f, -0.5f, y0, 0.5f, -0.5f, y1, 0.5f, -0.5f, y1, -0.5f, vTop, 1.0f);
		} else {
			std::string tex = getItemTexturePath(e.item);
			glDisable(GL_BLEND);
			glScalef(0.36f, 0.36f, 0.36f);
			if (!tex.empty() && GuiComponent::bindTexture(tex)) {
				glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
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

	mce::MaterialPtr chunkMaterial = mce::RenderMaterialGroup::switchable.getMaterial("terrain_opaque");
	mce::MaterialPtr chunk2Material = mce::RenderMaterialGroup::switchable.getMaterial("terrain_water");
	mce::MaterialPtr chunk3Material = mce::RenderMaterialGroup::switchable.getMaterial("terrain_blend");
	
	std::vector<mce::MaterialPtr*> mptrs = { &chunkMaterial, &chunk2Material, &chunk3Material };

	glm::mat4 projection = glm::perspective(glm::radians(85.0f),
		(float)screenx / (float)screeny,
		0.1f, 1000.0f);



	for (auto sl : mptrs)
	{
		(*sl)->myShader->setMatrix4("PROJ", glm::value_ptr(projection));
		(*sl)->myShader->setInt("TEXTURE_0", 0);
		(*sl)->myShader->setFloat("FAR_CHUNKS_DISTANCE", 100);
		(*sl)->myShader->setFloat("RENDER_DISTANCE", 100);
	}

	std::unordered_map<ChunkPos,RenderChunk*[8]> rc;

	//std::unordered_set<ChunkPos> renderChunkMap;

	std::unordered_set<ChunkPos> needsReRendering;
	std::unordered_set<ChunkPos> needsReRendering2;

	std::unique_ptr<RenderChunkBuilder> chunkBuilder;

	Recipes::getInstance()->init();

	std::vector<WorldItemEntity> worldItems;
	bool hasInitialSpawn = false;
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
		worldItems.clear();
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
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);//Set clear color to be brown with 0 alpha 
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);//Actually clears screen + depth buffer 

		bool renderWorld = ((gGuiContext.state == GameState::InGame) ||
		                    (gGuiContext.currentScreen && gGuiContext.currentScreen->renderGameBehind())) &&
		                   commandContext.level != nullptr && commandContext.dim != nullptr;

		if ((renderWorld || gGuiContext.loadingChunks) && commandContext.level != nullptr && commandContext.dim != nullptr) {
			if (gGuiContext.state == GameState::InGame && !gGuiContext.loadingChunks &&
			    (!hasInitialSpawn || gGuiContext.respawnRequested)) {
				auto* spawnChunk = commandContext.dim->getChunkSource().getOrLoadChunk(ChunkPos(0, 0), ChunkSource::LoadMode::None);
				if (spawnChunk && spawnChunk->getState() == ChunkState::Loaded) {
					c.spawnAtSurface();
					hasInitialSpawn = true;
					gGuiContext.respawnRequested = false;
				}
			}

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
			if (gGuiContext.state == GameState::InGame && !gGuiContext.currentScreen) {
				c.UpdateCam(deltaTime);

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

				if (hasTargetHit) {
					BlockPos hitPos = targetHit.getBlock();
					if (glinput.NewMouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
						FullBlock fb = region.getBlockAndData(hitPos);
						const Block* block = fb.id > 0 ? Block::mBlocks[fb.id] : nullptr;
						if (block && (gGuiContext.isCreative || block->getDestroySpeed() >= 0.0f)) {
							static Random dropRng;
							int dropId = block->getResource(dropRng, fb.data, 0);
							int dropCount = block->getResourceCount(dropRng, fb.data, 0);
							DataID dropData = (dropId == fb.id && block != Block::mFurnace && block != Block::mLitFurnace &&
							                   block != Block::mChest && block != Block::mTrappedChest) ? fb.data : 0;
							if (region.removeBlock(hitPos)) {
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
								if (!gGuiContext.isCreative && dropId > 0 && dropCount > 0) {
									ItemInstance dropItem(dropId, dropCount, dropData);
									if (!dropItem.isNull()) {
										Vec3 dropPos(hitPos.x + 0.5f, hitPos.y + 0.35f, hitPos.z + 0.5f);
										Vec3 dropVel((dropRng.nextFloat() - 0.5f) * 2.4f,
										             3.2f + dropRng.nextFloat() * 1.0f,
										             (dropRng.nextFloat() - 0.5f) * 2.4f);
										spawnWorldItem(worldItems, dropItem, dropPos, dropVel, 0.35f);
									}
								}
								rebuildChunksForBlock(hitPos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
								targetHit = region.clip(rayStart, rayEnd, false, false, 200, false);
								hasTargetHit = targetHit.isHit() && targetHit.isTile();
							}
						}
					} else if (glinput.NewMouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
						FullBlock fb = region.getBlockAndData(hitPos);
						if (!c.isSneaking() && openInteractiveBlockScreen(region, hitPos, fb.id, gGuiContext)) {
							// Interactive screen opened
						} else {
							gGuiContext.ensureInventoryInitialized();
							int slotIdx = std::max(0, std::min(8, gGuiContext.selectedHotbarSlot));
							ItemInstance& held = gGuiContext.inventorySlots[slotIdx];
							FacingID face = targetHit.getFacing();
							if (!held.isNull() && held.getStackSize() > 0 && held.isBlock() && face < Facing::MAX) {
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
											if (region.setBlockAndData(placePos, FullBlock(placeBlock->mID, placeData), Block::UPDATE_ALL)) {
												if (!gGuiContext.isCreative) {
													held.remove(1);
													if (held.getStackSize() <= 0) {
														held.setNull();
													}
												}
												rebuildChunksForBlock(placePos, rc, chunkBuilder, Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
											}
										}
									}
								}
							}
						}
					}
				}
			}

			if (!gGuiContext.loadingChunks) {
				float clampedDt = std::min(std::max(deltaTime, 0.0001f), 0.1f);
				tickFurnaces(gGuiContext, clampedDt);
				bool allowPickup = (gGuiContext.state == GameState::InGame && !gGuiContext.currentScreen);
				updateWorldItemEntities(worldItems, deltaTime, c.getRegion(), c.mBB, gGuiContext, allowPickup);
			}
			projection = glm::perspective(glm::radians(85.0f),
			                              static_cast<float>(screenx) / static_cast<float>(std::max(1, screeny)),
			                              0.1f, 1000.0f);
			for (auto sl : mptrs) {
				(*sl)->myShader->setMatrix4("PROJ", glm::value_ptr(projection));
			}
			glm::mat4 view = glm::lookAt(c.mPos, c.mPos + c.cameraFront, c.cameraUp);
		
		//chunkMaterial->myShader->setVector4("CHUNK_ORIGIN_AND_SCALE", glm::vec4(0, 0, 0, 1));
		for (auto sl : mptrs)
		{
			(*sl)->myShader->setMatrix4("WORLDVIEW", glm::value_ptr(view));
		}

		ChunkPos camChunkPos(c.mPos.x/16,c.mPos.z/16);
		int renderDistance = 3;
		int loadDistance = renderDistance+1;//account for post-processing requiring chunks around it to be loaded

		

		for (int x = -loadDistance; x <= loadDistance; x++)
		{
			for (int z = -loadDistance; z <= loadDistance; z++)
			{
				ChunkPos cp(camChunkPos.x+x, camChunkPos.z+z);

				if (rc.find(cp) == rc.end())
				{
					commandContext.dim->getChunkSource().getOrLoadChunk(cp, ChunkSource::LoadMode::Deferred);
					needsReRendering.insert(cp);
					
				}
			}
		}



			for (auto it = needsReRendering.begin(); it != needsReRendering.end(); )
			{


				ChunkPos pos = *it;

				if (commandContext.dim->getChunkSource().getOrLoadChunk(pos, ChunkSource::LoadMode::None)->getState() == ChunkState::Loaded)
				{
					needsReRendering2.insert(pos);
					for (int i = 1; i < 9; i++)
					{
						if(commandContext.dim->getChunkSource().getOrLoadChunk(pos + offsets[i], ChunkSource::LoadMode::None)->getState() == ChunkState::Loaded)
							needsReRendering2.insert(pos+offsets[i]);
					}
					/*
					std::cout << "Rendering chunk at " << pos.x << ", " << pos.z << " ChunkPos\n";
					//int k = rc.size();
					for (int i = 0; i < 8; i++)
					{
						BlockPos cp(pos.x * 16, i * 16, pos.z * 16);




						RenderChunk* chunkPart = new RenderChunk(cp);

						chunkPart->startRebuild(std::move(chunkBuilder));
						chunkPart->rebuild(false, true);
						chunkBuilder = chunkPart->endRebuild(Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
						//					chunkPart->startFaceSort(make_unique<RenderChunkSorter>());
						//chunkPart->faceSort();
						//chunkPart->endFaceSort();*
						rc[cp][i] = chunkPart;
					}*/
					it = needsReRendering.erase(it);
					//renderChunkMap.insert(pos);
				}
				else
				{
					it++;
				}
			}






			for (auto it = needsReRendering2.begin(); it != needsReRendering2.end(); )
			{
				ChunkPos pos = *it;

					std::cout << "Rendering chunk at " << pos.x << ", " << pos.z << " ChunkPos\n";
					//int k = rc.size();
					for (int i = 0; i < 8; i++)
					{
						BlockPos cp(pos.x * 16, i * 16, pos.z * 16);


						

						if(rc[cp][i] == nullptr)
							rc[cp][i] = new RenderChunk(cp);
						else
							rc[cp][i]->setDirty(Tick(0), true);

						RenderChunk* chunkPart = rc[cp][i];
						chunkPart->setDirty(Tick(0), true);

						chunkPart->startRebuild(std::move(chunkBuilder));
						chunkPart->rebuild(false, true);
						chunkBuilder = chunkPart->endRebuild(Vec3(c.mPos.x, c.mPos.y, c.mPos.z));
					}
					it = needsReRendering2.erase(it);
			}










		//m.render();
		for (auto & lc : rc)
		{
			for (int i = 0; i < 8; i++)
			{
				BlockPos position = lc.second[i]->getPosition();
				lc.second[i]->updateFaceSortState(Vec3(c.mPos.x, c.mPos.y, c.mPos.z));//
				if (lc.second[i]->isFaceSortDirty())
				{
					lc.second[i]->startFaceSort(make_unique<RenderChunkSorter>());
					lc.second[i]->faceSort();
					lc.second[i]->endFaceSort();
				}

				for (auto sl : mptrs)
				{
					glm::vec4 chunkOrigin(position.x, position.y, position.z, 1);
				(*sl)->myShader->setVector4("CHUNK_ORIGIN_AND_SCALE", chunkOrigin);
				}

				RenderChunkRenderParameters renderParams;
				renderParams.layer = TerrainLayer::Opaque;//Blend;//?
				renderParams.forceUnsorted = false;
				renderParams.forceFog = false;


				//shader.setMatrix4("model", glm::value_ptr(model));
				if (lc.second[i]->hasLayer(renderParams.layer))
					lc.second[i]->render(renderParams, currentFrame);
			}
		}

		for (auto& lc : rc)
		{
			for (int i = 0; i < 8; i++)
			{
				BlockPos position = lc.second[i]->getPosition();

				for (auto sl : mptrs)
				{
					glm::vec4 chunkOrigin(position.x, position.y, position.z, 1);
				(*sl)->myShader->setVector4("CHUNK_ORIGIN_AND_SCALE", chunkOrigin);
				}

				RenderChunkRenderParameters renderParams;
				renderParams.layer = TerrainLayer::Water;
				renderParams.forceUnsorted = false;
				renderParams.forceFog = false;


				//shader.setMatrix4("model", glm::value_ptr(model));
				if (lc.second[i]->hasLayer(renderParams.layer))
					lc.second[i]->render(renderParams, currentFrame);
			}
		}

		renderWorldItemEntities(worldItems, projection, view);

		if (hasTargetHit && gGuiContext.state == GameState::InGame && !gGuiContext.currentScreen) {
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
			}
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

		bool wantLock = (gGuiContext.state == GameState::InGame && gGuiContext.currentScreen == nullptr);
		if (wantLock != gCursorLocked) {
			gCursorLocked = wantLock;
			glfwSetInputMode(window, GLFW_CURSOR, gCursorLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
			if (gCursorLocked) {
				c.firstMouse = true;
			}
		}

		if (glinput.NewKeyDown(GLFW_KEY_U))
		{
			gCursorLocked = !gCursorLocked;
			glfwSetInputMode(window, GLFW_CURSOR, gCursorLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
		}
		glinput.Clear();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	saveActiveWorld();
	closeActiveWorld();
	commandContext.running = false;
	glfwTerminate();
}