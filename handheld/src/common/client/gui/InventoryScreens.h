#pragma once

#include "Screen.h"
#include <string>
#include <vector>

class InventoryScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	void keyPressed(int key) override;
	bool renderGameBehind() const override { return true; }

private:
	int mTab = 0; // 0 = Inventory, 1 = Crafting (2x2), 2 = Armor
	Button* mTabInv = nullptr;
	Button* mTabCraft = nullptr;
	Button* mTabArmor = nullptr;
	Button* mCloseButton = nullptr;
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

class WorkbenchScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	bool renderGameBehind() const override { return true; }

private:
	Button* mCloseButton = nullptr;
};

class StonecutterScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	bool renderGameBehind() const override { return true; }

private:
	Button* mCloseButton = nullptr;
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
