#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "SetupPopup.hpp"
#include "SuggestPopup.hpp"

using namespace geode::prelude;

class $modify(DemonMenuLayer, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;

		auto spr = CircleButtonSprite::createWithSpriteFrameName(
			"GJ_starsIcon_001.png", 1.0f, CircleBaseColor::Green, CircleBaseSize::Medium
		);
		auto btn = CCMenuItemSpriteExtra::create(
			spr, this, menu_selector(DemonMenuLayer::onOpenSuggestions)
		);
		btn->setID("suggestions-button"_spr);

		if (auto menu = this->getChildByID("bottom-menu")) {
			menu->addChild(btn);
			menu->updateLayout();
		}
		return true;
	}

	void onOpenSuggestions(CCObject*) {
		if (data::loadProfile().done) {
			if (auto p = SuggestPopup::create()) p->show();
		}
		else {
			if (auto p = SetupPopup::create()) p->show();
		}
	}
};
