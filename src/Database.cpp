#include "Database.h"

#include <sqlite3.h>

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

// RAII prepared statement: finalized automatically, throws on any error.
class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(std::string("SQL prepare failed: ") + sqlite3_errmsg(db));
        }
    }
    ~Statement() { sqlite3_finalize(stmt_); }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement& bind(int index, int value) {
        check(sqlite3_bind_int(stmt_, index, value));
        return *this;
    }
    Statement& bind(int index, double value) {
        check(sqlite3_bind_double(stmt_, index, value));
        return *this;
    }
    Statement& bind(int index, const std::string& value) {
        check(sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT));
        return *this;
    }
    Statement& bindNull(int index) {
        check(sqlite3_bind_null(stmt_, index));
        return *this;
    }

    // Returns true while there are rows to read.
    bool step() {
        const int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW) return true;
        if (rc == SQLITE_DONE) return false;
        throw std::runtime_error(std::string("SQL step failed: ") + sqlite3_errmsg(db_));
    }

    int columnInt(int col) const { return sqlite3_column_int(stmt_, col); }
    double columnDouble(int col) const { return sqlite3_column_double(stmt_, col); }
    std::string columnText(int col) const {
        const unsigned char* text = sqlite3_column_text(stmt_, col);
        return text ? reinterpret_cast<const char*>(text) : "";
    }

private:
    void check(int rc) {
        if (rc != SQLITE_OK) throw std::runtime_error(std::string("SQL bind failed: ") + sqlite3_errmsg(db_));
    }

    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open file: " + path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

PlayerStats readStatsRow(const Statement& s) {
    PlayerStats p;
    p.playerId = s.columnInt(0);
    p.name = s.columnText(1);
    p.wins = s.columnInt(2);
    p.losses = s.columnInt(3);
    p.draws = s.columnInt(4);
    p.matchesPlayed = s.columnInt(5);
    p.rating = s.columnDouble(6);
    p.rank = s.columnInt(7);  // NULL reads as 0 = unranked
    return p;
}

}  // namespace

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        const std::string message = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw std::runtime_error("Cannot open database '" + path + "': " + message);
    }
    sqlite3_busy_timeout(db_, 3000);  // the Python ranking process may hold a short write lock
    exec("PRAGMA foreign_keys = ON;");
}

Database::~Database() {
    sqlite3_close(db_);
}

void Database::exec(const std::string& sql) {
    char* error = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error ? error : "unknown error";
        sqlite3_free(error);
        throw std::runtime_error("SQL exec failed: " + message);
    }
}

void Database::applySqlFile(const std::string& path) {
    exec(readFile(path));
}

int Database::getOrCreatePlayer(const std::string& name) {
    Statement insert(db_, "INSERT OR IGNORE INTO players (name) VALUES (?);");
    insert.bind(1, name).step();

    Statement select(db_, "SELECT id FROM players WHERE name = ?;");
    select.bind(1, name);
    if (!select.step()) throw std::runtime_error("Player was not created: " + name);
    return select.columnInt(0);
}

long long Database::recordMatch(const MatchRecord& m) {
    Statement s(db_,
                "INSERT INTO matches (player1_id, player2_id, character1_id, character2_id, winner_id,"
                "                     duration_sec, p1_hp_pct, p2_hp_pct)"
                " VALUES (?, ?, ?, ?, ?, ?, ?, ?);");
    s.bind(1, m.player1Id).bind(2, m.player2Id).bind(3, m.character1Id).bind(4, m.character2Id);
    if (m.winnerId) {
        s.bind(5, *m.winnerId);
    } else {
        s.bindNull(5);
    }
    s.bind(6, m.durationSec).bind(7, m.p1HpPercent).bind(8, m.p2HpPercent);
    s.step();
    return sqlite3_last_insert_rowid(db_);
}

std::optional<PlayerStats> Database::getStats(int playerId) {
    Statement s(db_,
                "SELECT p.id, p.name, s.wins, s.losses, s.draws, s.matches_played, s.rating, s.rank"
                "  FROM players p JOIN player_stats s ON s.player_id = p.id"
                " WHERE p.id = ?;");
    s.bind(1, playerId);
    if (!s.step()) return std::nullopt;
    return readStatsRow(s);
}

std::vector<PlayerStats> Database::leaderboard(int limit) {
    Statement s(db_,
                "SELECT player_id, name, wins, losses, draws, matches_played, rating, rank"
                "  FROM leaderboard WHERE matches_played > 0 LIMIT ?;");
    s.bind(1, limit);
    std::vector<PlayerStats> rows;
    while (s.step()) rows.push_back(readStatsRow(s));
    return rows;
}
