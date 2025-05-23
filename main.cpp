#include <cstdint>
#include <iostream>

#include "core/server.h"
#include "core/static_file.h"
#include "core/threadpool.h"
#include "user/session_manager.h"
#include "user/user_manager.h"
#include "utils/config_parser.h"
#include "utils/logger.h"
#include "utils/signal_handler.h"

#define STR_HELPER(x) #x      // NOLINT(cppcoreguidelines-macro-usage)
#define STR(x) STR_HELPER(x)  // NOLINT(cppcoreguidelines-macro-usage)

std::filesystem::path getRootPath() {
#ifdef ROOT_PATH
    return STR(ROOT_PATH);
#else
    return std::filesystem::current_path();
#endif
}

int main() {
    try {
        auto& running = SignalHandler::setup();

        auto root_path = getRootPath();

        const ConfigParser config(root_path / "config.ini");

        Logger logger(config.getLogLevel());
        logger.logDivider("Server init");

        ThreadPool thread_pool(config.get("thread_count", 4), &logger);

        SessionManager session_manager;

        const std::string static_dir = config.get("static_dir", std::string("static"));
        const std::string drive_dir = config.get("drive_dir", std::string("files"));
        StaticFile static_file(root_path, static_dir, drive_dir, &logger, &session_manager);

        const std::string user_file = config.get("user_file", std::string("users.dat"));
        const std::filesystem::path user_path = weakly_canonical(root_path / "data" / user_file);
        UserManager user_manager(user_path, &logger, &session_manager, drive_dir);

        const uint16_t port = config.get("port", 8080);
        const bool linger = config.get("linger", true);
        Server server(port, linger, running, &logger, &thread_pool, &static_file, &user_manager);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Server crashed: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
