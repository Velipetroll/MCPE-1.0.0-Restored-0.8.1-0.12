#include "InBedScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"

void InBedScreen::init() {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		mLeaveBedButton = new Touch::TButton(1, "Leave Bed");
	} else {
		mLeaveBedButton = new Button(1, "Leave Bed");
	}
	buttons.push_back(mLeaveBedButton);
}

void InBedScreen::setupPositions() {
	if (mLeaveBedButton) {
		mLeaveBedButton->posX = width / 2 - 100;
		mLeaveBedButton->posY = height - 40;
		mLeaveBedButton->width = 200;
		mLeaveBedButton->height = 20;
	}
}

void InBedScreen::buttonClicked(Button* button) {
	if (context && button && button->id == 1) {
		context->resumeGame();
	}
}
