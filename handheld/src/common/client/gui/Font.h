#pragma once

#include <cstdint>
#include <string>

class Font {
public:
	Font();
	~Font() = default;

	bool init(const std::string& texturePath = "font/default8.png");

	void draw(const std::string& text, int x, int y, uint32_t color, bool shadow = false);
	void drawShadow(const std::string& text, int x, int y, uint32_t color);
	int width(const std::string& text) const;
	int height() const;
	int charWidth(unsigned char c) const;

private:
	unsigned int mTextureId = 0;
	int mCharWidths[256];
	bool mInitialized = false;

	void renderInternal(const std::string& text, int x, int y, uint32_t color, bool darken);
};
