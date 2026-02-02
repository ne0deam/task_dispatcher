#include <chrono>
#include <climits>
#include <iostream>
#include <print>
#include <thread>

#include "logger.hpp"
#include "task_dispatcher.hpp"

using namespace dispatcher;

int main() {
    TaskDispatcher td(std::thread::hardware_concurrency());
    std::vector<std::jthread> threads;

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < 10; j++) {
                td.schedule(TaskPriority::Normal,
                            [=]() { Logger::Get().Log("Normal priority message №" + std::to_string(10 * i + j)); });
                td.schedule(TaskPriority::High,
                            [=]() { Logger::Get().Log("High priority message №" + std::to_string(10 * i + j)); });
            }
        });
    }
}
