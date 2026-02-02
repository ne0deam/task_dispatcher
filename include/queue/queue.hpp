#pragma once
#include <functional>
#include <optional>

namespace dispatcher::queue {

struct QueueOptions {
    bool bounded;
    std::optional<int> capacity;
};

class IQueue {
public:
    virtual ~IQueue() = default;
    virtual void push(std::function<void()> task) = 0;
    virtual std::optional<std::function<void()>> try_pop() = 0;
};

}  // namespace dispatcher::queue