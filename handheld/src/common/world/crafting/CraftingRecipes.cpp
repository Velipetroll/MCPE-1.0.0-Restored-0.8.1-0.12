#include "world/crafting/CraftingRecipes.h"
#include "world/item/Item.h"
#include "world/level/block/Block.h"
#include <unordered_map>
#include <vector>

// --- CraftingContainer ---
CraftingContainer::CraftingContainer(int width, int height)
	: mWidth(width)
	, mHeight(height)
	, mItems(static_cast<size_t>(width * height)) {
}

ItemInstance* CraftingContainer::getItem(int x, int y) {
	if (x < 0 || y < 0 || x >= mWidth || y >= mHeight) return nullptr;
	ItemInstance& item = mItems[static_cast<size_t>(y * mWidth + x)];
	return item.isNull() ? nullptr : &item;
}

const ItemInstance* CraftingContainer::getItem(int x, int y) const {
	if (x < 0 || y < 0 || x >= mWidth || y >= mHeight) return nullptr;
	const ItemInstance& item = mItems[static_cast<size_t>(y * mWidth + x)];
	return item.isNull() ? nullptr : &item;
}

void CraftingContainer::clear() {
	for (ItemInstance& item : mItems) {
		item.setNull();
	}
}

namespace {
	bool matchesIngredient(const ItemInstance* actual, const ItemInstance& expected) {
		if (!actual || actual->isNull()) {
			return expected.isNull();
		}
		if (expected.isNull()) {
			return false;
		}
		if (actual->getId() != expected.getId()) {
			return false;
		}
		int expAux = expected.getAuxValue();
		return expAux == -1 || expAux == actual->getAuxValue();
	}
	std::vector<ItemInstance> compactIngredientList(const std::vector<ItemInstance>& list) {
		std::vector<ItemInstance> out;
		for (const ItemInstance& ing : list) {
			if (ing.isNull()) continue;
			bool merged = false;
			for (ItemInstance& existing : out) {
				if (existing.getId() == ing.getId() && existing.getAuxValue() == ing.getAuxValue()) {
					existing.add(static_cast<int>(ing.getStackSize()));
					merged = true;
					break;
				}
			}
			if (!merged) {
				out.push_back(ing);
			}
		}
		return out;
	}
}

// --- ShapedRecipe ---
ShapedRecipe::ShapedRecipe(int width, int height, std::vector<ItemInstance> ingredients, const ItemInstance& result,
                           ItemCategory category, bool stonecutter)
	: mWidth(width)
	, mHeight(height)
	, mIngredients(std::move(ingredients))
	, mResult(result)
	, mCategory(category)
	, mStonecutter(stonecutter) {
}

std::vector<ItemInstance> ShapedRecipe::getCompactIngredients() const {
	return compactIngredientList(mIngredients);
}

bool ShapedRecipe::matches(const CraftingContainer& container) const {
	for (int ox = 0; ox <= 3 - mWidth; ++ox) {
		for (int oy = 0; oy <= 3 - mHeight; ++oy) {
			if (matchesAt(container, ox, oy, false) || matchesAt(container, ox, oy, true)) {
				return true;
			}
		}
	}
	return false;
}

bool ShapedRecipe::matchesAt(const CraftingContainer& container, int offsetX, int offsetY, bool mirror) const {
	for (int x = 0; x < 3; ++x) {
		for (int y = 0; y < 3; ++y) {
			int rx = x - offsetX;
			int ry = y - offsetY;
			ItemInstance expected;
			if (rx >= 0 && ry >= 0 && rx < mWidth && ry < mHeight) {
				int idx = ry * mWidth + (mirror ? (mWidth - 1 - rx) : rx);
				if (idx >= 0 && idx < static_cast<int>(mIngredients.size())) {
					expected = mIngredients[static_cast<size_t>(idx)];
				}
			}
			const ItemInstance* actual = container.getItem(x, y);
			if (!matchesIngredient(actual, expected)) {
				return false;
			}
		}
	}
	return true;
}

ItemInstance ShapedRecipe::assemble(const CraftingContainer&) const {
	return mResult;
}

// --- ShapelessRecipe ---
ShapelessRecipe::ShapelessRecipe(std::vector<ItemInstance> ingredients, const ItemInstance& result,
                                 ItemCategory category, bool stonecutter)
	: mIngredients(std::move(ingredients))
	, mResult(result)
	, mCategory(category)
	, mStonecutter(stonecutter) {
}

std::vector<ItemInstance> ShapelessRecipe::getCompactIngredients() const {
	return compactIngredientList(mIngredients);
}

bool ShapelessRecipe::matches(const CraftingContainer& container) const {
	std::vector<ItemInstance> remaining = mIngredients;
	for (int y = 0; y < 3; ++y) {
		for (int x = 0; x < 3; ++x) {
			const ItemInstance* actual = container.getItem(x, y);
			if (!actual) continue;

			bool found = false;
			for (auto it = remaining.begin(); it != remaining.end(); ++it) {
				if (matchesIngredient(actual, *it)) {
					remaining.erase(it);
					found = true;
					break;
				}
			}
			if (!found) {
				return false;
			}
		}
	}
	return remaining.empty();
}

ItemInstance ShapelessRecipe::assemble(const CraftingContainer&) const {
	return mResult;
}

// --- Recipes ---
Recipes* Recipes::getInstance() {
	static Recipes instance;
	return &instance;
}

void Recipes::addShaped(int width, int height, std::vector<ItemInstance> ingredients, const ItemInstance& result,
                        ItemCategory category, bool stonecutter) {
	mRecipes.push_back(std::make_unique<ShapedRecipe>(width, height, std::move(ingredients), result, category, stonecutter));
}

void Recipes::addShapeless(std::vector<ItemInstance> ingredients, const ItemInstance& result,
                           ItemCategory category, bool stonecutter) {
	mRecipes.push_back(std::make_unique<ShapelessRecipe>(std::move(ingredients), result, category, stonecutter));
}

namespace {
	class CraftingBlockItem : public Item {
	public:
		explicit CraftingBlockItem(const Block* block, const std::string& name)
			: Item(name, static_cast<short>(block->mID) - 256) {
			mBlockId = block->mID;
		}
	};

	std::vector<std::unique_ptr<Item>>& getOwnedCraftingItems() {
		static std::vector<std::unique_ptr<Item>> owned;
		return owned;
	}

	Item* ensureItem(Item*& target, const char* name, short idOffset, int maxStack = 64) {
		if (target) return target;
		short actualId = static_cast<short>(256 + idOffset);
		if (actualId >= 0 && actualId < Item::MAX_ITEMS && Item::mItems[actualId]) {
			target = Item::mItems[actualId];
			return target;
		}
		auto item = std::make_unique<Item>(name, idOffset);
		item->setMaxStackSize(static_cast<byte>(maxStack));
		target = item.get();
		if (actualId >= 0 && actualId < Item::MAX_ITEMS) {
			Item::mItems[actualId] = target;
		}
		getOwnedCraftingItems().push_back(std::move(item));
		return target;
	}

	void ensureBlockItem(const Block* block, const char* name) {
		if (!block) return;
		int id = block->mID;
		if (id > 0 && id < Item::MAX_ITEMS && !Item::mItems[id]) {
			auto item = std::make_unique<CraftingBlockItem>(block, name);
			Item::mItems[id] = item.get();
			getOwnedCraftingItems().push_back(std::move(item));
		}
	}

	void ensureCraftingItemsRegistered() {
		if (!Block::mStone) {
			Block::initBlocks();
		}
		if (!Item::mApple) {
			Item::registerItems();
		}

		// Register items that are commented out in Item::registerItems()
		ensureItem(Item::mCoal, "coal", 7);
		ensureItem(Item::mDiamond, "diamond", 8);
		ensureItem(Item::mIronIngot, "iron_ingot", 9);
		ensureItem(Item::mGoldIngot, "gold_ingot", 10);
		ensureItem(Item::mStick, "stick", 24);
		ensureItem(Item::mWheat, "wheat", 40);

		ensureItem(Item::mShovel_iron, "iron_shovel", 0, 1);
		ensureItem(Item::mPickAxe_iron, "iron_pickaxe", 1, 1);
		ensureItem(Item::mHatchet_iron, "iron_axe", 2, 1);
		ensureItem(Item::mSword_iron, "iron_sword", 11, 1);

		ensureItem(Item::mSword_wood, "wooden_sword", 12, 1);
		ensureItem(Item::mShovel_wood, "wooden_shovel", 13, 1);
		ensureItem(Item::mPickAxe_wood, "wooden_pickaxe", 14, 1);
		ensureItem(Item::mHatchet_wood, "wooden_axe", 15, 1);

		ensureItem(Item::mSword_stone, "stone_sword", 16, 1);
		ensureItem(Item::mShovel_stone, "stone_shovel", 17, 1);
		ensureItem(Item::mPickAxe_stone, "stone_pickaxe", 18, 1);
		ensureItem(Item::mHatchet_stone, "stone_axe", 19, 1);

		ensureItem(Item::mSword_diamond, "diamond_sword", 20, 1);
		ensureItem(Item::mShovel_diamond, "diamond_shovel", 21, 1);
		ensureItem(Item::mPickAxe_diamond, "diamond_pickaxe", 22, 1);
		ensureItem(Item::mHatchet_diamond, "diamond_axe", 23, 1);

		ensureItem(Item::mHelmet_iron, "iron_helmet", 50, 1);
		ensureItem(Item::mChestplate_iron, "iron_chestplate", 51, 1);
		ensureItem(Item::mLeggings_iron, "iron_leggings", 52, 1);
		ensureItem(Item::mBoots_iron, "iron_boots", 53, 1);

		// Register BlockItems for blocks used in crafting & inventory
		ensureBlockItem(Block::mStone, "stone");
		ensureBlockItem(Block::mCobblestone, "cobblestone");
		ensureBlockItem(Block::mWoodPlanks, "planks");
		ensureBlockItem(Block::mLog, "log");
		ensureBlockItem(Block::mSand, "sand");
		ensureBlockItem(Block::mGlass, "glass");
		ensureBlockItem(Block::mSandStone, "sandstone");
		ensureBlockItem(Block::mTorch, "torch");
		ensureBlockItem(Block::mChest, "chest");
		ensureBlockItem(Block::mWorkBench, "crafting_table");
		ensureBlockItem(Block::mFurnace, "furnace");
		ensureBlockItem(Block::mStoneBrick, "stonebrick");
		ensureBlockItem(Block::mGlassPane, "glass_pane");
		ensureBlockItem(Block::mWoodenSlab, "wooden_slab");
		ensureBlockItem(Block::mCobblestoneWall, "cobblestone_wall");
		ensureBlockItem(Block::mStoneSlab, "stone_slab");
		ensureBlockItem(Block::mStonecutterBench, "stonecutter");
	}
}

void Recipes::init() {
	if (mInitialized) return;
	mInitialized = true;

	ensureCraftingItemsRegistered();

	ItemInstance empty;
	ItemInstance logAny(Block::mLog, 1, -1);
	ItemInstance planks(Block::mWoodPlanks, 1, 0);
	ItemInstance planksAny(Block::mWoodPlanks, 1, -1);
	ItemInstance stick(Item::mStick, 1, 0);
	ItemInstance cobble(Block::mCobblestone, 1, 0);
	ItemInstance stone(Block::mStone, 1, 0);
	ItemInstance coal(Item::mCoal, 1, -1);
	ItemInstance iron(Item::mIronIngot, 1, 0);
	ItemInstance diamond(Item::mDiamond, 1, 0);
	ItemInstance wheat(Item::mWheat, 1, 0);
	ItemInstance sand(Block::mSand, 1, 0);
	ItemInstance glass(Block::mGlass, 1, 0);

	// Planks from Log (shapeless)
	addShapeless({ logAny }, ItemInstance(Block::mWoodPlanks, 4, 0), ItemCategory::Structures);

	// Sticks (1x2)
	addShaped(1, 2, { planksAny, planksAny }, ItemInstance(Item::mStick, 4, 0), ItemCategory::Structures);

	// Crafting Table (2x2)
	addShaped(2, 2, { planksAny, planksAny, planksAny, planksAny }, ItemInstance(Block::mWorkBench, 1, 0), ItemCategory::Structures);

	// Torches (1x2)
	addShaped(1, 2, { coal, stick }, ItemInstance(Block::mTorch, 4, 0), ItemCategory::Decorations);

	// Chest (3x3)
	addShaped(3, 3, {
		planksAny, planksAny, planksAny,
		planksAny, empty,     planksAny,
		planksAny, planksAny, planksAny
	}, ItemInstance(Block::mChest, 1, 0), ItemCategory::Structures);

	// Furnace (3x3)
	addShaped(3, 3, {
		cobble, cobble, cobble,
		cobble, empty,  cobble,
		cobble, cobble, cobble
	}, ItemInstance(Block::mFurnace, 1, 0), ItemCategory::Structures);

	// Stonecutter (2x2)
	addShaped(2, 2, { cobble, cobble, cobble, cobble }, ItemInstance(Block::mStonecutterBench, 1, 0), ItemCategory::Structures);

	// Tools & Weapons helper
	auto addToolSet = [&](const ItemInstance& mat, const Item* pick, const Item* axe, const Item* shovel, const Item* sword) {
		if (pick) {
			addShaped(3, 3, {
				mat,   mat,   mat,
				empty, stick, empty,
				empty, stick, empty
			}, ItemInstance(pick, 1, 0), ItemCategory::Tools);
		}
		if (axe) {
			addShaped(2, 3, {
				mat,   mat,
				mat,   stick,
				empty, stick
			}, ItemInstance(axe, 1, 0), ItemCategory::Tools);
		}
		if (shovel) {
			addShaped(1, 3, { mat, stick, stick }, ItemInstance(shovel, 1, 0), ItemCategory::Tools);
		}
		if (sword) {
			addShaped(1, 3, { mat, mat, stick }, ItemInstance(sword, 1, 0), ItemCategory::Tools);
		}
	};

	addToolSet(planksAny, Item::mPickAxe_wood, Item::mHatchet_wood, Item::mShovel_wood, Item::mSword_wood);
	addToolSet(cobble, Item::mPickAxe_stone, Item::mHatchet_stone, Item::mShovel_stone, Item::mSword_stone);
	addToolSet(iron, Item::mPickAxe_iron, Item::mHatchet_iron, Item::mShovel_iron, Item::mSword_iron);
	addToolSet(diamond, Item::mPickAxe_diamond, Item::mHatchet_diamond, Item::mShovel_diamond, Item::mSword_diamond);

	// Iron Armor
	addShaped(3, 2, {
		iron, iron,  iron,
		iron, empty, iron
	}, ItemInstance(Item::mHelmet_iron, 1, 0), ItemCategory::Tools);
	addShaped(3, 3, {
		iron, empty, iron,
		iron, iron,  iron,
		iron, iron,  iron
	}, ItemInstance(Item::mChestplate_iron, 1, 0), ItemCategory::Tools);
	addShaped(3, 3, {
		iron, iron,  iron,
		iron, empty, iron,
		iron, empty, iron
	}, ItemInstance(Item::mLeggings_iron, 1, 0), ItemCategory::Tools);
	addShaped(3, 2, {
		iron, empty, iron,
		iron, empty, iron
	}, ItemInstance(Item::mBoots_iron, 1, 0), ItemCategory::Tools);

	// Bread (3x1)
	addShaped(3, 1, { wheat, wheat, wheat }, ItemInstance(Item::mBread, 1, 0), ItemCategory::FoodArmor);

	// Decorative & Stonecutter Blocks
	addShaped(2, 2, { stone, stone, stone, stone }, ItemInstance(Block::mStoneBrick, 4, 0), ItemCategory::Structures, true);
	addShaped(2, 2, { sand, sand, sand, sand }, ItemInstance(Block::mSandStone, 1, 0), ItemCategory::Structures, true);
	addShaped(3, 2, { glass, glass, glass, glass, glass, glass }, ItemInstance(Block::mGlassPane, 16, 0), ItemCategory::Decorations, false);
	addShaped(3, 1, { planksAny, planksAny, planksAny }, ItemInstance(Block::mWoodenSlab, 6, 0), ItemCategory::Structures, false);
	addShaped(3, 1, { cobble, cobble, cobble }, ItemInstance(Block::mStoneSlab, 6, 3), ItemCategory::Structures, true);
	addShaped(3, 2, { cobble, cobble, cobble, cobble, cobble, cobble }, ItemInstance(Block::mCobblestoneWall, 6, 0), ItemCategory::Structures, true);
}

ItemInstance Recipes::findMatchingRecipe(const CraftingContainer& container) const {
	for (const auto& recipe : mRecipes) {
		if (recipe && recipe->matches(container)) {
			return recipe->assemble(container);
		}
	}
	return ItemInstance();
}

namespace {
	struct ItemVisualInfo {
		const char* name;
		const char* texture;
	};

	const std::unordered_map<int, ItemVisualInfo>& getItemVisualMap() {
		static std::unordered_map<int, ItemVisualInfo> map;
		if (map.empty()) {
			auto regBlock = [&](const Block* b, const char* name, const char* tex) {
				if (b) map[b->getId()] = { name, tex };
			};
			auto regItem = [&](const Item* it, const char* name, const char* tex) {
				if (it) map[it->getId()] = { name, tex };
			};

			regBlock(Block::mStone, "Stone", "textures/blocks/stone.png");
			regBlock(Block::mCobblestone, "Cobblestone", "textures/blocks/cobblestone.png");
			regBlock(Block::mWoodPlanks, "Oak Planks", "textures/blocks/planks_oak.png");
			regBlock(Block::mLog, "Oak Log", "textures/blocks/log_oak.png");
			regBlock(Block::mSand, "Sand", "textures/blocks/sand.png");
			regBlock(Block::mGlass, "Glass", "textures/blocks/glass.png");
			regBlock(Block::mSandStone, "Sandstone", "textures/blocks/sandstone_normal.png");
			regBlock(Block::mTorch, "Torch", "textures/blocks/torch_on.png");
			regBlock(Block::mChest, "Chest", "textures/blocks/planks_oak.png");
			regBlock(Block::mWorkBench, "Crafting Table", "textures/blocks/crafting_table_top.png");
			regBlock(Block::mFurnace, "Furnace", "textures/blocks/furnace_front_off.png");
			regBlock(Block::mStoneBrick, "Stone Bricks", "textures/blocks/stonebrick.png");
			regBlock(Block::mGlassPane, "Glass Pane", "textures/blocks/glass.png");
			regBlock(Block::mWoodenSlab, "Oak Wood Slab", "textures/blocks/planks_oak.png");
			regBlock(Block::mStoneSlab, "Cobblestone Slab", "textures/blocks/cobblestone.png");
			regBlock(Block::mCobblestoneWall, "Cobblestone Wall", "textures/blocks/cobblestone.png");
			regBlock(Block::mStonecutterBench, "Stonecutter", "textures/blocks/stonecutter_top.png");

			regItem(Item::mStick, "Stick", "textures/items/stick.png");
			regItem(Item::mCoal, "Coal", "textures/items/coal.png");
			regItem(Item::mIronIngot, "Iron Ingot", "textures/items/iron_ingot.png");
			regItem(Item::mGoldIngot, "Gold Ingot", "textures/items/gold_ingot.png");
			regItem(Item::mDiamond, "Diamond", "textures/items/diamond.png");
			regItem(Item::mWheat, "Wheat", "textures/items/wheat.png");
			regItem(Item::mBread, "Bread", "textures/items/bread.png");

			regItem(Item::mPickAxe_wood, "Wooden Pickaxe", "textures/items/wood_pickaxe.png");
			regItem(Item::mHatchet_wood, "Wooden Axe", "textures/items/wood_axe.png");
			regItem(Item::mShovel_wood, "Wooden Shovel", "textures/items/wood_shovel.png");
			regItem(Item::mSword_wood, "Wooden Sword", "textures/items/wood_sword.png");

			regItem(Item::mPickAxe_stone, "Stone Pickaxe", "textures/items/stone_pickaxe.png");
			regItem(Item::mHatchet_stone, "Stone Axe", "textures/items/stone_axe.png");
			regItem(Item::mShovel_stone, "Stone Shovel", "textures/items/stone_shovel.png");
			regItem(Item::mSword_stone, "Stone Sword", "textures/items/stone_sword.png");

			regItem(Item::mPickAxe_iron, "Iron Pickaxe", "textures/items/iron_pickaxe.png");
			regItem(Item::mHatchet_iron, "Iron Axe", "textures/items/iron_axe.png");
			regItem(Item::mShovel_iron, "Iron Shovel", "textures/items/iron_shovel.png");
			regItem(Item::mSword_iron, "Iron Sword", "textures/items/iron_sword.png");

			regItem(Item::mPickAxe_diamond, "Diamond Pickaxe", "textures/items/diamond_pickaxe.png");
			regItem(Item::mHatchet_diamond, "Diamond Axe", "textures/items/diamond_axe.png");
			regItem(Item::mShovel_diamond, "Diamond Shovel", "textures/items/diamond_shovel.png");
			regItem(Item::mSword_diamond, "Diamond Sword", "textures/items/diamond_sword.png");

			regItem(Item::mHelmet_iron, "Iron Helmet", "textures/items/iron_helmet.png");
			regItem(Item::mChestplate_iron, "Iron Chestplate", "textures/items/iron_chestplate.png");
			regItem(Item::mLeggings_iron, "Iron Leggings", "textures/items/iron_leggings.png");
			regItem(Item::mBoots_iron, "Iron Boots", "textures/items/iron_boots.png");
		}
		return map;
	}
}

std::string getItemDisplayName(const ItemInstance& item) {
	if (item.isNull()) return "";
	const auto& map = getItemVisualMap();
	auto it = map.find(item.getId());
	if (it != map.end()) {
		return it->second.name;
	}
	return "Item #" + std::to_string(item.getId());
}

std::string getItemDescription(const ItemInstance& item) {
	if (item.isNull()) return "";
	int id = item.getId();
	if (Block::mWoodPlanks && id == Block::mWoodPlanks->getId()) return "Basic building material.";
	if (Item::mStick && id == Item::mStick->getId()) return "Used to craft tools and torches.";
	if (Block::mWorkBench && id == Block::mWorkBench->getId()) return "Unlocks 3x3 crafting recipes.";
	if (Block::mTorch && id == Block::mTorch->getId()) return "Lights up dark caves.";
	if (Block::mChest && id == Block::mChest->getId()) return "Stores items safely.";
	if (Block::mFurnace && id == Block::mFurnace->getId()) return "Smelts ores and cooks food.";
	if (Block::mStonecutterBench && id == Block::mStonecutterBench->getId()) return "Crafts stone blocks.";
	if (Block::mStoneBrick && id == Block::mStoneBrick->getId()) return "Decorative stone blocks for building.";
	if (Block::mSandStone && id == Block::mSandStone->getId()) return "Compressed sand blocks.";
	if (Block::mCobblestoneWall && id == Block::mCobblestoneWall->getId()) return "Keeps mobs out of your yard.";
	if (Block::mStoneSlab && id == Block::mStoneSlab->getId()) return "Half-height stone building block.";
	if (Block::mWoodenSlab && id == Block::mWoodenSlab->getId()) return "Half-height wooden building block.";
	if (Block::mGlassPane && id == Block::mGlassPane->getId()) return "Thin transparent window.";
	if (Item::mBread && id == Item::mBread->getId()) return "Restores hunger.";
	if (Item::mHelmet_iron && id == Item::mHelmet_iron->getId()) return "Protects your head.";
	if (Item::mChestplate_iron && id == Item::mChestplate_iron->getId()) return "Protects your chest.";
	if (Item::mLeggings_iron && id == Item::mLeggings_iron->getId()) return "Protects your legs.";
	if (Item::mBoots_iron && id == Item::mBoots_iron->getId()) return "Protects your feet.";
	return "Craftable item.";
}

std::string getItemTexturePath(const ItemInstance& item) {
	if (item.isNull()) return "";
	const auto& map = getItemVisualMap();
	auto it = map.find(item.getId());
	if (it != map.end()) {
		return it->second.texture;
	}
	return "";
}

int getArmorSlotForItem(const ItemInstance& item) {
	if (item.isNull()) return -1;
	int id = item.getId();
	if ((Item::mHelmet_cloth && id == Item::mHelmet_cloth->getId()) ||
	    (Item::mHelmet_chain && id == Item::mHelmet_chain->getId()) ||
	    (Item::mHelmet_iron && id == Item::mHelmet_iron->getId()) ||
	    (Item::mHelmet_diamond && id == Item::mHelmet_diamond->getId()) ||
	    (Item::mHelmet_gold && id == Item::mHelmet_gold->getId())) {
		return 0;
	}
	if ((Item::mChestplate_cloth && id == Item::mChestplate_cloth->getId()) ||
	    (Item::mChestplate_chain && id == Item::mChestplate_chain->getId()) ||
	    (Item::mChestplate_iron && id == Item::mChestplate_iron->getId()) ||
	    (Item::mChestplate_diamond && id == Item::mChestplate_diamond->getId()) ||
	    (Item::mChestplate_gold && id == Item::mChestplate_gold->getId())) {
		return 1;
	}
	if ((Item::mLeggings_cloth && id == Item::mLeggings_cloth->getId()) ||
	    (Item::mLeggings_chain && id == Item::mLeggings_chain->getId()) ||
	    (Item::mLeggings_iron && id == Item::mLeggings_iron->getId()) ||
	    (Item::mLeggings_diamond && id == Item::mLeggings_diamond->getId()) ||
	    (Item::mLeggings_gold && id == Item::mLeggings_gold->getId())) {
		return 2;
	}
	if ((Item::mBoots_cloth && id == Item::mBoots_cloth->getId()) ||
	    (Item::mBoots_chain && id == Item::mBoots_chain->getId()) ||
	    (Item::mBoots_iron && id == Item::mBoots_iron->getId()) ||
	    (Item::mBoots_diamond && id == Item::mBoots_diamond->getId()) ||
	    (Item::mBoots_gold && id == Item::mBoots_gold->getId())) {
		return 3;
	}
	return -1;
}
