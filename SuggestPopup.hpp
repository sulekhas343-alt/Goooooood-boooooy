#pragma once

#include "Types.hpp"

class SuggestPopup : public Popup, public LevelManagerDelegate {
protected:
	Cat m_cat = Cat::Easy;
	std::vector<Ref<CCNode>> m_nodes;
	CCLabelBMFont* m_status = nullptr;
	std::vector<LevelInfo> m_shown;
	std::string m_note;
	int m_page = 0;
	bool m_busy = false;
	async::TaskHolder<web::WebResponse> m_dl;

	// level picking state
	std::vector<LevelInfo> m_order;
	std::vector<LevelInfo> m_pending;
	std::vector<LevelInfo> m_accepted;
	size_t m_cursor = 0;
	size_t m_batchStart = 0;
	size_t m_acceptedBefore = 0;
	int m_batches = 0;
	int m_fails = 0;
	int m_target = 10;
	int m_minLikes = 4000;
	bool m_generating = false;
	bool m_likesUnavailable = false;

	bool init();
	void selectCat(Cat c);
	void ensureSource();
	void prepare();
	void startGenerate(bool reroll);
	void fetchNextBatch();
	void onBatch(CCArray* levels);
	void finish();
	void rebuild();
	void setStatus(std::string const& s);
	void releaseDelegate();

	void onTab(CCObject* sender);
	void onReroll(CCObject*);
	void onProfile(CCObject*);
	void onPrev(CCObject*);
	void onNext(CCObject*);
	void onPick(CCObject* sender);
	void onOpenLevel(CCObject* sender);

	// GD server callbacks (both signature variants so it works across GD versions)
	void loadLevelsFinished(CCArray* levels, char const* key, int);
	void loadLevelsFailed(char const* key, int);
	void loadLevelsFinished(CCArray* levels, char const* key);
	void loadLevelsFailed(char const* key);

public:
	static SuggestPopup* create();
	~SuggestPopup() override;
};
