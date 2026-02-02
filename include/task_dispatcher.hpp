#pragma once

#include "queue/priority_queue.hpp"
#include "thread_pool/thread_pool.hpp"
#include "types.hpp"
#include <functional>
#include <memory>
#include <unordered_map>

namespace dispatcher {

class TaskDispatcher {
private:
    std::shared_ptr<queue::PriorityQueue> priority_queue_;  // Общая очередь задач для пула потоков
    std::unique_ptr<thread_pool::ThreadPool> thread_pool_;  // Управляет пулом потоков

public:
    // Конструктор: принимает количество потоков и опционально конфигурацию очереди
    // Если конфигурация не указана, используется конфигурация по умолчанию
    explicit TaskDispatcher(size_t thread_count,
                            const std::unordered_map<TaskPriority, queue::QueueOptions> &priority_queue_config = {});

    void schedule(TaskPriority priority, std::function<void()> task);

    ~TaskDispatcher();
};

}  // namespace dispatcher