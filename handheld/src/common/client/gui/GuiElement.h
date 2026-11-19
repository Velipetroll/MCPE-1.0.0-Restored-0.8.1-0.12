#pragma once

#include "GuiComponent.h"

struct GuiContext;

class GuiElement : public GuiComponent {
public:
	bool active = true;
	bool visible = true;
	int posX = 0;
	int posY = 0;
	int width = 0;
	int height = 0;
	uint32_t color = 0xFFFFFFFF;

	GuiElement(int x = 0, int y = 0, int w = 0, int h = 0)
		: posX(x), posY(y), width(w), height(h) {}
	virtual ~GuiElement() = default;

	virtual void tick(GuiContext* ctx) {}
	virtual void render(GuiContext* ctx, int mouseX, int mouseY) {}
	virtual void setupPositions() {}
	virtual void mouseClicked(GuiContext* ctx, int x, int y, int button) {}
	virtual void mouseReleased(GuiContext* ctx, int x, int y, int button) {}
	virtual void keyPressed(GuiContext* ctx, int key) {}

	bool pointInside(int x, int y) const {
		return x >= posX && y >= posY && x < posX + width && y < posY + height;
	}
};
