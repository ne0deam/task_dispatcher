#pragma once

#include "queue/queue.hpp"
#include "types.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace dispatcher::queue {

class PriorityQueue {
private:
    std::map<TaskPriority, std::unique_ptr<IQueue>> queues_by_priority_;
    mutable std::mutex mutex_;
    std::condition_variable cv_tasks_available_;
    bool is_shutdown_ = false;
    std::atomic<size_t> num_total_tasks_{0};  // Атомарный счётчик общего количества задач

    static std::unique_ptr<IQueue> CreateQueue(const QueueOptions &options);
    bool HasTasksAvailable() const;

public:
    explicit PriorityQueue(const std::unordered_map<TaskPriority, QueueOptions> &priority_to_options);

    void push(TaskPriority priority, std::function<void()> task);

    std::optional<std::function<void()>> pop();

    void shutdown();

    ~PriorityQueue();
};

}  // namespace dispatcher::queue