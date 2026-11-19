#include "GuiContext.h"
#include "Screen.h"
#include "StartMenuScreen.h"
#include "PauseScreen.h"
#include "world/crafting/CraftingRecipes.h"
#include "world/item/Item.h"
#include "world/level/block/Block.h"
#include <algorithm>

void GuiContext::ensureInventoryInitialized() {
	if (inventoryInitialized) return;
	inventoryInitialized = true;

	Recipes::getInstance()->init();
}

bool GuiContext::addItemToInventory(const ItemInstance& item) {
	if (item.isNull() || item.getStackSize() <= 0) return true;
	ensureInventoryInitialized();

	ItemInstance rem(item);
	int maxStack = std::max(1, static_cast<int>(rem.getMaxStackSize()));

	// First pass: stack into existing matching slots
	for (auto& slot : inventorySlots) {
		if (!slot.isNull() && slot.sameItemAndAux(&rem) && slot.hasSameUserData(rem) && slot.getStackSize() < maxStack) {
			int space = maxStack - static_cast<int>(slot.getStackSize());
			int move = std::min(space, static_cast<int>(rem.getStackSize()));
			slot.add(move);
			rem.remove(move);
			if (rem.isNull() || rem.getStackSize() <= 0) return true;
		}
	}

	// Second pass: empty slots
	for (auto& slot : inventorySlots) {
		if (slot.isNull()) {
			int move = std::min(maxStack, static_cast<int>(rem.getStackSize()));
			slot = rem;
			slot.set(move);
			rem.remove(move);
			if (rem.isNull() || rem.getStackSize() <= 0) return true;
		}
	}

	return rem.isNull() || rem.getStackSize() <= 0;
}

void GuiContext::setScreen(Screen* screen) {
	if (currentScreen == screen) return;
	delete currentScreen;
	currentScreen = screen;
	if (currentScreen) {
		currentScreen->init(this, guiWidth, guiHeight);
	}
}

void GuiContext::startGame() {
	state = GameState::InGame;
	setScreen(nullptr);
}

void GuiContext::pauseGame() {
	if (state == GameState::InGame) {
		state = GameState::Paused;
		setScreen(new PauseScreen());
	}
}

void GuiContext::resumeGame() {
	if (state == GameState::Paused) {
		state = GameState::InGame;
		setScreen(nullptr);
	}
}

void GuiContext::quitToTitle() {
	state = GameState::StartMenu;
	setScreen(new StartMenuScreen());
}

void GuiContext::requestQuit() {
	shouldQuit = true;
}
