#include "OptionsPane.h"
#include "GuiContext.h"
#include "Font.h"
#include <glad/glad.h>
#include <algorithm>

OptionsItem::OptionsItem(const std::string& label, GuiElement* control)
	: GuiElement(0, 0, 200, 20), mLabel(label), mControl(control) {
	if (mControl && mControl->height > height) {
		height = mControl->height;
	}
}

OptionsItem::~OptionsItem() {
	delete mControl;
}

void OptionsItem::setupPositions() {
	if (mControl) {
		mControl->posX = posX + width - mControl->width - 15;
		mControl->posY = posY;
	}
}

void OptionsItem::tick(GuiContext* ctx) {
	if (mControl) mControl->tick(ctx);
}

void OptionsItem::render(GuiContext* ctx, int mouseX, int mouseY) {
	if (!visible) return;
	if (ctx && ctx->font) {
		ctx->font->drawShadow(mLabel, posX, posY + (height - 8) / 2, 0xFFBBBBBB);
	}
	if (mControl) {
		mControl->render(ctx, mouseX, mouseY);
	}
}

void OptionsItem::mouseClicked(GuiContext* ctx, int x, int y, int button) {
	if (mControl) mControl->mouseClicked(ctx, x, y, button);
}

void OptionsItem::mouseReleased(GuiContext* ctx, int x, int y, int button) {
	if (mControl) mControl->mouseReleased(ctx, x, y, button);
}

OptionsGroup::OptionsGroup(const std::string& title)
	: GuiElement(0, 0, 200, 14), mTitle(title) {
}

OptionsGroup::~OptionsGroup() {
	for (OptionsItem* item : mItems) delete item;
}

OptionsGroup* OptionsGroup::addOptionItem(const std::string& label, GuiElement* control) {
	mItems.push_back(new OptionsItem(label, control));
	return this;
}

void OptionsGroup::setupPositions() {
	int curY = posY + 12;
	for (OptionsItem* item : mItems) {
		item->posX = posX + 10;
		item->posY = curY;
		item->width = width - 5;
		item->setupPositions();
		curY += item->height + 3;
	}
	height = curY - posY;
}

void OptionsGroup::tick(GuiContext* ctx) {
	for (OptionsItem* item : mItems) item->tick(ctx);
}

void OptionsGroup::render(GuiContext* ctx, int mouseX, int mouseY) {
	if (!visible) return;
	if (ctx && ctx->font) {
		ctx->font->drawShadow(mTitle, posX + 2, posY, 0xFFFFFFFF);
	}
	for (OptionsItem* item : mItems) {
		item->render(ctx, mouseX, mouseY);
	}
}

void OptionsGroup::mouseClicked(GuiContext* ctx, int x, int y, int button) {
	for (OptionsItem* item : mItems) item->mouseClicked(ctx, x, y, button);
}

void OptionsGroup::mouseReleased(GuiContext* ctx, int x, int y, int button) {
	for (OptionsItem* item : mItems) item->mouseReleased(ctx, x, y, button);
}

OptionsPane::~OptionsPane() {
	for (OptionsGroup* g : mGroups) delete g;
}

OptionsGroup* OptionsPane::createOptionsGroup(const std::string& title) {
	auto* g = new OptionsGroup(title);
	mGroups.push_back(g);
	return g;
}

void OptionsPane::setupPositions() {
	int curY = posY + 4 - mScrollY;
	int totalH = 4;
	for (OptionsGroup* g : mGroups) {
		g->posX = posX;
		g->posY = curY;
		g->width = width;
		g->setupPositions();
		curY += g->height + 8;
		totalH += g->height + 8;
	}
	mContentHeight = totalH;
}

void OptionsPane::scrollBy(int dy) {
	int maxScroll = std::max(0, mContentHeight - height);
	mScrollY = std::max(0, std::min(maxScroll, mScrollY + dy));
	setupPositions();
}

void OptionsPane::tick(GuiContext* ctx) {
	for (OptionsGroup* g : mGroups) g->tick(ctx);
}

void OptionsPane::render(GuiContext* ctx, int mouseX, int mouseY) {
	if (!visible) return;

	if (mDraggingScroll) {
		int dy = mLastDragY - mouseY;
		mLastDragY = mouseY;
		if (dy != 0) scrollBy(dy);
	}

	int scale = ctx ? ctx->guiScale : 2;
	int winH = ctx ? ctx->windowHeight : (height * scale);
	int sx = posX * scale;
	int sy = winH - (posY + height) * scale;
	int sw = width * scale;
	int sh = height * scale;

	glEnable(GL_SCISSOR_TEST);
	glScissor(sx, sy, sw > 0 ? sw : 1, sh > 0 ? sh : 1);
	for (OptionsGroup* g : mGroups) {
		g->render(ctx, mouseX, mouseY);
	}
	glDisable(GL_SCISSOR_TEST);
}

void OptionsPane::mouseClicked(GuiContext* ctx, int x, int y, int button) {
	if (!pointInside(x, y)) return;
	if (button == 0) {
		mDraggingScroll = true;
		mLastDragY = y;
	}
	for (OptionsGroup* g : mGroups) g->mouseClicked(ctx, x, y, button);
}

void OptionsPane::mouseReleased(GuiContext* ctx, int x, int y, int button) {
	mDraggingScroll = false;
	for (OptionsGroup* g : mGroups) g->mouseReleased(ctx, x, y, button);
}
