#include "queue/priority_queue.hpp"
#include "types.hpp"
#include <atomic>
#include <functional>
#include <gtest/gtest.h>
#include <stdexcept>
#include <thread>

namespace dispatcher::queue {

using PriorityMap = std::unordered_map<TaskPriority, QueueOptions>;

// Вспомогательная функция для ожидания условия
template <typename Predicate>
void WaitFor(Predicate pred, std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
    auto start = std::chrono::steady_clock::now();
    while (!pred()) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            FAIL() << "Timeout waiting for condition.";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Конструктор с разными опциями очередей
TEST(PriorityQueueTest, ConstructorWithConfig) {
    EXPECT_NO_THROW(([] {
        PriorityMap config = {
            {TaskPriority::High, {false}},      // Unbounded
            {TaskPriority::Normal, {true, 10}}  // Bounded
        };
        PriorityQueue pq(config);
    })());

    EXPECT_NO_THROW(([] {
        PriorityMap config = {
            {TaskPriority::High, {true, 5}},  // Bounded
            {TaskPriority::Normal, {false}}   // Unbounded
        };
        PriorityQueue pq(config);
    })());
}

// Исключение при отсутствии очереди для приоритета в push
TEST(PriorityQueueTest, PushNonExistentPriorityThrows) {
    PriorityMap config = {{TaskPriority::Normal, {false}}};
    PriorityQueue pq(config);

    EXPECT_THROW({ pq.push(TaskPriority::High, []() {}); }, std::invalid_argument);
}

// Приоритеты - высокий приоритет извлекается первым
TEST(PriorityQueueTest, HighPriorityPopsFirst) {
    PriorityMap config = {{TaskPriority::High, {false}}, {TaskPriority::Normal, {false}}};
    PriorityQueue pq(config);

    bool normal_executed = false;
    bool high_executed = false;

    // Добавляем задачу нормального приоритета
    pq.push(TaskPriority::Normal, [&normal_executed]() { normal_executed = true; });
    // Ждём немного, чтобы убедиться, что она в очереди
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Добавляем задачу высокого приоритета
    pq.push(TaskPriority::High, [&high_executed]() { high_executed = true; });

    // Извлекаем и выполняем первую задачу
    auto task1 = pq.pop();
    ASSERT_TRUE(task1.has_value());
    task1.value()();                // Выполняем задачу
    EXPECT_TRUE(high_executed);     // Должна выполниться задача с высоким приоритетом
    EXPECT_FALSE(normal_executed);  // Задача с нормальным не должна

    // Извлекаем и выполняем вторую задачу
    auto task2 = pq.pop();
    ASSERT_TRUE(task2.has_value());
    task2.value()();
    EXPECT_TRUE(normal_executed);  // Теперь должна выполниться задача с нормальным приоритетом
}

// pop блокируется при пустой очереди
TEST(PriorityQueueTest, PopBlocksWhenEmpty) {
    PriorityMap config = {{TaskPriority::High, {false}}, {TaskPriority::Normal, {false}}};
    PriorityQueue pq(config);

    std::atomic<bool> pop_started{false};
    std::atomic<bool> pop_finished{false};

    std::thread pop_thread([&pq, &pop_started, &pop_finished]() {
        pop_started = true;
        auto task = pq.pop();  // Должен заблокироваться
        // После shutdown task будет nullopt
        pop_finished = true;
    });

    WaitFor([&pop_started] { return pop_started.load(); });
    EXPECT_FALSE(pop_finished.load());  // pop_thread должен быть заблокирован

    // Вызываем shutdown, чтобы разблокировать pop
    pq.shutdown();

    pop_thread.join();  // Ждём завершения потока
    EXPECT_TRUE(pop_finished.load());
}

// shutdown останавливает блокировку pop
TEST(PriorityQueueTest, ShutdownStopsPopBlocking) {
    PriorityMap config = {{TaskPriority::High, {false}}, {TaskPriority::Normal, {false}}};
    PriorityQueue pq(config);

    // Запускаем pop в отдельном потоке
    std::thread pop_thread([&pq]() {
        auto task = pq.pop();            // Должен вернуть nullopt после shutdown
        EXPECT_FALSE(task.has_value());  // Ожидаем nullopt
    });

    // Ждём немного, чтобы pop точно вошёл в ожидание
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Вызываем shutdown
    pq.shutdown();
    pop_thread.join();
}

// shutdown позволяет извлекать оставшиеся задачи
TEST(PriorityQueueTest, ShutdownAllowsRemainingTasks) {
    PriorityMap config = {{TaskPriority::High, {false}}, {TaskPriority::Normal, {false}}};
    PriorityQueue pq(config);

    std::atomic<int> counter(0);

    // Добавляем несколько задач
    pq.push(TaskPriority::Normal, [&counter]() { counter.fetch_add(1); });
    pq.push(TaskPriority::High, [&counter]() { counter.fetch_add(10); });
    pq.push(TaskPriority::Normal, [&counter]() { counter.fetch_add(1); });

    // Вызываем shutdown
    pq.shutdown();

    // Извлекаем и выполняем 3 задачи
    for (int i = 0; i < 3; ++i) {
        auto task = pq.pop();
        ASSERT_TRUE(task.has_value()) << "Expected a task, got nullopt on iteration " << i;
        task.value()();
    }

    // Извлекаем 4-ю задачу - её не должно быть
    auto no_task = pq.pop();
    EXPECT_FALSE(no_task.has_value());

    EXPECT_EQ(counter.load(), 12);  // 10 + 1 + 1
}

// Многопоточность - producer/consumer
TEST(PriorityQueueTest, MultithreadedProducerConsumer) {
    PriorityMap config = {
        {TaskPriority::High, {true, 100}},  // Bounded, чтобы лучше видеть параллельность
        {TaskPriority::Normal, {false}}     // Unbounded
    };
    PriorityQueue pq(config);

    const int NUM_HIGH_TASKS = 50;
    const int NUM_NORMAL_TASKS = 50;
    const int TOTAL_TASKS = NUM_HIGH_TASKS + NUM_NORMAL_TASKS;

    std::atomic<int> high_execution_counter(0);
    std::atomic<int> normal_execution_counter(0);

    // Поток-продюсер для высокого приоритета
    std::thread high_producer([&pq, NUM_HIGH_TASKS, &high_execution_counter]() {
        for (int i = 0; i < NUM_HIGH_TASKS; ++i) {
            pq.push(TaskPriority::High, [&high_execution_counter]() { high_execution_counter.fetch_add(1); });
        }
    });

    // Поток-продюсер для нормального приоритета
    std::thread normal_producer([&pq, NUM_NORMAL_TASKS, &normal_execution_counter]() {
        for (int i = 0; i < NUM_NORMAL_TASKS; ++i) {
            pq.push(TaskPriority::Normal, [&normal_execution_counter]() { normal_execution_counter.fetch_add(1); });
        }
    });

    // Поток-потребитель
    std::thread consumer([&pq, &high_execution_counter, &normal_execution_counter]() {
        int tasks_processed = 0;
        while (true) {
            auto task = pq.pop();  // Блокируется, пока не появится задача или не будет shutdown
            if (!task.has_value()) {
                break;
            }
            task.value()();     // Выполняем задачу (увеличивает атомарный счётчик)
            tasks_processed++;  // Увеличиваем счётчик обработанных задач
        }
    });

    // Ждём завершения producer-ов
    high_producer.join();
    normal_producer.join();

    // Теперь, когда все producer-ы завершены, вызываем shutdown
    pq.shutdown();

    // Ждём завершения consumer-а
    consumer.join();

    // Проверяем, что все задачи были выполнены
    EXPECT_EQ(high_execution_counter.load(), NUM_HIGH_TASKS);
    EXPECT_EQ(normal_execution_counter.load(), NUM_NORMAL_TASKS);
}

// Исключение при неверной конфигурации
TEST(PriorityQueueTest, ConstructorInvalidBoundedConfigThrows) {
    EXPECT_THROW(([] {
                     PriorityMap config = {
                         {TaskPriority::High, {true}}  // bounded = true, capacity = nullopt
                     };
                     PriorityQueue pq(config);
                 })(),
                 std::invalid_argument);

    EXPECT_THROW(([] {
                     PriorityMap config = {
                         {TaskPriority::High, {true, -1}}  // bounded = true, capacity <= 0
                     };
                     PriorityQueue pq(config);
                 })(),
                 std::invalid_argument);

    EXPECT_THROW(([] {
                     PriorityMap config = {
                         {TaskPriority::High, {true, 0}}  // bounded = true, capacity <= 0
                     };
                     PriorityQueue pq(config);
                 })(),
                 std::invalid_argument);
}

// Проверка корректности счётчика
TEST(PriorityQueueTest, CounterConsistencyNormalFlow) {
    PriorityMap config = {{TaskPriority::High, {false}}, {TaskPriority::Normal, {false}}};
    PriorityQueue pq(config);

    const int NUM_TASKS = 100;
    for (int i = 0; i < NUM_TASKS; ++i) {
        pq.push(TaskPriority::Normal, []() {});
    }

    for (int i = 0; i < NUM_TASKS; ++i) {
        auto task = pq.pop();
        ASSERT_TRUE(task.has_value());
        task.value()();
    }

    // После извлечения всех задач, попытка извлечения при shutdown должна вернуть nullopt
    pq.shutdown();
    auto task = pq.pop();
    EXPECT_FALSE(task.has_value());
}

TEST(PriorityQueueTest, MultipleConsumers) {
    PriorityMap config = {{TaskPriority::High, {false}}, {TaskPriority::Normal, {false}}};
    PriorityQueue pq(config);

    const int NUM_TASKS = 100;
    const int NUM_CONSUMERS = 4;
    std::atomic<int> total_consumed{0};

    // Поток-производитель
    std::thread producer([&pq, NUM_TASKS]() {
        for (int i = 0; i < NUM_TASKS; ++i) {
            // Добавляем задачи с разными приоритетами
            pq.push((i % 3 == 0) ? TaskPriority::High : TaskPriority::Normal, []() {});
        }
    });

    // Вектор потоков-потребителей
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back([&pq, &total_consumed]() {
            // Цикл потребителя: продолжаем, пока не получим nullopt после shutdown
            while (true) {
                auto task = pq.pop();  // Блокируется до задачи или shutdown
                if (!task.has_value()) {
                    // pop() вернул nullopt, это означает, что shutdown был вызван и очередь пуста
                    break;
                }
                // Задача получена, выполняем её
                (*task)();
                total_consumed.fetch_add(1);
            }
        });
    }

    // Ждём завершения производителя
    producer.join();

    // Вызываем shutdown, чтобы разбудить ожидающие pop() потоки и позволить им выйти из цикла
    pq.shutdown();

    // Ждём завершения всех потребителей
    for (auto &consumer : consumers) {
        consumer.join();
    }

    // Проверяем, что все задачи были извлечены и обработаны
    EXPECT_EQ(total_consumed.load(), NUM_TASKS);
}

}  // namespace dispatcher::queue