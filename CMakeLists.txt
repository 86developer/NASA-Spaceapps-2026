cmake_minimum_required(VERSION 3.15)
project(VitalGrid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags
if(MSVC)
    add_compile_options(/W4 /O2 /permissive-)
else()
    add_compile_options(-Wall -Wextra -O3 -pthread)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/engine/flight_surgeon.cpp
    src/soma/soma_watchdog.cpp
    src/network/websocket_server.cpp
)

# Header files for IDE navigation
set(HEADERS
    src/models/ridge_payload.hpp
    src/models/alert_payload.hpp
    src/engine/flight_surgeon.hpp
    src/soma/circular_buffer.hpp
    src/soma/soma_watchdog.hpp
    src/network/websocket_server.hpp
)

add_executable(vitalgrid_daemon ${SOURCES} ${HEADERS})

target_include_directories(vitalgrid_daemon PRIVATE src)

# Platform socket linking
if(WIN32)
    target_link_libraries(vitalgrid_daemon PRIVATE ws2_32)
else()
    target_link_libraries(vitalgrid_daemon PRIVATE pthread)
endif()
