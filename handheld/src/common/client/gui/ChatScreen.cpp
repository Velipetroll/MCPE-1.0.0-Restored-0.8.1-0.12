#include "ChatScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <algorithm>
#include <vector>

namespace {
std::vector<std::string> wrapTextLines(Font* font, const std::string& text, int maxWidth) {
	std::vector<std::string> lines;
	if (!font || maxWidth <= 0) {
		lines.push_back(text);
		return lines;
	}
	if (text.empty()) {
		lines.push_back("");
		return lines;
	}

	std::string current;
	std::string word;
	for (size_t i = 0; i <= text.size(); ++i) {
		char c = (i < text.size()) ? text[i] : '\0';
		if (c == ' ' || c == '\n' || c == '\0') {
			std::string candidate = current.empty() ? word : (current + " " + word);
			if (!current.empty() && font->width(candidate) > maxWidth) {
				lines.push_back(current);
				current = word;
			} else {
				current = candidate;
			}
			word.clear();

			if (c == '\n') {
				lines.push_back(current);
				current.clear();
			}
		} else {
			word.push_back(c);
			if (font->width(word) > maxWidth && word.size() > 1) {
				std::string part = word.substr(0, word.size() - 1);
				char last = word.back();
				if (!current.empty()) {
					lines.push_back(current);
					current.clear();
				}
				lines.push_back(part);
				word = std::string(1, last);
			}
		}
	}
	if (!current.empty() || lines.empty()) {
		lines.push_back(current);
	}
	return lines;
}
} // namespace

ChatScreen::ChatScreen(const std::string& initialText)
	: mInputText(initialText) {
}

void ChatScreen::init() {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		mCloseButton = new Touch::TButton(2, "Back");
		mSendButton = new Touch::TButton(1, ">");
	} else {
		mCloseButton = new Button(2, "Back");
		mSendButton = new Button(1, ">");
	}
	mCloseButton->width = 38;
	mCloseButton->height = 18;
	mSendButton->width = 20;
	mSendButton->height = 20;
	buttons.push_back(mCloseButton);
	buttons.push_back(mSendButton);
	mHistoryIndex = 0;
}

void ChatScreen::setupPositions() {
	if (mCloseButton) {
		mCloseButton->posX = width - mCloseButton->width - 4;
		mCloseButton->posY = 8;
	}
	if (mSendButton) {
		mSendButton->width = 20;
		mSendButton->height = 20;
		mSendButton->posX = width - mSendButton->width;
		mSendButton->posY = height - mSendButton->height;
	}
}

void ChatScreen::closeWindow() {
	if (context) {
		context->resumeGame();
	}
}

void ChatScreen::sendChatMessage() {
	if (context && !mInputText.empty()) {
		if (mInputText[0] == '/') {
			context->executeCommand(mInputText);
		} else {
			std::string name = context->options ? context->options->username : "Player";
			context->addChatMessage("<" + name + "> " + mInputText);
		}
		mSentHistory.push_back(mInputText);
		mHistoryIndex = static_cast<int>(mSentHistory.size());
		mInputText.clear();
	}
}

void ChatScreen::drawChatMessages(int bottomY) {
	if (!context || !font) return;
	int y = bottomY;
	for (const auto& msg : context->chatMessages) {
		auto lines = wrapTextLines(font, msg.formattedText, width);
		y -= static_cast<int>(lines.size()) * 9;
		uint32_t color = (!msg.rawText.empty() && msg.rawText[0] == '/') ? 0xFFA0A0A0u : 0xFFFFFFFFu;
		int lineY = y;
		for (const auto& line : lines) {
			font->drawShadow(line, 2, lineY, color);
			lineY += 9;
		}
	}
}

void ChatScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xC0101010, 0xD0101010);

	int inputWidth = width - (mSendButton ? mSendButton->width : 20);
	auto inputLines = wrapTextLines(font, mInputText + "_", inputWidth - 4);
	int inputHeight = std::max(static_cast<int>(inputLines.size()) * 9 + 4, mSendButton ? mSendButton->height : 20);
	int inputTop = height - inputHeight;

	drawChatMessages(inputTop);

	Screen::render(mouseX, mouseY, partialTicks);

	fill(0, inputTop, inputWidth, height, 0xFF000000);
	if (font) {
		int lineY = inputTop + 2;
		for (const auto& line : inputLines) {
			font->drawShadow(line, 2, lineY, 0xFFFFFFFF);
			lineY += 9;
		}
	}
}

void ChatScreen::keyPressed(int key) {
	if (key == 256 /* GLFW_KEY_ESCAPE */) {
		closeWindow();
		return;
	}
	if (key == 257 /* GLFW_KEY_ENTER */ || key == 335 /* GLFW_KEY_KP_ENTER */) {
		sendChatMessage();
		return;
	}
	if (key == 259 /* GLFW_KEY_BACKSPACE */) {
		if (!mInputText.empty()) {
			mInputText.pop_back();
		}
		return;
	}
	if (key == 265 /* GLFW_KEY_UP */) {
		if (!mSentHistory.empty()) {
			int sz = static_cast<int>(mSentHistory.size());
			mHistoryIndex = std::max(0, std::min(sz - 1, mHistoryIndex - 1));
			mInputText = mSentHistory[mHistoryIndex];
		}
		return;
	}
	if (key == 264 /* GLFW_KEY_DOWN */) {
		if (!mSentHistory.empty()) {
			int sz = static_cast<int>(mSentHistory.size());
			mHistoryIndex = std::max(0, std::min(sz, mHistoryIndex + 1));
			if (mHistoryIndex == sz) {
				mInputText.clear();
			} else {
				mInputText = mSentHistory[mHistoryIndex];
			}
		}
		return;
	}
	Screen::keyPressed(key);
}

void ChatScreen::charTyped(unsigned int codepoint) {
	if (codepoint >= 32 && codepoint <= 126) {
		if (mInputText.size() < 160) {
			mInputText.push_back(static_cast<char>(codepoint));
		}
	}
}

void ChatScreen::buttonClicked(Button* button) {
	if (!button) return;
	if (button == mCloseButton) {
		closeWindow();
	} else if (button == mSendButton) {
		sendChatMessage();
	}
}

bool ChatScreen::handleBackEvent(bool isDown) {
	if (!isDown) {
		closeWindow();
	}
	return true;
}
