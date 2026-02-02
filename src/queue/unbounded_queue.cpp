#include "queue/unbounded_queue.hpp"
#include <utility>

namespace dispatcher::queue {

UnboundedQueue::UnboundedQueue() {}

void UnboundedQueue::push(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(task));  // Добавляем задачу в очередь
}

std::optional<std::function<void()>> UnboundedQueue::try_pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return std::nullopt;
    }

    auto task = std::move(queue_.front());  // Извлекаем задачу
    queue_.pop();
    return task;
}

}  // namespace dispatcher::queue