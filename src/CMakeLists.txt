cmake_minimum_required(VERSION 2.8)
project(inet_apps)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread")

set(MODULES "./logger ./config ./server/handler/peer ./server/handler ./server/terminal ./server ")
#message("${MODULES}")
string(REGEX REPLACE "(([a-z]+) )" "\\2/\\2.c;" SOURCES ${MODULES})
#message("${SOURCES}")
string(REPLACE ".c" ".h;" HEADERS ${SOURCES})
#message("${HEADERS}")

include_directories(.)

add_executable(server main.c ${SOURCES} ${HEADERS})

#add_executable(client client/main.c ${CLIENT_SOURCES} ${CLIENT_HEADERS} ${DEPS})

#target_compile_definitions(triv_udp_serv PRIVATE -D_UDP_SERVER)
