#include "InventoryScreens.h"
#include "TouchControls.h"
#include "NinePatchLayer.h"
#include "GuiContext.h"
#include "Options.h"
#include "Font.h"
#include <glad/glad.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>

namespace {
	bool isTouchUI(GuiContext* ctx) {
		return ctx && ctx->options && ctx->options->useTouchUI;
	}

	Button* makeInvBtn(GuiContext* ctx, int id, const std::string& msg) {
		return isTouchUI(ctx) ? static_cast<Button*>(new Touch::TButton(id, msg)) : new Button(id, msg);
	}

	void drawSlotGrid(const Screen* s, int x, int y, int cols, int rows, int slotSize = 18) {
		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				int sx = x + c * slotSize;
				int sy = y + r * slotSize;
				s->fill(sx, sy, sx + slotSize - 1, sy + slotSize - 1, 0xFF8B8B8B);
				s->drawRect(sx, sy, sx + slotSize - 1, sy + slotSize - 1, 0xFF373737);
			}
		}
	}

	void drawPanel(const Screen* s, int x, int y, int w, int h) {
		s->fill(x, y, x + w, y + h, 0xFFC6C6C6);
		s->drawRect(x, y, x + w, y + h, 0xFF555555);
	}

	void drawPcBevelPanel(const Screen* s, int x, int y, int w, int h) {
		s->fill(x, y, x + w, y + h, 0xFF000000);
		s->fill(x + 1, y + 1, x + w - 1, y + h - 1, 0xFF555555);
		s->fill(x + 1, y + 1, x + w - 3, y + h - 3, 0xFFFFFFFF);
		s->fill(x + 3, y + 3, x + w - 3, y + h - 3, 0xFFC6C6C6);
	}

	void drawPcSlotBox(const Screen* s, int sx, int sy) {
		s->fill(sx, sy, sx + 18, sy + 18, 0xFFFFFFFF);
		s->fill(sx, sy, sx + 17, sy + 17, 0xFF373737);
		s->fill(sx + 1, sy + 1, sx + 17, sy + 17, 0xFF8B8B8B);
	}

	void drawPcLargeSlotBox(const Screen* s, int rx, int ry) {
		s->fill(rx, ry, rx + 26, ry + 26, 0xFFFFFFFF);
		s->fill(rx, ry, rx + 25, ry + 25, 0xFF373737);
		s->fill(rx + 1, ry + 1, rx + 25, ry + 25, 0xFF8B8B8B);
	}

	void drawPcItemStack(const GuiComponent* s, Font* font, const ItemInstance& item, int ix, int iy) {
		if (s) s->drawItemInSlot(font, item, ix, iy, true);
	}

	void drawPcTooltip(const Screen* s, Font* font, const std::string& text, int mouseX, int mouseY, int screenW, int screenH) {
		if (!font || text.empty()) return;
		int tw = font->width(text);
		int tx = mouseX + 10;
		int ty = mouseY - 12;
		if (tx + tw + 8 > screenW) tx = mouseX - tw - 12;
		if (ty < 4) ty = mouseY + 12;
		if (ty + 14 > screenH) ty = screenH - 16;

		s->fill(tx - 3, ty - 3, tx + tw + 3, ty + 11, 0xF0100010);
		s->drawRect(tx - 2, ty - 2, tx + tw + 2, ty + 10, 0xFF5000FF);
		font->drawShadow(text, tx, ty, 0xFFFFFFFF);
	}

	bool pointInSlot(int x, int y, int sx, int sy) {
		return x >= sx && x < sx + 18 && y >= sy && y < sy + 18;
	}

	void renderPcSlotWithItem(const Screen* s, Font* font, int sx, int sy, const ItemInstance& item, int mouseX, int mouseY, const char* emptyIcon, std::string& outHoverText) {
		drawPcSlotBox(s, sx, sy);
		if (item.isNull()) {
			if (emptyIcon && s->bindTexture(emptyIcon)) {
				s->blitScaled(sx + 1, sy + 1, 16, 16, 0, 0, 16, 16, 16, 16);
			}
		} else {
			drawPcItemStack(s, font, item, sx + 1, sy + 1);
		}
		if (pointInSlot(mouseX, mouseY, sx, sy)) {
			s->fill(sx + 1, sy + 1, sx + 17, sy + 17, 0x80FFFFFF);
			if (!item.isNull()) {
				outHoverText = getItemDisplayName(item);
			}
		}
	}

	void renderPcPlayerInventorySlots(const Screen* s, Font* font, GuiContext* ctx, int px, int py, int mouseX, int mouseY, std::string& outHoverText) {
		if (!ctx) return;
		// Main inventory 9x3 (slots 9..35)
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 9; ++c) {
				int idx = 9 + r * 9 + c;
				renderPcSlotWithItem(s, font, px + 8 + c * 18, py + 84 + r * 18, ctx->inventorySlots[static_cast<size_t>(idx)], mouseX, mouseY, nullptr, outHoverText);
			}
		}
		// Hotbar 9x1 (slots 0..8)
		for (int c = 0; c < 9; ++c) {
			renderPcSlotWithItem(s, font, px + 8 + c * 18, py + 142, ctx->inventorySlots[static_cast<size_t>(c)], mouseX, mouseY, nullptr, outHoverText);
		}
	}

	bool handlePcNormalSlotClick(ItemInstance& slot, ItemInstance& carried, int button) {
		if (button != 0 && button != 1) return false;
		if (slot.isNull() && carried.isNull()) return false;

		if (button == 0) {
			if (slot.isNull()) {
				slot = carried;
				carried.setNull();
			} else if (carried.isNull()) {
				carried = slot;
				slot.setNull();
			} else if (slot.sameItemAndAux(&carried) && slot.hasSameUserData(carried)) {
				int maxStack = std::max(1, static_cast<int>(slot.getMaxStackSize()));
				int space = maxStack - static_cast<int>(slot.getStackSize());
				if (space > 0) {
					int move = std::min(space, static_cast<int>(carried.getStackSize()));
					slot.add(move);
					carried.remove(move);
					if (carried.getStackSize() <= 0) carried.setNull();
				}
			} else {
				std::swap(slot, carried);
			}
			return true;
		}

		// Right click (button == 1)
		if (slot.isNull()) {
			if (!carried.isNull()) {
				slot = carried;
				slot.set(1);
				carried.remove(1);
				if (carried.getStackSize() <= 0) carried.setNull();
			}
		} else if (carried.isNull()) {
			int total = static_cast<int>(slot.getStackSize());
			int take = (total + 1) / 2;
			carried = slot;
			carried.set(take);
			slot.remove(take);
			if (slot.getStackSize() <= 0) slot.setNull();
		} else if (slot.sameItemAndAux(&carried) && slot.hasSameUserData(carried)) {
			int maxStack = std::max(1, static_cast<int>(slot.getMaxStackSize()));
			if (slot.getStackSize() < maxStack) {
				slot.add(1);
				carried.remove(1);
				if (carried.getStackSize() <= 0) carried.setNull();
			}
		} else {
			std::swap(slot, carried);
		}
		return true;
	}

	bool handlePcPlayerInventoryClick(GuiContext* ctx, ItemInstance& carried, int px, int py, int x, int y, int button) {
		if (!ctx) return false;
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 9; ++c) {
				if (pointInSlot(x, y, px + 8 + c * 18, py + 84 + r * 18)) {
					int idx = 9 + r * 9 + c;
					handlePcNormalSlotClick(ctx->inventorySlots[static_cast<size_t>(idx)], carried, button);
					return true;
				}
			}
		}
		for (int c = 0; c < 9; ++c) {
			if (pointInSlot(x, y, px + 8 + c * 18, py + 142)) {
				handlePcNormalSlotClick(ctx->inventorySlots[static_cast<size_t>(c)], carried, button);
				return true;
			}
		}
		return false;
	}

	bool handlePcOutsideDrop(GuiContext* ctx, ItemInstance& carried, int px, int py, int pw, int ph, int x, int y, int button) {
		if (!ctx || carried.isNull()) return false;
		if (x >= px && x < px + pw && y >= py && y < py + ph) return false;
		if (button == 0) {
			ctx->pendingWorldDrops.push_back(carried);
			carried.setNull();
			return true;
		}
		if (button == 1) {
			ItemInstance one(carried);
			one.set(1);
			ctx->pendingWorldDrops.push_back(one);
			carried.remove(1);
			if (carried.getStackSize() <= 0) carried.setNull();
			return true;
		}
		return false;
	}

	bool handlePcResultSlotClick(ItemInstance& result, ItemInstance& carried, CraftingContainer& grid) {
		if (result.isNull()) return false;
		int maxStack = std::max(1, static_cast<int>(result.getMaxStackSize()));
		if (carried.isNull()) {
			carried = result;
		} else if (carried.sameItemAndAux(&result) && carried.hasSameUserData(result) &&
		           static_cast<int>(carried.getStackSize()) + static_cast<int>(result.getStackSize()) <= maxStack) {
			carried.add(static_cast<int>(result.getStackSize()));
		} else {
			return false;
		}

		for (int i = 0; i < grid.getContainerSize(); ++i) {
			ItemInstance& cell = grid.at(i);
			if (!cell.isNull()) {
				cell.remove(1);
				if (cell.getStackSize() <= 0) cell.setNull();
			}
		}
		return true;
	}

	void returnCraftingItemsToContext(GuiContext* ctx, ItemInstance& carried, CraftingContainer& grid) {
		if (!ctx) return;
		if (!carried.isNull()) {
			ctx->addItemToInventory(carried);
			carried.setNull();
		}
		for (int i = 0; i < grid.getContainerSize(); ++i) {
			ItemInstance& cell = grid.at(i);
			if (!cell.isNull()) {
				ctx->addItemToInventory(cell);
				cell.setNull();
			}
		}
	}

	void drawWordWrap(Font* font, const std::string& text, int x, int y, int maxWidth, uint32_t color) {
		if (!font || text.empty() || maxWidth <= 0) return;
		std::istringstream iss(text);
		std::string word;
		std::string line;
		int curY = y;
		while (iss >> word) {
			std::string candidate = line.empty() ? word : (line + " " + word);
			if (!line.empty() && font->width(candidate) > maxWidth) {
				font->drawShadow(line, x, curY, color);
				curY += font->height() + 2;
				line = word;
			} else {
				line = candidate;
			}
		}
		if (!line.empty()) {
			font->drawShadow(line, x, curY, color);
		}
	}

	void drawTexturedBox3D(float x0, float y0, float z0, float w, float h, float d, int u, int v, float texW = 64.0f, float texH = 64.0f) {
		float x1 = x0 + w;
		float y1 = y0 + h;
		float z1 = z0 + d;

		auto quad = [&](float ax, float ay, float az,
		                float bx, float by, float bz,
		                float cx, float cy, float cz,
		                float dx, float dy, float dz,
		                int su, int sv, int sw, int sh) {
			float u0 = static_cast<float>(su) / texW;
			float v0 = static_cast<float>(sv) / texH;
			float u1 = static_cast<float>(su + sw) / texW;
			float v1 = static_cast<float>(sv + sh) / texH;
			glTexCoord2f(u0, v1); glVertex3f(ax, ay, az);
			glTexCoord2f(u1, v1); glVertex3f(bx, by, bz);
			glTexCoord2f(u1, v0); glVertex3f(cx, cy, cz);
			glTexCoord2f(u0, v0); glVertex3f(dx, dy, dz);
		};

		int iw = static_cast<int>(w);
		int ih = static_cast<int>(h);
		int id = static_cast<int>(d);

		glBegin(GL_QUADS);
		// Front (+Z)
		quad(x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1, u + id, v + id, iw, ih);
		// Back (-Z)
		quad(x1, y0, z0, x0, y0, z0, x0, y1, z0, x1, y1, z0, u + id + iw + id, v + id, iw, ih);
		// Right (-X)
		quad(x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0, u, v + id, id, ih);
		// Left (+X)
		quad(x1, y0, z1, x1, y0, z0, x1, y1, z0, x1, y1, z1, u + id + iw, v + id, id, ih);
		// Top (+Y)
		quad(x0, y1, z1, x1, y1, z1, x1, y1, z0, x0, y1, z0, u + id, v, iw, id);
		// Bottom (-Y)
		quad(x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, u + id + iw, v, iw, id);
		glEnd();
	}

	void renderSteve3D(const Screen* s, int boxX, int boxY, int boxW, int boxH, int mouseX, int mouseY, bool followMouse) {
		if (!s->bindTexture("textures/entity/steve.png")) return;

		float centerX = static_cast<float>(boxX) + static_cast<float>(boxW) * 0.5f;
		float bottomY = static_cast<float>(boxY + boxH) - static_cast<float>(boxH) * 0.08f;
		float scale = std::min(static_cast<float>(boxW) / 24.0f, static_cast<float>(boxH) / 38.0f);

		float bodyYaw = 0.0f;
		float bodyPitch = 0.0f;
		float headYaw = 0.0f;
		float headPitch = 0.0f;
		float limbSwing = 0.0f;

		if (followMouse) {
			float dx = centerX - static_cast<float>(mouseX);
			float dy = (static_cast<float>(boxY) + static_cast<float>(boxH) * 0.25f) - static_cast<float>(mouseY);
			float lookYaw = std::atan(dx / 40.0f);
			float lookPitch = std::atan(dy / 40.0f);
			bodyYaw = lookYaw * 20.0f;
			headYaw = lookYaw * 40.0f - bodyYaw;
			headPitch = lookPitch * 20.0f;
			bodyPitch = -10.0f + lookPitch * 5.0f;
		} else {
			using clock = std::chrono::steady_clock;
			static const auto startTime = clock::now();
			float t = std::chrono::duration<float>(clock::now() - startTime).count();
			float v17 = std::sin(t) * 10.0f;
			float v18 = std::cos(t * 0.05f) * 10.0f;
			bodyYaw = std::atan(v17 / 40.0f) * 20.0f;
			headYaw = bodyYaw;
			headPitch = std::atan(v18 / 40.0f) * -20.0f;
			bodyPitch = headPitch;
			limbSwing = std::cos(t * 0.25f * 20.0f) * 0.25f * 57.29578f;
		}

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glClear(GL_DEPTH_BUFFER_BIT);
		glDisable(GL_CULL_FACE);
		glEnable(GL_TEXTURE_2D);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		glPushMatrix();
		glTranslatef(centerX, bottomY, 50.0f);
		glScalef(scale, -scale, scale);
		glRotatef(bodyPitch, 1.0f, 0.0f, 0.0f);
		glRotatef(-bodyYaw, 0.0f, 1.0f, 0.0f);

		// Right Leg (pivot at hip y = 12)
		glPushMatrix();
		glTranslatef(-2.0f, 12.0f, 0.0f);
		glRotatef(-limbSwing, 1.0f, 0.0f, 0.0f);
		drawTexturedBox3D(-2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 0, 16);
		glPopMatrix();

		// Left Leg (pivot at hip y = 12)
		glPushMatrix();
		glTranslatef(2.0f, 12.0f, 0.0f);
		glRotatef(limbSwing, 1.0f, 0.0f, 0.0f);
		drawTexturedBox3D(-2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 16, 48);
		glPopMatrix();

		// Torso (x: -4..4, y: 12..24, z: -2..2)
		drawTexturedBox3D(-4.0f, 12.0f, -2.0f, 8.0f, 12.0f, 4.0f, 16, 16);

		// Right Arm (pivot at shoulder y = 22)
		glPushMatrix();
		glTranslatef(-6.0f, 22.0f, 0.0f);
		glRotatef(limbSwing, 1.0f, 0.0f, 0.0f);
		glRotatef(4.0f, 0.0f, 0.0f, -1.0f);
		drawTexturedBox3D(-2.0f, -10.0f, -2.0f, 4.0f, 12.0f, 4.0f, 40, 16);
		glPopMatrix();

		// Left Arm (pivot at shoulder y = 22)
		glPushMatrix();
		glTranslatef(6.0f, 22.0f, 0.0f);
		glRotatef(-limbSwing, 1.0f, 0.0f, 0.0f);
		glRotatef(4.0f, 0.0f, 0.0f, 1.0f);
		drawTexturedBox3D(-2.0f, -10.0f, -2.0f, 4.0f, 12.0f, 4.0f, 32, 48);
		glPopMatrix();

		// Head (x: -4..4, y: 24..32, z: -4..4)
		glPushMatrix();
		glTranslatef(0.0f, 24.0f, 0.0f);
		glRotatef(-headYaw, 0.0f, 1.0f, 0.0f);
		glRotatef(-headPitch, 1.0f, 0.0f, 0.0f);
		drawTexturedBox3D(-4.0f, 0.0f, -4.0f, 8.0f, 8.0f, 8.0f, 0, 0);
		glPopMatrix();

		glPopMatrix();

		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	}

	void drawHotbarOverlay(const Screen* s, Font* font, GuiContext* ctx, int width, int height) {
		int barW = 182;
		int barH = 22;
		int x = width / 2 - barW / 2;
		int y = height - barH;
		int selectedSlot = ctx ? ctx->selectedHotbarSlot : 0;
		if (s->bindTexture("textures/gui/gui.png")) {
			s->blit(x, y, 0, 0, barW, barH, 256, 256);
			s->blit(x - 1 + selectedSlot * 20, y - 1, 0, 22, 24, 24, 256, 256);
		} else {
			s->fill(x, y, x + barW, y + barH, 0xAA222222);
			s->drawRect(x + selectedSlot * 20, y, x + (selectedSlot + 1) * 20, y + barH, 0xFFFFFFFF);
		}
		if (ctx) {
			for (int i = 0; i < 9; ++i) {
				const ItemInstance& item = ctx->inventorySlots[static_cast<size_t>(i)];
				if (!item.isNull()) {
					drawPcItemStack(s, font, item, x + 3 + i * 20, y + 3);
				}
			}
		}
	}

	class CategoryTabButton : public Button {
	public:
		CategoryTabButton(int id, int iconIndex, const int* selectedCategory, int catIndex)
			: Button(id, "")
			, mIconIndex(iconIndex)
			, mSelectedCategory(selectedCategory)
			, mCatIndex(catIndex)
			, mSelBg("textures/gui/spritesheet.png", IntRectangle{ 0, 32, 8, 8 }, 2, 2, 2, 2, 256, 256)
			, mUnselBg("textures/gui/spritesheet.png", IntRectangle{ 8, 32, 8, 8 }, 2, 2, 2, 2, 256, 256) {
		}

		void render(GuiContext* ctx, int mouseX, int mouseY) override {
			if (!visible) return;
			bool selected = mSelectedCategory && (*mSelectedCategory == mCatIndex);
			(selected ? mSelBg : mUnselBg).draw(posX, posY, width, height);
			if (bindTexture("textures/gui/spritesheet.png")) {
				int u = 32 * (mIconIndex / 2);
				int v = 32 * ((mIconIndex & 1) + 2);
				blitScaled(posX, posY, width, height, u, v, 32, 32, 256, 256);
			}
		}

	private:
		int mIconIndex;
		const int* mSelectedCategory;
		int mCatIndex;
		NinePatchLayer mSelBg;
		NinePatchLayer mUnselBg;
	};

	class CraftButton : public Button {
	public:
		CraftButton(int id)
			: Button(id, "")
			, mNormalBg("textures/gui/spritesheet.png", IntRectangle{ 112, 0, 8, 67 }, 2, 2, 2, 2, 256, 256)
			, mPressedBg("textures/gui/spritesheet.png", IntRectangle{ 120, 0, 8, 67 }, 2, 2, 2, 2, 256, 256) {
		}

		void setRecipe(const TouchRecipeEntry* recipe) {
			mRecipe = recipe;
		}

		void render(GuiContext* ctx, int mouseX, int mouseY) override {
			if (!visible) return;
			hovered = active && pointInside(mouseX, mouseY);
			bool isPressed = active && pressed && hovered;
			(isPressed ? mPressedBg : mNormalBg).draw(posX, posY, width, height);

			if (!mRecipe) return;
			float v9 = static_cast<float>(width) * 0.5f;
			float v10 = static_cast<float>(height) * 0.5f;
			float v11 = static_cast<float>(posX) + v9 * 0.5f - 8.0f;
			float v12 = static_cast<float>(posY) + v10 * 0.5f - 9.0f;

			size_t count = std::min<size_t>(4, mRecipe->reqs.size());
			for (size_t i = 0; i < count; ++i) {
				int ix = static_cast<int>(v11 + v9 * static_cast<float>(i & 1));
				int iy = static_cast<int>(v12 + v10 * static_cast<float>(i >> 1));
				drawItemInSlot(nullptr, mRecipe->reqs[i].item, ix, iy, false);
			}

			if (ctx && ctx->font) {
				glPushMatrix();
				glScalef(0.66667f, 0.66667f, 1.0f);
				for (size_t i = 0; i < count; ++i) {
					const TouchRecipeReq& req = mRecipe->reqs[i];
					float ix = v11 + v9 * static_cast<float>(i & 1);
					float iy = v12 + v10 * static_cast<float>(i >> 1);
					int tx = static_cast<int>(ix * 1.5f + 4.0f);
					int ty = static_cast<int>(iy * 1.5f + 23.0f);
					std::string label = std::to_string(req.have) + "/" + std::to_string(req.need);
					if (req.have < req.need) {
						ctx->font->drawShadow(label, tx, ty, 0xFFFF5555);
					} else {
						ctx->font->drawShadow(label, tx, ty, 0xFFF0F0F0);
					}
				}
				glPopMatrix();
			}
		}

	private:
		const TouchRecipeEntry* mRecipe = nullptr;
		NinePatchLayer mNormalBg;
		NinePatchLayer mPressedBg;
	};

	class ArmorSlotButton : public Button {
	public:
		ArmorSlotButton(int id, int slotIndex)
			: Button(id, "")
			, mSlotIndex(slotIndex)
			, mBg("textures/gui/spritesheet.png", IntRectangle{ 0, 32, 8, 8 }, 3, 3, 3, 3, 256, 256) {
		}

		void render(GuiContext* ctx, int mouseX, int mouseY) override {
			if (!visible) return;
			mBg.draw(posX, posY, width, height);
			if (ctx && mSlotIndex >= 0 && mSlotIndex < 4 && !ctx->armorSlots[static_cast<size_t>(mSlotIndex)].isNull()) {
				drawPcItemStack(this, ctx->font, ctx->armorSlots[static_cast<size_t>(mSlotIndex)], posX + 2, posY + 2);
				return;
			}
			static const char* emptyIcons[4] = {
				"textures/items/empty_armor_slot_helmet.png",
				"textures/items/empty_armor_slot_chestplate.png",
				"textures/items/empty_armor_slot_leggings.png",
				"textures/items/empty_armor_slot_boots.png"
			};
			if (mSlotIndex >= 0 && mSlotIndex < 4 && bindTexture(emptyIcons[mSlotIndex])) {
				blitScaled(posX + 2, posY + 2, 16, 16, 0, 0, 16, 16, 16, 16);
			}
		}

	private:
		int mSlotIndex;
		NinePatchLayer mBg;
	};

	void drawPcClassicCrafting(const Screen* s, Font* font, int px, int py, int gridSize, const char* title) {
		if (font && title) s->drawString(font, title, px + 8, py + 6, 0xFF404040);
		if (gridSize == 3) {
			drawSlotGrid(s, px + 38, py + 18, 3, 3, 18);
		}
	}
}

// --- InventoryScreen ---
bool InventoryScreen::renderGameBehind() const {
	return !isTouchUI(context);
}

void InventoryScreen::updateCraftingResult() {
	Recipes::getInstance()->init();
	mResultSlot = Recipes::getInstance()->findMatchingRecipe(mCraftGrid);
}

void InventoryScreen::returnItemsAndClose() {
	returnCraftingItemsToContext(context, mCarriedItem, mCraftGrid);
	mResultSlot.setNull();
	if (context) context->startGame();
}

void InventoryScreen::init() {
	mCloseButton = nullptr;
	mHeader = mBackButton = mCraftButton = mArmorButton = nullptr;
	if (context) context->ensureInventoryInitialized();
	mCraftGrid.clear();
	mResultSlot.setNull();
	mCarriedItem.setNull();

	if (isTouchUI(context)) {
		mHeader = new Touch::THeader(0, "Select blocks");
		mBackButton = new Touch::TButton(1, "Back");
		mCraftButton = new Touch::TButton(2, "Craft");
		mArmorButton = new Touch::TButton(3, "Armor");
		buttons.push_back(mHeader);
		buttons.push_back(mBackButton);
		buttons.push_back(mCraftButton);
		buttons.push_back(mArmorButton);
	} else {
		mCloseButton = makeInvBtn(context, 1, "X");
		buttons.push_back(mCloseButton);
	}
}

void InventoryScreen::setupPositions() {
	if (isTouchUI(context)) {
		if (mHeader) { mHeader->posX = 0; mHeader->posY = 0; mHeader->width = width; mHeader->height = 30; }
		if (mBackButton) { mBackButton->posX = 4; mBackButton->posY = 5; mBackButton->width = 38; mBackButton->height = 20; }
		if (mArmorButton) { mArmorButton->posX = width - 42; mArmorButton->posY = 5; mArmorButton->width = 38; mArmorButton->height = 20; }
		if (mCraftButton) { mCraftButton->posX = width - 84; mCraftButton->posY = 5; mCraftButton->width = 38; mCraftButton->height = 20; }
		mPaneCols = std::max(1, (width - 20) / 32);
		mPaneW = 32 * mPaneCols;
		mPaneX = (width - mPaneW) / 2;
		mPaneY = 30;
		mPaneH = std::max(32, height - 56);
		return;
	}

	int pw = 176, ph = 166;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	if (mCloseButton) { mCloseButton->posX = px + pw - 20; mCloseButton->posY = py - 22; mCloseButton->width = 20; mCloseButton->height = 20; }
}

void InventoryScreen::render(int mouseX, int mouseY, float partialTicks) {
	if (isTouchUI(context)) {
		static NinePatchLayer bgLayer("textures/gui/spritesheet.png", IntRectangle{ 0, 0, 16, 16 }, 4, 4, 4, 4, 256, 256);
		static NinePatchLayer paneFrame("textures/gui/spritesheet.png", IntRectangle{ 28, 42, 4, 4 }, 1, 1, 1, 1, 256, 256);
		paneFrame.setExcluded(16);

		bgLayer.draw(0, 0, width, height);
		Screen::render(mouseX, mouseY, partialTicks);

		const int totalSlots = 27;
		int totalRows = (totalSlots + mPaneCols - 1) / mPaneCols;
		int maxScroll = std::max(0, totalRows * 32 - mPaneH);
		if (mDragging) {
			int dy = mLastDragY - mouseY;
			if (std::abs(mouseY - mPressStartY) > 4) mDidDrag = true;
			mLastDragY = mouseY;
			mScrollY = std::max(0, std::min(maxScroll, mScrollY + dy));
		}

		fill(mPaneX, mPaneY, mPaneX + mPaneW, mPaneY + mPaneH, 0xFF333333);

		int scale = context ? context->guiScale : 2;
		int winH = context ? context->windowHeight : (height * scale);
		glEnable(GL_SCISSOR_TEST);
		glScissor(mPaneX * scale, winH - (mPaneY + mPaneH) * scale, mPaneW * scale, mPaneH * scale);

		bool hasSlotTex = bindTexture("textures/gui/gui.png");
		for (int i = 0; i < totalSlots; ++i) {
			int invIdx = 9 + i;
			int col = i % mPaneCols;
			int row = i / mPaneCols;
			int sx = mPaneX + col * 32;
			int sy = mPaneY + row * 32 - mScrollY;
			bool sel = (invIdx == mSelectedItem);
			if (hasSlotTex) {
				blitScaled(sx, sy, 32, 32, 200, 46, 16, 16, 256, 256);
			} else {
				fill(sx + 2, sy + 2, sx + 30, sy + 30, 0xFF2B2B2B);
				drawRect(sx + 2, sy + 2, sx + 30, sy + 30, 0xFF555555);
			}
			if (sel) {
				drawRect(sx + 1, sy + 1, sx + 31, sy + 31, 0xFFFFFFFF);
			}
			if (context && !context->inventorySlots[static_cast<size_t>(invIdx)].isNull()) {
				drawPcItemStack(this, font, context->inventorySlots[static_cast<size_t>(invIdx)], sx + 8, sy + 8);
				hasSlotTex = bindTexture("textures/gui/gui.png");
			}
		}
		glDisable(GL_SCISSOR_TEST);

		paneFrame.draw(mPaneX - 1, mPaneY - 1, mPaneW + 2, mPaneH + 2);

		drawHotbarOverlay(this, font, context, width, height);
		return;
	}

	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	int pw = 176, ph = 166;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	drawPcBevelPanel(this, px, py, pw, ph);

	std::string hoverText;

	// Armor slots (1x4) at (px + 8, py + 8 + i * 18)
	static const char* armorEmptyTex[4] = {
		"textures/gui/newgui/empty_armor_slot_helmet.png",
		"textures/gui/newgui/empty_armor_slot_chestplate.png",
		"textures/gui/newgui/empty_armor_slot_leggings.png",
		"textures/gui/newgui/empty_armor_slot_boots.png"
	};
	for (int i = 0; i < 4; ++i) {
		ItemInstance armorItem = context ? context->armorSlots[static_cast<size_t>(i)] : ItemInstance();
		renderPcSlotWithItem(this, font, px + 8, py + 8 + i * 18, armorItem, mouseX, mouseY, armorEmptyTex[i], hoverText);
	}

	// Player 3D box (52x70) from (px + 26, py + 8) to (px + 78, py + 78)
	fill(px + 26, py + 8, px + 78, py + 78, 0xFF000000);
	drawRect(px + 26, py + 8, px + 78, py + 78, 0xFF373737);
	renderSteve3D(this, px + 26, py + 8, 52, 70, mouseX, mouseY, true);

	// Crafting 2x2
	if (font) drawString(font, "Crafting", px + 97, py + 8, 0xFF404040);
	for (int r = 0; r < 2; ++r) {
		for (int c = 0; c < 2; ++c) {
			renderPcSlotWithItem(this, font, px + 98 + c * 18, py + 18 + r * 18, mCraftGrid.at(r * 2 + c), mouseX, mouseY, nullptr, hoverText);
		}
	}

	// Arrow
	if (bindTexture("textures/gui/newgui/arrow_large.png")) {
		blitScaled(px + 134, py + 28, 18, 15, 0, 0, 22, 15, 22, 15);
	} else if (font) {
		drawString(font, "->", px + 136, py + 32, 0xFF404040);
	}

	// Result slot at (px + 154, py + 28)
	renderPcSlotWithItem(this, font, px + 154, py + 28, mResultSlot, mouseX, mouseY, nullptr, hoverText);

	// Player inventory 9x3 + hotbar 9x1
	renderPcPlayerInventorySlots(this, font, context, px, py, mouseX, mouseY, hoverText);

	Screen::render(mouseX, mouseY, partialTicks);

	if (mCarriedItem.isNull() && !hoverText.empty()) {
		drawPcTooltip(this, font, hoverText, mouseX, mouseY, width, height);
	}
	if (!mCarriedItem.isNull()) {
		drawPcItemStack(this, font, mCarriedItem, mouseX - 8, mouseY - 8);
	}
}

void InventoryScreen::mouseClicked(int x, int y, int button) {
	if (isTouchUI(context)) {
		if (button == 0) {
			if (x >= mPaneX && x < mPaneX + mPaneW && y >= mPaneY && y < mPaneY + mPaneH) {
				mDragging = true;
				mDidDrag = false;
				mLastDragY = y;
				mPressStartY = y;
				return;
			}

			int barX = width / 2 - 91;
			int barY = height - 22;
			if (x >= barX && x < barX + 180 && y >= barY && y < height && context) {
				context->selectedHotbarSlot = std::max(0, std::min(8, (x - barX) / 20));
				return;
			}
		}
		Screen::mouseClicked(x, y, button);
		return;
	}

	if ((button == 0 || button == 1) && context) {
		int pw = 176, ph = 166;
		int px = (width - pw) / 2, py = (height - ph) / 2;

		for (int i = 0; i < 4; ++i) {
			if (pointInSlot(x, y, px + 8, py + 8 + i * 18)) {
				handlePcNormalSlotClick(context->armorSlots[static_cast<size_t>(i)], mCarriedItem, button);
				return;
			}
		}

		for (int r = 0; r < 2; ++r) {
			for (int c = 0; c < 2; ++c) {
				if (pointInSlot(x, y, px + 98 + c * 18, py + 18 + r * 18)) {
					if (handlePcNormalSlotClick(mCraftGrid.at(r * 2 + c), mCarriedItem, button)) {
						updateCraftingResult();
					}
					return;
				}
			}
		}

		if (pointInSlot(x, y, px + 154, py + 28)) {
			if (handlePcResultSlotClick(mResultSlot, mCarriedItem, mCraftGrid)) {
				updateCraftingResult();
			}
			return;
		}

		if (handlePcPlayerInventoryClick(context, mCarriedItem, px, py, x, y, button)) {
			return;
		}
		if (handlePcOutsideDrop(context, mCarriedItem, px, py, pw, ph, x, y, button)) {
			return;
		}
	}

	Screen::mouseClicked(x, y, button);
}

void InventoryScreen::mouseReleased(int x, int y, int button) {
	if (mDragging && !mDidDrag && isTouchUI(context) && context && button == 0) {
		if (x >= mPaneX && x < mPaneX + mPaneW && y >= mPaneY && y < mPaneY + mPaneH) {
			int col = (x - mPaneX) / 32;
			int row = (y - mPaneY + mScrollY) / 32;
			int idx = row * mPaneCols + col;
			if (idx >= 0 && idx < 27) {
				int invIdx = 9 + idx;
				mSelectedItem = invIdx;
				int hbSlot = std::max(0, std::min(8, context->selectedHotbarSlot));
				std::swap(context->inventorySlots[static_cast<size_t>(hbSlot)], context->inventorySlots[static_cast<size_t>(invIdx)]);
			}
		}
	}
	mDragging = false;
	mDidDrag = false;
	Screen::mouseReleased(x, y, button);
}

void InventoryScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (isTouchUI(context)) {
		if (button->id == 1) context->startGame();
		else if (button->id == 2) context->setScreen(new WorkbenchScreen(true));
		else if (button->id == 3) context->setScreen(new ArmorScreen());
		return;
	}

	if (button->id == 1) {
		returnItemsAndClose();
	}
}

void InventoryScreen::keyPressed(int key) {
	if (key == 69 /* GLFW_KEY_E */ || key == 256 /* GLFW_KEY_ESCAPE */) {
		if (isTouchUI(context)) {
			if (context) context->startGame();
		} else {
			returnItemsAndClose();
		}
		return;
	}
	Screen::keyPressed(key);
}

// --- ArmorScreen ---
bool ArmorScreen::renderGameBehind() const {
	return !isTouchUI(context);
}

void ArmorScreen::rebuildArmorList() {
	mArmorInvIndices.clear();
	if (!context) return;
	for (int i = 0; i < 36; ++i) {
		const ItemInstance& item = context->inventorySlots[static_cast<size_t>(i)];
		if (!item.isNull() && getArmorSlotForItem(item) >= 0) {
			mArmorInvIndices.push_back(i);
		}
	}
}

void ArmorScreen::init() {
	if (context) context->ensureInventoryInitialized();
	mHeader = new Touch::THeader(0, "Armor");
	mBackButton = new Touch::TButton(1, "Back");
	buttons.push_back(mHeader);
	buttons.push_back(mBackButton);
	for (int i = 0; i < 4; ++i) {
		mArmorSlots[i] = new ArmorSlotButton(10 + i, i);
		buttons.push_back(mArmorSlots[i]);
	}
	rebuildArmorList();
}

void ArmorScreen::setupPositions() {
	if (mHeader) { mHeader->posX = 0; mHeader->posY = 0; mHeader->width = width; mHeader->height = 30; }
	if (mBackButton) { mBackButton->posX = 4; mBackButton->posY = 5; mBackButton->width = 38; mBackButton->height = 20; }

	int wBase = static_cast<int>(static_cast<float>(width) / 1.8f) - 20;
	mPaneX = 10;
	mPaneY = 32;
	mPaneW = std::max(32, wBase & ~31);
	mPaneH = std::max(32, height - 40);

	int slotX = mPaneW + 20;
	for (int i = 0; i < 4; ++i) {
		if (mArmorSlots[i]) {
			mArmorSlots[i]->posX = slotX;
			mArmorSlots[i]->posY = 32 + 24 * i;
			mArmorSlots[i]->width = 20;
			mArmorSlots[i]->height = 20;
		}
	}

	int rightStart = slotX + 20;
	int remW = std::max(20, width - rightStart);
	mBoxX = rightStart + remW / 10;
	mBoxY = 32;
	mBoxW = std::max(20, remW - 2 * (remW / 10));
	mBoxH = mPaneH;
}

void ArmorScreen::render(int mouseX, int mouseY, float partialTicks) {
	static NinePatchLayer bgLayer("textures/gui/spritesheet.png", IntRectangle{ 0, 0, 16, 16 }, 4, 4, 4, 4, 256, 256);
	static NinePatchLayer paneFrame("textures/gui/spritesheet.png", IntRectangle{ 28, 42, 4, 4 }, 1, 1, 1, 1, 256, 256);
	static NinePatchLayer playerBox("textures/gui/spritesheet.png", IntRectangle{ 0, 20, 8, 8 }, 3, 3, 3, 3, 256, 256);
	paneFrame.setExcluded(16);

	bgLayer.draw(0, 0, width, height);
	Screen::render(mouseX, mouseY, partialTicks);

	// Left armor inventory pane
	const int totalArmorItems = static_cast<int>(mArmorInvIndices.size());
	int cols = std::max(1, mPaneW / 32);
	int rows = (totalArmorItems + cols - 1) / cols;
	int maxScroll = std::max(0, rows * 32 - mPaneH);
	if (mDragging) {
		int dy = mLastDragY - mouseY;
		if (std::abs(mouseY - mPressStartY) > 4) mDidDrag = true;
		mLastDragY = mouseY;
		mScrollY = std::max(0, std::min(maxScroll, mScrollY + dy));
	}

	fill(mPaneX, mPaneY, mPaneX + mPaneW, mPaneY + mPaneH, 0xFF333333);

	int scale = context ? context->guiScale : 2;
	int winH = context ? context->windowHeight : (height * scale);
	glEnable(GL_SCISSOR_TEST);
	glScissor(mPaneX * scale, winH - (mPaneY + mPaneH) * scale, mPaneW * scale, mPaneH * scale);
	int drawSlots = std::max(totalArmorItems, cols * std::max(1, (mPaneH + 31) / 32));
	bool hasSlotTex = bindTexture("textures/gui/gui.png");
	for (int i = 0; i < drawSlots; ++i) {
		int c = i % cols;
		int r = i / cols;
		int sx = mPaneX + c * 32;
		int sy = mPaneY + r * 32 - mScrollY;
		if (hasSlotTex) {
			blitScaled(sx, sy, 32, 32, 200, 46, 16, 16, 256, 256);
		} else {
			fill(sx + 2, sy + 2, sx + 30, sy + 30, 0x80303030);
			drawRect(sx + 2, sy + 2, sx + 30, sy + 30, 0xFF555555);
		}
		if (context && i < totalArmorItems) {
			const ItemInstance& armorItem = context->inventorySlots[static_cast<size_t>(mArmorInvIndices[static_cast<size_t>(i)])];
			drawPcItemStack(this, font, armorItem, sx + 8, sy + 8);
			hasSlotTex = bindTexture("textures/gui/gui.png");
		}
	}
	glDisable(GL_SCISSOR_TEST);
	paneFrame.draw(mPaneX - 1, mPaneY - 1, mPaneW + 2, mPaneH + 2);

	// Right player 3D preview box
	playerBox.draw(mBoxX, mBoxY, mBoxW, mBoxH);
	renderSteve3D(this, mBoxX, mBoxY, mBoxW, mBoxH, mouseX, mouseY, false);
}

void ArmorScreen::mouseClicked(int x, int y, int button) {
	if (button == 0 && x >= mPaneX && x < mPaneX + mPaneW && y >= mPaneY && y < mPaneY + mPaneH) {
		mDragging = true;
		mDidDrag = false;
		mLastDragY = y;
		mPressStartY = y;
		return;
	}
	Screen::mouseClicked(x, y, button);
}

void ArmorScreen::mouseReleased(int x, int y, int button) {
	if (mDragging && !mDidDrag && context && button == 0) {
		if (x >= mPaneX && x < mPaneX + mPaneW && y >= mPaneY && y < mPaneY + mPaneH) {
			int cols = std::max(1, mPaneW / 32);
			int c = (x - mPaneX) / 32;
			int r = (y - mPaneY + mScrollY) / 32;
			int idx = r * cols + c;
			if (idx >= 0 && idx < static_cast<int>(mArmorInvIndices.size())) {
				int invIdx = mArmorInvIndices[static_cast<size_t>(idx)];
				ItemInstance& src = context->inventorySlots[static_cast<size_t>(invIdx)];
				int targetSlot = getArmorSlotForItem(src);
				if (targetSlot >= 0 && targetSlot < 4) {
					std::swap(context->armorSlots[static_cast<size_t>(targetSlot)], src);
					rebuildArmorList();
				}
			}
		}
	}
	mDragging = false;
	mDidDrag = false;
	Screen::mouseReleased(x, y, button);
}

void ArmorScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (button->id == 1) {
		context->setScreen(new InventoryScreen());
	} else if (button->id >= 10 && button->id <= 13) {
		int slotIdx = button->id - 10;
		ItemInstance& equipped = context->armorSlots[static_cast<size_t>(slotIdx)];
		if (!equipped.isNull()) {
			if (context->addItemToInventory(equipped)) {
				equipped.setNull();
				rebuildArmorList();
			}
		}
	}
}

void ArmorScreen::keyPressed(int key) {
	if (key == 256 /* ESCAPE */ || key == 69 /* E */) {
		if (context) context->setScreen(new InventoryScreen());
		return;
	}
	Screen::keyPressed(key);
}

// --- CreativeInventoryScreen ---
void CreativeInventoryScreen::init() {
	mCatButtons.clear();
	static const char* cats[4] = { "Building", "Decor", "Tools", "Misc" };
	for (int i = 0; i < 4; ++i) {
		Button* b = makeInvBtn(context, 10 + i, cats[i]);
		mCatButtons.push_back(b);
		buttons.push_back(b);
	}
	mCloseButton = makeInvBtn(context, 1, "X");
	buttons.push_back(mCloseButton);
}

void CreativeInventoryScreen::setupPositions() {
	int pw = 220, ph = 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	for (size_t i = 0; i < mCatButtons.size(); ++i) {
		mCatButtons[i]->posX = px + static_cast<int>(i) * 48;
		mCatButtons[i]->posY = py - 22;
		mCatButtons[i]->width = 46;
		mCatButtons[i]->height = 20;
		mCatButtons[i]->active = (static_cast<int>(i) != mCategory);
	}
	if (mCloseButton) { mCloseButton->posX = px + pw - 20; mCloseButton->posY = py - 22; mCloseButton->width = 20; mCloseButton->height = 20; }
}

void CreativeInventoryScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	int pw = 220, ph = 150;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	drawPanel(this, px, py, pw, ph);
	if (font) drawString(font, "Creative Inventory", px + 8, py + 6, 0xFF404040);
	drawSlotGrid(this, px + 28, py + 22, 9, 5);
	drawSlotGrid(this, px + 28, py + 120, 9, 1);
	Screen::render(mouseX, mouseY, partialTicks);
}

void CreativeInventoryScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (button->id == 1) context->startGame();
	else if (button->id >= 10 && button->id <= 13) {
		mCategory = button->id - 10;
		setupPositions();
	}
}

void CreativeInventoryScreen::keyPressed(int key) {
	if (key == 69 || key == 256) {
		if (context) context->startGame();
		return;
	}
	Screen::keyPressed(key);
}

// --- PaneCraftingScreen ---
PaneCraftingScreen::PaneCraftingScreen(int numCategories, int singleIconIndex, bool is2x2, const std::string& pcTitle)
	: mNumCategories(numCategories)
	, mSingleIconIndex(singleIconIndex)
	, mIs2x2(is2x2)
	, mPcTitle(pcTitle) {
}

bool PaneCraftingScreen::renderGameBehind() const {
	return !isTouchUI(context);
}

void PaneCraftingScreen::updateCraftingResult() {
	Recipes::getInstance()->init();
	mResultSlot = Recipes::getInstance()->findMatchingRecipe(mCraftGrid);
}

void PaneCraftingScreen::closeScreen() {
	if (!context) return;
	if (!isTouchUI(context)) {
		returnCraftingItemsToContext(context, mCarriedItem, mCraftGrid);
		mResultSlot.setNull();
	}
	if (isTouchUI(context) && mIs2x2) {
		context->setScreen(new InventoryScreen());
	} else {
		context->startGame();
	}
}

void PaneCraftingScreen::recheckRecipes() {
	mTouchRecipes.clear();
	if (!context) return;
	Recipes::getInstance()->init();

	bool isStonecutter = (mNumCategories == 1 && mSingleIconIndex == 5);
	int catMask = 1 << mCategory;
	int maxGridSize = mIs2x2 ? 2 : 3;

	auto countInInventory = [&](int id, int aux) -> int {
		int total = 0;
		for (const ItemInstance& slot : context->inventorySlots) {
			if (!slot.isNull() && slot.getId() == id && (aux == -1 || slot.getAuxValue() == aux)) {
				total += static_cast<int>(slot.getStackSize());
			}
		}
		return total;
	};

	const CraftingRecipe* prevSelected = (mSelectedRecipe >= 0 && mSelectedRecipe < static_cast<int>(mTouchRecipes.size()))
		? mTouchRecipes[static_cast<size_t>(mSelectedRecipe)].recipe
		: nullptr;

	for (const auto& uptr : Recipes::getInstance()->getRecipes()) {
		const CraftingRecipe* rec = uptr.get();
		if (!rec) continue;
		if (isStonecutter) {
			if (!rec->isStonecutter()) continue;
		} else {
			if (rec->isStonecutter()) continue;
			if (rec->getCraftingSize() > maxGridSize) continue;
			if ((static_cast<int>(rec->getCategory()) & catMask) == 0) continue;
		}

		TouchRecipeEntry entry;
		entry.recipe = rec;
		entry.result = rec->getResultItem();
		entry.name = getItemDisplayName(entry.result);
		entry.desc = getItemDescription(entry.result);
		entry.inventoryCount = countInInventory(entry.result.getId(), entry.result.getAuxValue());

		bool allMet = true;
		for (const ItemInstance& ing : rec->getCompactIngredients()) {
			TouchRecipeReq req;
			req.item = ing;
			req.need = static_cast<int>(ing.getStackSize());
			req.have = countInInventory(ing.getId(), ing.getAuxValue());
			if (req.have < req.need) allMet = false;
			entry.reqs.push_back(req);
		}
		entry.canBeCrafted = allMet;
		mTouchRecipes.push_back(std::move(entry));
	}

	sortRecipes();

	if (prevSelected) {
		for (size_t i = 0; i < mTouchRecipes.size(); ++i) {
			if (mTouchRecipes[i].recipe == prevSelected) {
				mSelectedRecipe = static_cast<int>(i);
				break;
			}
		}
	}
	if (mSelectedRecipe < 0 || mSelectedRecipe >= static_cast<int>(mTouchRecipes.size())) {
		mSelectedRecipe = 0;
	}
}

void PaneCraftingScreen::sortRecipes() {
	std::stable_sort(mTouchRecipes.begin(), mTouchRecipes.end(), [](const TouchRecipeEntry& a, const TouchRecipeEntry& b) {
		if (a.canBeCrafted != b.canBeCrafted) return a.canBeCrafted > b.canBeCrafted;
		return a.name < b.name;
	});
}

void PaneCraftingScreen::init() {
	mCatButtons.clear();
	mCraftButton = nullptr;
	mCloseButton = nullptr;
	if (context) context->ensureInventoryInitialized();
	mCraftGrid.clear();
	mResultSlot.setNull();
	mCarriedItem.setNull();

	if (isTouchUI(context)) {
		for (int i = 0; i < mNumCategories; ++i) {
			int iconIdx = (mNumCategories == 1) ? mSingleIconIndex : i;
			Button* tabBtn = new CategoryTabButton(100 + i, iconIdx, &mCategory, i);
			mCatButtons.push_back(tabBtn);
			buttons.push_back(tabBtn);
		}
		mCraftButton = new CraftButton(2);
		mCloseButton = new ImageButton(1, "textures/gui/spritesheet.png", 60, 0, 18, 18, false);
		buttons.push_back(mCraftButton);
		buttons.push_back(mCloseButton);
		recheckRecipes();
	} else {
		mCloseButton = makeInvBtn(context, 1, "X");
		buttons.push_back(mCloseButton);
	}
}

void PaneCraftingScreen::setupPositions() {
	if (isTouchUI(context)) {
		int v2 = std::max(4, mNumCategories);
		int tabSize = std::max(16, (height - 16) / v2);
		for (size_t i = 0; i < mCatButtons.size(); ++i) {
			mCatButtons[i]->posX = 6;
			mCatButtons[i]->posY = 6 + static_cast<int>(i) * (tabSize + 1);
			mCatButtons[i]->width = tabSize;
			mCatButtons[i]->height = tabSize;
		}

		mListX = tabSize + 12;
		mListY = 8;
		mListW = std::max(40, width - mListX - 100);
		mListH = std::max(24, height - 16);

		if (mCraftButton) {
			mCraftButton->posX = width - 95;
			mCraftButton->posY = 20;
			mCraftButton->width = 88;
			mCraftButton->height = 62;
		}
		if (mCloseButton) {
			mCloseButton->posX = width - 19;
			mCloseButton->posY = 0;
			mCloseButton->width = 19;
			mCloseButton->height = 19;
		}
		return;
	}

	int pw = 176, ph = 166;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	if (mCloseButton) { mCloseButton->posX = px + pw - 20; mCloseButton->posY = py - 22; mCloseButton->width = 20; mCloseButton->height = 20; }
}

void PaneCraftingScreen::render(int mouseX, int mouseY, float partialTicks) {
	if (isTouchUI(context)) {
		static NinePatchLayer bgLayer("textures/gui/spritesheet.png", IntRectangle{ 0, 0, 16, 16 }, 4, 4, 4, 4, 256, 256);
		static NinePatchLayer frameLayer("textures/gui/spritesheet.png", IntRectangle{ 0, 20, 8, 8 }, 1, 2, 1, 2, 256, 256);
		static NinePatchLayer rowSel("textures/gui/spritesheet.png", IntRectangle{ 20, 32, 8, 8 }, 2, 2, 2, 2, 256, 256);
		static NinePatchLayer rowUnsel("textures/gui/spritesheet.png", IntRectangle{ 28, 32, 8, 8 }, 2, 2, 2, 2, 256, 256);
		frameLayer.setExcluded(16);

		if (mSelectedRecipe < 0 || mSelectedRecipe >= static_cast<int>(mTouchRecipes.size())) {
			mSelectedRecipe = 0;
		}
		const TouchRecipeEntry* activeRecipe = mTouchRecipes.empty() ? nullptr : &mTouchRecipes[static_cast<size_t>(mSelectedRecipe)];
		if (mCraftButton) {
			static_cast<CraftButton*>(mCraftButton)->setRecipe(activeRecipe);
		}

		bgLayer.draw(0, 0, width, height);
		Screen::render(mouseX, mouseY, partialTicks);

		const int rowH = 22;
		int maxScroll = std::max(0, static_cast<int>(mTouchRecipes.size()) * rowH - mListH);
		if (mDragging) {
			int dy = mLastDragY - mouseY;
			mLastDragY = mouseY;
			mScrollY = std::max(0, std::min(maxScroll, mScrollY + dy));
		}

		int scale = context ? context->guiScale : 2;
		int winH = context ? context->windowHeight : (height * scale);
		glEnable(GL_SCISSOR_TEST);
		glScissor(mListX * scale, winH - (mListY + mListH) * scale, mListW * scale, mListH * scale);
		for (size_t i = 0; i < mTouchRecipes.size(); ++i) {
			const TouchRecipeEntry& rec = mTouchRecipes[i];
			int ry = mListY + static_cast<int>(i) * rowH - mScrollY;
			bool sel = (static_cast<int>(i) == mSelectedRecipe);
			(sel ? rowSel : rowUnsel).draw(mListX, ry, mListW, rowH);

			// Result icon (16x16) on the right side of the row
			drawPcItemStack(this, nullptr, rec.result, mListX + mListW - 18, ry + 3);

			if (font) {
				if (rec.canBeCrafted) {
					font->drawShadow(rec.name, mListX + 5, ry + 6, sel ? 0xFFFFFFA0 : 0xFFF0F0F0);
				} else {
					font->draw(rec.name, mListX + 6, ry + 8, 0xC0AAAAAA);
					font->draw(rec.name, mListX + 5, ry + 7, 0xC0635558);
				}

				if (rec.inventoryCount > 0) {
					std::string cnt = std::to_string(rec.inventoryCount);
					glPushMatrix();
					glScalef(0.66667f, 0.66667f, 1.0f);
					int tx = static_cast<int>(static_cast<float>(mListX + mListW - 4 * static_cast<int>(cnt.size()) - 2) * 1.5f);
					int ty = static_cast<int>(static_cast<float>(ry + rowH - 8) * 1.5f);
					font->drawShadow(cnt, tx, ty, rec.canBeCrafted ? 0xFFF0F0F0 : 0xC0635558);
					glPopMatrix();
				}
			}
		}
		fillGradient(mListX, mListY, mListX + mListW, mListY + 6, 0x44000000, 0x00000000);
		fillGradient(mListX, mListY + mListH - 6, mListX + mListW, mListY + mListH, 0x00000000, 0x44000000);
		glDisable(GL_SCISSOR_TEST);

		frameLayer.draw(mListX - 1, mListY - 2, mListW + 2, mListH + 4);

		if (activeRecipe && font && mCraftButton) {
			drawWordWrap(font, activeRecipe->desc, mCraftButton->posX, mCraftButton->posY + mCraftButton->height + 6, mCraftButton->width, 0xFFF0F0F0);
		}
		return;
	}

	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	int pw = 176, ph = 166;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	drawPcBevelPanel(this, px, py, pw, ph);

	std::string hoverText;
	if (font) drawString(font, "Crafting", px + 28, py + 6, 0xFF404040);

	// 3x3 crafting grid at (px + 30, py + 17)
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			renderPcSlotWithItem(this, font, px + 30 + c * 18, py + 17 + r * 18, mCraftGrid.at(r * 3 + c), mouseX, mouseY, nullptr, hoverText);
		}
	}

	// Arrow at (px + 90, py + 35)
	if (bindTexture("textures/gui/newgui/arrow_large.png")) {
		blitScaled(px + 90, py + 35, 22, 15, 0, 0, 22, 15, 22, 15);
	} else if (font) {
		drawString(font, "->", px + 94, py + 39, 0xFF404040);
	}

	// Large result slot (26x26) at (px + 120, py + 31), hit-box (18x18) at (px + 124, py + 35)
	drawPcLargeSlotBox(this, px + 120, py + 31);
	if (!mResultSlot.isNull()) {
		drawPcItemStack(this, font, mResultSlot, px + 125, py + 36);
	}
	if (pointInSlot(mouseX, mouseY, px + 124, py + 35)) {
		fill(px + 125, py + 36, px + 141, py + 52, 0x80FFFFFF);
		if (!mResultSlot.isNull()) {
			hoverText = getItemDisplayName(mResultSlot);
		}
	}

	if (font) drawString(font, "Inventory", px + 8, py + 72, 0xFF404040);
	renderPcPlayerInventorySlots(this, font, context, px, py, mouseX, mouseY, hoverText);

	Screen::render(mouseX, mouseY, partialTicks);

	if (mCarriedItem.isNull() && !hoverText.empty()) {
		drawPcTooltip(this, font, hoverText, mouseX, mouseY, width, height);
	}
	if (!mCarriedItem.isNull()) {
		drawPcItemStack(this, font, mCarriedItem, mouseX - 8, mouseY - 8);
	}
}

void PaneCraftingScreen::mouseClicked(int x, int y, int button) {
	if (isTouchUI(context)) {
		if (button == 0 && x >= mListX && x < mListX + mListW && y >= mListY && y < mListY + mListH) {
			mDragging = true;
			mLastDragY = y;
			int idx = (y - mListY + mScrollY) / 22;
			if (idx >= 0 && idx < static_cast<int>(mTouchRecipes.size())) {
				mSelectedRecipe = idx;
			}
			return;
		}
		Screen::mouseClicked(x, y, button);
		return;
	}

	if ((button == 0 || button == 1) && context) {
		int pw = 176, ph = 166;
		int px = (width - pw) / 2, py = (height - ph) / 2;

		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				if (pointInSlot(x, y, px + 30 + c * 18, py + 17 + r * 18)) {
					if (handlePcNormalSlotClick(mCraftGrid.at(r * 3 + c), mCarriedItem, button)) {
						updateCraftingResult();
					}
					return;
				}
			}
		}

		if (pointInSlot(x, y, px + 124, py + 35)) {
			if (handlePcResultSlotClick(mResultSlot, mCarriedItem, mCraftGrid)) {
				updateCraftingResult();
			}
			return;
		}

		if (handlePcPlayerInventoryClick(context, mCarriedItem, px, py, x, y, button)) {
			return;
		}
		if (handlePcOutsideDrop(context, mCarriedItem, px, py, pw, ph, x, y, button)) {
			return;
		}
	}

	Screen::mouseClicked(x, y, button);
}

void PaneCraftingScreen::mouseReleased(int x, int y, int button) {
	mDragging = false;
	Screen::mouseReleased(x, y, button);
}

void PaneCraftingScreen::buttonClicked(Button* button) {
	if (!context || !button) return;
	if (button->id == 1) {
		closeScreen();
	} else if (button->id == 2 && isTouchUI(context)) {
		if (mSelectedRecipe >= 0 && mSelectedRecipe < static_cast<int>(mTouchRecipes.size())) {
			const TouchRecipeEntry& active = mTouchRecipes[static_cast<size_t>(mSelectedRecipe)];
			if (active.canBeCrafted) {
				for (const TouchRecipeReq& req : active.reqs) {
					int remaining = req.need;
					int reqId = req.item.getId();
					int reqAux = req.item.getAuxValue();
					for (ItemInstance& slot : context->inventorySlots) {
						if (remaining <= 0) break;
						if (!slot.isNull() && slot.getId() == reqId && (reqAux == -1 || slot.getAuxValue() == reqAux)) {
							int take = std::min(remaining, static_cast<int>(slot.getStackSize()));
							slot.remove(take);
							if (slot.getStackSize() <= 0) slot.setNull();
							remaining -= take;
						}
					}
				}
				context->addItemToInventory(active.result);
				recheckRecipes();
			}
		}
	} else if (button->id >= 100 && button->id < 100 + mNumCategories) {
		mCategory = button->id - 100;
		mSelectedRecipe = 0;
		mScrollY = 0;
		recheckRecipes();
	}
}

void PaneCraftingScreen::keyPressed(int key) {
	if (key == 256 /* ESCAPE */ || key == 69 /* E */) {
		closeScreen();
		return;
	}
	Screen::keyPressed(key);
}

// --- WorkbenchScreen ---
WorkbenchScreen::WorkbenchScreen(bool is2x2)
	: PaneCraftingScreen(4, 0, is2x2, "Crafting Table") {
}

// --- StonecutterScreen ---
StonecutterScreen::StonecutterScreen()
	: PaneCraftingScreen(1, 5, false, "Stonecutter") {
}

// --- Smelting helpers & Furnace tick ---
namespace {

ItemInstance getSmeltingResult(const ItemInstance& input) {
	if (input.isNull() || input.getStackSize() <= 0) return ItemInstance();
	int id = input.getId();
	if (Block::mCobblestone && id == Block::mCobblestone->mID && Block::mStone) {
		return ItemInstance(Block::mStone, 1, 0);
	}
	if (Block::mSand && id == Block::mSand->mID && Block::mGlass) {
		return ItemInstance(Block::mGlass, 1, 0);
	}
	if (Block::mIronOre && id == Block::mIronOre->mID && Item::mIronIngot) {
		return ItemInstance(Item::mIronIngot, 1, 0);
	}
	if (Block::mGoldOre && id == Block::mGoldOre->mID && Item::mGoldIngot) {
		return ItemInstance(Item::mGoldIngot, 1, 0);
	}
	if (Block::mClay && id == Block::mClay->mID && Block::mHardenedClay) {
		return ItemInstance(Block::mHardenedClay, 1, 0);
	}
	if (Item::mClay && id == Item::mClay->getId() && Item::mBrick) {
		return ItemInstance(Item::mBrick, 1, 0);
	}
	if (Block::mLog && id == Block::mLog->mID && Item::mCoal) {
		return ItemInstance(Item::mCoal, 1, 1); // Charcoal
	}
	if (Item::mPorkChop_raw && id == Item::mPorkChop_raw->getId() && Item::mPorkChop_cooked) {
		return ItemInstance(Item::mPorkChop_cooked, 1, 0);
	}
	if (Item::mBeef_raw && id == Item::mBeef_raw->getId() && Item::mBeef_cooked) {
		return ItemInstance(Item::mBeef_cooked, 1, 0);
	}
	if (Item::mChicken_raw && id == Item::mChicken_raw->getId() && Item::mChicken_cooked) {
		return ItemInstance(Item::mChicken_cooked, 1, 0);
	}
	if (Item::mPotato && id == Item::mPotato->getId() && Item::mPotatoBaked) {
		return ItemInstance(Item::mPotatoBaked, 1, 0);
	}
	if (Item::mFish_raw_cod && id == Item::mFish_raw_cod->getId() && Item::mFish_cooked_cod) {
		return ItemInstance(Item::mFish_cooked_cod, 1, 0);
	}
	return ItemInstance();
}

float getFuelBurnSeconds(const ItemInstance& fuel) {
	if (fuel.isNull() || fuel.getStackSize() <= 0) return 0.0f;
	int id = fuel.getId();
	if (Item::mCoal && id == Item::mCoal->getId()) return 80.0f;
	if (Block::mWoodPlanks && id == Block::mWoodPlanks->mID) return 15.0f;
	if (Block::mLog && id == Block::mLog->mID) return 15.0f;
	if (Block::mWorkBench && id == Block::mWorkBench->mID) return 15.0f;
	if (Block::mChest && id == Block::mChest->mID) return 15.0f;
	if (Item::mStick && id == Item::mStick->getId()) return 5.0f;
	if (Block::mSapling && id == Block::mSapling->mID) return 5.0f;
	if (Block::mCoalBlock && id == Block::mCoalBlock->mID) return 800.0f;
	return 0.0f;
}

bool handleTakeOnlySlotClick(ItemInstance& slot, ItemInstance& carried) {
	if (slot.isNull()) return false;
	int maxStack = std::max(1, static_cast<int>(slot.getMaxStackSize()));
	if (carried.isNull()) {
		carried = slot;
		slot.setNull();
		return true;
	}
	if (carried.sameItemAndAux(&slot) && carried.hasSameUserData(slot)) {
		int space = maxStack - static_cast<int>(carried.getStackSize());
		if (space > 0) {
			int move = std::min(space, static_cast<int>(slot.getStackSize()));
			carried.add(move);
			slot.remove(move);
			if (slot.getStackSize() <= 0) slot.setNull();
			return true;
		}
	}
	return false;
}

} // namespace

void tickFurnaces(GuiContext& ctx, float dt) {
	if (dt <= 0.0f) return;
	for (auto& kv : ctx.furnaces) {
		FurnaceData& fd = kv.second;
		if (fd.burnTime > 0.0f) {
			fd.burnTime = std::max(0.0f, fd.burnTime - dt);
		}
		ItemInstance result = getSmeltingResult(fd.input);
		bool canSmelt = !result.isNull() &&
			(fd.output.isNull() ||
			 (fd.output.sameItemAndAux(&result) && fd.output.hasSameUserData(result) &&
			  static_cast<int>(fd.output.getStackSize()) + static_cast<int>(result.getStackSize()) <= static_cast<int>(fd.output.getMaxStackSize())));

		if (canSmelt) {
			if (fd.burnTime <= 0.0f) {
				float fuelSecs = getFuelBurnSeconds(fd.fuel);
				if (fuelSecs > 0.0f) {
					fd.burnTime = fuelSecs;
					fd.burnDuration = fuelSecs;
					fd.fuel.remove(1);
					if (fd.fuel.getStackSize() <= 0) fd.fuel.setNull();
				}
			}
			if (fd.burnTime > 0.0f) {
				fd.cookTime += dt;
				if (fd.cookTime >= 10.0f) {
					fd.cookTime = 0.0f;
					fd.input.remove(1);
					if (fd.input.getStackSize() <= 0) fd.input.setNull();
					if (fd.output.isNull()) {
						fd.output = result;
					} else {
						fd.output.add(static_cast<int>(result.getStackSize()));
					}
				}
			} else {
				fd.cookTime = 0.0f;
			}
		} else {
			fd.cookTime = 0.0f;
		}
	}
}

// --- FurnaceScreen ---
FurnaceScreen::FurnaceScreen(const BlockCoordKey& pos) : mPos(pos) {}

void FurnaceScreen::closeScreen() {
	if (context && !mCarriedItem.isNull()) {
		context->addItemToInventory(mCarriedItem);
		mCarriedItem.setNull();
	}
	if (context) context->startGame();
}

void FurnaceScreen::init() {
	if (context) context->ensureInventoryInitialized();
	mCarriedItem.setNull();
	mCloseButton = makeInvBtn(context, 1, "X");
	buttons.push_back(mCloseButton);
}

void FurnaceScreen::setupPositions() {
	int pw = 176, ph = 166;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	if (mCloseButton) { mCloseButton->posX = px + pw - 20; mCloseButton->posY = py - 22; mCloseButton->width = 20; mCloseButton->height = 20; }
}

void FurnaceScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	int pw = 176, ph = 166;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	drawPanel(this, px, py, pw, ph);
	if (font) drawString(font, "Furnace", px + 8, py + 6, 0xFF404040);

	std::string hoverText;
	FurnaceData* fd = context ? &context->furnaces[mPos] : nullptr;
	ItemInstance input = fd ? fd->input : ItemInstance();
	ItemInstance fuel = fd ? fd->fuel : ItemInstance();
	ItemInstance output = fd ? fd->output : ItemInstance();

	renderPcSlotWithItem(this, font, px + 56, py + 17, input, mouseX, mouseY, nullptr, hoverText);
	renderPcSlotWithItem(this, font, px + 56, py + 53, fuel, mouseX, mouseY, nullptr, hoverText);
	renderPcSlotWithItem(this, font, px + 116, py + 35, output, mouseX, mouseY, nullptr, hoverText);

	// Burn & cook progress indicators
	fill(px + 56, py + 37, px + 72, py + 50, 0xFF8B8B8B);
	if (fd && fd->burnTime > 0.0f && fd->burnDuration > 0.0f) {
		int h = std::max(1, std::min(13, static_cast<int>(13.0f * (fd->burnTime / fd->burnDuration))));
		fill(px + 58, py + 50 - h, px + 70, py + 50, 0xFFFF8800);
	}
	fill(px + 80, py + 40, px + 104, py + 46, 0xFF8B8B8B);
	if (fd && fd->cookTime > 0.0f) {
		int w = std::max(1, std::min(24, static_cast<int>(24.0f * (fd->cookTime / 10.0f))));
		fill(px + 80, py + 40, px + 80 + w, py + 46, 0xFFFFFFFF);
	}

	if (font) drawString(font, "Inventory", px + 8, py + 72, 0xFF404040);
	renderPcPlayerInventorySlots(this, font, context, px, py, mouseX, mouseY, hoverText);

	Screen::render(mouseX, mouseY, partialTicks);

	if (mCarriedItem.isNull() && !hoverText.empty()) {
		drawPcTooltip(this, font, hoverText, mouseX, mouseY, width, height);
	}
	if (!mCarriedItem.isNull()) {
		drawPcItemStack(this, font, mCarriedItem, mouseX - 8, mouseY - 8);
	}
}

void FurnaceScreen::mouseClicked(int x, int y, int button) {
	if ((button == 0 || button == 1) && context) {
		int pw = 176, ph = 166;
		int px = (width - pw) / 2, py = (height - ph) / 2;
		FurnaceData& fd = context->furnaces[mPos];

		if (pointInSlot(x, y, px + 56, py + 17)) {
			handlePcNormalSlotClick(fd.input, mCarriedItem, button);
			return;
		}
		if (pointInSlot(x, y, px + 56, py + 53)) {
			handlePcNormalSlotClick(fd.fuel, mCarriedItem, button);
			return;
		}
		if (pointInSlot(x, y, px + 116, py + 35)) {
			handleTakeOnlySlotClick(fd.output, mCarriedItem);
			return;
		}
		if (handlePcPlayerInventoryClick(context, mCarriedItem, px, py, x, y, button)) {
			return;
		}
		if (handlePcOutsideDrop(context, mCarriedItem, px, py, pw, ph, x, y, button)) {
			return;
		}
	}
	Screen::mouseClicked(x, y, button);
}

void FurnaceScreen::buttonClicked(Button* button) {
	if (button && button->id == 1) closeScreen();
}

void FurnaceScreen::keyPressed(int key) {
	if (key == 256 /* ESCAPE */ || key == 69 /* E */) {
		closeScreen();
		return;
	}
	Screen::keyPressed(key);
}

// --- ChestScreen ---
ChestScreen::ChestScreen(bool isDouble) : mIsDouble(isDouble) {}

ChestScreen::ChestScreen(const BlockCoordKey& primaryPos, bool isDouble, const BlockCoordKey& secondaryPos)
	: mPrimaryPos(primaryPos)
	, mSecondaryPos(secondaryPos)
	, mIsDouble(isDouble) {}

void ChestScreen::closeScreen() {
	if (context && !mCarriedItem.isNull()) {
		context->addItemToInventory(mCarriedItem);
		mCarriedItem.setNull();
	}
	if (context) context->startGame();
}

void ChestScreen::init() {
	if (context) context->ensureInventoryInitialized();
	mCarriedItem.setNull();
	mCloseButton = makeInvBtn(context, 1, "X");
	buttons.push_back(mCloseButton);
}

void ChestScreen::setupPositions() {
	int rows = mIsDouble ? 6 : 3;
	int pw = 176;
	int ph = 18 + rows * 18 + 14 + 78;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	if (mCloseButton) { mCloseButton->posX = px + pw - 20; mCloseButton->posY = py - 22; mCloseButton->width = 20; mCloseButton->height = 20; }
}

void ChestScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	int rows = mIsDouble ? 6 : 3;
	int chestH = rows * 18;
	int pw = 176;
	int ph = 18 + chestH + 14 + 78;
	int px = (width - pw) / 2, py = (height - ph) / 2;
	drawPanel(this, px, py, pw, ph);
	if (font) drawString(font, mIsDouble ? "Large Chest" : "Chest", px + 8, py + 6, 0xFF404040);

	std::string hoverText;
	ChestData* primary = context ? &context->chests[mPrimaryPos] : nullptr;
	ChestData* secondary = (context && mIsDouble) ? &context->chests[mSecondaryPos] : nullptr;

	for (int r = 0; r < rows; ++r) {
		for (int c = 0; c < 9; ++c) {
			ItemInstance slotItem;
			if (r < 3 && primary) {
				slotItem = primary->slots[static_cast<size_t>(r * 9 + c)];
			} else if (r >= 3 && secondary) {
				slotItem = secondary->slots[static_cast<size_t>((r - 3) * 9 + c)];
			}
			renderPcSlotWithItem(this, font, px + 8 + c * 18, py + 18 + r * 18, slotItem, mouseX, mouseY, nullptr, hoverText);
		}
	}

	int invHeaderY = py + 18 + chestH + 3;
	if (font) drawString(font, "Inventory", px + 8, invHeaderY, 0xFF404040);
	int invAnchorY = py + 18 + chestH + 14 - 84;
	renderPcPlayerInventorySlots(this, font, context, px, invAnchorY, mouseX, mouseY, hoverText);

	Screen::render(mouseX, mouseY, partialTicks);

	if (mCarriedItem.isNull() && !hoverText.empty()) {
		drawPcTooltip(this, font, hoverText, mouseX, mouseY, width, height);
	}
	if (!mCarriedItem.isNull()) {
		drawPcItemStack(this, font, mCarriedItem, mouseX - 8, mouseY - 8);
	}
}

void ChestScreen::mouseClicked(int x, int y, int button) {
	if ((button == 0 || button == 1) && context) {
		int rows = mIsDouble ? 6 : 3;
		int chestH = rows * 18;
		int pw = 176;
		int ph = 18 + chestH + 14 + 78;
		int px = (width - pw) / 2, py = (height - ph) / 2;

		ChestData& primary = context->chests[mPrimaryPos];
		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < 9; ++c) {
				if (pointInSlot(x, y, px + 8 + c * 18, py + 18 + r * 18)) {
					if (r < 3) {
						handlePcNormalSlotClick(primary.slots[static_cast<size_t>(r * 9 + c)], mCarriedItem, button);
					} else if (mIsDouble) {
						ChestData& secondary = context->chests[mSecondaryPos];
						handlePcNormalSlotClick(secondary.slots[static_cast<size_t>((r - 3) * 9 + c)], mCarriedItem, button);
					}
					return;
				}
			}
		}

		int invAnchorY = py + 18 + chestH + 14 - 84;
		if (handlePcPlayerInventoryClick(context, mCarriedItem, px, invAnchorY, x, y, button)) {
			return;
		}
		if (handlePcOutsideDrop(context, mCarriedItem, px, py, pw, ph, x, y, button)) {
			return;
		}
	}
	Screen::mouseClicked(x, y, button);
}

void ChestScreen::buttonClicked(Button* button) {
	if (button && button->id == 1) closeScreen();
}

void ChestScreen::keyPressed(int key) {
	if (key == 256 /* ESCAPE */ || key == 69 /* E */) {
		closeScreen();
		return;
	}
	Screen::keyPressed(key);
}

// --- TextEditScreen ---
void TextEditScreen::init() {
	mDoneButton = makeInvBtn(context, 1, "Done");
	buttons.push_back(mDoneButton);
}

void TextEditScreen::setupPositions() {
	if (mDoneButton) {
		mDoneButton->posX = width / 2 - 100;
		mDoneButton->posY = height - 40;
		mDoneButton->width = 200;
		mDoneButton->height = 20;
	}
}

void TextEditScreen::render(int mouseX, int mouseY, float partialTicks) {
	fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010);
	if (font) drawCenteredString(font, "Edit Sign Message", width / 2, 24, 0xFFFFFFFF);
	int bx = width / 2 - 60, by = 52;
	fill(bx, by, bx + 120, by + 64, 0xFF9C7B49);
	drawRect(bx, by, bx + 120, by + 64, 0xFF403018);
	for (int i = 0; i < 4; ++i) {
		std::string line = mLines[i] + (mActiveLine == i ? "_" : "");
		if (font) drawCenteredString(font, line, width / 2, by + 8 + i * 14, 0xFF000000);
	}
	Screen::render(mouseX, mouseY, partialTicks);
}

void TextEditScreen::mouseClicked(int x, int y, int button) {
	if (button == 0) {
		int bx = width / 2 - 60, by = 52;
		if (x >= bx && x < bx + 120 && y >= by && y < by + 64) {
			mActiveLine = (y - by) / 16;
			if (mActiveLine < 0) mActiveLine = 0;
			if (mActiveLine > 3) mActiveLine = 3;
		}
	}
	Screen::mouseClicked(x, y, button);
}

void TextEditScreen::keyPressed(int key) {
	if (key == 259 /* BACKSPACE */) {
		if (!mLines[mActiveLine].empty()) mLines[mActiveLine].pop_back();
	} else if (key == 257 /* ENTER */) {
		mActiveLine = (mActiveLine + 1) % 4;
	} else if (key >= 32 && key <= 126) {
		if (mLines[mActiveLine].size() < 15) mLines[mActiveLine].push_back(static_cast<char>(key));
	}
	Screen::keyPressed(key);
}

void TextEditScreen::buttonClicked(Button* button) {
	if (context && button && button->id == 1) context->startGame();
}
