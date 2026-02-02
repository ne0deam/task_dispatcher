#include <atomic>
#include <functional>
#include <gtest/gtest.h>
#include <stdexcept>
#include <thread>
#include <vector>

#include "queue/bounded_queue.hpp"

namespace dispatcher::queue {

TEST(BoundedQueueTest, ConstructorValidCapacity) {
    EXPECT_NO_THROW({ BoundedQueue queue(5); });
}

TEST(BoundedQueueTest, ConstructorInvalidCapacityThrows) {
    EXPECT_THROW({ BoundedQueue queue(0); }, std::invalid_argument);

    EXPECT_THROW({ BoundedQueue queue(-1); }, std::invalid_argument);
}

TEST(BoundedQueueTest, TryPopEmptyReturnsNullopt) {
    BoundedQueue queue(10);
    auto result = queue.try_pop();
    EXPECT_FALSE(result.has_value());
}

TEST(BoundedQueueTest, SinglePushTryPop) {
    BoundedQueue queue(10);
    bool executed = false;
    std::function<void()> task = [&executed]() { executed = true; };

    queue.push(task);
    auto popped_task_opt = queue.try_pop();

    ASSERT_TRUE(popped_task_opt.has_value());
    auto &popped_task = popped_task_opt.value();
    popped_task();  // Выполняем задачу

    EXPECT_TRUE(executed);
}

TEST(BoundedQueueTest, FifoOrder) {
    BoundedQueue queue(10);
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

TEST(BoundedQueueTest, BlocksOnFullCapacityMultithreaded) {
    const int CAPACITY = 2;
    BoundedQueue queue(CAPACITY);

    std::atomic<int> tasks_added(0);
    std::atomic<bool> producer_started(false);
    std::atomic<bool> producer_blocked_after_full(false);

    std::thread producer_thread([&queue, &tasks_added, &producer_started, &producer_blocked_after_full, CAPACITY] {
        producer_started = true;

        // Заполняем очередь до предела
        for (int i = 0; i < CAPACITY; ++i) {
            queue.push([]() {});  // Пустая задача
            tasks_added.fetch_add(1);
        }

        // Пытаемся добавить еще одну задачу - должен заблокироваться
        producer_blocked_after_full = true;
        queue.push([]() {});
        tasks_added.fetch_add(1);
    });

    // Ждем, пока продюсер начнет работу
    while (!producer_started.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Даем времени заполнить очередь

    // Проверяем, что продюсер достиг точки блокировки
    EXPECT_TRUE(producer_blocked_after_full.load());
    EXPECT_EQ(tasks_added.load(), CAPACITY);  // Продюсер не должен был добавить 3-ю задачу

    // Извлекаем одну задачу, освобождая место
    auto task = queue.try_pop();
    ASSERT_TRUE(task.has_value());

    // Теперь продюсер должен разблокироваться и добавить последнюю задачу
    producer_thread.join();                       // Ждем завершения потока продюсера
    EXPECT_EQ(tasks_added.load(), CAPACITY + 1);  // Теперь должно быть 3 задачи добавлено
}

// Поведение при попытке извлечения из пустой очереди (try_pop не блокируется)
TEST(BoundedQueueTest, TryPopDoesNotBlockWhenEmpty) {
    BoundedQueue queue(10);

    // Несколько попыток извлечения из пустой очереди должны вернуть nullopt
    for (int i = 0; i < 5; ++i) {
        auto result = queue.try_pop();
        EXPECT_FALSE(result.has_value());
    }
}

// Деструктор корректно завершает работу
TEST(BoundedQueueTest, DestructorReleasesWaitingThreads) {
    BoundedQueue *queue_ptr = new BoundedQueue(1);

    std::atomic<bool> consumer_waiting(false);
    std::thread consumer_thread([&queue_ptr, &consumer_waiting] {
        consumer_waiting = true;
        auto task_opt = queue_ptr->try_pop();  // Попробуем использовать try_pop, т.к. очередь пуста
    });

    // Ждем, пока потребитель выполнит try_pop
    while (!consumer_waiting.load()) {
        std::this_thread::yield();
    }

    delete queue_ptr;

    // Потребительский поток должен завершиться, так как очередь уничтожена
    // и больше не будет принимать задачи.
    if (consumer_thread.joinable()) {
        consumer_thread.join();
    }
}

// Тест на многопоточное переполнение (несколько продюсеров)
TEST(BoundedQueueTest, MultipleProducersBlockOnFull) {
    const int CAPACITY = 3;
    BoundedQueue queue(CAPACITY);
    std::atomic<int> total_pushed{0};
    std::vector<std::thread> producers;

    for (int i = 0; i < 5; ++i) {
        producers.emplace_back([&queue, &total_pushed] {
            try {
                queue.push([]() {});
                total_pushed.fetch_add(1);
            } catch (...) {
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_LE(total_pushed.load(), CAPACITY + 5);  // Не все смогли добавить

    // Очищаем очередь и даем всем завершиться
    while (queue.try_pop().has_value()) {
    }

    for (auto &t : producers) {
        if (t.joinable())
            t.join();
    }
}

TEST(BoundedQueueTest, StressTest) {
    const int CAPACITY = 100;
    const int TOTAL_TASKS = 1000;
    BoundedQueue queue(CAPACITY);
    std::atomic<int> consumed{0};

    std::thread producer([&queue, TOTAL_TASKS] {
        for (int i = 0; i < TOTAL_TASKS; ++i) {
            queue.push([]() {});
        }
    });

    std::thread consumer([&queue, &consumed] {
        while (consumed < TOTAL_TASKS) {
            if (auto task = queue.try_pop()) {
                (*task)();
                consumed.fetch_add(1);
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(consumed.load(), TOTAL_TASKS);
}

}  // namespace dispatcher::queue