#include "DisconnectionScreen.h"
#include "StartMenuScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"

DisconnectionScreen::DisconnectionScreen(const std::string& reason)
	: mReason(reason) {
}

void DisconnectionScreen::init() {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		mOkButton = new Touch::TButton(1, "Ok");
	} else {
		mOkButton = new Button(1, "Ok");
	}
	buttons.push_back(mOkButton);
}

void DisconnectionScreen::setupPositions() {
	if (mOkButton) {
		mOkButton->posX = width / 2 - 64;
		mOkButton->posY = height / 2 + 20;
		mOkButton->width = 128;
		mOkButton->height = (context && context->options && context->options->useTouchUI) ? 24 : 20;
	}
}

void DisconnectionScreen::render(int mouseX, int mouseY, float partialTicks) {
	renderDirtBackground();

	if (font) {
		drawCenteredString(font, mReason, width / 2, height / 2 - 12, 0xFFFFFFFF);
	}

	Screen::render(mouseX, mouseY, partialTicks);
}

void DisconnectionScreen::buttonClicked(Button* button) {
	if (context && button && button->id == 1) {
		context->quitToTitle();
	}
}
