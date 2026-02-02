#pragma once
#include "queue/queue.hpp"

namespace dispatcher::queue {

class UnboundedQueue : public IQueue {
    // здесь ваш код
public:
    explicit UnboundedQueue(int capacity);

    void push(std::function<void()> task) override;

    std::optional<std::function<void()>> try_pop() override;

    ~UnboundedQueue() override;
};

}  // namespace dispatcher::queue