#pragma once

#include "world/item/ItemInstance.h"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class Font;
class Options;
class Screen;

enum class GameState {
	StartMenu,
	InGame,
	Paused
};

struct BlockCoordKey {
	int x = 0;
	int y = 0;
	int z = 0;
	bool operator==(const BlockCoordKey& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct BlockCoordHash {
	size_t operator()(const BlockCoordKey& k) const {
		size_t h = std::hash<int>()(k.x);
		h ^= std::hash<int>()(k.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<int>()(k.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	}
};

struct ChestData {
	std::array<ItemInstance, 27> slots;
};

struct FurnaceData {
	ItemInstance input;
	ItemInstance fuel;
	ItemInstance output;
	float burnTime = 0.0f;
	float burnDuration = 0.0f;
	float cookTime = 0.0f;
};

struct GuiContext {
	Font* font = nullptr;
	Options* options = nullptr;
	GameState state = GameState::StartMenu;
	Screen* currentScreen = nullptr;
	bool shouldQuit = false;
	bool loadingChunks = false;
	float loadingProgress = 0.0f;
	struct ChatMessage {
		std::string sender;
		std::string rawText;
		std::string formattedText;
		int age = 0;
		int maxAge = 200;
	};
	std::vector<ChatMessage> chatMessages;
	std::vector<std::string> chatHistory;
	void tickChatMessages();
	int windowWidth = 854;
	int windowHeight = 480;
	int guiWidth = 427;
	int guiHeight = 240;
	int guiScale = 2;
	float deltaTime = 0.016f;

	std::array<ItemInstance, 36> inventorySlots;
	std::array<ItemInstance, 4> armorSlots;
	int selectedHotbarSlot = 0;
	bool inventoryInitialized = false;
	bool isCreative = false;
	int health = 20;
	int hunger = 20;
	float saturation = 5.0f;
	float exhaustion = 0.0f;
	float healTimer = 0.0f;
	float starveTimer = 0.0f;
	float hurtFlashTimer = 0.0f;
	bool respawnRequested = false;
	bool sleepCompleteRequested = false;
	std::vector<ItemInstance> pendingWorldDrops;

	int64_t worldTime = 1000;
	float worldTimeSubTick = 0.0f;
	bool hasBedSpawn = false;
	int bedSpawnX = 0;
	int bedSpawnY = 80;
	int bedSpawnZ = 0;
	std::string statusBannerText;
	float statusBannerTimer = 0.0f;

	std::string activeWorldDir = "./restore";
	std::string activeWorldName = "My World";
	int64_t activeWorldSeed = 0;
	bool worldOpenRequested = false;
	bool worldSaveAndCloseRequested = false;
	bool hasWorldLoaded = false;

	bool hasPlayerTransform = false;
	float playerX = 0.5f;
	float playerY = 80.0f;
	float playerZ = 0.5f;
	double playerYaw = 0.0;
	double playerPitch = 0.0;

	std::unordered_map<BlockCoordKey, ChestData, BlockCoordHash> chests;
	std::unordered_map<BlockCoordKey, FurnaceData, BlockCoordHash> furnaces;

	bool pendingTeleport = false;
	float pendingTpX = 0.0f;
	float pendingTpY = 0.0f;
	float pendingTpZ = 0.0f;
	struct PendingMobSpawn { std::string type; float x = 0.0f; float y = 0.0f; float z = 0.0f; };
	std::vector<PendingMobSpawn> pendingMobSpawns;
	struct PendingSetBlock { int x = 0; int y = 0; int z = 0; int blockId = 0; int aux = 0; };
	std::vector<PendingSetBlock> pendingSetBlocks;

	void ensureInventoryInitialized();
	ItemInstance addItemToInventoryRemainder(const ItemInstance& item);
	bool addItemToInventory(const ItemInstance& item);
	void resetSurvivalStats();
	int applyDamage(int rawDamage, bool ignoreArmor = false);
	void addChatMessage(const std::string& msg);
	void executeCommand(const std::string& cmdLine);

	void setScreen(Screen* screen);
	void startGame();
	void pauseGame();
	void resumeGame();
	void quitToTitle();
	void requestQuit();
};
