cmake_minimum_required(VERSION 3.5)
project(libmemcontext VERSION 1.0.0 LANGUAGES C)

# main library
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_DEBUG")
set(src
	src/memcontext.c
)
add_library(memcontext ${src})
target_include_directories(memcontext
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_options(memcontext PRIVATE -Werror -Wall -Wextra)
target_compile_features(memcontext PRIVATE c_std_11)

find_package (Threads)
target_link_libraries(memcontext Threads::Threads)

# unit tests
set(test_src
	src/test_memcontext.c
)
add_executable(test_memcontext ${test_src})
target_include_directories(test_memcontext
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

find_package(cmocka 1.1.3)
target_link_libraries(test_memcontext memcontext cmocka)
target_compile_features(test_memcontext PRIVATE c_std_11)
target_compile_options(test_memcontext PRIVATE -Werror -Wall -Wextra -O0 -ggdb3 -Wl,--wrap=malloc_free)
