#include "Font.h"
#include "GuiComponent.h"

#include <glad/glad.h>
#include "stb_image.h"

Font::Font() {
	for (int i = 0; i < 256; ++i) {
		mCharWidths[i] = 6;
	}
	mCharWidths[' '] = 4;
}

bool Font::init(const std::string& texturePath) {
	std::string fullPath = GuiComponent::resolveAssetPath(texturePath);
	int w = 0, h = 0, channels = 0;
	stbi_set_flip_vertically_on_load(0);
	unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &channels, 4);
	if (!data) {
		fullPath = GuiComponent::resolveAssetPath("font/glyph_00.png");
		data = stbi_load(fullPath.c_str(), &w, &h, &channels, 4);
		if (!data) {
			return false;
		}
	}

	int cellW = w / 16;
	int cellH = h / 16;
	if (cellW > 0 && cellH > 0) {
		for (int c = 0; c < 256; ++c) {
			int col = c % 16;
			int row = c / 16;
			int maxX = 0;
			bool hasPixel = false;
			for (int px = 0; px < cellW; ++px) {
				for (int py = 0; py < cellH; ++py) {
					int imgX = col * cellW + px;
					int imgY = row * cellH + py;
					unsigned char alpha = data[(imgY * w + imgX) * 4 + 3];
					if (alpha > 10) {
						hasPixel = true;
						if (px > maxX) maxX = px;
					}
				}
			}
			if (c == ' ') {
				mCharWidths[c] = 4;
			} else if (hasPixel) {
				mCharWidths[c] = (maxX + 1) * 8 / cellW + 1;
			} else {
				mCharWidths[c] = 4;
			}
		}
	}

	GLuint texId = 0;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	stbi_image_free(data);

	mTextureId = texId;
	mInitialized = true;
	return true;
}

int Font::height() const {
	return 8;
}

int Font::charWidth(unsigned char c) const {
	return mCharWidths[c];
}

int Font::width(const std::string& text) const {
	int total = 0;
	for (unsigned char c : text) {
		total += mCharWidths[c];
	}
	return total;
}

void Font::draw(const std::string& text, int x, int y, uint32_t color, bool shadow) {
	if (!mInitialized || text.empty()) return;
	if ((color & 0xFC000000u) == 0) {
		color |= 0xFF000000u;
	}
	if (shadow) {
		renderInternal(text, x + 1, y + 1, color, true);
	}
	renderInternal(text, x, y, color, false);
}

void Font::drawShadow(const std::string& text, int x, int y, uint32_t color) {
	draw(text, x, y, color, true);
}

void Font::renderInternal(const std::string& text, int x, int y, uint32_t color, bool darken) {
	float a = ((color >> 24) & 0xFF) / 255.0f;
	float r = ((color >> 16) & 0xFF) / 255.0f;
	float g = ((color >> 8) & 0xFF) / 255.0f;
	float b = (color & 0xFF) / 255.0f;
	if (darken) {
		r *= 0.25f;
		g *= 0.25f;
		b *= 0.25f;
	}

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, mTextureId);
	glColor4f(r, g, b, a);

	float curX = static_cast<float>(x);
	float curY = static_cast<float>(y);

	glBegin(GL_QUADS);
	for (unsigned char c : text) {
		int col = c % 16;
		int row = c / 16;
		float u0 = col / 16.0f;
		float v0 = row / 16.0f;
		float u1 = (col + 1) / 16.0f;
		float v1 = (row + 1) / 16.0f;

		glTexCoord2f(u0, v0); glVertex2f(curX, curY);
		glTexCoord2f(u0, v1); glVertex2f(curX, curY + 8.0f);
		glTexCoord2f(u1, v1); glVertex2f(curX + 8.0f, curY + 8.0f);
		glTexCoord2f(u1, v0); glVertex2f(curX + 8.0f, curY);

		curX += static_cast<float>(mCharWidths[c]);
	}
	glEnd();
}
