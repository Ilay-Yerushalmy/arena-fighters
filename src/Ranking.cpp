#include "Ranking.h"

#include <cstdlib>

#include "Config.h"

namespace {

std::string pythonExecutable() {
    if (const char* env = std::getenv("ARENA_PYTHON")) return env;
#ifdef _WIN32
    return "python";
#else
    return "python3";
#endif
}

}  // namespace

RankingOutcome runRankingService(const std::string& dbPath) {
    const std::string script = config::projectRoot() + "/ranking/ranking.py";
    std::string command = "\"" + pythonExecutable() + "\" \"" + script + "\" --db \"" + dbPath + "\"";
#ifdef _WIN32
    // cmd.exe strips the outer quotes when a command line starts with a quote.
    command = "\"" + command + "\"";
#endif

    const int exitCode = std::system(command.c_str());
    if (exitCode != 0) {
        return {false, "Ranking service failed (exit code " + std::to_string(exitCode) +
                           "). Stats were saved; ratings will update after the next match."};
    }
    return {true, "Rankings updated"};
}
