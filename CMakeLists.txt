cmake_minimum_required(VERSION 3.5)

project(mhc_server_2 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Boost_INCLUDE_DIR /home/dima/Boost/boost_1_72_0/Boost_Library_Binary/include)
set(Boost_LIBRARY_DIR /home/dima/Boost/boost_1_72_0/Boost_Library_Binary/lib)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

find_package(Qt5Core)
find_package(Qt5Sql)

include_directories(${Boost_INCLUDE_DIR})

link_directories(${Boost_LIBRARY_DIR})

add_executable(mhc_server_2
  main.cpp
  service.cpp
  acceptor.cpp
  server.cpp
)

target_link_libraries(mhc_server_2 Qt5::Core Qt5::Sql)
#target_link_libraries(mhc_server_2 ${Boost_LIBRARIES})
