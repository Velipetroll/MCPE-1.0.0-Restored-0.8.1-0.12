#pragma once

#include "Screen.h"
#include "NinePatchLayer.h"
#include <string>

class AddExternalServerScreen : public Screen {
public:
	~AddExternalServerScreen() override;
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	void mouseClicked(int x, int y, int button) override;
	void keyPressed(int key) override;

private:
	std::string mServerName = "External Server";
	std::string mAddress = "127.0.0.1";
	std::string mPort = "19132";
	int mFocusedField = 0; // 0 = name, 1 = address, 2 = port

	Button* mAddButton = nullptr;
	Button* mCancelButton = nullptr;
	NinePatchLayer* mFrame = nullptr;
};
