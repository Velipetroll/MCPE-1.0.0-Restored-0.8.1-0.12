#pragma once

#include "Screen.h"
#include <string>

class ChatScreen : public Screen {
public:
	explicit ChatScreen(const std::string& initialText = "");

	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	void keyPressed(int key) override;
	bool renderGameBehind() const override { return true; }

private:
	std::string mInputText;
	Button* mSendButton = nullptr;
	Button* mBackButton = nullptr;

	void sendMessage();
};
