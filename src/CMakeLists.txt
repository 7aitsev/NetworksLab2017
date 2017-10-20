cmake_minimum_required(VERSION 2.8.12)
project(tcp_service)

set(CMAKE_C_FLAGS "-pthread -D_GNU_SOURCE")

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

set(SERVER_TARGET server)
add_executable(${SERVER_TARGET} main.c ${SOURCES} ${HEADERS})
target_link_libraries(${SERVER_TARGET} pthread)
target_compile_options(${SERVER_TARGET} PUBLIC -O3)

set(SERVER_DEBUG_TARGET server_debug)
add_executable(${SERVER_DEBUG_TARGET} EXCLUDE_FROM_ALL main.c ${SOURCES} ${HEADERS})
target_link_libraries(${SERVER_DEBUG_TARGET} pthread -fsanitize=thread)
target_compile_options(${SERVER_DEBUG_TARGET} PUBLIC -Wall -Wextra -fsanitize=thread -fPIE -pie -O0 -g)
