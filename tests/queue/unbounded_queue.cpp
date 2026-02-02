#include <atomic>
#include <functional>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "queue/unbounded_queue.hpp"

namespace dispatcher::queue {

TEST(UnboundedQueueTest, ConstructorNoCapacity) {
    EXPECT_NO_THROW({ UnboundedQueue queue; });
}

TEST(UnboundedQueueTest, TryPopEmptyReturnsNullopt) {
    UnboundedQueue queue;
    auto result = queue.try_pop();
    EXPECT_FALSE(result.has_value());
}

TEST(UnboundedQueueTest, SinglePushTryPop) {
    UnboundedQueue queue;
    bool executed = false;
    std::function<void()> task = [&executed]() { executed = true; };

    queue.push(task);
    auto popped_task_opt = queue.try_pop();

    ASSERT_TRUE(popped_task_opt.has_value());
    auto &popped_task = popped_task_opt.value();
    popped_task();

    EXPECT_TRUE(executed);
}

TEST(UnboundedQueueTest, FifoOrder) {
    UnboundedQueue queue;
    std::vector<int> execution_order;

    for (int i = 0; i < 3; ++i) {
        queue.push([&execution_order, i]() { execution_order.push_back(i); });
    }

    // Извлекаем и выполняем 3 задачи
    for (int expected_id = 0; expected_id < 3; ++expected_id) {
        auto task_opt = queue.try_pop();
        ASSERT_TRUE(task_opt.has_value());
        task_opt.value()();
    }

    // Проверяем порядок выполнения
    std::vector<int> expected_order = {0, 1, 2};
    EXPECT_EQ(execution_order, expected_order);
}

// Поведение при многопоточном доступе (Producer-Consumer)
TEST(UnboundedQueueTest, MultithreadedProducerConsumer) {
    const int NUM_TASKS = 100;
    UnboundedQueue queue;

    std::atomic<int> counter(0);
    std::vector<int> results(NUM_TASKS, -1);  // Вектор для хранения результатов

    // Поток-продюсер
    std::thread producer([&queue, NUM_TASKS, &results]() {
        for (int i = 0; i < NUM_TASKS; ++i) {
            // Создаем задачу, которая запишет свой ID в вектор
            // Теперь захватываем &results по ссылке во внутренней лямбде
            queue.push([i, &results]() { results[i] = i; });
        }
    });

    // Поток-потребитель
    std::thread consumer([&queue, NUM_TASKS, &counter]() {
        int received = 0;
        while (received < NUM_TASKS) {
            auto task_opt = queue.try_pop();
            if (task_opt.has_value()) {
                task_opt.value()();  // Выполняем задачу
                received++;
                counter.fetch_add(1);
            } else {
                // Если очередь пуста, немного подождем перед следующей попыткой
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(counter.load(), NUM_TASKS);

    // Проверяем, что все задачи были выполнены
    for (int i = 0; i < NUM_TASKS; ++i) {
        EXPECT_EQ(results[i], i);
    }
}

TEST(UnboundedQueueTest, PushDoesNotBlock) {
    UnboundedQueue queue;
    const int NUM_TASKS = 10000;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Добавляем много задач быстро
    for (int i = 0; i < NUM_TASKS; ++i) {
        queue.push([]() {});
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Проверяем, что очередь не пуста после добавления
    EXPECT_TRUE(queue.try_pop().has_value());  // Извлекаем хотя бы одну, чтобы проверить, что добавление прошло

    bool queue_not_empty_after_pushes = false;
    for (int i = 0; i < NUM_TASKS && !queue_not_empty_after_pushes; ++i) {
        queue_not_empty_after_pushes |= queue.try_pop().has_value();
    }
    EXPECT_TRUE(queue_not_empty_after_pushes);
}

}  // namespace dispatcher::queue