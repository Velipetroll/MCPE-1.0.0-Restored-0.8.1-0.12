#pragma once

#include "Screen.h"
#include <string>

class ProgressScreen : public Screen {
public:
	explicit ProgressScreen(const std::string& title = "Generating world");
	~ProgressScreen() override;

	void init() override;
	void tick() override;
	void render(int mouseX, int mouseY, float partialTicks) override;

private:
	std::string mTitle;
	float mProgress = 0.0f;
};
