#pragma once

#include "Screen.h"

class DeathScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void tick() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	bool renderGameBehind() const override { return true; }

private:
	Button* mRespawnButton = nullptr;
	Button* mMainMenuButton = nullptr;
	int mTickCounter = 0;
	float mTickAccum = 0.0f;
};
