#include <exception>
#include <filesystem>
#include <iostream>

#include "Config.h"
#include "Game.h"

int main() {
    try {
        const std::filesystem::path dataDir = std::filesystem::path(config::projectRoot()) / "data";
        std::filesystem::create_directories(dataDir);

        Game game((dataDir / "arena.db").string());
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
