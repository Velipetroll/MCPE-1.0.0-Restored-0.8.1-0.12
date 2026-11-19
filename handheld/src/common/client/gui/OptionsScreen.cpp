#include "OptionsScreen.h"
#include "StartMenuScreen.h"
#include "PauseScreen.h"
#include "TouchControls.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <glad/glad.h>

namespace {
class TouchCategoryButton : public Button {
public:
	TouchCategoryButton(int id, int iconU, int iconV, NinePatchLayer* normalBg, NinePatchLayer* selectedBg, int* selectedIdx, int myIdx)
		: Button(id, 0, 0, 28, 28, "")
		, mIconU(iconU)
		, mIconV(iconV)
		, mNormalBg(normalBg)
		, mSelectedBg(selectedBg)
		, mSelectedIdx(selectedIdx)
		, mMyIdx(myIdx) {}

	void render(GuiContext* ctx, int mouseX, int mouseY) override {
		if (!visible) return;
		bool isSelected = (mSelectedIdx && *mSelectedIdx == mMyIdx);
		bool isHovered = active && pointInside(mouseX, mouseY);
		NinePatchLayer* bg = (isSelected || (pressed && isHovered)) ? mSelectedBg : mNormalBg;
		if (bg) {
			bg->draw(posX, posY, width, height);
		}
		if (bindTexture("textures/gui/touchgui2.png")) {
			blitScaled(posX, posY, width, height, mIconU, mIconV, 28, 28, 256, 256);
		}
	}

private:
	int mIconU;
	int mIconV;
	NinePatchLayer* mNormalBg;
	NinePatchLayer* mSelectedBg;
	int* mSelectedIdx;
	int mMyIdx;
};
} // namespace

OptionsScreen::OptionsScreen(bool fromPause)
	: mFromPause(fromPause) {
}

OptionsScreen::~OptionsScreen() {
	delete mCatNormal;
	delete mCatSelected;
}

static Slider* makeTouchSlider(Options* opts, Options::Option opt, float minV, float maxV) {
	auto* s = new Slider(opts, opt, minV, maxV);
	s->width = 100;
	s->height = 20;
	return s;
}

static Slider* makeTouchStepSlider(Options* opts, Options::Option opt, const std::vector<int>& steps) {
	auto* s = new Slider(opts, opt, steps);
	s->width = 100;
	s->height = 20;
	return s;
}

void OptionsScreen::init() {
	Options* opts = context ? context->options : nullptr;
	bool touch = opts && opts->useTouchUI;

	mCategoryButtons.clear();
	mOptionPanes.clear();

	if (touch) {
		auto* header = new Touch::THeader(0, "Options");
		buttons.push_back(header);

		mBackButton = new Touch::TButton(1, "Back");
		buttons.push_back(mBackButton);

		delete mCatNormal;
		delete mCatSelected;
		mCatNormal = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 8, 32, 8, 8 }, 2, 2, 2, 2, 256, 256);
		mCatSelected = new NinePatchLayer("textures/gui/spritesheet.png", IntRectangle{ 0, 32, 8, 8 }, 2, 2, 2, 2, 256, 256);

		static const int iconU[4] = { 134, 106, 134, 106 };
		static const int iconV[4] = { 0, 0, 28, 28 };
		for (int i = 0; i < 4; ++i) {
			auto* catBtn = new TouchCategoryButton(100 + i, iconU[i], iconV[i], mCatNormal, mCatSelected, &mSelectedCategory, i);
			mCategoryButtons.push_back(catBtn);
			buttons.push_back(catBtn);
		}

		// Tab 0: Game
		auto* paneGame = new OptionsPane();
		auto* grpGame = paneGame->createOptionsGroup("Game");
		auto* nameBtn = new Touch::TButton(200, opts ? opts->username : "Steve");
		nameBtn->width = 100; nameBtn->height = 18; nameBtn->active = false;
		grpGame->addOptionItem("Name", nameBtn);
		grpGame->addOptionItem("Difficulty", makeTouchStepSlider(opts, Options::Option::DIFFICULTY, { 0, 1, 2, 3 }));
		grpGame->addOptionItem("Third Person", new TouchSwitch(opts, Options::Option::THIRD_PERSON));
		grpGame->addOptionItem("Local Server Multiplayer", new TouchSwitch(opts, Options::Option::SERVER_VISIBLE));
		grpGame->addOptionItem("Touch UI", new TouchSwitch(opts, Options::Option::USE_TOUCH_UI));

		auto* grpRealms = paneGame->createOptionsGroup("Realms");
		auto* loginBtn = new Touch::TButton(201, "Login");
		loginBtn->width = 100; loginBtn->height = 20;
		grpRealms->addOptionItem("Invited to Realms Alpha?", loginBtn);

		// Tab 1: Controls
		auto* paneControls = new OptionsPane();
		auto* grpInput = paneControls->createOptionsGroup("Controls");
		grpInput->addOptionItem("Sensitivity", makeTouchSlider(opts, Options::Option::SENSITIVITY, 0.0f, 1.0f));
		grpInput->addOptionItem("Invert Y-Axis", new TouchSwitch(opts, Options::Option::INVERT_MOUSE));
		grpInput->addOptionItem("Lefty", new TouchSwitch(opts, Options::Option::LEFT_HANDED));
		grpInput->addOptionItem("Use Touch Joypad", new TouchSwitch(opts, Options::Option::USE_TOUCH_JOYPAD));
		grpInput->addOptionItem("D-Pad Size", makeTouchSlider(opts, Options::Option::PIXELS_PER_MILLIMETER, 0.0f, 1.0f));
		auto* grpFeedback = paneControls->createOptionsGroup("Feedback");
		grpFeedback->addOptionItem("Vibrate on Destroy", new TouchSwitch(opts, Options::Option::DESTROY_VIBRATION));

		// Tab 2: Graphics
		auto* paneGraphics = new OptionsPane();
		auto* grpGraphics = paneGraphics->createOptionsGroup("Graphics");
		grpGraphics->addOptionItem("View Distance", makeTouchStepSlider(opts, Options::Option::RENDER_DISTANCE, { 0, 1, 2, 3 }));
		grpGraphics->addOptionItem("GUI Scale", makeTouchStepSlider(opts, Options::Option::GUI_SCALE, { 1, 2, 3, 4 }));
		grpGraphics->addOptionItem("Fancy Graphics", new TouchSwitch(opts, Options::Option::FANCY_GRAPHICS));
		grpGraphics->addOptionItem("Beautiful Skies", new TouchSwitch(opts, Options::Option::FANCY_SKIES));
		grpGraphics->addOptionItem("Animated Water", new TouchSwitch(opts, Options::Option::ANIMATE_TEXTURES));
		auto* grpExp = paneGraphics->createOptionsGroup("Experimental");
		grpExp->addOptionItem("Hide GUI", new TouchSwitch(opts, Options::Option::HIDE_GUI));

		// Tab 3: Sound
		auto* paneSound = new OptionsPane();
		auto* grpAudio = paneSound->createOptionsGroup("Audio");
		grpAudio->addOptionItem("Sound Volume", makeTouchSlider(opts, Options::Option::SOUND, 0.0f, 1.0f));
		grpAudio->addOptionItem("Music Volume", makeTouchSlider(opts, Options::Option::MUSIC, 0.0f, 1.0f));

		mOptionPanes = { paneGame, paneControls, paneGraphics, paneSound };
		for (OptionsPane* p : mOptionPanes) {
			elements.push_back(p);
		}
		return;
	}

	// PC Mode
	mMusicSlider = new Slider(opts, Options::Option::MUSIC, 0.0f, 1.0f);
	mSoundSlider = new Slider(opts, Options::Option::SOUND, 0.0f, 1.0f);
	mSensitivitySlider = new Slider(opts, Options::Option::SENSITIVITY, 0.0f, 1.0f);
	mRenderDistanceSlider = new Slider(opts, Options::Option::RENDER_DISTANCE, { 0, 1, 2, 3 });
	mGuiScaleSlider = new Slider(opts, Options::Option::GUI_SCALE, { 1, 2, 3, 4 });
	mDifficultySlider = new Slider(opts, Options::Option::DIFFICULTY, { 0, 1, 2, 3 });

	elements.push_back(mMusicSlider);
	elements.push_back(mSoundSlider);
	elements.push_back(mSensitivitySlider);
	elements.push_back(mRenderDistanceSlider);
	elements.push_back(mGuiScaleSlider);
	elements.push_back(mDifficultySlider);

	mInvertMouseButton = new Button(10, "Invert Mouse");
	mViewBobbingButton = new Button(11, "View Bobbing");
	mFancyGraphicsButton = new Button(12, "Fancy Graphics");
	mUiStyleButton = new Button(13, "UI Style");
	mThirdPersonButton = new Button(14, "Third Person");
	mBackButton = new Button(1, "Done");

	buttons.push_back(mInvertMouseButton);
	buttons.push_back(mViewBobbingButton);
	buttons.push_back(mFancyGraphicsButton);
	buttons.push_back(mUiStyleButton);
	buttons.push_back(mThirdPersonButton);
	buttons.push_back(mBackButton);

	updateButtonLabels();
}

void OptionsScreen::updateButtonLabels() {
	Options* opts = context ? context->options : nullptr;
	if (!opts) return;
	if (mInvertMouseButton) mInvertMouseButton->msg = opts->getMessage(Options::Option::INVERT_MOUSE);
	if (mViewBobbingButton) mViewBobbingButton->msg = opts->getMessage(Options::Option::VIEW_BOBBING);
	if (mFancyGraphicsButton) mFancyGraphicsButton->msg = opts->getMessage(Options::Option::FANCY_GRAPHICS);
	if (mUiStyleButton) mUiStyleButton->msg = opts->getMessage(Options::Option::USE_TOUCH_UI);
	if (mThirdPersonButton) mThirdPersonButton->msg = opts->getMessage(Options::Option::THIRD_PERSON);
}

void OptionsScreen::setupPositions() {
	bool touch = context && context->options && context->options->useTouchUI;

	if (touch) {
		int headerH = 30;
		if (mBackButton) {
			mBackButton->posX = 4;
			mBackButton->posY = 5;
			mBackButton->width = 48;
			mBackButton->height = 20;
		}
		if (!buttons.empty() && buttons[0]->id == 0) {
			buttons[0]->posX = 0;
			buttons[0]->posY = 0;
			buttons[0]->width = width;
			buttons[0]->height = headerH;
		}

		int catSize = 28;
		int v3 = (height - headerH - 3 - (4 * catSize + 2 * 3)) / 2;
		if (v3 < 0) v3 = 0;
		for (size_t i = 0; i < mCategoryButtons.size(); ++i) {
			Button* b = mCategoryButtons[i];
			b->posX = 5;
			b->posY = headerH + 3 + v3 + 29 * static_cast<int>(i);
			b->width = catSize;
			b->height = catSize;
			b->active = (static_cast<int>(i) != mSelectedCategory);
		}

		for (size_t i = 0; i < mOptionPanes.size(); ++i) {
			OptionsPane* p = mOptionPanes[i];
			p->visible = (static_cast<int>(i) == mSelectedCategory);
			p->posX = catSize + 20;
			p->posY = headerH + 3;
			p->width = width - catSize - 20;
			p->height = height - headerH - 3;
			p->setupPositions();
		}
		return;
	}

	// PC Mode layout
	int colW = 150;
	int gap = 10;
	int leftX = width / 2 - colW - gap / 2;
	int rightX = width / 2 + gap / 2;
	int startY = 38;
	int rowH = 24;

	if (mMusicSlider) { mMusicSlider->posX = leftX; mMusicSlider->posY = startY; mMusicSlider->width = colW; mMusicSlider->height = 20; }
	if (mSoundSlider) { mSoundSlider->posX = rightX; mSoundSlider->posY = startY; mSoundSlider->width = colW; mSoundSlider->height = 20; }
	if (mSensitivitySlider) { mSensitivitySlider->posX = leftX; mSensitivitySlider->posY = startY + rowH; mSensitivitySlider->width = colW; mSensitivitySlider->height = 20; }
	if (mRenderDistanceSlider) { mRenderDistanceSlider->posX = rightX; mRenderDistanceSlider->posY = startY + rowH; mRenderDistanceSlider->width = colW; mRenderDistanceSlider->height = 20; }
	if (mGuiScaleSlider) { mGuiScaleSlider->posX = leftX; mGuiScaleSlider->posY = startY + rowH * 2; mGuiScaleSlider->width = colW; mGuiScaleSlider->height = 20; }
	if (mDifficultySlider) { mDifficultySlider->posX = rightX; mDifficultySlider->posY = startY + rowH * 2; mDifficultySlider->width = colW; mDifficultySlider->height = 20; }

	if (mInvertMouseButton) { mInvertMouseButton->posX = leftX; mInvertMouseButton->posY = startY + rowH * 3; mInvertMouseButton->width = colW; mInvertMouseButton->height = 20; }
	if (mViewBobbingButton) { mViewBobbingButton->posX = rightX; mViewBobbingButton->posY = startY + rowH * 3; mViewBobbingButton->width = colW; mViewBobbingButton->height = 20; }
	if (mFancyGraphicsButton) { mFancyGraphicsButton->posX = leftX; mFancyGraphicsButton->posY = startY + rowH * 4; mFancyGraphicsButton->width = colW; mFancyGraphicsButton->height = 20; }
	if (mUiStyleButton) { mUiStyleButton->posX = rightX; mUiStyleButton->posY = startY + rowH * 4; mUiStyleButton->width = colW; mUiStyleButton->height = 20; }
	if (mThirdPersonButton) { mThirdPersonButton->visible = false; }

	if (mBackButton) {
		mBackButton->posX = width / 2 - 100;
		mBackButton->posY = height - 28;
		mBackButton->width = 200;
		mBackButton->height = 20;
	}
}

void OptionsScreen::render(int mouseX, int mouseY, float partialTicks) {
	bool touch = context && context->options && context->options->useTouchUI;
	if (touch) {
		if (!mFromPause) {
			renderMenuBackground(partialTicks);
		}
		fillGradient(0, 0, width, height, 0xC0101010, 0xD0101010);
		fill(0, 0, 38, height, 0xFF958782);
		Screen::render(mouseX, mouseY, partialTicks);
		return;
	}

	if (mFromPause) {
		fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	} else {
		renderDirtBackground();
	}
	if (font) {
		drawCenteredString(font, "Options", width / 2, 16, 0xFFFFFFFF);
	}

	Screen::render(mouseX, mouseY, partialTicks);
}

void OptionsScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	Options* opts = context->options;

	if (button->id == 1) {
		if (mFromPause) {
			context->setScreen(new PauseScreen());
		} else {
			context->setScreen(new StartMenuScreen());
		}
		return;
	}

	if (button->id >= 100 && button->id < 104) {
		mSelectedCategory = button->id - 100;
		setupPositions();
		return;
	}

	if (opts) {
		if (button->id == 10) opts->toggle(Options::Option::INVERT_MOUSE);
		if (button->id == 11) opts->toggle(Options::Option::VIEW_BOBBING);
		if (button->id == 12) opts->toggle(Options::Option::FANCY_GRAPHICS);
		if (button->id == 14) opts->toggle(Options::Option::THIRD_PERSON);
		if (button->id == 13) {
			opts->toggle(Options::Option::USE_TOUCH_UI);
			context->setScreen(new OptionsScreen(mFromPause));
			return;
		}
		updateButtonLabels();
	}
}

bool OptionsScreen::handleBackEvent(bool /*isDown*/) {
	if (!context) return false;
	if (mFromPause) {
		context->setScreen(new PauseScreen());
	} else {
		context->setScreen(new StartMenuScreen());
	}
	return true;
}
