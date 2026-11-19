#include "AddExternalServerScreen.h"
#include "SelectWorldScreen.h"
#include "PlayScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"

AddExternalServerScreen::~AddExternalServerScreen() {
	delete mFrame;
}

void AddExternalServerScreen::init() {
	bool touch = context && context->options && context->options->useTouchUI;
	delete mFrame;
	mFrame = nullptr;
	if (touch) {
		mFrame = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 34, 43, 14, 14 }, 3, 3, 3, 3, 256, 256);

		auto* header = new Touch::THeader(0, "Add External Server");
		header->posX = 0;
		header->posY = 0;
		header->width = width;
		header->height = 30;
		buttons.push_back(header);

		mAddButton = new Touch::TButton(1, "Add Server");
		mCancelButton = new Touch::TButton(2, "Back");
	} else {
		mAddButton = new Button(1, "Add Server");
		mCancelButton = new Button(2, "Cancel");
	}
	buttons.push_back(mAddButton);
	buttons.push_back(mCancelButton);
}

void AddExternalServerScreen::setupPositions() {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		int headerH = 30;
		if (!buttons.empty() && buttons[0]->id == 0) {
			buttons[0]->posX = 0;
			buttons[0]->posY = 0;
			buttons[0]->width = width;
			buttons[0]->height = headerH;
		}
		if (mCancelButton) {
			mCancelButton->posX = 4;
			mCancelButton->posY = 5;
			mCancelButton->width = 38;
			mCancelButton->height = 20;
		}
		if (mAddButton) {
			mAddButton->width = 100;
			mAddButton->height = 24;
			mAddButton->posX = (3 * (width - 10)) / 4 - mAddButton->width / 2;
			mAddButton->posY = headerH + 54;
		}
		return;
	}
	if (mAddButton) { mAddButton->posX = width / 2 - 154; mAddButton->posY = height - 32; mAddButton->width = 150; mAddButton->height = 20; }
	if (mCancelButton) { mCancelButton->posX = width / 2 + 4; mCancelButton->posY = height - 32; mCancelButton->width = 150; mCancelButton->height = 20; }
}

void AddExternalServerScreen::render(int mouseX, int mouseY, float partialTicks) {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		renderMenuBackground(partialTicks);
		int headerH = 30;
		if (mFrame) {
			mFrame->draw(5, headerH + 5, width - 10, height - headerH - 10);
		}

		int lx = 10;
		int lw = std::max(80, width / 2 - 20);
		int y0 = headerH + 10;

		if (font) drawString(font, "Server Name", lx, y0, 0xFFE0E0E0);
		fill(lx, y0 + 12, lx + lw, y0 + 30, 0xFF000000);
		drawRect(lx, y0 + 12, lx + lw, y0 + 30, (mFocusedField == 0) ? 0xFFFFFFFF : 0xFFA0A0A0);
		if (font) drawString(font, mServerName + (mFocusedField == 0 ? "_" : ""), lx + 4, y0 + 17, 0xFFE0E0E0);

		int y1 = y0 + 38;
		if (font) drawString(font, "Address", lx, y1, 0xFFE0E0E0);
		fill(lx, y1 + 10, lx + lw, y1 + 28, 0xFF000000);
		drawRect(lx, y1 + 10, lx + lw, y1 + 28, (mFocusedField == 1) ? 0xFFFFFFFF : 0xFFA0A0A0);
		if (font) drawString(font, mAddress + (mFocusedField == 1 ? "_" : ""), lx + 4, y1 + 15, 0xFFE0E0E0);

		int y2 = y1 + 36;
		if (font) drawString(font, "Port", lx, y2, 0xFFE0E0E0);
		fill(lx, y2 + 10, lx + lw, y2 + 28, 0xFF000000);
		drawRect(lx, y2 + 10, lx + lw, y2 + 28, (mFocusedField == 2) ? 0xFFFFFFFF : 0xFFA0A0A0);
		if (font) drawString(font, mPort + (mFocusedField == 2 ? "_" : ""), lx + 4, y2 + 15, 0xFFE0E0E0);

		if (font) {
			drawString(font, "Add server by IP/Address.", width / 2 + 10, headerH + 14, 0xFFE0E0E0);
		}

		Screen::render(mouseX, mouseY, partialTicks);
		return;
	}

	renderDirtBackground();
	if (font) {
		drawCenteredString(font, "Add External Server", width / 2, 16, 0xFFFFFFFF);
	}

	int cx = width / 2 - 100;

	if (font) drawString(font, "Server Name", cx, 34, 0xFFA0A0A0);
	fill(cx, 44, cx + 200, 64, 0xFF000000);
	drawRect(cx, 44, cx + 200, 64, (mFocusedField == 0) ? 0xFFFFFFFF : 0xFFA0A0A0);
	if (font) drawString(font, mServerName + (mFocusedField == 0 ? "_" : ""), cx + 6, 50, 0xFFE0E0E0);

	if (font) drawString(font, "IP / Address", cx, 72, 0xFFA0A0A0);
	fill(cx, 82, cx + 200, 102, 0xFF000000);
	drawRect(cx, 82, cx + 200, 102, (mFocusedField == 1) ? 0xFFFFFFFF : 0xFFA0A0A0);
	if (font) drawString(font, mAddress + (mFocusedField == 1 ? "_" : ""), cx + 6, 88, 0xFFE0E0E0);

	if (font) drawString(font, "Port", cx, 110, 0xFFA0A0A0);
	fill(cx, 120, cx + 200, 140, 0xFF000000);
	drawRect(cx, 120, cx + 200, 140, (mFocusedField == 2) ? 0xFFFFFFFF : 0xFFA0A0A0);
	if (font) drawString(font, mPort + (mFocusedField == 2 ? "_" : ""), cx + 6, 126, 0xFFE0E0E0);

	Screen::render(mouseX, mouseY, partialTicks);
}

void AddExternalServerScreen::mouseClicked(int x, int y, int button) {
	if (button == 0) {
		bool touch = context && context->options && context->options->useTouchUI;
		if (touch) {
			int lx = 10;
			int lw = std::max(80, width / 2 - 20);
			int y0 = 30 + 10;
			int y1 = y0 + 38;
			int y2 = y1 + 36;
			if (x >= lx && x < lx + lw && y >= y0 + 12 && y < y0 + 30) mFocusedField = 0;
			else if (x >= lx && x < lx + lw && y >= y1 + 10 && y < y1 + 28) mFocusedField = 1;
			else if (x >= lx && x < lx + lw && y >= y2 + 10 && y < y2 + 28) mFocusedField = 2;
		} else {
			int cx = width / 2 - 100;
			if (x >= cx && x < cx + 200 && y >= 44 && y < 64) mFocusedField = 0;
			else if (x >= cx && x < cx + 200 && y >= 82 && y < 102) mFocusedField = 1;
			else if (x >= cx && x < cx + 200 && y >= 120 && y < 140) mFocusedField = 2;
		}
	}
	Screen::mouseClicked(x, y, button);
}

void AddExternalServerScreen::keyPressed(int key) {
	if (key == 256 /* GLFW_KEY_ESCAPE */) {
		handleBackEvent(true);
		return;
	}
	std::string& target = (mFocusedField == 0) ? mServerName : ((mFocusedField == 1) ? mAddress : mPort);
	if (key == 259 /* GLFW_KEY_BACKSPACE */) {
		if (!target.empty()) target.pop_back();
	}
	Screen::keyPressed(key);
}

void AddExternalServerScreen::charTyped(unsigned int codepoint) {
	std::string& target = (mFocusedField == 0) ? mServerName : ((mFocusedField == 1) ? mAddress : mPort);
	if (codepoint >= 32 && codepoint <= 126) {
		if (target.size() < 28) {
			target.push_back(static_cast<char>(codepoint));
		}
	}
}

void AddExternalServerScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (button->id == 1 || button->id == 2) {
		bool touch = context->options && context->options->useTouchUI;
		if (touch) {
			context->setScreen(new PlayScreen(true));
		} else {
			context->setScreen(new SelectWorldScreen());
		}
	}
}

bool AddExternalServerScreen::handleBackEvent(bool /*isDown*/) {
	if (!context) return false;
	bool touch = context->options && context->options->useTouchUI;
	if (touch) {
		context->setScreen(new PlayScreen(true));
	} else {
		context->setScreen(new SelectWorldScreen());
	}
	return true;
}
