#include "task_dispatcher.hpp"
#include <utility>

namespace dispatcher {

// Создание конфигурации по умолчанию
std::unordered_map<TaskPriority, queue::QueueOptions> GetDefaultQueueConfig() {
    return {
        {TaskPriority::High, {true, 1000}},  // Ограниченная очередь на 1000 для High
        {TaskPriority::Normal, {false}}      // Неограниченная очередь для Normal
    };
}

// Конструктор
TaskDispatcher::TaskDispatcher(size_t thread_count,
                               const std::unordered_map<TaskPriority, queue::QueueOptions> &priority_queue_config)
    : priority_queue_(nullptr), thread_pool_(nullptr) {

    // Создаём очередь с нужной конфигурацией
    auto queue = std::make_shared<queue::PriorityQueue>(priority_queue_config.empty() ? GetDefaultQueueConfig()
                                                                                      : priority_queue_config);

    auto pool = std::make_unique<thread_pool::ThreadPool>(queue, thread_count);
    priority_queue_ = std::move(queue);
    thread_pool_ = std::move(pool);
}

void TaskDispatcher::schedule(TaskPriority priority, std::function<void()> task) {
    priority_queue_->push(priority, std::move(task));
}

TaskDispatcher::~TaskDispatcher() {}

}  // namespace dispatcher