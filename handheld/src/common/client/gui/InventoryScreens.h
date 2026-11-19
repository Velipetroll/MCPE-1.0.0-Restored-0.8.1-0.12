#pragma once

#include "Screen.h"
#include "world/crafting/CraftingRecipes.h"
#include "world/item/ItemInstance.h"
#include <string>
#include <vector>

class InventoryScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void mouseClicked(int x, int y, int button) override;
	void mouseReleased(int x, int y, int button) override;
	void buttonClicked(Button* button) override;
	void keyPressed(int key) override;
	bool renderGameBehind() const override;

private:
	void updateCraftingResult();
	void returnItemsAndClose();

	Button* mCloseButton = nullptr;
	CraftingContainer mCraftGrid{ 2, 2 };
	ItemInstance mResultSlot;
	ItemInstance mCarriedItem;

	// Touch mode (Touch::IngameBlockSelectionScreen)
	Button* mHeader = nullptr;
	Button* mBackButton = nullptr;
	Button* mCraftButton = nullptr;
	Button* mArmorButton = nullptr;
	int mPaneX = 0;
	int mPaneY = 30;
	int mPaneW = 0;
	int mPaneH = 0;
	int mPaneCols = 1;
	int mScrollY = 0;
	bool mDragging = false;
	bool mDidDrag = false;
	int mLastDragY = 0;
	int mPressStartY = 0;
	int mSelectedItem = 0;
};

class ArmorScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void mouseClicked(int x, int y, int button) override;
	void mouseReleased(int x, int y, int button) override;
	void buttonClicked(Button* button) override;
	void keyPressed(int key) override;
	bool renderGameBehind() const override;

private:
	void rebuildArmorList();

	Button* mHeader = nullptr;
	Button* mBackButton = nullptr;
	Button* mArmorSlots[4] = { nullptr, nullptr, nullptr, nullptr };
	std::vector<int> mArmorInvIndices;
	int mPaneX = 10;
	int mPaneY = 32;
	int mPaneW = 0;
	int mPaneH = 0;
	int mBoxX = 0;
	int mBoxY = 32;
	int mBoxW = 0;
	int mBoxH = 0;
	int mScrollY = 0;
	bool mDragging = false;
	bool mDidDrag = false;
	int mLastDragY = 0;
	int mPressStartY = 0;
};

class CreativeInventoryScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	void keyPressed(int key) override;
	bool renderGameBehind() const override { return true; }

private:
	int mCategory = 0; // 0 = Building, 1 = Decoration, 2 = Tools, 3 = Misc
	std::vector<Button*> mCatButtons;
	Button* mCloseButton = nullptr;
};

struct TouchRecipeReq {
	ItemInstance item;
	int have = 0;
	int need = 0;
};

struct TouchRecipeEntry {
	const CraftingRecipe* recipe = nullptr;
	ItemInstance result;
	std::string name;
	std::string desc;
	std::vector<TouchRecipeReq> reqs;
	bool canBeCrafted = false;
	int inventoryCount = 0;
};

class PaneCraftingScreen : public Screen {
public:
	PaneCraftingScreen(int numCategories, int singleIconIndex, bool is2x2, const std::string& pcTitle);
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void mouseClicked(int x, int y, int button) override;
	void mouseReleased(int x, int y, int button) override;
	void buttonClicked(Button* button) override;
	void keyPressed(int key) override;
	bool renderGameBehind() const override;

protected:
	void closeScreen();
	void updateCraftingResult();
	void recheckRecipes();
	void sortRecipes();

	int mNumCategories = 4;
	int mSingleIconIndex = 0;
	bool mIs2x2 = false;
	std::string mPcTitle;
	int mCategory = 0;
	int mSelectedRecipe = 0;
	int mScrollY = 0;
	bool mDragging = false;
	int mLastDragY = 0;

	int mListX = 0;
	int mListY = 8;
	int mListW = 0;
	int mListH = 0;

	std::vector<TouchRecipeEntry> mTouchRecipes;
	std::vector<Button*> mCatButtons;
	Button* mCraftButton = nullptr;
	Button* mCloseButton = nullptr;

	// PC mode crafting state
	CraftingContainer mCraftGrid{ 3, 3 };
	ItemInstance mResultSlot;
	ItemInstance mCarriedItem;
};

class WorkbenchScreen : public PaneCraftingScreen {
public:
	explicit WorkbenchScreen(bool is2x2 = false);
};

class StonecutterScreen : public PaneCraftingScreen {
public:
	StonecutterScreen();
};

class FurnaceScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	bool renderGameBehind() const override { return true; }

private:
	Button* mCloseButton = nullptr;
};

class ChestScreen : public Screen {
public:
	explicit ChestScreen(bool isDouble = false);
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	bool renderGameBehind() const override { return true; }

private:
	bool mIsDouble = false;
	Button* mCloseButton = nullptr;
};

class TextEditScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	void mouseClicked(int x, int y, int button) override;
	void keyPressed(int key) override;
	bool renderGameBehind() const override { return true; }

private:
	std::string mLines[4];
	int mActiveLine = 0;
	Button* mDoneButton = nullptr;
};
