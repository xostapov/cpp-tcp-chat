cmake_minimum_required(VERSION 3.10)
project(chat_app)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(PLATFORM_LIBS ws2_32)
else()
    set(PLATFORM_LIBS pthread)
endif()

add_executable(server src/server.cpp)
add_executable(client src/client.cpp)

if(WIN32)
    target_link_libraries(server ${PLATFORM_LIBS})
    target_link_libraries(client ${PLATFORM_LIBS})
else()
    target_link_libraries(server ${PLATFORM_LIBS})
    target_link_libraries(client ${PLATFORM_LIBS})
endif() 