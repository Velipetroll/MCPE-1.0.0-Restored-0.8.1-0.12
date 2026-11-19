#pragma once

#include "Button.h"
#include "NinePatchLayer.h"
#include "Options.h"
#include <string>

namespace Touch {

class TButton : public Button {
public:
	TButton(int id, const std::string& msg);
	TButton(int id, int x, int y, int w, int h, const std::string& msg);

	void render(GuiContext* ctx, int mouseX, int mouseY) override;

private:
	NinePatchLayer mNormalBg;
	NinePatchLayer mPressedBg;
};

class THeader : public Button {
public:
	THeader(int id, const std::string& msg);

	void render(GuiContext* ctx, int mouseX, int mouseY) override;
};

} // namespace Touch

class ImageButton : public Button {
public:
	ImageButton(int id, const std::string& iconTexture, int u, int v, int uw, int vh, bool withBackground = false);

	void render(GuiContext* ctx, int mouseX, int mouseY) override;

private:
	std::string mIconTexture;
	int mU = 0;
	int mV = 0;
	int mUW = 0;
	int mVH = 0;
	bool mWithBackground = false;
	NinePatchLayer mBgNormal;
	NinePatchLayer mBgPressed;
};

class TouchSwitch : public GuiElement {
public:
	TouchSwitch(Options* options, Options::Option option);

	void render(GuiContext* ctx, int mouseX, int mouseY) override;
	void mouseClicked(GuiContext* ctx, int x, int y, int button) override;

private:
	Options* mOptions = nullptr;
	Options::Option mOption;
};
