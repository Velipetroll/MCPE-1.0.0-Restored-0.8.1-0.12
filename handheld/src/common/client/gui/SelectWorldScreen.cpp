#include "SelectWorldScreen.h"
#include "ProgressScreen.h"
#include "CreateWorldScreen.h"
#include "DeleteWorldScreen.h"
#include "AddExternalServerScreen.h"
#include "StartMenuScreen.h"
#include "TouchControls.h"
#include "WorldPersistence.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <filesystem>

static WorldEntry buildWorldEntry(const std::string& fallbackName, const std::string& dirPath, bool isDefault) {
	WorldEntry entry;
	entry.name = fallbackName;
	entry.path = dirPath;
	entry.isDefault = isDefault;
	WorldMetadata meta;
	meta.name = fallbackName;
	if (loadWorldMetadata(dirPath, meta)) {
		entry.name = meta.name.empty() ? fallbackName : meta.name;
		entry.seed = meta.seed;
		entry.isCreative = meta.isCreative;
	} else {
		entry.seed = isDefault ? 0 : parseSeedString(fallbackName);
		entry.isCreative = false;
	}
	entry.info = entry.isCreative ? "Creative Mode" : "Survival Mode";
	return entry;
}

void SelectWorldScreen::loadWorldList() {
	namespace fs = std::filesystem;
	mWorlds.clear();

	if (fs::exists("./restore")) {
		mWorlds.push_back(buildWorldEntry("My World (restore)", "./restore", true));
	} else {
		mWorlds.push_back(buildWorldEntry("My World", "./restore", true));
	}

	if (fs::exists("./worlds") && fs::is_directory("./worlds")) {
		for (const auto& entry : fs::directory_iterator("./worlds")) {
			if (entry.is_directory()) {
				std::string name = entry.path().filename().string();
				mWorlds.push_back(buildWorldEntry(name, entry.path().string(), false));
			}
		}
	}

	if (mSelectedIndex >= static_cast<int>(mWorlds.size())) {
		mSelectedIndex = static_cast<int>(mWorlds.size()) - 1;
	}
}

void SelectWorldScreen::init() {
	loadWorldList();
	bool touch = context && context->options && context->options->useTouchUI;

	if (touch) {
		auto* header = new Touch::THeader(0, "Select World");
		buttons.push_back(header);

		mPlayButton = new Touch::TButton(1, "Play");
		mCreateButton = new Touch::TButton(2, "New");
		mDeleteButton = new Touch::TButton(3, mEditMode ? "Done" : "Edit");
		mExternalButton = new Touch::TButton(5, "External");
		mBackButton = new Touch::TButton(4, "Back");
	} else {
		mPlayButton = new Button(1, "Play Selected World");
		mCreateButton = new Button(2, "Create New World");
		mDeleteButton = new Button(3, "Edit / Delete");
		mExternalButton = new Button(5, "External Server");
		mBackButton = new Button(4, "Cancel");
	}

	buttons.push_back(mPlayButton);
	buttons.push_back(mCreateButton);
	buttons.push_back(mDeleteButton);
	buttons.push_back(mExternalButton);
	buttons.push_back(mBackButton);

	updateButtonStates();
}

void SelectWorldScreen::updateButtonStates() {
	bool hasSel = (mSelectedIndex >= 0 && mSelectedIndex < static_cast<int>(mWorlds.size()));
	if (mPlayButton) mPlayButton->active = hasSel;
	if (mDeleteButton) mDeleteButton->active = hasSel && !mWorlds[mSelectedIndex].isDefault;
}

void SelectWorldScreen::setupPositions() {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		if (mBackButton) { mBackButton->posX = 0; mBackButton->posY = 0; mBackButton->width = 48; mBackButton->height = 24; }
		if (mDeleteButton) { mDeleteButton->posX = width - 98; mDeleteButton->posY = 0; mDeleteButton->width = 48; mDeleteButton->height = 24; mDeleteButton->active = true; }
		if (mCreateButton) { mCreateButton->posX = width - 48; mCreateButton->posY = 0; mCreateButton->width = 48; mCreateButton->height = 24; }
		if (!buttons.empty() && buttons[0]->id == 0) {
			buttons[0]->posX = 48;
			buttons[0]->posY = 0;
			buttons[0]->width = width - 146;
			buttons[0]->height = 24;
		}

		int btnW = 100;
		if (mPlayButton) { mPlayButton->posX = width / 2 - btnW - 4; mPlayButton->posY = height - 30; mPlayButton->width = btnW; mPlayButton->height = 24; }
		if (mExternalButton) { mExternalButton->posX = width / 2 + 4; mExternalButton->posY = height - 30; mExternalButton->width = btnW; mExternalButton->height = 24; }
		return;
	}

	int btnW = 100;
	int gap = 6;
	int y1 = height - 52;
	int y2 = height - 28;
	if (mPlayButton) { mPlayButton->posX = width / 2 - 154; mPlayButton->posY = y1; mPlayButton->width = 150; mPlayButton->height = 20; }
	if (mCreateButton) { mCreateButton->posX = width / 2 + 4; mCreateButton->posY = y1; mCreateButton->width = 150; mCreateButton->height = 20; }
	if (mDeleteButton) { mDeleteButton->posX = width / 2 - 154; mDeleteButton->posY = y2; mDeleteButton->width = btnW; mDeleteButton->height = 20; }
	if (mExternalButton) { mExternalButton->posX = width / 2 - 48; mExternalButton->posY = y2; mExternalButton->width = btnW; mExternalButton->height = 20; }
	if (mBackButton) { mBackButton->posX = width / 2 + 58; mBackButton->posY = y2; mBackButton->width = btnW - 4; mBackButton->height = 20; }
}

void SelectWorldScreen::render(int mouseX, int mouseY, float partialTicks) {
	renderDirtBackground();

	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		// Horizontal card list (RolledSelectionListH / TouchWorldSelectionList)
		int cardW = 110;
		int cardH = 110;
		int gap = 12;
		int startY = (height - cardH) / 2;
		int startX = 24 - mScrollX;

		for (size_t i = 0; i < mWorlds.size(); ++i) {
			int cx = startX + static_cast<int>(i) * (cardW + gap);
			fill(cx, startY, cx + cardW, startY + cardH, 0xA0000000);
			if (static_cast<int>(i) == mSelectedIndex) {
				drawRect(cx, startY, cx + cardW, startY + cardH, 0xFFFFFFFF);
			} else {
				drawRect(cx, startY, cx + cardW, startY + cardH, 0xFF555555);
			}
			if (font) {
				std::string subInfo = mEditMode
					? (mWorlds[i].info + "  Seed: " + std::to_string(mWorlds[i].seed))
					: mWorlds[i].info;
				drawCenteredString(font, mWorlds[i].name, cx + cardW / 2, startY + cardH - 28, 0xFFFFFFFF);
				drawCenteredString(font, subInfo, cx + cardW / 2, startY + cardH - 16, 0xFFAAAAAA);
			}
			if (mEditMode && !mWorlds[i].isDefault) {
				int xbx = cx + cardW - 18;
				int xby = startY + 4;
				fill(xbx, xby, xbx + 14, xby + 14, 0xFFAA2222);
				drawRect(xbx, xby, xbx + 14, xby + 14, 0xFFFFFFFF);
				if (font) drawCenteredString(font, "X", xbx + 7, xby + 3, 0xFFFFFFFF);
			}
		}
		Screen::render(mouseX, mouseY, partialTicks);
		return;
	}

	if (font) {
		drawCenteredString(font, "Select World", width / 2, 16, 0xFFFFFFFF);
	}

	int listLeft = width / 2 - 130;
	int listRight = width / 2 + 130;
	int listTop = 32;
	int listBottom = height - 60;
	fill(listLeft, listTop, listRight, listBottom, 0x90000000);
	drawRect(listLeft, listTop, listRight, listBottom, 0xFF404040);

	int rowH = 30;
	for (size_t i = 0; i < mWorlds.size(); ++i) {
		int ry = listTop + 4 + static_cast<int>(i) * rowH;
		if (ry + rowH > listBottom) break;

		if (static_cast<int>(i) == mSelectedIndex) {
			fill(listLeft + 4, ry, listRight - 4, ry + rowH - 2, 0xFF333333);
			drawRect(listLeft + 4, ry, listRight - 4, ry + rowH - 2, 0xFFFFFFFF);
		}
		if (font) {
			drawString(font, mWorlds[i].name, listLeft + 10, ry + 4, 0xFFFFFFFF);
			drawString(font, mWorlds[i].info, listLeft + 10, ry + 15, 0xFFAAAAAA);
		}
	}

	Screen::render(mouseX, mouseY, partialTicks);
}

void SelectWorldScreen::mouseClicked(int x, int y, int button) {
	bool touch = context && context->options && context->options->useTouchUI;
	if (button == 0 && touch) {
		int cardW = 110;
		int cardH = 110;
		int gap = 12;
		int startY = (height - cardH) / 2;
		int startX = 24 - mScrollX;
		if (y >= startY && y < startY + cardH) {
			for (size_t i = 0; i < mWorlds.size(); ++i) {
				int cx = startX + static_cast<int>(i) * (cardW + gap);
				if (x >= cx && x < cx + cardW) {
					if (mEditMode && !mWorlds[i].isDefault) {
						int xbx = cx + cardW - 18;
						int xby = startY + 4;
						if (x >= xbx && x < xbx + 14 && y >= xby && y < xby + 14) {
							context->setScreen(new DeleteWorldScreen(mWorlds[i].name, mWorlds[i].path));
							return;
						}
					}
					mSelectedIndex = static_cast<int>(i);
					updateButtonStates();
					break;
				}
			}
		}
	} else if (button == 0) {
		int listLeft = width / 2 - 130;
		int listRight = width / 2 + 130;
		int listTop = 32;
		int listBottom = height - 60;
		if (x >= listLeft && x < listRight && y >= listTop && y < listBottom) {
			int idx = (y - (listTop + 4)) / 30;
			if (idx >= 0 && idx < static_cast<int>(mWorlds.size())) {
				mSelectedIndex = idx;
				updateButtonStates();
			}
		}
	}
	Screen::mouseClicked(x, y, button);
}

void SelectWorldScreen::buttonClicked(Button* button) {
	if (!context || !button) return;

	if (button->id == 1) {
		if (mSelectedIndex >= 0 && mSelectedIndex < static_cast<int>(mWorlds.size())) {
			const auto& sel = mWorlds[static_cast<size_t>(mSelectedIndex)];
			context->activeWorldDir = sel.path;
			context->activeWorldName = sel.name;
			context->activeWorldSeed = sel.seed;
			context->isCreative = sel.isCreative;
			context->worldOpenRequested = true;
			context->setScreen(new ProgressScreen());
		}
	} else if (button->id == 2) {
		context->setScreen(new CreateWorldScreen());
	} else if (button->id == 3) {
		bool touch = context && context->options && context->options->useTouchUI;
		if (touch) {
			mEditMode = !mEditMode;
			if (mDeleteButton) mDeleteButton->msg = mEditMode ? "Done" : "Edit";
		} else if (mSelectedIndex >= 0 && mSelectedIndex < static_cast<int>(mWorlds.size()) && !mWorlds[mSelectedIndex].isDefault) {
			context->setScreen(new DeleteWorldScreen(mWorlds[mSelectedIndex].name, mWorlds[mSelectedIndex].path));
		}
	} else if (button->id == 5) {
		context->setScreen(new AddExternalServerScreen());
	} else if (button->id == 4) {
		context->setScreen(new StartMenuScreen());
	}
}
