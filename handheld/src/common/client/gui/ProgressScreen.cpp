#include "ProgressScreen.h"
#include "GuiContext.h"
#include "Font.h"

ProgressScreen::ProgressScreen(const std::string& title)
	: mTitle(title) {
}

ProgressScreen::~ProgressScreen() {
	if (context) {
		context->loadingChunks = false;
	}
}

void ProgressScreen::init() {
	if (context) {
		context->loadingChunks = true;
		context->loadingProgress = 0.0f;
		context->resetSurvivalStats();
		context->respawnRequested = true;
	}
}

void ProgressScreen::tick() {
	if (!context) return;
	mProgress = context->loadingProgress;
	if (mProgress >= 1.0f) {
		context->loadingChunks = false;
		context->startGame();
	}
}

void ProgressScreen::render(int mouseX, int mouseY, float partialTicks) {
	renderDirtBackground();

	int cx = width / 2;
	int cy = height / 2;

	if (font) {
		drawCenteredString(font, mTitle, cx, cy - 20, 0xFFFFFFFF);
		const char* status = (mProgress < 0.3f) ? "Generating chunks..." :
		                     (mProgress < 0.99f) ? "Building terrain..." : "Done!";
		drawCenteredString(font, status, cx, cy - 4, 0xFFA0A0A0);
	}

	int barW = 100;
	int barX = cx - barW / 2;
	int barY = cy + 12;
	fill(barX, barY, barX + barW, barY + 2, 0xFF808080);
	int fillW = static_cast<int>(barW * (mProgress > 1.0f ? 1.0f : mProgress));
	if (fillW > 0) {
		fill(barX, barY, barX + fillW, barY + 2, 0xFF80FF80);
	}

	Screen::render(mouseX, mouseY, partialTicks);
}
