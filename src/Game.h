#pragma once

#include <array>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Database.h"
#include "Match.h"
#include "Ranking.h"
#include "UI.h"

enum class Screen { Menu, CharacterSelect, Fighting, SavingResult, Results, Leaderboard };

// Top-level state machine: menu -> character select -> fight -> results / leaderboard.
class Game {
public:
    explicit Game(const std::string& dbPath);
    void run();

private:
    void update(float dt);
    void draw() const;

    void updateMenu();
    void updateCharacterSelect();
    void updateFighting(float dt);
    void updateSavingResult();
    void updateResults(float dt);
    void updateLeaderboard();

    void drawMenu() const;
    void drawCharacterSelect() const;
    void drawResults() const;
    void drawLeaderboard() const;

    void startMatch();
    void saveMatchResult();
    void openLeaderboard(Screen returnTo);
    Popup makeStatsPopup(int playerIndex, bool rankingOk) const;

    Database db_;
    std::string dbPath_;
    Screen screen_ = Screen::Menu;
    bool quit_ = false;

    // Menu
    std::array<std::string, 2> names_{"Player 1", "Player 2"};
    int activeField_ = 0;
    std::string menuError_;

    // Character select
    std::array<int, 2> cursor_{0, 1};
    std::array<bool, 2> locked_{false, false};

    // Match + persistence
    std::array<int, 2> playerIds_{0, 0};
    std::unique_ptr<Match> match_;
    std::optional<MatchResult> lastResult_;
    std::array<PlayerStats, 2> statsBefore_{};
    std::array<PlayerStats, 2> statsAfter_{};
    std::future<RankingOutcome> rankingJob_;
    std::vector<Popup> popups_;

    // Leaderboard
    std::vector<PlayerStats> leaderboard_;
    Screen leaderboardReturn_ = Screen::Menu;
};
