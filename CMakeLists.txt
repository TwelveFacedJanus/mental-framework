cmake_minimum_required(VERSION 3.11)

project(Mental LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

set(SOURCES
	mental/main.cpp
)
	

add_executable(${PROJECT_NAME} ${SOURCES})
