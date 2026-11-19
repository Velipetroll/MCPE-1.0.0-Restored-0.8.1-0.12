#pragma once

#include "Screen.h"

class InBedScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void buttonClicked(Button* button) override;
	bool renderGameBehind() const override { return true; }

private:
	Button* mLeaveBedButton = nullptr;
};
