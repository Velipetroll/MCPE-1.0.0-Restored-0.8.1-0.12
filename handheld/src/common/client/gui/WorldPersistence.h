#pragma once

#include "GuiContext.h"
#include <cstdint>
#include <string>

struct WorldMetadata {
	std::string name = "My World";
	int64_t seed = 0;
	bool isCreative = false;

	int64_t worldTime = 1000;
	bool hasBedSpawn = false;
	int bedSpawnX = 0;
	int bedSpawnY = 80;
	int bedSpawnZ = 0;

	bool hasPlayerPos = false;
	float playerX = 0.5f;
	float playerY = 80.0f;
	float playerZ = 0.5f;
	double playerYaw = 0.0;
	double playerPitch = 0.0;

	int health = 20;
	int hunger = 20;
	float saturation = 5.0f;
	int selectedHotbarSlot = 0;
	bool hasInventory = false;
	std::array<ItemInstance, 36> inventorySlots;
	std::array<ItemInstance, 4> armorSlots;

	std::unordered_map<BlockCoordKey, ChestData, BlockCoordHash> chests;
	std::unordered_map<BlockCoordKey, FurnaceData, BlockCoordHash> furnaces;
};

int64_t parseSeedString(const std::string& seedText);
std::string sanitizeWorldFolderName(const std::string& rawName);
std::string getWorldDbPath(const std::string& worldDir);

bool loadWorldMetadata(const std::string& worldDir, WorldMetadata& outMeta);
bool saveWorldMetadata(const std::string& worldDir, const WorldMetadata& meta);

void applyMetadataToContext(const WorldMetadata& meta, GuiContext& ctx);
void captureMetadataFromContext(const GuiContext& ctx, WorldMetadata& outMeta);
