#include "WorldPersistence.h"
#include "world/crafting/CraftingRecipes.h"
#include <json/json.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>

namespace {

Json::Value serializeItem(const ItemInstance& item) {
	Json::Value obj(Json::objectValue);
	if (item.isNull() || item.getStackSize() <= 0) {
		obj["id"] = 0;
		obj["aux"] = 0;
		obj["count"] = 0;
		return obj;
	}
	obj["id"] = item.getId();
	obj["aux"] = static_cast<int>(item.getAuxValue());
	obj["count"] = static_cast<int>(item.getStackSize());
	return obj;
}

ItemInstance deserializeItem(const Json::Value& obj) {
	if (!obj.isObject()) return ItemInstance();
	int id = obj.get("id", 0).asInt();
	int aux = obj.get("aux", 0).asInt();
	int count = obj.get("count", 0).asInt();
	if (id <= 0 || count <= 0) return ItemInstance();
	ItemInstance inst(id, count, aux);
	if (inst.isNull() || !inst.isValid()) return ItemInstance();
	return inst;
}

bool isAllDigitsOrNeg(const std::string& s) {
	if (s.empty()) return false;
	size_t start = (s[0] == '-' || s[0] == '+') ? 1 : 0;
	if (start >= s.size()) return false;
	for (size_t i = start; i < s.size(); ++i) {
		if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
	}
	return true;
}

} // namespace

int64_t parseSeedString(const std::string& seedText) {
	std::string trimmed = seedText;
	while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
		trimmed.erase(trimmed.begin());
	}
	while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
		trimmed.pop_back();
	}

	if (trimmed.empty()) {
		static std::mt19937_64 rng(static_cast<uint64_t>(
			std::chrono::high_resolution_clock::now().time_since_epoch().count()));
		int64_t val = static_cast<int64_t>(rng() & 0x7FFFFFFFULL);
		return val == 0 ? 1337 : val;
	}

	if (isAllDigitsOrNeg(trimmed)) {
		try {
			return std::stoll(trimmed);
		} catch (...) {
			// Fall through to hash
		}
	}

	// Java String.hashCode() compatible seed hashing
	int32_t h = 0;
	for (unsigned char c : trimmed) {
		h = static_cast<int32_t>(static_cast<uint32_t>(h) * 31u + c);
	}
	return static_cast<int64_t>(h);
}

std::string sanitizeWorldFolderName(const std::string& rawName) {
	std::string out;
	out.reserve(rawName.size());
	for (unsigned char c : rawName) {
		if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
		    c == '"' || c == '<' || c == '>' || c == '|' || c < 32) {
			out.push_back('_');
		} else {
			out.push_back(static_cast<char>(c));
		}
	}
	while (!out.empty() && (out.front() == ' ' || out.front() == '.')) out.erase(out.begin());
	while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
	if (out.empty()) out = "New World";
	return out;
}

std::string getWorldDbPath(const std::string& worldDir) {
	if (worldDir == "./restore" || worldDir == "restore") {
		return "./restore/mcworld";
	}
	return worldDir;
}

bool loadWorldMetadata(const std::string& worldDir, WorldMetadata& outMeta) {
	namespace fs = std::filesystem;
	std::string metaPath = worldDir + "/world_meta.json";
	if (!fs::exists(metaPath)) {
		return false;
	}

	std::ifstream ifs(metaPath, std::ios::binary);
	if (!ifs.is_open()) {
		return false;
	}

	Json::Value root;
	Json::Reader reader;
	if (!reader.parse(ifs, root, false) || !root.isObject()) {
		return false;
	}

	Recipes::getInstance()->init();

	if (root.isMember("name") && root["name"].isString()) {
		outMeta.name = root["name"].asString();
	}
	if (root.isMember("seed")) {
		outMeta.seed = root["seed"].asInt64();
	}
	if (root.isMember("gameMode") && root["gameMode"].isString()) {
		outMeta.isCreative = (root["gameMode"].asString() == "creative");
	} else if (root.isMember("isCreative")) {
		outMeta.isCreative = root["isCreative"].asBool();
	}

	outMeta.health = std::max(0, std::min(20, root.get("health", 20).asInt()));
	outMeta.hunger = std::max(0, std::min(20, root.get("hunger", 20).asInt()));
	outMeta.saturation = std::max(0.0f, std::min(20.0f, root.get("saturation", 5.0f).asFloat()));
	outMeta.selectedHotbarSlot = std::max(0, std::min(8, root.get("selectedHotbarSlot", 0).asInt()));

	const Json::Value& player = root["player"];
	if (player.isObject()) {
		outMeta.hasPlayerPos = player.get("hasPos", false).asBool();
		outMeta.playerX = player.get("x", 0.5f).asFloat();
		outMeta.playerY = player.get("y", 80.0f).asFloat();
		outMeta.playerZ = player.get("z", 0.5f).asFloat();
		outMeta.playerYaw = player.get("yaw", 0.0).asDouble();
		outMeta.playerPitch = player.get("pitch", 0.0).asDouble();
	}

	for (auto& s : outMeta.inventorySlots) s.setNull();
	for (auto& a : outMeta.armorSlots) a.setNull();

	const Json::Value& inv = root["inventory"];
	if (inv.isArray()) {
		outMeta.hasInventory = true;
		for (const auto& entry : inv) {
			if (!entry.isObject()) continue;
			int slot = entry.get("slot", -1).asInt();
			if (slot >= 0 && slot < 36) {
				outMeta.inventorySlots[static_cast<size_t>(slot)] = deserializeItem(entry);
			}
		}
	}

	const Json::Value& armor = root["armor"];
	if (armor.isArray()) {
		for (const auto& entry : armor) {
			if (!entry.isObject()) continue;
			int slot = entry.get("slot", -1).asInt();
			if (slot >= 0 && slot < 4) {
				outMeta.armorSlots[static_cast<size_t>(slot)] = deserializeItem(entry);
			}
		}
	}

	outMeta.chests.clear();
	const Json::Value& chests = root["chests"];
	if (chests.isArray()) {
		for (const auto& c : chests) {
			if (!c.isObject()) continue;
			BlockCoordKey key{ c.get("x", 0).asInt(), c.get("y", 0).asInt(), c.get("z", 0).asInt() };
			ChestData data;
			const Json::Value& slots = c["slots"];
			if (slots.isArray()) {
				for (const auto& s : slots) {
					if (!s.isObject()) continue;
					int slotIdx = s.get("slot", -1).asInt();
					if (slotIdx >= 0 && slotIdx < 27) {
						data.slots[static_cast<size_t>(slotIdx)] = deserializeItem(s);
					}
				}
			}
			outMeta.chests[key] = std::move(data);
		}
	}

	outMeta.furnaces.clear();
	const Json::Value& furnaces = root["furnaces"];
	if (furnaces.isArray()) {
		for (const auto& f : furnaces) {
			if (!f.isObject()) continue;
			BlockCoordKey key{ f.get("x", 0).asInt(), f.get("y", 0).asInt(), f.get("z", 0).asInt() };
			FurnaceData data;
			data.burnTime = f.get("burnTime", 0.0f).asFloat();
			data.burnDuration = f.get("burnDuration", 0.0f).asFloat();
			data.cookTime = f.get("cookTime", 0.0f).asFloat();
			data.input = deserializeItem(f["input"]);
			data.fuel = deserializeItem(f["fuel"]);
			data.output = deserializeItem(f["output"]);
			outMeta.furnaces[key] = std::move(data);
		}
	}

	return true;
}

bool saveWorldMetadata(const std::string& worldDir, const WorldMetadata& meta) {
	namespace fs = std::filesystem;
	std::error_code ec;
	fs::create_directories(worldDir, ec);

	Json::Value root(Json::objectValue);
	root["name"] = meta.name;
	root["seed"] = static_cast<Json::Int64>(meta.seed);
	root["gameMode"] = meta.isCreative ? "creative" : "survival";
	root["isCreative"] = meta.isCreative;
	root["health"] = meta.health;
	root["hunger"] = meta.hunger;
	root["saturation"] = meta.saturation;
	root["selectedHotbarSlot"] = meta.selectedHotbarSlot;

	Json::Value player(Json::objectValue);
	player["hasPos"] = meta.hasPlayerPos;
	player["x"] = meta.playerX;
	player["y"] = meta.playerY;
	player["z"] = meta.playerZ;
	player["yaw"] = meta.playerYaw;
	player["pitch"] = meta.playerPitch;
	root["player"] = player;

	if (meta.hasInventory) {
		Json::Value inv(Json::arrayValue);
		for (int i = 0; i < 36; ++i) {
			const ItemInstance& item = meta.inventorySlots[static_cast<size_t>(i)];
			if (!item.isNull() && item.getStackSize() > 0) {
				Json::Value entry = serializeItem(item);
				entry["slot"] = i;
				inv.append(entry);
			}
		}
		root["inventory"] = inv;
	}

	Json::Value armor(Json::arrayValue);
	for (int i = 0; i < 4; ++i) {
		const ItemInstance& item = meta.armorSlots[static_cast<size_t>(i)];
		if (!item.isNull() && item.getStackSize() > 0) {
			Json::Value entry = serializeItem(item);
			entry["slot"] = i;
			armor.append(entry);
		}
	}
	root["armor"] = armor;

	Json::Value chests(Json::arrayValue);
	for (const auto& kv : meta.chests) {
		Json::Value slots(Json::arrayValue);
		for (int i = 0; i < 27; ++i) {
			const ItemInstance& item = kv.second.slots[static_cast<size_t>(i)];
			if (!item.isNull() && item.getStackSize() > 0) {
				Json::Value entry = serializeItem(item);
				entry["slot"] = i;
				slots.append(entry);
			}
		}
		if (slots.empty()) continue;
		Json::Value c(Json::objectValue);
		c["x"] = kv.first.x;
		c["y"] = kv.first.y;
		c["z"] = kv.first.z;
		c["slots"] = slots;
		chests.append(c);
	}
	root["chests"] = chests;

	Json::Value furnaces(Json::arrayValue);
	for (const auto& kv : meta.furnaces) {
		const FurnaceData& fd = kv.second;
		if (fd.input.isNull() && fd.fuel.isNull() && fd.output.isNull() && fd.burnTime <= 0.0f) {
			continue;
		}
		Json::Value f(Json::objectValue);
		f["x"] = kv.first.x;
		f["y"] = kv.first.y;
		f["z"] = kv.first.z;
		f["burnTime"] = fd.burnTime;
		f["burnDuration"] = fd.burnDuration;
		f["cookTime"] = fd.cookTime;
		f["input"] = serializeItem(fd.input);
		f["fuel"] = serializeItem(fd.fuel);
		f["output"] = serializeItem(fd.output);
		furnaces.append(f);
	}
	root["furnaces"] = furnaces;

	std::string tmpPath = worldDir + "/world_meta.json.tmp";
	std::string finalPath = worldDir + "/world_meta.json";
	{
		std::ofstream ofs(tmpPath, std::ios::binary | std::ios::trunc);
		if (!ofs.is_open()) {
			return false;
		}
		Json::StyledWriter writer;
		ofs << writer.write(root);
		ofs.flush();
		if (!ofs.good()) {
			return false;
		}
	}
	fs::rename(tmpPath, finalPath, ec);
	if (ec) {
		std::ofstream ofs(finalPath, std::ios::binary | std::ios::trunc);
		if (!ofs.is_open()) return false;
		Json::StyledWriter writer;
		ofs << writer.write(root);
		fs::remove(tmpPath, ec);
	}
	return true;
}

void applyMetadataToContext(const WorldMetadata& meta, GuiContext& ctx) {
	ctx.activeWorldName = meta.name;
	ctx.activeWorldSeed = meta.seed;
	ctx.isCreative = meta.isCreative;
	ctx.health = (meta.health > 0) ? meta.health : 20;
	ctx.hunger = (meta.health > 0) ? meta.hunger : 20;
	ctx.saturation = (meta.health > 0) ? meta.saturation : 5.0f;
	ctx.exhaustion = 0.0f;
	ctx.healTimer = 0.0f;
	ctx.starveTimer = 0.0f;
	ctx.selectedHotbarSlot = std::max(0, std::min(8, meta.selectedHotbarSlot));

	ctx.hasPlayerTransform = meta.hasPlayerPos;
	ctx.playerX = meta.playerX;
	ctx.playerY = meta.playerY;
	ctx.playerZ = meta.playerZ;
	ctx.playerYaw = meta.playerYaw;
	ctx.playerPitch = meta.playerPitch;

	for (auto& s : ctx.inventorySlots) s.setNull();
	for (auto& a : ctx.armorSlots) a.setNull();
	if (meta.hasInventory) {
		ctx.inventorySlots = meta.inventorySlots;
		ctx.armorSlots = meta.armorSlots;
		ctx.inventoryInitialized = true;
	} else {
		ctx.inventoryInitialized = false;
		ctx.ensureInventoryInitialized();
	}

	ctx.chests = meta.chests;
	ctx.furnaces = meta.furnaces;
	ctx.pendingWorldDrops.clear();
}

void captureMetadataFromContext(const GuiContext& ctx, WorldMetadata& outMeta) {
	outMeta.name = ctx.activeWorldName;
	outMeta.seed = ctx.activeWorldSeed;
	outMeta.isCreative = ctx.isCreative;
	outMeta.health = ctx.health;
	outMeta.hunger = ctx.hunger;
	outMeta.saturation = ctx.saturation;
	outMeta.selectedHotbarSlot = ctx.selectedHotbarSlot;

	outMeta.hasPlayerPos = ctx.hasPlayerTransform;
	outMeta.playerX = ctx.playerX;
	outMeta.playerY = ctx.playerY;
	outMeta.playerZ = ctx.playerZ;
	outMeta.playerYaw = ctx.playerYaw;
	outMeta.playerPitch = ctx.playerPitch;

	outMeta.hasInventory = ctx.inventoryInitialized;
	outMeta.inventorySlots = ctx.inventorySlots;
	outMeta.armorSlots = ctx.armorSlots;
	outMeta.chests = ctx.chests;
	outMeta.furnaces = ctx.furnaces;
}
