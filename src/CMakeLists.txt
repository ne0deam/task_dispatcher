add_subdirectory(queue)
add_subdirectory(thread_pool)

add_library(task_dispatcher
    task_dispatcher.cpp
)

target_link_libraries(task_dispatcher
    PUBLIC
        thread_pool
        queue
)