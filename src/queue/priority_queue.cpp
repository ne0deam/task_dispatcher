#include "queue/priority_queue.hpp"
#include "queue/bounded_queue.hpp"
#include "queue/unbounded_queue.hpp"
#include <stdexcept>
#include <utility>

namespace dispatcher::queue {

std::unique_ptr<IQueue> PriorityQueue::CreateQueue(const QueueOptions &options) {
    if (options.bounded) {
        if (!options.capacity.has_value() || options.capacity.value() <= 0) {
            throw std::invalid_argument("Bounded queue requires a positive capacity.");
        }
        return std::make_unique<BoundedQueue>(options.capacity.value());
    } else {
        return std::make_unique<UnboundedQueue>();
    }
}

PriorityQueue::PriorityQueue(const std::unordered_map<TaskPriority, QueueOptions> &priority_to_options) {
    for (const auto &[priority, options] : priority_to_options) {
        queues_by_priority_[priority] = CreateQueue(options);
    }
}

void PriorityQueue::push(TaskPriority priority, std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = queues_by_priority_.find(priority);
    if (it == queues_by_priority_.end()) {
        throw std::invalid_argument("Queue for given priority does not exist.");
    }

    // Пробуем добавить задачу. Обновляем счётчик только если push успешен.
    it->second->push(std::move(task));
    num_total_tasks_.fetch_add(1, std::memory_order_release);

    cv_tasks_available_.notify_one();
}

bool PriorityQueue::HasTasksAvailable() const { return num_total_tasks_.load(std::memory_order_acquire) > 0; }

std::optional<std::function<void()>> PriorityQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);

    while (true) {
        cv_tasks_available_.wait(lock, [this] { return is_shutdown_ || HasTasksAvailable(); });

        if (is_shutdown_) {
            for (auto &[priority, queue] : queues_by_priority_) {
                auto task_opt = queue->try_pop();
                if (task_opt.has_value()) {
                    num_total_tasks_.fetch_sub(1, std::memory_order_release);
                    return task_opt;
                }
            }
            return std::nullopt;
        }

        for (auto &[priority, queue] : queues_by_priority_) {
            auto task_opt = queue->try_pop();
            if (task_opt.has_value()) {
                num_total_tasks_.fetch_sub(1, std::memory_order_release);
                return task_opt;
            }
        }
    }
}

void PriorityQueue::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        is_shutdown_ = true;
    }
    cv_tasks_available_.notify_all();
}

PriorityQueue::~PriorityQueue() { shutdown(); }

}  // namespace dispatcher::queue