#include "CreateWorldScreen.h"
#include "ProgressScreen.h"
#include "SelectWorldScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <filesystem>

void CreateWorldScreen::init() {
	bool touch = context && context->options && context->options->useTouchUI;

	if (touch) {
		auto* header = new Touch::THeader(0, "Create New World");
		header->posX = 0;
		header->posY = 0;
		header->width = width;
		header->height = 26;
		buttons.push_back(header);

		mGameModeButton = new Touch::TButton(1, "Game Mode");
		mCreateButton = new Touch::TButton(2, "Generate World!");
		mCancelButton = new Touch::TButton(3, "Back");
	} else {
		mGameModeButton = new Button(1, "Game Mode");
		mCreateButton = new Button(2, "Generate World!");
		mCancelButton = new Button(3, "Cancel");
	}

	buttons.push_back(mGameModeButton);
	buttons.push_back(mCreateButton);
	buttons.push_back(mCancelButton);

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
		if (mCancelButton) { mCancelButton->posX = 4; mCancelButton->posY = 3; mCancelButton->width = 48; mCancelButton->height = 20; }
		if (mGameModeButton) { mGameModeButton->posX = cx; mGameModeButton->posY = 128; mGameModeButton->width = 200; mGameModeButton->height = 24; }
		if (mCreateButton) { mCreateButton->posX = cx; mCreateButton->posY = height - 32; mCreateButton->width = 200; mCreateButton->height = 24; }
		return;
	}

	if (mGameModeButton) { mGameModeButton->posX = cx; mGameModeButton->posY = 128; mGameModeButton->width = 200; mGameModeButton->height = 20; }
	if (mCreateButton) { mCreateButton->posX = width / 2 - 154; mCreateButton->posY = height - 32; mCreateButton->width = 150; mCreateButton->height = 20; }
	if (mCancelButton) { mCancelButton->posX = width / 2 + 4; mCancelButton->posY = height - 32; mCancelButton->width = 150; mCancelButton->height = 20; }
}

void CreateWorldScreen::render(int mouseX, int mouseY, float partialTicks) {
	renderDirtBackground();

	bool touch = context && context->options && context->options->useTouchUI;
	if (!touch && font) {
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
		int cx = width / 2 - 100;
		if (x >= cx && x < cx + 200 && y >= 48 && y < 68) {
			mFocusedField = 0;
		} else if (x >= cx && x < cx + 200 && y >= 88 && y < 108) {
			mFocusedField = 1;
		}
	}
	Screen::mouseClicked(x, y, button);
}

void CreateWorldScreen::keyPressed(int key) {
	std::string& target = (mFocusedField == 0) ? mWorldName : mSeedText;
	if (key == 259 /* GLFW_KEY_BACKSPACE */) {
		if (!target.empty()) target.pop_back();
	} else if (key >= 32 && key <= 126) {
		if (target.size() < 24) {
			target.push_back(static_cast<char>(key));
		}
	}
	Screen::keyPressed(key);
}

void CreateWorldScreen::buttonClicked(Button* button) {
	if (!context || !button) return;

	if (button->id == 1) {
		mCreativeMode = !mCreativeMode;
		updateLabels();
	} else if (button->id == 2) {
		std::string folderName = mWorldName.empty() ? "New World" : mWorldName;
		std::error_code ec;
		std::filesystem::create_directories("./worlds/" + folderName, ec);
		context->setScreen(new ProgressScreen());
	} else if (button->id == 3) {
		context->setScreen(new SelectWorldScreen());
	}
}
