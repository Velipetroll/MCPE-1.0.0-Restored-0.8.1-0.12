#include "NinePatchLayer.h"

NinePatchLayer::NinePatchLayer(const std::string& texturePath, const IntRectangle& srcRect,
                               int borderLeft, int borderTop, int borderRight, int borderBottom,
                               int texW, int texH)
	: mTexturePath(texturePath)
	, mSrc(srcRect)
	, mBorderLeft(borderLeft)
	, mBorderTop(borderTop)
	, mBorderRight(borderRight)
	, mBorderBottom(borderBottom)
	, mTexW(texW)
	, mTexH(texH) {
}

void NinePatchLayer::draw(int x, int y, int w, int h) const {
	if (!bindTexture(mTexturePath)) {
		fill(x, y, x + w, y + h, 0xFF666666);
		return;
	}

	int bl = mBorderLeft;
	int br = mBorderRight;
	int bt = mBorderTop;
	int bb = mBorderBottom;

	int centerW = w - bl - br;
	int centerH = h - bt - bb;
	int srcCenterW = mSrc.w - bl - br;
	int srcCenterH = mSrc.h - bt - bb;

	int sx0 = mSrc.x;
	int sx1 = mSrc.x + bl;
	int sx2 = mSrc.x + bl + srcCenterW;

	int sy0 = mSrc.y;
	int sy1 = mSrc.y + bt;
	int sy2 = mSrc.y + bt + srcCenterH;

	// Top row
	blitScaled(x, y, bl, bt, sx0, sy0, bl, bt, mTexW, mTexH);
	if (centerW > 0) blitScaled(x + bl, y, centerW, bt, sx1, sy0, srcCenterW, bt, mTexW, mTexH);
	blitScaled(x + bl + centerW, y, br, bt, sx2, sy0, br, bt, mTexW, mTexH);

	// Middle row
	if (centerH > 0) {
		blitScaled(x, y + bt, bl, centerH, sx0, sy1, bl, srcCenterH, mTexW, mTexH);
		if (centerW > 0) blitScaled(x + bl, y + bt, centerW, centerH, sx1, sy1, srcCenterW, srcCenterH, mTexW, mTexH);
		blitScaled(x + bl + centerW, y + bt, br, centerH, sx2, sy1, br, srcCenterH, mTexW, mTexH);
	}

	// Bottom row
	blitScaled(x, y + bt + centerH, bl, bb, sx0, sy2, bl, bb, mTexW, mTexH);
	if (centerW > 0) blitScaled(x + bl, y + bt + centerH, centerW, bb, sx1, sy2, srcCenterW, bb, mTexW, mTexH);
	blitScaled(x + bl + centerW, y + bt + centerH, br, bb, sx2, sy2, br, bb, mTexW, mTexH);
}
