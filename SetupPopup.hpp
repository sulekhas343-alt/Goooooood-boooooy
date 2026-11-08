#pragma once

#include "Types.hpp"

class SetupPopup : public Popup {
protected:
	int m_step = 0;
	std::vector<Ref<CCNode>> m_nodes;
	TextInput* m_input = nullptr;
	async::TaskHolder<web::WebResponse> m_dl;
	Profile m_profile;
	std::vector<char> m_sel;
	bool m_busy = false;
	bool m_resolved = false;
	std::string m_msg;
	std::string m_query;

	bool init();
	void build();
	void clearNodes();
	void initSel();
	void loadAll();
	void doResolve();
	std::vector<std::string> const& items() const;

	void onFind(CCObject*);
	void onSkip(CCObject*);
	void onNext(CCObject*);
	void onBack(CCObject*);
	void onToggle(CCObject* sender);
	void onStyle(CCObject* sender);

public:
	static SetupPopup* create();
};
