#include "CreateWorldScreen.h"
#include "ProgressScreen.h"
#include "SelectWorldScreen.h"
#include "PlayScreen.h"
#include "TouchControls.h"
#include "WorldPersistence.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <filesystem>

namespace {
class ModeTabButton : public Button {
public:
	ModeTabButton(int id, const std::string& label, NinePatchLayer* normalBg, NinePatchLayer* selectedBg, bool* creativeFlag, bool isCreativeBtn)
		: Button(id, 0, 0, 38, 26, label)
		, mNormalBg(normalBg)
		, mSelectedBg(selectedBg)
		, mCreativeFlag(creativeFlag)
		, mIsCreativeBtn(isCreativeBtn) {}

	void render(GuiContext* ctx, int mouseX, int mouseY) override {
		if (!visible) return;
		bool isSelected = (mCreativeFlag && (*mCreativeFlag == mIsCreativeBtn));
		bool isHovered = active && pointInside(mouseX, mouseY);
		NinePatchLayer* bg = (isSelected || (pressed && isHovered)) ? mSelectedBg : mNormalBg;
		if (bg) {
			bg->draw(posX, posY, width, height);
		}
		if (ctx && ctx->font && !msg.empty()) {
			int color = isSelected ? 0xFFFFFFA0 : 0xFFE0E0E0;
			drawCenteredString(ctx->font, msg, posX + width / 2, posY + (height - 8) / 2, color);
		}
	}

private:
	NinePatchLayer* mNormalBg;
	NinePatchLayer* mSelectedBg;
	bool* mCreativeFlag;
	bool mIsCreativeBtn;
};
} // namespace

CreateWorldScreen::~CreateWorldScreen() {
	delete mFrame;
	delete mTabNormal;
	delete mTabSelected;
}

void CreateWorldScreen::init() {
	bool touch = context && context->options && context->options->useTouchUI;
	delete mFrame;
	delete mTabNormal;
	delete mTabSelected;
	mFrame = nullptr;
	mTabNormal = nullptr;
	mTabSelected = nullptr;
	mGameModeButton = nullptr;
	mCreativeButton = nullptr;
	mSurvivalButton = nullptr;

	if (touch) {
		mFrame = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 34, 43, 14, 14 }, 3, 3, 3, 3, 256, 256);
		mTabNormal = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 8, 32, 8, 8 }, 2, 2, 2, 2, 256, 256);
		mTabSelected = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 0, 32, 8, 8 }, 2, 2, 2, 2, 256, 256);

		auto* header = new Touch::THeader(0, "Create a Local Game");
		header->posX = 0;
		header->posY = 0;
		header->width = width;
		header->height = 30;
		buttons.push_back(header);

		mCreativeButton = new ModeTabButton(4, "Creative", mTabNormal, mTabSelected, &mCreativeMode, true);
		mSurvivalButton = new ModeTabButton(5, "Survival", mTabNormal, mTabSelected, &mCreativeMode, false);
		mCreateButton = new Touch::TButton(2, "Generate World");
		mCancelButton = new Touch::TButton(3, "Back");

		buttons.push_back(mCreativeButton);
		buttons.push_back(mSurvivalButton);
		buttons.push_back(mCreateButton);
		buttons.push_back(mCancelButton);
	} else {
		mGameModeButton = new Button(1, "Game Mode");
		mCreateButton = new Button(2, "Generate World!");
		mCancelButton = new Button(3, "Cancel");

		buttons.push_back(mGameModeButton);
		buttons.push_back(mCreateButton);
		buttons.push_back(mCancelButton);
	}

	updateLabels();
}

void CreateWorldScreen::updateLabels() {
	if (mGameModeButton) {
		mGameModeButton->msg = std::string("Game Mode: ") + (mCreativeMode ? "Creative" : "Survival");
	}
}

void CreateWorldScreen::setupPositions() {
	bool touch = context && context->options && context->options->useTouchUI;
	int cx = width / 2 - 100;

	if (touch) {
		int headerH = 30;
		if (!buttons.empty() && buttons[0]->id == 0) {
			buttons[0]->posX = 0;
			buttons[0]->posY = 0;
			buttons[0]->width = width;
			buttons[0]->height = headerH;
		}
		if (mCancelButton) {
			mCancelButton->posX = 4;
			mCancelButton->posY = 5;
			mCancelButton->width = 38;
			mCancelButton->height = 20;
		}
		int genW = std::max(120, static_cast<int>(static_cast<float>(width) / 2.3f));
		int modeW = genW / 2 - 5;
		int halfW = width / 2;
		int descW = std::max(80, halfW - 12);
		int descX = halfW + halfW / 2 - descW / 2 - 3;
		int modeY = height / 2 + 10;

		if (mCreativeButton) {
			mCreativeButton->posX = descX;
			mCreativeButton->posY = modeY;
			mCreativeButton->width = modeW;
			mCreativeButton->height = 26;
		}
		if (mSurvivalButton) {
			mSurvivalButton->posX = descX + descW - modeW;
			mSurvivalButton->posY = modeY;
			mSurvivalButton->width = modeW;
			mSurvivalButton->height = 26;
		}
		if (mCreateButton) {
			mCreateButton->width = genW;
			mCreateButton->height = 24;
			mCreateButton->posX = halfW - genW / 2;
			mCreateButton->posY = modeY + 26 + 10;
		}
		return;
	}

	if (mGameModeButton) { mGameModeButton->posX = cx; mGameModeButton->posY = 128; mGameModeButton->width = 200; mGameModeButton->height = 20; }
	if (mCreateButton) { mCreateButton->posX = width / 2 - 154; mCreateButton->posY = height - 32; mCreateButton->width = 150; mCreateButton->height = 20; }
	if (mCancelButton) { mCancelButton->posX = width / 2 + 4; mCancelButton->posY = height - 32; mCancelButton->width = 150; mCancelButton->height = 20; }
}

void CreateWorldScreen::render(int mouseX, int mouseY, float partialTicks) {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		renderMenuBackground(partialTicks);
		int headerH = 30;
		int frameTop = headerH + 10;
		int frameBottom = mCreateButton ? (mCreateButton->posY + mCreateButton->height + 10) : (height - 10);
		if (mFrame) {
			mFrame->draw(5, frameTop, width - 10, std::max(60, frameBottom - frameTop));
		}

		int lx = 10;
		int lw = std::max(100, width / 2 - 20);
		int nameLabelY = headerH + 15;
		int nameBoxY = nameLabelY + 10;
		int seedLabelY = nameBoxY + 18 + 13;
		int seedBoxY = seedLabelY + 10;

		if (font) drawString(font, "Name", 11, nameLabelY, 0xFFE0E0E0);
		fill(lx, nameBoxY, lx + lw, nameBoxY + 18, 0xFF000000);
		drawRect(lx, nameBoxY, lx + lw, nameBoxY + 18, (mFocusedField == 0) ? 0xFFFFFFFF : 0xFFA0A0A0);
		if (font) drawString(font, mWorldName + (mFocusedField == 0 ? "_" : ""), lx + 4, nameBoxY + 5, 0xFFE0E0E0);

		if (font) drawString(font, "Seed", 11, seedLabelY, 0xFFE0E0E0);
		fill(lx, seedBoxY, lx + lw, seedBoxY + 18, 0xFF000000);
		drawRect(lx, seedBoxY, lx + lw, seedBoxY + 18, (mFocusedField == 1) ? 0xFFFFFFFF : 0xFFA0A0A0);
		if (font) drawString(font, mSeedText + (mFocusedField == 1 ? "_" : ""), lx + 4, seedBoxY + 5, 0xFFE0E0E0);

		int halfW = width / 2;
		int descW = std::max(80, halfW - 12);
		int descX = halfW + halfW / 2 - descW / 2 - 3;
		int descY = headerH + 15;
		if (font) {
			if (mCreativeMode) {
				drawString(font, "Unlimited resources,", descX, descY, 0xFFE0E0E0);
				drawString(font, "flying and no damage.", descX, descY + 11, 0xFFE0E0E0);
			} else {
				drawString(font, "Limited resources, collect", descX, descY, 0xFFE0E0E0);
				drawString(font, "materials and survive.", descX, descY + 11, 0xFFE0E0E0);
			}
		}

		Screen::render(mouseX, mouseY, partialTicks);
		return;
	}

	renderDirtBackground();
	if (font) {
		drawCenteredString(font, "Create New World", width / 2, 16, 0xFFFFFFFF);
	}

	int cx = width / 2 - 100;

	// World Name box
	if (font) drawString(font, "World Name", cx, 38, 0xFFA0A0A0);
	fill(cx, 48, cx + 200, 68, 0xFF000000);
	drawRect(cx, 48, cx + 200, 68, (mFocusedField == 0) ? 0xFFFFFFFF : 0xFFA0A0A0);
	if (font) drawString(font, mWorldName + (mFocusedField == 0 ? "_" : ""), cx + 6, 54, 0xFFE0E0E0);

	// Seed box
	if (font) drawString(font, "Seed (optional)", cx, 78, 0xFFA0A0A0);
	fill(cx, 88, cx + 200, 108, 0xFF000000);
	drawRect(cx, 88, cx + 200, 108, (mFocusedField == 1) ? 0xFFFFFFFF : 0xFFA0A0A0);
	if (font) drawString(font, mSeedText + (mFocusedField == 1 ? "_" : ""), cx + 6, 94, 0xFFE0E0E0);

	Screen::render(mouseX, mouseY, partialTicks);
}

void CreateWorldScreen::mouseClicked(int x, int y, int button) {
	if (button == 0) {
		bool touch = context && context->options && context->options->useTouchUI;
		if (touch) {
			int lx = 10;
			int lw = std::max(100, width / 2 - 20);
			int nameBoxY = 30 + 15 + 10;
			int seedBoxY = nameBoxY + 18 + 13 + 10;
			if (x >= lx && x < lx + lw && y >= nameBoxY && y < nameBoxY + 18) {
				mFocusedField = 0;
			} else if (x >= lx && x < lx + lw && y >= seedBoxY && y < seedBoxY + 18) {
				mFocusedField = 1;
			}
		} else {
			int cx = width / 2 - 100;
			if (x >= cx && x < cx + 200 && y >= 48 && y < 68) {
				mFocusedField = 0;
			} else if (x >= cx && x < cx + 200 && y >= 88 && y < 108) {
				mFocusedField = 1;
			}
		}
	}
	Screen::mouseClicked(x, y, button);
}

void CreateWorldScreen::keyPressed(int key) {
	if (key == 256 /* GLFW_KEY_ESCAPE */) {
		handleBackEvent(true);
		return;
	}
	std::string& target = (mFocusedField == 0) ? mWorldName : mSeedText;
	if (key == 259 /* GLFW_KEY_BACKSPACE */) {
		if (!target.empty()) target.pop_back();
	}
	Screen::keyPressed(key);
}

void CreateWorldScreen::charTyped(unsigned int codepoint) {
	std::string& target = (mFocusedField == 0) ? mWorldName : mSeedText;
	if (codepoint >= 32 && codepoint <= 126) {
		if (target.size() < 24) {
			target.push_back(static_cast<char>(codepoint));
		}
	}
}

void CreateWorldScreen::buttonClicked(Button* button) {
	if (!context || !button) return;

	if (button->id == 1) {
		mCreativeMode = !mCreativeMode;
		updateLabels();
	} else if (button->id == 4) {
		mCreativeMode = true;
		updateLabels();
	} else if (button->id == 5) {
		mCreativeMode = false;
		updateLabels();
	} else if (button->id == 2) {
		namespace fs = std::filesystem;
		std::string displayName = mWorldName.empty() ? "New World" : mWorldName;
		std::string baseFolder = sanitizeWorldFolderName(displayName);
		std::string folderName = baseFolder;
		int suffix = 2;
		while (fs::exists("./worlds/" + folderName)) {
			folderName = baseFolder + " (" + std::to_string(suffix++) + ")";
		}
		std::string worldDir = "./worlds/" + folderName;
		std::error_code ec;
		fs::create_directories(worldDir, ec);

		int64_t seed = parseSeedString(mSeedText);
		WorldMetadata meta;
		meta.name = displayName;
		meta.seed = seed;
		meta.isCreative = mCreativeMode;
		meta.health = 20;
		meta.hunger = 20;
		meta.saturation = 5.0f;
		meta.hasPlayerPos = false;
		meta.hasInventory = false;
		saveWorldMetadata(worldDir, meta);

		context->activeWorldDir = worldDir;
		context->activeWorldName = displayName;
		context->activeWorldSeed = seed;
		context->isCreative = mCreativeMode;
		context->resetSurvivalStats();
		context->respawnRequested = true;
		context->worldOpenRequested = true;
		context->setScreen(new ProgressScreen());
	} else if (button->id == 3) {
		bool touch = context->options && context->options->useTouchUI;
		if (touch) {
			context->setScreen(new PlayScreen(true));
		} else {
			context->setScreen(new SelectWorldScreen());
		}
	}
}

bool CreateWorldScreen::handleBackEvent(bool /*isDown*/) {
	if (!context) return false;
	bool touch = context->options && context->options->useTouchUI;
	if (touch) {
		context->setScreen(new PlayScreen(true));
	} else {
		context->setScreen(new SelectWorldScreen());
	}
	return true;
}
