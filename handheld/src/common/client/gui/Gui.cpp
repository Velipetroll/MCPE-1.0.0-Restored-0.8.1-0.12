#include "Gui.h"
#include "GuiContext.h"
#include "ChatScreen.h"
#include "InventoryScreens.h"
#include "PauseScreen.h"
#include "Options.h"
#include "Font.h"
#include "util/Random.h"
#include "world/attribute/AttributeInstance.h"
#include "world/crafting/CraftingRecipes.h"
#include "world/effect/MobEffect.h"
#include "world/entity/player/Player.h"
#include "world/food/FoodConstants.h"
#include <glad/glad.h>

static bool s_useTouchUI = true;

void Gui::render(float partialTicks, int guiW, int guiH, Options* options, Font* font, GuiContext* ctx) {
	if (options && options->hideGUI) return;
	s_useTouchUI = (!options || options->useTouchUI);
	bool showSurvivalHud = true;
	if (ctx) {
		ctx->ensureInventoryInitialized();
		selectedSlot = ctx->selectedHotbarSlot;
		if (ctx->localPlayer) {
			health = ctx->localPlayer->getHealth();
			maxHealth = ctx->localPlayer->getMaxHealth();
			hunger = (int)ctx->localPlayer->getAttribute(Player::HUNGER).getCurrentValue();
			showSurvivalHud = !ctx->localPlayer->isCreative();
		} else {
			health = ctx->health;
			maxHealth = 20;
			hunger = FoodConstants::MAX_FOOD;
			showSurvivalHud = !ctx->isCreative;
		}
		armor = getTotalArmorDefense(ctx->armorSlots);
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	renderCrosshair(guiW, guiH);
	renderHotbar(guiW, guiH, font, ctx);
	if (showSurvivalHud) {
		renderHearts(guiW, guiH, ctx);
		renderHunger(guiW, guiH, ctx);
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
	if (!ctx->options || !ctx->options->useTouchUI) return false;
	// [...] slot at the end of the hotbar (MCPE 0.8.1: numSlots = 10 -> sp = guiW/2 - 100)
	int dotX = guiW / 2 - 100 + 181;
	int dotY = guiH - 22;
	if (guiX >= dotX && guiX < dotX + 20 && guiY >= dotY && guiY < dotY + 22) {
		if (ctx->isCreative) {
			ctx->setScreen(new CreativeInventoryScreen());
		} else {
			ctx->setScreen(new InventoryScreen());
		}
		return true;
	}
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

void Gui::renderChat(int guiW, int /*guiH*/, Font* font, GuiContext* ctx) {
	if (!ctx || !font) return;
	ctx->tickChatMessages();
	if (dynamic_cast<ChatScreen*>(ctx->currentScreen) != nullptr) return;
	if (ctx->chatMessages.empty()) return;

	int activeCount = 0;
	for (int i = static_cast<int>(ctx->chatMessages.size()) - 1; i >= 0; --i) {
		if (ctx->chatMessages[i].age < ctx->chatMessages[i].maxAge) {
			++activeCount;
		}
	}

	int lineIdx = 0;
	for (int i = static_cast<int>(ctx->chatMessages.size()) - 1; i >= 0; --i) {
		const auto& msg = ctx->chatMessages[i];
		if (msg.age >= msg.maxAge) continue;

		float t = (1.0f - static_cast<float>(msg.age) / static_cast<float>(msg.maxAge)) * 10.0f;
		if (t < 0.0f) t = 0.0f;
		else if (t > 1.0f) t = 1.0f;

		int alpha = static_cast<int>(t * t * 255.0f);
		if (alpha <= 0) continue;

		if (activeCount <= 10) {
			++lineIdx;
			int y = 9 * lineIdx + 18;
			uint32_t bgColor = static_cast<uint32_t>((alpha >> 1) << 24);
			fill(2, y - 1, guiW / 2 + 2, y + 8, bgColor);

			uint32_t baseRgb = (!msg.rawText.empty() && msg.rawText[0] == '/') ? 0x00A0A0A0u : 0x00FFFFFFu;
			uint32_t textColor = (static_cast<uint32_t>(alpha) << 24) | baseRgb;
			font->drawShadow(msg.formattedText, 2, y, textColor);
		} else {
			--activeCount;
		}
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
	// MCPE 0.8.1 Gui::renderToolBar: 9 item slots + 1 [...] slot in Touch UI, 9 slots in PC UI
	const int numSlots = s_useTouchUI ? 10 : 9;
	int sp = guiW / 2 - 10 * numSlots;
	int sp2 = guiH - 22;

	if (bindTexture("textures/gui/gui.png")) {
		glColor4f(1.0f, 1.0f, 1.0f, 0.65f);
		if (s_useTouchUI) {
			blit(sp, sp2, 0, 0, 181, 22, 256, 256);
			blit(sp + 181, sp2, 161, 0, 20, 22, 256, 256);
			blit(sp + 201, sp2, 181, 0, 1, 22, 256, 256);
		} else {
			blit(sp, sp2, 0, 0, 182, 22, 256, 256);
		}
		blit(sp - 1 + 20 * selectedSlot, sp2 - 1, 0, 22, 24, 24, 256, 256);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	} else {
		fill(sp, sp2, sp + 20 * numSlots + 2, sp2 + 22, 0xAA222222);
		drawRect(sp + selectedSlot * 20, sp2, sp + (selectedSlot + 1) * 20, sp2 + 22, 0xFFFFFFFF);
	}

	if (ctx) {
		for (int i = 0; i < 9; ++i) {
			const ItemInstance& item = ctx->inventorySlots[static_cast<size_t>(i)];
			if (!item.isNull()) {
				drawItemInSlot(font, item, sp + 3 + i * 20, sp2 + 3);
			}
		}
	}

	if (s_useTouchUI) {
		// [...] icon in the last slot (MCPE 0.8.1: 10*NumSlots + v7/2 - 16, v8 - 13, 228, 248, 14, 4, 28, 8)
		int dotIconX = 10 * numSlots + guiW / 2 - 16;
		int dotIconY = guiH - 13;
		if (bindTexture("textures/gui/gui.png")) {
			blitScaled(dotIconX, dotIconY, 14, 4, 228, 248, 28, 8, 256, 256);
		} else if (font) {
			drawCenteredString(font, "...", dotIconX + 7, dotIconY - 2, 0xFFE0E0E0);
		}
	}
}

void Gui::renderHearts(int guiW, int guiH, GuiContext* ctx) {
	(void)ctx;
	const int numSlots = s_useTouchUI ? 10 : 9;
	int startX = guiW / 2 - 10 * numSlots;
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

void Gui::renderHunger(int guiW, int guiH, GuiContext* ctx) {
	Player* player = ctx ? ctx->localPlayer : nullptr;
	if (player && player->isCreative()) {
		return;
	}

	int hungerVal = player ? (int)player->getAttribute(Player::HUNGER).getCurrentValue() : hunger;
	int satVal = player ? (int)player->getAttribute(Player::SATURATION).getCurrentValue() : (int)FoodConstants::START_SATURATION;
	bool hasHungerEffect = player && MobEffect::HUNGER && player->hasEffect(*MobEffect::HUNGER);
	int tickCount = ctx ? ctx->hudTickCount : 0;

	const int numSlots = s_useTouchUI ? 10 : 9;
	int rightEdge = guiW / 2 + 10 * numSlots + 2;
	int baseY = guiH - 32;
	const int NUM_ICONS = FoodConstants::MAX_FOOD / 2;

	int bgU = hasHungerEffect ? 133 : 16;
	int fullU = (hasHungerEffect ? 52 : 16) + 36;
	int halfU = (hasHungerEffect ? 52 : 16) + 45;
	Random rand;
	int shakeMod = std::max(0, hungerVal) * 3 + 1;
	bool doShake = (satVal <= 0) && ((tickCount % shakeMod) == 0);

	if (bindTexture("textures/gui/icons.png")) {
		for (int i = 0; i < NUM_ICONS; ++i) {
			int fx = rightEdge - i * 8 - 9;
			int y = doShake ? (baseY - 1 + (rand.nextUnsignedInt() & 1)) : baseY;
			blit(fx, y, bgU, 27, 9, 9, 256, 256);
			int threshold = i * 2 + 1;
			if (threshold < hungerVal) {
				blit(fx, y, fullU, 27, 9, 9, 256, 256);
			} else if (threshold == hungerVal) {
				blit(fx, y, halfU, 27, 9, 9, 256, 256);
			}
		}
	} else {
		for (int i = 0; i < NUM_ICONS; ++i) {
			int fx = rightEdge - i * 9 - 8;
			fill(fx, baseY, fx + 7, baseY + 7, (i * 2 < hungerVal) ? 0xFFD27D2D : 0xFF333333);
		}
	}
}

void Gui::renderArmor(int guiW, int guiH) {
	const int numSlots = s_useTouchUI ? 10 : 9;
	int startX = guiW / 2 - 10 * numSlots;
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
