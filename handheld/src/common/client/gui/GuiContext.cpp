#include "GuiContext.h"
#include "Screen.h"
#include "StartMenuScreen.h"
#include "PauseScreen.h"
#include "world/crafting/CraftingRecipes.h"
#include "world/item/Item.h"
#include "world/level/block/Block.h"
#include <algorithm>
#include <cmath>

void GuiContext::ensureInventoryInitialized() {
	if (inventoryInitialized) return;
	inventoryInitialized = true;

	Recipes::getInstance()->init();

	// Sample 3D blocks and 2D items in the hotbar for render testing
	if (Block::mWoodPlanks) inventorySlots[0] = ItemInstance(Block::mWoodPlanks, 16, 0);
	if (Block::mCobblestone) inventorySlots[1] = ItemInstance(Block::mCobblestone, 32, 0);
	if (Block::mWorkBench) inventorySlots[2] = ItemInstance(Block::mWorkBench, 1, 0);
	if (Block::mGlass) inventorySlots[3] = ItemInstance(Block::mGlass, 12, 0);
	if (Block::mStone) inventorySlots[4] = ItemInstance(Block::mStone, 64, 0);
	if (Item::mSword_iron) inventorySlots[5] = ItemInstance(Item::mSword_iron, 1, 0);
	if (Item::mPickAxe_diamond) inventorySlots[6] = ItemInstance(Item::mPickAxe_diamond, 1, 0);
	if (Item::mApple) inventorySlots[7] = ItemInstance(Item::mApple, 8, 0);
	if (Item::mDiamond) inventorySlots[8] = ItemInstance(Item::mDiamond, 16, 0);
}

ItemInstance GuiContext::addItemToInventoryRemainder(const ItemInstance& item) {
	if (item.isNull() || item.getStackSize() <= 0) return ItemInstance();
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
			if (rem.isNull() || rem.getStackSize() <= 0) return ItemInstance();
		}
	}

	// Second pass: empty slots
	for (auto& slot : inventorySlots) {
		if (slot.isNull()) {
			int move = std::min(maxStack, static_cast<int>(rem.getStackSize()));
			slot = rem;
			slot.set(move);
			rem.remove(move);
			if (rem.isNull() || rem.getStackSize() <= 0) return ItemInstance();
		}
	}

	return rem;
}

bool GuiContext::addItemToInventory(const ItemInstance& item) {
	ItemInstance rem = addItemToInventoryRemainder(item);
	return rem.isNull() || rem.getStackSize() <= 0;
}

void GuiContext::resetSurvivalStats() {
	health = 20;
	hunger = 20;
	saturation = 5.0f;
	exhaustion = 0.0f;
	healTimer = 0.0f;
	starveTimer = 0.0f;
	hurtFlashTimer = 0.0f;
}

int GuiContext::applyDamage(int rawDamage, bool ignoreArmor) {
	if (rawDamage <= 0 || isCreative) return 0;

	int finalDamage = rawDamage;
	if (!ignoreArmor) {
		int defense = getTotalArmorDefense(armorSlots);
		if (defense > 0) {
			float mult = 1.0f - static_cast<float>(defense) * 0.04f;
			finalDamage = std::max(1, static_cast<int>(std::round(static_cast<float>(rawDamage) * mult)));

			int armorWear = std::max(1, rawDamage / 4);
			for (auto& piece : armorSlots) {
				if (piece.isNull()) continue;
				int maxDur = getItemMaxDurability(piece);
				if (maxDur > 0) {
					int nextAux = static_cast<int>(piece.getAuxValue()) + armorWear;
					if (nextAux > maxDur) {
						piece.setNull();
					} else {
						piece.setAuxValue(static_cast<short>(nextAux));
					}
				}
			}
		}
	}

	health = std::max(0, health - finalDamage);
	hurtFlashTimer = 0.40f;
	exhaustion = std::min(40.0f, exhaustion + 0.1f);
	return finalDamage;
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
	if (state == GameState::StartMenu) {
		resetSurvivalStats();
	}
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
	if (state == GameState::Paused || currentScreen != nullptr) {
		state = GameState::InGame;
		setScreen(nullptr);
	}
}

void GuiContext::quitToTitle() {
	if (hasWorldLoaded) {
		worldSaveAndCloseRequested = true;
	}
	state = GameState::StartMenu;
	setScreen(new StartMenuScreen());
}

void GuiContext::requestQuit() {
	shouldQuit = true;
}
