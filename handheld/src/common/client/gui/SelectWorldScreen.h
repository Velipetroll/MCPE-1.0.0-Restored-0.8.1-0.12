#pragma once

#include "Screen.h"
#include <string>
#include <vector>

struct WorldEntry {
	std::string name;
	std::string path;
	std::string info;
	bool isDefault = false;
};

class SelectWorldScreen : public Screen {
public:
	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	void mouseClicked(int x, int y, int button) override;

private:
	std::vector<WorldEntry> mWorlds;
	int mSelectedIndex = 0;
	bool mEditMode = false;
	int mScrollX = 0;
	bool mDragging = false;
	int mLastDragX = 0;

	Button* mPlayButton = nullptr;
	Button* mCreateButton = nullptr;
	Button* mDeleteButton = nullptr;
	Button* mExternalButton = nullptr;
	Button* mBackButton = nullptr;

	void loadWorldList();
	void updateButtonStates();
};
