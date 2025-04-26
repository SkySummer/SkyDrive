#ifndef UTILS_SIGNAL_HANDLER_H
#define UTILS_SIGNAL_HANDLER_H

#include <atomic>
#include <csignal>

class SignalHandler {
public:
    [[nodiscard]] static std::atomic<bool>& setup() {
        static std::atomic running{true};

        struct sigaction action {};
        action.sa_handler = [](int /*signum*/) { running.store(false, std::memory_order_relaxed); };
        action.sa_flags = SA_RESTART;
        sigemptyset(&action.sa_mask);

        sigaction(SIGINT, &action, nullptr);
        sigaction(SIGTERM, &action, nullptr);
        sigaction(SIGHUP, &action, nullptr);
        sigaction(SIGQUIT, &action, nullptr);

        return running;
    }
};

#endif  // UTILS_SIGNAL_HANDLER_H
