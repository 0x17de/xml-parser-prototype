cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 17)

find_package(PugiXML REQUIRED)

add_executable(main ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
target_link_libraries(main pugixml)
