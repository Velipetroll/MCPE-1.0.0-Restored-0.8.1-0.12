#include "PauseScreen.h"
#include "OptionsScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"

void PauseScreen::init() {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		auto* header = new Touch::THeader(0, "Game Menu");
		header->posX = 0;
		header->posY = 0;
		header->width = width;
		header->height = 26;
		buttons.push_back(header);

		mResumeButton = new Touch::TButton(1, "Back to Game");
		mOptionsButton = new Touch::TButton(2, "Options");
		mQuitToTitleButton = new Touch::TButton(3, "Quit to Title");
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
	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);

	bool touch = context && context->options && context->options->useTouchUI;
	if (!touch && font) {
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
