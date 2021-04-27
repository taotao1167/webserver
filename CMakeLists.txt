cmake_minimum_required(VERSION 2.9)
project(app)

option(WITH_IPV6 "Compile with IPv6 support." ON)
option(WITH_WEBSOCKET "Compile with WebSocket support." ON)
option(WITH_SSL "Compile with SSL support, need OpenSSL." ON)
option(WATCH_RAM "Malloc debug." OFF)

find_package(Threads REQUIRED)

if (WITH_IPV6)
	ADD_DEFINITIONS(-DWITH_IPV6)
endif()
if (WITH_WEBSOCKET)
	ADD_DEFINITIONS(-DWITH_WEBSOCKET)
endif()
if (WITH_SSL)
	ADD_DEFINITIONS(-DWITH_SSL)
	find_package(OpenSSL REQUIRED)
endif()
if (WATCH_RAM)
	ADD_DEFINITIONS(-DWATCH_RAM)
endif()

if (WIN32)
	# target_link_libraries(app ws2_32)
	include_directories("/mingw64/include")
	link_directories("/mingw64/lib")
else()
	add_compile_options(-rdynamic)
endif()

if (NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Release)
endif()
SET(SRCLIST tt_web.c tt_web_dispatch.c tt_file.c tt_buffer.c tt_malloc_debug.c tt_sha1.c tt_base64.c tt_session.c tt_rbtree.c tt_handler.c tt_msgqueue.c tt_semaphore.c)

SET(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib")
SET(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")

add_compile_options(-Wall -pthread -O3)

add_library(webserver ${SRCLIST})

link_directories(${LIBRARY_OUTPUT_PATH})
add_executable(app main.c)
target_link_libraries(app pthread webserver event)
if (WITH_SSL)
 	target_link_libraries(app crypto ssl event_openssl)
endif()

