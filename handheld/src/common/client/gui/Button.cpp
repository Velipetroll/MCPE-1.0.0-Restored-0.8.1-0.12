#include "Button.h"
#include "GuiContext.h"
#include "Font.h"

Button::Button(int id, const std::string& msg)
	: GuiElement(0, 0, 200, 20), id(id), msg(msg) {
}

Button::Button(int id, int x, int y, const std::string& msg)
	: GuiElement(x, y, 200, 20), id(id), msg(msg) {
}

Button::Button(int id, int x, int y, int w, int h, const std::string& msg)
	: GuiElement(x, y, w, h), id(id), msg(msg) {
}

int Button::getYImage(bool isHovered) const {
	if (!active) return 0;
	if (isHovered || pressed) return 2;
	return 1;
}

void Button::render(GuiContext* ctx, int mouseX, int mouseY) {
	if (!visible) return;

	hovered = active && pointInside(mouseX, mouseY);
	int yImage = getYImage(hovered);

	if (bindTexture("textures/gui/gui.png")) {
		int halfW = width / 2;
		int rightW = width - halfW;
		int v = 46 + yImage * 20;
		blitScaled(posX, posY, halfW, height, 0, v, halfW, 20, 256, 256);
		blitScaled(posX + halfW, posY, rightW, height, 200 - rightW, v, rightW, 20, 256, 256);
	} else {
		uint32_t bg = !active ? 0xFF444444 : (hovered || pressed ? 0xFF6B7EBF : 0xFF707070);
		fill(posX, posY, posX + width, posY + height, bg);
		drawRect(posX, posY, posX + width, posY + height, 0xFF000000);
	}

	uint32_t textColor = 0xFFE0E0E0;
	if (!active) {
		textColor = 0xFFA0A0A0;
	} else if (hovered || pressed) {
		textColor = 0xFFFFFFA0;
	}

	if (ctx && ctx->font) {
		drawCenteredString(ctx->font, msg, posX + width / 2, posY + (height - 8) / 2, textColor);
	}
}

bool Button::clicked(GuiContext* ctx, int mouseX, int mouseY) const {
	return active && visible && pointInside(mouseX, mouseY);
}

void Button::released(int mouseX, int mouseY) {
	pressed = false;
}

void Button::setPressed() {
	pressed = true;
}
