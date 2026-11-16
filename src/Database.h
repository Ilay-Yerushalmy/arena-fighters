#pragma once

#include <optional>
#include <string>
#include <vector>

struct sqlite3;

struct PlayerStats {
    int playerId = 0;
    std::string name;
    int wins = 0;
    int losses = 0;
    int draws = 0;
    int matchesPlayed = 0;
    double rating = 1000.0;
    int rank = 0;  // 0 = not ranked yet
};

struct MatchRecord {
    int player1Id;
    int player2Id;
    int character1Id;
    int character2Id;
    std::optional<int> winnerId;  // empty = draw
    double durationSec;
    double p1HpPercent;
    double p2HpPercent;
};

// Thin RAII wrapper around SQLite. All statements are prepared + parameter-bound
// (no string concatenation -> no SQL injection).
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void exec(const std::string& sql);
    void applySqlFile(const std::string& path);

    int getOrCreatePlayer(const std::string& name);
    long long recordMatch(const MatchRecord& match);  // stats are updated by an SQL trigger
    std::optional<PlayerStats> getStats(int playerId);
    std::vector<PlayerStats> leaderboard(int limit);

private:
    sqlite3* db_ = nullptr;
};
