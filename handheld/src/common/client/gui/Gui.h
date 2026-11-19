#pragma once

#include "GuiComponent.h"

class Options;
class Font;
struct GuiContext;

class Gui : public GuiComponent {
public:
	int selectedSlot = 0;
	int health = 20;
	int maxHealth = 20;

	void render(float partialTicks, int guiW, int guiH, Options* options, Font* font, GuiContext* ctx = nullptr);
	bool handleHudClick(int guiX, int guiY, int guiW, int guiH, GuiContext* ctx);

private:
	void renderCrosshair(int guiW, int guiH);
	void renderHotbar(int guiW, int guiH, Font* font, GuiContext* ctx);
	void renderHearts(int guiW, int guiH);
	void renderTouchButtons(int guiW, int guiH, Font* font);
	void renderChat(int guiW, int guiH, Font* font, GuiContext* ctx);
};
