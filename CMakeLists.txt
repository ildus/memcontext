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

find_package(cmocka 1.1.3)

LIST(APPEND tests_names "test_memcontext")
LIST(APPEND tests_options "-Wall -Werror -Wextra")
LIST(APPEND tests_link "-Wl,--wrap,free")

# Declare all tests targets
LIST(LENGTH tests_names count)
MATH(EXPR count "${count} - 1")
FOREACH(i RANGE ${count})
    LIST(GET tests_names ${i} test_name)
    LIST(GET tests_options ${i} test_options)
    LIST(GET tests_link ${i} test_link)
    add_executable(${test_name} src/${test_name}.c)
	target_include_directories(${test_name}
		PUBLIC
			$<INSTALL_INTERFACE:include>
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	)
    target_link_libraries(
        ${test_name}
		cmocka
        -fprofile-arcs
        memcontext
		${test_link}
    )
	target_compile_features(${test_name}
		PRIVATE c_std_11
	)
    ADD_TEST(${test_name} ${test_name})
ENDFOREACH()
ENABLE_TESTING()
