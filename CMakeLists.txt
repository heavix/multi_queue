cmake_minimum_required (VERSION 2.8.2)
project (MultiQueueTest)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
find_package(Threads)

add_executable ( ${PROJECT_NAME} CPQueue.h MultiQueueProcessor.h MultiQueueTest.cpp )

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})