project(threadPool)
cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 20)

add_executable(threadPool
	main.cpp
)

# Testing
include(CTest)
find_package(GTest REQUIRED)

add_executable(tests
	tests.cpp
)

target_link_libraries(tests
    PRIVATE gtest
    PRIVATE pthread
)

add_executable(parallelaccumulate
	parallelaccumulate.cpp
)

target_link_libraries(parallelaccumulate
	PRIVATE gtest
	PRIVATE pthread
)

gtest_discover_tests(tests)
gtest_discover_tests(parallelaccumulate)
