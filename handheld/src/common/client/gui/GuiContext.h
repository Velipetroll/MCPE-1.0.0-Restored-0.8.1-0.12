#pragma once

#include "world/item/ItemInstance.h"
#include <array>
#include <string>
#include <vector>

class Font;
class Options;
class Screen;

enum class GameState {
	StartMenu,
	InGame,
	Paused
};

struct GuiContext {
	Font* font = nullptr;
	Options* options = nullptr;
	GameState state = GameState::StartMenu;
	Screen* currentScreen = nullptr;
	bool shouldQuit = false;
	bool loadingChunks = false;
	float loadingProgress = 0.0f;
	std::vector<std::string> chatHistory;
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

	void ensureInventoryInitialized();
	bool addItemToInventory(const ItemInstance& item);

	void setScreen(Screen* screen);
	void startGame();
	void pauseGame();
	void resumeGame();
	void quitToTitle();
	void requestQuit();
};
