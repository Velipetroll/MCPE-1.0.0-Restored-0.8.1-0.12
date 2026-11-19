#include "Slider.h"
#include "GuiContext.h"
#include "Font.h"
#include <algorithm>
#include <cmath>

Slider::Slider(Options* options, Options::Option option, float minVal, float maxVal)
	: GuiElement(0, 0, 200, 20)
	, mOptions(options)
	, mOption(option)
	, mIsStepSlider(false)
	, mMinValue(minVal)
	, mMaxValue(maxVal) {
	if (mOptions) {
		float val = mOptions->getProgressValue(mOption);
		float range = mMaxValue - mMinValue;
		mProgress = (range > 0.0001f) ? (val - mMinValue) / range : 0.0f;
		mProgress = std::max(0.0f, std::min(1.0f, mProgress));
	}
}

Slider::Slider(Options* options, Options::Option option, const std::vector<int>& steps)
	: GuiElement(0, 0, 200, 20)
	, mOptions(options)
	, mOption(option)
	, mIsStepSlider(true)
	, mSteps(steps) {
	if (mOptions && !mSteps.empty()) {
		int cur = mOptions->getIntValue(mOption);
		int bestIdx = 0;
		for (size_t i = 0; i < mSteps.size(); ++i) {
			if (mSteps[i] == cur) {
				bestIdx = static_cast<int>(i);
				break;
			}
		}
		mSelectedStep = bestIdx;
		mProgress = (mSteps.size() > 1) ? static_cast<float>(bestIdx) / static_cast<float>(mSteps.size() - 1) : 0.0f;
	}
}

void Slider::mouseClicked(GuiContext* ctx, int x, int y, int button) {
	if (!active || !visible || button != 0) return;
	if (pointInside(x, y)) {
		mDragging = true;
		mLastMouseX = x;
		if (width > 10) {
			mProgress = std::max(0.0f, std::min(1.0f, static_cast<float>(x - (posX + 5)) / static_cast<float>(width - 10)));
		}
		setOption(ctx);
	}
}

void Slider::mouseReleased(GuiContext* ctx, int x, int y, int button) {
	if (!mDragging) return;
	mDragging = false;
	if (mIsStepSlider && mSteps.size() > 1) {
		int idx = static_cast<int>(std::round(mProgress * static_cast<float>(mSteps.size() - 1)));
		idx = std::max(0, std::min(static_cast<int>(mSteps.size()) - 1, idx));
		mSelectedStep = idx;
		mProgress = static_cast<float>(idx) / static_cast<float>(mSteps.size() - 1);
	}
	setOption(ctx);
}

void Slider::tick(GuiContext* ctx) {
	if (mDragging && width > 10) {
		mProgress = std::max(0.0f, std::min(1.0f, static_cast<float>(mLastMouseX - (posX + 5)) / static_cast<float>(width - 10)));
		setOption(ctx);
	}
}

void Slider::setOption(GuiContext* ctx) {
	Options* opts = mOptions ? mOptions : (ctx ? ctx->options : nullptr);
	if (!opts) return;

	if (mIsStepSlider && !mSteps.empty()) {
		int idx = static_cast<int>(std::round(mProgress * static_cast<float>(mSteps.size() - 1)));
		idx = std::max(0, std::min(static_cast<int>(mSteps.size()) - 1, idx));
		mSelectedStep = idx;
		opts->set(mOption, mSteps[idx]);
	} else {
		float val = mMinValue + (mMaxValue - mMinValue) * mProgress;
		opts->set(mOption, val);
	}
}

void Slider::render(GuiContext* ctx, int mouseX, int mouseY) {
	if (!visible) return;
	mLastMouseX = mouseX;

	if (mDragging && width > 10) {
		mProgress = std::max(0.0f, std::min(1.0f, static_cast<float>(mouseX - (posX + 5)) / static_cast<float>(width - 10)));
		setOption(ctx);
	}

	// Base bar
	fill(posX + 5, posY + 7, posX + width - 5, posY + 10, 0xFF707070);

	// Step ticks
	if (mIsStepSlider && mSteps.size() > 1) {
		for (size_t i = 0; i < mSteps.size(); ++i) {
			float t = static_cast<float>(i) / static_cast<float>(mSteps.size() - 1);
			int tx = posX + 5 + static_cast<int>((width - 10) * t);
			fill(tx - 1, posY + 5, tx + 1, posY + 12, 0xFF909090);
		}
	}

	// Knob
	int knobX = posX + 5 + static_cast<int>((width - 10) * mProgress) - 5;
	if (bindTexture("textures/gui/touchgui.png")) {
		blit(knobX, posY, 225, 125, 11, 17, 256, 256);
	} else {
		fill(knobX, posY + 1, knobX + 11, posY + 18, 0xFFC0C0C0);
		drawRect(knobX, posY + 1, knobX + 11, posY + 18, 0xFF000000);
	}

	// Label
	Options* opts = mOptions ? mOptions : (ctx ? ctx->options : nullptr);
	if (opts && ctx && ctx->font) {
		std::string label = opts->getMessage(mOption);
		drawCenteredString(ctx->font, label, posX + width / 2, posY + (height - 8) / 2, 0xFFFFFFFF);
	}
}
