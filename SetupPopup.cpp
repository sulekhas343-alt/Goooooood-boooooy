#include "SetupPopup.hpp"
#include "SuggestPopup.hpp"

static std::vector<std::string> const MODES = {
	"Cube", "Ship", "Ball", "UFO", "Wave", "Robot", "Spider", "Swing"
};
static std::vector<std::string> const SKILLS = {
	"Nerve Control", "Chokepoints", "Timings", "Flow", "Memory",
	"Fast-Paced", "Slow-Paced", "Duals", "Gimmicky", "XL Levels"
};
static std::vector<std::string> const NONE;

SetupPopup* SetupPopup::create() {
	auto ret = new SetupPopup();
	if (ret->init()) {
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

bool SetupPopup::init() {
	if (!Popup::init(380.f, 270.f)) return false;
	this->setTitle("Set up your profile");
	m_profile = data::loadProfile();
	m_query = m_profile.hardestName;
	this->build();
	return true;
}

std::vector<std::string> const& SetupPopup::items() const {
	if (m_step == 1) return MODES;
	if (m_step == 3) return SKILLS;
	return NONE;
}

void SetupPopup::initSel() {
	auto const& its = this->items();
	auto const& chosen = m_step == 1 ? m_profile.modes : m_profile.skills;
	m_sel.assign(its.size(), 0);
	for (size_t i = 0; i < its.size(); i++) {
		for (auto const& c : chosen) {
			if (c == its[i]) m_sel[i] = 1;
		}
	}
}

void SetupPopup::clearNodes() {
	for (auto& n : m_nodes) {
		if (n) n->removeFromParent();
	}
	m_nodes.clear();
	m_input = nullptr;
}

static CCLabelBMFont* makeLabel(std::string const& text, char const* font, float scale) {
	auto l = CCLabelBMFont::create(text.c_str(), font);
	l->setScale(scale);
	l->setAlignment(kCCTextAlignmentCenter);
	return l;
}

void SetupPopup::build() {
	this->clearNodes();

	auto addLabel = [&](std::string const& text, float y, char const* font, float scale) {
		auto l = makeLabel(text, font, scale);
		m_mainLayer->addChildAtPosition(l, Anchor::Center, {0.f, y});
		m_nodes.push_back(l);
	};
	auto addButton = [&](char const* text, float x, float y, SEL_MenuHandler cb) {
		auto b = CCMenuItemSpriteExtra::create(
			ButtonSprite::create(text, "goldFont.fnt", "GJ_button_01.png", 0.8f), this, cb
		);
		m_buttonMenu->addChildAtPosition(b, Anchor::Center, {x, y});
		m_nodes.push_back(b);
		return b;
	};

	if (m_step == 0) {
		addLabel("What is your hardest demon?", 88.f, "bigFont.fnt", 0.5f);
		addLabel("Type its level name or level ID", 68.f, "chatFont.fnt", 0.8f);

		m_input = TextInput::create(250.f, "Level name or ID", "bigFont.fnt");
		m_input->setString(m_query);
		m_mainLayer->addChildAtPosition(m_input, Anchor::Center, {0.f, 36.f});
		m_nodes.push_back(m_input);

		addLabel(m_msg, -20.f, "chatFont.fnt", 0.85f);

		addButton("Find", -110.f, -95.f, menu_selector(SetupPopup::onFind));
		addButton("Skip", 0.f, -95.f, menu_selector(SetupPopup::onSkip));
		if (m_resolved) addButton("Next", 110.f, -95.f, menu_selector(SetupPopup::onNext));
		return;
	}

	if (m_step == 2) {
		addLabel("What type of levels do you like?", 85.f, "bigFont.fnt", 0.45f);
		addLabel("Learny = memorize it. Skill-based = pure execution.", 62.f, "chatFont.fnt", 0.8f);
		auto a = addButton("Learny", 0.f, 25.f, menu_selector(SetupPopup::onStyle));
		a->setTag(0);
		auto b = addButton("Skill-based", 0.f, -15.f, menu_selector(SetupPopup::onStyle));
		b->setTag(1);
		auto c = addButton("Both", 0.f, -55.f, menu_selector(SetupPopup::onStyle));
		c->setTag(2);
		addButton("Back", -110.f, -105.f, menu_selector(SetupPopup::onBack));
		return;
	}

	// steps 1 (gamemodes) and 3 (skills): checkbox grids
	if (m_step == 1) {
		addLabel("What are your favourite gamemodes?", 88.f, "bigFont.fnt", 0.42f);
		addLabel("Pick as many as you like", 68.f, "chatFont.fnt", 0.8f);
	}
	else {
		addLabel("What skills do you want to practice?", 88.f, "bigFont.fnt", 0.42f);
		addLabel("Pick as many as you like (XL Levels = very long levels)", 68.f, "chatFont.fnt", 0.8f);
	}

	auto const& its = this->items();
	for (size_t i = 0; i < its.size(); i++) {
		float x = (i % 2 == 0) ? -95.f : 55.f;
		float y = 40.f - static_cast<float>(i / 2) * 28.f;
		auto tog = CCMenuItemToggler::createWithStandardSprites(
			this, menu_selector(SetupPopup::onToggle), 0.6f
		);
		tog->setTag(static_cast<int>(i));
		tog->toggle(m_sel[i] != 0);
		m_buttonMenu->addChildAtPosition(tog, Anchor::Center, {x, y});
		m_nodes.push_back(tog);

		auto l = CCLabelBMFont::create(its[i].c_str(), "bigFont.fnt");
		l->setScale(0.4f);
		l->setAnchorPoint({0.f, 0.5f});
		m_buttonMenu->addChildAtPosition(l, Anchor::Center, {x + 18.f, y});
		m_nodes.push_back(l);
	}

	addButton("Back", -110.f, -105.f, menu_selector(SetupPopup::onBack));
	addButton(m_step == 3 ? "Finish" : "Next", 110.f, -105.f, menu_selector(SetupPopup::onNext));
}

void SetupPopup::onToggle(CCObject* sender) {
	size_t i = static_cast<size_t>(static_cast<CCNode*>(sender)->getTag());
	if (i < m_sel.size()) m_sel[i] = m_sel[i] ? 0 : 1;
}

void SetupPopup::onStyle(CCObject* sender) {
	int t = static_cast<CCNode*>(sender)->getTag();
	m_profile.style = t == 0 ? "Learny" : (t == 1 ? "Skill-based" : "Both");
	m_step = 3;
	this->initSel();
	this->build();
}

void SetupPopup::onBack(CCObject*) {
	if (m_busy) return;
	m_step = std::max(0, m_step - 1);
	this->initSel();
	this->build();
}

void SetupPopup::onSkip(CCObject*) {
	if (m_busy) return;
	m_profile.hardestId = 0;
	m_profile.hardestName = "";
	m_profile.hardestAredl = 0;
	m_profile.hardestTier = 0;
	m_step = 1;
	this->initSel();
	this->build();
}

void SetupPopup::onFind(CCObject*) {
	if (m_busy) return;
	if (m_input) m_query = std::string(m_input->getString());
	if (m_query.empty()) {
		m_msg = "Type a level name or ID first.";
		this->build();
		return;
	}
	this->loadAll();
}

void SetupPopup::loadAll() {
	m_busy = true;
	m_resolved = false;

	auto next = [this](bool ok, std::string err) {
		if (!ok) {
			m_busy = false;
			m_msg = err + "\nYou can tap Skip to continue without it.";
			this->build();
			return;
		}
		this->loadAll();
	};

	if (!data::aredlLoaded) {
		m_msg = "Loading the AREDL list...";
		this->build();
		data::fetchSource(m_dl, "aredl", next);
		return;
	}
	if (!data::gddlLoaded) {
		m_msg = "Loading the GDDL list (can take a few seconds)...";
		this->build();
		data::fetchSource(m_dl, "gddl", next);
		return;
	}
	m_busy = false;
	this->doResolve();
}

void SetupPopup::doResolve() {
	auto r = data::resolveHardest(m_query);
	if (!r.found) {
		m_resolved = false;
		m_msg = "Couldn't find that level on AREDL or GDDL.\nTry the exact name or the level ID, or tap Skip.";
		this->build();
		return;
	}

	m_profile.hardestId = r.id;
	m_profile.hardestName = r.name;
	m_profile.hardestAredl = r.aredl;
	m_profile.hardestTier = r.tier;
	m_resolved = true;

	if (r.aredl > 0) {
		m_msg = fmt::format("{}\nAREDL #{} - Extreme demon", r.name, r.aredl);
	}
	else if (r.tier > 0) {
		m_msg = fmt::format("{}\nGDDL tier {} - {} demon", r.name, r.tier, catName(data::tierToCat(r.tier)));
	}
	else {
		m_msg = fmt::format("{}\nFound it, but it has no rating yet.", r.name);
	}
	this->build();
}

void SetupPopup::onNext(CCObject*) {
	if (m_busy) return;

	if (m_step == 1 || m_step == 3) {
		auto const& its = this->items();
		std::vector<std::string> picked;
		for (size_t i = 0; i < its.size(); i++) {
			if (m_sel[i]) picked.push_back(its[i]);
		}
		if (m_step == 1) m_profile.modes = picked;
		else m_profile.skills = picked;
	}

	if (m_step == 3) {
		m_profile.done = true;
		data::saveProfile(m_profile);
		data::clearSets();
		if (auto p = SuggestPopup::create()) p->show();
		this->onClose(nullptr);
		return;
	}

	m_step++;
	this->initSel();
	this->build();
}
