#pragma once

#include "Screen.h"

class InBedScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void tick() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	bool handleBackEvent(bool isDown = false) override;
	bool renderGameBehind() const override { return true; }

private:
	Button* mLeaveBedButton = nullptr;
	float mSleepTimer = 0.0f;
};
