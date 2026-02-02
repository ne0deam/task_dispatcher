#pragma once
#include "queue/queue.hpp"
#include <condition_variable>
#include <mutex>
#include <queue>

namespace dispatcher::queue {

class BoundedQueue : public IQueue {
private:
    mutable std::mutex mutex_;          // Защита доступа к внутренним данным
    std::condition_variable not_full_;  // Условие: очередь не полна (для push)
    std::queue<std::function<void()>> queue_;
    const int capacity_;

public:
    explicit BoundedQueue(int capacity);

    void push(std::function<void()> task) override;

    std::optional<std::function<void()>> try_pop() override;

    ~BoundedQueue() override;
};

}  // namespace dispatcher::queue