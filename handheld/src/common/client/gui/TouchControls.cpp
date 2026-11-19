#include "TouchControls.h"
#include "GuiContext.h"
#include "Font.h"
#include <glad/glad.h>

namespace Touch {

TButton::TButton(int id, const std::string& msg)
	: Button(id, 0, 0, 66, 26, msg)
	, mNormalBg("textures/gui/spritesheet.png", IntRectangle{8, 32, 8, 8}, 2, 2, 2, 2, 256, 256)
	, mPressedBg("textures/gui/spritesheet.png", IntRectangle{0, 32, 8, 8}, 2, 2, 2, 2, 256, 256) {
}

TButton::TButton(int id, int x, int y, int w, int h, const std::string& msg)
	: Button(id, x, y, w, h, msg)
	, mNormalBg("textures/gui/spritesheet.png", IntRectangle{8, 32, 8, 8}, 2, 2, 2, 2, 256, 256)
	, mPressedBg("textures/gui/spritesheet.png", IntRectangle{0, 32, 8, 8}, 2, 2, 2, 2, 256, 256) {
}

void TButton::render(GuiContext* ctx, int mouseX, int mouseY) {
	if (!visible) return;

	hovered = active && pointInside(mouseX, mouseY);
	bool isPressed = active && pressed && hovered;

	if (isPressed) {
		mPressedBg.draw(posX, posY, width, height);
	} else {
		mNormalBg.draw(posX, posY, width, height);
	}

	uint32_t textColor = 0xFFE0E0E0;
	if (!active) {
		textColor = 0xFFA0A0A0;
	} else if (hovered || isPressed) {
		textColor = 0xFFFFFFA0;
	}

	if (ctx && ctx->font && !msg.empty()) {
		drawCenteredString(ctx->font, msg, posX + width / 2, posY + (height - 8) / 2, textColor);
	}
}

THeader::THeader(int id, const std::string& msg)
	: Button(id, 0, 0, 66, 26, msg) {
	active = false;
}

void THeader::render(GuiContext* ctx, int mouseX, int mouseY) {
	if (!visible) return;

	if (bindTexture("textures/gui/touchgui.png")) {
		blitScaled(posX, posY, 2, 25, 150, 26, 2, 25, 256, 256);
		if (width > 4) {
			blitScaled(posX + 2, posY, width - 4, 25, 153, 26, 8, 25, 256, 256);
			blitScaled(posX + 2, posY + 25, width - 4, 3, 153, 52, 8, 3, 256, 256);
		}
		blitScaled(posX + width - 2, posY, 2, 25, 162, 26, 2, 25, 256, 256);
	} else {
		fill(posX, posY, posX + width, posY + height, 0xFF333333);
	}

	if (ctx && ctx->font && !msg.empty()) {
		drawCenteredString(ctx->font, msg, posX + width / 2, posY + (height - 8) / 2, 0xFFE0E0E0);
	}
}

} // namespace Touch

ImageButton::ImageButton(int id, const std::string& iconTexture, int u, int v, int uw, int vh, bool withBackground)
	: Button(id, 0, 0, 32, 32, "")
	, mIconTexture(iconTexture)
	, mU(u)
	, mV(v)
	, mUW(uw)
	, mVH(vh)
	, mWithBackground(withBackground)
	, mBgNormal("textures/gui/spritesheet.png", IntRectangle{112, 0, 8, 67}, 2, 2, 2, 2, 256, 256)
	, mBgPressed("textures/gui/spritesheet.png", IntRectangle{120, 0, 8, 67}, 2, 2, 2, 2, 256, 256) {
}

void ImageButton::render(GuiContext* ctx, int mouseX, int mouseY) {
	if (!visible) return;

	hovered = active && pointInside(mouseX, mouseY);
	bool isPressed = active && pressed && hovered;

	if (mWithBackground) {
		if (isPressed) {
			mBgPressed.draw(posX, posY, width, height);
		} else {
			mBgNormal.draw(posX, posY, width, height);
		}
	}

	if (bindTexture(mIconTexture)) {
		float scale = isPressed ? 0.95f : 1.0f;
		int drawW = static_cast<int>(mUW * scale);
		int drawH = static_cast<int>(mVH * scale);
		int drawX = posX + (width - drawW) / 2;
		int drawY = posY + (height - drawH) / 2;

		if (!active) {
			glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
		} else {
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		}
		blitScaled(drawX, drawY, drawW, drawH, mU, mV, mUW, mVH, 256, 256);
	}
}

TouchSwitch::TouchSwitch(Options* options, Options::Option option)
	: GuiElement(0, 0, 38, 20)
	, mOptions(options)
	, mOption(option) {
}

void TouchSwitch::render(GuiContext* ctx, int mouseX, int mouseY) {
	if (!visible) return;
	bool on = mOptions ? mOptions->getBooleanValue(mOption) : false;
	if (bindTexture("textures/gui/touchgui.png")) {
		int u = on ? 198 : 160;
		blit(posX, posY, u, 206, 38, 20, 256, 256);
	} else {
		fill(posX, posY, posX + 38, posY + 20, on ? 0xFF33AA33 : 0xFF666666);
	}
}

void TouchSwitch::mouseClicked(GuiContext* ctx, int x, int y, int button) {
	if (!active || !visible || button != 0) return;
	if (pointInside(x, y) && mOptions) {
		mOptions->toggle(mOption);
	}
}
