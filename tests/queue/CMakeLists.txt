set(target queue_test)

add_executable(${target}
    bounded_queue.cpp
    unbounded_queue.cpp
    priority_queue.cpp
)

target_link_libraries(${target}
    PRIVATE
        GTest::GTest
        GTest::Main
        queue
)

add_test(NAME ${target} COMMAND ${target})