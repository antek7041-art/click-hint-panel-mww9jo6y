#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <algorithm>
#include <sstream>

using namespace geode::prelude;

static std::string keyFor(GJGameLevel* lvl) {
	return fmt::format("ref_{}", lvl->m_levelID.value());
}
static std::vector<float> parse(std::string const& s) {
	std::vector<float> out;
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, ',')) if (!item.empty()) out.push_back(std::strtof(item.c_str(), nullptr));
	return out;
}
static std::string serialize(std::vector<float> const& v) {
	std::string s;
	for (auto x : v) s += fmt::format("{:.1f},", x);
	return s;
}

class $modify(HintPlayLayer, PlayLayer) {
	struct Fields {
		std::vector<float> reference;
		std::vector<float> current;
		int groundFrames = 0;
		CCLayerColor* panel = nullptr;
		CCLabelBMFont* title = nullptr;
		CCLabelBMFont* info = nullptr;
	};

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
		auto f = m_fields.self();
		f->reference = parse(Mod::get()->getSavedValue<std::string>(keyFor(level), ""));

		auto win = CCDirector::get()->getWinSize();
		f->panel = CCLayerColor::create({0, 0, 0, 150}, 80.f, 60.f);
		f->panel->setPosition({win.width - 90.f, win.height / 2.f - 30.f});

		f->title = CCLabelBMFont::create("KLIK", "bigFont.fnt");
		f->title->setScale(0.45f);
		f->title->setPosition({40.f, 44.f});
		f->panel->addChild(f->title);

		f->info = CCLabelBMFont::create(f->reference.empty() ? "brak\nnagrania" : "-", "goldFont.fnt");
		f->info->setScale(0.5f);
		f->info->setPosition({40.f, 20.f});
		f->panel->addChild(f->info);

		this->addChild(f->panel, 1000);
		return true;
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);
		auto f = m_fields.self();
		if (!m_player1) return;

		// licznik klatek spędzonych na ziemi od lądowania
		if (m_player1->m_isOnGround) f->groundFrames++;
		else f->groundFrames = 0;

		if (!f->panel || f->reference.empty()) return;
		float x = m_player1->getPositionX();
		auto it = std::lower_bound(f->reference.begin(), f->reference.end(), x - 10.f);
		if (it == f->reference.end()) {
			f->info->setString("koniec");
			f->title->setColor({255, 255, 255});
			return;
		}
		float blocks = (*it - x) / 30.f;
		float warn = static_cast<float>(Mod::get()->getSettingValue<double>("warn-distance"));
		if (blocks <= warn) {
			f->title->setString("KLIKNIJ!");
			f->title->setColor({255, 60, 60});
		} else {
			f->title->setString("KLIK");
			f->title->setColor({255, 255, 255});
		}
		f->info->setString(fmt::format("za {:.1f}", blocks).c_str());
	}

	// Kulka z numerem 1-10 w miejscu skoku + dźwięk
	void showBall(CCPoint pos, int frames) {
		auto dot = CCDrawNode::create();
		dot->drawDot({0, 0}, 9.f, {0.2f, 1.f, 0.6f, 1.f});
		auto label = CCLabelBMFont::create(std::to_string(frames).c_str(), "bigFont.fnt");
		label->setScale(0.45f);
		label->setPosition({0.f, 24.f});
		dot->addChild(label);
		dot->setPosition({pos.x, pos.y + 16.f});
		m_objectLayer->addChild(dot, 1000);
		dot->runAction(CCSequence::create(
			CCDelayTime::create(0.8f),
			CCScaleTo::create(0.2f, 0.f),
			CCRemoveSelf::create(),
			nullptr
		));

		if (Mod::get()->getSettingValue<bool>("play-sound")) {
			float vol = static_cast<float>(Mod::get()->getSettingValue<double>("sound-volume"));
			FMODAudioEngine::sharedEngine()->playEffect("chime.wav"_spr, 1.f, 1.f, vol);
		}
	}

	void levelComplete() {
		auto f = m_fields.self();
		if (!f->current.empty()) Mod::get()->setSavedValue<std::string>(keyFor(m_level), serialize(f->current));
		PlayLayer::levelComplete();
	}

	void resetLevel() {
		PlayLayer::resetLevel();
		m_fields->current.clear();
		m_fields->groundFrames = 0;
	}
};

class $modify(GJBaseGameLayer) {
	void handleButton(bool down, int button, bool isPlayer1) {
		GJBaseGameLayer::handleButton(down, button, isPlayer1);
		if (!(down && button == 1 && isPlayer1)) return;
		auto pl = PlayLayer::get();
		if (!pl || !pl->m_player1) return;
		auto h = static_cast<HintPlayLayer*>(pl);
		h->m_fields->current.push_back(pl->m_player1->getPositionX());
		int n = h->m_fields->groundFrames;
		if (pl->m_player1->m_isOnGround && n <= 10) h->showBall(pl->m_player1->getPosition(), std::max(1, n));
	}
};
