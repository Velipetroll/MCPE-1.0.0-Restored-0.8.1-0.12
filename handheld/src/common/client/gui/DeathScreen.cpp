#include "DeathScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <glad/glad.h>

void DeathScreen::init() {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		mRespawnButton = new Touch::TButton(1, "Respawn");
		mMainMenuButton = new Touch::TButton(2, "Main Menu");
	} else {
		mRespawnButton = new Button(1, "Respawn");
		mMainMenuButton = new Button(2, "Main Menu");
	}
	buttons.push_back(mRespawnButton);
	buttons.push_back(mMainMenuButton);
}

void DeathScreen::setupPositions() {
	int btnW = 160;
	int btnH = (context && context->options && context->options->useTouchUI) ? 24 : 20;
	if (mRespawnButton) {
		mRespawnButton->posX = width / 2 - btnW / 2;
		mRespawnButton->posY = height / 2 - 4;
		mRespawnButton->width = btnW;
		mRespawnButton->height = btnH;
	}
	if (mMainMenuButton) {
		mMainMenuButton->posX = width / 2 - btnW / 2;
		mMainMenuButton->posY = height / 2 + 24;
		mMainMenuButton->width = btnW;
		mMainMenuButton->height = btnH;
	}
}

void DeathScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0x60500000, 0x60500000);

	if (font) {
		glPushMatrix();
		glTranslatef(static_cast<float>(width / 2), static_cast<float>(height / 4), 0.0f);
		glScalef(2.0f, 2.0f, 1.0f);
		int tw = font->width("You died!");
		font->drawShadow("You died!", -tw / 2, -4, 0xFFFFFFFF);
		glPopMatrix();
	}

	Screen::render(mouseX, mouseY, partialTicks);
}

void DeathScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (button->id == 1) {
		context->resumeGame();
	} else if (button->id == 2) {
		context->quitToTitle();
	}
}
