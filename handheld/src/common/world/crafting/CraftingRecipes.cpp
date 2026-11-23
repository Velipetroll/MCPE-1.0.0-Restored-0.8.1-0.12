#include "world/crafting/CraftingRecipes.h"
#include "world/item/Item.h"
#include "world/level/block/Block.h"
#include "world/level/material/Material.h"
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
		Item::initServerData();

		// Register items that are commented out in Item::registerItems()
		ensureItem(Item::mFlintAndSteel, "flint_and_steel", 3, 1)->setMaxDamage(64);
		ensureItem(Item::mBow, "bow", 5, 1)->setMaxDamage(384);
		ensureItem(Item::mArrow, "arrow", 6);
		ensureItem(Item::mSign, "sign", 67, 16);
		ensureItem(Item::mDoor_wood, "door_wood", 68, 1);
		ensureItem(Item::mBucket, "bucket", 69, 16);
		ensureItem(Item::mMinecart, "minecart", 72, 1);
		ensureItem(Item::mSaddle, "saddle", 73, 1);
		ensureItem(Item::mDoor_iron, "door_iron", 74, 1);
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
		ensureItem(Item::mPainting, "painting", 65);
		ensureItem(Item::mRedStone, "redstone", 75);
		ensureItem(Item::mSnowBall, "snowball", 76, 16);
		ensureItem(Item::mBoat, "boat", 77, 1);
		ensureItem(Item::mLeather, "leather", 78);
		ensureItem(Item::mBrick, "brick", 80);
		ensureItem(Item::mClay, "clay_ball", 81);
		ensureItem(Item::mReeds, "reeds", 82);
		ensureItem(Item::mPaper, "paper", 83);
		ensureItem(Item::mBook, "book", 84);
		ensureItem(Item::mSlimeBall, "slime_ball", 85);
		ensureItem(Item::mEgg, "egg", 88, 16);
		ensureItem(Item::mCompass, "compass", 89);
		ensureItem(Item::mFishingRod, "fishing_rod", 90, 1, true, true);
		ensureItem(Item::mClock, "clock", 91);
		ensureItem(Item::mYellowDust, "glowstone_dust", 92);
		ensureItem(Item::mDye_powder, "dye", 95);
		ensureItem(Item::mBone, "bone", 96, 64, true, false);
		ensureItem(Item::mSugar, "sugar", 97);
		ensureItem(Item::mCake, "cake", 98, 1);
		ensureItem(Item::mBed, "bed", 99, 1);
		ensureItem(Item::mShears, "shears", 103, 1)->setMaxDamage(238);
		ensureItem(Item::mMobPlacer, "spawn_egg", 127);
		ensureItem(Item::mEmerald, "emerald", 132);
		ensureItem(Item::mNetherbrick, "netherbrick", 149);
		ensureItem(Item::mNetherQuartz, "quartz", 150);
		ensureItem(Item::mDoor_spruce, "spruce_door", 171);
		ensureItem(Item::mDoor_birch, "birch_door", 172);
		ensureItem(Item::mDoor_jungle, "jungle_door", 173);
		ensureItem(Item::mDoor_acacia, "acacia_door", 174);
		ensureItem(Item::mDoor_darkoak, "dark_oak_door", 175);

		ensureItem(Item::mHoe_wood, "wooden_hoe", 34, 1, true, false)->setMaxDamage(59);
		ensureItem(Item::mHoe_stone, "stone_hoe", 35, 1, true, false)->setMaxDamage(131);
		ensureItem(Item::mHoe_iron, "iron_hoe", 36, 1, true, false)->setMaxDamage(250);
		ensureItem(Item::mHoe_diamond, "diamond_hoe", 37, 1, true, false)->setMaxDamage(1561);
		ensureItem(Item::mHoe_gold, "golden_hoe", 38, 1, true, false)->setMaxDamage(32);

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

		if (Item::mMushroomStew) const_cast<Item*>(Item::mMushroomStew)->setMaxStackSize(1);
		if (Item::mBeetrootSoup) const_cast<Item*>(Item::mBeetrootSoup)->setMaxStackSize(1);

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

	// Bed (3x2)
	ItemInstance woolAny(Block::mWool, 1, -1);
	addShaped(3, 2, {
		woolAny,   woolAny,   woolAny,
		planksAny, planksAny, planksAny
	}, ItemInstance(Item::mBed, 1, 0), ItemCategory::Structures);

	// Decorative & Stonecutter Blocks
	auto isSC = [](const ItemInstance& res) -> bool {
		if (res.isNull() || res.getId() > 255) return false;
		const Block* b = Block::mBlocks[res.getId()];
		if (!b || b == Block::mLapisBlock || b == Block::mFurnace || b == Block::mStonecutterBench ||
		    (b == Block::mStoneSlab && res.getAuxValue() == 2)) {
			return false;
		}
		return b->getMaterial().isType(MaterialType::Stone) || b->getMaterial().isType(MaterialType::Sand);
	};
	auto addShaped081 = [&](int w, int h, std::vector<ItemInstance> ing, const ItemInstance& res, ItemCategory cat) {
		addShaped(w, h, std::move(ing), res, cat, isSC(res));
	};
	auto addShapeless081 = [&](std::vector<ItemInstance> ing, const ItemInstance& res, ItemCategory cat) {
		addShapeless(std::move(ing), res, cat, isSC(res));
	};

	// StructureRecipes
	addShaped081(2, 2, { sand, sand, sand, sand }, ItemInstance(Block::mSandStone, 1, 0), ItemCategory::Structures);
	addShaped081(2, 2, { ItemInstance(Block::mSandStone, 1, 0), ItemInstance(Block::mSandStone, 1, 0), ItemInstance(Block::mSandStone, 1, 0), ItemInstance(Block::mSandStone, 1, 0) }, ItemInstance(Block::mSandStone, 4, 2), ItemCategory::Structures);
	addShaped081(1, 2, { ItemInstance(Block::mStoneSlab, 1, 1), ItemInstance(Block::mStoneSlab, 1, 1) }, ItemInstance(Block::mSandStone, 1, 1), ItemCategory::Structures);
	addShaped081(2, 2, { stone, stone, stone, stone }, ItemInstance(Block::mStoneBrick, 4, 0), ItemCategory::Structures);
	addShaped081(3, 2, { glass, glass, glass, glass, glass, glass }, ItemInstance(Block::mGlassPane, 16, 0), ItemCategory::Decorations);
	if (Item::mNetherbrick && Block::mNetherBrick) {
		ItemInstance nb(Item::mNetherbrick, 1, 0);
		addShaped081(2, 2, { nb, nb, nb, nb }, ItemInstance(Block::mNetherBrick, 1, 0), ItemCategory::Structures);
	}
	if (Item::mNetherQuartz && Block::mQuartzBlock) {
		ItemInstance nq(Item::mNetherQuartz, 1, 0);
		addShaped081(2, 2, { nq, nq, nq, nq }, ItemInstance(Block::mQuartzBlock, 1, 0), ItemCategory::Structures);
		addShaped081(1, 2, { ItemInstance(Block::mStoneSlab, 1, 6), ItemInstance(Block::mStoneSlab, 1, 6) }, ItemInstance(Block::mQuartzBlock, 1, 1), ItemCategory::Structures);
		addShaped081(1, 2, { ItemInstance(Block::mQuartzBlock, 1, 0), ItemInstance(Block::mQuartzBlock, 1, 0) }, ItemInstance(Block::mQuartzBlock, 2, 2), ItemCategory::Structures);
	}

	// ToolRecipes: Hoes & Shears
	auto addHoe = [&](const ItemInstance& mat, const Item* hoe) {
		if (hoe) {
			addShaped081(2, 3, { mat, mat, empty, stick, empty, stick }, ItemInstance(hoe, 1, 0), ItemCategory::Tools);
		}
	};
	addHoe(planksAny, Item::mHoe_wood);
	addHoe(cobble, Item::mHoe_stone);
	addHoe(iron, Item::mHoe_iron);
	addHoe(diamond, Item::mHoe_diamond);
	addHoe(gold, Item::mHoe_gold);
	if (Item::mShears) {
		addShaped081(2, 2, { empty, iron, iron, empty }, ItemInstance(Item::mShears, 1, 0), ItemCategory::Tools);
	}

	// WeaponRecipes: Bow & Arrow
	if (Item::mBow && Item::mString) {
		ItemInstance str(Item::mString, 1, 0);
		addShaped081(3, 3, { empty, stick, str, stick, empty, str, empty, stick, str }, ItemInstance(Item::mBow, 1, 0), ItemCategory::Tools);
	}
	if (Item::mArrow && Item::mFlint && Item::mFeather) {
		addShaped081(1, 3, { ItemInstance(Item::mFlint, 1, 0), stick, ItemInstance(Item::mFeather, 1, 0) }, ItemInstance(Item::mArrow, 4, 0), ItemCategory::Tools);
	}

	// OreRecipes (12 recipes)
	struct OrePair { const Block* block; ItemInstance item9; };
	OrePair orePairs[] = {
		{ Block::mGoldBlock, ItemInstance(Item::mGoldIngot, 9, 0) },
		{ Block::mIronBlock, ItemInstance(Item::mIronIngot, 9, 0) },
		{ Block::mDiamondBlock, ItemInstance(Item::mDiamond, 9, 0) },
		{ Block::mLapisBlock, ItemInstance(Item::mDye_powder, 9, 4) },
		{ Block::mHayBlock, ItemInstance(Item::mWheat, 9, 0) },
		{ Block::mCoalBlock, ItemInstance(Item::mCoal, 9, 0) }
	};
	for (const auto& op : orePairs) {
		if (!op.block || op.item9.isNull()) continue;
		ItemInstance single(op.item9.getItem(), 1, op.item9.getAuxValue());
		addShaped081(3, 3, { single, single, single, single, single, single, single, single, single }, ItemInstance(op.block, 1, 0), ItemCategory::Structures);
		addShaped081(1, 1, { ItemInstance(op.block, 1, 0) }, op.item9, ItemCategory::Structures);
	}

	// FoodRecipes
	if (Item::mMushroomStew && Block::mBrownMushroom && Block::mRedMushroom && Item::mBowl) {
		addShapeless081({ ItemInstance(Block::mBrownMushroom, 1, 0), ItemInstance(Block::mRedMushroom, 1, 0), ItemInstance(Item::mBowl, 1, 0) }, ItemInstance(Item::mMushroomStew, 1, 0), ItemCategory::FoodArmor);
	}
	if (Item::mBeetrootSoup && Item::mBeetroot && Item::mBowl) {
		ItemInstance beet(Item::mBeetroot, 1, 0);
		addShaped081(2, 3, { beet, beet, beet, beet, ItemInstance(Item::mBowl, 1, 0), empty }, ItemInstance(Item::mBeetrootSoup, 1, 0), ItemCategory::FoodArmor);
	}
	if (Block::mMelon && Item::mMelon) {
		ItemInstance m(Item::mMelon, 1, 0);
		addShaped081(3, 3, { m, m, m, m, m, m, m, m, m }, ItemInstance(Block::mMelon, 1, 0), ItemCategory::FoodArmor);
		if (Item::mSeeds_melon) {
			addShaped081(1, 1, { m }, ItemInstance(Item::mSeeds_melon, 1, 0), ItemCategory::FoodArmor);
		}
	}
	if (Block::mPumpkin && Item::mSeeds_pumpkin) {
		addShaped081(1, 1, { ItemInstance(Block::mPumpkin, 1, 0) }, ItemInstance(Item::mSeeds_pumpkin, 4, 0), ItemCategory::FoodArmor);
	}
	if (Item::mPumpkinPie && Block::mPumpkin && Item::mSugar && Item::mEgg) {
		addShapeless081({ ItemInstance(Block::mPumpkin, 1, 0), ItemInstance(Item::mSugar, 1, 0), ItemInstance(Item::mEgg, 1, 0) }, ItemInstance(Item::mPumpkinPie, 1, 0), ItemCategory::FoodArmor);
	}

	// ClothDyeRecipes (32 recipes)
	if (Item::mDye_powder && Block::mWool) {
		for (int c = 0; c < 16; ++c) {
			addShapeless081({ ItemInstance(Item::mDye_powder, 1, c), ItemInstance(Block::mWool, 1, 0) }, ItemInstance(Block::mWool, 1, (~c) & 0xF), ItemCategory::Decorations);
		}
		if (Block::mYellowFlower) addShapeless081({ ItemInstance(Block::mYellowFlower, 1, 0) }, ItemInstance(Item::mDye_powder, 2, 11), ItemCategory::Decorations);
		if (Item::mBeetroot) addShapeless081({ ItemInstance(Item::mBeetroot, 1, 0) }, ItemInstance(Item::mDye_powder, 2, 1), ItemCategory::Decorations);
		if (Item::mBone) addShapeless081({ ItemInstance(Item::mBone, 1, 0) }, ItemInstance(Item::mDye_powder, 3, 15), ItemCategory::Decorations);
		auto dye = [&](int aux) { return ItemInstance(Item::mDye_powder, 1, aux); };
		addShapeless081({ dye(1), dye(15) }, ItemInstance(Item::mDye_powder, 2, 9), ItemCategory::Decorations);
		addShapeless081({ dye(1), dye(11) }, ItemInstance(Item::mDye_powder, 2, 14), ItemCategory::Decorations);
		addShapeless081({ dye(2), dye(15) }, ItemInstance(Item::mDye_powder, 2, 10), ItemCategory::Decorations);
		addShapeless081({ dye(0), dye(15) }, ItemInstance(Item::mDye_powder, 2, 8), ItemCategory::Decorations);
		addShapeless081({ dye(8), dye(15) }, ItemInstance(Item::mDye_powder, 2, 7), ItemCategory::Decorations);
		addShapeless081({ dye(0), dye(15), dye(15) }, ItemInstance(Item::mDye_powder, 3, 7), ItemCategory::Decorations);
		addShapeless081({ dye(0), dye(14) }, ItemInstance(Item::mDye_powder, 2, 3), ItemCategory::Decorations);
		addShapeless081({ dye(0), dye(1), dye(11) }, ItemInstance(Item::mDye_powder, 3, 3), ItemCategory::Decorations);
		addShapeless081({ dye(4), dye(15) }, ItemInstance(Item::mDye_powder, 2, 12), ItemCategory::Decorations);
		addShapeless081({ dye(4), dye(2) }, ItemInstance(Item::mDye_powder, 2, 6), ItemCategory::Decorations);
		addShapeless081({ dye(4), dye(1) }, ItemInstance(Item::mDye_powder, 2, 5), ItemCategory::Decorations);
		addShapeless081({ dye(5), dye(9) }, ItemInstance(Item::mDye_powder, 2, 13), ItemCategory::Decorations);
		addShapeless081({ dye(4), dye(1), dye(9) }, ItemInstance(Item::mDye_powder, 3, 13), ItemCategory::Decorations);
		addShapeless081({ dye(4), dye(1), dye(1), dye(15) }, ItemInstance(Item::mDye_powder, 4, 13), ItemCategory::Decorations);
	}

	// Base Recipes.cpp
	for (int p = 0; p < 3; ++p) {
		addShapeless081({ ItemInstance(Block::mLog, 1, p) }, ItemInstance(Block::mWoodPlanks, 4, p), ItemCategory::Structures);
		addShaped081(3, 1, { ItemInstance(Block::mWoodPlanks, 1, p), ItemInstance(Block::mWoodPlanks, 1, p), ItemInstance(Block::mWoodPlanks, 1, p) }, ItemInstance(Block::mWoodenSlab, 6, p), ItemCategory::Structures);
	}
	if (Item::mReeds && Item::mPaper) {
		addShaped081(3, 1, { ItemInstance(Item::mReeds, 1, 0), ItemInstance(Item::mReeds, 1, 0), ItemInstance(Item::mReeds, 1, 0) }, ItemInstance(Item::mPaper, 3, 0), ItemCategory::Structures);
		if (Item::mBook) {
			ItemInstance pap(Item::mPaper, 1, 0);
			addShaped081(1, 3, { pap, pap, pap }, ItemInstance(Item::mBook, 1, 0), ItemCategory::Structures);
			if (Block::mBookshelf) {
				addShaped081(3, 3, { planksAny, planksAny, planksAny, ItemInstance(Item::mBook, 1, 0), ItemInstance(Item::mBook, 1, 0), ItemInstance(Item::mBook, 1, 0), planksAny, planksAny, planksAny }, ItemInstance(Block::mBookshelf, 1, 0), ItemCategory::Decorations);
			}
		}
		if (Item::mSugar) {
			addShaped081(1, 1, { ItemInstance(Item::mReeds, 1, 0) }, ItemInstance(Item::mSugar, 1, 0), ItemCategory::FoodArmor);
		}
	}
	addShaped081(3, 2, { stick, stick, stick, stick, stick, stick }, ItemInstance(Block::mFence, 2, 0), ItemCategory::Structures);
	addShaped081(3, 2, { cobble, cobble, cobble, cobble, cobble, cobble }, ItemInstance(Block::mCobblestoneWall, 6, 0), ItemCategory::Structures);
	addShaped081(3, 2, { stick, planksAny, stick, stick, planksAny, stick }, ItemInstance(Block::mFenceGateOak, 1, 0), ItemCategory::Structures);
	if (Item::mSnowBall) addShaped081(2, 2, { ItemInstance(Item::mSnowBall, 1, 0), ItemInstance(Item::mSnowBall, 1, 0), ItemInstance(Item::mSnowBall, 1, 0), ItemInstance(Item::mSnowBall, 1, 0) }, ItemInstance(Block::mSnow, 1, 0), ItemCategory::Structures);
	if (Item::mClay) addShaped081(2, 2, { ItemInstance(Item::mClay, 1, 0), ItemInstance(Item::mClay, 1, 0), ItemInstance(Item::mClay, 1, 0), ItemInstance(Item::mClay, 1, 0) }, ItemInstance(Block::mClay, 1, 0), ItemCategory::Structures);
	if (Item::mBrick) addShaped081(2, 2, { ItemInstance(Item::mBrick, 1, 0), ItemInstance(Item::mBrick, 1, 0), ItemInstance(Item::mBrick, 1, 0), ItemInstance(Item::mBrick, 1, 0) }, ItemInstance(Block::mBrick, 1, 0), ItemCategory::Structures);
	if (Item::mYellowDust) addShaped081(2, 2, { ItemInstance(Item::mYellowDust, 1, 0), ItemInstance(Item::mYellowDust, 1, 0), ItemInstance(Item::mYellowDust, 1, 0), ItemInstance(Item::mYellowDust, 1, 0) }, ItemInstance(Block::mGlowStone, 1, 0), ItemCategory::Decorations);
	if (Item::mString) addShaped081(2, 2, { ItemInstance(Item::mString, 1, 0), ItemInstance(Item::mString, 1, 0), ItemInstance(Item::mString, 1, 0), ItemInstance(Item::mString, 1, 0) }, ItemInstance(Block::mWool, 1, 0), ItemCategory::Structures);
	if (Item::mSulphur) {
		ItemInstance gp(Item::mSulphur, 1, 0);
		addShaped081(3, 3, { gp, sand, gp, sand, gp, sand, gp, sand, gp }, ItemInstance(Block::mTNT, 1, 0), ItemCategory::Tools);
	}
	addShaped081(3, 1, { cobble, cobble, cobble }, ItemInstance(Block::mStoneSlab, 6, 3), ItemCategory::Structures);
	addShaped081(3, 1, { stone, stone, stone }, ItemInstance(Block::mStoneSlab, 6, 0), ItemCategory::Structures);
	addShaped081(3, 1, { ItemInstance(Block::mSandStone, 1, 0), ItemInstance(Block::mSandStone, 1, 0), ItemInstance(Block::mSandStone, 1, 0) }, ItemInstance(Block::mStoneSlab, 6, 1), ItemCategory::Structures);
	addShaped081(3, 1, { ItemInstance(Block::mBrick, 1, 0), ItemInstance(Block::mBrick, 1, 0), ItemInstance(Block::mBrick, 1, 0) }, ItemInstance(Block::mStoneSlab, 6, 4), ItemCategory::Structures);
	addShaped081(3, 1, { ItemInstance(Block::mStoneBrick, 1, 0), ItemInstance(Block::mStoneBrick, 1, 0), ItemInstance(Block::mStoneBrick, 1, 0) }, ItemInstance(Block::mStoneSlab, 6, 5), ItemCategory::Structures);
	addShaped081(3, 1, { ItemInstance(Block::mQuartzBlock, 1, 0), ItemInstance(Block::mQuartzBlock, 1, 0), ItemInstance(Block::mQuartzBlock, 1, 0) }, ItemInstance(Block::mStoneSlab, 6, 6), ItemCategory::Structures);
	addShaped081(3, 3, { stick, empty, stick, stick, stick, stick, stick, empty, stick }, ItemInstance(Block::mLadder, 2, 0), ItemCategory::Structures);
	if (Item::mDoor_wood) addShaped081(2, 3, { planksAny, planksAny, planksAny, planksAny, planksAny, planksAny }, ItemInstance(Item::mDoor_wood, 1, 0), ItemCategory::Structures);
	addShaped081(3, 2, { planksAny, planksAny, planksAny, planksAny, planksAny, planksAny }, ItemInstance(Block::mTrapdoor, 2, 0), ItemCategory::Structures);
	if (Item::mSign) addShaped081(3, 3, { planksAny, planksAny, planksAny, planksAny, planksAny, planksAny, empty, stick, empty }, ItemInstance(Item::mSign, 1, 0), ItemCategory::Decorations);
	if (Item::mCake && Item::mBucket && Item::mSugar && Item::mEgg) {
		ItemInstance milk(Item::mBucket, 1, 1);
		ItemInstance sug(Item::mSugar, 1, 0);
		addShaped081(3, 3, { milk, milk, milk, sug, ItemInstance(Item::mEgg, 1, 0), sug, wheat, wheat, wheat }, ItemInstance(Item::mCake, 1, 0), ItemCategory::FoodArmor);
	}
	if (Item::mBowl) addShaped081(3, 2, { planksAny, empty, planksAny, empty, planksAny, empty }, ItemInstance(Item::mBowl, 4, 0), ItemCategory::Structures);
	addShaped081(3, 3, { iron, empty, iron, iron, stick, iron, iron, empty, iron }, ItemInstance(Block::mRail, 16, 0), ItemCategory::Structures);
	if (Item::mRedStone) addShaped081(3, 3, { gold, empty, gold, gold, stick, gold, gold, ItemInstance(Item::mRedStone, 1, 0), gold }, ItemInstance(Block::mGoldenRail, 6, 0), ItemCategory::Structures);
	if (Item::mMinecart) addShaped081(3, 2, { iron, empty, iron, iron, iron, iron }, ItemInstance(Item::mMinecart, 1, 0), ItemCategory::Tools);
	addShaped081(1, 2, { ItemInstance(Block::mPumpkin, 1, 0), ItemInstance(Block::mTorch, 1, 0) }, ItemInstance(Block::mLitPumpkin, 1, 0), ItemCategory::Decorations);
	if (Item::mBucket) addShaped081(3, 2, { iron, empty, iron, empty, iron, empty }, ItemInstance(Item::mBucket, 1, 0), ItemCategory::Tools);
	if (Item::mFlintAndSteel && Item::mFlint) addShaped081(2, 2, { iron, empty, empty, ItemInstance(Item::mFlint, 1, 0) }, ItemInstance(Item::mFlintAndSteel, 1, 0), ItemCategory::Tools);
	auto addStairs = [&](const ItemInstance& mat, const Block* stair) {
		if (stair) addShaped081(3, 3, { mat, empty, empty, mat, mat, empty, mat, mat, mat }, ItemInstance(stair, 4, 0), ItemCategory::Structures);
	};
	addStairs(ItemInstance(Block::mWoodPlanks, 1, 0), Block::mOakStairs);
	addStairs(ItemInstance(Block::mWoodPlanks, 1, 1), Block::mSpruceStairs);
	addStairs(ItemInstance(Block::mWoodPlanks, 1, 2), Block::mBirchStairs);
	addStairs(cobble, Block::mStoneStairs);
	addStairs(ItemInstance(Block::mBrick, 1, 0), Block::mBrickStairs);
	addStairs(ItemInstance(Block::mSandStone, 1, 0), Block::mSandstoneStairs);
	addStairs(ItemInstance(Block::mStoneBrick, 1, 0), Block::mStoneBrickStairs);
	addStairs(ItemInstance(Block::mNetherBrick, 1, 0), Block::mNetherBrickStairs);
	if (Item::mPainting) addShaped081(3, 3, { stick, stick, stick, stick, woolAny, stick, stick, stick, stick }, ItemInstance(Item::mPainting, 1, 0), ItemCategory::Decorations);
	if (Item::mClock && Item::mRedStone) addShaped081(3, 3, { empty, gold, empty, gold, ItemInstance(Item::mRedStone, 1, 0), gold, empty, gold, empty }, ItemInstance(Item::mClock, 1, 0), ItemCategory::Tools);
	if (Item::mCompass && Item::mRedStone) addShaped081(3, 3, { empty, iron, empty, iron, ItemInstance(Item::mRedStone, 1, 0), iron, empty, iron, empty }, ItemInstance(Item::mCompass, 1, 0), ItemCategory::Tools);
	addShaped081(3, 2, { iron, iron, iron, iron, iron, iron }, ItemInstance(Block::mIronFence, 16, 0), ItemCategory::Structures);
	if (Block::mNetherReactor) addShaped081(3, 3, { iron, diamond, iron, iron, diamond, iron, iron, diamond, iron }, ItemInstance(Block::mNetherReactor, 1, 0), ItemCategory::Structures);
	if (Block::mAnvil && Block::mIronBlock) {
		ItemInstance ib(Block::mIronBlock, 1, 0);
		addShaped081(3, 3, { ib, ib, ib, empty, iron, empty, iron, iron, iron }, ItemInstance(Block::mAnvil, 1, 0), ItemCategory::Structures);
	}
	if (Block::mWoolCarpet) addShaped081(2, 1, { ItemInstance(Block::mWool, 1, 14), ItemInstance(Block::mWool, 1, 14) }, ItemInstance(Block::mWoolCarpet, 3, 14), ItemCategory::Decorations);
}

ItemInstance Recipes::findMatchingRecipe(const CraftingContainer& container, bool isStonecutter) const {
	for (const auto& recipe : mRecipes) {
		if (recipe && recipe->isStonecutter() == isStonecutter && recipe->matches(container)) {
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
			regBlock(Block::mBed, "Bed", "textures/items/bed.png");
			regBlock(Block::mTrapdoor, "Oak Trapdoor", "textures/blocks/trapdoor.png");
			regBlock(Block::mIronTrapdoor, "Iron Trapdoor", "textures/blocks/iron_trapdoor.png");
			regBlock(Block::mFence, "Oak Fence", "textures/blocks/planks_oak.png");
			regBlock(Block::mNetherFence, "Nether Brick Fence", "textures/blocks/nether_brick.png");
			regBlock(Block::mSpuceFenceGate, "Spruce Fence Gate", "textures/blocks/planks_spruce.png");
			regBlock(Block::mBirchFenceGate, "Birch Fence Gate", "textures/blocks/planks_birch.png");
			regBlock(Block::mJungleFenceGate, "Jungle Fence Gate", "textures/blocks/planks_jungle.png");
			regBlock(Block::mAcaciaFenceGate, "Acacia Fence Gate", "textures/blocks/planks_acacia.png");
			regBlock(Block::mDarkOakFenceGate, "Dark Oak Fence Gate", "textures/blocks/planks_big_oak.png");
			regBlock(Block::mIronFence, "Iron Bars", "textures/blocks/iron_bars.png");
			regBlock(Block::mLadder, "Ladder", "textures/blocks/ladder.png");
			regBlock(Block::mRail, "Rail", "textures/blocks/rail_normal.png");
			regBlock(Block::mGoldenRail, "Powered Rail", "textures/blocks/rail_golden.png");
			regBlock(Block::mDetectorRail, "Detector Rail", "textures/blocks/rail_detector.png");
			regBlock(Block::mActivatorRail, "Activator Rail", "textures/blocks/rail_activator.png");
			regBlock(Block::mGoldBlock, "Block of Gold", "textures/blocks/gold_block.png");
			regBlock(Block::mIronBlock, "Block of Iron", "textures/blocks/iron_block.png");
			regBlock(Block::mDiamondBlock, "Block of Diamond", "textures/blocks/diamond_block.png");
			regBlock(Block::mEmeraldBlock, "Block of Emerald", "textures/blocks/emerald_block.png");
			regBlock(Block::mRedstoneBlock, "Block of Redstone", "textures/blocks/redstone_block.png");
			regBlock(Block::mQuartzBlock, "Block of Quartz", "textures/blocks/quartz_block_side.png");
			regBlock(Block::mNetherBrick, "Nether Brick", "textures/blocks/nether_brick.png");
			regBlock(Block::mWeb, "Cobweb", "textures/blocks/web.png");
			regBlock(Block::mDeadBush, "Dead Bush", "textures/blocks/deadbush.png");
			regBlock(Block::mBrownMushroom, "Brown Mushroom", "textures/blocks/mushroom_brown.png");
			regBlock(Block::mRedMushroom, "Red Mushroom", "textures/blocks/mushroom_red.png");
			regBlock(Block::mLitPumpkin, "Jack o'Lantern", "textures/blocks/pumpkin_face_on.png");
			regBlock(Block::mTopSnow, "Snow", "textures/blocks/snow.png");
			regBlock(Block::mPackedIce, "Packed Ice", "textures/blocks/ice_packed.png");
			regBlock(Block::mMycelium, "Mycelium", "textures/blocks/mycelium_top.png");
			regBlock(Block::mPodzol, "Podzol", "textures/blocks/dirt_podzol_top.png");
			regBlock(Block::mHayBlock, "Hay Bale", "textures/blocks/hay_block_side.png");
			regBlock(Block::mWoolCarpet, "Carpet", "textures/blocks/wool_colored_white.png");
			regBlock(Block::mStainedClay, "Stained Clay", "textures/blocks/hardened_clay.png");
			regBlock(Block::mRedSandstone, "Red Sandstone", "textures/blocks/red_sandstone_normal.png");
			regBlock(Block::mPrismarine, "Prismarine", "textures/blocks/prismarine_rough.png");
			regBlock(Block::mSeaLantern, "Sea Lantern", "textures/blocks/sea_lantern.png");
			regBlock(Block::mEndStone, "End Stone", "textures/blocks/end_stone.png");
			regBlock(Block::mEndBrick, "End Stone Bricks", "textures/blocks/end_bricks.png");
			regBlock(Block::mPurpurBlock, "Purpur Block", "textures/blocks/purpur_block.png");
			regBlock(Block::mVine, "Vines", "textures/blocks/vine_carried.png");
			regBlock(Block::mWaterlily, "Lily Pad", "textures/blocks/waterlily.png");
			regBlock(Block::mAnvil, "Anvil", "textures/blocks/anvil_base.png");
			regBlock(Block::mTrappedChest, "Trapped Chest", "textures/blocks/chest_front.png");
			regBlock(Block::mOakStairs, "Oak Wood Stairs", "textures/blocks/planks_oak.png");
			regBlock(Block::mSpruceStairs, "Spruce Wood Stairs", "textures/blocks/planks_spruce.png");
			regBlock(Block::mBirchStairs, "Birch Wood Stairs", "textures/blocks/planks_birch.png");
			regBlock(Block::mJungleStairs, "Jungle Wood Stairs", "textures/blocks/planks_jungle.png");
			regBlock(Block::mAcaciaStairs, "Acacia Wood Stairs", "textures/blocks/planks_acacia.png");
			regBlock(Block::mDarkOakStairs, "Dark Oak Wood Stairs", "textures/blocks/planks_big_oak.png");
			regBlock(Block::mStoneStairs, "Cobblestone Stairs", "textures/blocks/cobblestone.png");
			regBlock(Block::mBrickStairs, "Brick Stairs", "textures/blocks/brick.png");
			regBlock(Block::mStoneBrickStairs, "Stone Brick Stairs", "textures/blocks/stonebrick.png");
			regBlock(Block::mSandstoneStairs, "Sandstone Stairs", "textures/blocks/sandstone_normal.png");
			regBlock(Block::mRedSandstoneStairs, "Red Sandstone Stairs", "textures/blocks/red_sandstone_normal.png");
			regBlock(Block::mNetherBrickStairs, "Nether Brick Stairs", "textures/blocks/nether_brick.png");
			regBlock(Block::mQuartzStairs, "Quartz Stairs", "textures/blocks/quartz_block_side.png");
			regBlock(Block::mLitRedStoneTorch, "Redstone Torch", "textures/blocks/redstone_torch_on.png");

			// Materials & misc items
			regItem(Item::mArrow, "Arrow", "textures/items/arrow.png");
			regItem(Item::mBed, "Bed", "textures/items/bed.png");
			regItem(Item::mStick, "Stick", "textures/items/stick.png");
			regItem(Item::mBowl, "Bowl", "textures/items/bowl.png");
			regItem(Item::mBow, "Bow", "textures/items/bow_standby.png");
			regItem(Item::mFlintAndSteel, "Flint and Steel", "textures/items/flint_and_steel.png");
			regItem(Item::mDoor_wood, "Oak Door", "textures/items/door_wood.png");
			regItem(Item::mDoor_spruce, "Spruce Door", "textures/items/door_spruce.png");
			regItem(Item::mDoor_birch, "Birch Door", "textures/items/door_birch.png");
			regItem(Item::mDoor_jungle, "Jungle Door", "textures/items/door_jungle.png");
			regItem(Item::mDoor_acacia, "Acacia Door", "textures/items/door_acacia.png");
			regItem(Item::mDoor_darkoak, "Dark Oak Door", "textures/items/door_dark_oak.png");
			regItem(Item::mDoor_iron, "Iron Door", "textures/items/door_iron.png");
			regItem(Item::mSign, "Sign", "textures/items/sign.png");
			regItem(Item::mPainting, "Painting", "textures/items/painting.png");
			regItem(Item::mBucket, "Bucket", "textures/items/bucket_empty.png");
			regItem(Item::mMinecart, "Minecart", "textures/items/minecart_normal.png");
			regItem(Item::mBoat, "Boat", "textures/items/boat.png");
			regItem(Item::mClock, "Clock", "textures/items/clock_item.png");
			regItem(Item::mCompass, "Compass", "textures/items/compass_item.png");
			regItem(Item::mSnowBall, "Snowball", "textures/items/snowball.png");
			regItem(Item::mEgg, "Egg", "textures/items/egg.png");
			regItem(Item::mCake, "Cake", "textures/items/cake.png");
			regItem(Item::mReeds, "Sugar Canes", "textures/items/reeds.png");
			regItem(Item::mSlimeBall, "Slimeball", "textures/items/slimeball.png");
			regItem(Item::mMobPlacer, "Spawn Egg", "textures/items/spawn_egg.png");
			regItem(Item::mHoe_wood, "Wooden Hoe", "textures/items/wood_hoe.png");
			regItem(Item::mHoe_stone, "Stone Hoe", "textures/items/stone_hoe.png");
			regItem(Item::mHoe_iron, "Iron Hoe", "textures/items/iron_hoe.png");
			regItem(Item::mHoe_diamond, "Diamond Hoe", "textures/items/diamond_hoe.png");
			regItem(Item::mHoe_gold, "Golden Hoe", "textures/items/gold_hoe.png");
			regItem(Item::mFishingRod, "Fishing Rod", "textures/items/fishing_rod_uncast.png");
			regBlock(Block::mFenceGateOak, "Oak Fence Gate", "textures/blocks/planks_oak.png");
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
			regItem(Item::mFish_raw_clownfish, "Clownfish", "textures/items/fish_clownfish_raw.png");
			regItem(Item::mFish_raw_pufferfish, "Pufferfish", "textures/items/fish_pufferfish_raw.png");
			regItem(Item::mChorusFruit, "Chorus Fruit", "textures/items/chorus_fruit.png");
			regItem(Item::mChorusFruitPopped, "Popped Chorus Fruit", "textures/items/chorus_fruit_popped.png");
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
	short aux = item.getAuxValue();
	if (Item::mCoal && id == Item::mCoal->getId() && aux == 1) {
		return "Charcoal";
	}
	if (Item::mDye_powder && id == Item::mDye_powder->getId()) {
		static const char* dyeNames[16] = {
			"Ink Sac", "Rose Red", "Cactus Green", "Cocoa Beans",
			"Lapis Lazuli", "Purple Dye", "Cyan Dye", "Light Gray Dye",
			"Gray Dye", "Pink Dye", "Lime Dye", "Dandelion Yellow",
			"Light Blue Dye", "Magenta Dye", "Orange Dye", "Bone Meal"
		};
		if (aux >= 0 && aux < 16) return dyeNames[aux];
	}
	if (Item::mBucket && id == Item::mBucket->getId()) {
		if (aux == 1) return "Milk Bucket";
		if (Block::mFlowingWater && aux == Block::mFlowingWater->mID) return "Water Bucket";
		if (Block::mFlowingLava && aux == Block::mFlowingLava->mID) return "Lava Bucket";
		return "Bucket";
	}
	if (Item::mMobPlacer && id == Item::mMobPlacer->getId()) {
		switch (aux) {
			case 10: return "Spawn Chicken";
			case 11: return "Spawn Cow";
			case 12: return "Spawn Pig";
			case 13: return "Spawn Sheep";
			case 32: return "Spawn Zombie";
			case 33: return "Spawn Creeper";
			case 34: return "Spawn Skeleton";
			case 35: return "Spawn Spider";
			default: return "Spawn Egg";
		}
	}
	if (Block::mWool && id == Block::mWool->getId()) {
		static const char* woolColors[16] = {
			"White Wool", "Orange Wool", "Magenta Wool", "Light Blue Wool",
			"Yellow Wool", "Lime Wool", "Pink Wool", "Gray Wool",
			"Light Gray Wool", "Cyan Wool", "Purple Wool", "Blue Wool",
			"Brown Wool", "Green Wool", "Red Wool", "Black Wool"
		};
		if (aux >= 0 && aux < 16) return woolColors[aux];
	}
	if (Block::mWoodPlanks && id == Block::mWoodPlanks->getId()) {
		static const char* plankNames[6] = {
			"Oak Planks", "Spruce Planks", "Birch Planks", "Jungle Planks", "Acacia Planks", "Dark Oak Planks"
		};
		if (aux >= 0 && aux < 6) return plankNames[aux];
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
	short aux = item.getAuxValue();
	if (Item::mCoal && id == Item::mCoal->getId() && aux == 1) {
		return "textures/items/charcoal.png";
	}
	if (Item::mDye_powder && id == Item::mDye_powder->getId()) {
		static const char* dyeTex[16] = {
			"textures/items/dye_powder_black.png", "textures/items/dye_powder_red.png",
			"textures/items/dye_powder_green.png", "textures/items/dye_powder_brown.png",
			"textures/items/dye_powder_blue.png", "textures/items/dye_powder_purple.png",
			"textures/items/dye_powder_cyan.png", "textures/items/dye_powder_silver.png",
			"textures/items/dye_powder_gray.png", "textures/items/dye_powder_pink.png",
			"textures/items/dye_powder_lime.png", "textures/items/dye_powder_yellow.png",
			"textures/items/dye_powder_light_blue.png", "textures/items/dye_powder_magenta.png",
			"textures/items/dye_powder_orange.png", "textures/items/dye_powder_white.png"
		};
		if (aux >= 0 && aux < 16) return dyeTex[aux];
	}
	if (Item::mBucket && id == Item::mBucket->getId()) {
		if (aux == 1) return "textures/items/bucket_milk.png";
		if (Block::mFlowingWater && aux == Block::mFlowingWater->mID) return "textures/items/bucket_water.png";
		if (Block::mFlowingLava && aux == Block::mFlowingLava->mID) return "textures/items/bucket_lava.png";
		return "textures/items/bucket_empty.png";
	}
	if (Item::mMobPlacer && id == Item::mMobPlacer->getId()) {
		switch (aux) {
			case 10: return "textures/items/egg_chicken.png";
			case 11: return "textures/items/egg_cow.png";
			case 12: return "textures/items/egg_pig.png";
			case 13: return "textures/items/egg_sheep.png";
			case 32: return "textures/items/egg_zombie.png";
			case 33: return "textures/items/egg_creeper.png";
			case 34: return "textures/items/egg_skeleton.png";
			case 35: return "textures/items/egg_spider.png";
			default: return "textures/items/spawn_egg.png";
		}
	}
	if ((Block::mWool && id == Block::mWool->getId()) || (Block::mWoolCarpet && id == Block::mWoolCarpet->getId())) {
		static const char* woolTex[16] = {
			"textures/blocks/wool_colored_white.png", "textures/blocks/wool_colored_orange.png",
			"textures/blocks/wool_colored_magenta.png", "textures/blocks/wool_colored_light_blue.png",
			"textures/blocks/wool_colored_yellow.png", "textures/blocks/wool_colored_lime.png",
			"textures/blocks/wool_colored_pink.png", "textures/blocks/wool_colored_gray.png",
			"textures/blocks/wool_colored_silver.png", "textures/blocks/wool_colored_cyan.png",
			"textures/blocks/wool_colored_purple.png", "textures/blocks/wool_colored_blue.png",
			"textures/blocks/wool_colored_brown.png", "textures/blocks/wool_colored_green.png",
			"textures/blocks/wool_colored_red.png", "textures/blocks/wool_colored_black.png"
		};
		if (aux >= 0 && aux < 16) return woolTex[aux];
	}
	if ((Block::mWoodPlanks && id == Block::mWoodPlanks->getId()) || (Block::mWoodenSlab && id == Block::mWoodenSlab->getId())) {
		static const char* plankTex[6] = {
			"textures/blocks/planks_oak.png", "textures/blocks/planks_spruce.png",
			"textures/blocks/planks_birch.png", "textures/blocks/planks_jungle.png",
			"textures/blocks/planks_acacia.png", "textures/blocks/planks_big_oak.png"
		};
		if (aux >= 0 && aux < 6) return plankTex[aux];
	}
	if (Block::mLog && id == Block::mLog->getId()) {
		static const char* logTex[4] = {
			"textures/blocks/log_oak.png", "textures/blocks/log_spruce.png",
			"textures/blocks/log_birch.png", "textures/blocks/log_jungle.png"
		};
		if (aux >= 0 && aux < 4) return logTex[aux];
	}
	if (Block::mLog2 && id == Block::mLog2->getId()) {
		return (aux == 1) ? "textures/blocks/log_big_oak.png" : "textures/blocks/log_acacia.png";
	}
	if (Block::mStoneBrick && id == Block::mStoneBrick->getId()) {
		if (aux == 1) return "textures/blocks/stonebrick_mossy.png";
		if (aux == 2) return "textures/blocks/stonebrick_cracked.png";
		if (aux == 3) return "textures/blocks/stonebrick_carved.png";
	}
	if (Block::mQuartzBlock && id == Block::mQuartzBlock->getId()) {
		if (aux == 1) return "textures/blocks/quartz_block_chiseled.png";
		if (aux == 2) return "textures/blocks/quartz_block_lines.png";
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
	    (Block::mLitRedStoneTorch && id == Block::mLitRedStoneTorch->getId()) ||
	    (Block::mGlassPane && id == Block::mGlassPane->getId()) ||
	    (Block::mSapling && id == Block::mSapling->getId()) ||
	    (Block::mYellowFlower && id == Block::mYellowFlower->getId()) ||
	    (Block::mRedFlower && id == Block::mRedFlower->getId()) ||
	    (Block::mTallgrass && id == Block::mTallgrass->getId()) ||
	    (Block::mDeadBush && id == Block::mDeadBush->getId()) ||
	    (Block::mBrownMushroom && id == Block::mBrownMushroom->getId()) ||
	    (Block::mRedMushroom && id == Block::mRedMushroom->getId()) ||
	    (Block::mWeb && id == Block::mWeb->getId()) ||
	    (Block::mLadder && id == Block::mLadder->getId()) ||
	    (Block::mRail && id == Block::mRail->getId()) ||
	    (Block::mGoldenRail && id == Block::mGoldenRail->getId()) ||
	    (Block::mDetectorRail && id == Block::mDetectorRail->getId()) ||
	    (Block::mActivatorRail && id == Block::mActivatorRail->getId()) ||
	    (Block::mIronFence && id == Block::mIronFence->getId()) ||
	    (Block::mVine && id == Block::mVine->getId()) ||
	    (Block::mWaterlily && id == Block::mWaterlily->getId()) ||
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
		int attackDamage;
	};
	static const ToolEntry tools[] = {
		{ Item::mPickAxe_wood, ToolType::Pickaxe, 0, 2.0f, 59, 2 },
		{ Item::mPickAxe_stone, ToolType::Pickaxe, 1, 4.0f, 131, 3 },
		{ Item::mPickAxe_iron, ToolType::Pickaxe, 2, 6.0f, 250, 4 },
		{ Item::mPickAxe_diamond, ToolType::Pickaxe, 3, 8.0f, 1561, 5 },
		{ Item::mPickAxe_gold, ToolType::Pickaxe, 0, 12.0f, 32, 2 },

		{ Item::mHatchet_wood, ToolType::Axe, 0, 2.0f, 59, 3 },
		{ Item::mHatchet_stone, ToolType::Axe, 1, 4.0f, 131, 4 },
		{ Item::mHatchet_iron, ToolType::Axe, 2, 6.0f, 250, 5 },
		{ Item::mHatchet_diamond, ToolType::Axe, 3, 8.0f, 1561, 6 },
		{ Item::mHatchet_gold, ToolType::Axe, 0, 12.0f, 32, 3 },

		{ Item::mShovel_wood, ToolType::Shovel, 0, 2.0f, 59, 1 },
		{ Item::mShovel_stone, ToolType::Shovel, 1, 4.0f, 131, 2 },
		{ Item::mShovel_iron, ToolType::Shovel, 2, 6.0f, 250, 3 },
		{ Item::mShovel_diamond, ToolType::Shovel, 3, 8.0f, 1561, 4 },
		{ Item::mShovel_gold, ToolType::Shovel, 0, 12.0f, 32, 1 },

		{ Item::mSword_wood, ToolType::Sword, 0, 1.5f, 59, 4 },
		{ Item::mSword_stone, ToolType::Sword, 1, 1.5f, 131, 5 },
		{ Item::mSword_iron, ToolType::Sword, 2, 1.5f, 250, 6 },
		{ Item::mSword_diamond, ToolType::Sword, 3, 1.5f, 1561, 7 },
		{ Item::mSword_gold, ToolType::Sword, 0, 1.5f, 32, 4 },

		{ Item::mShears, ToolType::Shears, 0, 5.0f, 238, 1 },
	};
	for (const auto& t : tools) {
		if (t.item && id == t.item->getId()) {
			info.type = t.type;
			info.harvestLevel = t.harvestLevel;
			info.speedMultiplier = t.speed;
			info.maxDurability = t.maxDurability;
			info.attackDamage = t.attackDamage;
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
	const Item* it = item.getItem();
	if (!it) return info;

	if (FoodItemComponent* food = it->getFood()) {
		info.isFood = true;
		info.nutrition = food->getNutrition();
		info.saturation = static_cast<float>(food->getNutrition()) * food->getSaturationModifier() * 2.0f;
		info.healHealth = 0;
		info.alwaysEdible = (it == Item::mApple_gold || it == Item::mApple_enchanted);
		info.returnsBowl = (food->getUsingConvertsTo() == "bowl");
		return info;
	}
	if (Item::mChorusFruit && it == Item::mChorusFruit) {
		info.isFood = true;
		info.nutrition = 4;
		info.saturation = 4.0f * 0.3f * 2.0f;
		info.alwaysEdible = true;
		return info;
	}
	return info;
}

static std::string normalizeItemKey(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for (unsigned char c : s) {
		if (c == ' ' || c == '-') out.push_back('_');
		else out.push_back(static_cast<char>(std::tolower(c)));
	}
	if (out.rfind("minecraft:", 0) == 0) {
		out = out.substr(10);
	}
	return out;
}

ItemInstance findItemOrBlockByName(const std::string& query, int count, int aux) {
	Recipes::getInstance()->init();
	std::string key = normalizeItemKey(query);
	if (key.empty()) return ItemInstance();

	// Numeric ID lookup
	bool allDigits = true;
	for (unsigned char c : key) {
		if (!std::isdigit(c)) { allDigits = false; break; }
	}
	if (allDigits) {
		int id = std::atoi(key.c_str());
		if (id > 0 && id < 256 && Block::mBlocks[id]) {
			return ItemInstance(Block::mBlocks[id], count, aux);
		}
		if (id >= 256 && id < 4096 && Item::mItems[id]) {
			return ItemInstance(Item::mItems[id], count, aux);
		}
		return ItemInstance();
	}

	for (int id = 1; id < 256; ++id) {
		if (Block::mBlocks[id]) {
			ItemInstance cand(Block::mBlocks[id], count, aux);
			if (normalizeItemKey(getItemDisplayName(cand)) == key) return cand;
		}
	}
	for (int id = 256; id < 512; ++id) {
		if (Item::mItems[id]) {
			ItemInstance cand(Item::mItems[id], count, aux);
			if (normalizeItemKey(getItemDisplayName(cand)) == key) return cand;
		}
	}
	return ItemInstance();
}
