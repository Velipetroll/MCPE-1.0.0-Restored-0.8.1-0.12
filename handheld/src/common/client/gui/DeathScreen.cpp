#include "DeathScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <glad/glad.h>

void DeathScreen::init() {
	mTickCounter = 0;
	mTickAccum = 0.0f;
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		mRespawnButton = new Touch::TButton(1, "Respawn!");
		mMainMenuButton = new Touch::TButton(2, "Main menu");
	} else {
		mRespawnButton = new Button(1, "Respawn!");
		mMainMenuButton = new Button(2, "Main menu");
	}
	buttons.push_back(mRespawnButton);
	buttons.push_back(mMainMenuButton);
}

void DeathScreen::setupPositions() {
	int btnW = width / 4;
	int btnH = (context && context->options && context->options->useTouchUI) ? 24 : 20;
	int posY = height / 2;
	if (mRespawnButton) {
		mRespawnButton->posX = width / 2 - btnW - 10;
		mRespawnButton->posY = posY;
		mRespawnButton->width = btnW;
		mRespawnButton->height = btnH;
	}
	if (mMainMenuButton) {
		mMainMenuButton->posX = width / 2 + 10;
		mMainMenuButton->posY = posY;
		mMainMenuButton->width = btnW;
		mMainMenuButton->height = btnH;
	}
}

void DeathScreen::tick() {
	Screen::tick();
	float dt = context ? std::min(std::max(context->deltaTime, 0.001f), 0.1f) : 0.05f;
	mTickAccum += dt;
	while (mTickAccum >= 0.05f) {
		mTickAccum -= 0.05f;
		++mTickCounter;
	}
}

void DeathScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0x60500000, 0xA0803030);

	if (font) {
		glPushMatrix();
		glScalef(2.0f, 2.0f, 2.0f);
		drawCenteredString(font, "You died!", width / 4, height / 8, 0xFFFFFFFF);
		glPopMatrix();
	}

	if (mTickCounter > 29) {
		Screen::render(mouseX, mouseY, partialTicks);
	}
}

void DeathScreen::buttonClicked(Button* button) {
	if (!context || !button || mTickCounter <= 29) return;
	if (button->id == 1) {
		context->resetSurvivalStats();
		context->respawnRequested = true;
		context->startGame();
	} else if (button->id == 2) {
		context->resetSurvivalStats();
		context->respawnRequested = true;
		context->quitToTitle();
	}
}
