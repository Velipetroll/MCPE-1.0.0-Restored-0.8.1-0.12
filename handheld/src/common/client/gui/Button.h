#pragma once

#include "GuiElement.h"
#include <string>

class Button : public GuiElement {
public:
	int id = 0;
	std::string msg;
	bool hovered = false;
	bool pressed = false;

	Button(int id, const std::string& msg);
	Button(int id, int x, int y, const std::string& msg);
	Button(int id, int x, int y, int w, int h, const std::string& msg);

	virtual int getYImage(bool isHovered) const;
	virtual void render(GuiContext* ctx, int mouseX, int mouseY) override;
	virtual bool clicked(GuiContext* ctx, int mouseX, int mouseY) const;
	virtual void released(int mouseX, int mouseY);
	virtual void setPressed();
};
