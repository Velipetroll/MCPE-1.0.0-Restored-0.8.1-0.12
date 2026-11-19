#include "AddExternalServerScreen.h"
#include "SelectWorldScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"

void AddExternalServerScreen::init() {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		auto* header = new Touch::THeader(0, "Add External Server");
		header->posX = 0;
		header->posY = 0;
		header->width = width;
		header->height = 26;
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
		if (mCancelButton) { mCancelButton->posX = 4; mCancelButton->posY = 3; mCancelButton->width = 48; mCancelButton->height = 20; }
		if (mAddButton) { mAddButton->posX = width / 2 - 100; mAddButton->posY = height - 32; mAddButton->width = 200; mAddButton->height = 24; }
		return;
	}
	if (mAddButton) { mAddButton->posX = width / 2 - 154; mAddButton->posY = height - 32; mAddButton->width = 150; mAddButton->height = 20; }
	if (mCancelButton) { mCancelButton->posX = width / 2 + 4; mCancelButton->posY = height - 32; mCancelButton->width = 150; mCancelButton->height = 20; }
}

void AddExternalServerScreen::render(int mouseX, int mouseY, float partialTicks) {
	renderDirtBackground();

	bool touch = context && context->options && context->options->useTouchUI;
	if (!touch && font) {
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
		int cx = width / 2 - 100;
		if (x >= cx && x < cx + 200 && y >= 44 && y < 64) mFocusedField = 0;
		else if (x >= cx && x < cx + 200 && y >= 82 && y < 102) mFocusedField = 1;
		else if (x >= cx && x < cx + 200 && y >= 120 && y < 140) mFocusedField = 2;
	}
	Screen::mouseClicked(x, y, button);
}

void AddExternalServerScreen::keyPressed(int key) {
	std::string& target = (mFocusedField == 0) ? mServerName : ((mFocusedField == 1) ? mAddress : mPort);
	if (key == 259 /* GLFW_KEY_BACKSPACE */) {
		if (!target.empty()) target.pop_back();
	} else if (key >= 32 && key <= 126) {
		if (target.size() < 28) {
			target.push_back(static_cast<char>(key));
		}
	}
	Screen::keyPressed(key);
}

void AddExternalServerScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (button->id == 1 || button->id == 2) {
		context->setScreen(new SelectWorldScreen());
	}
}
