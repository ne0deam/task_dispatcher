#pragma once

#include <memory>

#include "queue/priority_queue.hpp"
#include "thread_pool/thread_pool.hpp"
#include "types.hpp"

namespace dispatcher {

class TaskDispatcher {
    // здесь ваш код
public:
    // TaskDispatcher(size_t thread_count, ?);

    void schedule(TaskPriority priority, std::function<void()> task);
    ~TaskDispatcher();
};

}  // namespace dispatcher