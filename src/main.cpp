#include "matching_engine/matching_engine.hpp"
#include "server/server.hpp"

#include <csignal>

namespace me::matching {
    MatchingEngine* g_engine = nullptr;
    void signal_handler(int) { if (g_engine) g_engine->stop(); }
}

int main(int argc, char* argv[]) {
    using namespace me::matching;

    int port = 3001;
    if (argc > 1) port = std::stoi(argv[1]);

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    MatchingEngine engine;
    g_engine = &engine;

    engine.start();
    start_server(engine, port);
    engine.stop();

    return 0;
}
