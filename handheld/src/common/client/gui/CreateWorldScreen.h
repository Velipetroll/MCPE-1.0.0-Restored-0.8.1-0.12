#pragma once

#include "Screen.h"
#include <string>

class CreateWorldScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	void mouseClicked(int x, int y, int button) override;
	void keyPressed(int key) override;

private:
	std::string mWorldName = "New World";
	std::string mSeedText = "";
	bool mCreativeMode = false;
	int mFocusedField = 0; // 0 = name, 1 = seed

	Button* mGameModeButton = nullptr;
	Button* mCreateButton = nullptr;
	Button* mCancelButton = nullptr;

	void updateLabels();
};
