#pragma once

#include "Screen.h"
#include <string>

class DeleteWorldScreen : public Screen {
public:
	DeleteWorldScreen(const std::string& worldName, const std::string& worldPath);

	void init() override;
	void setupPositions() override;
	void render(int mouseX, int mouseY, float partialTicks) override;
	void buttonClicked(Button* button) override;
	bool handleBackEvent(bool isDown = false) override;

private:
	std::string mWorldName;
	std::string mWorldPath;
	Button* mDeleteButton = nullptr;
	Button* mCancelButton = nullptr;
};
