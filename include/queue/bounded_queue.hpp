#pragma once
#include "queue/queue.hpp"

namespace dispatcher::queue {

class BoundedQueue : public IQueue {
    // здесь ваш код
public:
    explicit BoundedQueue(int capacity);

    void push(std::function<void()> task) override;

    std::optional<std::function<void()>> try_pop() override;

    ~BoundedQueue() override;
};

}  // namespace dispatcher::queue