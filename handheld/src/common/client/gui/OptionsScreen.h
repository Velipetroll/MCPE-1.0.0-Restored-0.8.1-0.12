#pragma once

#include "Screen.h"
#include "Slider.h"
#include "OptionsPane.h"

class OptionsScreen : public Screen {
public:
	explicit OptionsScreen(bool fromPause = false);

	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	bool renderGameBehind() const override { return mFromPause; }

private:
	bool mFromPause = false;
	Button* mBackButton = nullptr;
	Button* mInvertMouseButton = nullptr;
	Button* mViewBobbingButton = nullptr;
	Button* mFancyGraphicsButton = nullptr;
	Button* mUiStyleButton = nullptr;
	Button* mThirdPersonButton = nullptr;

	Slider* mMusicSlider = nullptr;
	Slider* mSoundSlider = nullptr;
	Slider* mSensitivitySlider = nullptr;
	Slider* mRenderDistanceSlider = nullptr;
	Slider* mGuiScaleSlider = nullptr;
	Slider* mDifficultySlider = nullptr;

	int mSelectedCategory = 0;
	std::vector<Button*> mCategoryButtons;
	std::vector<OptionsPane*> mOptionPanes;

	void updateButtonLabels();
};
