#include "StartMenuScreen.h"
#include "PlayScreen.h"
#include "SelectWorldScreen.h"
#include "OptionsScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <glad/glad.h>
#include <cmath>
#include <random>
#include <chrono>

static const char* gMenuSplashes[] = {
	"Scientific!", "Cooler than Spock!", "Collaborate and listen!", "Never dig down!",
	"Take frequent breaks!", "Not linear!", "Han shot first!", "Nice to meet you!",
	"Buckets of lava!", "Ride the pig!", "Larger than Earth!", "sqrt(-1) love you!",
	"Phobos anomaly!", "Punching wood!", "Falling off cliffs!", "0% sugar!",
	"150% hyperbole!", "Synecdoche!", "Let's danec!", "Seecret Friday update!",
	"Ported implementation!", "Kiss the sky!", "20 GOTO 10!", "Verlet intregration!",
	"Peter Griffin!", "Do not distribute!", "Cogito ergo sum!", "4815162342 lines of code!",
	"A skeleton popped out!", "The Work of Notch!", "The sum of its parts!",
	"BTAF used to be good!", "I miss ADOM!", "umop-apisdn!", "OICU812!",
	"Pocket Edition!", "100% Restored!"
};

void StartMenuScreen::init() {
	static std::mt19937 rng(
		static_cast<unsigned int>(std::random_device{}() ^
		static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()))
	);
	std::uniform_int_distribution<size_t> dist(0, (sizeof(gMenuSplashes) / sizeof(gMenuSplashes[0])) - 1);
	mSplashText = gMenuSplashes[dist(rng)];
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		mPlayButton = new Touch::TButton(1, "Play");
		mRealmsButton = new Touch::TButton(4, "Play on Realms");
		mRealmsButton->active = false;
		mOptionsButton = new ImageButton(2, "textures/gui/touchgui.png", 218, 0, 22, 21, true);
		mQuitButton = nullptr;

		buttons.push_back(mPlayButton);
		buttons.push_back(mRealmsButton);
		buttons.push_back(mOptionsButton);
	} else {
		mPlayButton = new Button(1, "Play");
		mRealmsButton = new Button(4, "Join Game");
		mRealmsButton->active = false;
		mOptionsButton = new Button(2, "Options");
		mQuitButton = new Button(3, "Quit");

		buttons.push_back(mPlayButton);
		buttons.push_back(mRealmsButton);
		buttons.push_back(mOptionsButton);
		buttons.push_back(mQuitButton);
	}
}

void StartMenuScreen::setupPositions() {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		GuiTextureInfo info = loadTexture("textures/gui/title.png");
		float dataW = info.width > 0 ? static_cast<float>(info.width) : 256.0f;
		float dataH = info.height > 0 ? static_cast<float>(info.height) : 64.0f;
		float v10 = std::min(width * 0.5f, dataW * 0.5f);
		int logoH = static_cast<int>((v10 * 2.0f / dataW) * dataH);
		int logoBottom = 12 + logoH;

		int btnW = 140;
		int btnH = 26;
		int cx = (width - btnW) / 2;
		int totalBtnH = btnH * 2 + 6;
		int startY = logoBottom + (height - logoBottom - totalBtnH) / 2;

		if (mPlayButton) {
			mPlayButton->posX = cx;
			mPlayButton->posY = startY;
			mPlayButton->width = btnW;
			mPlayButton->height = btnH;
		}
		if (mRealmsButton) {
			mRealmsButton->posX = cx;
			mRealmsButton->posY = startY + btnH + 6;
			mRealmsButton->width = btnW;
			mRealmsButton->height = btnH;
		}
		if (mOptionsButton) {
			mOptionsButton->posX = width - 32 - 2;
			mOptionsButton->posY = height - 32 - 2;
			mOptionsButton->width = 32;
			mOptionsButton->height = 32;
		}
		return;
	}

	int btnW = 160;
	int btnH = 20;
	int cx = (width - btnW) / 2;

	if (mPlayButton) { mPlayButton->posX = cx; mPlayButton->posY = height / 2 - 3; mPlayButton->width = btnW; mPlayButton->height = btnH; }
	if (mRealmsButton) { mRealmsButton->posX = cx; mRealmsButton->posY = height / 2 + 25; mRealmsButton->width = btnW; mRealmsButton->height = btnH; }
	if (mOptionsButton) { mOptionsButton->posX = cx; mOptionsButton->posY = height / 2 + 55; mOptionsButton->width = 78; mOptionsButton->height = btnH; }
	if (mQuitButton) { mQuitButton->posX = cx + 78 + 4; mQuitButton->posY = height / 2 + 55; mQuitButton->width = 78; mQuitButton->height = btnH; }
}

void StartMenuScreen::render(int mouseX, int mouseY, float partialTicks) {
	mSplashTimer += (context ? context->deltaTime : 0.016f);
	renderMenuBackground(partialTicks);

	int logoX = width / 2 - 110;
	int logoW = 220;
	int logoH = 44;
	if (bindTexture("textures/gui/title.png")) {
		GuiTextureInfo info = loadTexture("textures/gui/title.png");
		float dataW = info.width > 0 ? static_cast<float>(info.width) : 512.0f;
		float dataH = info.height > 0 ? static_cast<float>(info.height) : 64.0f;
		float v10 = std::min(width * 0.5f, dataW * 0.5f);
		logoW = static_cast<int>(v10 * 2.0f);
		logoH = static_cast<int>((logoW / dataW) * dataH);
		logoX = width / 2 - logoW / 2;
		blitScaled(logoX, 12, logoW, logoH, 0, 0, static_cast<int>(dataW), static_cast<int>(dataH), static_cast<int>(dataW), static_cast<int>(dataH));
	} else if (font) {
		drawCenteredString(font, "MINECRAFT PE 1.0.0", width / 2, 36, 0xFFFFFFFF);
	}

	// Animated splash text
	if (font && !mSplashText.empty()) {
		float pulse = std::pow(std::sin(mSplashTimer * 3.14159f * 2.3f), 4.0f) * 0.06f + 1.3f;
		int sw = font->width(mSplashText);
		float maxW = width * 0.3125f;
		if (sw > 0 && sw * pulse > maxW) {
			pulse = maxW / static_cast<float>(sw);
		}
		float logoRight = static_cast<float>(logoX + logoW);
		float logoBottom = static_cast<float>(12 + logoH);
		glPushMatrix();
		glTranslatef(logoRight * 0.71f, logoBottom - 15.0f, 0.0f);
		glRotatef(-20.0f, 0.0f, 0.0f, 1.0f);
		glScalef(pulse, pulse, 1.0f);
		font->drawShadow(mSplashText, -sw / 2, -4, 0xFFFFFF00);
		glPopMatrix();
	}

	bool touch = context && context->options && context->options->useTouchUI;
	if (font) {
		if (touch) {
			drawString(font, "Minecraft PE 1.0.0", 1, height - 20, 0xFFFFFFFF);
			drawString(font, "Mojang AB", 1, height - 10, 0xFFFFFFFF);
		} else {
			drawString(font, "Minecraft PE 1.0.0", 4, height - 12, 0xFFFFFFFF);
			std::string copy = "Mojang AB";
			drawString(font, copy, width - font->width(copy) - 4, height - 12, 0xFFFFFFFF);
		}
	}

	Screen::render(mouseX, mouseY, partialTicks);
}

void StartMenuScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	bool touch = context->options && context->options->useTouchUI;
	if (button->id == 1) {
		if (touch) {
			context->setScreen(new PlayScreen(true));
		} else {
			context->setScreen(new SelectWorldScreen());
		}
	} else if (button->id == 4 && touch) {
		context->setScreen(new PlayScreen(false));
	} else if (button->id == 2) {
		context->setScreen(new OptionsScreen(false));
	} else if (button->id == 3) {
		context->requestQuit();
	}
}
