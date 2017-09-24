cmake_minimum_required(VERSION 2.8)
project(triv_mtp_tcp_serv)

set(CMAKE_C_FLAGS "-Wall -Wextra -pthread -D_GNU_SOURCE")

set(_MODULES "./logger ./server/handler/peer ./server/handler ./server/terminal ./server ")
#message("${_MODULES}")
string(REGEX REPLACE "(([a-z]+) )" "\\2/\\2.\# " MODULES ${_MODULES})
#message("${MODULES}")
string(REPLACE "\# " "c;" SOURCES ${MODULES})
#message("${SOURCES}")
string(REPLACE "\# " "h;" HEADERS ${MODULES})
#message("${HEADERS}")

include_directories(.)

add_executable(triv_mtp_tcp_serv main.c ${SOURCES} ${HEADERS})

target_link_libraries(triv_mtp_tcp_serv pthread)
