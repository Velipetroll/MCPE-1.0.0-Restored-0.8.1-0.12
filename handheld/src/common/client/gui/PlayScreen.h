#pragma once

#include "Screen.h"
#include "NinePatchLayer.h"
#include "SelectWorldScreen.h"
#include <string>
#include <vector>

class PlayScreen : public Screen {
public:
	explicit PlayScreen(bool localWorlds = true);
	~PlayScreen() override;

	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	void mouseClicked(int x, int y, int button) override;
	void mouseReleased(int x, int y, int button) override;

private:
	bool mLocalWorlds = true;
	std::vector<WorldEntry> mWorlds;
	bool mEditMode = false;
	int mScrollY = 0;
	bool mDragging = false;
	int mLastDragY = 0;

	int mListX = 10;
	int mListY = 32;
	int mListW = 200;
	int mListH = 150;

	NinePatchLayer* mFrame = nullptr;
	NinePatchLayer* mItemNormal = nullptr;
	NinePatchLayer* mItemPressed = nullptr;
	Button* mHeader = nullptr;
	Button* mBackButton = nullptr;
	Button* mEditButton = nullptr;
	Button* mNewButton = nullptr;
	Button* mExternalButton = nullptr;

	void loadWorlds();
};
