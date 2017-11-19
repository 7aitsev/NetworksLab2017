cmake_minimum_required(VERSION 2.8.12)
project(udp_service)

include_directories(.)

set(CMAKE_C_FLAGS "-Wall -Wextra -g -O0 -DWINVER=0x0600")

set(_MODULES "./logger ./server ./server/terminal ")
#message("${_MODULES}")
string(REGEX REPLACE "(([a-z]+) )" "\\2/\\2.\# " MODULES ${_MODULES})
#message("${MODULES}")
string(REPLACE "\# " "c;" SOURCES ${MODULES})
#message("${SOURCES}")
string(REPLACE "\# " "h;" HEADERS ${MODULES})
#message("${HEADERS}")

set(SERVER_TARGET server)
add_executable(${SERVER_TARGET} server/main.c ${SOURCES} ${HEADERS})
target_link_libraries(${SERVER_TARGET} ws2_32)