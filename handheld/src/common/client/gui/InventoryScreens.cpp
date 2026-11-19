#include "InventoryScreens.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"

namespace {
	Button* makeInvBtn(GuiContext* ctx, int id, const std::string& msg) {
		bool touch = ctx && ctx->options && ctx->options->useTouchUI;
		return touch ? static_cast<Button*>(new Touch::TButton(id, msg)) : new Button(id, msg);
	}

	void drawSlotGrid(const Screen* s, int x, int y, int cols, int rows, int slotSize = 18) {
		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				int sx = x + c * slotSize;
				int sy = y + r * slotSize;
				s->fill(sx, sy, sx + slotSize - 1, sy + slotSize - 1, 0xFF8B8B8B);
				s->drawRect(sx, sy, sx + slotSize - 1, sy + slotSize - 1, 0xFF373737);
			}
		}
	}

	void drawPanel(const Screen* s, int x, int y, int w, int h) {
		s->fill(x, y, x + w, y + h, 0xFFC6C6C6);
		s->drawRect(x, y, x + w, y + h, 0xFF555555);
	}
}

// --- InventoryScreen ---
void InventoryScreen::init() {
	mTabInv = makeInvBtn(context, 10, "Inventory");
	mTabCraft = makeInvBtn(context, 11, "Crafting");
	mTabArmor = makeInvBtn(context, 12, "Armor");
	mCloseButton = makeInvBtn(context, 1, "X");
	buttons.push_back(mTabInv);
	buttons.push_back(mTabCraft);
	buttons.push_back(mTabArmor);
	buttons.push_back(mCloseButton);
}

void InventoryScreen::setupPositions() {
	int pw = 220, ph = 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	if (mTabInv) { mTabInv->posX = px; mTabInv->posY = py - 22; mTabInv->width = 64; mTabInv->height = 20; mTabInv->active = (mTab != 0); }
	if (mTabCraft) { mTabCraft->posX = px + 66; mTabCraft->posY = py - 22; mTabCraft->width = 64; mTabCraft->height = 20; mTabCraft->active = (mTab != 1); }
	if (mTabArmor) { mTabArmor->posX = px + 132; mTabArmor->posY = py - 22; mTabArmor->width = 64; mTabArmor->height = 20; mTabArmor->active = (mTab != 2); }
	if (mCloseButton) { mCloseButton->posX = px + pw - 20; mCloseButton->posY = py - 22; mCloseButton->width = 20; mCloseButton->height = 20; }
}

void InventoryScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	int pw = 220, ph = 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	drawPanel(this, px, py, pw, ph);

	if (mTab == 0) {
		if (font) drawString(font, "Inventory", px + 8, py + 6, 0xFF404040);
		drawSlotGrid(this, px + 28, py + 20, 9, 3);
		drawSlotGrid(this, px + 28, py + 84, 9, 1);
	} else if (mTab == 1) {
		if (font) drawString(font, "Crafting (2x2)", px + 8, py + 6, 0xFF404040);
		drawSlotGrid(this, px + 36, py + 24, 2, 2);
		if (font) drawString(font, "->", px + 84, py + 38, 0xFF404040);
		drawSlotGrid(this, px + 106, py + 32, 1, 1);
		drawSlotGrid(this, px + 28, py + 84, 9, 3);
	} else {
		if (font) drawString(font, "Armor", px + 8, py + 6, 0xFF404040);
		drawSlotGrid(this, px + 16, py + 22, 1, 4);
		fill(px + 44, py + 22, px + 96, py + 94, 0xFF000000);
		if (font) drawCenteredString(font, "Player", px + 70, py + 54, 0xFFFFFFFF);
		drawSlotGrid(this, px + 28, py + 102, 9, 2);
	}
	Screen::render(mouseX, mouseY, partialTicks);
}

void InventoryScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (button->id == 1) context->resumeGame();
	else if (button->id >= 10 && button->id <= 12) {
		mTab = button->id - 10;
		setupPositions();
	}
}

void InventoryScreen::keyPressed(int key) {
	if (key == 69 /* GLFW_KEY_E */ || key == 256 /* GLFW_KEY_ESCAPE */) {
		if (context) context->resumeGame();
		return;
	}
	Screen::keyPressed(key);
}

// --- CreativeInventoryScreen ---
void CreativeInventoryScreen::init() {
	mCatButtons.clear();
	static const char* cats[4] = { "Building", "Decor", "Tools", "Misc" };
	for (int i = 0; i < 4; ++i) {
		Button* b = makeInvBtn(context, 10 + i, cats[i]);
		mCatButtons.push_back(b);
		buttons.push_back(b);
	}
	mCloseButton = makeInvBtn(context, 1, "X");
	buttons.push_back(mCloseButton);
}

void CreativeInventoryScreen::setupPositions() {
	int pw = 220, ph = 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	for (size_t i = 0; i < mCatButtons.size(); ++i) {
		mCatButtons[i]->posX = px + static_cast<int>(i) * 48;
		mCatButtons[i]->posY = py - 22;
		mCatButtons[i]->width = 46;
		mCatButtons[i]->height = 20;
		mCatButtons[i]->active = (static_cast<int>(i) != mCategory);
	}
	if (mCloseButton) { mCloseButton->posX = px + pw - 20; mCloseButton->posY = py - 22; mCloseButton->width = 20; mCloseButton->height = 20; }
}

void CreativeInventoryScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	int pw = 220, ph = 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	drawPanel(this, px, py, pw, ph);
	if (font) drawString(font, "Creative Inventory", px + 8, py + 6, 0xFF404040);
	drawSlotGrid(this, px + 28, py + 22, 9, 5);
	drawSlotGrid(this, px + 28, py + 120, 9, 1);
	Screen::render(mouseX, mouseY, partialTicks);
}

void CreativeInventoryScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (button->id == 1) context->resumeGame();
	else if (button->id >= 10 && button->id <= 13) {
		mCategory = button->id - 10;
		setupPositions();
	}
}

void CreativeInventoryScreen::keyPressed(int key) {
	if (key == 69 || key == 256) {
		if (context) context->resumeGame();
		return;
	}
	Screen::keyPressed(key);
}

// --- WorkbenchScreen ---
void WorkbenchScreen::init() {
	mCloseButton = makeInvBtn(context, 1, "X");
	buttons.push_back(mCloseButton);
}

void WorkbenchScreen::setupPositions() {
	int pw = 220, ph = 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	if (mCloseButton) { mCloseButton->posX = px + pw - 20; mCloseButton->posY = py - 22; mCloseButton->width = 20; mCloseButton->height = 20; }
}

void WorkbenchScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	int pw = 220, ph = 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	drawPanel(this, px, py, pw, ph);
	if (font) drawString(font, "Crafting (3x3)", px + 8, py + 6, 0xFF404040);
	drawSlotGrid(this, px + 30, py + 20, 3, 3);
	if (font) drawString(font, "->", px + 94, py + 42, 0xFF404040);
	drawSlotGrid(this, px + 116, py + 38, 1, 1);
	drawSlotGrid(this, px + 28, py + 84, 9, 3);
	Screen::render(mouseX, mouseY, partialTicks);
}

void WorkbenchScreen::buttonClicked(Button* button) {
	if (context && button && button->id == 1) context->resumeGame();
}

// --- StonecutterScreen ---
void StonecutterScreen::init() {
	mCloseButton = makeInvBtn(context, 1, "X");
	buttons.push_back(mCloseButton);
}

void StonecutterScreen::setupPositions() {
	int pw = 220, ph = 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	if (mCloseButton) { mCloseButton->posX = px + pw - 20; mCloseButton->posY = py - 22; mCloseButton->width = 20; mCloseButton->height = 20; }
}

void StonecutterScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	int pw = 220, ph = 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	drawPanel(this, px, py, pw, ph);
	if (font) drawString(font, "Stonecutter", px + 8, py + 6, 0xFF404040);
	drawSlotGrid(this, px + 28, py + 22, 4, 3);
	if (font) drawString(font, "->", px + 112, py + 42, 0xFF404040);
	drawSlotGrid(this, px + 134, py + 38, 1, 1);
	drawSlotGrid(this, px + 28, py + 84, 9, 3);
	Screen::render(mouseX, mouseY, partialTicks);
}

void StonecutterScreen::buttonClicked(Button* button) {
	if (context && button && button->id == 1) context->resumeGame();
}

// --- FurnaceScreen ---
void FurnaceScreen::init() {
	mCloseButton = makeInvBtn(context, 1, "X");
	buttons.push_back(mCloseButton);
}

void FurnaceScreen::setupPositions() {
	int pw = 220, ph = 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	if (mCloseButton) { mCloseButton->posX = px + pw - 20; mCloseButton->posY = py - 22; mCloseButton->width = 20; mCloseButton->height = 20; }
}

void FurnaceScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	int pw = 220, ph = 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	drawPanel(this, px, py, pw, ph);
	if (font) drawString(font, "Furnace", px + 8, py + 6, 0xFF404040);
	drawSlotGrid(this, px + 56, py + 20, 1, 1); // Input
	drawSlotGrid(this, px + 56, py + 56, 1, 1); // Fuel
	if (font) drawString(font, "->", px + 86, py + 40, 0xFF404040);
	drawSlotGrid(this, px + 116, py + 38, 1, 1); // Output
	drawSlotGrid(this, px + 28, py + 84, 9, 3);
	Screen::render(mouseX, mouseY, partialTicks);
}

void FurnaceScreen::buttonClicked(Button* button) {
	if (context && button && button->id == 1) context->resumeGame();
}

// --- ChestScreen ---
ChestScreen::ChestScreen(bool isDouble) : mIsDouble(isDouble) {}

void ChestScreen::init() {
	mCloseButton = makeInvBtn(context, 1, "X");
	buttons.push_back(mCloseButton);
}

void ChestScreen::setupPositions() {
	int pw = 220, ph = mIsDouble ? 186 : 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	if (mCloseButton) { mCloseButton->posX = px + pw - 20; mCloseButton->posY = py - 22; mCloseButton->width = 20; mCloseButton->height = 20; }
}

void ChestScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	int pw = 220, ph = mIsDouble ? 186 : 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	drawPanel(this, px, py, pw, ph);
	if (font) drawString(font, mIsDouble ? "Large Chest" : "Chest", px + 8, py + 6, 0xFF404040);
	int rows = mIsDouble ? 5 : 3;
	drawSlotGrid(this, px + 28, py + 20, 9, rows);
	drawSlotGrid(this, px + 28, py + 24 + rows * 18, 9, 3);
	Screen::render(mouseX, mouseY, partialTicks);
}

void ChestScreen::buttonClicked(Button* button) {
	if (context && button && button->id == 1) context->resumeGame();
}

// --- TextEditScreen ---
void TextEditScreen::init() {
	mDoneButton = makeInvBtn(context, 1, "Done");
	buttons.push_back(mDoneButton);
}

void TextEditScreen::setupPositions() {
	if (mDoneButton) {
		mDoneButton->posX = width / 2 - 100;
		mDoneButton->posY = height - 40;
		mDoneButton->width = 200;
		mDoneButton->height = 20;
	}
}

void TextEditScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	if (font) drawCenteredString(font, "Edit Sign Message", width / 2, 24, 0xFFFFFFFF);
	int bx = width / 2 - 60, by = 52;
	fill(bx, by, bx + 120, by + 64, 0xFF9C7B49);
	drawRect(bx, by, bx + 120, by + 64, 0xFF403018);
	for (int i = 0; i < 4; ++i) {
		std::string line = mLines[i] + (mActiveLine == i ? "_" : "");
		if (font) drawCenteredString(font, line, width / 2, by + 8 + i * 14, 0xFF000000);
	}
	Screen::render(mouseX, mouseY, partialTicks);
}

void TextEditScreen::mouseClicked(int x, int y, int button) {
	if (button == 0) {
		int bx = width / 2 - 60, by = 52;
		if (x >= bx && x < bx + 120 && y >= by && y < by + 64) {
			mActiveLine = (y - by) / 16;
			if (mActiveLine < 0) mActiveLine = 0;
			if (mActiveLine > 3) mActiveLine = 3;
		}
	}
	Screen::mouseClicked(x, y, button);
}

void TextEditScreen::keyPressed(int key) {
	if (key == 259 /* BACKSPACE */) {
		if (!mLines[mActiveLine].empty()) mLines[mActiveLine].pop_back();
	} else if (key == 257 /* ENTER */) {
		mActiveLine = (mActiveLine + 1) % 4;
	} else if (key >= 32 && key <= 126) {
		if (mLines[mActiveLine].size() < 15) mLines[mActiveLine].push_back(static_cast<char>(key));
	}
	Screen::keyPressed(key);
}

void TextEditScreen::buttonClicked(Button* button) {
	if (context && button && button->id == 1) context->resumeGame();
}
