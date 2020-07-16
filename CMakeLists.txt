cmake_minimum_required(VERSION 3.5)

project(CoTracer_server LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

find_package(Boost REQUIRED)
if(Boost_FOUND)
    message("Boost found!")
    message("Boost version = ${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION}")
    include_directories(${Boost_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "Cannot find Boost")
endif()

find_package(Qt5 COMPONENTS Core Sql REQUIRED)

set(PROJECT_SOURCES
    main.cpp
    service.cpp
    acceptor.cpp
    server.cpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Sql)
