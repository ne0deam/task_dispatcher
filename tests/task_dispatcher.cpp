#include "task_dispatcher.hpp"
#include "queue/queue.hpp"
#include "types.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <gtest/gtest.h>
#include <stdexcept>
#include <thread>
#include <vector>

namespace dispatcher {

using PriorityMap = std::unordered_map<TaskPriority, queue::QueueOptions>;

// Вспомогательная функция для ожидания условия
template <typename Predicate>
void WaitFor(Predicate pred, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
    auto start = std::chrono::steady_clock::now();
    while (!pred()) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            FAIL() << "Timeout waiting for condition.";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Конструктор с разным количеством потоков и конфигурацией по умолчанию
TEST(TaskDispatcherTest, ConstructorWithDefaults) {
    EXPECT_NO_THROW({
        TaskDispatcher td(2);  // 2 потока, конфигурация по умолчанию
    });

    EXPECT_NO_THROW({
        TaskDispatcher td(4);  // 4 потока, конфигурация по умолчанию
    });
}

// Конструктор с кастомной конфигурацией
TEST(TaskDispatcherTest, ConstructorWithCustomConfig) {
    EXPECT_NO_THROW(([] {
        PriorityMap custom_config = {
            {TaskPriority::High, {true, 500}},  // Bounded 500
            {TaskPriority::Normal, {false}}     // Unbounded
        };
        TaskDispatcher td(2, custom_config);
    })());
}

// Исключение при нулевом количестве потоков (делегируется в ThreadPool)
TEST(TaskDispatcherTest, ConstructorZeroThreadsThrows) {
    EXPECT_THROW({ TaskDispatcher td(0); }, std::invalid_argument);
}

// Задачи выполняются
TEST(TaskDispatcherTest, TasksAreExecuted) {
    TaskDispatcher td(2);

    std::atomic<bool> task_executed{false};

    td.schedule(TaskPriority::Normal, [&task_executed]() { task_executed = true; });

    // Ждём выполнения задачи
    WaitFor([&task_executed] { return task_executed.load(); });
    EXPECT_TRUE(task_executed.load());
}

// Проверка выполнения задач с разными приоритетами
TEST(TaskDispatcherTest, PriorityTest) {
    TaskDispatcher td(2);

    std::vector<int> execution_order;
    std::mutex order_mutex;
    std::atomic<int> tasks_completed{0};

    // Добавляем задачи вперемешку
    td.schedule(TaskPriority::Normal, [&]() {
        {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(1);  // Normal
        }
        tasks_completed.fetch_add(1);  // Увеличиваем счётчик ПОСЛЕ выполнения задачи
    });

    td.schedule(TaskPriority::High, [&]() {
        {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(2);  // High
        }
        tasks_completed.fetch_add(1);
    });

    td.schedule(TaskPriority::Normal, [&]() {
        {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(3);  // Normal
        }
        tasks_completed.fetch_add(1);
    });

    td.schedule(TaskPriority::High, [&]() {
        {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(4);  // High
        }
        tasks_completed.fetch_add(1);
    });

    // Ждем выполнения всех 4 задач
    WaitFor([&tasks_completed] { return tasks_completed.load() == 4; });

    // Проверяем, что все задачи выполнены
    EXPECT_EQ(execution_order.size(), 4);

    // Проверяем наличие всех типов задач
    bool has_high = false, has_normal = false;
    for (int id : execution_order) {
        if (id == 2 || id == 4)
            has_high = true;
        if (id == 1 || id == 3)
            has_normal = true;
    }
    EXPECT_TRUE(has_high);
    EXPECT_TRUE(has_normal);
}

// Многопоточное планирование
TEST(TaskDispatcherTest, ConcurrentScheduling) {
    const int NUM_THREADS = 4;
    const int TASKS_PER_THREAD = 50;
    TaskDispatcher td(NUM_THREADS);

    std::atomic<int> counter{0};

    std::vector<std::thread> schedulers;
    for (int t = 0; t < NUM_THREADS; ++t) {
        schedulers.emplace_back([&td, TASKS_PER_THREAD, &counter, t]() {
            for (int i = 0; i < TASKS_PER_THREAD; ++i) {
                td.schedule((i % 2 == 0) ? TaskPriority::High : TaskPriority::Normal,
                            [&counter]() { counter.fetch_add(1); });
            }
        });
    }

    for (auto &th : schedulers) {
        th.join();
    }

    // Ждём, пока все запланированные задачи выполнятся
    const int TOTAL_TASKS = NUM_THREADS * TASKS_PER_THREAD;
    WaitFor([&counter, TOTAL_TASKS] { return counter.load() == TOTAL_TASKS; });

    EXPECT_EQ(counter.load(), TOTAL_TASKS);
}

// Проверим, что деструктор не падает и дожидается выполнения задач
TEST(TaskDispatcherTest, DestructorWaitsForTasks) {
    std::atomic<int> counter{0};

    {
        TaskDispatcher td(2);

        // Добавляем задачи
        for (int i = 0; i < 10; ++i) {
            td.schedule(TaskPriority::Normal, [&counter]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Имитация работы
                counter.fetch_add(1);
            });
        }
        // Сразу выходим из области видимости, вызывая деструктор td
    }  // Деструктор TaskDispatcher должен дождаться завершения потоков

    // К этому моменту все задачи должны быть выполнены
    EXPECT_EQ(counter.load(), 10);
}

// Добавление задачи после запуска деструктора
// Проверим, что задача, добавленная перед деструктором, выполнится
TEST(TaskDispatcherTest, TaskAddedBeforeDestructionIsExecuted) {
    std::atomic<bool> task_executed{false};

    {
        TaskDispatcher td(1);
        td.schedule(TaskPriority::Normal, [&task_executed]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Немного работы
            task_executed = true;
        });
        // task_executed ещё false
    }  // Вызывается деструктор

    // После выхода из области видимости task_executed должен стать true
    EXPECT_TRUE(task_executed.load());
}

}  // namespace dispatcher