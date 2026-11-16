#pragma once

#include <string>

struct RankingOutcome {
    bool ok;
    std::string message;
};

// Runs ranking/ranking.py as a child process. The Python service reads unrated
// matches from the shared SQLite file, updates Elo ratings and ranks, and exits.
// Designed to be called from std::async so the game loop never freezes.
RankingOutcome runRankingService(const std::string& dbPath);
