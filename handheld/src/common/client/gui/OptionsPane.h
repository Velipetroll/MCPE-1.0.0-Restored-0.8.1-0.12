#pragma once

#include "GuiElement.h"
#include "Options.h"
#include <string>
#include <vector>

class OptionsItem : public GuiElement {
public:
	OptionsItem(const std::string& label, GuiElement* control);
	~OptionsItem() override;

	void setupPositions() override;
	void tick(GuiContext* ctx) override;
	void render(GuiContext* ctx, int mouseX, int mouseY) override;
	void mouseClicked(GuiContext* ctx, int x, int y, int button) override;
	void mouseReleased(GuiContext* ctx, int x, int y, int button) override;

private:
	std::string mLabel;
	GuiElement* mControl = nullptr;
};

class OptionsGroup : public GuiElement {
public:
	explicit OptionsGroup(const std::string& title);
	~OptionsGroup() override;

	OptionsGroup* addOptionItem(const std::string& label, GuiElement* control);
	void setupPositions() override;
	void tick(GuiContext* ctx) override;
	void render(GuiContext* ctx, int mouseX, int mouseY) override;
	void mouseClicked(GuiContext* ctx, int x, int y, int button) override;
	void mouseReleased(GuiContext* ctx, int x, int y, int button) override;

private:
	std::string mTitle;
	std::vector<OptionsItem*> mItems;
};

class OptionsPane : public GuiElement {
public:
	~OptionsPane() override;

	OptionsGroup* createOptionsGroup(const std::string& title);
	void setupPositions() override;
	void tick(GuiContext* ctx) override;
	void render(GuiContext* ctx, int mouseX, int mouseY) override;
	void mouseClicked(GuiContext* ctx, int x, int y, int button) override;
	void mouseReleased(GuiContext* ctx, int x, int y, int button) override;
	void scrollBy(int dy);

private:
	std::vector<OptionsGroup*> mGroups;
	int mScrollY = 0;
	int mContentHeight = 0;
	bool mDraggingScroll = false;
	int mLastDragY = 0;
};
