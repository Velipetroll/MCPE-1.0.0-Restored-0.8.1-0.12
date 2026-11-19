#include "PlayScreen.h"
#include "ProgressScreen.h"
#include "StartMenuScreen.h"
#include "CreateWorldScreen.h"
#include "DeleteWorldScreen.h"
#include "AddExternalServerScreen.h"
#include "TouchControls.h"
#include "WorldPersistence.h"
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
	delete mDeleteNormal;
	delete mDeletePressed;
}

void PlayScreen::loadWorlds() {
	namespace fs = std::filesystem;
	mWorlds.clear();
	if (!mLocalWorlds) return;

	auto makeEntry = [](const std::string& fallbackName, const std::string& dirPath, bool isDefault) {
		WorldEntry we;
		we.name = fallbackName;
		we.path = dirPath;
		we.isDefault = isDefault;
		WorldMetadata meta;
		meta.name = fallbackName;
		if (loadWorldMetadata(dirPath, meta)) {
			we.name = meta.name.empty() ? fallbackName : meta.name;
			we.seed = meta.seed;
			we.isCreative = meta.isCreative;
		} else {
			we.seed = isDefault ? 0 : parseSeedString(fallbackName);
			we.isCreative = false;
		}
		we.info = we.isCreative ? "Creative Mode" : "Survival Mode";
		return we;
	};

	if (fs::exists("./restore")) {
		mWorlds.push_back(makeEntry("My World (restore)", "./restore", true));
	} else {
		mWorlds.push_back(makeEntry("My World", "./restore", true));
	}

	if (fs::exists("./worlds") && fs::is_directory("./worlds")) {
		for (const auto& entry : fs::directory_iterator("./worlds")) {
			if (entry.is_directory()) {
				mWorlds.push_back(makeEntry(entry.path().filename().string(), entry.path().string(), false));
			}
		}
	}
}

void PlayScreen::init() {
	loadWorlds();

	delete mFrame;
	delete mItemNormal;
	delete mItemPressed;
	delete mDeleteNormal;
	delete mDeletePressed;
	mFrame = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 34, 43, 14, 14 }, 3, 3, 3, 3, 256, 256);
	mItemNormal = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 8, 32, 8, 8 }, 2, 2, 2, 2, 256, 256);
	mItemPressed = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 0, 32, 8, 8 }, 2, 2, 2, 2, 256, 256);
	mDeleteNormal = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 112, 0, 8, 67 }, 2, 2, 2, 2, 256, 256);
	mDeletePressed = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 120, 0, 8, 67 }, 2, 2, 2, 2, 256, 256);

	mHeader = new Touch::THeader(0, mLocalWorlds ? "Play" : "Realms");
	mBackButton = new Touch::TButton(1, "Back");
	mBackButton->width = 38;
	mBackButton->height = 20;

	buttons.push_back(mHeader);
	buttons.push_back(mBackButton);

	if (mLocalWorlds) {
		mNewButton = new Touch::TButton(2, "New");
		mNewButton->width = 38;
		mNewButton->height = 20;

		mEditButton = new Touch::TButton(3, mEditMode ? "Done" : "Edit");
		mEditButton->width = 38;
		mEditButton->height = 20;

		mExternalButton = new Touch::TButton(2626, "External");
		mExternalButton->width = 50;
		mExternalButton->height = 20;

		buttons.push_back(mNewButton);
		buttons.push_back(mEditButton);
		buttons.push_back(mExternalButton);
	}
}

void PlayScreen::setupPositions() {
	int headerH = 30;
	if (mBackButton) {
		mBackButton->posX = 4;
		mBackButton->posY = 5;
	}
	if (mHeader) {
		mHeader->posX = 0;
		mHeader->posY = 0;
		mHeader->width = width;
		mHeader->height = headerH;
	}

	if (mLocalWorlds) {
		if (mEditButton) {
			mEditButton->posX = width - mEditButton->width - 4;
			mEditButton->posY = 5;
		}
		if (mNewButton && mEditButton) {
			mNewButton->posX = width - mEditButton->width - 8 - mNewButton->width;
			mNewButton->posY = 5;
		}
		if (mExternalButton && mNewButton) {
			mExternalButton->posX = mNewButton->posX - mExternalButton->width - 4;
			mExternalButton->posY = 5;
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

	if (!mLocalWorlds) {
		if (font) {
			drawCenteredString(font, "Pocket Realms is not available in this build.", width / 2, mListY + mListH / 2 - 4, 0xFFA0A0A0);
		}
		Screen::render(mouseX, mouseY, partialTicks);
		return;
	}

	const int rowH = 32;
	if (mDragging) {
		int dy = mLastDragY - mouseY;
		mLastDragY = mouseY;
		int maxScroll = std::max(0, static_cast<int>(mWorlds.size()) * rowH - mListH);
		mScrollY = std::max(0, std::min(maxScroll, mScrollY + dy));
	}

	int scale = context ? context->guiScale : 2;
	int winH = context ? context->windowHeight : (height * scale);
	glEnable(GL_SCISSOR_TEST);
	glScissor(mListX * scale, winH - (mListY + mListH) * scale, mListW * scale, mListH * scale);

	for (size_t i = 0; i < mWorlds.size(); ++i) {
		int ry = mListY + static_cast<int>(i) * rowH - mScrollY;
		bool showDelete = (mEditMode && !mWorlds[i].isDefault);
		int rowW = showDelete ? (mListW - 32) : mListW;

		bool rowPressed = (mDragging && !mPressedDelete && mPressedIndex == static_cast<int>(i));
		NinePatchLayer* bg = rowPressed ? mItemPressed : mItemNormal;
		if (bg) {
			bg->draw(mListX, ry, rowW, rowH);
		} else {
			fill(mListX, ry, mListX + rowW, ry + rowH, rowPressed ? 0xFF6B7EBF : 0xFF555555);
		}

		if (font) {
			int titleColor = rowPressed ? 0xFFFFFFA0 : 0xFFFFFFFF;
			std::string infoLine = mEditMode
				? (mWorlds[i].info + "  Seed: " + std::to_string(mWorlds[i].seed))
				: mWorlds[i].info;
			drawString(font, mWorlds[i].name, mListX + 5, ry + 5, titleColor);
			drawString(font, infoLine, mListX + 5, ry + 16, 0xFFBBBBBB);
		}

		if (showDelete) {
			int xbx = mListX + mListW - 32;
			int xby = ry;
			bool delPressed = (mDragging && mPressedDelete && mPressedIndex == static_cast<int>(i));
			NinePatchLayer* delBg = delPressed ? mDeletePressed : mDeleteNormal;
			if (delBg) {
				delBg->draw(xbx, xby, 32, 32);
			}
			if (bindTexture("textures/gui/gui.png")) {
				blitScaled(xbx + (32 - 11) / 2, xby + (32 - 11) / 2, 11, 11, 182, 10, 11, 11, 256, 256);
			}
		}
	}
	glDisable(GL_SCISSOR_TEST);

	Screen::render(mouseX, mouseY, partialTicks);
}

void PlayScreen::mouseClicked(int x, int y, int button) {
	if (button == 0 && mLocalWorlds && x >= mListX && x < mListX + mListW && y >= mListY && y < mListY + mListH) {
		mDragging = true;
		mLastDragY = y;
		const int rowH = 32;
		int idx = (y - mListY + mScrollY) / rowH;
		mPressedIndex = -1;
		mPressedDelete = false;
		if (idx >= 0 && idx < static_cast<int>(mWorlds.size())) {
			mPressedIndex = idx;
			if (mEditMode && !mWorlds[idx].isDefault) {
				int xbx = mListX + mListW - 32;
				if (x >= xbx && x < xbx + 32) {
					mPressedDelete = true;
				}
			}
		}
	}
	Screen::mouseClicked(x, y, button);
}

void PlayScreen::mouseReleased(int x, int y, int button) {
	if (button == 0 && mDragging && mLocalWorlds && x >= mListX && x < mListX + mListW && y >= mListY && y < mListY + mListH) {
		const int rowH = 32;
		int idx = (y - mListY + mScrollY) / rowH;
		if (idx == mPressedIndex && idx >= 0 && idx < static_cast<int>(mWorlds.size())) {
			if (mPressedDelete && mEditMode && !mWorlds[idx].isDefault) {
				int xbx = mListX + mListW - 32;
				if (x >= xbx && x < xbx + 32) {
					mDragging = false;
					mPressedIndex = -1;
					mPressedDelete = false;
					context->setScreen(new DeleteWorldScreen(mWorlds[idx].name, mWorlds[idx].path));
					return;
				}
			} else if (!mEditMode) {
				mDragging = false;
				mPressedIndex = -1;
				mPressedDelete = false;
				const auto& sel = mWorlds[static_cast<size_t>(idx)];
				context->activeWorldDir = sel.path;
				context->activeWorldName = sel.name;
				context->activeWorldSeed = sel.seed;
				context->isCreative = sel.isCreative;
				context->worldOpenRequested = true;
				context->setScreen(new ProgressScreen());
				return;
			}
		}
	}
	mDragging = false;
	mPressedIndex = -1;
	mPressedDelete = false;
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
