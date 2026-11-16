#include "Game.h"

#include <algorithm>
#include <cctype>
#include <chrono>

#include "Characters.h"
#include "Config.h"

namespace {

constexpr size_t kMaxNameLength = 16;
const Color kP1Color{80, 160, 255, 255};
const Color kP2Color{255, 90, 90, 255};

std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(' ');
    if (begin == std::string::npos) return "";
    const auto end = s.find_last_not_of(' ');
    return s.substr(begin, end - begin + 1);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

}  // namespace

Game::Game(const std::string& dbPath) : db_(dbPath), dbPath_(dbPath) {
    const std::string root = config::projectRoot();
    db_.applySqlFile(root + "/db/schema.sql");
    db_.applySqlFile(root + "/db/seed.sql");
}

void Game::run() {
    InitWindow(config::kScreenWidth, config::kScreenHeight, "Arena Fighters");
    SetExitKey(KEY_NULL);  // Esc is handled per screen
    SetTargetFPS(60);

    while (!quit_ && !WindowShouldClose()) {
        const float dt = std::min(GetFrameTime(), 1.0f / 30.0f);
        update(dt);

        BeginDrawing();
        ClearBackground(Color{18, 18, 24, 255});
        draw();
        EndDrawing();
    }

    if (rankingJob_.valid()) rankingJob_.wait();
    CloseWindow();
}

void Game::update(float dt) {
    switch (screen_) {
        case Screen::Menu: updateMenu(); break;
        case Screen::CharacterSelect: updateCharacterSelect(); break;
        case Screen::Fighting: updateFighting(dt); break;
        case Screen::SavingResult: updateSavingResult(); break;
        case Screen::Results: updateResults(dt); break;
        case Screen::Leaderboard: updateLeaderboard(); break;
    }
}

void Game::draw() const {
    switch (screen_) {
        case Screen::Menu: drawMenu(); break;
        case Screen::CharacterSelect: drawCharacterSelect(); break;
        case Screen::Fighting: match_->draw(); break;
        case Screen::SavingResult:
            match_->draw();
            DrawRectangle(0, 0, config::kScreenWidth, config::kScreenHeight, Fade(BLACK, 0.6f));
            ui::drawCenteredText("Saving match and updating rankings...", 340, 30, RAYWHITE);
            break;
        case Screen::Results: drawResults(); break;
        case Screen::Leaderboard: drawLeaderboard(); break;
    }
}

// ---------------------------------------------------------------------------
// Menu (player name entry)
// ---------------------------------------------------------------------------

void Game::updateMenu() {
    std::string& field = names_[activeField_];
    for (int c = GetCharPressed(); c > 0; c = GetCharPressed()) {
        if (c >= 32 && c <= 125 && field.size() < kMaxNameLength) field.push_back(static_cast<char>(c));
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !field.empty()) field.pop_back();
    if (IsKeyPressed(KEY_TAB)) activeField_ = 1 - activeField_;
    if (IsKeyPressed(KEY_F1)) openLeaderboard(Screen::Menu);
    if (IsKeyPressed(KEY_ESCAPE)) quit_ = true;

    if (IsKeyPressed(KEY_ENTER)) {
        const std::string a = trim(names_[0]);
        const std::string b = trim(names_[1]);
        if (a.empty() || b.empty()) {
            menuError_ = "Both players need a name.";
        } else if (toLower(a) == toLower(b)) {
            menuError_ = "Players must have different names.";
        } else {
            names_ = {a, b};
            playerIds_ = {db_.getOrCreatePlayer(a), db_.getOrCreatePlayer(b)};
            menuError_.clear();
            locked_ = {false, false};
            screen_ = Screen::CharacterSelect;
        }
    }
}

void Game::drawMenu() const {
    ui::drawCenteredText("ARENA FIGHTERS", 90, 80, GOLD);
    ui::drawCenteredText("Local 1v1 fighting game", 180, 24, LIGHTGRAY);

    for (int i = 0; i < 2; ++i) {
        const int y = 280 + i * 90;
        const Rectangle box{440, static_cast<float>(y), 400, 50};
        const bool active = i == activeField_;
        DrawText(i == 0 ? "Player 1" : "Player 2", 300, y + 14, 24, i == 0 ? kP1Color : kP2Color);
        DrawRectangleRec(box, active ? Color{50, 50, 70, 255} : Color{35, 35, 45, 255});
        DrawRectangleLinesEx(box, 2.0f, active ? GOLD : GRAY);
        std::string shown = names_[i];
        if (active && static_cast<int>(GetTime() * 2) % 2 == 0) shown += "_";
        DrawText(shown.c_str(), 455, y + 12, 26, RAYWHITE);
    }

    if (!menuError_.empty()) ui::drawCenteredText(menuError_, 480, 22, RED);
    ui::drawCenteredText("[Tab] switch player   [Enter] continue   [F1] leaderboard   [Esc] quit", 560, 20, GRAY);
}

// ---------------------------------------------------------------------------
// Character select
// ---------------------------------------------------------------------------

void Game::updateCharacterSelect() {
    struct SelectKeys { KeyboardKey left, right, up, down, confirm; };
    constexpr SelectKeys keys[2] = {{KEY_A, KEY_D, KEY_W, KEY_S, KEY_F},
                                    {KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_I}};

    for (int i = 0; i < 2; ++i) {
        if (locked_[i]) continue;
        int& c = cursor_[i];
        if (IsKeyPressed(keys[i].left)) c = (c + kCharacterCount - 1) % kCharacterCount;
        if (IsKeyPressed(keys[i].right)) c = (c + 1) % kCharacterCount;
        if (IsKeyPressed(keys[i].up) || IsKeyPressed(keys[i].down)) c = (c + 4) % kCharacterCount;
        if (IsKeyPressed(keys[i].confirm)) locked_[i] = true;
    }

    if (IsKeyPressed(KEY_ESCAPE)) screen_ = Screen::Menu;
    if (locked_[0] && locked_[1]) startMatch();
}

void Game::drawCharacterSelect() const {
    ui::drawCenteredText("CHOOSE YOUR FIGHTER", 40, 48, GOLD);
    const auto& roster = characterRoster();

    constexpr int cardW = 260, cardH = 190, gap = 20;
    constexpr int startX = (config::kScreenWidth - 4 * cardW - 3 * gap) / 2;
    constexpr int startY = 130;

    for (int idx = 0; idx < kCharacterCount; ++idx) {
        const CharacterDef& c = roster[idx];
        const float x = static_cast<float>(startX + (idx % 4) * (cardW + gap));
        const float y = static_cast<float>(startY + (idx / 4) * (cardH + gap));
        const Rectangle card{x, y, cardW, cardH};

        DrawRectangleRec(card, Color{35, 35, 48, 255});
        DrawRectangle(static_cast<int>(x), static_cast<int>(y), cardW, 8, c.color);
        DrawText(c.name.c_str(), static_cast<int>(x + 14), static_cast<int>(y + 18), 28, c.color);
        DrawText(TextFormat("HP %d  SPD %d  ATK %d", static_cast<int>(c.maxHp), static_cast<int>(c.moveSpeed),
                            static_cast<int>(c.attackDamage)),
                 static_cast<int>(x + 14), static_cast<int>(y + 56), 16, LIGHTGRAY);
        DrawText(TextFormat("Skill: %s (%.1fs)", c.skill.name.c_str(), c.skill.cooldown), static_cast<int>(x + 14),
                 static_cast<int>(y + 84), 16, RAYWHITE);
        DrawText(TextFormat("Ultimate: %s", c.ultimate.name.c_str()), static_cast<int>(x + 14),
                 static_cast<int>(y + 108), 16, GOLD);
        DrawText(c.description.c_str(), static_cast<int>(x + 14), static_cast<int>(y + 140), 13, GRAY);

        for (int p = 0; p < 2; ++p) {
            if (cursor_[p] != idx) continue;
            const float inset = p == 0 ? -4.0f : -10.0f;
            const Rectangle outline{x + inset, y + inset, cardW - 2 * inset, cardH - 2 * inset};
            DrawRectangleLinesEx(outline, locked_[p] ? 5.0f : 3.0f, p == 0 ? kP1Color : kP2Color);
        }
    }

    for (int p = 0; p < 2; ++p) {
        const std::string status = names_[p] + (locked_[p] ? "  -  LOCKED IN" : "  -  choosing...");
        DrawText(status.c_str(), p == 0 ? 90 : 700, 560, 24, p == 0 ? kP1Color : kP2Color);
    }
    ui::drawCenteredText("P1: WASD + [F] lock      P2: Arrows + [I] lock      [Esc] back", 640, 20, GRAY);
}

// ---------------------------------------------------------------------------
// Fight + saving
// ---------------------------------------------------------------------------

void Game::startMatch() {
    const auto& roster = characterRoster();
    match_ = std::make_unique<Match>(roster[cursor_[0]], roster[cursor_[1]], names_);
    popups_.clear();
    screen_ = Screen::Fighting;
}

void Game::updateFighting(float dt) {
    match_->update(dt);
    if (match_->finished()) saveMatchResult();
}

void Game::saveMatchResult() {
    const MatchResult r = match_->result();
    lastResult_ = r;
    const auto& roster = characterRoster();

    for (int i = 0; i < 2; ++i) statsBefore_[i] = db_.getStats(playerIds_[i]).value_or(PlayerStats{});

    MatchRecord record{};
    record.player1Id = playerIds_[0];
    record.player2Id = playerIds_[1];
    record.character1Id = roster[cursor_[0]].id;
    record.character2Id = roster[cursor_[1]].id;
    if (r.winnerIndex >= 0) record.winnerId = playerIds_[r.winnerIndex];
    record.durationSec = r.durationSec;
    record.p1HpPercent = r.hpPercentLeft[0];
    record.p2HpPercent = r.hpPercentLeft[1];

    db_.recordMatch(record);  // AFTER INSERT trigger updates wins / losses / draws

    // Python Elo service runs in the background so the window keeps rendering.
    rankingJob_ = std::async(std::launch::async, runRankingService, dbPath_);
    screen_ = Screen::SavingResult;
}

void Game::updateSavingResult() {
    using namespace std::chrono_literals;
    if (rankingJob_.wait_for(0ms) != std::future_status::ready) return;

    const RankingOutcome outcome = rankingJob_.get();
    for (int i = 0; i < 2; ++i) statsAfter_[i] = db_.getStats(playerIds_[i]).value_or(PlayerStats{});

    popups_.clear();
    popups_.push_back(makeStatsPopup(0, outcome.ok));
    popups_.push_back(makeStatsPopup(1, outcome.ok));
    if (!outcome.ok) {
        Popup warning{"Ranking service", {outcome.message}, ORANGE};
        warning.delay = 0.6f;
        popups_.push_back(warning);
    }
    screen_ = Screen::Results;
}

Popup Game::makeStatsPopup(int i, bool rankingOk) const {
    const PlayerStats& before = statsBefore_[i];
    const PlayerStats& after = statsAfter_[i];
    const int winner = lastResult_ ? lastResult_->winnerIndex : -1;

    Popup p;
    p.delay = 0.25f * static_cast<float>(i);
    if (winner == -1) {
        p.title = names_[i] + "  -  DRAW";
        p.accent = LIGHTGRAY;
    } else if (winner == i) {
        p.title = names_[i] + "  -  VICTORY";
        p.accent = GOLD;
    } else {
        p.title = names_[i] + "  -  DEFEAT";
        p.accent = Color{220, 80, 80, 255};
    }

    p.lines.push_back(TextFormat("Wins %d   Losses %d   Draws %d", after.wins, after.losses, after.draws));
    if (lastResult_) p.lines.push_back(TextFormat("Damage dealt: %.0f", lastResult_->damageDealt[i]));

    if (!rankingOk) {
        p.lines.emplace_back("Rating: update pending");
        return p;
    }
    p.lines.push_back(TextFormat("Rating %.0f  (%+.0f)", after.rating, after.rating - before.rating));
    const int moved = before.rank - after.rank;  // positive = climbed
    if (before.rank == 0) {
        p.lines.push_back(TextFormat("Rank #%d  (new on the board)", after.rank));
    } else if (moved > 0) {
        p.lines.push_back(TextFormat("Rank #%d  (up %d)", after.rank, moved));
    } else if (moved < 0) {
        p.lines.push_back(TextFormat("Rank #%d  (down %d)", after.rank, -moved));
    } else {
        p.lines.push_back(TextFormat("Rank #%d  (no change)", after.rank));
    }
    return p;
}

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------

void Game::updateResults(float dt) {
    for (Popup& p : popups_) p.age += dt;

    if (IsKeyPressed(KEY_ENTER)) startMatch();
    if (IsKeyPressed(KEY_C)) {
        locked_ = {false, false};
        screen_ = Screen::CharacterSelect;
    }
    if (IsKeyPressed(KEY_F1)) openLeaderboard(Screen::Results);
    if (IsKeyPressed(KEY_M)) screen_ = Screen::Menu;
}

void Game::drawResults() const {
    const int winner = lastResult_ ? lastResult_->winnerIndex : -1;
    const std::string headline = winner == -1 ? "DRAW!" : names_[winner] + " WINS!";
    ui::drawCenteredText(headline, 60, 64, GOLD);
    if (lastResult_) {
        ui::drawCenteredText(TextFormat("Match length: %.1f seconds", lastResult_->durationSec), 140, 22, LIGHTGRAY);
    }

    if (popups_.size() >= 2) {
        ui::drawPopup(popups_[0], 90, 210, 520);
        ui::drawPopup(popups_[1], 670, 210, 520);
    }
    if (popups_.size() >= 3) ui::drawPopup(popups_[2], 240, 440, 800);

    ui::drawCenteredText("[Enter] rematch   [C] change fighters   [F1] leaderboard   [M] menu", 640, 20, GRAY);
}

// ---------------------------------------------------------------------------
// Leaderboard
// ---------------------------------------------------------------------------

void Game::openLeaderboard(Screen returnTo) {
    leaderboard_ = db_.leaderboard(10);
    leaderboardReturn_ = returnTo;
    screen_ = Screen::Leaderboard;
}

void Game::updateLeaderboard() {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_F1)) screen_ = leaderboardReturn_;
}

void Game::drawLeaderboard() const {
    ui::drawCenteredText("LEADERBOARD", 50, 56, GOLD);

    constexpr int cols[] = {200, 290, 610, 740, 830, 920, 1010};
    const char* headers[] = {"#", "Player", "Rating", "W", "L", "D", "Win %"};
    for (int c = 0; c < 7; ++c) DrawText(headers[c], cols[c], 140, 22, GRAY);
    DrawLine(190, 170, 1090, 170, GRAY);

    if (leaderboard_.empty()) {
        ui::drawCenteredText("No ranked matches yet - go fight!", 260, 26, LIGHTGRAY);
    }

    for (size_t i = 0; i < leaderboard_.size(); ++i) {
        const PlayerStats& p = leaderboard_[i];
        const int y = 185 + static_cast<int>(i) * 40;
        const Color color = i == 0 ? GOLD : RAYWHITE;
        const double winRate = p.matchesPlayed > 0 ? 100.0 * p.wins / p.matchesPlayed : 0.0;

        DrawText(p.rank > 0 ? TextFormat("%d", p.rank) : "-", cols[0], y, 24, color);
        DrawText(p.name.c_str(), cols[1], y, 24, color);
        DrawText(TextFormat("%.0f", p.rating), cols[2], y, 24, color);
        DrawText(TextFormat("%d", p.wins), cols[3], y, 24, color);
        DrawText(TextFormat("%d", p.losses), cols[4], y, 24, color);
        DrawText(TextFormat("%d", p.draws), cols[5], y, 24, color);
        DrawText(TextFormat("%.1f", winRate), cols[6], y, 24, color);
    }

    ui::drawCenteredText("[Esc] back", 650, 20, GRAY);
}
