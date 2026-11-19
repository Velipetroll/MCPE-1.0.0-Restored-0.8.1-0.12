#pragma once

#include "GuiComponent.h"
#include "GuiElement.h"
#include "Button.h"
#include <vector>

struct GuiContext;

class Screen : public GuiComponent {
public:
	int width = 0;
	int height = 0;
	GuiContext* context = nullptr;
	Font* font = nullptr;
	std::vector<Button*> buttons;
	std::vector<GuiElement*> elements;
	Button* lastPressedButton = nullptr;

	virtual ~Screen();

	void init(GuiContext* ctx, int w, int h);
	virtual void init() {}
	virtual void setupPositions() {}
	virtual void tick();
	virtual void render(int mouseX, int mouseY, float partialTicks);
	virtual void buttonClicked(Button* button) {}
	virtual void mouseClicked(int x, int y, int button);
	virtual void mouseReleased(int x, int y, int button);
	virtual void mouseScrolled(double yoffset) {}
	virtual void keyPressed(int key);
	virtual void charTyped(unsigned int codepoint) {}
	virtual bool handleBackEvent(bool isDown = false) { return false; }
	virtual bool renderGameBehind() const { return false; }

	void renderMenuBackground(float partialTicks);
	void renderDirtBackground();
};
