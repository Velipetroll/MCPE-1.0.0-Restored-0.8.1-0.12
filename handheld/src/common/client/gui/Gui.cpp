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
		health = ctx->health;
		hunger = ctx->hunger;
		armor = getTotalArmorDefense(ctx->armorSlots);
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	renderCrosshair(guiW, guiH);
	renderHotbar(guiW, guiH, font, ctx);
	if (!ctx || !ctx->isCreative) {
		renderHearts(guiW, guiH);
		renderHunger(guiW, guiH);
		if (armor > 0) {
			renderArmor(guiW, guiH);
		}
	}
	if (ctx && ctx->statusBannerTimer > 0.0f && !ctx->statusBannerText.empty() && font) {
		float fade = std::min(1.0f, ctx->statusBannerTimer * 2.0f);
		uint32_t alpha = static_cast<uint32_t>(fade * 255.0f);
		drawCenteredString(font, ctx->statusBannerText, guiW / 2, guiH - 58, (alpha << 24) | 0x00FFFFFF);
	}
	if (options && options->useTouchUI) {
		renderTouchButtons(guiW, guiH, font);
	}
	renderChat(guiW, guiH, font, ctx);
}

bool Gui::handleHudClick(int guiX, int guiY, int guiW, int guiH, GuiContext* ctx) {
	if (!ctx) return false;
	// [...] slot at the end of the hotbar (MCPE 0.8.1: numSlots = 10 -> sp = guiW/2 - 100)
	int dotX = guiW / 2 - 100 + 180;
	int dotY = guiH - 22;
	if (guiX >= dotX && guiX < dotX + 22 && guiY >= dotY && guiY < dotY + 22) {
		if (ctx->isCreative) {
			ctx->setScreen(new CreativeInventoryScreen());
		} else {
			ctx->setScreen(new InventoryScreen());
		}
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
	// MCPE 0.8.1 Gui::renderToolBar: 9 item slots + 1 [...] slot = 10 slots
	const int numSlots = 10;
	int sp = guiW / 2 - 10 * numSlots;
	int sp2 = guiH - 22;

	if (bindTexture("textures/gui/gui.png")) {
		glColor4f(1.0f, 1.0f, 1.0f, 0.65f);
		blit(sp, sp2, 0, 0, 180, 22, 256, 256);
		blit(sp + 180, sp2, 160, 0, 20, 22, 256, 256);
		blit(sp + 20 * numSlots, sp2, 180, 0, 2, 22, 256, 256);
		blit(sp - 1 + 20 * selectedSlot, sp2 - 1, 0, 22, 24, 22, 256, 256);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	} else {
		fill(sp, sp2, sp + 20 * numSlots + 2, sp2 + 22, 0xAA222222);
		drawRect(sp + selectedSlot * 20, sp2, sp + (selectedSlot + 1) * 20, sp2 + 22, 0xFFFFFFFF);
	}

	if (ctx) {
		for (int i = 0; i < numSlots - 1; ++i) {
			const ItemInstance& item = ctx->inventorySlots[static_cast<size_t>(i)];
			if (!item.isNull()) {
				drawItemInSlot(font, item, sp + 4 + i * 20, sp2 + 2);
			}
		}
	}

	// [...] icon in the last slot (MCPE 0.8.1: 10*NumSlots + v7/2 - 16, v8 - 13, 228, 248, 14, 4, 28, 8)
	int dotIconX = 10 * numSlots + guiW / 2 - 16;
	int dotIconY = guiH - 13;
	if (bindTexture("textures/gui/gui.png")) {
		blitScaled(dotIconX, dotIconY, 14, 4, 228, 248, 28, 8, 256, 256);
	} else if (font) {
		drawCenteredString(font, "...", dotIconX + 7, dotIconY - 2, 0xFFE0E0E0);
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

void Gui::renderHunger(int guiW, int guiH) {
	int rightEdge = guiW / 2 + 91;
	int y = guiH - 32;

	if (bindTexture("textures/gui/icons.png")) {
		for (int i = 0; i < 10; ++i) {
			int fx = rightEdge - i * 8 - 9;
			blit(fx, y, 16, 27, 9, 9, 256, 256); // empty drumstick
			if (i * 2 + 1 < hunger) {
				blit(fx, y, 52, 27, 9, 9, 256, 256); // full drumstick
			} else if (i * 2 + 1 == hunger) {
				blit(fx, y, 61, 27, 9, 9, 256, 256); // half drumstick
			}
		}
	} else {
		for (int i = 0; i < 10; ++i) {
			int fx = rightEdge - i * 9 - 8;
			fill(fx, y, fx + 7, y + 7, (i * 2 < hunger) ? 0xFFD27D2D : 0xFF333333);
		}
	}
}

void Gui::renderArmor(int guiW, int guiH) {
	int startX = guiW / 2 - 91;
	int y = guiH - 42;

	if (bindTexture("textures/gui/icons.png")) {
		for (int i = 0; i < 10; ++i) {
			int ax = startX + i * 8;
			if (i * 2 + 1 < armor) {
				blit(ax, y, 34, 9, 9, 9, 256, 256); // full armor
			} else if (i * 2 + 1 == armor) {
				blit(ax, y, 25, 9, 9, 9, 256, 256); // half armor
			} else {
				blit(ax, y, 16, 9, 9, 9, 256, 256); // empty armor
			}
		}
	} else {
		for (int i = 0; i < 10; ++i) {
			int ax = startX + i * 9;
			fill(ax, y, ax + 7, y + 7, (i * 2 < armor) ? 0xFFC0C8D0 : 0xFF333333);
		}
	}
}
