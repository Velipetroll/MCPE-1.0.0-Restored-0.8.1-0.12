#include "PlayScreen.h"
#include "ProgressScreen.h"
#include "StartMenuScreen.h"
#include "CreateWorldScreen.h"
#include "DeleteWorldScreen.h"
#include "AddExternalServerScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <glad/glad.h>
#include <algorithm>
#include <filesystem>

PlayScreen::PlayScreen(bool localWorlds)
	: mLocalWorlds(localWorlds) {
}

PlayScreen::~PlayScreen() {
	delete mFrame;
	delete mItemNormal;
	delete mItemPressed;
}

void PlayScreen::loadWorlds() {
	namespace fs = std::filesystem;
	mWorlds.clear();
	if (!mLocalWorlds) return;

	if (fs::exists("./restore")) {
		mWorlds.push_back({ "My World (restore)", "./restore", "Survival Mode", true });
	} else {
		mWorlds.push_back({ "My World", "./restore", "Survival Mode", true });
	}

	if (fs::exists("./worlds") && fs::is_directory("./worlds")) {
		for (const auto& entry : fs::directory_iterator("./worlds")) {
			if (entry.is_directory()) {
				mWorlds.push_back({ entry.path().filename().string(), entry.path().string(), "Saved World", false });
			}
		}
	}
}

void PlayScreen::init() {
	loadWorlds();

	delete mFrame;
	delete mItemNormal;
	delete mItemPressed;
	mFrame = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 34, 43, 14, 14 }, 3, 3, 3, 3, 256, 256);
	mItemNormal = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 8, 32, 8, 8 }, 2, 2, 2, 2, 256, 256);
	mItemPressed = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 0, 32, 8, 8 }, 2, 2, 2, 2, 256, 256);

	mHeader = new Touch::THeader(0, mLocalWorlds ? "Play" : "Realms");
	mBackButton = new Touch::TButton(1, "Back");
	mBackButton->width = 38;
	mBackButton->height = 18;

	buttons.push_back(mHeader);
	buttons.push_back(mBackButton);

	if (mLocalWorlds) {
		mNewButton = new Touch::TButton(2, "New");
		mNewButton->width = 38;
		mNewButton->height = 18;

		mEditButton = new Touch::TButton(3, mEditMode ? "Done" : "Edit");
		mEditButton->width = 38;
		mEditButton->height = 18;

		mExternalButton = new Touch::TButton(2626, "External");
		mExternalButton->width = 50;
		mExternalButton->height = 18;

		buttons.push_back(mNewButton);
		buttons.push_back(mEditButton);
		buttons.push_back(mExternalButton);
	}
}

void PlayScreen::setupPositions() {
	if (mBackButton) {
		mBackButton->posX = 4;
		mBackButton->posY = 4;
	}
	int headerH = (mBackButton ? mBackButton->height : 18) + 8;
	if (mHeader) {
		mHeader->posX = 0;
		mHeader->posY = 0;
		mHeader->width = width;
		mHeader->height = headerH;
	}

	if (mLocalWorlds) {
		if (mEditButton) {
			mEditButton->posX = width - mEditButton->width - 4;
			mEditButton->posY = 4;
		}
		if (mNewButton && mEditButton) {
			mNewButton->posX = width - mEditButton->width - 8 - mNewButton->width;
			mNewButton->posY = 4;
		}
		if (mExternalButton && mNewButton) {
			mExternalButton->posX = mNewButton->posX - mExternalButton->width - 4;
			mExternalButton->posY = 4;
		}
	}

	mListX = 10;
	mListY = headerH + 6;
	mListW = width - 20;
	mListH = height - mListY - 6;
}

void PlayScreen::render(int mouseX, int mouseY, float partialTicks) {
	renderMenuBackground(partialTicks);

	if (mFrame) {
		mFrame->draw(mListX - 3, mListY - 3, mListW + 6, mListH + 6);
	}
	fill(mListX, mListY, mListX + mListW, mListY + mListH, 0x90000000);

	if (!mLocalWorlds) {
		if (font) {
			drawCenteredString(font, "Pocket Realms is not available in this build.", width / 2, mListY + mListH / 2 - 4, 0xFFA0A0A0);
		}
		Screen::render(mouseX, mouseY, partialTicks);
		return;
	}

	if (mDragging) {
		int dy = mLastDragY - mouseY;
		mLastDragY = mouseY;
		int maxScroll = std::max(0, static_cast<int>(mWorlds.size()) * 28 - mListH);
		mScrollY = std::max(0, std::min(maxScroll, mScrollY + dy));
	}

	int scale = context ? context->guiScale : 2;
	int winH = context ? context->windowHeight : (height * scale);
	glEnable(GL_SCISSOR_TEST);
	glScissor(mListX * scale, winH - (mListY + mListH) * scale, mListW * scale, mListH * scale);

	int rowH = 28;
	for (size_t i = 0; i < mWorlds.size(); ++i) {
		int ry = mListY + static_cast<int>(i) * rowH - mScrollY;
		bool hov = (mouseX >= mListX && mouseX < mListX + mListW && mouseY >= ry && mouseY < ry + rowH);
		NinePatchLayer* bg = hov ? mItemPressed : mItemNormal;
		if (bg) {
			bg->draw(mListX + 2, ry + 1, mListW - 4, rowH - 2);
		} else {
			fill(mListX + 2, ry + 1, mListX + mListW - 2, ry + rowH - 1, hov ? 0xFF6B7EBF : 0xFF555555);
		}

		if (font) {
			drawString(font, mWorlds[i].name, mListX + 8, ry + 4, 0xFFFFFFFF);
			drawString(font, mWorlds[i].info, mListX + 8, ry + 15, 0xFFAAAAAA);
		}

		if (mEditMode && !mWorlds[i].isDefault) {
			int xbx = mListX + mListW - 22;
			int xby = ry + 6;
			fill(xbx, xby, xbx + 16, xby + 16, 0xFFAA2222);
			drawRect(xbx, xby, xbx + 16, xby + 16, 0xFFFFFFFF);
			if (font) drawCenteredString(font, "X", xbx + 8, xby + 4, 0xFFFFFFFF);
		}
	}
	glDisable(GL_SCISSOR_TEST);

	Screen::render(mouseX, mouseY, partialTicks);
}

void PlayScreen::mouseClicked(int x, int y, int button) {
	if (button == 0 && mLocalWorlds && x >= mListX && x < mListX + mListW && y >= mListY && y < mListY + mListH) {
		mDragging = true;
		mLastDragY = y;
		int rowH = 28;
		int idx = (y - mListY + mScrollY) / rowH;
		if (idx >= 0 && idx < static_cast<int>(mWorlds.size())) {
			if (mEditMode && !mWorlds[idx].isDefault) {
				int xbx = mListX + mListW - 22;
				if (x >= xbx && x < xbx + 16) {
					context->setScreen(new DeleteWorldScreen(mWorlds[idx].name, mWorlds[idx].path));
					return;
				}
			} else if (!mEditMode) {
				context->setScreen(new ProgressScreen());
				return;
			}
		}
	}
	Screen::mouseClicked(x, y, button);
}

void PlayScreen::mouseReleased(int x, int y, int button) {
	mDragging = false;
	Screen::mouseReleased(x, y, button);
}

void PlayScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (button->id == 1) {
		context->setScreen(new StartMenuScreen());
	} else if (button->id == 2) {
		context->setScreen(new CreateWorldScreen());
	} else if (button->id == 3) {
		mEditMode = !mEditMode;
		if (mEditButton) mEditButton->msg = mEditMode ? "Done" : "Edit";
	} else if (button->id == 2626) {
		context->setScreen(new AddExternalServerScreen());
	}
}
