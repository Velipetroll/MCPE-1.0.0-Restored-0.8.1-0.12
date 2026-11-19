#pragma once

#include "Screen.h"
#include "NinePatchLayer.h"

class PauseScreen : public Screen {
public:
	~PauseScreen() override;
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	bool handleBackEvent(bool isDown = false) override;
	bool renderGameBehind() const override { return true; }

private:
	Button* mResumeButton = nullptr;
	Button* mOptionsButton = nullptr;
	Button* mQuitToTitleButton = nullptr;
	NinePatchLayer* mListBg = nullptr;
	NinePatchLayer* mRowBg = nullptr;
};
