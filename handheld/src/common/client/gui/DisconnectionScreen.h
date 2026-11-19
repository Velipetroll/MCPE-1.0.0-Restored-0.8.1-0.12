#pragma once

#include "Screen.h"
#include <string>

class DisconnectionScreen : public Screen {
public:
	explicit DisconnectionScreen(const std::string& reason = "Disconnected from server");

	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;

private:
	std::string mReason;
	Button* mOkButton = nullptr;
};
