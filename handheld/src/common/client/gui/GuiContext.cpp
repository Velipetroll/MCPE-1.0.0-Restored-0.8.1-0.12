#include "GuiContext.h"
#include "Screen.h"
#include "StartMenuScreen.h"
#include "PauseScreen.h"
#include "DeathScreen.h"
#include "Options.h"
#include "world/crafting/CraftingRecipes.h"
#include "world/item/Item.h"
#include "world/level/block/Block.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

void GuiContext::ensureInventoryInitialized() {
	if (inventoryInitialized) return;
	inventoryInitialized = true;

	Recipes::getInstance()->init();

	// Phase 7 test items in hotbar (bow, arrows, TNT, flint & steel, doors, torch, sword, food)
	if (Item::mBow) inventorySlots[0] = ItemInstance(Item::mBow, 1, 0);
	if (Item::mArrow) inventorySlots[1] = ItemInstance(Item::mArrow, 64, 0);
	if (Block::mTNT) inventorySlots[2] = ItemInstance(Block::mTNT, 16, 0);
	if (Item::mFlintAndSteel) inventorySlots[3] = ItemInstance(Item::mFlintAndSteel, 1, 0);
	if (Item::mDoor_wood) inventorySlots[4] = ItemInstance(Item::mDoor_wood, 4, 0);
	if (Block::mFenceGateOak) inventorySlots[5] = ItemInstance(Block::mFenceGateOak, 4, 0);
	if (Block::mTorch) inventorySlots[6] = ItemInstance(Block::mTorch, 32, 0);
	if (Item::mSword_iron) inventorySlots[7] = ItemInstance(Item::mSword_iron, 1, 0);
	if (Item::mApple) inventorySlots[8] = ItemInstance(Item::mApple, 16, 0);
}

ItemInstance GuiContext::addItemToInventoryRemainder(const ItemInstance& item) {
	if (item.isNull() || item.getStackSize() <= 0) return ItemInstance();
	ensureInventoryInitialized();

	ItemInstance rem(item);
	int maxStack = std::max(1, static_cast<int>(rem.getMaxStackSize()));

	// First pass: stack into existing matching slots
	for (auto& slot : inventorySlots) {
		if (!slot.isNull() && slot.sameItemAndAux(&rem) && slot.hasSameUserData(rem) && slot.getStackSize() < maxStack) {
			int space = maxStack - static_cast<int>(slot.getStackSize());
			int move = std::min(space, static_cast<int>(rem.getStackSize()));
			slot.add(move);
			rem.remove(move);
			if (rem.isNull() || rem.getStackSize() <= 0) return ItemInstance();
		}
	}

	// Second pass: empty slots
	for (auto& slot : inventorySlots) {
		if (slot.isNull()) {
			int move = std::min(maxStack, static_cast<int>(rem.getStackSize()));
			slot = rem;
			slot.set(move);
			rem.remove(move);
			if (rem.isNull() || rem.getStackSize() <= 0) return ItemInstance();
		}
	}

	return rem;
}

bool GuiContext::addItemToInventory(const ItemInstance& item) {
	ItemInstance rem = addItemToInventoryRemainder(item);
	return rem.isNull() || rem.getStackSize() <= 0;
}

void GuiContext::resetSurvivalStats() {
	health = 20;
	hunger = 20;
	saturation = 5.0f;
	exhaustion = 0.0f;
	healTimer = 0.0f;
	starveTimer = 0.0f;
	hurtFlashTimer = 0.0f;
}

int GuiContext::applyDamage(int rawDamage, bool ignoreArmor) {
	if (rawDamage <= 0 || isCreative) return 0;

	int finalDamage = rawDamage;
	if (!ignoreArmor) {
		int defense = getTotalArmorDefense(armorSlots);
		if (defense > 0) {
			float mult = 1.0f - static_cast<float>(defense) * 0.04f;
			finalDamage = std::max(1, static_cast<int>(std::round(static_cast<float>(rawDamage) * mult)));

			int armorWear = std::max(1, rawDamage / 4);
			for (auto& piece : armorSlots) {
				if (piece.isNull()) continue;
				int maxDur = getItemMaxDurability(piece);
				if (maxDur > 0) {
					int nextAux = static_cast<int>(piece.getAuxValue()) + armorWear;
					if (nextAux > maxDur) {
						piece.setNull();
					} else {
						piece.setAuxValue(static_cast<short>(nextAux));
					}
				}
			}
		}
	}

	health = std::max(0, health - finalDamage);
	hurtFlashTimer = 0.40f;
	exhaustion = std::min(40.0f, exhaustion + 0.1f);
	return finalDamage;
}

void GuiContext::setScreen(Screen* screen) {
	if (currentScreen == screen) return;
	delete currentScreen;
	currentScreen = screen;
	if (currentScreen) {
		currentScreen->init(this, guiWidth, guiHeight);
	}
}

void GuiContext::startGame() {
	if (state == GameState::StartMenu) {
		resetSurvivalStats();
	}
	state = GameState::InGame;
	setScreen(nullptr);
}

void GuiContext::pauseGame() {
	if (state == GameState::InGame) {
		state = GameState::Paused;
		setScreen(new PauseScreen());
	}
}

void GuiContext::resumeGame() {
	if (state == GameState::Paused || currentScreen != nullptr) {
		state = GameState::InGame;
		setScreen(nullptr);
	}
}

void GuiContext::quitToTitle() {
	if (hasWorldLoaded) {
		worldSaveAndCloseRequested = true;
	}
	state = GameState::StartMenu;
	setScreen(new StartMenuScreen());
}

void GuiContext::requestQuit() {
	shouldQuit = true;
}

void GuiContext::addChatMessage(const std::string& msg) {
	ChatMessage entry;
	entry.rawText = msg;
	entry.formattedText = msg;
	entry.age = 0;
	entry.maxAge = 200;
	chatMessages.insert(chatMessages.begin(), entry);
	while (chatMessages.size() > 30) {
		chatMessages.pop_back();
	}

	chatHistory.push_back(msg);
	while (chatHistory.size() > 30) {
		chatHistory.erase(chatHistory.begin());
	}
}

void GuiContext::tickChatMessages() {
	static float accum = 0.0f;
	accum += deltaTime;
	while (accum >= 0.05f) {
		accum -= 0.05f;
		for (auto& m : chatMessages) {
			++m.age;
		}
	}
}

static std::string toLowerStr(std::string s) {
	for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

static float parseRelCoord(const std::string& tok, float base, bool* ok = nullptr) {
	if (tok.empty()) { if (ok) *ok = false; return base; }
	if (tok[0] == '~') {
		if (tok.size() == 1) { if (ok) *ok = true; return base; }
		char* endp = nullptr;
		float off = std::strtof(tok.c_str() + 1, &endp);
		if (endp == tok.c_str() + 1) { if (ok) *ok = false; return base; }
		if (ok) *ok = true;
		return base + off;
	}
	char* endp = nullptr;
	float val = std::strtof(tok.c_str(), &endp);
	if (endp == tok.c_str()) { if (ok) *ok = false; return base; }
	if (ok) *ok = true;
	return val;
}

void GuiContext::executeCommand(const std::string& cmdLine) {
	if (cmdLine.empty() || cmdLine[0] != '/') return;
	std::vector<std::string> args;
	{
		std::istringstream iss(cmdLine.substr(1));
		std::string tok;
		while (iss >> tok) args.push_back(tok);
	}
	if (args.empty()) return;

	std::string cmd = toLowerStr(args[0]);
	if (cmd == "help" || cmd == "?") {
		addChatMessage("Commands: /help, /gamemode <0|1>, /time <set|add> <val>,");
		addChatMessage("/give <item> [count] [aux], /clear, /tp <x> <y> <z>,");
		addChatMessage("/spawnpoint [x y z], /kill, /heal, /summon <mob> [x y z],");
		addChatMessage("/setblock <x> <y> <z> <block>, /difficulty <0-3>, /say, /me, /seed");
	} else if (cmd == "gamemode" || cmd == "gm") {
		if (args.size() < 2) { addChatMessage("Usage: /gamemode <0|1|survival|creative>"); return; }
		std::string m = toLowerStr(args[1]);
		if (m == "1" || m == "c" || m == "creative") {
			isCreative = true;
			addChatMessage("Set game mode to Creative");
		} else if (m == "0" || m == "s" || m == "survival") {
			isCreative = false;
			addChatMessage("Set game mode to Survival");
		} else {
			addChatMessage("Unknown gamemode: " + args[1]);
		}
	} else if (cmd == "time") {
		if (args.size() < 3) { addChatMessage("Usage: /time <set|add> <value>"); return; }
		std::string sub = toLowerStr(args[1]);
		std::string val = toLowerStr(args[2]);
		if (sub == "set") {
			if (val == "day") worldTime = 1000;
			else if (val == "noon") worldTime = 6000;
			else if (val == "sunset" || val == "dusk") worldTime = 12000;
			else if (val == "night") worldTime = 13000;
			else if (val == "midnight") worldTime = 18000;
			else if (val == "sunrise" || val == "dawn") worldTime = 23000;
			else worldTime = std::max<int64_t>(0, std::atoll(val.c_str()));
			addChatMessage("Set time to " + std::to_string(worldTime));
		} else if (sub == "add") {
			int64_t delta = std::atoll(val.c_str());
			worldTime = std::max<int64_t>(0, worldTime + delta);
			addChatMessage("Added " + std::to_string(delta) + " to time (" + std::to_string(worldTime) + ")");
		} else {
			addChatMessage("Usage: /time <set|add> <value>");
		}
	} else if (cmd == "give") {
		if (args.size() < 2) { addChatMessage("Usage: /give <item> [count] [aux]"); return; }
		int count = (args.size() >= 3) ? std::max(1, std::min(64, std::atoi(args[2].c_str()))) : 1;
		int aux = (args.size() >= 4) ? std::atoi(args[3].c_str()) : 0;
		ItemInstance item = findItemOrBlockByName(args[1], count, aux);
		if (item.isNull()) {
			addChatMessage("Unknown item: " + args[1]);
			return;
		}
		ItemInstance rem = addItemToInventoryRemainder(item);
		int given = count - (rem.isNull() ? 0 : static_cast<int>(rem.getStackSize()));
		addChatMessage("Gave " + std::to_string(given) + " x " + getItemDisplayName(item));
	} else if (cmd == "clear") {
		for (auto& s : inventorySlots) s.setNull();
		for (auto& a : armorSlots) a.setNull();
		addChatMessage("Cleared inventory");
	} else if (cmd == "tp" || cmd == "teleport") {
		if (args.size() < 4) { addChatMessage("Usage: /tp <x> <y> <z>"); return; }
		bool okX = false, okY = false, okZ = false;
		float tx = parseRelCoord(args[1], playerX, &okX);
		float ty = parseRelCoord(args[2], playerY, &okY);
		float tz = parseRelCoord(args[3], playerZ, &okZ);
		if (!okX || !okY || !okZ) { addChatMessage("Invalid coordinates"); return; }
		pendingTeleport = true;
		pendingTpX = tx;
		pendingTpY = ty;
		pendingTpZ = tz;
		addChatMessage("Teleported to " + std::to_string(static_cast<int>(tx)) + ", " + std::to_string(static_cast<int>(ty)) + ", " + std::to_string(static_cast<int>(tz)));
	} else if (cmd == "spawnpoint") {
		float sx = playerX, sy = playerY, sz = playerZ;
		if (args.size() >= 4) {
			sx = parseRelCoord(args[1], playerX);
			sy = parseRelCoord(args[2], playerY);
			sz = parseRelCoord(args[3], playerZ);
		}
		hasBedSpawn = true;
		bedSpawnX = static_cast<int>(std::floor(sx));
		bedSpawnY = static_cast<int>(std::floor(sy));
		bedSpawnZ = static_cast<int>(std::floor(sz));
		addChatMessage("Set spawnpoint to " + std::to_string(bedSpawnX) + ", " + std::to_string(bedSpawnY) + ", " + std::to_string(bedSpawnZ));
	} else if (cmd == "kill") {
		health = 0;
		addChatMessage("Ouch! That looked like it hurt.");
		setScreen(new DeathScreen());
	} else if (cmd == "heal") {
		health = 20;
		hunger = 20;
		saturation = 5.0f;
		addChatMessage("Restored health and hunger");
	} else if (cmd == "summon") {
		if (args.size() < 2) { addChatMessage("Usage: /summon <pig|cow|sheep|chicken|zombie|skeleton|creeper> [x y z]"); return; }
		float sx = playerX + 2.0f, sy = playerY, sz = playerZ + 2.0f;
		if (args.size() >= 5) {
			sx = parseRelCoord(args[2], playerX);
			sy = parseRelCoord(args[3], playerY);
			sz = parseRelCoord(args[4], playerZ);
		}
		pendingMobSpawns.push_back({ toLowerStr(args[1]), sx, sy, sz });
		addChatMessage("Summoned " + args[1]);
	} else if (cmd == "setblock") {
		if (args.size() < 5) { addChatMessage("Usage: /setblock <x> <y> <z> <block>"); return; }
		int bx = static_cast<int>(std::floor(parseRelCoord(args[1], playerX)));
		int by = static_cast<int>(std::floor(parseRelCoord(args[2], playerY)));
		int bz = static_cast<int>(std::floor(parseRelCoord(args[3], playerZ)));
		ItemInstance blk = findItemOrBlockByName(args[4], 1, 0);
		if (blk.isNull() || blk.getId() <= 0 || blk.getId() >= 256) {
			addChatMessage("Unknown block: " + args[4]);
			return;
		}
		pendingSetBlocks.push_back({ bx, by, bz, blk.getId(), 0 });
		addChatMessage("Placed " + getItemDisplayName(blk) + " at " + std::to_string(bx) + ", " + std::to_string(by) + ", " + std::to_string(bz));
	} else if (cmd == "difficulty") {
		if (args.size() < 2 || !options) { addChatMessage("Usage: /difficulty <0|1|2|3|peaceful|easy|normal|hard>"); return; }
		std::string d = toLowerStr(args[1]);
		if (d == "0" || d == "p" || d == "peaceful") options->difficulty = 0;
		else if (d == "1" || d == "e" || d == "easy") options->difficulty = 1;
		else if (d == "2" || d == "n" || d == "normal") options->difficulty = 2;
		else if (d == "3" || d == "h" || d == "hard") options->difficulty = 3;
		else { addChatMessage("Unknown difficulty: " + args[1]); return; }
		addChatMessage("Set difficulty to " + std::to_string(options->difficulty));
	} else if (cmd == "weather") {
		if (args.size() < 2) { addChatMessage("Usage: /weather <clear|rain|thunder>"); return; }
		addChatMessage("Changing to " + toLowerStr(args[1]) + " weather");
	} else if (cmd == "say") {
		std::string text = (cmdLine.size() > 5) ? cmdLine.substr(5) : "";
		addChatMessage("[Server] " + text);
	} else if (cmd == "me") {
		std::string name = options ? options->username : "Player";
		std::string text = (cmdLine.size() > 4) ? cmdLine.substr(4) : "";
		addChatMessage("* " + name + " " + text);
	} else if (cmd == "seed") {
		addChatMessage("World: " + (activeWorldName.empty() ? std::string("Default") : activeWorldName));
	} else {
		addChatMessage("Unknown command: /" + cmd + " (type /help)");
	}
}
