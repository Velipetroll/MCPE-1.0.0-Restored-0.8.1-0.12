#pragma once

#include "world/item/ItemCategory.h"
#include "world/item/ItemInstance.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

class CraftingContainer {
public:
	CraftingContainer(int width, int height);

	int getWidth() const { return mWidth; }
	int getHeight() const { return mHeight; }
	int getContainerSize() const { return static_cast<int>(mItems.size()); }

	ItemInstance* getItem(int x, int y);
	const ItemInstance* getItem(int x, int y) const;

	ItemInstance& at(int index) { return mItems[index]; }
	const ItemInstance& at(int index) const { return mItems[index]; }

	void clear();

private:
	int mWidth = 0;
	int mHeight = 0;
	std::vector<ItemInstance> mItems;
};

class CraftingRecipe {
public:
	static constexpr int SIZE_2X2 = 2;
	static constexpr int SIZE_3X3 = 3;

	virtual ~CraftingRecipe() = default;
	virtual bool matches(const CraftingContainer& container) const = 0;
	virtual ItemInstance assemble(const CraftingContainer& container) const = 0;
	virtual const ItemInstance& getResultItem() const = 0;
	virtual int getCraftingSize() const = 0;
	virtual ItemCategory getCategory() const = 0;
	virtual bool isStonecutter() const = 0;
	virtual std::vector<ItemInstance> getCompactIngredients() const = 0;
};

class ShapedRecipe : public CraftingRecipe {
public:
	ShapedRecipe(int width, int height, std::vector<ItemInstance> ingredients, const ItemInstance& result,
	             ItemCategory category = ItemCategory::Structures, bool stonecutter = false);

	bool matches(const CraftingContainer& container) const override;
	ItemInstance assemble(const CraftingContainer& container) const override;
	const ItemInstance& getResultItem() const override { return mResult; }
	int getCraftingSize() const override { return (mWidth <= 2 && mHeight <= 2) ? SIZE_2X2 : SIZE_3X3; }
	ItemCategory getCategory() const override { return mCategory; }
	bool isStonecutter() const override { return mStonecutter; }
	std::vector<ItemInstance> getCompactIngredients() const override;

private:
	bool matchesAt(const CraftingContainer& container, int offsetX, int offsetY, bool mirror) const;

	int mWidth = 0;
	int mHeight = 0;
	std::vector<ItemInstance> mIngredients;
	ItemInstance mResult;
	ItemCategory mCategory = ItemCategory::Structures;
	bool mStonecutter = false;
};

class ShapelessRecipe : public CraftingRecipe {
public:
	ShapelessRecipe(std::vector<ItemInstance> ingredients, const ItemInstance& result,
	                ItemCategory category = ItemCategory::Structures, bool stonecutter = false);

	bool matches(const CraftingContainer& container) const override;
	ItemInstance assemble(const CraftingContainer& container) const override;
	const ItemInstance& getResultItem() const override { return mResult; }
	int getCraftingSize() const override { return (mIngredients.size() <= 4) ? SIZE_2X2 : SIZE_3X3; }
	ItemCategory getCategory() const override { return mCategory; }
	bool isStonecutter() const override { return mStonecutter; }
	std::vector<ItemInstance> getCompactIngredients() const override;

private:
	std::vector<ItemInstance> mIngredients;
	ItemInstance mResult;
	ItemCategory mCategory = ItemCategory::Structures;
	bool mStonecutter = false;
};

class Recipes {
public:
	static Recipes* getInstance();

	void init();
	ItemInstance findMatchingRecipe(const CraftingContainer& container) const;
	const std::vector<std::unique_ptr<CraftingRecipe>>& getRecipes() const { return mRecipes; }

private:
	Recipes() = default;
	void addShaped(int width, int height, std::vector<ItemInstance> ingredients, const ItemInstance& result,
	               ItemCategory category = ItemCategory::Structures, bool stonecutter = false);
	void addShapeless(std::vector<ItemInstance> ingredients, const ItemInstance& result,
	                  ItemCategory category = ItemCategory::Structures, bool stonecutter = false);

	bool mInitialized = false;
	std::vector<std::unique_ptr<CraftingRecipe>> mRecipes;
};

struct BlockRenderInfo {
	bool is3D = false;
	bool isSlab = false;
	bool isTransparent = false;
	std::string topTex;
	std::string frontTex;
	std::string sideTex;
};

enum class ToolType {
	None,
	Pickaxe,
	Axe,
	Shovel,
	Sword,
	Shears
};

struct ToolInfo {
	ToolType type = ToolType::None;
	int harvestLevel = 0; // 0=wood/gold, 1=stone, 2=iron, 3=diamond
	float speedMultiplier = 1.0f;
	int maxDurability = 0;
	int attackDamage = 1;
};

struct FoodInfo {
	bool isFood = false;
	int nutrition = 0;
	float saturation = 0.0f;
	int healHealth = 0;
	bool alwaysEdible = false;
	bool returnsBowl = false;
};

std::string getItemDisplayName(const ItemInstance& item);
std::string getItemDescription(const ItemInstance& item);
std::string getItemTexturePath(const ItemInstance& item);
BlockRenderInfo getBlockRenderInfo(const ItemInstance& item);
int getArmorSlotForItem(const ItemInstance& item);
int getArmorDefensePoints(const ItemInstance& item);
int getTotalArmorDefense(const std::array<ItemInstance, 4>& armorSlots);
int getItemMaxDurability(const ItemInstance& item);
ToolInfo getToolInfo(const ItemInstance& item);
FoodInfo getFoodInfo(const ItemInstance& item);
