#pragma once

#include "GuiElement.h"
#include <string>

struct IntRectangle {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

class NinePatchLayer : public GuiElement {
public:
	NinePatchLayer(const std::string& texturePath, const IntRectangle& srcRect,
	               int borderLeft, int borderTop, int borderRight, int borderBottom,
	               int texW = 256, int texH = 256);

	void draw(int x, int y, int w, int h) const;

private:
	std::string mTexturePath;
	IntRectangle mSrc;
	int mBorderLeft;
	int mBorderTop;
	int mBorderRight;
	int mBorderBottom;
	int mTexW;
	int mTexH;
};
