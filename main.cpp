#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Pengaturan
// ---------------------------------------------------------------------------
struct Cfg {
	bool noclip = false;
	bool speedhack = false;
	bool confirmPractice = false;
	bool confirmExit = false;
	bool confirmEdit = false;
	bool hideAttempts = false;
	bool hidePercent = false;
	float speed = 1.0f;
	bool loaded = false;
};

struct Feature {
	const char* name;
	const char* key;
	bool Cfg::* ptr;
};

static const std::vector<Feature>& features() {
	static const std::vector<Feature> list = {
		{"Noclip",           "noclip",           &Cfg::noclip},
		{"Speedhack",        "speedhack",        &Cfg::speedhack},
		{"Confirm Practice", "confirmPractice",  &Cfg::confirmPractice},
		{"Confirm Exit",     "confirmExit",      &Cfg::confirmExit},
		{"Confirm Edit",     "confirmEdit",      &Cfg::confirmEdit},
		{"Hide Attempts",    "hideAttempts",     &Cfg::hideAttempts},
		{"Hide Percentage",  "hidePercent",      &Cfg::hidePercent},
	};
	return list;
}

static Cfg& cfg() {
	static Cfg c;
	if (!c.loaded) {
		c.loaded = true;
		auto mod = Mod::get();
		for (auto const& f : features()) {
			c.*(f.ptr) = mod->getSavedValue<bool>(f.key);
		}
		float s = mod->getSavedValue<float>("speed");
		c.speed = s > 0.05f ? s : 1.0f;
	}
	return c;
}

// ---------------------------------------------------------------------------
// Menu Coconut (popup bergaya GD)
// ---------------------------------------------------------------------------
class CoconutPopup : public Popup {
protected:
	CCLabelBMFont* m_speedLabel = nullptr;

	bool init() {
		if (!Popup::init(340.f, 270.f)) return false;
		this->setTitle("Coconut Mod");

		auto size = m_mainLayer->getContentSize();
		float viewW = size.width - 30.f;
		float viewH = size.height - 60.f;

		auto scroll = ScrollLayer::create({viewW, viewH});
		scroll->setPosition({15.f, 15.f});
		m_mainLayer->addChild(scroll);

		auto const& fs = features();
		const float rowH = 38.f;
		int rows = static_cast<int>(fs.size()) + 1;
		float contentH = std::max(viewH, rows * rowH + 10.f);
		scroll->m_contentLayer->setContentSize({viewW, contentH});

		auto menu = CCMenu::create();
		menu->setContentSize({viewW, contentH});
		menu->setPosition({0.f, 0.f});
		scroll->m_contentLayer->addChild(menu);

		for (int i = 0; i < static_cast<int>(fs.size()); ++i) {
			float y = contentH - 24.f - i * rowH;

			auto label = CCLabelBMFont::create(fs[i].name, "bigFont.fnt");
			label->setScale(.5f);
			label->setAnchorPoint({0.f, .5f});
			label->setPosition({14.f, y});
			scroll->m_contentLayer->addChild(label);

			auto off = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
			auto on = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
			auto toggler = CCMenuItemToggler::create(off, on, this, menu_selector(CoconutPopup::onToggle));
			toggler->setTag(i);
			toggler->setScale(.8f);
			toggler->setPosition({viewW - 34.f, y});
			toggler->toggle(cfg().*(fs[i].ptr));
			menu->addChild(toggler);
		}

		// Baris pengatur kecepatan (untuk Speedhack)
		float ySpeed = contentH - 24.f - static_cast<int>(fs.size()) * rowH;
		m_speedLabel = CCLabelBMFont::create("", "bigFont.fnt");
		m_speedLabel->setScale(.5f);
		m_speedLabel->setAnchorPoint({0.f, .5f});
		m_speedLabel->setPosition({14.f, ySpeed});
		scroll->m_contentLayer->addChild(m_speedLabel);
		this->refreshSpeedLabel();

		auto minusSpr = ButtonSprite::create("-", 30, true, "bigFont.fnt", "GJ_button_01.png", 28.f, .6f);
		auto minusBtn = CCMenuItemSpriteExtra::create(minusSpr, this, menu_selector(CoconutPopup::onSpeed));
		minusBtn->setTag(0);
		minusBtn->setPosition({viewW - 80.f, ySpeed});
		menu->addChild(minusBtn);

		auto plusSpr = ButtonSprite::create("+", 30, true, "bigFont.fnt", "GJ_button_01.png", 28.f, .6f);
		auto plusBtn = CCMenuItemSpriteExtra::create(plusSpr, this, menu_selector(CoconutPopup::onSpeed));
		plusBtn->setTag(1);
		plusBtn->setPosition({viewW - 34.f, ySpeed});
		menu->addChild(plusBtn);

		scroll->moveToTop();
		return true;
	}

	void refreshSpeedLabel() {
		char buf[48];
		std::snprintf(buf, sizeof(buf), "Speed: %.1fx", cfg().speed);
		m_speedLabel->setString(buf);
	}

	void onToggle(CCObject* sender) {
		int idx = static_cast<CCNode*>(sender)->getTag();
		auto const& fs = features();
		if (idx < 0 || idx >= static_cast<int>(fs.size())) return;
		bool nowOn = !static_cast<CCMenuItemToggler*>(sender)->isToggled();
		cfg().*(fs[idx].ptr) = nowOn;
		Mod::get()->setSavedValue<bool>(fs[idx].key, nowOn);
	}

	void onSpeed(CCObject* sender) {
		int dir = static_cast<CCNode*>(sender)->getTag() == 1 ? 1 : -1;
		float s = cfg().speed + dir * 0.1f;
		s = std::round(s * 10.f) / 10.f;
		s = std::max(0.1f, std::min(5.0f, s));
		cfg().speed = s;
		Mod::get()->setSavedValue<float>("speed", s);
		this->refreshSpeedLabel();
	}

public:
	static CoconutPopup* create() {
		auto ret = new CoconutPopup();
		if (ret->init()) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};

// ---------------------------------------------------------------------------
// Speedhack
// ---------------------------------------------------------------------------
class $modify(CoconutScheduler, CCScheduler) {
	void update(float dt) {
		auto& c = cfg();
		if (c.speedhack && c.speed > 0.f) dt *= c.speed;
		CCScheduler::update(dt);
	}
};

// ---------------------------------------------------------------------------
// Noclip + sembunyikan label di dalam level
// ---------------------------------------------------------------------------
class $modify(CoconutPlayLayer, PlayLayer) {
	void destroyPlayer(PlayerObject* player, GameObject* object) {
		if (cfg().noclip) return;
		PlayLayer::destroyPlayer(player, object);
	}

	void setupHasCompleted() {
		PlayLayer::setupHasCompleted();
		auto& c = cfg();
		if (c.hideAttempts && m_attemptLabel) m_attemptLabel->setVisible(false);
		if (c.hidePercent && m_percentageLabel) m_percentageLabel->setVisible(false);
	}
};

// ---------------------------------------------------------------------------
// Menu pause: tombol Coconut, Confirm Practice, Confirm Exit
// ---------------------------------------------------------------------------
static bool s_allowQuit = false;
static bool s_allowPractice = false;

class $modify(CoconutPause, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();
		s_allowQuit = false;
		s_allowPractice = false;

		auto win = CCDirector::get()->getWinSize();
		auto spr = ButtonSprite::create("Coconut");
		spr->setScale(.6f);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CoconutPause::onCoconut));
		auto menu = CCMenu::create();
		menu->setPosition({58.f, win.height - 22.f});
		menu->addChild(btn);
		this->addChild(menu, 100);
	}

	void onCoconut(CCObject*) {
		CoconutPopup::create()->show();
	}

	void onQuit(CCObject* sender) {
		if (!cfg().confirmExit || s_allowQuit) return PauseLayer::onQuit(sender);
		createQuickPopup(
			"Exit Level", "Are you sure you want to exit this level?", "No", "Yes",
			[this, sender](FLAlertLayer*, bool yes) {
				if (yes) {
					s_allowQuit = true;
					this->onQuit(sender);
				}
			}
		);
	}

	void tryQuit(CCObject* sender) {
		if (!cfg().confirmExit || s_allowQuit) return PauseLayer::tryQuit(sender);
		createQuickPopup(
			"Exit Level", "Are you sure you want to exit this level?", "No", "Yes",
			[this, sender](FLAlertLayer*, bool yes) {
				if (yes) {
					s_allowQuit = true;
					this->tryQuit(sender);
				}
			}
		);
	}

	void onPracticeMode(CCObject* sender) {
		if (!cfg().confirmPractice || s_allowPractice) return PauseLayer::onPracticeMode(sender);
		createQuickPopup(
			"Practice Mode", "Do you want to enter practice mode?", "No", "Yes",
			[this, sender](FLAlertLayer*, bool yes) {
				if (yes) {
					s_allowPractice = true;
					this->onPracticeMode(sender);
				}
			}
		);
	}
};

// ---------------------------------------------------------------------------
// Confirm Edit (dari layar level milikmu)
// ---------------------------------------------------------------------------
static bool s_allowEdit = false;

class $modify(CoconutEdit, EditLevelLayer) {
	void onEdit(CCObject* sender) {
		if (!cfg().confirmEdit || s_allowEdit) return EditLevelLayer::onEdit(sender);
		createQuickPopup(
			"Edit Level", "Do you want to open this level in the editor?", "No", "Yes",
			[this, sender](FLAlertLayer*, bool yes) {
				if (yes) {
					s_allowEdit = true;
					this->onEdit(sender);
					s_allowEdit = false;
				}
			}
		);
	}
};

// ---------------------------------------------------------------------------
// Tombol Coconut di menu utama
// ---------------------------------------------------------------------------
class $modify(CoconutMenu, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;

		auto win = CCDirector::get()->getWinSize();
		auto spr = ButtonSprite::create("Coconut");
		spr->setScale(.6f);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CoconutMenu::onCoconut));
		auto menu = CCMenu::create();
		menu->setPosition({win.width - 58.f, win.height - 22.f});
		menu->addChild(btn);
		this->addChild(menu, 100);
		return true;
	}

	void onCoconut(CCObject*) {
		CoconutPopup::create()->show();
	}
};
