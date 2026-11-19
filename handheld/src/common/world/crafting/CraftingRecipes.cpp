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

	Item* ensureItem(Item*& target, const char* name, short idOffset, int maxStack = 64, bool handEquipped = false, bool mirroredArt = false) {
		if (target) {
			if (handEquipped) target->setHandEquipped();
			if (mirroredArt) target->setIsMirroredArt(true);
			return target;
		}
		short actualId = static_cast<short>(256 + idOffset);
		if (actualId >= 0 && actualId < Item::MAX_ITEMS && Item::mItems[actualId]) {
			target = Item::mItems[actualId];
			if (handEquipped) target->setHandEquipped();
			if (mirroredArt) target->setIsMirroredArt(true);
			return target;
		}
		auto item = std::make_unique<Item>(name, idOffset);
		item->setMaxStackSize(static_cast<byte>(maxStack));
		if (handEquipped) item->setHandEquipped();
		if (mirroredArt) item->setIsMirroredArt(true);
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
		ensureItem(Item::mStick, "stick", 24, 64, true, false);
		ensureItem(Item::mBowl, "bowl", 25);
		ensureItem(Item::mFeather, "feather", 32);
		ensureItem(Item::mSulphur, "gunpowder", 33);
		ensureItem(Item::mWheat, "wheat", 40);
		ensureItem(Item::mFlint, "flint", 62);
		ensureItem(Item::mRedStone, "redstone", 75);
		ensureItem(Item::mLeather, "leather", 78);
		ensureItem(Item::mBrick, "brick", 80);
		ensureItem(Item::mClay, "clay_ball", 81);
		ensureItem(Item::mPaper, "paper", 83);
		ensureItem(Item::mBook, "book", 84);
		ensureItem(Item::mYellowDust, "glowstone_dust", 92);
		ensureItem(Item::mDye_powder, "dye", 95);
		ensureItem(Item::mBone, "bone", 96, 64, true, false);
		ensureItem(Item::mSugar, "sugar", 97);
		ensureItem(Item::mShears, "shears", 103, 1)->setMaxDamage(238);
		ensureItem(Item::mEmerald, "emerald", 132);
		ensureItem(Item::mNetherbrick, "netherbrick", 149);
		ensureItem(Item::mNetherQuartz, "quartz", 150);

		ensureItem(Item::mShovel_iron, "iron_shovel", 0, 1, true, false)->setMaxDamage(250);
		ensureItem(Item::mPickAxe_iron, "iron_pickaxe", 1, 1, true, false)->setMaxDamage(250);
		ensureItem(Item::mHatchet_iron, "iron_axe", 2, 1, true, false)->setMaxDamage(250);
		ensureItem(Item::mSword_iron, "iron_sword", 11, 1, true, false)->setMaxDamage(250);

		ensureItem(Item::mSword_wood, "wooden_sword", 12, 1, true, false)->setMaxDamage(59);
		ensureItem(Item::mShovel_wood, "wooden_shovel", 13, 1, true, false)->setMaxDamage(59);
		ensureItem(Item::mPickAxe_wood, "wooden_pickaxe", 14, 1, true, false)->setMaxDamage(59);
		ensureItem(Item::mHatchet_wood, "wooden_axe", 15, 1, true, false)->setMaxDamage(59);

		ensureItem(Item::mSword_stone, "stone_sword", 16, 1, true, false)->setMaxDamage(131);
		ensureItem(Item::mShovel_stone, "stone_shovel", 17, 1, true, false)->setMaxDamage(131);
		ensureItem(Item::mPickAxe_stone, "stone_pickaxe", 18, 1, true, false)->setMaxDamage(131);
		ensureItem(Item::mHatchet_stone, "stone_axe", 19, 1, true, false)->setMaxDamage(131);

		ensureItem(Item::mSword_diamond, "diamond_sword", 20, 1, true, false)->setMaxDamage(1561);
		ensureItem(Item::mShovel_diamond, "diamond_shovel", 21, 1, true, false)->setMaxDamage(1561);
		ensureItem(Item::mPickAxe_diamond, "diamond_pickaxe", 22, 1, true, false)->setMaxDamage(1561);
		ensureItem(Item::mHatchet_diamond, "diamond_axe", 23, 1, true, false)->setMaxDamage(1561);

		ensureItem(Item::mSword_gold, "golden_sword", 27, 1, true, false)->setMaxDamage(32);
		ensureItem(Item::mShovel_gold, "golden_shovel", 28, 1, true, false)->setMaxDamage(32);
		ensureItem(Item::mPickAxe_gold, "golden_pickaxe", 29, 1, true, false)->setMaxDamage(32);
		ensureItem(Item::mHatchet_gold, "golden_axe", 30, 1, true, false)->setMaxDamage(32);

		ensureItem(Item::mHelmet_cloth, "leather_helmet", 42, 1)->setMaxDamage(55);
		ensureItem(Item::mChestplate_cloth, "leather_chestplate", 43, 1)->setMaxDamage(80);
		ensureItem(Item::mLeggings_cloth, "leather_leggings", 44, 1)->setMaxDamage(75);
		ensureItem(Item::mBoots_cloth, "leather_boots", 45, 1)->setMaxDamage(65);

		ensureItem(Item::mHelmet_chain, "chainmail_helmet", 46, 1)->setMaxDamage(165);
		ensureItem(Item::mChestplate_chain, "chainmail_chestplate", 47, 1)->setMaxDamage(240);
		ensureItem(Item::mLeggings_chain, "chainmail_leggings", 48, 1)->setMaxDamage(225);
		ensureItem(Item::mBoots_chain, "chainmail_boots", 49, 1)->setMaxDamage(195);

		ensureItem(Item::mHelmet_iron, "iron_helmet", 50, 1)->setMaxDamage(165);
		ensureItem(Item::mChestplate_iron, "iron_chestplate", 51, 1)->setMaxDamage(240);
		ensureItem(Item::mLeggings_iron, "iron_leggings", 52, 1)->setMaxDamage(225);
		ensureItem(Item::mBoots_iron, "iron_boots", 53, 1)->setMaxDamage(195);

		ensureItem(Item::mHelmet_diamond, "diamond_helmet", 54, 1)->setMaxDamage(363);
		ensureItem(Item::mChestplate_diamond, "diamond_chestplate", 55, 1)->setMaxDamage(528);
		ensureItem(Item::mLeggings_diamond, "diamond_leggings", 56, 1)->setMaxDamage(495);
		ensureItem(Item::mBoots_diamond, "diamond_boots", 57, 1)->setMaxDamage(429);

		ensureItem(Item::mHelmet_gold, "golden_helmet", 58, 1)->setMaxDamage(77);
		ensureItem(Item::mChestplate_gold, "golden_chestplate", 59, 1)->setMaxDamage(112);
		ensureItem(Item::mLeggings_gold, "golden_leggings", 60, 1)->setMaxDamage(105);
		ensureItem(Item::mBoots_gold, "golden_boots", 61, 1)->setMaxDamage(91);

		// Register BlockItems for blocks used in crafting & inventory, plus all remaining blocks for drops
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

		for (int i = 1; i < 256; ++i) {
			if (Block::mBlocks[i] && !Item::mItems[i]) {
				ensureBlockItem(Block::mBlocks[i], Block::mBlocks[i]->getDescriptionId().c_str());
			}
		}
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
	ItemInstance gold(Item::mGoldIngot, 1, 0);
	addToolSet(gold, Item::mPickAxe_gold, Item::mHatchet_gold, Item::mShovel_gold, Item::mSword_gold);

	// Armor sets helper
	auto addArmorSet = [&](const ItemInstance& mat, const Item* helm, const Item* chest, const Item* legs, const Item* boots) {
		if (helm) {
			addShaped(3, 2, {
				mat, mat,   mat,
				mat, empty, mat
			}, ItemInstance(helm, 1, 0), ItemCategory::Tools);
		}
		if (chest) {
			addShaped(3, 3, {
				mat, empty, mat,
				mat, mat,   mat,
				mat, mat,   mat
			}, ItemInstance(chest, 1, 0), ItemCategory::Tools);
		}
		if (legs) {
			addShaped(3, 3, {
				mat, mat,   mat,
				mat, empty, mat,
				mat, empty, mat
			}, ItemInstance(legs, 1, 0), ItemCategory::Tools);
		}
		if (boots) {
			addShaped(3, 2, {
				mat, empty, mat,
				mat, empty, mat
			}, ItemInstance(boots, 1, 0), ItemCategory::Tools);
		}
	};

	ItemInstance leather(Item::mLeather, 1, 0);
	addArmorSet(leather, Item::mHelmet_cloth, Item::mChestplate_cloth, Item::mLeggings_cloth, Item::mBoots_cloth);
	addArmorSet(iron, Item::mHelmet_iron, Item::mChestplate_iron, Item::mLeggings_iron, Item::mBoots_iron);
	addArmorSet(gold, Item::mHelmet_gold, Item::mChestplate_gold, Item::mLeggings_gold, Item::mBoots_gold);
	addArmorSet(diamond, Item::mHelmet_diamond, Item::mChestplate_diamond, Item::mLeggings_diamond, Item::mBoots_diamond);

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
			Recipes::getInstance()->init();
			auto regBlock = [&](const Block* b, const char* name, const char* tex) {
				if (b) map[b->getId()] = { name, tex };
			};
			auto regItem = [&](const Item* it, const char* name, const char* tex) {
				if (it) map[it->getId()] = { name, tex };
			};

			regBlock(Block::mStone, "Stone", "textures/blocks/stone.png");
			regBlock(Block::mGrass, "Grass Block", "textures/blocks/grass_carried.png");
			regBlock(Block::mDirt, "Dirt", "textures/blocks/dirt.png");
			regBlock(Block::mCobblestone, "Cobblestone", "textures/blocks/cobblestone.png");
			regBlock(Block::mWoodPlanks, "Oak Planks", "textures/blocks/planks_oak.png");
			regBlock(Block::mSapling, "Sapling", "textures/blocks/sapling_oak.png");
			regBlock(Block::mBedrock, "Bedrock", "textures/blocks/bedrock.png");
			regBlock(Block::mSand, "Sand", "textures/blocks/sand.png");
			regBlock(Block::mGravel, "Gravel", "textures/blocks/gravel.png");
			regBlock(Block::mGoldOre, "Gold Ore", "textures/blocks/gold_ore.png");
			regBlock(Block::mIronOre, "Iron Ore", "textures/blocks/iron_ore.png");
			regBlock(Block::mCoalOre, "Coal Ore", "textures/blocks/coal_ore.png");
			regBlock(Block::mLog, "Oak Log", "textures/blocks/log_oak.png");
			regBlock(Block::mLog2, "Acacia Log", "textures/blocks/log_acacia.png");
			regBlock(Block::mLeaves, "Leaves", "textures/blocks/leaves_oak_carried.tga");
			regBlock(Block::mLeaves2, "Leaves", "textures/blocks/leaves_acacia_carried.tga");
			regBlock(Block::mSponge, "Sponge", "textures/blocks/sponge.png");
			regBlock(Block::mGlass, "Glass", "textures/blocks/glass.png");
			regBlock(Block::mLapisOre, "Lapis Lazuli Ore", "textures/blocks/lapis_ore.png");
			regBlock(Block::mLapisBlock, "Lapis Lazuli Block", "textures/blocks/lapis_block.png");
			regBlock(Block::mSandStone, "Sandstone", "textures/blocks/sandstone_normal.png");
			regBlock(Block::mWool, "Wool", "textures/blocks/wool_colored_white.png");
			regBlock(Block::mYellowFlower, "Dandelion", "textures/blocks/flower_dandelion.png");
			regBlock(Block::mRedFlower, "Poppy", "textures/blocks/flower_rose.png");
			regBlock(Block::mTallgrass, "Grass", "textures/blocks/tallgrass_carried.tga");
			regBlock(Block::mMossyCobblestone, "Moss Stone", "textures/blocks/cobblestone_mossy.png");
			regBlock(Block::mObsidian, "Obsidian", "textures/blocks/obsidian.png");
			regBlock(Block::mTorch, "Torch", "textures/blocks/torch_on.png");
			regBlock(Block::mChest, "Chest", "textures/blocks/chest_front.png");
			regBlock(Block::mDiamondOre, "Diamond Ore", "textures/blocks/diamond_ore.png");
			regBlock(Block::mWorkBench, "Crafting Table", "textures/blocks/crafting_table_top.png");
			regBlock(Block::mFurnace, "Furnace", "textures/blocks/furnace_front_off.png");
			regBlock(Block::mLitFurnace, "Furnace", "textures/blocks/furnace_front_on.png");
			regBlock(Block::mRedStoneOre, "Redstone Ore", "textures/blocks/redstone_ore.png");
			regBlock(Block::mLitRedStoneOre, "Redstone Ore", "textures/blocks/redstone_ore.png");
			regBlock(Block::mIce, "Ice", "textures/blocks/ice.png");
			regBlock(Block::mSnow, "Snow Block", "textures/blocks/snow.png");
			regBlock(Block::mCactus, "Cactus", "textures/blocks/cactus_side.tga");
			regBlock(Block::mClay, "Clay Block", "textures/blocks/clay.png");
			regBlock(Block::mReeds, "Sugar Canes", "textures/items/reeds.png");
			regBlock(Block::mPumpkin, "Pumpkin", "textures/blocks/pumpkin_face_off.png");
			regBlock(Block::mNetherrack, "Netherrack", "textures/blocks/netherrack.png");
			regBlock(Block::mSoulSand, "Soul Sand", "textures/blocks/soul_sand.png");
			regBlock(Block::mGlowStone, "Glowstone", "textures/blocks/glowstone.png");
			regBlock(Block::mStoneBrick, "Stone Bricks", "textures/blocks/stonebrick.png");
			regBlock(Block::mMelon, "Melon", "textures/blocks/melon_side.png");
			regBlock(Block::mGlassPane, "Glass Pane", "textures/blocks/glass.png");
			regBlock(Block::mWoodenSlab, "Oak Wood Slab", "textures/blocks/planks_oak.png");
			regBlock(Block::mStoneSlab, "Cobblestone Slab", "textures/blocks/cobblestone.png");
			regBlock(Block::mCobblestoneWall, "Cobblestone Wall", "textures/blocks/cobblestone.png");
			regBlock(Block::mBookshelf, "Bookshelf", "textures/blocks/bookshelf.png");
			regBlock(Block::mTNT, "TNT", "textures/blocks/tnt_side.png");
			regBlock(Block::mEmeraldOre, "Emerald Ore", "textures/blocks/emerald_ore.png");
			regBlock(Block::mQuartzOre, "Nether Quartz Ore", "textures/blocks/quartz_ore.png");
			regBlock(Block::mHardenedClay, "Hardened Clay", "textures/blocks/hardened_clay.png");
			regBlock(Block::mCoalBlock, "Block of Coal", "textures/blocks/coal_block.png");
			regBlock(Block::mStonecutterBench, "Stonecutter", "textures/blocks/stonecutter_top.png");
			regBlock(Block::mBrick, "Bricks", "textures/blocks/brick.png");

			// Materials & misc items
			regItem(Item::mStick, "Stick", "textures/items/stick.png");
			regItem(Item::mBowl, "Bowl", "textures/items/bowl.png");
			regItem(Item::mCoal, "Coal", "textures/items/coal.png");
			regItem(Item::mIronIngot, "Iron Ingot", "textures/items/iron_ingot.png");
			regItem(Item::mGoldIngot, "Gold Ingot", "textures/items/gold_ingot.png");
			regItem(Item::mDiamond, "Diamond", "textures/items/diamond.png");
			regItem(Item::mWheat, "Wheat", "textures/items/wheat.png");
			regItem(Item::mClay, "Clay", "textures/items/clay_ball.png");
			regItem(Item::mBrick, "Brick", "textures/items/brick.png");
			regItem(Item::mFlint, "Flint", "textures/items/flint.png");
			regItem(Item::mLeather, "Leather", "textures/items/leather.png");
			regItem(Item::mFeather, "Feather", "textures/items/feather.png");
			regItem(Item::mSulphur, "Gunpowder", "textures/items/gunpowder.png");
			regItem(Item::mRedStone, "Redstone", "textures/items/redstone_dust.png");
			regItem(Item::mYellowDust, "Glowstone Dust", "textures/items/glowstone_dust.png");
			regItem(Item::mDye_powder, "Dye", "textures/items/dye_powder_white.png");
			regItem(Item::mBone, "Bone", "textures/items/bone.png");
			regItem(Item::mSugar, "Sugar", "textures/items/sugar.png");
			regItem(Item::mPaper, "Paper", "textures/items/paper.png");
			regItem(Item::mBook, "Book", "textures/items/book_normal.png");
			regItem(Item::mEmerald, "Emerald", "textures/items/emerald.png");
			regItem(Item::mNetherbrick, "Nether Brick", "textures/items/netherbrick.png");
			regItem(Item::mNetherQuartz, "Nether Quartz", "textures/items/quartz.png");
			regItem(Item::mSeeds_wheat, "Wheat Seeds", "textures/items/seeds_wheat.png");
			regItem(Item::mShears, "Shears", "textures/items/shears.png");

			// Food items
			regItem(Item::mApple, "Apple", "textures/items/apple.png");
			regItem(Item::mApple_gold, "Golden Apple", "textures/items/apple_golden.png");
			regItem(Item::mApple_enchanted, "Enchanted Golden Apple", "textures/items/apple_golden.png");
			regItem(Item::mMushroomStew, "Mushroom Stew", "textures/items/mushroom_stew.png");
			regItem(Item::mBread, "Bread", "textures/items/bread.png");
			regItem(Item::mPorkChop_raw, "Raw Porkchop", "textures/items/porkchop_raw.png");
			regItem(Item::mPorkChop_cooked, "Cooked Porkchop", "textures/items/porkchop_cooked.png");
			regItem(Item::mFish_raw_cod, "Raw Fish", "textures/items/fish_raw.png");
			regItem(Item::mFish_cooked_cod, "Cooked Fish", "textures/items/fish_cooked.png");
			regItem(Item::mFish_raw_salmon, "Raw Salmon", "textures/items/fish_salmon_raw.png");
			regItem(Item::mFish_cooked_salmon, "Cooked Salmon", "textures/items/fish_salmon_cooked.png");
			regItem(Item::mCookie, "Cookie", "textures/items/cookie.png");
			regItem(Item::mMelon, "Melon", "textures/items/melon.png");
			regItem(Item::mBeef_raw, "Raw Beef", "textures/items/beef_raw.png");
			regItem(Item::mBeef_cooked, "Steak", "textures/items/beef_cooked.png");
			regItem(Item::mChicken_raw, "Raw Chicken", "textures/items/chicken_raw.png");
			regItem(Item::mChicken_cooked, "Cooked Chicken", "textures/items/chicken_cooked.png");
			regItem(Item::mRotten_flesh, "Rotten Flesh", "textures/items/rotten_flesh.png");
			regItem(Item::mSpider_eye, "Spider Eye", "textures/items/spider_eye.png");
			regItem(Item::mCarrot, "Carrot", "textures/items/carrot.png");
			regItem(Item::mPotato, "Potato", "textures/items/potato.png");
			regItem(Item::mPotatoBaked, "Baked Potato", "textures/items/potato_baked.png");
			regItem(Item::mPoisonous_potato, "Poisonous Potato", "textures/items/potato_poisonous.png");
			regItem(Item::mGoldenCarrot, "Golden Carrot", "textures/items/carrot_golden.png");
			regItem(Item::mPumpkinPie, "Pumpkin Pie", "textures/items/pumpkin_pie.png");
			regItem(Item::mRabbitRaw, "Raw Rabbit", "textures/items/rabbit_raw.png");
			regItem(Item::mRabbitCooked, "Cooked Rabbit", "textures/items/rabbit_cooked.png");
			regItem(Item::mRabbitStew, "Rabbit Stew", "textures/items/rabbit_stew.png");
			regItem(Item::mMutton_raw, "Raw Mutton", "textures/items/mutton_raw.png");
			regItem(Item::mMutton_cooked, "Cooked Mutton", "textures/items/mutton_cooked.png");
			regItem(Item::mBeetroot, "Beetroot", "textures/items/beetroot.png");
			regItem(Item::mBeetrootSoup, "Beetroot Soup", "textures/items/beetroot_soup.png");

			// Tools & Weapons
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

			regItem(Item::mPickAxe_gold, "Golden Pickaxe", "textures/items/gold_pickaxe.png");
			regItem(Item::mHatchet_gold, "Golden Axe", "textures/items/gold_axe.png");
			regItem(Item::mShovel_gold, "Golden Shovel", "textures/items/gold_shovel.png");
			regItem(Item::mSword_gold, "Golden Sword", "textures/items/gold_sword.png");

			// Armor
			regItem(Item::mHelmet_cloth, "Leather Cap", "textures/items/leather_helmet.tga");
			regItem(Item::mChestplate_cloth, "Leather Tunic", "textures/items/leather_chestplate.png");
			regItem(Item::mLeggings_cloth, "Leather Pants", "textures/items/leather_leggings.tga");
			regItem(Item::mBoots_cloth, "Leather Boots", "textures/items/leather_boots.tga");

			regItem(Item::mHelmet_chain, "Chain Helmet", "textures/items/chainmail_helmet.png");
			regItem(Item::mChestplate_chain, "Chain Chestplate", "textures/items/chainmail_chestplate.png");
			regItem(Item::mLeggings_chain, "Chain Leggings", "textures/items/chainmail_leggings.png");
			regItem(Item::mBoots_chain, "Chain Boots", "textures/items/chainmail_boots.png");

			regItem(Item::mHelmet_iron, "Iron Helmet", "textures/items/iron_helmet.png");
			regItem(Item::mChestplate_iron, "Iron Chestplate", "textures/items/iron_chestplate.png");
			regItem(Item::mLeggings_iron, "Iron Leggings", "textures/items/iron_leggings.png");
			regItem(Item::mBoots_iron, "Iron Boots", "textures/items/iron_boots.png");

			regItem(Item::mHelmet_diamond, "Diamond Helmet", "textures/items/diamond_helmet.png");
			regItem(Item::mChestplate_diamond, "Diamond Chestplate", "textures/items/diamond_chestplate.png");
			regItem(Item::mLeggings_diamond, "Diamond Leggings", "textures/items/diamond_leggings.png");
			regItem(Item::mBoots_diamond, "Diamond Boots", "textures/items/diamond_boots.png");

			regItem(Item::mHelmet_gold, "Golden Helmet", "textures/items/gold_helmet.png");
			regItem(Item::mChestplate_gold, "Golden Chestplate", "textures/items/gold_chestplate.png");
			regItem(Item::mLeggings_gold, "Golden Leggings", "textures/items/gold_leggings.png");
			regItem(Item::mBoots_gold, "Golden Boots", "textures/items/gold_boots.png");
		}
		return map;
	}
}

std::string getItemDisplayName(const ItemInstance& item) {
	if (item.isNull()) return "";
	int id = item.getId();
	if (Item::mCoal && id == Item::mCoal->getId() && item.getAuxValue() == 1) {
		return "Charcoal";
	}
	if (Item::mDye_powder && id == Item::mDye_powder->getId() && item.getAuxValue() == 4) {
		return "Lapis Lazuli";
	}
	const auto& map = getItemVisualMap();
	auto it = map.find(id);
	if (it != map.end()) {
		return it->second.name;
	}
	return "Item #" + std::to_string(id);
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
	if (getFoodInfo(item).isFood) return "Restores hunger.";
	int armorSlot = getArmorSlotForItem(item);
	if (armorSlot == 0) return "Protects your head.";
	if (armorSlot == 1) return "Protects your chest.";
	if (armorSlot == 2) return "Protects your legs.";
	if (armorSlot == 3) return "Protects your feet.";
	return "Craftable item.";
}

std::string getItemTexturePath(const ItemInstance& item) {
	if (item.isNull()) return "";
	int id = item.getId();
	if (Item::mCoal && id == Item::mCoal->getId() && item.getAuxValue() == 1) {
		return "textures/items/charcoal.png";
	}
	if (Item::mDye_powder && id == Item::mDye_powder->getId() && item.getAuxValue() == 4) {
		return "textures/items/dye_powder_blue.png";
	}
	const auto& map = getItemVisualMap();
	auto it = map.find(id);
	if (it != map.end()) {
		return it->second.texture;
	}
	return "";
}

BlockRenderInfo getBlockRenderInfo(const ItemInstance& item) {
	BlockRenderInfo info;
	if (item.isNull()) return info;
	int id = item.getId();
	if (id <= 0 || id >= 256) return info;

	// Flat 2D blocks in inventory
	if ((Block::mTorch && id == Block::mTorch->getId()) ||
	    (Block::mGlassPane && id == Block::mGlassPane->getId()) ||
	    (Block::mSapling && id == Block::mSapling->getId()) ||
	    (Block::mYellowFlower && id == Block::mYellowFlower->getId()) ||
	    (Block::mRedFlower && id == Block::mRedFlower->getId()) ||
	    (Block::mTallgrass && id == Block::mTallgrass->getId()) ||
	    (Block::mReeds && id == Block::mReeds->getId())) {
		return info;
	}

	info.is3D = true;
	std::string baseTex = getItemTexturePath(item);
	info.topTex = baseTex;
	info.frontTex = baseTex;
	info.sideTex = baseTex;

	if (Block::mGrass && id == Block::mGrass->getId()) {
		info.topTex = "textures/blocks/grass_carried.png";
		info.frontTex = "textures/blocks/grass_side_carried.png";
		info.sideTex = "textures/blocks/grass_side_carried.png";
	} else if (Block::mLog && id == Block::mLog->getId()) {
		info.topTex = "textures/blocks/log_oak_top.png";
		info.frontTex = "textures/blocks/log_oak.png";
		info.sideTex = "textures/blocks/log_oak.png";
	} else if (Block::mLog2 && id == Block::mLog2->getId()) {
		info.topTex = "textures/blocks/log_acacia_top.png";
		info.frontTex = "textures/blocks/log_acacia.png";
		info.sideTex = "textures/blocks/log_acacia.png";
	} else if (Block::mChest && id == Block::mChest->getId()) {
		info.topTex = "textures/blocks/chest_top.png";
		info.frontTex = "textures/blocks/chest_front.png";
		info.sideTex = "textures/blocks/chest_side.png";
	} else if (Block::mWorkBench && id == Block::mWorkBench->getId()) {
		info.topTex = "textures/blocks/crafting_table_top.png";
		info.frontTex = "textures/blocks/crafting_table_front.png";
		info.sideTex = "textures/blocks/crafting_table_side.png";
	} else if ((Block::mFurnace && id == Block::mFurnace->getId()) ||
	           (Block::mLitFurnace && id == Block::mLitFurnace->getId())) {
		info.topTex = "textures/blocks/furnace_top.png";
		info.frontTex = (Block::mLitFurnace && id == Block::mLitFurnace->getId())
			? "textures/blocks/furnace_front_on.png"
			: "textures/blocks/furnace_front_off.png";
		info.sideTex = "textures/blocks/furnace_side.png";
	} else if (Block::mSandStone && id == Block::mSandStone->getId()) {
		info.topTex = "textures/blocks/sandstone_top.png";
		info.frontTex = "textures/blocks/sandstone_normal.png";
		info.sideTex = "textures/blocks/sandstone_normal.png";
	} else if (Block::mStonecutterBench && id == Block::mStonecutterBench->getId()) {
		info.topTex = "textures/blocks/stonecutter_top.png";
		info.frontTex = "textures/blocks/stonecutter_side.png";
		info.sideTex = "textures/blocks/stonecutter_other_side.png";
	} else if (Block::mStoneSlab && id == Block::mStoneSlab->getId()) {
		info.isSlab = true;
		info.topTex = "textures/blocks/stone_slab_top.png";
		info.frontTex = "textures/blocks/stone_slab_side.png";
		info.sideTex = "textures/blocks/stone_slab_side.png";
	} else if (Block::mWoodenSlab && id == Block::mWoodenSlab->getId()) {
		info.isSlab = true;
	} else if (Block::mBookshelf && id == Block::mBookshelf->getId()) {
		info.topTex = "textures/blocks/planks_oak.png";
		info.frontTex = "textures/blocks/bookshelf.png";
		info.sideTex = "textures/blocks/bookshelf.png";
	} else if (Block::mTNT && id == Block::mTNT->getId()) {
		info.topTex = "textures/blocks/tnt_top.png";
		info.frontTex = "textures/blocks/tnt_side.png";
		info.sideTex = "textures/blocks/tnt_side.png";
	} else if (Block::mPumpkin && id == Block::mPumpkin->getId()) {
		info.topTex = "textures/blocks/pumpkin_top.png";
		info.frontTex = "textures/blocks/pumpkin_face_off.png";
		info.sideTex = "textures/blocks/pumpkin_side.png";
	} else if (Block::mMelon && id == Block::mMelon->getId()) {
		info.topTex = "textures/blocks/melon_top.png";
		info.frontTex = "textures/blocks/melon_side.png";
		info.sideTex = "textures/blocks/melon_side.png";
	} else if (Block::mCactus && id == Block::mCactus->getId()) {
		info.topTex = "textures/blocks/cactus_top.tga";
		info.frontTex = "textures/blocks/cactus_side.tga";
		info.sideTex = "textures/blocks/cactus_side.tga";
	} else if ((Block::mGlass && id == Block::mGlass->getId()) ||
	           (Block::mIce && id == Block::mIce->getId()) ||
	           (Block::mLeaves && id == Block::mLeaves->getId()) ||
	           (Block::mLeaves2 && id == Block::mLeaves2->getId())) {
		info.isTransparent = true;
	}

	return info;
}

namespace {
	struct ArmorMeta {
		const Item* item;
		int slot;
		int defense;
		int maxDurability;
	};

	const std::vector<ArmorMeta>& getArmorTable() {
		static std::vector<ArmorMeta> table;
		if (table.empty()) {
			Recipes::getInstance()->init();
			table = {
				{ Item::mHelmet_cloth, 0, 1, 55 },
				{ Item::mChestplate_cloth, 1, 3, 80 },
				{ Item::mLeggings_cloth, 2, 2, 75 },
				{ Item::mBoots_cloth, 3, 1, 65 },
				{ Item::mHelmet_chain, 0, 2, 165 },
				{ Item::mChestplate_chain, 1, 5, 240 },
				{ Item::mLeggings_chain, 2, 4, 225 },
				{ Item::mBoots_chain, 3, 1, 195 },
				{ Item::mHelmet_iron, 0, 2, 165 },
				{ Item::mChestplate_iron, 1, 6, 240 },
				{ Item::mLeggings_iron, 2, 5, 225 },
				{ Item::mBoots_iron, 3, 2, 195 },
				{ Item::mHelmet_gold, 0, 2, 77 },
				{ Item::mChestplate_gold, 1, 5, 112 },
				{ Item::mLeggings_gold, 2, 3, 105 },
				{ Item::mBoots_gold, 3, 1, 91 },
				{ Item::mHelmet_diamond, 0, 3, 363 },
				{ Item::mChestplate_diamond, 1, 8, 528 },
				{ Item::mLeggings_diamond, 2, 6, 495 },
				{ Item::mBoots_diamond, 3, 3, 429 },
			};
		}
		return table;
	}
}

int getArmorSlotForItem(const ItemInstance& item) {
	if (item.isNull()) return -1;
	int id = item.getId();
	for (const auto& entry : getArmorTable()) {
		if (entry.item && id == entry.item->getId()) return entry.slot;
	}
	return -1;
}

int getArmorDefensePoints(const ItemInstance& item) {
	if (item.isNull()) return 0;
	int id = item.getId();
	for (const auto& entry : getArmorTable()) {
		if (entry.item && id == entry.item->getId()) return entry.defense;
	}
	return 0;
}

int getTotalArmorDefense(const std::array<ItemInstance, 4>& armorSlots) {
	int total = 0;
	for (const auto& slot : armorSlots) {
		total += getArmorDefensePoints(slot);
	}
	return std::min(20, total);
}

ToolInfo getToolInfo(const ItemInstance& item) {
	ToolInfo info;
	if (item.isNull()) return info;
	Recipes::getInstance()->init();
	int id = item.getId();

	struct ToolEntry {
		const Item* item;
		ToolType type;
		int harvestLevel;
		float speed;
		int maxDurability;
	};
	static const ToolEntry tools[] = {
		{ Item::mPickAxe_wood, ToolType::Pickaxe, 0, 2.0f, 59 },
		{ Item::mPickAxe_stone, ToolType::Pickaxe, 1, 4.0f, 131 },
		{ Item::mPickAxe_iron, ToolType::Pickaxe, 2, 6.0f, 250 },
		{ Item::mPickAxe_diamond, ToolType::Pickaxe, 3, 8.0f, 1561 },
		{ Item::mPickAxe_gold, ToolType::Pickaxe, 0, 12.0f, 32 },

		{ Item::mHatchet_wood, ToolType::Axe, 0, 2.0f, 59 },
		{ Item::mHatchet_stone, ToolType::Axe, 1, 4.0f, 131 },
		{ Item::mHatchet_iron, ToolType::Axe, 2, 6.0f, 250 },
		{ Item::mHatchet_diamond, ToolType::Axe, 3, 8.0f, 1561 },
		{ Item::mHatchet_gold, ToolType::Axe, 0, 12.0f, 32 },

		{ Item::mShovel_wood, ToolType::Shovel, 0, 2.0f, 59 },
		{ Item::mShovel_stone, ToolType::Shovel, 1, 4.0f, 131 },
		{ Item::mShovel_iron, ToolType::Shovel, 2, 6.0f, 250 },
		{ Item::mShovel_diamond, ToolType::Shovel, 3, 8.0f, 1561 },
		{ Item::mShovel_gold, ToolType::Shovel, 0, 12.0f, 32 },

		{ Item::mSword_wood, ToolType::Sword, 0, 1.5f, 59 },
		{ Item::mSword_stone, ToolType::Sword, 1, 1.5f, 131 },
		{ Item::mSword_iron, ToolType::Sword, 2, 1.5f, 250 },
		{ Item::mSword_diamond, ToolType::Sword, 3, 1.5f, 1561 },
		{ Item::mSword_gold, ToolType::Sword, 0, 1.5f, 32 },

		{ Item::mShears, ToolType::Shears, 0, 5.0f, 238 },
	};
	for (const auto& t : tools) {
		if (t.item && id == t.item->getId()) {
			info.type = t.type;
			info.harvestLevel = t.harvestLevel;
			info.speedMultiplier = t.speed;
			info.maxDurability = t.maxDurability;
			return info;
		}
	}
	return info;
}

int getItemMaxDurability(const ItemInstance& item) {
	if (item.isNull()) return 0;
	ToolInfo t = getToolInfo(item);
	if (t.maxDurability > 0) return t.maxDurability;
	int id = item.getId();
	for (const auto& entry : getArmorTable()) {
		if (entry.item && id == entry.item->getId()) return entry.maxDurability;
	}
	return 0;
}

FoodInfo getFoodInfo(const ItemInstance& item) {
	FoodInfo info;
	if (item.isNull()) return info;
	Recipes::getInstance()->init();
	int id = item.getId();

	struct FoodEntry {
		const Item* item;
		int nutrition;
		float saturation;
		int healHealth;
		bool alwaysEdible;
		bool returnsBowl;
	};
	static const FoodEntry foods[] = {
		{ Item::mApple, 4, 2.4f, 0, false, false },
		{ Item::mApple_gold, 4, 9.6f, 4, true, false },
		{ Item::mApple_enchanted, 4, 9.6f, 10, true, false },
		{ Item::mBread, 5, 6.0f, 0, false, false },
		{ Item::mPorkChop_raw, 3, 1.8f, 0, false, false },
		{ Item::mPorkChop_cooked, 8, 12.8f, 0, false, false },
		{ Item::mBeef_raw, 3, 1.8f, 0, false, false },
		{ Item::mBeef_cooked, 8, 12.8f, 0, false, false },
		{ Item::mChicken_raw, 2, 1.2f, 0, false, false },
		{ Item::mChicken_cooked, 6, 7.2f, 0, false, false },
		{ Item::mMutton_raw, 2, 1.2f, 0, false, false },
		{ Item::mMutton_cooked, 6, 9.6f, 0, false, false },
		{ Item::mRabbitRaw, 3, 1.8f, 0, false, false },
		{ Item::mRabbitCooked, 5, 6.0f, 0, false, false },
		{ Item::mRabbitStew, 10, 12.0f, 0, false, true },
		{ Item::mMushroomStew, 6, 7.2f, 0, false, true },
		{ Item::mBeetroot, 1, 1.2f, 0, false, false },
		{ Item::mBeetrootSoup, 6, 7.2f, 0, false, true },
		{ Item::mFish_raw_cod, 2, 0.4f, 0, false, false },
		{ Item::mFish_cooked_cod, 5, 6.0f, 0, false, false },
		{ Item::mFish_raw_salmon, 2, 0.4f, 0, false, false },
		{ Item::mFish_cooked_salmon, 6, 9.6f, 0, false, false },
		{ Item::mCookie, 2, 0.4f, 0, false, false },
		{ Item::mMelon, 2, 1.2f, 0, false, false },
		{ Item::mCarrot, 3, 3.6f, 0, false, false },
		{ Item::mGoldenCarrot, 6, 14.4f, 0, false, false },
		{ Item::mPotato, 1, 0.6f, 0, false, false },
		{ Item::mPotatoBaked, 5, 6.0f, 0, false, false },
		{ Item::mPoisonous_potato, 2, 1.2f, 0, false, false },
		{ Item::mPumpkinPie, 8, 4.8f, 0, false, false },
		{ Item::mRotten_flesh, 4, 0.8f, 0, false, false },
		{ Item::mSpider_eye, 2, 3.2f, 0, false, false },
	};
	for (const auto& f : foods) {
		if (f.item && id == f.item->getId()) {
			info.isFood = true;
			info.nutrition = f.nutrition;
			info.saturation = f.saturation;
			info.healHealth = f.healHealth;
			info.alwaysEdible = f.alwaysEdible;
			info.returnsBowl = f.returnsBowl;
			return info;
		}
	}
	return info;
}
