#include "queue/bounded_queue.hpp"
#include <stdexcept>
#include <utility>

namespace dispatcher::queue {

BoundedQueue::BoundedQueue(int capacity)
    : capacity_(capacity > 0 ? capacity : throw std::invalid_argument("Capacity must be positive")) {}

void BoundedQueue::push(std::function<void()> task) {
    std::unique_lock<std::mutex> lock(mutex_);
    // Ждем, пока не появится место в очереди
    not_full_.wait(lock, [this] { return static_cast<int>(queue_.size()) < capacity_; });
    queue_.push(std::move(task));
}

std::optional<std::function<void()>> BoundedQueue::try_pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return std::nullopt;
    }
    auto task = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    not_full_.notify_one();  // Сообщаем, что освободилось место
    return task;
}

BoundedQueue::~BoundedQueue() {}

}  // namespace dispatcher::queue