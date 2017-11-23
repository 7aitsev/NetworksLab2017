cmake_minimum_required(VERSION 2.8.12)
project(udp_service)

include_directories(.)

set(DEPS_H ./lib/termproto.h ./lib/werror.h)
set(DEPS_S ./lib/termproto.c ./lib/werror.c)

if(WIN32)
    set(CMAKE_C_FLAGS "-Wall -Wextra -g -O0 -DWINVER=0x0600")

    set(_MODULES "./logger ./server ./server/terminal ./server/handler ./server/handler/peer ./server/service ")
    #message("${_MODULES}")
    string(REGEX REPLACE "(([a-z]+) )" "\\2/\\2.\# " MODULES ${_MODULES})
    #message("${MODULES}")
    string(REPLACE "\# " "c;" SOURCES ${MODULES})
    #message("${SOURCES}")
    string(REPLACE "\# " "h;" HEADERS ${MODULES})
    #message("${HEADERS}")

    list(APPEND SOURCES ${DEPS_S})
    list(APPEND HEADERS ${DEPS_H})

    set(SERVER_TARGET server)
    add_executable(${SERVER_TARGET} server/main.c ${SOURCES} ${HEADERS})
    target_link_libraries(${SERVER_TARGET} ws2_32)
elseif(UNIX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O0 -g -fsanitize=address -fPIE -pie")

    set(CLIENT_TARGET client)
    add_executable(${CLIENT_TARGET} client/main.c ./lib/termproto.h ./lib/termproto.c)
    #    target_link_libraries(${CLIENT_TARGET} )
endif()
