#include "GuiComponent.h"
#include "Font.h"
#include "client/renderer/block/BlockGraphics.h"
#include "world/Facing.h"
#include "world/crafting/CraftingRecipes.h"
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "world/level/block/Block.h"
#include "world/phys/AABB.h"

#include <glad/glad.h>
#include "stb_image.h"

#include <algorithm>
#include <cmath>
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
	std::vector<std::string> prefixes = {
		"",
		"resource_packs/vanilla/",
		"assets/resource_packs/vanilla/",
		"../assets/resource_packs/vanilla/",
		"../../assets/resource_packs/vanilla/"
	};
	std::vector<std::string> exts = { "" };
	if (relativePath.find('.') == std::string::npos) {
		exts.push_back(".png");
		exts.push_back(".tga");
	}
	for (const auto& ext : exts) {
		std::string rel = relativePath + ext;
		for (const auto& prefix : prefixes) {
			std::string path = prefix + rel;
			if (fs::exists(path)) {
				return path;
			}
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
	blitScaledTinted(x, y, w, h, u, v, uw, vh, texW, texH, 0xFFFFFF);
}

void GuiComponent::blitScaledTinted(int x, int y, int w, int h, int u, int v, int uw, int vh, int texW, int texH, uint32_t rgbColor) const {
	float u0 = static_cast<float>(u) / static_cast<float>(texW);
	float v0 = static_cast<float>(v) / static_cast<float>(texH);
	float u1 = static_cast<float>(u + uw) / static_cast<float>(texW);
	float v1 = static_cast<float>(v + vh) / static_cast<float>(texH);

	float r = static_cast<float>((rgbColor >> 16) & 0xFF) / 255.0f;
	float g = static_cast<float>((rgbColor >> 8) & 0xFF) / 255.0f;
	float b = static_cast<float>(rgbColor & 0xFF) / 255.0f;

	glEnable(GL_TEXTURE_2D);
	glColor4f(r, g, b, 1.0f);
	glBegin(GL_QUADS);
	glTexCoord2f(u0, v0); glVertex3f(static_cast<float>(x), static_cast<float>(y), zLayer);
	glTexCoord2f(u0, v1); glVertex3f(static_cast<float>(x), static_cast<float>(y + h), zLayer);
	glTexCoord2f(u1, v1); glVertex3f(static_cast<float>(x + w), static_cast<float>(y + h), zLayer);
	glTexCoord2f(u1, v0); glVertex3f(static_cast<float>(x + w), static_cast<float>(y), zLayer);
	glEnd();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
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

void GuiComponent::drawString(Font* font, const std::string& text, int x, int y, uint32_t color, bool shadow) const {
	if (font) {
		font->draw(text, x, y, color, shadow);
	}
}

void GuiComponent::drawCenteredString(Font* font, const std::string& text, int x, int y, uint32_t color, bool shadow) const {
	if (font) {
		int w = font->width(text);
		font->draw(text, x - w / 2, y, color, shadow);
	}
}

namespace {
	bool canRenderGuiBlockShape(BlockShape shape) {
		switch (shape) {
			case BlockShape::BLOCK:
			case BlockShape::STAIRS:
			case BlockShape::FENCE:
			case BlockShape::CACTUS:
			case BlockShape::FENCE_GATE:
			case BlockShape::CHEST:
			case BlockShape::TREE:
			case BlockShape::WALL:
			case BlockShape::BLOCK_HALF:
			case BlockShape::TOP_SNOW:
			case BlockShape::PISTON:
			case BlockShape::PORTAL_FRAME:
			case BlockShape::ANVIL:
				return true;
			default:
				return false;
		}
	}

	std::string getGuiBlockFaceTexture(const ItemInstance& item, const Block* block, const BlockGraphics* bg, FacingID face, bool carried) {
		int aux = static_cast<int>(item.getAuxValue());
		int variant = block ? block->getVariant(aux) : aux;

		if (block && Block::mGrass && block == Block::mGrass && carried) {
			if (face == Facing::UP) return "textures/blocks/grass_carried.png";
			if (face == Facing::DOWN) return "textures/blocks/dirt.png";
			return "textures/blocks/grass_side_carried.png";
		}
		if (block && Block::mLitFurnace && block == Block::mLitFurnace && face == Facing::SOUTH) {
			return "textures/blocks/furnace_front_on.png";
		}
		if (bg) {
			const TextureUVCoordinateSet& uv = carried ? bg->getCarriedTexture(face, variant) : bg->getTexture(face, variant);
			if (!uv.sourceFileLocation.mPath.empty()) {
				return uv.sourceFileLocation.mPath;
			}
		}
		return getItemTexturePath(item);
	}

	uint32_t getGuiItemTintColor(const ItemInstance& item) {
		int id = item.getId();
		int aux = static_cast<int>(item.getAuxValue());

		if (Block::mWaterlily && id == Block::mWaterlily->getId()) {
			return static_cast<uint32_t>(Block::mWaterlily->getColor(aux)) & 0xFFFFFFu;
		}
		auto eqItem = [&](const Item* it) { return it && id == it->getId(); };
		if (eqItem(Item::mHelmet_cloth) || eqItem(Item::mChestplate_cloth) ||
		    eqItem(Item::mLeggings_cloth) || eqItem(Item::mBoots_cloth)) {
			return 0xA06540u;
		}
		return 0xFFFFFFu;
	}

	float clamp01(float v) {
		return std::max(0.0f, std::min(1.0f, v));
	}

	void drawGuiQuad(const std::string& tex, float shade, float tr, float tg, float tb,
	                 float ax, float ay, float az, float au, float av,
	                 float bx, float by, float bz, float bu, float bv,
	                 float cx, float cy, float cz, float cu, float cv,
	                 float dx, float dy, float dz, float du, float dv) {
		if (tex.empty()) return;
		GuiTextureInfo info = GuiComponent::loadTexture(tex);
		if (info.id == 0) return;

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, info.id);

		float vScale = (info.width > 0 && info.height > info.width)
			? (static_cast<float>(info.width) / static_cast<float>(info.height))
			: 1.0f;

		glColor4f(shade * tr, shade * tg, shade * tb, 1.0f);
		glBegin(GL_QUADS);
		glTexCoord2f(au, av * vScale); glVertex3f(ax, ay, az);
		glTexCoord2f(bu, bv * vScale); glVertex3f(bx, by, bz);
		glTexCoord2f(cu, cv * vScale); glVertex3f(cx, cy, cz);
		glTexCoord2f(du, dv * vScale); glVertex3f(dx, dy, dz);
		glEnd();
	}

	void renderGuiBoxFace(FacingID face, const std::string& tex, float shade, float tr, float tg, float tb,
	                      float x0, float y0, float z0, float x1, float y1, float z1,
	                      float ox = 0.0f, float oy = 0.0f, float oz = 0.0f) {
		float ax0 = x0 + ox, ay0 = y0 + oy, az0 = z0 + oz;
		float ax1 = x1 + ox, ay1 = y1 + oy, az1 = z1 + oz;

		switch (face) {
			case Facing::UP: {
				float u0 = clamp01(x0), u1 = clamp01(x1);
				float v0 = clamp01(z0), v1 = clamp01(z1);
				drawGuiQuad(tex, shade, tr, tg, tb,
				            ax0, ay1, az1, u0, v1,
				            ax1, ay1, az1, u1, v1,
				            ax1, ay1, az0, u1, v0,
				            ax0, ay1, az0, u0, v0);
				break;
			}
			case Facing::DOWN: {
				float u0 = clamp01(x0), u1 = clamp01(x1);
				float v0 = clamp01(z0), v1 = clamp01(z1);
				drawGuiQuad(tex, shade, tr, tg, tb,
				            ax0, ay0, az0, u0, v0,
				            ax1, ay0, az0, u1, v0,
				            ax1, ay0, az1, u1, v1,
				            ax0, ay0, az1, u0, v1);
				break;
			}
			case Facing::NORTH: {
				float u0 = clamp01(1.0f - x1), u1 = clamp01(1.0f - x0);
				float v0 = clamp01(1.0f - y1), v1 = clamp01(1.0f - y0);
				drawGuiQuad(tex, shade, tr, tg, tb,
				            ax1, ay0, az0, u0, v1,
				            ax0, ay0, az0, u1, v1,
				            ax0, ay1, az0, u1, v0,
				            ax1, ay1, az0, u0, v0);
				break;
			}
			case Facing::SOUTH: {
				float u0 = clamp01(x0), u1 = clamp01(x1);
				float v0 = clamp01(1.0f - y1), v1 = clamp01(1.0f - y0);
				drawGuiQuad(tex, shade, tr, tg, tb,
				            ax0, ay0, az1, u0, v1,
				            ax1, ay0, az1, u1, v1,
				            ax1, ay1, az1, u1, v0,
				            ax0, ay1, az1, u0, v0);
				break;
			}
			case Facing::WEST: {
				float u0 = clamp01(z0), u1 = clamp01(z1);
				float v0 = clamp01(1.0f - y1), v1 = clamp01(1.0f - y0);
				drawGuiQuad(tex, shade, tr, tg, tb,
				            ax0, ay0, az0, u0, v1,
				            ax0, ay0, az1, u1, v1,
				            ax0, ay1, az1, u1, v0,
				            ax0, ay1, az0, u0, v0);
				break;
			}
			case Facing::EAST: {
				float u0 = clamp01(1.0f - z1), u1 = clamp01(1.0f - z0);
				float v0 = clamp01(1.0f - y1), v1 = clamp01(1.0f - y0);
				drawGuiQuad(tex, shade, tr, tg, tb,
				            ax1, ay0, az1, u0, v1,
				            ax1, ay0, az0, u1, v1,
				            ax1, ay1, az0, u1, v0,
				            ax1, ay1, az1, u0, v0);
				break;
			}
			default:
				break;
		}
	}

	void renderGuiBoxAllFaces(const ItemInstance& item, const Block* block, const BlockGraphics* bg,
	                          float tr, float tg, float tb,
	                          float x0, float y0, float z0, float x1, float y1, float z1,
	                          bool carried, float shadeScale = 1.0f,
	                          float upShade = 1.0f, float downShade = 1.0f,
	                          float nsShade = 0.5f, float weShade = 0.73f) {
		renderGuiBoxFace(Facing::DOWN,  getGuiBlockFaceTexture(item, block, bg, Facing::DOWN, carried),  downShade * shadeScale, tr, tg, tb, x0, y0, z0, x1, y1, z1);
		renderGuiBoxFace(Facing::UP,    getGuiBlockFaceTexture(item, block, bg, Facing::UP, carried),    upShade * shadeScale,   tr, tg, tb, x0, y0, z0, x1, y1, z1);
		renderGuiBoxFace(Facing::NORTH, getGuiBlockFaceTexture(item, block, bg, Facing::NORTH, carried), nsShade * shadeScale,   tr, tg, tb, x0, y0, z0, x1, y1, z1);
		renderGuiBoxFace(Facing::SOUTH, getGuiBlockFaceTexture(item, block, bg, Facing::SOUTH, carried), nsShade * shadeScale,   tr, tg, tb, x0, y0, z0, x1, y1, z1);
		renderGuiBoxFace(Facing::WEST,  getGuiBlockFaceTexture(item, block, bg, Facing::WEST, carried),  weShade * shadeScale,   tr, tg, tb, x0, y0, z0, x1, y1, z1);
		renderGuiBoxFace(Facing::EAST,  getGuiBlockFaceTexture(item, block, bg, Facing::EAST, carried),  weShade * shadeScale,   tr, tg, tb, x0, y0, z0, x1, y1, z1);
	}
}

bool GuiComponent::isBlock3D(const ItemInstance& item) {
	if (item.isNull()) return false;
	int id = item.getId();
	if (id <= 0 || id >= 256 || !Block::mBlocks[id]) return false;
	const BlockGraphics* bg = BlockGraphics::mBlocks[id];
	BlockShape shape = bg ? bg->getBlockShape() : BlockShape::BLOCK;
	return canRenderGuiBlockShape(shape);
}

namespace {
	float gCompPlayerX = 0.0f, gCompPlayerZ = 0.0f, gCompPlayerYaw = 0.0f;
	float gCompSpawnX = 0.0f, gCompSpawnZ = 0.0f;
	double gClockWorldTime = 0.0;
}

void GuiComponent::setCompassClockState(float playerX, float playerZ, float playerYaw, float spawnX, float spawnZ, double worldTime) {
	gCompPlayerX = playerX;
	gCompPlayerZ = playerZ;
	gCompPlayerYaw = playerYaw;
	gCompSpawnX = spawnX;
	gCompSpawnZ = spawnZ;
	gClockWorldTime = worldTime;
}

int GuiComponent::getCompassFrame() {
	float dx = gCompSpawnX - gCompPlayerX;
	float dz = gCompSpawnZ - gCompPlayerZ;
	float deg = std::atan2(dz, dx) * 57.2957795f - gCompPlayerYaw + 90.0f;
	deg = std::fmod(deg, 360.0f);
	if (deg < 0.0f) deg += 360.0f;
	return static_cast<int>((deg / 360.0f) * 32.0f) & 31;
}

int GuiComponent::getClockFrame() {
	long long wt = static_cast<long long>(gClockWorldTime) % 24000;
	if (wt < 0) wt += 24000;
	float v3 = static_cast<float>(wt) / 24000.0f - 0.25f;
	if (v3 < 0.0f) v3 += 1.0f;
	if (v3 > 1.0f) v3 -= 1.0f;
	float v4 = 1.0f - (std::cos(v3 * 3.14159265f) + 1.0f) * 0.5f;
	float timeOfDay = v3 + (v4 - v3) / 3.0f;
	return static_cast<int>(timeOfDay * 64.0f) & 63;
}

void GuiComponent::getItemSpritePathAndTint(const ItemInstance& item, std::string& outTex, uint32_t& outTint) {
	outTex = getItemTexturePath(item);
	outTint = getGuiItemTintColor(item);
	if (item.isNull()) return;

	int id = item.getId();
	const Block* block = (id > 0 && id < 256) ? Block::mBlocks[id] : nullptr;
	const BlockGraphics* bg = (id > 0 && id < 256) ? BlockGraphics::mBlocks[id] : nullptr;
	BlockShape shape = bg ? bg->getBlockShape() : BlockShape::BLOCK;

	if (block) {
		if (Block::mWaterlily && id == Block::mWaterlily->getId()) {
			outTex = "textures/blocks/waterlily.png";
		} else if (shape != BlockShape::RAIL &&
		           outTex.rfind("textures/items/", 0) != 0 &&
		           outTex.find("_carried") == std::string::npos &&
		           bg) {
			if (Block::mGlassPane && id == Block::mGlassPane->getId()) {
				outTex = "textures/blocks/glass.png";
			} else {
				std::string carried = getGuiBlockFaceTexture(item, block, bg, Facing::UP, true);
				if (!carried.empty()) {
					outTex = carried;
				}
			}
		}
	}
}

void GuiComponent::drawBlock3DModel(const ItemInstance& item, float shade, bool isGui) {
	if (item.isNull()) return;
	int id = item.getId();
	int aux = static_cast<int>(item.getAuxValue());
	const Block* block = (id > 0 && id < 256) ? Block::mBlocks[id] : nullptr;
	if (!block) return;
	const BlockGraphics* bg = BlockGraphics::mBlocks[id];
	BlockShape shape = bg ? bg->getBlockShape() : BlockShape::BLOCK;

	bool isTransparent = (Block::mGlass && id == Block::mGlass->getId()) ||
	                     (Block::mIce && id == Block::mIce->getId()) ||
	                     (Block::mLeaves && id == Block::mLeaves->getId()) ||
	                     (Block::mLeaves2 && id == Block::mLeaves2->getId());

	float upShade = 1.0f;
	float downShade = isGui ? 1.0f : 0.55f;
	float nsShade = isGui ? 0.5f : 0.85f;
	float weShade = isGui ? 0.73f : 0.70f;
	float tr = 1.0f, tg = 1.0f, tb = 1.0f;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(-0.5f, -0.5f, -0.5f);

	if (shape == BlockShape::CACTUS) {
		// MCPE 0.8.1 TileRenderer::renderGuiTile case 13
		renderGuiBoxFace(Facing::DOWN,  getGuiBlockFaceTexture(item, block, bg, Facing::DOWN, false),  downShade * shade, tr, tg, tb, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
		renderGuiBoxFace(Facing::UP,    getGuiBlockFaceTexture(item, block, bg, Facing::UP, false),    upShade * shade,   tr, tg, tb, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
		renderGuiBoxFace(Facing::NORTH, getGuiBlockFaceTexture(item, block, bg, Facing::NORTH, false), nsShade * shade,   tr, tg, tb, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,  0.0625f);
		renderGuiBoxFace(Facing::SOUTH, getGuiBlockFaceTexture(item, block, bg, Facing::SOUTH, false), nsShade * shade,   tr, tg, tb, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, -0.0625f);
		renderGuiBoxFace(Facing::WEST,  getGuiBlockFaceTexture(item, block, bg, Facing::WEST, false),  weShade * shade,   tr, tg, tb, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  0.0625f, 0.0f, 0.0f);
		renderGuiBoxFace(Facing::EAST,  getGuiBlockFaceTexture(item, block, bg, Facing::EAST, false),  weShade * shade,   tr, tg, tb, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, -0.0625f, 0.0f, 0.0f);
	} else if (shape == BlockShape::STAIRS) {
		// MCPE 0.8.1 TileRenderer::renderGuiTile case 10
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, false, shade, upShade, downShade, nsShade, weShade);
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, 0.0f, 0.0f, 0.5f, 1.0f, 0.5f, 1.0f, false, shade, upShade, downShade, nsShade, weShade);
	} else if (shape == BlockShape::FENCE) {
		// MCPE 0.8.1 TileRenderer::renderGuiTile case 11
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, 0.375f,  0.0f,    0.0f,   0.625f,  1.0f,    0.25f,  false, shade, upShade, downShade, nsShade, weShade);
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, 0.375f,  0.0f,    0.75f,  0.625f,  1.0f,    1.0f,   false, shade, upShade, downShade, nsShade, weShade);
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, 0.4375f, 0.8125f, -0.125f, 0.5625f, 0.9375f, 1.125f, false, shade, upShade, downShade, nsShade, weShade);
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, 0.4375f, 0.3125f, -0.125f, 0.5625f, 0.4375f, 1.125f, false, shade, upShade, downShade, nsShade, weShade);
	} else if (shape == BlockShape::FENCE_GATE) {
		// MCPE 0.8.1 TileRenderer::renderGuiTile case 21
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, 0.4375f, 0.3f, 0.0f,   0.5625f, 1.0f,    0.125f, false, shade, upShade, downShade, nsShade, weShade);
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, 0.4375f, 0.3f, 0.875f, 0.5625f, 1.0f,    1.0f,   false, shade, upShade, downShade, nsShade, weShade);
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, 0.4375f, 0.5f, 0.0f,   0.5625f, 0.9375f, 1.0f,   false, shade, upShade, downShade, nsShade, weShade);
	} else if (shape == BlockShape::WALL) {
		// MCPE 0.8.1 TileRenderer::renderGuiTile case 32
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb,  0.75f, 0.0f, 0.25f,   1.25f, 1.0f,    0.75f,   false, shade, upShade, downShade, nsShade, weShade);
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb,  0.5f,  0.0f, 0.3125f, 0.75f, 0.8125f, 0.6875f, false, shade, upShade, downShade, nsShade, weShade);
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb,  0.25f, 0.0f, 0.3125f, 0.5f,  0.8125f, 0.6875f, false, shade, upShade, downShade, nsShade, weShade);
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, -0.25f, 0.0f, 0.25f,   0.25f, 1.0f,    0.75f,   false, shade, upShade, downShade, nsShade, weShade);
	} else if (shape == BlockShape::ANVIL) {
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, 0.125f, 0.0f,   0.125f, 0.875f, 0.25f, 0.875f, false, shade, upShade, downShade, nsShade, weShade);
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, 0.25f,  0.25f,  0.1875f, 0.75f,  0.625f, 0.8125f, false, shade, upShade, downShade, nsShade, weShade);
		renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, 0.0f,   0.625f, 0.1875f, 1.0f,   1.0f,   0.8125f, false, shade, upShade, downShade, nsShade, weShade);
	} else {
		// MCPE 0.8.1 TileRenderer::renderGuiTile standard block / tree / chest / slab / carpet / snow
		AABB shapeBox(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
		shapeBox = block->getVisualShape(static_cast<DataID>(aux), shapeBox, false);
		float bx0 = shapeBox.min.x, by0 = shapeBox.min.y, bz0 = shapeBox.min.z;
		float bx1 = shapeBox.max.x, by1 = shapeBox.max.y, bz1 = shapeBox.max.z;

		if (isGui) {
			if (isTransparent) {
				renderGuiBoxFace(Facing::DOWN,  getGuiBlockFaceTexture(item, block, bg, Facing::DOWN, true),  0.5f * shade,  tr, tg, tb, bx0, by0, bz0, bx1, by1, bz1);
				renderGuiBoxFace(Facing::NORTH, getGuiBlockFaceTexture(item, block, bg, Facing::NORTH, true), 0.5f * shade,  tr, tg, tb, bx0, by0, bz0, bx1, by1, bz1);
				renderGuiBoxFace(Facing::EAST,  getGuiBlockFaceTexture(item, block, bg, Facing::EAST, true),  0.73f * shade, tr, tg, tb, bx0, by0, bz0, bx1, by1, bz1);
			}
			renderGuiBoxFace(Facing::UP,    getGuiBlockFaceTexture(item, block, bg, Facing::UP, true),    upShade * shade, tr, tg, tb, bx0, by0, bz0, bx1, by1, bz1);
			renderGuiBoxFace(Facing::SOUTH, getGuiBlockFaceTexture(item, block, bg, Facing::SOUTH, true), nsShade * shade, tr, tg, tb, bx0, by0, bz0, bx1, by1, bz1);
			renderGuiBoxFace(Facing::WEST,  getGuiBlockFaceTexture(item, block, bg, Facing::WEST, true),  weShade * shade, tr, tg, tb, bx0, by0, bz0, bx1, by1, bz1);
		} else {
			renderGuiBoxAllFaces(item, block, bg, tr, tg, tb, bx0, by0, bz0, bx1, by1, bz1, true, shade, upShade, downShade, nsShade, weShade);
		}
	}

	glPopMatrix();
}

void GuiComponent::drawItemInSlot(Font* font, const ItemInstance& item, int x, int y, bool drawCount) const {
	if (item.isNull()) return;

	if (isBlock3D(item)) {
		int id = item.getId();
		bool isTransparent = (Block::mGlass && id == Block::mGlass->getId()) ||
		                     (Block::mIce && id == Block::mIce->getId()) ||
		                     (Block::mLeaves && id == Block::mLeaves->getId()) ||
		                     (Block::mLeaves2 && id == Block::mLeaves2->getId());

		GLboolean wasDepthTest = glIsEnabled(GL_DEPTH_TEST);
		GLboolean wasCullFace = glIsEnabled(GL_CULL_FACE);
		GLboolean wasBlend = glIsEnabled(GL_BLEND);
		GLboolean wasAlphaTest = glIsEnabled(GL_ALPHA_TEST);

		glEnable(GL_DEPTH_TEST);
		glDepthMask(isTransparent ? GL_FALSE : GL_TRUE);
		glClear(GL_DEPTH_BUFFER_BIT);
		glDisable(GL_CULL_FACE);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.1f);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glTranslatef(static_cast<float>(x) + 8.0f, static_cast<float>(y) + 8.0f, 100.0f);
		glScalef(9.0f, -9.0f, 9.0f);
		glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
		glRotatef(45.0f, 0.0f, 1.0f, 0.0f);

		drawBlock3DModel(item, 1.0f, true);

		glPopMatrix();

		if (!wasDepthTest) glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		if (wasCullFace) glEnable(GL_CULL_FACE);
		if (!wasBlend) glDisable(GL_BLEND);
		if (!wasAlphaTest) glDisable(GL_ALPHA_TEST);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	} else {
		std::string tex;
		uint32_t tint = 0xFFFFFFu;
		getItemSpritePathAndTint(item, tex, tint);
		int srcFrame = 0;
		if (Item::mCompass && item.getId() == Item::mCompass->getId()) {
			tex = "textures/items/compass_atlas.png";
			srcFrame = getCompassFrame();
		} else if (Item::mClock && item.getId() == Item::mClock->getId()) {
			tex = "textures/items/watch_atlas.png";
			srcFrame = getClockFrame();
		}

		GuiTextureInfo info = !tex.empty() ? loadTexture(tex) : GuiTextureInfo{};
		if (info.id != 0) {
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, info.id);
			int frameW = info.width > 0 ? info.width : 16;
			int frameH = (info.width > 0 && info.height > info.width) ? info.width : (info.height > 0 ? info.height : 16);
			int texW = info.width > 0 ? info.width : 16;
			int texH = info.height > 0 ? info.height : 16;
			int srcY = srcFrame * frameH;
			if (srcY + frameH > texH) srcY = 0;
			blitScaledTinted(x, y, 16, 16, 0, srcY, frameW, frameH, texW, texH, tint);
		} else {
			fill(x + 1, y + 1, x + 15, y + 15, 0xFF7FA87F);
			drawRect(x + 1, y + 1, x + 15, y + 15, 0xFF2B402B);
		}
	}

	int maxDur = getItemMaxDurability(item);
	int dmg = static_cast<int>(item.getAuxValue());
	if (maxDur > 0 && dmg > 0) {
		float frac = std::max(0.0f, std::min(1.0f, static_cast<float>(maxDur - dmg) / static_cast<float>(maxDur)));
		int barW = static_cast<int>(std::round(frac * 13.0f));
		uint32_t barColor = (frac > 0.5f) ? 0xFF33EE33 : ((frac > 0.2f) ? 0xFFEECC22 : 0xFFEE2222);
		fill(x + 2, y + 13, x + 15, y + 15, 0xFF000000);
		if (barW > 0) {
			fill(x + 2, y + 13, x + 2 + barW, y + 14, barColor);
		}
	}

	if (drawCount && font && item.getStackSize() > 1) {
		std::string cnt = std::to_string(static_cast<int>(item.getStackSize()));
		font->drawShadow(cnt, x + 17 - font->width(cnt), y + 9, 0xFFFFFFFF);
	}
}
