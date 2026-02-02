#pragma once

#include "queue/priority_queue.hpp"
#include <memory>
#include <thread>
#include <vector>

namespace dispatcher::thread_pool {

class ThreadPool {
private:
    void WorkerLoop();

    std::shared_ptr<queue::PriorityQueue> priority_queue_;  // Общая очередь задач
    std::vector<std::thread> worker_threads_;               // Вектор рабочих потоков
    const size_t num_threads_;

public:
    ThreadPool(std::shared_ptr<queue::PriorityQueue> queue, size_t num_threads);

    ~ThreadPool();

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
};

}  // namespace dispatcher::thread_pool
