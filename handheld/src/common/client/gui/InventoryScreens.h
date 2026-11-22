#pragma once

#include "Screen.h"
#include "GuiContext.h"
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
	bool handleBackEvent(bool isDown = false) override;
	bool renderGameBehind() const override;
	bool isPauseScreen() const override { return false; }

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
	bool mEmbedded = false;
	int mCustomX = 0;
	int mCustomY = 0;
	ItemInstance* mExternalCarried = nullptr;
public:
	void setEmbeddedPos(int x, int y) { mEmbedded = true; mCustomX = x; mCustomY = y; }
	void setExternalCarried(ItemInstance* ext) { mExternalCarried = ext; }
	Button* getCloseButton() const { return mCloseButton; }
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
	bool handleBackEvent(bool isDown = false) override;
	bool renderGameBehind() const override;
	bool isPauseScreen() const override { return false; }

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
	~CreativeInventoryScreen() override;
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void mouseClicked(int x, int y, int button) override;
	void mouseReleased(int x, int y, int button) override;
	void mouseScrolled(double yoffset) override;
	void buttonClicked(Button* button) override;
	void keyPressed(int key) override;
	bool handleBackEvent(bool isDown = false) override;
	bool renderGameBehind() const override;
	bool isPauseScreen() const override { return false; }

private:
	void ensureCreativeItems();
	int getMaxScroll() const;

	int mCategory = 0; // 0 = Building, 1 = Decoration, 2 = Tools, 3 = Misc
	std::vector<Button*> mCatButtons;
	Button* mCloseButton = nullptr;
	Button* mInventoryButton = nullptr;
	bool mShowSurvivalInv = false;
	InventoryScreen* mSideInventory = nullptr;
	std::vector<ItemInstance> mFilteredItems[4];
	ItemInstance mCarriedItem;

	int mFrameX = 0;
	int mFrameY = 0;
	int mFrameW = 0;
	int mFrameH = 0;
	int mTabSize = 24;
	int mPaneX = 0;
	int mPaneY = 0;
	int mPaneW = 0;
	int mPaneH = 0;
	int mPaneCols = 9;
	int mCellSize = 26;
	int mScrollY = 0;
	bool mDragging = false;
	bool mDidDrag = false;
	int mLastDragY = 0;
	int mPressStartY = 0;
	int mSelectedCreativeIdx = -1;
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
	bool handleBackEvent(bool isDown = false) override;
	bool renderGameBehind() const override;
	bool isPauseScreen() const override { return false; }

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
	FurnaceScreen() = default;
	explicit FurnaceScreen(const BlockCoordKey& pos);
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void mouseClicked(int x, int y, int button) override;
	void mouseReleased(int x, int y, int button) override;
	void buttonClicked(Button* button) override;
	void keyPressed(int key) override;
	bool handleBackEvent(bool isDown = false) override;
	bool renderGameBehind() const override;
	bool isPauseScreen() const override { return false; }

private:
	void closeScreen();
	void getTouchSlotRects(int& inX, int& inY, int& fuelX, int& fuelY, int& outX, int& outY) const;
	BlockCoordKey mPos{};
	ItemInstance mCarriedItem;
	Button* mCloseButton = nullptr;

	// Touch UI state
	Button* mHeader = nullptr;
	Button* mBackButton = nullptr;
	int mSelectedSlot = 0; // 0 = Ingredient, 1 = Fuel
	int mPaneX = 10;
	int mPaneY = 32;
	int mPaneW = 0;
	int mPaneH = 0;
	int mPaneCols = 1;
	int mScrollY = 0;
	bool mDragging = false;
	bool mDidDrag = false;
	int mLastDragY = 0;
	int mPressStartY = 0;
};

class ChestScreen : public Screen {
public:
	explicit ChestScreen(bool isDouble = false);
	ChestScreen(const BlockCoordKey& primaryPos, bool isDouble, const BlockCoordKey& secondaryPos = BlockCoordKey{});
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void mouseClicked(int x, int y, int button) override;
	void buttonClicked(Button* button) override;
	void keyPressed(int key) override;
	bool handleBackEvent(bool isDown = false) override;
	bool renderGameBehind() const override { return true; }
	bool isPauseScreen() const override { return false; }

private:
	void closeScreen();
	BlockCoordKey mPrimaryPos{};
	BlockCoordKey mSecondaryPos{};
	bool mIsDouble = false;
	ItemInstance mCarriedItem;
	Button* mCloseButton = nullptr;
};

void tickFurnaces(GuiContext& ctx, float dt);

class TextEditScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	void mouseClicked(int x, int y, int button) override;
	void keyPressed(int key) override;
	bool handleBackEvent(bool isDown = false) override;
	bool renderGameBehind() const override { return true; }
	bool isPauseScreen() const override { return false; }

private:
	std::string mLines[4];
	int mActiveLine = 0;
	Button* mDoneButton = nullptr;
};
