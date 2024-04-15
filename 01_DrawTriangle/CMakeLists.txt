cmake_minimum_required(VERSION 3.29) 

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS)

project(drawtriangle VERSION 1.0)

aux_source_directory (${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/shaders)
include_directories(drawtriangle PUBLIC ../include)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/
)

add_executable(drawtriangle ${SRC_LIST})

if(MSVC)
	set_property(TARGET drawtriangle  APPEND PROPERTY LINK_FLAGS "/NODEFAULTLIB:MSVCRT")
endif()

target_link_libraries(drawtriangle PUBLIC glfw3 vulkan-1) 