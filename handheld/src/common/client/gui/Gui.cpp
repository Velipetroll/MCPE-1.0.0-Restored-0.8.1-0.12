#include "Gui.h"
#include "GuiContext.h"
#include "ChatScreen.h"
#include "InventoryScreens.h"
#include "PauseScreen.h"
#include "Options.h"
#include "Font.h"
#include "world/crafting/CraftingRecipes.h"
#include <glad/glad.h>

void Gui::render(float partialTicks, int guiW, int guiH, Options* options, Font* font, GuiContext* ctx) {
	if (options && options->hideGUI) return;
	if (ctx) {
		ctx->ensureInventoryInitialized();
		selectedSlot = ctx->selectedHotbarSlot;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	renderCrosshair(guiW, guiH);
	renderHotbar(guiW, guiH, font, ctx);
	renderHearts(guiW, guiH);
	if (options && options->useTouchUI) {
		renderTouchButtons(guiW, guiH, font);
	}
	renderChat(guiW, guiH, font, ctx);
}

bool Gui::handleHudClick(int guiX, int guiY, int guiW, int guiH, GuiContext* ctx) {
	if (!ctx) return false;
	// [...] button to the right of hotbar
	int dotX = guiW / 2 - 91 + 182 + 2;
	int dotY = guiH - 22;
	if (guiX >= dotX && guiX < dotX + 20 && guiY >= dotY && guiY < dotY + 22) {
		ctx->setScreen(new InventoryScreen());
		return true;
	}
	if (!ctx->options || !ctx->options->useTouchUI) return false;
	// Chat icon (guiW - 42, 4, 18, 18)
	if (guiX >= guiW - 42 && guiX < guiW - 24 && guiY >= 4 && guiY < 22) {
		ctx->setScreen(new ChatScreen());
		return true;
	}
	// Pause icon (guiW - 20, 4, 18, 18)
	if (guiX >= guiW - 20 && guiX < guiW - 2 && guiY >= 4 && guiY < 22) {
		ctx->pauseGame();
		return true;
	}
	return false;
}

void Gui::renderTouchButtons(int guiW, int guiH, Font* font) {
	if (bindTexture("textures/gui/gui.png")) {
		blit(guiW - 42, 4, 200, 82, 18, 18, 256, 256); // Chat
		blit(guiW - 20, 4, 200, 64, 18, 18, 256, 256); // Pause
	} else {
		fill(guiW - 42, 4, guiW - 24, 22, 0x80000000);
		if (font) drawCenteredString(font, "T", guiW - 33, 9, 0xFFFFFFFF);
		fill(guiW - 20, 4, guiW - 2, 22, 0x80000000);
		if (font) drawCenteredString(font, "||", guiW - 11, 9, 0xFFFFFFFF);
	}
}

void Gui::renderChat(int guiW, int guiH, Font* font, GuiContext* ctx) {
	if (!ctx || !font || ctx->chatHistory.empty()) return;
	int y = guiH - 48;
	int count = 0;
	for (int i = static_cast<int>(ctx->chatHistory.size()) - 1; i >= 0 && count < 5; --i, ++count) {
		fill(4, y - 1, 220, y + 9, 0x60000000);
		drawString(font, ctx->chatHistory[i], 6, y, 0xFFFFFFFF);
		y -= 10;
	}
}

void Gui::renderCrosshair(int guiW, int guiH) {
	int cx = guiW / 2 - 8;
	int cy = guiH / 2 - 8;
	if (bindTexture("textures/gui/icons.png")) {
		glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR);
		blit(cx, cy, 0, 0, 16, 16, 256, 256);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		fill(guiW / 2 - 4, guiH / 2 - 1, guiW / 2 + 4, guiH / 2 + 1, 0xCCFFFFFF);
		fill(guiW / 2 - 1, guiH / 2 - 4, guiW / 2 + 1, guiH / 2 + 4, 0xCCFFFFFF);
	}
}

void Gui::renderHotbar(int guiW, int guiH, Font* font, GuiContext* ctx) {
	int barW = 182;
	int barH = 22;
	int x = guiW / 2 - barW / 2;
	int y = guiH - barH;

	if (bindTexture("textures/gui/gui.png")) {
		blit(x, y, 0, 0, barW, barH, 256, 256);
		blit(x - 1 + selectedSlot * 20, y - 1, 0, 22, 24, 24, 256, 256);
	} else {
		fill(x, y, x + barW, y + barH, 0xAA222222);
		drawRect(x + selectedSlot * 20, y, x + (selectedSlot + 1) * 20, y + barH, 0xFFFFFFFF);
	}

	if (ctx) {
		for (int i = 0; i < 9; ++i) {
			const ItemInstance& item = ctx->inventorySlots[static_cast<size_t>(i)];
			if (item.isNull()) continue;
			int ix = x + 3 + i * 20;
			int iy = y + 3;
			std::string tex = getItemTexturePath(item);
			if (!tex.empty() && bindTexture(tex)) {
				blitScaled(ix, iy, 16, 16, 0, 0, 16, 16, 16, 16);
			} else {
				fill(ix, iy, ix + 16, iy + 16, 0xFF88AA88);
			}
			if (font && item.getStackSize() > 1) {
				std::string cnt = std::to_string(static_cast<int>(item.getStackSize()));
				font->drawShadow(cnt, ix + 17 - font->width(cnt), iy + 9, 0xFFFFFFFF);
			}
		}
	}

	// [...] button to the right of hotbar
	int dotX = x + barW + 2;
	if (bindTexture("textures/gui/gui.png")) {
		blit(dotX, y, 0, 0, 20, 22, 256, 256);
		blitScaled(dotX + 3, y + 9, 14, 4, 228, 248, 28, 8, 256, 256);
	} else {
		fill(dotX, y, dotX + 20, y + barH, 0x80222222);
		drawRect(dotX, y, dotX + 20, y + barH, 0xFF808080);
		if (font) {
			drawCenteredString(font, "...", dotX + 10, y + 7, 0xFFE0E0E0);
		}
	}
}

void Gui::renderHearts(int guiW, int guiH) {
	int startX = guiW / 2 - 91;
	int y = guiH - 32;

	if (bindTexture("textures/gui/icons.png")) {
		for (int i = 0; i < 10; ++i) {
			int hx = startX + i * 8;
			blit(hx, y, 16, 0, 9, 9, 256, 256); // empty heart container
			if (i * 2 + 1 < health) {
				blit(hx, y, 52, 0, 9, 9, 256, 256); // full heart
			} else if (i * 2 + 1 == health) {
				blit(hx, y, 61, 0, 9, 9, 256, 256); // half heart
			}
		}
	} else {
		for (int i = 0; i < 10; ++i) {
			int hx = startX + i * 9;
			fill(hx, y, hx + 7, y + 7, (i * 2 < health) ? 0xFFFF2020 : 0xFF333333);
		}
	}
}
