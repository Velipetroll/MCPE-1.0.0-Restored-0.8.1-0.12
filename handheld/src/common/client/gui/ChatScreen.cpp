#include "ChatScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"

ChatScreen::ChatScreen(const std::string& initialText)
	: mInputText(initialText) {
}

void ChatScreen::init() {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		mSendButton = new Touch::TButton(1, "Send");
		mBackButton = new Touch::TButton(2, "Back");
	} else {
		mSendButton = new Button(1, "Send");
		mBackButton = new Button(2, "Back");
	}
	buttons.push_back(mSendButton);
	buttons.push_back(mBackButton);
}

void ChatScreen::setupPositions() {
	if (mSendButton) {
		mSendButton->posX = width - 54;
		mSendButton->posY = height - 24;
		mSendButton->width = 50;
		mSendButton->height = 20;
	}
	if (mBackButton) {
		mBackButton->posX = 4;
		mBackButton->posY = 4;
		mBackButton->width = 44;
		mBackButton->height = 18;
	}
}

void ChatScreen::sendMessage() {
	if (context && !mInputText.empty()) {
		if (mInputText[0] == '/') {
			context->executeCommand(mInputText);
		} else {
			std::string name = context->options ? context->options->username : "Player";
			context->addChatMessage("<" + name + "> " + mInputText);
		}
		mInputText.clear();
	}
}

void ChatScreen::render(int mouseX, int mouseY, float partialTicks) {
	// Chat history
	if (context && font) {
		int startY = height - 38;
		for (int i = static_cast<int>(context->chatHistory.size()) - 1; i >= 0; --i) {
			if (startY < 26) break;
			fill(4, startY - 2, width - 4, startY + 10, 0x80000000);
			drawString(font, context->chatHistory[i], 8, startY, 0xFFFFFFFF);
			startY -= 12;
		}
	}

	// Input bar
	fill(4, height - 24, width - 58, height - 4, 0xC0000000);
	drawRect(4, height - 24, width - 58, height - 4, 0xFF808080);
	if (font) {
		drawString(font, mInputText + "_", 8, height - 18, 0xFFE0E0E0);
	}

	Screen::render(mouseX, mouseY, partialTicks);
}

void ChatScreen::keyPressed(int key) {
	if (key == 256 /* GLFW_KEY_ESCAPE */) {
		if (context) context->resumeGame();
		return;
	}
	if (key == 257 /* GLFW_KEY_ENTER */) {
		sendMessage();
		if (context) context->resumeGame();
		return;
	}
	if (key == 259 /* GLFW_KEY_BACKSPACE */) {
		if (!mInputText.empty()) mInputText.pop_back();
	}
	Screen::keyPressed(key);
}

void ChatScreen::charTyped(unsigned int codepoint) {
	if (codepoint >= 32 && codepoint <= 126) {
		if (mInputText.size() < 64) {
			mInputText.push_back(static_cast<char>(codepoint));
		}
	}
}

void ChatScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (button->id == 1) {
		sendMessage();
		context->resumeGame();
	} else if (button->id == 2) {
		context->resumeGame();
	}
}

bool ChatScreen::handleBackEvent(bool /*isDown*/) {
	if (!context) return false;
	context->resumeGame();
	return true;
}
