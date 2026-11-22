#include "InBedScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <algorithm>

void InBedScreen::init() {
	mSleepTimer = 0.0f;
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
		mLeaveBedButton->width = width / 3;
		mLeaveBedButton->height = static_cast<int>(static_cast<float>(height) * 0.15f);
		mLeaveBedButton->posY = height - static_cast<int>(static_cast<double>(mLeaveBedButton->height) * 1.5);
		mLeaveBedButton->posX = width / 2 - mLeaveBedButton->width / 2;
	}
}

void InBedScreen::tick() {
	Screen::tick();
	if (!context) return;
	float dt = std::min(std::max(context->deltaTime, 0.001f), 0.05f);
	mSleepTimer += dt;
	if (mSleepTimer >= 5.0f) { // 100 ticks at 20 TPS (1:1 0.8.1)
		context->sleepCompleteRequested = true;
		context->resumeGame();
	}
}

void InBedScreen::render(int mouseX, int mouseY, float partialTicks) {
	float progress = std::min(1.0f, mSleepTimer / 5.0f);
	uint32_t alpha = static_cast<uint32_t>(96.0f + progress * 124.0f);
	uint32_t fadeCol = (alpha << 24) | 0x050814;
	fillGradient(0, 0, width, height, fadeCol, fadeCol);

	if (font) {
		drawCenteredString(font, "Sleeping...", width / 2, height / 2 - 20, 0xFFE8E8FF);
	}

	Screen::render(mouseX, mouseY, partialTicks);
}

void InBedScreen::buttonClicked(Button* button) {
	if (context && button && button->id == 1) {
		context->resumeGame();
	}
}

bool InBedScreen::handleBackEvent(bool /*isDown*/) {
	if (!context) return false;
	context->resumeGame();
	return true;
}
