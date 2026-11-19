#include "PauseScreen.h"
#include "OptionsScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <algorithm>

PauseScreen::~PauseScreen() {
	delete mListBg;
	delete mRowBg;
}

void PauseScreen::init() {
	bool touch = context && context->options && context->options->useTouchUI;
	delete mListBg;
	delete mRowBg;
	mListBg = nullptr;
	mRowBg = nullptr;

	if (touch) {
		mListBg = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 0, 53, 3, 4 }, 1, 1, 1, 1, 256, 256);
		mRowBg = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 0, 54, 3, 3 }, 1, 1, 1, 1, 256, 256);

		mResumeButton = new Touch::TButton(1, "Back to game");
		mOptionsButton = new Touch::TButton(2, "Options");
		mQuitToTitleButton = new Touch::TButton(3, "Quit to title");
	} else {
		mResumeButton = new Button(1, "Back to Game");
		mOptionsButton = new Button(2, "Options");
		mQuitToTitleButton = new Button(3, "Quit to Title");
	}

	buttons.push_back(mResumeButton);
	buttons.push_back(mOptionsButton);
	buttons.push_back(mQuitToTitleButton);
}

void PauseScreen::setupPositions() {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		int v2 = std::max(10, width / 20);
		int colW = 8 * v2;
		int btnH = 24;

		if (mResumeButton) {
			mResumeButton->posX = v2;
			mResumeButton->posY = 48;
			mResumeButton->width = colW;
			mResumeButton->height = btnH;
		}
		if (mOptionsButton) {
			mOptionsButton->posX = v2;
			mOptionsButton->posY = 80;
			mOptionsButton->width = colW;
			mOptionsButton->height = btnH;
		}
		if (mQuitToTitleButton) {
			mQuitToTitleButton->posX = v2;
			mQuitToTitleButton->posY = 112;
			mQuitToTitleButton->width = colW;
			mQuitToTitleButton->height = btnH;
		}
		return;
	}

	int btnW = 160;
	int btnH = 20;
	int cx = width / 2 - btnW / 2;
	int startY = height / 2 - 36;

	if (mResumeButton) {
		mResumeButton->posX = cx;
		mResumeButton->posY = startY;
		mResumeButton->width = btnW;
		mResumeButton->height = btnH;
	}
	if (mOptionsButton) {
		mOptionsButton->posX = cx;
		mOptionsButton->posY = startY + 26;
		mOptionsButton->width = btnW;
		mOptionsButton->height = btnH;
	}
	if (mQuitToTitleButton) {
		mQuitToTitleButton->posX = cx;
		mQuitToTitleButton->posY = startY + 52;
		mQuitToTitleButton->width = btnW;
		mQuitToTitleButton->height = btnH;
	}
}

void PauseScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xC0101010, 0xD0101010);

	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		int v2 = std::max(10, width / 20);
		int v3 = std::max(10, height / 10);
		int colW = 8 * v2;
		int listX = width - colW - v2;
		int listY = v3;
		int listH = 8 * v3;

		if (font) {
			drawCenteredString(font, "Game menu", v2 + colW / 2, 48 - 17, 0xFFFFFFFF);
		}
		if (mListBg) {
			mListBg->draw(listX, listY, colW, listH);
		}
		if (mRowBg) {
			mRowBg->draw(listX + 1, listY + 1, colW - 2, 15);
		}
		if (font) {
			std::string playerName = (context && context->options && !context->options->username.empty())
				? context->options->username
				: "Steve";
			drawString(font, playerName, listX + 4, listY + 5, 0xFFFFFFFF);
		}
	} else if (font) {
		drawCenteredString(font, "Game Menu", width / 2, height / 2 - 64, 0xFFFFFFFF);
	}

	Screen::render(mouseX, mouseY, partialTicks);
}

void PauseScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (button->id == 1) {
		context->resumeGame();
	} else if (button->id == 2) {
		context->setScreen(new OptionsScreen(true));
	} else if (button->id == 3) {
		context->quitToTitle();
	}
}
