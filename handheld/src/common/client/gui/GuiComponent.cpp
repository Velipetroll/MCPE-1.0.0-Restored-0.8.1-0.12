#include "GuiComponent.h"
#include "Font.h"
#include "world/crafting/CraftingRecipes.h"
#include "world/item/ItemInstance.h"

#include <glad/glad.h>
#include "stb_image.h"

#include <filesystem>
#include <unordered_map>
#include <vector>

namespace {
	std::unordered_map<std::string, GuiTextureInfo>& getTextureCache() {
		static std::unordered_map<std::string, GuiTextureInfo> cache;
		return cache;
	}

	void unpackARGB(uint32_t color, float& a, float& r, float& g, float& b) {
		a = ((color >> 24) & 0xFF) / 255.0f;
		r = ((color >> 16) & 0xFF) / 255.0f;
		g = ((color >> 8) & 0xFF) / 255.0f;
		b = (color & 0xFF) / 255.0f;
	}
}

std::string GuiComponent::resolveAssetPath(const std::string& relativePath) {
	namespace fs = std::filesystem;
	std::vector<std::string> candidates = {
		relativePath,
		"resource_packs/vanilla/" + relativePath,
		"assets/resource_packs/vanilla/" + relativePath,
		"../assets/resource_packs/vanilla/" + relativePath,
		"../../assets/resource_packs/vanilla/" + relativePath
	};
	for (const auto& path : candidates) {
		if (fs::exists(path)) {
			return path;
		}
	}
	return relativePath;
}

GuiTextureInfo GuiComponent::loadTexture(const std::string& relativePath) {
	auto& cache = getTextureCache();
	auto it = cache.find(relativePath);
	if (it != cache.end()) {
		return it->second;
	}

	std::string fullPath = resolveAssetPath(relativePath);
	int w = 0, h = 0, channels = 0;
	stbi_set_flip_vertically_on_load(0);
	unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &channels, 4);
	GuiTextureInfo info;
	if (data) {
		GLuint texId = 0;
		glGenTextures(1, &texId);
		glBindTexture(GL_TEXTURE_2D, texId);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);

		info.id = texId;
		info.width = w;
		info.height = h;
	}
	cache[relativePath] = info;
	return info;
}

bool GuiComponent::bindTexture(const std::string& relativePath) {
	GuiTextureInfo info = loadTexture(relativePath);
	if (info.id == 0) {
		return false;
	}
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, info.id);
	return true;
}

void GuiComponent::blit(int x, int y, int u, int v, int w, int h, int texW, int texH) const {
	blitScaled(x, y, w, h, u, v, w, h, texW, texH);
}

void GuiComponent::blitScaled(int x, int y, int w, int h, int u, int v, int uw, int vh, int texW, int texH) const {
	float u0 = static_cast<float>(u) / static_cast<float>(texW);
	float v0 = static_cast<float>(v) / static_cast<float>(texH);
	float u1 = static_cast<float>(u + uw) / static_cast<float>(texW);
	float v1 = static_cast<float>(v + vh) / static_cast<float>(texH);

	glEnable(GL_TEXTURE_2D);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBegin(GL_QUADS);
	glTexCoord2f(u0, v0); glVertex3f(static_cast<float>(x), static_cast<float>(y), zLayer);
	glTexCoord2f(u0, v1); glVertex3f(static_cast<float>(x), static_cast<float>(y + h), zLayer);
	glTexCoord2f(u1, v1); glVertex3f(static_cast<float>(x + w), static_cast<float>(y + h), zLayer);
	glTexCoord2f(u1, v0); glVertex3f(static_cast<float>(x + w), static_cast<float>(y), zLayer);
	glEnd();
}

void GuiComponent::fill(int x0, int y0, int x1, int y1, uint32_t color) const {
	if (x0 > x1) std::swap(x0, x1);
	if (y0 > y1) std::swap(y0, y1);

	float a, r, g, b;
	unpackARGB(color, a, r, g, b);

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(r, g, b, a);
	glBegin(GL_QUADS);
	glVertex3f(static_cast<float>(x0), static_cast<float>(y0), zLayer);
	glVertex3f(static_cast<float>(x0), static_cast<float>(y1), zLayer);
	glVertex3f(static_cast<float>(x1), static_cast<float>(y1), zLayer);
	glVertex3f(static_cast<float>(x1), static_cast<float>(y0), zLayer);
	glEnd();
	glEnable(GL_TEXTURE_2D);
}

void GuiComponent::fillGradient(int x0, int y0, int x1, int y1, uint32_t colorTop, uint32_t colorBottom) const {
	if (x0 > x1) std::swap(x0, x1);
	if (y0 > y1) std::swap(y0, y1);

	float at, rt, gt, bt;
	float ab, rb, gb, bb;
	unpackARGB(colorTop, at, rt, gt, bt);
	unpackARGB(colorBottom, ab, rb, gb, bb);

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_QUADS);
	glColor4f(rt, gt, bt, at);
	glVertex3f(static_cast<float>(x0), static_cast<float>(y0), zLayer);
	glColor4f(rb, gb, bb, ab);
	glVertex3f(static_cast<float>(x0), static_cast<float>(y1), zLayer);
	glVertex3f(static_cast<float>(x1), static_cast<float>(y1), zLayer);
	glColor4f(rt, gt, bt, at);
	glVertex3f(static_cast<float>(x1), static_cast<float>(y0), zLayer);
	glEnd();
	glEnable(GL_TEXTURE_2D);
}

void GuiComponent::drawRect(int x0, int y0, int x1, int y1, uint32_t color) const {
	fill(x0, y0, x1, y0 + 1, color);
	fill(x0, y1 - 1, x1, y1, color);
	fill(x0, y0, x0 + 1, y1, color);
	fill(x1 - 1, y0, x1, y1, color);
}

void GuiComponent::drawString(Font* font, const std::string& text, int x, int y, uint32_t color) const {
	if (font) {
		font->drawShadow(text, x, y, color);
	}
}

void GuiComponent::drawCenteredString(Font* font, const std::string& text, int x, int y, uint32_t color) const {
	if (font) {
		int w = font->width(text);
		font->drawShadow(text, x - w / 2, y, color);
	}
}

void GuiComponent::drawItemInSlot(Font* font, const ItemInstance& item, int x, int y, bool drawCount) const {
	if (item.isNull()) return;

	BlockRenderInfo blockInfo = getBlockRenderInfo(item);
	if (blockInfo.is3D) {
		float y0 = -0.5f;
		float y1 = blockInfo.isSlab ? 0.0f : 0.5f;
		float vTop = blockInfo.isSlab ? 0.5f : 0.0f;

		GLboolean wasDepthTest = glIsEnabled(GL_DEPTH_TEST);
		GLboolean wasCullFace = glIsEnabled(GL_CULL_FACE);
		GLboolean wasBlend = glIsEnabled(GL_BLEND);

		glEnable(GL_DEPTH_TEST);
		glDepthMask(blockInfo.isTransparent ? GL_FALSE : GL_TRUE);
		glClear(GL_DEPTH_BUFFER_BIT);
		glDisable(GL_CULL_FACE);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glTranslatef(static_cast<float>(x) + 8.0f, static_cast<float>(y) + 8.0f, 100.0f);
		glScalef(9.5f, -9.5f, 9.5f);
		glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
		glRotatef(-45.0f, 0.0f, 1.0f, 0.0f);

		auto drawQuad = [&](const std::string& tex, float shade,
		                    float ax, float ay, float az,
		                    float bx, float by, float bz,
		                    float cx, float cy, float cz,
		                    float dx, float dy, float dz,
		                    float v0, float v1) {
			if (!tex.empty() && bindTexture(tex)) {
				glColor4f(shade, shade, shade, 1.0f);
				glBegin(GL_QUADS);
				glTexCoord2f(0.0f, v1); glVertex3f(ax, ay, az);
				glTexCoord2f(1.0f, v1); glVertex3f(bx, by, bz);
				glTexCoord2f(1.0f, v0); glVertex3f(cx, cy, cz);
				glTexCoord2f(0.0f, v0); glVertex3f(dx, dy, dz);
				glEnd();
			}
		};

		if (blockInfo.isTransparent) {
			// Back faces first for transparent blocks like glass
			drawQuad(blockInfo.topTex, 0.5f, -0.5f, y0, -0.5f, 0.5f, y0, -0.5f, 0.5f, y0, 0.5f, -0.5f, y0, 0.5f, 0.0f, 1.0f);
			drawQuad(blockInfo.frontTex, 0.6f, 0.5f, y0, -0.5f, -0.5f, y0, -0.5f, -0.5f, y1, -0.5f, 0.5f, y1, -0.5f, vTop, 1.0f);
			drawQuad(blockInfo.sideTex, 0.6f, -0.5f, y0, -0.5f, -0.5f, y0, 0.5f, -0.5f, y1, 0.5f, -0.5f, y1, -0.5f, vTop, 1.0f);
		}

		// Top (+Y)
		drawQuad(blockInfo.topTex, 1.0f, -0.5f, y1, 0.5f, 0.5f, y1, 0.5f, 0.5f, y1, -0.5f, -0.5f, y1, -0.5f, 0.0f, 1.0f);
		// Front (+Z, faces front-left)
		drawQuad(blockInfo.frontTex, 0.8f, -0.5f, y0, 0.5f, 0.5f, y0, 0.5f, 0.5f, y1, 0.5f, -0.5f, y1, 0.5f, vTop, 1.0f);
		// Side (+X, faces front-right)
		drawQuad(blockInfo.sideTex, 0.6f, 0.5f, y0, 0.5f, 0.5f, y0, -0.5f, 0.5f, y1, -0.5f, 0.5f, y1, 0.5f, vTop, 1.0f);

		glPopMatrix();

		if (!wasDepthTest) glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		if (wasCullFace) glEnable(GL_CULL_FACE);
		if (!wasBlend) glDisable(GL_BLEND);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	} else {
		std::string tex = getItemTexturePath(item);
		if (!tex.empty() && bindTexture(tex)) {
			blitScaled(x, y, 16, 16, 0, 0, 16, 16, 16, 16);
		} else {
			fill(x + 1, y + 1, x + 15, y + 15, 0xFF7FA87F);
			drawRect(x + 1, y + 1, x + 15, y + 15, 0xFF2B402B);
		}
	}

	if (drawCount && font && item.getStackSize() > 1) {
		std::string cnt = std::to_string(static_cast<int>(item.getStackSize()));
		font->drawShadow(cnt, x + 17 - font->width(cnt), y + 9, 0xFFFFFFFF);
	}
}
