#pragma once

#include "Screen.h"

class StartMenuScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;

private:
	Button* mPlayButton = nullptr;
	Button* mRealmsButton = nullptr;
	Button* mOptionsButton = nullptr;
	Button* mQuitButton = nullptr;
	float mSplashTimer = 0.0f;
	std::string mSplashText;
};
