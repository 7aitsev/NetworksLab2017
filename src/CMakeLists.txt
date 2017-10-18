cmake_minimum_required(VERSION 2.8)
project(tcp_service)

set(CMAKE_C_FLAGS "-Wall -Wextra -pthread -D_GNU_SOURCE")

set(DEPS_H ./lib/efunc.h ./lib/termproto.h)
set(DEPS_S ./lib/efunc.c ./lib/termproto.c)

set(_MODULES "./logger ./server/handler/peer ./server/handler ./server/service ./server/terminal ./server ")
#message("${_MODULES}")
string(REGEX REPLACE "(([a-z]+) )" "\\2/\\2.\# " MODULES ${_MODULES})
#message("${MODULES}")
string(REPLACE "\# " "c;" SOURCES ${MODULES})
#message("${SOURCES}")
string(REPLACE "\# " "h;" HEADERS ${MODULES})
#message("${HEADERS}")

include_directories(.)

list(APPEND SOURCES ${DEPS_S})
list(APPEND HEADERS ${DEPS_H})

add_executable(server main.c ${SOURCES} ${HEADERS})

target_link_libraries(server pthread)
