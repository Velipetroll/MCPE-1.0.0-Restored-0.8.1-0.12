#include "DeleteWorldScreen.h"
#include "SelectWorldScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <filesystem>

DeleteWorldScreen::DeleteWorldScreen(const std::string& worldName, const std::string& worldPath)
	: mWorldName(worldName), mWorldPath(worldPath) {
}

void DeleteWorldScreen::init() {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		auto* header = new Touch::THeader(0, "Delete World");
		header->posX = 0;
		header->posY = 0;
		header->width = width;
		header->height = 26;
		buttons.push_back(header);

		mDeleteButton = new Touch::TButton(1, "Delete");
		mCancelButton = new Touch::TButton(2, "Cancel");
	} else {
		mDeleteButton = new Button(1, "Delete");
		mCancelButton = new Button(2, "Cancel");
	}
	buttons.push_back(mDeleteButton);
	buttons.push_back(mCancelButton);
}

void DeleteWorldScreen::setupPositions() {
	bool touch = context && context->options && context->options->useTouchUI;
	int btnW = 100;
	int btnH = touch ? 24 : 20;
	int y = height / 2 + 16;

	if (mDeleteButton) { mDeleteButton->posX = width / 2 - btnW - 6; mDeleteButton->posY = y; mDeleteButton->width = btnW; mDeleteButton->height = btnH; }
	if (mCancelButton) { mCancelButton->posX = width / 2 + 6; mCancelButton->posY = y; mCancelButton->width = btnW; mCancelButton->height = btnH; }
}

void DeleteWorldScreen::render(int mouseX, int mouseY, float partialTicks) {
	renderDirtBackground();

	if (font) {
		drawCenteredString(font, "Are you sure you want to delete this world?", width / 2, height / 2 - 28, 0xFFFFFFFF);
		drawCenteredString(font, "'" + mWorldName + "' will be lost forever!", width / 2, height / 2 - 12, 0xFFA0A0A0);
	}

	Screen::render(mouseX, mouseY, partialTicks);
}

void DeleteWorldScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (button->id == 1) {
		std::error_code ec;
		std::filesystem::remove_all(mWorldPath, ec);
		context->setScreen(new SelectWorldScreen());
	} else if (button->id == 2) {
		context->setScreen(new SelectWorldScreen());
	}
}
