#pragma once

#include "GuiElement.h"
#include "Options.h"
#include <string>
#include <vector>

class Slider : public GuiElement {
public:
	Slider(Options* options, Options::Option option, float minVal = 0.0f, float maxVal = 1.0f);
	Slider(Options* options, Options::Option option, const std::vector<int>& steps);

	void render(GuiContext* ctx, int mouseX, int mouseY) override;
	void tick(GuiContext* ctx) override;
	void mouseClicked(GuiContext* ctx, int x, int y, int button) override;
	void mouseReleased(GuiContext* ctx, int x, int y, int button) override;

	void setOption(GuiContext* ctx);

private:
	Options* mOptions = nullptr;
	Options::Option mOption;
	bool mIsStepSlider = false;
	float mMinValue = 0.0f;
	float mMaxValue = 1.0f;
	float mProgress = 0.0f;
	std::vector<int> mSteps;
	int mSelectedStep = 0;
	bool mDragging = false;
	int mLastMouseX = 0;
};
