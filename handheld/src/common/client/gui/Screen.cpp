#include "Screen.h"
#include "GuiContext.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

Screen::~Screen() {
	for (Button* b : buttons) {
		delete b;
	}
	for (GuiElement* e : elements) {
		delete e;
	}
	buttons.clear();
	elements.clear();
}

void Screen::init(GuiContext* ctx, int w, int h) {
	context = ctx;
	width = w;
	height = h;
	font = ctx ? ctx->font : nullptr;

	for (Button* b : buttons) delete b;
	for (GuiElement* e : elements) delete e;
	buttons.clear();
	elements.clear();
	lastPressedButton = nullptr;

	init();
	setupPositions();
}

void Screen::tick() {
	for (GuiElement* e : elements) {
		if (e) e->tick(context);
	}
}

void Screen::render(int mouseX, int mouseY, float partialTicks) {
	for (GuiElement* e : elements) {
		if (e && e->visible) {
			e->render(context, mouseX, mouseY);
		}
	}
	for (Button* b : buttons) {
		if (b && b->visible) {
			b->render(context, mouseX, mouseY);
		}
	}
}

void Screen::mouseClicked(int x, int y, int button) {
	if (button == 0) {
		for (Button* b : buttons) {
			if (b && b->clicked(context, x, y)) {
				lastPressedButton = b;
				b->setPressed();
				return;
			}
		}
	}
	for (GuiElement* e : elements) {
		if (e && e->visible) {
			e->mouseClicked(context, x, y, button);
		}
	}
}

void Screen::mouseReleased(int x, int y, int button) {
	for (GuiElement* e : elements) {
		if (e && e->visible) {
			e->mouseReleased(context, x, y, button);
		}
	}
	if (button == 0 && lastPressedButton) {
		Button* releasedBtn = lastPressedButton;
		lastPressedButton = nullptr;
		releasedBtn->released(x, y);
		if (releasedBtn->clicked(context, x, y)) {
			buttonClicked(releasedBtn);
			return;
		}
	}
}

void Screen::keyPressed(int key) {
	for (GuiElement* e : elements) {
		if (e && e->visible) {
			e->keyPressed(context, key);
		}
	}
}

void Screen::renderDirtBackground() {
	if (bindTexture("textures/gui/background.png")) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glColor4f(0.25f, 0.25f, 0.25f, 1.0f);
		glBegin(GL_QUADS);
		float uMax = static_cast<float>(width) / 32.0f;
		float vMax = static_cast<float>(height) / 32.0f;
		glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
		glTexCoord2f(0.0f, vMax); glVertex2f(0.0f, static_cast<float>(height));
		glTexCoord2f(uMax, vMax); glVertex2f(static_cast<float>(width), static_cast<float>(height));
		glTexCoord2f(uMax, 0.0f); glVertex2f(static_cast<float>(width), 0.0f);
		glEnd();
	} else {
		fillGradient(0, 0, width, height, 0xFF202020, 0xFF101010);
	}
}

void Screen::renderMenuBackground(float partialTicks) {
	static float panoramaTime = 0.0f;
	float dt = context ? context->deltaTime : 0.016f;
	panoramaTime += dt * 20.0f;

	static const char* panoramaPaths[6] = {
		"textures/gui/background/panorama_0.png",
		"textures/gui/background/panorama_1.png",
		"textures/gui/background/panorama_2.png",
		"textures/gui/background/panorama_3.png",
		"textures/gui/background/panorama_4.png",
		"textures/gui/background/panorama_5.png"
	};

	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glm::mat4 proj = glm::perspective(glm::radians(100.0f), 1.0f, 0.05f, 10.0f);
	glLoadMatrixf(glm::value_ptr(proj));

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
	glRotatef(std::sin(panoramaTime / 400.0f) * 25.0f + 20.0f, 1.0f, 0.0f, 0.0f);
	glRotatef(-panoramaTime * 0.1f, 0.0f, 1.0f, 0.0f);

	for (int face = 0; face < 6; ++face) {
		glPushMatrix();
		if (face == 1) glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
		if (face == 2) glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
		if (face == 3) glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
		if (face == 4) glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
		if (face == 5) glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);

		if (bindTexture(panoramaPaths[face])) {
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, 1.0f);
			glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, 1.0f);
			glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, 1.0f);
			glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f, 1.0f);
			glEnd();
		}
		glPopMatrix();
	}

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glDepthMask(GL_TRUE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Semi-transparent gradient overlay like MCPE 0.8.1
	fillGradient(0, 0, width, height, 0x59FFFFFF, 0x59000000);
}
