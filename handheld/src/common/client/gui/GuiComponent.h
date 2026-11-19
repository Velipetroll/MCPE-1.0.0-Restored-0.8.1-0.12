#pragma once

#include <cstdint>
#include <string>

class Font;

struct GuiTextureInfo {
	unsigned int id = 0;
	int width = 0;
	int height = 0;
};

class GuiComponent {
public:
	float zLayer = 0.0f;

	virtual ~GuiComponent() = default;

	void blit(int x, int y, int u, int v, int w, int h, int texW = 256, int texH = 256) const;
	void blitScaled(int x, int y, int w, int h, int u, int v, int uw, int vh, int texW = 256, int texH = 256) const;
	void fill(int x0, int y0, int x1, int y1, uint32_t color) const;
	void fillGradient(int x0, int y0, int x1, int y1, uint32_t colorTop, uint32_t colorBottom) const;
	void drawRect(int x0, int y0, int x1, int y1, uint32_t color) const;

	void drawString(Font* font, const std::string& text, int x, int y, uint32_t color) const;
	void drawCenteredString(Font* font, const std::string& text, int x, int y, uint32_t color) const;

	static bool bindTexture(const std::string& relativePath);
	static GuiTextureInfo loadTexture(const std::string& relativePath);
	static std::string resolveAssetPath(const std::string& relativePath);
};
