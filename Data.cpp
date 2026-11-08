#include "Types.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <random>

using namespace geode::prelude;

char const* catName(Cat c) {
	switch (c) {
		case Cat::Easy: return "Easy";
		case Cat::Medium: return "Medium";
		case Cat::Hard: return "Hard";
		case Cat::Insane: return "Insane";
		case Cat::Extreme: return "Extreme";
		case Cat::List: return "List";
	}
	return "?";
}

char const* catKey(Cat c) {
	switch (c) {
		case Cat::Easy: return "easy";
		case Cat::Medium: return "medium";
		case Cat::Hard: return "hard";
		case Cat::Insane: return "insane";
		case Cat::Extreme: return "extreme";
		case Cat::List: return "list";
	}
	return "easy";
}

char const* sourceKey(Cat c) {
	if (c == Cat::Extreme) return "aredl";
	if (c == Cat::List) return "pointercrate";
	return "gddl";
}

char const* sourceName(Cat c) {
	if (c == Cat::Extreme) return "AREDL";
	if (c == Cat::List) return "Pointercrate";
	return "GDDL";
}

namespace data {

std::vector<LevelInfo> gddl, aredl, pointercrate;
bool gddlLoaded = false, aredlLoaded = false, pcLoaded = false;

// ---------------------------------------------------------------- helpers

static std::string lower(std::string s) {
	for (auto& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	return s;
}

static int64_t getInt(matjson::Value& v, std::initializer_list<char const*> keys) {
	if (!v.isObject()) return 0;
	for (auto k : keys) {
		if (!v.contains(k)) continue;
		auto& f = v[k];
		if (f.isNumber()) return static_cast<int64_t>(std::llround(f.asDouble().unwrapOr(0.0)));
		if (f.isString()) {
			try { return std::stoll(f.asString().unwrapOr("")); } catch (...) {}
		}
	}
	return 0;
}

static std::string getStr(matjson::Value& v, std::initializer_list<char const*> keys) {
	if (!v.isObject()) return "";
	for (auto k : keys) {
		if (!v.contains(k)) continue;
		auto& f = v[k];
		if (f.isString()) return f.asString().unwrapOr("");
	}
	return "";
}

// Collects every tag-like string it can find (works for ["Wave"] or [{"name":"Wave"}] ...)
static void collect(matjson::Value& v, std::string& out, int depth = 0) {
	if (depth > 4) return;
	if (v.isString()) {
		out += ' ';
		out += lower(v.asString().unwrapOr(""));
		return;
	}
	if (v.isArray()) {
		auto arr = v.asArray();
		if (arr.isErr()) return;
		auto items = arr.unwrap();
		for (auto& e : items) collect(e, out, depth + 1);
		return;
	}
	if (v.isObject()) {
		for (auto k : {"Name", "name", "Tag", "tag", "Title", "title"}) {
			if (v.contains(k)) collect(v[k], out, depth + 1);
		}
	}
}

static void collectTags(matjson::Value& item, std::string& out) {
	if (!item.isObject()) return;
	for (auto k : {"Tags", "tags", "Skillsets", "skillsets", "SkillSets", "Skills"}) {
		if (item.contains(k)) collect(item[k], out);
	}
}

static bool getItems(matjson::Value& json, std::vector<matjson::Value>& out) {
	matjson::Value* arr = &json;
	if (!json.isArray()) {
		if (!json.isObject()) return false;
		arr = nullptr;
		for (auto k : {"levels", "Levels", "data", "Data", "list", "List"}) {
			if (json.contains(k) && json[k].isArray()) {
				arr = &json[k];
				break;
			}
		}
		if (!arr) return false;
	}
	auto res = arr->asArray();
	if (res.isErr()) return false;
	out = res.unwrap();
	return true;
}

// ---------------------------------------------------------------- parsing

static bool parseGddl(matjson::Value& json) {
	std::vector<matjson::Value> items;
	if (!getItems(json, items)) return false;
	std::vector<LevelInfo> list;
	for (auto& item : items) {
		LevelInfo l;
		l.id = static_cast<int>(getInt(item, {"ID", "id", "LevelID", "levelID", "level_id"}));
		l.name = getStr(item, {"Name", "name"});
		l.tier = static_cast<int>(getInt(item, {"Rating", "rating", "Tier", "tier"}));
		if (item.contains("Meta") && item["Meta"].isObject()) {
			auto& meta = item["Meta"];
			if (l.id == 0) l.id = static_cast<int>(getInt(meta, {"ID", "id", "LevelID"}));
			if (l.name.empty()) l.name = getStr(meta, {"Name", "name"});
		}
		collectTags(item, l.tags);
		if (l.id > 0 && l.tier > 0) list.push_back(std::move(l));
	}
	if (list.empty()) return false;
	gddl = std::move(list);
	gddlLoaded = true;
	return true;
}

static bool parseAredl(matjson::Value& json) {
	std::vector<matjson::Value> items;
	if (!getItems(json, items)) return false;
	std::vector<LevelInfo> list;
	for (auto& item : items) {
		LevelInfo l;
		l.id = static_cast<int>(getInt(item, {"level_id", "levelID", "LevelID"}));
		l.position = static_cast<int>(getInt(item, {"position", "Position"}));
		l.name = getStr(item, {"name", "Name"});
		collectTags(item, l.tags);
		if (l.id > 0 && l.position > 0) list.push_back(std::move(l));
	}
	if (list.empty()) return false;
	aredl = std::move(list);
	aredlLoaded = true;
	return true;
}

static bool parsePointercrate(matjson::Value& json) {
	std::vector<matjson::Value> items;
	if (!getItems(json, items)) return false;
	std::vector<LevelInfo> list;
	for (auto& item : items) {
		LevelInfo l;
		l.id = static_cast<int>(getInt(item, {"level_id"}));
		l.position = static_cast<int>(getInt(item, {"position"}));
		l.name = getStr(item, {"name"});
		if (l.id > 0 && l.position > 0) list.push_back(std::move(l));
	}
	if (list.empty()) return false;
	pointercrate = std::move(list);
	pcLoaded = true;
	return true;
}

static bool parseWhich(std::string const& which, matjson::Value& json) {
	if (which == "gddl") return parseGddl(json);
	if (which == "aredl") return parseAredl(json);
	return parsePointercrate(json);
}

static std::string urlFor(std::string const& which) {
	if (which == "gddl") return Mod::get()->getSettingValue<std::string>("gddl-url");
	if (which == "aredl") return Mod::get()->getSettingValue<std::string>("aredl-url");
	return Mod::get()->getSettingValue<std::string>("pointercrate-url");
}

void fetchSource(
	async::TaskHolder<web::WebResponse>& holder,
	std::string which,
	std::function<void(bool, std::string)> done
) {
	web::WebRequest req;
	req.userAgent("GeodeMod/DailyDemonSuggestions");
	holder.spawn(req.get(urlFor(which)), [which, done](web::WebResponse res) {
		if (!res.ok()) {
			done(false, fmt::format("Could not download the {} list (HTTP {}).", which, res.code()));
			return;
		}
		auto json = res.json();
		if (json.isErr()) {
			done(false, fmt::format("The {} list returned unreadable data.", which));
			return;
		}
		auto value = json.unwrap();
		if (!parseWhich(which, value)) {
			done(false, fmt::format("The {} list has an unexpected format.", which));
			return;
		}
		done(true, "");
	});
}

bool sourceReady(Cat c) {
	if (c == Cat::Extreme) return aredlLoaded;
	if (c == Cat::List) return pcLoaded;
	return gddlLoaded;
}

Cat tierToCat(int tier) {
	auto mod = Mod::get();
	if (tier <= mod->getSettingValue<int64_t>("easy-max-tier")) return Cat::Easy;
	if (tier <= mod->getSettingValue<int64_t>("medium-max-tier")) return Cat::Medium;
	if (tier <= mod->getSettingValue<int64_t>("hard-max-tier")) return Cat::Hard;
	if (tier <= mod->getSettingValue<int64_t>("insane-max-tier")) return Cat::Insane;
	return Cat::Extreme;
}

// ---------------------------------------------------------------- periods

static long daysFromCivil(int y, int m, int d) {
	y -= m <= 2;
	long era = (y >= 0 ? y : y - 399) / 400;
	long yoe = y - era * 400;
	long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}

std::string freqSetting(Cat c) {
	return Mod::get()->getSettingValue<std::string>(std::string("freq-") + catKey(c));
}

std::string periodKey(Cat c) {
	std::time_t t = std::time(nullptr);
	std::tm* lt = std::localtime(&t);
	int y = lt->tm_year + 1900, m = lt->tm_mon + 1, d = lt->tm_mday;
	auto f = freqSetting(c);
	if (f == "Weekly") return fmt::format("W{}", (daysFromCivil(y, m, d) + 3) / 7);
	if (f == "Monthly") return fmt::format("M{}", y * 100 + m);
	return fmt::format("D{}", y * 10000 + m * 100 + d);
}

// ---------------------------------------------------------------- profile

Profile loadProfile() {
	auto mod = Mod::get();
	Profile p;
	p.done = mod->getSavedValue<bool>("profile-done", false);
	p.hardestId = mod->getSavedValue<int>("hardest-id", 0);
	p.hardestName = mod->getSavedValue<std::string>("hardest-name", "");
	p.hardestAredl = mod->getSavedValue<int>("hardest-aredl", 0);
	p.hardestTier = mod->getSavedValue<int>("hardest-tier", 0);
	p.modes = mod->getSavedValue<std::vector<std::string>>("pref-modes", std::vector<std::string>{});
	p.skills = mod->getSavedValue<std::vector<std::string>>("pref-skills", std::vector<std::string>{});
	p.style = mod->getSavedValue<std::string>("pref-style", "Both");
	return p;
}

void saveProfile(Profile const& p) {
	auto mod = Mod::get();
	mod->setSavedValue<bool>("profile-done", p.done);
	mod->setSavedValue<int>("hardest-id", p.hardestId);
	mod->setSavedValue<std::string>("hardest-name", p.hardestName);
	mod->setSavedValue<int>("hardest-aredl", p.hardestAredl);
	mod->setSavedValue<int>("hardest-tier", p.hardestTier);
	mod->setSavedValue<std::vector<std::string>>("pref-modes", p.modes);
	mod->setSavedValue<std::vector<std::string>>("pref-skills", p.skills);
	mod->setSavedValue<std::string>("pref-style", p.style);
}

void clearSets() {
	for (int i = 0; i < CAT_COUNT; i++) {
		Mod::get()->setSavedValue<std::string>(std::string("per-") + catKey(static_cast<Cat>(i)), "");
	}
}

// ---------------------------------------------------------------- scoring

static bool has(std::string const& hay, char const* needle) {
	return hay.find(needle) != std::string::npos;
}

static std::string modeKeyword(std::string const& m) {
	return lower(m); // cube, ship, ball, ufo, wave, robot, spider, swing
}

static std::string skillKeyword(std::string const& s) {
	if (s == "Nerve Control") return "nerve";
	if (s == "Chokepoints") return "choke";
	if (s == "Timings") return "timing";
	if (s == "Flow") return "flow";
	if (s == "Memory") return "memory";
	if (s == "Fast-Paced") return "fast";
	if (s == "Slow-Paced") return "slow";
	if (s == "Duals") return "dual";
	if (s == "Gimmicky") return "gimmick";
	if (s == "XL Levels") return "xl";
	return "";
}

static float scoreLevel(LevelInfo const& l, Profile const& p) {
	float s = 0.f;
	for (auto& m : p.modes) {
		auto k = modeKeyword(m);
		if (!k.empty() && has(l.tags, k.c_str())) s += 3.f;
	}
	for (auto& sk : p.skills) {
		auto k = skillKeyword(sk);
		if (!k.empty() && has(l.tags, k.c_str())) s += 2.f;
	}
	bool learny = has(l.tags, "learn") || has(l.tags, "memory");
	bool skilly = has(l.tags, "flow") || has(l.tags, "timing") || has(l.tags, "nerve") || has(l.tags, "skill");
	if (p.style == "Learny") {
		if (learny) s += 2.f;
	}
	else if (p.style == "Skill-based") {
		if (skilly) s += 2.f;
		if (learny) s -= 2.f;
	}
	// stay close to the player's current level of difficulty
	if (l.tier > 0 && p.hardestTier > 0) {
		s -= std::abs(l.tier - (p.hardestTier + 1)) * 0.5f;
	}
	if (l.tier == 0 && l.position > 0 && p.hardestAredl > 0) {
		int target = std::max(1, p.hardestAredl - 20);
		s -= std::abs(l.position - target) / 40.f;
	}
	return s;
}

std::vector<LevelInfo> buildPool(Cat c, Profile const& p, bool personalize, uint32_t seed) {
	std::vector<LevelInfo> pool;
	auto stats = GameStatsManager::sharedState();
	bool skip = Mod::get()->getSettingValue<bool>("skip-completed");

	auto consider = [&](LevelInfo const& l) {
		if (skip && stats->hasCompletedOnlineLevel(l.id)) return;
		pool.push_back(l);
	};

	if (c == Cat::List) {
		for (auto const& l : pointercrate) consider(l);
	}
	else if (c == Cat::Extreme) {
		for (auto const& l : aredl) consider(l);
	}
	else {
		for (auto const& l : gddl) {
			if (tierToCat(l.tier) == c) consider(l);
		}
	}

	std::mt19937 rng(seed);
	std::uniform_real_distribution<float> jitter(0.f, 3.f);
	std::vector<std::pair<float, size_t>> keyed;
	keyed.reserve(pool.size());
	for (size_t i = 0; i < pool.size(); i++) {
		float s = jitter(rng);
		if (personalize && p.done) s += scoreLevel(pool[i], p);
		keyed.push_back({s, i});
	}
	std::sort(keyed.begin(), keyed.end(), [](auto const& a, auto const& b) {
		return a.first > b.first;
	});

	std::vector<LevelInfo> out;
	out.reserve(pool.size());
	for (auto& k : keyed) out.push_back(pool[k.second]);
	return out;
}

LevelInfo const* findById(Cat c, int id) {
	auto const& v = c == Cat::Extreme ? aredl : (c == Cat::List ? pointercrate : gddl);
	for (auto const& l : v) {
		if (l.id == id) return &l;
	}
	return nullptr;
}

// ---------------------------------------------------------------- hardest lookup

Resolved resolveHardest(std::string const& rawQuery) {
	Resolved r;
	std::string q = rawQuery;
	while (!q.empty() && std::isspace(static_cast<unsigned char>(q.front()))) q.erase(q.begin());
	while (!q.empty() && std::isspace(static_cast<unsigned char>(q.back()))) q.pop_back();
	if (q.empty()) return r;

	bool numeric = std::all_of(q.begin(), q.end(), [](char ch) {
		return std::isdigit(static_cast<unsigned char>(ch)) != 0;
	});

	LevelInfo const* a = nullptr;
	LevelInfo const* g = nullptr;

	if (numeric) {
		int id = 0;
		try { id = std::stoi(q); } catch (...) { return r; }
		for (auto const& l : aredl) if (l.id == id) { a = &l; break; }
		for (auto const& l : gddl) if (l.id == id) { g = &l; break; }
	}
	else {
		auto lq = lower(q);
		auto pick = [&](std::vector<LevelInfo> const& v) -> LevelInfo const* {
			for (auto const& l : v) if (lower(l.name) == lq) return &l;
			for (auto const& l : v) if (lower(l.name).find(lq) != std::string::npos) return &l;
			return nullptr;
		};
		a = pick(aredl);
		g = pick(gddl);
		if (a && !g) {
			for (auto const& l : gddl) if (l.id == a->id) { g = &l; break; }
		}
		else if (g && !a) {
			for (auto const& l : aredl) if (l.id == g->id) { a = &l; break; }
		}
	}

	if (!a && !g) return r;
	r.found = true;
	if (a) { r.id = a->id; r.name = a->name; r.aredl = a->position; }
	if (g) {
		if (r.id == 0) r.id = g->id;
		if (r.name.empty()) r.name = g->name;
		r.tier = g->tier;
	}
	return r;
}

// ---------------------------------------------------------------- saved sets

std::vector<int> loadSaved(Cat c) {
	auto mod = Mod::get();
	std::string k = catKey(c);
	if (mod->getSavedValue<std::string>("per-" + k, "") != periodKey(c)) return {};
	return mod->getSavedValue<std::vector<int>>("set-" + k, std::vector<int>{});
}

void saveSet(Cat c, std::vector<int> const& ids) {
	auto mod = Mod::get();
	std::string k = catKey(c);
	mod->setSavedValue<std::string>("per-" + k, periodKey(c));
	mod->setSavedValue<std::vector<int>>("set-" + k, ids);
	mod->setSavedValue<int>("pick-" + k, 0);
}

int chosenFor(Cat c) {
	auto ids = loadSaved(c);
	int pick = Mod::get()->getSavedValue<int>(std::string("pick-") + catKey(c), 0);
	if (std::find(ids.begin(), ids.end(), pick) == ids.end()) return 0;
	return pick;
}

void setChosen(Cat c, int id) {
	Mod::get()->setSavedValue<int>(std::string("pick-") + catKey(c), id);
}

} // namespace data
