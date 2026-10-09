cmake_minimum_required(VERSION 3.16)
project(simulation_zoo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Exhibits register themselves at load time, so every exhibit object must end up
# in the executable. OBJECT libraries guarantee that (a static library would let
# the linker drop unreferenced registration units).
file(GLOB EXHIBIT_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/exhibits/*/*.cpp")

add_library(zoo_core OBJECT core/registry.cpp)
target_include_directories(zoo_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(zoo_exhibits OBJECT ${EXHIBIT_SOURCES})
target_link_libraries(zoo_exhibits PUBLIC zoo_core)

add_executable(zoo
    apps/zoo.cpp
    $<TARGET_OBJECTS:zoo_core>
    $<TARGET_OBJECTS:zoo_exhibits>)
target_include_directories(zoo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(MSVC)
    target_compile_options(zoo_core PRIVATE /W4)
    target_compile_options(zoo_exhibits PRIVATE /W4)
    target_compile_options(zoo PRIVATE /W4)
else()
    target_compile_options(zoo_core PRIVATE -Wall -Wextra -O3)
    target_compile_options(zoo_exhibits PRIVATE -Wall -Wextra -O3)
    target_compile_options(zoo PRIVATE -Wall -Wextra -O3)
endif()
