#pragma once

#include <cstdint>
#include <string>

class Font;
class ItemInstance;

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
	void blitScaledTinted(int x, int y, int w, int h, int u, int v, int uw, int vh, int texW, int texH, uint32_t rgbColor) const;
	void fill(int x0, int y0, int x1, int y1, uint32_t color) const;
	void fillGradient(int x0, int y0, int x1, int y1, uint32_t colorTop, uint32_t colorBottom) const;
	void drawRect(int x0, int y0, int x1, int y1, uint32_t color) const;

	void drawString(Font* font, const std::string& text, int x, int y, uint32_t color, bool shadow = true) const;
	void drawCenteredString(Font* font, const std::string& text, int x, int y, uint32_t color, bool shadow = true) const;
	void drawItemInSlot(Font* font, const ItemInstance& item, int x, int y, bool drawCount = true) const;

	static bool bindTexture(const std::string& relativePath);
	static GuiTextureInfo loadTexture(const std::string& relativePath);
	static std::string resolveAssetPath(const std::string& relativePath);

	static bool isBlock3D(const ItemInstance& item);
	static void drawBlock3DModel(const ItemInstance& item, float shade = 1.0f, bool isGui = true);
	static void getItemSpritePathAndTint(const ItemInstance& item, std::string& outTex, uint32_t& outTint);
};
