cmake_minimum_required(VERSION 3.10)

project(bottle-recognizer)
set(CMAKE_OSX_DEPLOYMENT_TARGET 15.7)

if (APPLE)
    set(CMAKE_PREFIX_PATH /opt/homebrew)
elseif (WIN32)
    set(CMAKE_PREFIX_PATH "C:/lib/install/opencv")
endif()
find_package(OpenCV REQUIRED)
add_executable(bottle-recognizer src/main.cpp)
target_include_directories(bottle-recognizer PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(bottle-recognizer PRIVATE ${OpenCV_LIBS})