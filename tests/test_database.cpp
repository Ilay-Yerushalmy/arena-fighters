// Headless tests for the C++ persistence layer (no window needed).
#include <cstdlib>
#include <iostream>
#include <string>

#include "Database.h"

namespace {

int failures = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::cerr << "FAILED: " #cond " (" << __FILE__ << ':' << __LINE__ << ")\n"; \
            ++failures;                                                             \
        }                                                                           \
    } while (0)

}  // namespace

int main() {
    {
        Database db(":memory:");
        db.applySqlFile(std::string(ARENA_DEFAULT_ROOT) + "/db/schema.sql");
        db.applySqlFile(std::string(ARENA_DEFAULT_ROOT) + "/db/seed.sql");

        const int a = db.getOrCreatePlayer("Ilay");
        const int b = db.getOrCreatePlayer("Dana");
        CHECK(a != b);
        CHECK(db.getOrCreatePlayer("ilay") == a);  // names are case-insensitive

        auto stats = db.getStats(a);
        CHECK(stats.has_value());
        CHECK(stats->matchesPlayed == 0);
        CHECK(stats->rank == 0);

        db.recordMatch({a, b, 1, 2, a, 45.0, 60.0, 0.0});
        CHECK(db.getStats(a)->wins == 1);
        CHECK(db.getStats(b)->losses == 1);

        db.recordMatch({a, b, 3, 4, std::nullopt, 99.0, 40.0, 40.0});
        CHECK(db.getStats(a)->draws == 1);
        CHECK(db.getStats(b)->matchesPlayed == 2);

        CHECK(db.leaderboard(10).size() == 2);
    }

    if (failures == 0) {
        std::cout << "All database tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " check(s) failed\n";
    return EXIT_FAILURE;
}
