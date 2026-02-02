#include "thread_pool/thread_pool.hpp"
#include <utility>

namespace dispatcher::thread_pool {

ThreadPool::ThreadPool(std::shared_ptr<queue::PriorityQueue> queue, size_t num_threads)
    : priority_queue_(std::move(queue)), num_threads_(num_threads) {

    if (!priority_queue_) {
        throw std::invalid_argument("PriorityQueue cannot be null");
    }

    if (num_threads_ == 0) {
        throw std::invalid_argument("Number of threads must be positive");
    }

    worker_threads_.reserve(num_threads_);
    for (size_t i = 0; i < num_threads_; ++i) {
        worker_threads_.emplace_back(&ThreadPool::WorkerLoop, this);
    }
}

// Метод запускает каждый рабочий поток
void ThreadPool::WorkerLoop() {
    while (true) {
        // Извлекаем задачу из общей очереди
        auto task_opt = priority_queue_->pop();

        // Если pop() вернул nullopt, это означает, что shutdown был вызван
        // Поток должен завершить выполнение цикла и, следовательно, сам поток.
        if (!task_opt.has_value()) {
            break;
        }

        // Если задача получена, выполняем её
        task_opt.value()();
    }
}

ThreadPool::~ThreadPool() {
    // Вызываем shutdown у очереди задач.
    if (priority_queue_) {
        priority_queue_->shutdown();
    }

    // Ждём завершения всех рабочих потоков
    for (auto &thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

}  // namespace dispatcher::thread_pool