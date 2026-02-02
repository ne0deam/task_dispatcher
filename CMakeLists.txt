cmake_minimum_required(VERSION 3.25)
project(priority_task_dispatcher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


find_package(GTest REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# set(target dispatcher)

# add_executable(${target} main.cpp)
# target_include_directories(${target} PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
# )

# target_link_libraries(${target} 
#     PRIVATE
#         task_dispatcher
# )

include(CTest)
enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
