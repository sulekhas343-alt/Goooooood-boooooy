#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <functional>

using namespace geode::prelude;

enum class Cat { Easy = 0, Medium, Hard, Insane, Extreme, List };
constexpr int CAT_COUNT = 6;

char const* catName(Cat c);        // "Easy"
char const* catKey(Cat c);         // "easy"
char const* sourceKey(Cat c);      // "gddl" / "aredl" / "pointercrate"
char const* sourceName(Cat c);     // "GDDL" / "AREDL" / "Pointercrate"

struct LevelInfo {
	int id = 0;
	std::string name;
	int tier = 0;       // GDDL tier (0 = none)
	int position = 0;   // AREDL / Pointercrate position (0 = none)
	std::string tags;   // lowercase searchable tag text
	int likes = -1;
	int length = -1;
};

struct Profile {
	bool done = false;
	int hardestId = 0;
	std::string hardestName;
	int hardestAredl = 0;
	int hardestTier = 0;
	std::vector<std::string> modes;
	std::vector<std::string> skills;
	std::string style = "Both";
};

namespace data {
	extern std::vector<LevelInfo> gddl, aredl, pointercrate;
	extern bool gddlLoaded, aredlLoaded, pcLoaded;

	bool sourceReady(Cat c);
	Cat tierToCat(int tier);

	// Downloads + parses one source ("gddl", "aredl" or "pointercrate").
	void fetchSource(
		async::TaskHolder<web::WebResponse>& holder,
		std::string which,
		std::function<void(bool, std::string)> done
	);

	std::string freqSetting(Cat c);
	std::string periodKey(Cat c);

	Profile loadProfile();
	void saveProfile(Profile const& p);
	void clearSets();

	std::vector<LevelInfo> buildPool(Cat c, Profile const& p, bool personalize, uint32_t seed);
	LevelInfo const* findById(Cat c, int id);

	struct Resolved {
		bool found = false;
		int id = 0;
		std::string name;
		int aredl = 0;
		int tier = 0;
	};
	Resolved resolveHardest(std::string const& query);

	std::vector<int> loadSaved(Cat c);
	void saveSet(Cat c, std::vector<int> const& ids);
	int chosenFor(Cat c);
	void setChosen(Cat c, int id);
}
