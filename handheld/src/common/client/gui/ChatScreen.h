#pragma once

#include "Screen.h"
#include <string>
#include <vector>

class ChatScreen : public Screen {
public:
	explicit ChatScreen(const std::string& initialText = "");

	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	void keyPressed(int key) override;
	void charTyped(unsigned int codepoint) override;
	bool handleBackEvent(bool isDown = false) override;
	bool renderGameBehind() const override { return true; }
	bool isPauseScreen() const override { return false; }

private:
	std::string mInputText;
	std::vector<std::string> mSentHistory;
	int mHistoryIndex = 0;
	Button* mSendButton = nullptr;
	Button* mCloseButton = nullptr;

	void sendChatMessage();
	void closeWindow();
	void drawChatMessages(int bottomY);
};
