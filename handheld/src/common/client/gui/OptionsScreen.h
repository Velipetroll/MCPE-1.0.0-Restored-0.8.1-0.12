#pragma once

#include "Screen.h"
#include "Slider.h"
#include "OptionsPane.h"
#include "NinePatchLayer.h"

class OptionsScreen : public Screen {
public:
	explicit OptionsScreen(bool fromPause = false);
	~OptionsScreen() override;

	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	bool handleBackEvent(bool isDown = false) override;
	bool renderGameBehind() const override { return mFromPause; }

private:
	bool mFromPause = false;
	Button* mBackButton = nullptr;
	Button* mInvertMouseButton = nullptr;
	Button* mViewBobbingButton = nullptr;
	Button* mFancyGraphicsButton = nullptr;
	Button* mUiStyleButton = nullptr;
	Button* mThirdPersonButton = nullptr;
	Button* mServerVisibleButton = nullptr;
	Button* mLeftHandedButton = nullptr;
	Button* mFancySkiesButton = nullptr;
	Button* mAnimateTexturesButton = nullptr;
	Button* mHideGuiButton = nullptr;
	Button* mShowFpsButton = nullptr;
	Button* mGreedyMeshingButton = nullptr;

	Slider* mMusicSlider = nullptr;
	Slider* mSoundSlider = nullptr;
	Slider* mSensitivitySlider = nullptr;
	Slider* mRenderDistanceSlider = nullptr;
	Slider* mGuiScaleSlider = nullptr;
	Slider* mDifficultySlider = nullptr;
	Slider* mFpsLimitSlider = nullptr;

	int mSelectedCategory = 0;
	std::vector<Button*> mCategoryButtons;
	std::vector<OptionsPane*> mOptionPanes;
	NinePatchLayer* mCatNormal = nullptr;
	NinePatchLayer* mCatSelected = nullptr;

	void updateButtonLabels();
};
