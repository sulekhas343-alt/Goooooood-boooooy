#include "SuggestPopup.hpp"
#include "SetupPopup.hpp"

#include <algorithm>
#include <map>
#include <random>

static std::map<int, int> s_likes;
static std::map<int, int> s_len;

template <class T>
static int intOf(T const& v) {
	if constexpr (requires { v.value(); }) return static_cast<int>(v.value());
	else return static_cast<int>(v);
}

static std::string likesText(int likes) {
	if (likes < 0) return "";
	if (likes >= 1000) return fmt::format(" ({:.1f}k likes)", likes / 1000.0);
	return fmt::format(" ({} likes)", likes);
}

SuggestPopup* SuggestPopup::create() {
	auto ret = new SuggestPopup();
	if (ret->init()) {
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

SuggestPopup::~SuggestPopup() {
	this->releaseDelegate();
}

void SuggestPopup::releaseDelegate() {
	auto glm = GameLevelManager::sharedState();
	if (glm && glm->m_levelManagerDelegate == static_cast<LevelManagerDelegate*>(this)) {
		glm->m_levelManagerDelegate = nullptr;
	}
}

bool SuggestPopup::init() {
	if (!Popup::init(420.f, 290.f)) return false;
	this->setTitle("Demon Suggestions");

	m_status = CCLabelBMFont::create("", "bigFont.fnt");
	m_status->setScale(0.4f);
	m_status->setAlignment(kCCTextAlignmentCenter);
	m_mainLayer->addChildAtPosition(m_status, Anchor::Center, {0.f, -10.f});

	auto rerollBtn = CCMenuItemSpriteExtra::create(
		ButtonSprite::create("Reroll", "goldFont.fnt", "GJ_button_01.png", 0.7f),
		this, menu_selector(SuggestPopup::onReroll)
	);
	m_buttonMenu->addChildAtPosition(rerollBtn, Anchor::Bottom, {-70.f, 24.f});

	auto profileBtn = CCMenuItemSpriteExtra::create(
		ButtonSprite::create("My Profile", "goldFont.fnt", "GJ_button_01.png", 0.7f),
		this, menu_selector(SuggestPopup::onProfile)
	);
	m_buttonMenu->addChildAtPosition(profileBtn, Anchor::Bottom, {70.f, 24.f});

	auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
	prevSpr->setScale(0.7f);
	auto prevBtn = CCMenuItemSpriteExtra::create(prevSpr, this, menu_selector(SuggestPopup::onPrev));
	m_buttonMenu->addChildAtPosition(prevBtn, Anchor::Left, {22.f, -10.f});

	auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
	nextSpr->setScale(0.7f);
	nextSpr->setFlipX(true);
	auto nextBtn = CCMenuItemSpriteExtra::create(nextSpr, this, menu_selector(SuggestPopup::onNext));
	m_buttonMenu->addChildAtPosition(nextBtn, Anchor::Right, {-22.f, -10.f});

	int last = Mod::get()->getSavedValue<int>("last-cat", 0);
	last = std::clamp(last, 0, CAT_COUNT - 1);
	this->selectCat(static_cast<Cat>(last));
	return true;
}

void SuggestPopup::setStatus(std::string const& s) {
	m_status->setString(s.c_str());
}

void SuggestPopup::selectCat(Cat c) {
	m_cat = c;
	Mod::get()->setSavedValue<int>("last-cat", static_cast<int>(c));
	m_page = 0;
	m_shown.clear();
	m_note.clear();
	this->rebuild();
	this->ensureSource();
}

void SuggestPopup::ensureSource() {
	if (data::sourceReady(m_cat)) {
		this->prepare();
		return;
	}
	m_busy = true;
	this->setStatus(fmt::format("Downloading the {} list...", sourceName(m_cat)));
	Cat c = m_cat;
	data::fetchSource(m_dl, sourceKey(c), [this, c](bool ok, std::string err) {
		m_busy = false;
		if (!ok) {
			this->setStatus(err + "\nCheck your connection, then tap Reroll.");
			return;
		}
		if (m_cat == c) this->prepare();
	});
}

void SuggestPopup::prepare() {
	auto saved = data::loadSaved(m_cat);
	if (!saved.empty()) {
		m_shown.clear();
		for (int id : saved) {
			if (auto l = data::findById(m_cat, id)) {
				LevelInfo x = *l;
				auto it = s_likes.find(id);
				if (it != s_likes.end()) x.likes = it->second;
				auto jt = s_len.find(id);
				if (jt != s_len.end()) x.length = jt->second;
				m_shown.push_back(std::move(x));
			}
		}
		if (!m_shown.empty()) {
			m_note.clear();
			this->setStatus("");
			this->rebuild();
			return;
		}
	}
	this->startGenerate(false);
}

void SuggestPopup::startGenerate(bool reroll) {
	auto profile = data::loadProfile();
	bool personalize = Mod::get()->getSettingValue<bool>("use-profile");

	uint32_t seed;
	if (reroll) seed = std::random_device{}();
	else seed = static_cast<uint32_t>(std::hash<std::string>{}(data::periodKey(m_cat) + catKey(m_cat)));

	m_order = data::buildPool(m_cat, profile, personalize, seed);
	m_shown.clear();
	m_note.clear();

	if (m_order.empty()) {
		this->setStatus("No levels found for this category.\nMaybe you beat them all, or the list failed to load.");
		this->rebuild();
		return;
	}

	m_target = static_cast<int>(Mod::get()->getSettingValue<int64_t>("suggestion-count"));
	m_minLikes = static_cast<int>(Mod::get()->getSettingValue<int64_t>("min-likes"));
	m_accepted.clear();
	m_pending.clear();
	m_cursor = 0;
	m_batches = 0;
	m_fails = 0;
	m_likesUnavailable = false;
	m_busy = true;
	m_generating = true;

	if (m_minLikes <= 0) {
		for (size_t i = 0; i < m_order.size() && static_cast<int>(m_accepted.size()) < m_target; i++) {
			m_accepted.push_back(m_order[i]);
		}
		this->finish();
		return;
	}

	this->rebuild();
	this->setStatus("Picking levels for you...");
	this->fetchNextBatch();
}

void SuggestPopup::fetchNextBatch() {
	m_pending.clear();
	m_batchStart = m_cursor;
	m_acceptedBefore = m_accepted.size();
	std::string ids;

	while (m_cursor < m_order.size() && m_pending.size() < 10 && static_cast<int>(m_accepted.size()) < m_target) {
		auto const& l = m_order[m_cursor++];
		auto it = s_likes.find(l.id);
		if (it != s_likes.end()) {
			if (it->second >= m_minLikes) {
				LevelInfo x = l;
				x.likes = it->second;
				auto jt = s_len.find(l.id);
				if (jt != s_len.end()) x.length = jt->second;
				m_accepted.push_back(std::move(x));
			}
			continue;
		}
		m_pending.push_back(l);
		if (!ids.empty()) ids += ',';
		ids += std::to_string(l.id);
	}

	if (static_cast<int>(m_accepted.size()) >= m_target || m_pending.empty() || m_batches >= 14) {
		this->finish();
		return;
	}

	m_batches++;
	this->setStatus(fmt::format("Checking likes... ({}/{} found)", m_accepted.size(), m_target));

	// type 10 = "list of level IDs" search (same thing GD uses for map packs)
	auto search = GJSearchObject::create(static_cast<SearchType>(10), ids);
	auto glm = GameLevelManager::sharedState();
	glm->m_levelManagerDelegate = this;
	glm->getOnlineLevels(search);
}

void SuggestPopup::loadLevelsFinished(CCArray* levels, char const*, int) { this->onBatch(levels); }
void SuggestPopup::loadLevelsFailed(char const*, int) { this->onBatch(nullptr); }
void SuggestPopup::loadLevelsFinished(CCArray* levels, char const*) { this->onBatch(levels); }
void SuggestPopup::loadLevelsFailed(char const*) { this->onBatch(nullptr); }

void SuggestPopup::onBatch(CCArray* levels) {
	if (!m_generating) return;

	if (!levels) {
		m_fails++;
		if (m_fails >= 2) {
			// the GD server isn't answering: fall back to unfiltered picks
			m_likesUnavailable = true;
			m_accepted.clear();
			for (size_t i = 0; i < m_order.size() && static_cast<int>(m_accepted.size()) < m_target; i++) {
				m_accepted.push_back(m_order[i]);
			}
			this->finish();
			return;
		}
		m_cursor = m_batchStart;
		m_accepted.resize(m_acceptedBefore);
		this->fetchNextBatch();
		return;
	}

	m_fails = 0;
	for (auto level : CCArrayExt<GJGameLevel*>(levels)) {
		int id = intOf(level->m_levelID);
		s_likes[id] = intOf(level->m_likes);
		s_len[id] = intOf(level->m_levelLength);
	}

	for (auto const& l : m_pending) {
		if (static_cast<int>(m_accepted.size()) >= m_target) break;
		auto it = s_likes.find(l.id);
		if (it == s_likes.end() || it->second < m_minLikes) continue;
		LevelInfo x = l;
		x.likes = it->second;
		auto jt = s_len.find(l.id);
		if (jt != s_len.end()) x.length = jt->second;
		m_accepted.push_back(std::move(x));
	}
	m_pending.clear();

	if (static_cast<int>(m_accepted.size()) >= m_target) this->finish();
	else this->fetchNextBatch();
}

void SuggestPopup::finish() {
	m_generating = false;
	m_busy = false;
	this->releaseDelegate();

	if (m_accepted.empty()) {
		m_shown.clear();
		this->setStatus(fmt::format(
			"No levels with {}+ likes found here.\nLower 'Minimum likes' in the mod settings.", m_minLikes
		));
		this->rebuild();
		return;
	}

	if (static_cast<int>(m_accepted.size()) > m_target) m_accepted.resize(m_target);

	auto prof = data::loadProfile();
	if (std::find(prof.skills.begin(), prof.skills.end(), std::string("XL Levels")) != prof.skills.end()) {
		std::stable_partition(m_accepted.begin(), m_accepted.end(), [](LevelInfo const& l) {
			return l.length >= 4;
		});
	}

	if (!m_likesUnavailable) {
		std::vector<int> ids;
		for (auto const& l : m_accepted) ids.push_back(l.id);
		data::saveSet(m_cat, ids);
		m_note.clear();
	}
	else {
		m_note = "Like check unavailable right now - showing unfiltered picks.";
	}

	m_shown = m_accepted;
	m_page = 0;
	this->setStatus("");
	this->rebuild();
}

void SuggestPopup::rebuild() {
	for (auto& n : m_nodes) {
		if (n) n->removeFromParent();
	}
	m_nodes.clear();

	// category tabs
	for (int i = 0; i < CAT_COUNT; i++) {
		bool sel = i == static_cast<int>(m_cat);
		auto spr = ButtonSprite::create(
			catName(static_cast<Cat>(i)), "bigFont.fnt",
			sel ? "GJ_button_02.png" : "GJ_button_01.png", 0.45f
		);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(SuggestPopup::onTab));
		btn->setTag(i);
		m_buttonMenu->addChildAtPosition(btn, Anchor::Top, {(i - 2.5f) * 64.f, -48.f});
		m_nodes.push_back(btn);
	}

	constexpr int PER_PAGE = 5;
	int pages = std::max(1, static_cast<int>((m_shown.size() + PER_PAGE - 1) / PER_PAGE));
	m_page = std::clamp(m_page, 0, pages - 1);

	auto info = CCLabelBMFont::create(
		fmt::format("{} | refreshes {} | page {}/{}", sourceName(m_cat), data::freqSetting(m_cat), m_page + 1, pages).c_str(),
		"chatFont.fnt"
	);
	info->setScale(0.7f);
	m_buttonMenu->addChildAtPosition(info, Anchor::Top, {0.f, -75.f});
	m_nodes.push_back(info);

	int chosen = data::chosenFor(m_cat);

	for (int r = 0; r < PER_PAGE; r++) {
		size_t idx = static_cast<size_t>(m_page * PER_PAGE + r);
		if (idx >= m_shown.size()) break;
		auto const& s = m_shown[idx];
		float y = -102.f - r * 30.f;

		if (GameStatsManager::sharedState()->hasCompletedOnlineLevel(s.id)) {
			auto check = CCSprite::createWithSpriteFrameName("GJ_completesIcon_001.png");
			check->setScale(0.55f);
			m_buttonMenu->addChildAtPosition(check, Anchor::Top, {-192.f, y});
			m_nodes.push_back(check);
		}

		std::string tag = s.tier > 0 ? fmt::format("T{}", s.tier) : fmt::format("#{}", s.position);
		auto label = CCLabelBMFont::create(
			(tag + "  " + s.name + likesText(s.likes)).c_str(), "goldFont.fnt"
		);
		label->limitLabelWidth(215.f, 0.6f, 0.2f);
		label->setAnchorPoint({0.f, 0.5f});
		m_buttonMenu->addChildAtPosition(label, Anchor::Top, {-178.f, y});
		m_nodes.push_back(label);

		bool isPick = s.id == chosen;
		auto pickBtn = CCMenuItemSpriteExtra::create(
			ButtonSprite::create(isPick ? "Picked" : "Pick", "goldFont.fnt", isPick ? "GJ_button_02.png" : "GJ_button_01.png", 0.6f),
			this, menu_selector(SuggestPopup::onPick)
		);
		pickBtn->setTag(s.id);
		m_buttonMenu->addChildAtPosition(pickBtn, Anchor::Top, {110.f, y});
		m_nodes.push_back(pickBtn);

		auto viewBtn = CCMenuItemSpriteExtra::create(
			ButtonSprite::create("View", "goldFont.fnt", "GJ_button_01.png", 0.6f),
			this, menu_selector(SuggestPopup::onOpenLevel)
		);
		viewBtn->setTag(s.id);
		m_buttonMenu->addChildAtPosition(viewBtn, Anchor::Top, {170.f, y});
		m_nodes.push_back(viewBtn);
	}

	if (!m_shown.empty()) {
		std::string text = "Tap Pick on the level you want to beat.";
		if (chosen != 0) {
			for (auto const& s : m_shown) {
				if (s.id == chosen) text = "Your pick: " + s.name;
			}
		}
		if (!m_note.empty()) text = m_note;
		auto pl = CCLabelBMFont::create(text.c_str(), "chatFont.fnt");
		pl->limitLabelWidth(320.f, 0.8f, 0.3f);
		m_buttonMenu->addChildAtPosition(pl, Anchor::Bottom, {0.f, 55.f});
		m_nodes.push_back(pl);
	}
}

void SuggestPopup::onTab(CCObject* sender) {
	if (m_busy) return;
	this->selectCat(static_cast<Cat>(static_cast<CCNode*>(sender)->getTag()));
}

void SuggestPopup::onReroll(CCObject*) {
	if (m_busy) return;
	if (!data::sourceReady(m_cat)) {
		this->ensureSource();
		return;
	}
	this->startGenerate(true);
}

void SuggestPopup::onProfile(CCObject*) {
	if (m_busy) return;
	if (auto p = SetupPopup::create()) p->show();
	this->onClose(nullptr);
}

void SuggestPopup::onPrev(CCObject*) {
	if (m_busy) return;
	m_page--;
	this->rebuild();
}

void SuggestPopup::onNext(CCObject*) {
	if (m_busy) return;
	m_page++;
	this->rebuild();
}

void SuggestPopup::onPick(CCObject* sender) {
	data::setChosen(m_cat, static_cast<CCNode*>(sender)->getTag());
	this->rebuild();
}

void SuggestPopup::onOpenLevel(CCObject* sender) {
	int id = static_cast<CCNode*>(sender)->getTag();
	auto search = GJSearchObject::create(SearchType::Search, std::to_string(id));
	auto scene = LevelBrowserLayer::scene(search);
	CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(0.5f, scene));
}
